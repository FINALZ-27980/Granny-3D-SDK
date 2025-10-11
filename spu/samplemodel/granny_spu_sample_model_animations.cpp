// ========================================================================
// $File: //jeffr/granny_29/rt/cell/spu/granny_spu_sample_model_animations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_spu_sample_model_animations.h"

#include "granny_animation_binding.h"
#include "granny_control.h"
#include "granny_controlled_animation.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_mirror_specification.h"
#include "granny_model_instance.h"
#include "granny_retargeter.h"
#include "granny_skeleton.h"
#include "granny_spu_animation.h"
#include "granny_spu_animation_binding.h"
#include "granny_spu_command.h"
#include "granny_spu_commands_spuside.h"
#include "granny_spu_controlled_animation.h"
#include "granny_spu_curve.h"
#include "granny_spu_curve_sample.h"
#include "granny_spu_dma_utilities.h"
#include "granny_spu_track_group.h"
#include "granny_track_group.h"
#include "granny_track_mask.h"

#include <alloca.h>
#include "spumemstream.h"

#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

USING_GRANNY_NAMESPACE;

typedef ArrayAligner<bone,   MaxSPUBones> bone_aligner;
typedef ArrayAligner<real32, MaxSPUBones> mask_aligner;

struct curve_sampling_store
{
    ALIGN16(model_instance)        InstanceCache;
    ALIGN16(model_control_binding) BindingCache;
    ALIGN16(spu_animation_binding) AnimationBindingCache;
    ALIGN16(control)               CurrentControl;

    ALIGN16(ClassAligner<spu_animation>)        AnimationAligner;
    ALIGN16(ClassAligner<spu_track_group>)      TrackGroupAligner;
    ALIGN16(ClassAligner<retargeter>)           RetargetAligner;
    ALIGN16(ClassAligner<mirror_specification>) MirrorAligner;
    ALIGN16(bone_aligner)                       BoneAligner;

    ALIGN16(int32)                    LocalAccum_Count[MaxSPUBones];
    ALIGN16(real32)                   LocalAccum_Weight[MaxSPUBones];
    ALIGN16(uint32)                   LocalAccum_Flags[MaxSPUBones];
    ALIGN16(Vectormath::Aos::Vector3) LocalAccum_Position[MaxSPUBones];
    ALIGN16(Vectormath::Aos::Quat)    LocalAccum_Orientation[MaxSPUBones];
    ALIGN16(Vectormath::Aos::Matrix3) LocalAccum_ScaleShear[MaxSPUBones];

    ALIGN16(int32)                    TrackNameMap[MaxSPUBones];
    ALIGN16(spu_transform_track)      TransformTracks[MaxSPUBones];

    ALIGN16(transform)                RetargetingTransforms[MaxSPUBones];
    ALIGN16(retargeter*)              Retargeter;

    ALIGN16(transform)                MirrorTransformCache[MaxSPUBones];
    ALIGN16(uint16)                   MirrorRemaps[MaxSPUBones];
    ALIGN16(uint8)                    MirrorPostFlips[MaxSPUBones];
    ALIGN16(mirror_specification*)    MirrorSpecification;

    ALIGN16(track_mask*)              TrackMask;
    ALIGN16(track_mask*)              ModelMask;
    ALIGN16(real32*)                  TrackMaskWeights;
    ALIGN16(ClassAligner<track_mask>) TrackMaskAligner;
    ALIGN16(mask_aligner)             TrackMaskWeightAligner;

    ALIGN_N(uint8, 128)               CurveDataBuffer[MaxSPUTransformTrackSize + SPU_STREAM_EXTRA];
};

// Lame, but make the dma tags global state so everything can get at them.  These are
// setup by the command dispatcher...
static const int g_DMATagInput   = 0;
static const int g_DMATagInputBG = 1;
static const int g_DMATagOutput  = 2;

static const int g_DMAMaskInput   = (1 << g_DMATagInput);
static const int g_DMAMaskInputBG = (1 << g_DMATagInputBG);
static const int g_DMAMaskOutput  = (1 << g_DMATagOutput);


static real32
SPUGetMaskWeight(curve_sampling_store* Store, track_mask* Mask, int32x Index)
{
    if (Mask == 0)
        return 1.0f;

    if (Index >= Mask->BoneWeightCount)
        return Mask->DefaultWeight;

    return Store->TrackMaskWeights[Index];
}

// Make this static so we don't have to deal with the Global_Allowed... from the PPU
static void
SPUFindAllowedErrorNumbers(real32 AllowedError,
                           real32 *AllowedErrorEnd,
                           real32 *AllowedErrorScaler )
{
    const real32 AllowedLODErrorFadingFactor = 0.8f;

    // So the fade out begins at AllowedError and ends at AllowedErrorEnd - below AllowedErrorEnd, we just sample the first value.
    *AllowedErrorEnd = 0.0f;
    *AllowedErrorScaler = 0.0f;
    if ( AllowedError > 0.0f )
    {
        *AllowedErrorEnd = AllowedError * AllowedLODErrorFadingFactor;
        *AllowedErrorScaler = 1.0f / ( AllowedError - *AllowedErrorEnd );
    }
}

static void
TransformToIntrinsic(transform const&          Transform,
                     int32*                   Flags,
                     Vectormath::Aos::Vector3* Pos,
                     Vectormath::Aos::Quat*    Ori,
                     Vectormath::Aos::Matrix3* ScaleShear)
{
    using namespace Vectormath::Aos;

    *Flags = Transform.Flags;

    if (Transform.Flags & HasPosition)
    {
        *Pos = Vector3(Transform.Position[0],
                       Transform.Position[1],
                       Transform.Position[2]);
    }
    else
    {
        *Pos = Vector3(0, 0, 0);
    }

    if (Transform.Flags & HasOrientation)
    {
        *Ori = Quat(Transform.Orientation[0],
                    Transform.Orientation[1],
                    Transform.Orientation[2],
                    Transform.Orientation[3]);
    }
    else
    {
        *Ori = Quat::identity();
    }

    if (Transform.Flags & HasScaleShear)
    {
        *ScaleShear = Matrix3(Vector3(Transform.ScaleShear[0][0], Transform.ScaleShear[0][1], Transform.ScaleShear[0][2]),
                              Vector3(Transform.ScaleShear[1][0], Transform.ScaleShear[1][1], Transform.ScaleShear[1][2]),
                              Vector3(Transform.ScaleShear[2][0], Transform.ScaleShear[2][1], Transform.ScaleShear[2][2]));
    }
    else
    {
        *ScaleShear = Matrix3::identity();
    }
}


static void
LinearBlend(int32& Flags,
            Vectormath::Aos::Vector3& Position,
            Vectormath::Aos::Quat&    Orientation,
            Vectormath::Aos::Matrix3& ScaleShear,
            real32 t,
            transform const &B)
{
    using namespace Vectormath::Aos;

    int32 B_Flags;
    Vectormath::Aos::Vector3 B_Position;
    Vectormath::Aos::Quat    B_Orientation;
    Vectormath::Aos::Matrix3 B_ScaleShear;
    TransformToIntrinsic(B, &B_Flags, &B_Position, &B_Orientation, &B_ScaleShear);

    real32 A_Weight = (1.0f - t);
    real32 B_Weight = t;

    Flags = Flags | B_Flags;
    if (Flags & HasPosition)
    {
        Position = (Position * A_Weight) + (B_Position * B_Weight);
    }
    else
    {
        Position = Vector3(0, 0, 0);
    }

    if (Flags & HasOrientation)
    {
        Orientation = normalize((Orientation * A_Weight) +
                                (B_Orientation * B_Weight));
    }
    else
    {
        Orientation = Quat::identity();
    }

    if (Flags & HasScaleShear)
    {
        ScaleShear = ((ScaleShear   * A_Weight) +
                      (B_ScaleShear * B_Weight));
    }
    else
    {
        ScaleShear = Matrix3::identity();
    }
}

static void
SampleTrackUUULocalSPU(sample_context const& NOALIAS Context,
                       spu_transform_track const& NOALIAS SourceTrack,
                       uint8* SPUCurveAddrBase,
                       int32& Flags,
                       Vectormath::Aos::Vector3& Position,
                       Vectormath::Aos::Quat& Orientation,
                       Vectormath::Aos::Matrix3& ScaleShear)
{
    using namespace Vectormath::Aos;

    if (SourceTrack.PosCurveOffset == SPUTransformTrackNoCurve)
    {
        Flags |= (SourceTrack.LODTransform.Flags & HasPosition);
        Position = Vector3(SourceTrack.LODTransform.Position[0],
                           SourceTrack.LODTransform.Position[1],
                           SourceTrack.LODTransform.Position[2]);
    }
    else
    {
        Flags |= HasPosition;


        uint8* PosCurveBytes = SPUCurveAddrBase + SourceTrack.PosCurveOffset;
        EvaluateSPUCurve(Context, PosCurveBytes, 3, (real32*)&Position);
    }

    if (SourceTrack.OriCurveOffset == SPUTransformTrackNoCurve)
    {
        Flags |= (SourceTrack.LODTransform.Flags & HasOrientation);
        Orientation = Quat(SourceTrack.LODTransform.Orientation[0],
                           SourceTrack.LODTransform.Orientation[1],
                           SourceTrack.LODTransform.Orientation[2],
                           SourceTrack.LODTransform.Orientation[3]);
    }
    else
    {
        Flags |= HasOrientation;

        uint8* OriCurveBytes = SPUCurveAddrBase + SourceTrack.OriCurveOffset;
        EvaluateSPUCurve(Context, OriCurveBytes, 4, (real32*)&Orientation);
    }

    if (SourceTrack.SSCurveOffset == SPUTransformTrackNoCurve)
    {
        Flags |= (SourceTrack.LODTransform.Flags & HasScaleShear);
        ScaleShear = Matrix3(Vector3(SourceTrack.LODTransform.ScaleShear[0][0],
                                     SourceTrack.LODTransform.ScaleShear[0][1],
                                     SourceTrack.LODTransform.ScaleShear[0][2]),
                             Vector3(SourceTrack.LODTransform.ScaleShear[1][0],
                                     SourceTrack.LODTransform.ScaleShear[1][1],
                                     SourceTrack.LODTransform.ScaleShear[1][2]),
                             Vector3(SourceTrack.LODTransform.ScaleShear[2][0],
                                     SourceTrack.LODTransform.ScaleShear[2][1],
                                     SourceTrack.LODTransform.ScaleShear[2][2]));
    }
    else
    {
        Flags |= HasScaleShear;

        // Matrix3 intrinsics are 12 wide, rather than 9.  Just do a quick shuffle here...
        real32 SampledMatrix[3][3];
        CompileAssert(sizeof(SampledMatrix) == 9 * sizeof(real32));

        uint8* SSCurveBytes = SPUCurveAddrBase + SourceTrack.SSCurveOffset;
        EvaluateSPUCurve(Context, SSCurveBytes, 9, (real32*)SampledMatrix);
        ScaleShear = Matrix3(Vector3(SampledMatrix[0][0], SampledMatrix[0][1], SampledMatrix[0][2]),
                             Vector3(SampledMatrix[1][0], SampledMatrix[1][1], SampledMatrix[1][2]),
                             Vector3(SampledMatrix[2][0], SampledMatrix[2][1], SampledMatrix[2][2]));
    }
}



// Note Source may be the same as Dest, be careful.
//  Commentary: Umm.  Yeah, this is based on the old rebaser, but the math is obviously
//      twitchy, since we need to ensure that the resulting transform is separated into
//      it's s/r/p components.  Tread carefully, etc, etc.
static void
RebasingTransformSPU(int32& Flags,
                     Vectormath::Aos::Vector3& Pos,
                     Vectormath::Aos::Quat&    Ori,
                     Vectormath::Aos::Matrix3& SS,
                     transform const& Rebaser)
{
    using namespace Vectormath::Aos;

    int32   RebasingFlags;
    Vector3 RebasingPosition;
    Quat    RebasingOrientation;
    Matrix3 RebasingScaleShear;
    TransformToIntrinsic(Rebaser,
                         &RebasingFlags, &RebasingPosition,
                         &RebasingOrientation, &RebasingScaleShear);

    Pos += RebasingPosition;
    Ori *= RebasingOrientation;

    if ((Flags | RebasingFlags) & HasScaleShear)
    {
        Transform3 OriXF(RebasingOrientation, Vector3(0, 0, 0));
        Transform3 RebaserScaleXF(RebasingScaleShear, Vector3(0, 0, 0));
        Transform3 InvOriXF(conj(RebasingOrientation), Vector3(0, 0, 0));

        SS = (Transform3(SS, Vector3(0, 0, 0)) * InvOriXF * RebaserScaleXF * OriXF).getUpper3x3();
        Flags = (HasPosition | HasOrientation | HasScaleShear);
    }
    else
    {
        Flags = (HasPosition | HasOrientation);
    }
}

static void
AccumulateLocalTransformSPU(curve_sampling_store* NOALIAS Store,
                            int32x SkeletonBoneIndex,
                            real32 Weight,
                            transform const& NOALIAS ReferenceTransform,
                            quaternion_mode Mode,
                            int32 Flags,
                            Vectormath::Aos::Vector3 const& Pos2,
                            Vectormath::Aos::Quat const&    Ori2,
                            Vectormath::Aos::Matrix3 const& ScaleShear2)
{
    using namespace Vectormath::Aos;
    int32&   Local_Count  = Store->LocalAccum_Count[SkeletonBoneIndex];
    uint32&  Local_Flags  = Store->LocalAccum_Flags[SkeletonBoneIndex];
    real32&  Local_Weight = Store->LocalAccum_Weight[SkeletonBoneIndex];
    Vector3& Local_Pos    = Store->LocalAccum_Position[SkeletonBoneIndex];
    Quat&    Local_Ori    = Store->LocalAccum_Orientation[SkeletonBoneIndex];
    Matrix3& Local_SS     = Store->LocalAccum_ScaleShear[SkeletonBoneIndex];

    // Retarget the animation if necessary...
    Vector3 UsePos = Pos2;
    Quat    UseOri = Ori2;
    Matrix3 UseScaleShear = ScaleShear2;
    if (Store->Retargeter)
    {
        RebasingTransformSPU(Flags, UsePos, UseOri, UseScaleShear,
                             Store->RetargetingTransforms[SkeletonBoneIndex]);
    }

    real32 QuaternionWeight;
    switch(Mode)
    {
        default:
        case BlendQuaternionDirectly:
        {
            QuaternionWeight = Weight;
        } break;

        case BlendQuaternionInverted:
        {
            QuaternionWeight = -Weight;
        } break;

        case BlendQuaternionNeighborhooded:
        {
            Quat RefOri(ReferenceTransform.Orientation[0],
                        ReferenceTransform.Orientation[1],
                        ReferenceTransform.Orientation[2],
                        ReferenceTransform.Orientation[3]);

            real32 IP = dot(RefOri, UseOri);
            QuaternionWeight = (IP >= 0) ? Weight : -Weight;
        } break;

        case BlendQuaternionAccumNeighborhooded:
        {
            real32 IP;
            if (Local_Count == 0)
            {
                Quat RefOri(ReferenceTransform.Orientation[0],
                            ReferenceTransform.Orientation[1],
                            ReferenceTransform.Orientation[2],
                            ReferenceTransform.Orientation[3]);
                IP = dot(RefOri, UseOri);
            }
            else
            {
                // neighborhood to the blend in progress...
                IP = dot(Local_Ori, UseOri);
            }

            QuaternionWeight = (IP >= 0) ? Weight : -Weight;
        } break;
    }

    if (Local_Count)
    {
        // We've already started accumulating, so there's data in there.
        Local_Pos += UsePos        * Weight;
        Local_Ori += UseOri     * QuaternionWeight;
        Local_SS  += UseScaleShear * Weight;

        Local_Flags  |= Flags;
        Local_Weight += Weight;
        ++Local_Count;
    }
    else
    {
        // First time this transform has been accumulated to this time around, so the existing data is ignored.
        // Note: Always multiply against the quaternion weight, since it may be -1
        if (Weight == 1.0f)
        {
            Local_Pos = UsePos;
            Local_Ori = UseOri * QuaternionWeight;
            Local_SS  = UseScaleShear;
        }
        else
        {
            Local_Pos = UsePos        * Weight;
            Local_Ori = UseOri     * QuaternionWeight;
            Local_SS  = UseScaleShear * Weight;
        }

        Local_Flags  = Flags;
        Local_Weight = Weight;
        Local_Count  = 1;
    }
}


static void
EndLocalPoseAccumulationLODSPU(curve_sampling_store* NOALIAS Store,
                               bone* Bones,
                               real32 LocalPoseFillThreshold,
                               int32x FirstBone, int32x BoneCount,
                               real32 AllowedError)
{
    using namespace Vectormath::Aos;

    real32 AllowedErrorEnd;
    real32 AllowedErrorScaler;
    SPUFindAllowedErrorNumbers(AllowedError, &AllowedErrorEnd, &AllowedErrorScaler);

    {for(int32x SkeletonBoneIndex = FirstBone;
         SkeletonBoneIndex < (FirstBone + BoneCount);
         ++SkeletonBoneIndex)
    {
        int32&   Local_Count  = Store->LocalAccum_Count[SkeletonBoneIndex];
        real32&  Local_Weight = Store->LocalAccum_Weight[SkeletonBoneIndex];
        uint32&  Local_Flags  = Store->LocalAccum_Flags[SkeletonBoneIndex];
        Vector3& Local_Pos    = Store->LocalAccum_Position[SkeletonBoneIndex];
        Quat&    Local_Ori    = Store->LocalAccum_Orientation[SkeletonBoneIndex];
        Matrix3& Local_SS     = Store->LocalAccum_ScaleShear[SkeletonBoneIndex];

        bone const& NOALIAS Bone = Bones[SkeletonBoneIndex];
        int32  BoneFlags;
        Vector3 BonePosition;
        Quat    BoneOrientation;
        Matrix3 BoneScaleShear;
        TransformToIntrinsic(Bone.LocalTransform,
                             &BoneFlags, &BonePosition,
                             &BoneOrientation, &BoneScaleShear);


        if (Local_Count != 0)
        {
            if (Local_Weight < LocalPoseFillThreshold)
            {
                AccumulateLocalTransformSPU(Store, SkeletonBoneIndex,
                                            LocalPoseFillThreshold - Local_Weight,
                                            Bone.LocalTransform,
                                            BlendQuaternionAccumNeighborhooded,
                                            BoneFlags,
                                            BonePosition,
                                            BoneOrientation,
                                            BoneScaleShear);
            }

            // Determine what blending regime the bone is in
            // w.r.t. the allowed error
            const real32 BoneError = Bone.LODError;
            real32 BlendFactor = ( AllowedError - BoneError ) * AllowedErrorScaler;
            if (BlendFactor <= 0.0f)
            {
                // No change, this bone is not lerped
            }
            else if (BlendFactor < 0.99f)  // cap to prevent ludicrous weights
            {
                // Since the localpose already has weight, we need to compute a new weight
                //  such that the accumulate we're going to do here does the correct lerp.
                //  In this case, the correct factor is (Weight / (1 - BlendFactor))
                BlendFactor *= (Local_Weight / (1 - BlendFactor));
                AccumulateLocalTransformSPU(Store, SkeletonBoneIndex,
                                            BlendFactor,
                                            Bone.LocalTransform,
                                            BlendQuaternionNeighborhooded,
                                            BoneFlags,
                                            BonePosition,
                                            BoneOrientation,
                                            BoneScaleShear);
            }
            else
            {
                // Just set the transform to the reference transform.
                Local_Count  = 1;
                Local_Weight = 1.0f;
                Local_Flags  = Bone.LocalTransform.Flags;
                Local_Pos    = BonePosition;
                Local_Ori    = BoneOrientation;
                Local_SS     = BoneScaleShear;
            }

            if ((Local_Count != 1) || (Local_Weight != 1.0f))
            {
                real32 const InverseTotalWeight = 1.0f / Local_Weight;
                Local_Pos *= InverseTotalWeight;
                Local_SS  *= InverseTotalWeight;
            }

            Local_Ori = normalize(Local_Ori);
        }
        else
        {
            Local_Count  = 1;
            Local_Weight = 1.0f;
            Local_Flags  = Bone.LocalTransform.Flags;
            Local_Pos    = BonePosition;
            Local_Ori    = BoneOrientation;
            Local_SS     = BoneScaleShear;
        }
    }}
}


static void
SPUAnimationAccumulateBindingState(curve_sampling_store* NOALIAS Store,
                                   model_control_binding& NOALIAS Binding,
                                   spu_controlled_animation& NOALIAS ControlledAnim,
                                   control& NOALIAS Control,
                                   int32x FirstBone, int32x BoneCount,
                                   bone* NOALIAS SkeletonBones,
                                   real32 AllowedError)
{
    using namespace Vectormath::Aos;

    real32 const ControlWeight = GetControlEffectiveWeight(Control);
    if (ControlWeight <= TrackWeightEpsilon)
        return;

    //__asm volatile ("stopd 0,1,1");

    // We have to wait for the animation binding, since it contains all the pointers we
    // need to start transfering...
    spu_animation_binding* AnimBinding = &Store->AnimationBindingCache;
    cellDmaGet(AnimBinding, (uint64x)ControlledAnim.Binding, sizeof(spu_animation_binding), g_DMATagInput, 0, 0);
    cellDmaWaitTagStatusAll(g_DMAMaskInput);

    int32*               TrackNameMap    = Store->TrackNameMap;
    spu_transform_track* TransformTracks = Store->TransformTracks;

    if (ControlledAnim.MirrorSpecification)
    {
        Store->MirrorSpecification =
            Store->MirrorAligner.TransferFrom((uint64x)ControlledAnim.MirrorSpecification, g_DMATagInput);
        cellDmaGet(Store->MirrorRemaps,
                   (uint64x)Store->MirrorSpecification->Remaps,
                   ((sizeof(uint16) * Store->MirrorSpecification->RemapCount) + 15) & ~15,
                   g_DMATagInput, 0, 0);
        cellDmaGet(Store->MirrorPostFlips,
                   (uint64x)Store->MirrorSpecification->PostFlips,
                   ((sizeof(uint8) * Store->MirrorSpecification->RemapCount) + 15) & ~15,
                   g_DMATagInput, 0, 0);

        // Reset the pointers
        Store->MirrorSpecification->Remaps    = Store->MirrorRemaps;
        Store->MirrorSpecification->PostFlips = Store->MirrorPostFlips;
    }
    else
    {
        Store->MirrorSpecification = 0;
    }

    if (AnimBinding->Retargeter)
    {
        AnimBinding->Retargeter = Store->Retargeter =
            Store->RetargetAligner.TransferFromNB((uint64x)AnimBinding->Retargeter, g_DMATagInput);
        cellDmaWaitTagStatusAll(g_DMAMaskInput);
        cellDmaLargeGet(Store->RetargetingTransforms, (uint64x)AnimBinding->Retargeter->RetargetingTransforms,
                        Store->Retargeter->NumBones * SizeOf(transform),
                        g_DMATagInput, 0, 0);
        //cellDmaWaitTagStatusAll(g_DMAMaskInput);
    }
    else
    {
        Store->Retargeter = 0;
    }

    // The rest can be issued one right after the other...
    cellDmaLargeGet(TrackNameMap, (uint64x)AnimBinding->TrackNameRemaps,
                    sizeof(int32) * AnimBinding->TrackNameRemapCount,
                    g_DMATagInput, 0, 0);
    cellDmaGet(TransformTracks, (uint64x)AnimBinding->ID.TransformTracks,
               sizeof(spu_transform_track) * AnimBinding->ID.TransformTrackCount,
               g_DMATagInput, 0, 0);


    SPU_MEM_STREAM memStream;
    spu_mem_init(&memStream,
                 Store->CurveDataBuffer, sizeof(Store->CurveDataBuffer),
                 AnimBinding->ID.CurveBytes, AnimBinding->ID.CurveByteCount);

    // Wait occurs below, try to do some work in between...
    real32 AllowedErrorEnd, AllowedErrorScaler;
    sample_context Context;
    {
        SPUFindAllowedErrorNumbers ( AllowedError, &AllowedErrorEnd, &AllowedErrorScaler );

        Context.LocalClock = GetControlClampedLocalClock(Control);
        Context.FrameIndex = 0;
        Context.LocalDuration = GetControlLocalDuration(Control);
        GetControlLoopState(Control, Context.UnderflowLoop, Context.OverflowLoop);
    }

    // Wait for DMA above to complete
    cellDmaWaitTagStatusAll(g_DMAMaskInput);

    spu_track_group* TrackGroup = Store->TrackGroupAligner.TransferFromNB((uint64x)AnimBinding->ID.TrackGroup, g_DMATagInputBG);

    uint8* StreamAddress = Store->CurveDataBuffer;
    int32  TrackingCurveOffset = 0;

    {for(int32x TrackIndex = 0; TrackIndex < AnimBinding->ID.TransformTrackCount; ++TrackIndex)
    {
        spu_transform_track const& Track = TransformTracks[TrackIndex];
        if (!spu_mem_stream(&memStream, (void*)&StreamAddress, Track.BytesRequiredToSample,
                            (MaxSPUTransformTrackSize - Track.BytesRequiredToSample), g_DMATagInputBG))
        {
            // Bad!
            break;
        }

        uint8* CurveAddress = StreamAddress - TrackingCurveOffset;
        TrackingCurveOffset += Track.BytesRequiredToSample;
        StreamAddress += Track.BytesRequiredToSample;

        real32 Weight = ControlWeight;

        int SkeletonBoneIndex = TrackNameMap[Track.FromNameIndex];
        if (SkeletonBoneIndex < FirstBone || SkeletonBoneIndex >= FirstBone + BoneCount)
            continue;

        if (Store->TrackMask != 0)
        {
            Weight *= SPUGetMaskWeight(Store, Store->TrackMask, TrackIndex);
        }
        else if (Store->ModelMask != 0)
        {
            Weight *= SPUGetMaskWeight(Store, Store->ModelMask, SkeletonBoneIndex);
        }

        if (Weight > TrackWeightEpsilon)
        {
            int32  Flags = 0;
            Vector3 Position;
            Quat    Orientation;
            Matrix3 ScaleShear;

            // Note careful setting of the == case. If the AllowedError is zero
            // (i.e. they want no LOD), we can still use the LOD if the error is
            // also zero. This happens when the track is constant (which is pretty
            // common), and it's quite a bit faster to do the LOD thing than to
            // actually go and sample it, because the number of calls and memory
            // hits is lower.
            if (AllowedErrorEnd >= Track.AnimLODValue)
            {
                TransformToIntrinsic(Track.LODTransform,
                                     &Flags, &Position, &Orientation, &ScaleShear);
            }
            else
            {
                // Wait for the spu_mem_stream to complete
                cellDmaWaitTagStatusAll(g_DMAMaskInputBG);
                SampleTrackUUULocalSPU(Context, Track, CurveAddress,
                                       Flags, Position, Orientation, ScaleShear);

                if (AllowedError >= Track.AnimLODValue)
                {
                    transform const& NOALIAS SampledTransformApprox = Track.LODTransform;
                    real32 BlendFactor = (AllowedError - Track.AnimLODValue) * AllowedErrorScaler;

                    LinearBlend(Flags, Position, Orientation, ScaleShear,
                                BlendFactor,
                                SampledTransformApprox);
                }
            }

            // TODO: Handle overridable accumulation
            if (SkeletonBoneIndex == 0)
            {
                // Block until the background dma completes...
                cellDmaWaitTagStatusAll(g_DMAMaskInputBG);

                if (TrackGroup->Flags & AccumulationIsVDA)
                {
                    Flags &= ~(HasPosition | HasOrientation);
                    Position = Vector3(0, 0, 0);
                    Orientation = Quat::identity();
                }
            }

            if (Store->MirrorSpecification == 0)
            {
                // Just accumulate right here

                // TODO: Get from binding.  We'll need to finesse this...
                quaternion_mode QuatMode = BlendQuaternionNeighborhooded;

                transform const& ReferenceTransform = SkeletonBones[SkeletonBoneIndex].LocalTransform;
                AccumulateLocalTransformSPU(Store, SkeletonBoneIndex,
                                            Weight, ReferenceTransform, QuatMode,
                                            Flags, Position, Orientation, ScaleShear);
            }
            else
            {
                // We need to mirror this before accumulating.
                Store->MirrorTransformCache[SkeletonBoneIndex].Flags = Flags;

                Store->MirrorTransformCache[SkeletonBoneIndex].Position[0] = Position[0];
                Store->MirrorTransformCache[SkeletonBoneIndex].Position[1] = Position[1];
                Store->MirrorTransformCache[SkeletonBoneIndex].Position[2] = Position[2];

                Store->MirrorTransformCache[SkeletonBoneIndex].Orientation[0] = Orientation[0];
                Store->MirrorTransformCache[SkeletonBoneIndex].Orientation[1] = Orientation[1];
                Store->MirrorTransformCache[SkeletonBoneIndex].Orientation[2] = Orientation[2];
                Store->MirrorTransformCache[SkeletonBoneIndex].Orientation[3] = Orientation[3];

                {for(int32x Col = 0; Col < 3; ++Col)
                {
                    Store->MirrorTransformCache[SkeletonBoneIndex].ScaleShear[Col][0] = ScaleShear[Col][0];
                    Store->MirrorTransformCache[SkeletonBoneIndex].ScaleShear[Col][1] = ScaleShear[Col][1];
                    Store->MirrorTransformCache[SkeletonBoneIndex].ScaleShear[Col][2] = ScaleShear[Col][2];
                }}
            }
        }
    }}

    // TODO: optimize, this is a crazy conversion from/to SPU format

    // If we have a mirror specification, mirror and accumulate
    if (Store->MirrorSpecification != 0)
    {
        // Do the mirroring
        MaskedMirrorPoseTransforms(Store->MirrorSpecification,
                                   SizeOf(transform), Store->MirrorTransformCache,
                                   SizeOf(bone), &SkeletonBones[0].ParentIndex,
                                   BoneCount, 0);

        {for(int32x TrackIndex = 0; TrackIndex < AnimBinding->ID.TransformTrackCount; ++TrackIndex)
        {
            spu_transform_track const& Track = TransformTracks[TrackIndex];
            real32 Weight = ControlWeight;

            int SkeletonBoneIndex = TrackNameMap[Track.FromNameIndex];
            if (SkeletonBoneIndex < FirstBone || SkeletonBoneIndex >= FirstBone + BoneCount)
                continue;

            if (Store->TrackMask != 0)
            {
                Weight *= SPUGetMaskWeight(Store, Store->TrackMask, TrackIndex);
            }
            else if (Store->ModelMask != 0)
            {
                Weight *= SPUGetMaskWeight(Store, Store->ModelMask, SkeletonBoneIndex);
            }

            if (Weight > TrackWeightEpsilon)
            {
                int32  Flags = 0;
                Vector3 Position;
                Quat    Orientation;
                Matrix3 ScaleShear;
                {
                    // We need to mirror this before accumulating.
                    Flags = Store->MirrorTransformCache[SkeletonBoneIndex].Flags;

                    Position[0] = Store->MirrorTransformCache[SkeletonBoneIndex].Position[0];
                    Position[1] = Store->MirrorTransformCache[SkeletonBoneIndex].Position[1];
                    Position[2] = Store->MirrorTransformCache[SkeletonBoneIndex].Position[2];

                    Orientation[0] = Store->MirrorTransformCache[SkeletonBoneIndex].Orientation[0];
                    Orientation[1] = Store->MirrorTransformCache[SkeletonBoneIndex].Orientation[1];
                    Orientation[2] = Store->MirrorTransformCache[SkeletonBoneIndex].Orientation[2];
                    Orientation[3] = Store->MirrorTransformCache[SkeletonBoneIndex].Orientation[3];

                    {for(int32x Col = 0; Col < 3; ++Col)
                    {
                        ScaleShear[Col][0] = Store->MirrorTransformCache[SkeletonBoneIndex].ScaleShear[Col][0];
                        ScaleShear[Col][1] = Store->MirrorTransformCache[SkeletonBoneIndex].ScaleShear[Col][1];
                        ScaleShear[Col][2] = Store->MirrorTransformCache[SkeletonBoneIndex].ScaleShear[Col][2];
                    }}
                }

                // TODO: Get from binding.  We'll need to finesse this...
                quaternion_mode QuatMode = BlendQuaternionNeighborhooded;

                transform const& ReferenceTransform = SkeletonBones[SkeletonBoneIndex].LocalTransform;
                AccumulateLocalTransformSPU(Store, SkeletonBoneIndex,
                                            Weight, ReferenceTransform, QuatMode,
                                            Flags, Position, Orientation, ScaleShear);
            }
        }}
    }
}


// It's necessary in staged samples to have access to the bone array, so return the
// correctly aligned pointer to that array...
static bone*
SampleModelControlsSPU(curve_sampling_store* LocalStore,
                       uint64x InstanceEA,
                       int32   BoneCount,
                       real32  LocalPoseFillThreshold,
                       real32  AllowedError,
                       uint64x SingleControlEA)
{
    model_instance* Instance = &LocalStore->InstanceCache;

    // Start transferring the model instance.  Do the memset while that's occuring
    cellDmaLargeGet(Instance, (uint64x)InstanceEA, sizeof(model_instance), g_DMATagInput, 0, 0);
    {
        // Since we build the local_pose_transforms in local memory, just set the count to
        // zero.  We could use builtin_memset here, but cunningly, this requires linking
        // to the std library, which is problematic in RAW mode.
        //__builtin_memset(Store->LocalAccum_Count, 0, sizeof(Store->LocalAccum_Count));
        {for(int32x Idx = 0; Idx < BoneCount; ++Idx)
        {
            LocalStore->LocalAccum_Count[Idx] = 0;
        }}
    }
    cellDmaWaitTagStatusAll(g_DMAMaskInput);

    bone* SkeletonBones =
        LocalStore->BoneAligner.TransferElementsFrom((uint64x)Instance->CachedBones,
                                                     Instance->CachedBoneCount, g_DMATagInput);

    uint64x SentinelEA    = (uint64x)InstanceEA + OffsetFromType(model_instance, BindingSentinel);
    uint64x CurrBindingEA = (uint64x)Instance->BindingSentinel.ModelNext;
    while (CurrBindingEA != SentinelEA)
    {
        Assert(IS_ALIGNED_16(CurrBindingEA));
        model_control_binding* Binding = &LocalStore->BindingCache;
        control* Control               = &LocalStore->CurrentControl;

        cellDmaLargeGet(Binding, (uint64x)CurrBindingEA, sizeof(model_control_binding), g_DMATagInput, 0, 0);
        cellDmaWaitTagStatusAll(g_DMAMaskInput);

        spu_controlled_animation ControlledAnim;
        __builtin_memcpy(&ControlledAnim, Binding->Derived, sizeof(spu_controlled_animation));
        if (Binding->BindingType == SPUControlledAnim)
        {
            if (SingleControlEA == 0 || SingleControlEA == (uint64x)Binding->Control)
            {
                cellDmaLargeGet(Control, (uint64x)Binding->Control, sizeof(control),
                                g_DMATagInput, 0, 0);
                cellDmaWaitTagStatusAll(g_DMAMaskInput);

                if (ControlHasEffect(*Control))
                {
                    track_mask const* Mask = 0;
                    if (ControlledAnim.TrackMask)
                    {
                        LocalStore->TrackMask =
                            LocalStore->TrackMaskAligner.TransferFrom((uint64x)ControlledAnim.TrackMask, g_DMATagInput);
                        Mask = LocalStore->TrackMask;
                    }
                    else if (ControlledAnim.ModelMask)
                    {
                        LocalStore->ModelMask =
                            LocalStore->TrackMaskAligner.TransferFrom((uint64x)ControlledAnim.ModelMask, g_DMATagInput);
                        Mask = LocalStore->ModelMask;
                    }

                    if (Mask != 0 && Mask->BoneWeightCount != 0)
                    {
                        cellDmaWaitTagStatusAll(g_DMAMaskInput);

                        LocalStore->TrackMaskWeights =
                            LocalStore->TrackMaskWeightAligner.TransferElementsFrom((uint64x)Mask->BoneWeights,
                                                                                    Mask->BoneWeightCount,
                                                                                    g_DMATagInput);
                        cellDmaWaitTagStatusAll(g_DMAMaskInput);
                    }
                    else
                    {
                        LocalStore->TrackMask = 0;
                        LocalStore->ModelMask = 0;
                    }

                    SPUAnimationAccumulateBindingState(LocalStore,
                                                       *Binding, ControlledAnim, *Control,
                                                       0, BoneCount,
                                                       SkeletonBones,
                                                       AllowedError);
                }
                else
                {
                    // Done, all the rest are zero weight, or inactive
                    break;
                }
            }
        }
        else
        {
            // Log error.  Must start SPU animations with PlayControlledSPUAnimation
        }

        CurrBindingEA = (uint64x)Binding->ModelNext;
    }

    EndLocalPoseAccumulationLODSPU(LocalStore, SkeletonBones,
                                   LocalPoseFillThreshold,
                                   0, BoneCount,
                                   AllowedError);

    return SkeletonBones;
}


static inline Vectormath::Aos::Matrix4
Matrix4FromGrannyMatrix(real32 const* GrannyMatrix)
{
    using namespace Vectormath::Aos;
    return Matrix4(Vector4(GrannyMatrix[0  + 0], GrannyMatrix[0  + 1], GrannyMatrix[0  + 2], GrannyMatrix[0  + 3]),
                   Vector4(GrannyMatrix[4  + 0], GrannyMatrix[4  + 1], GrannyMatrix[4  + 2], GrannyMatrix[4  + 3]),
                   Vector4(GrannyMatrix[8  + 0], GrannyMatrix[8  + 1], GrannyMatrix[8  + 2], GrannyMatrix[8  + 3]),
                   Vector4(GrannyMatrix[12 + 0], GrannyMatrix[12 + 1], GrannyMatrix[12 + 2], GrannyMatrix[12 + 3]));
}


static void
BuildWorldPoseMatrixSPU(uint32 Flags,
                        Vectormath::Aos::Vector3& Position,
                        Vectormath::Aos::Quat&    Orientation,
                        Vectormath::Aos::Matrix3& ScaleShear,

                        Vectormath::Aos::Matrix4 const& NOALIAS ParentWorld,
                        real32 const* NOALIAS InverseWorld,

                        Vectormath::Aos::Matrix4* NOALIAS WorldMatrix,
                        Vectormath::Aos::Matrix4* NOALIAS CompositeMatrix)
{
    using namespace Vectormath::Aos;

    Matrix4 TransformMatrix = Matrix4(Orientation, Position);
    if (Flags & HasScaleShear)
        TransformMatrix *= Matrix4(ScaleShear, Vector3(0));

    *WorldMatrix = ParentWorld * TransformMatrix;

    if (CompositeMatrix)
    {
        Matrix4 InverseWorldMatrix = Matrix4FromGrannyMatrix(InverseWorld);
        *CompositeMatrix = InverseWorldMatrix * (*WorldMatrix);
    }
}


static void ComputeMatrixBufferPtrs(uint8* Buffer, uint32x Size,
                                    Vectormath::Aos::Matrix4*& WorldBuffer,
                                    Vectormath::Aos::Matrix4*& CompositeBuffer)
{
    using namespace Vectormath::Aos;

    Assert(Size >= sizeof(Matrix4) * MaxSPUBones * 2);

    WorldBuffer     = (Matrix4*)Buffer;
    CompositeBuffer = (Matrix4*)(Buffer + (sizeof(Matrix4) * MaxSPUBones));

    Assert(IS_ALIGNED_N(WorldBuffer, 128));
    Assert(IS_ALIGNED_N(CompositeBuffer, 128));
}


static void
BuildWorldPoseSPU(curve_sampling_store* NOALIAS Store,
                  bone const* NOALIAS SkeletonBones,
                  int32x FirstBone, int32x BoneCount,
                  int32x ValidLocalBoneCount,
                  Vectormath::Aos::Matrix4 const& NOALIAS OffsetMatrix,
                  bool ComputeComposites)
{
    using namespace Vectormath::Aos;

    Matrix4* WorldBuffer;
    Matrix4* CompositeBuffer;
    ComputeMatrixBufferPtrs(Store->CurveDataBuffer, sizeof(Store->CurveDataBuffer),
                            WorldBuffer, CompositeBuffer);

    bone const* Bone      = SkeletonBones + FirstBone;
    Matrix4*    World     = WorldBuffer + FirstBone;

    // If we're not computing the composites, just leave them NULLed out.
    Matrix4* Composite = 0;
    if (ComputeComposites)
        Composite = CompositeBuffer + FirstBone;

    uint32 const*  Local_Flags = Store->LocalAccum_Flags + FirstBone;
    Vector3 const* Local_Pos   = Store->LocalAccum_Position + FirstBone;
    Quat    const* Local_Ori   = Store->LocalAccum_Orientation + FirstBone;
    Matrix3 const* Local_SS    = Store->LocalAccum_ScaleShear + FirstBone;

    Bone        += FirstBone;
    World       += FirstBone;
    Composite   += FirstBone;
    Local_Flags += FirstBone;
    Local_Pos   += FirstBone;
    Local_Ori   += FirstBone;
    Local_SS    += FirstBone;

    while (BoneCount)
    {
        int32x ParentIndex = Bone->ParentIndex;
        Matrix4 const& ParentWorld = ((ParentIndex == NoParentBone) ?
                                      OffsetMatrix :
                                      WorldBuffer[ParentIndex]);
        int32 Flags;
        Vector3 Position;
        Quat    Orientation;
        Matrix3 ScaleShear;
        if (ValidLocalBoneCount > 0)
        {
            Flags       = *Local_Flags;
            Position    = *Local_Pos;
            Orientation = *Local_Ori;
            ScaleShear  = *Local_SS;
        }
        else
        {
            TransformToIntrinsic(Bone->LocalTransform,
                                 &Flags, &Position, &Orientation, &ScaleShear);
        }

        BuildWorldPoseMatrixSPU(Flags, Position, Orientation, ScaleShear,
                                ParentWorld,
                                (real32*)Bone->InverseWorld4x4,
                                World, Composite);


        ++Local_Flags;
        ++Local_Pos;
        ++Local_Ori;
        ++Local_SS;

        ++Bone;
        ++World;
        if (Composite)
            ++Composite;

        --BoneCount;
        --ValidLocalBoneCount;
    }
}


static void
PutTransformsToLocalPose(curve_sampling_store*     LocalStore,
                         uint32x                   BoneCount,
                         uint64x                   LocalPoseTransformsEA)
{
    local_pose_transform* LocalTransforms = (local_pose_transform*)LocalStore->CurveDataBuffer;

    // Convert our data into local_pose_transforms.  We could theoretically do this in 2
    // batches to help hide DMA latency...
    {for(uint32x Idx = 0; Idx < (uint32x)BoneCount; ++Idx)
    {
        LocalTransforms[Idx].Weight = LocalStore->LocalAccum_Weight[Idx];
        LocalTransforms[Idx].Count  = LocalStore->LocalAccum_Count[Idx];

        LocalTransforms[Idx].Transform.Position[0] = LocalStore->LocalAccum_Position[Idx][0];
        LocalTransforms[Idx].Transform.Position[1] = LocalStore->LocalAccum_Position[Idx][1];
        LocalTransforms[Idx].Transform.Position[2] = LocalStore->LocalAccum_Position[Idx][2];

        LocalTransforms[Idx].Transform.Orientation[0] = LocalStore->LocalAccum_Orientation[Idx][0];
        LocalTransforms[Idx].Transform.Orientation[1] = LocalStore->LocalAccum_Orientation[Idx][1];
        LocalTransforms[Idx].Transform.Orientation[2] = LocalStore->LocalAccum_Orientation[Idx][2];
        LocalTransforms[Idx].Transform.Orientation[3] = LocalStore->LocalAccum_Orientation[Idx][3];

        {for(int32x Col = 0; Col < 3; ++Col)
        {
            LocalTransforms[Idx].Transform.ScaleShear[Col][0] = LocalStore->LocalAccum_ScaleShear[Idx][Col][0];
            LocalTransforms[Idx].Transform.ScaleShear[Col][1] = LocalStore->LocalAccum_ScaleShear[Idx][Col][1];
            LocalTransforms[Idx].Transform.ScaleShear[Col][2] = LocalStore->LocalAccum_ScaleShear[Idx][Col][2];
        }}

        LocalTransforms[Idx].Transform.Flags = LocalStore->LocalAccum_Flags[Idx];
        LocalTransforms[Idx].TraversalID = 0;
    }}

    cellDmaPut(LocalTransforms,
               LocalPoseTransformsEA,
               sizeof(local_pose_transform) * BoneCount,
               g_DMATagOutput, 0, 0);
}


// TODO: probably just delete these functions, they're vestigial
static void
SampleModelAnimationsSPU(curve_sampling_store* LocalStore,
                         uint64x InstanceEA,
                         uint32x BoneCount,
                         uint64x LocalPoseTransformsEA,
                         real32  LocalPoseFillThreshold,
                         real32  AllowedError)
{
    SampleModelControlsSPU(LocalStore,
                           InstanceEA,
                           BoneCount,
                           LocalPoseFillThreshold,
                           AllowedError,
                           0);
    PutTransformsToLocalPose(LocalStore, BoneCount, LocalPoseTransformsEA);
}

// TODO: probably just delete these functions, they're vestigial
static void
SampleSingleControlSPU(curve_sampling_store*     LocalStore,
                       radspu_command_ssc const* Command)
{
    SampleModelControlsSPU(LocalStore,
                           Command->InstanceEA,
                           Command->BoneCount,
                           Command->LocalPoseFillThreshold, Command->
                           AllowedError,
                           Command->ControlEA);
    PutTransformsToLocalPose(LocalStore, Command->BoneCount, Command->LocalPoseTransformsEA);
}


static void
SampleModelAnimationsAcceleratedSPU(curve_sampling_store* Store,
                                    uint64x  InstanceEA,
                                    int32x   BoneCount,
                                    uint64x  OffsetEA,
                                    uint64x  WorldPoseEA,
                                    uint64x  CompositeEA,
                                    real32   LocalPoseFillThreshold,
                                    real32   AllowedError)
{
    using namespace Vectormath::Aos;

    // Bring in the offset matrix asynchrously if necessary...
    ALIGN16(real32) OffsetBuffer[16];
    if (OffsetEA)
    {
        cellDmaGet(OffsetBuffer, OffsetEA, sizeof(OffsetBuffer), g_DMATagInput, 0, 0);
    }

    bone* SkeletonBones = SampleModelControlsSPU(Store, InstanceEA, BoneCount, LocalPoseFillThreshold, AllowedError, 0);

    // Sync to the above DMA, which should be long done, and setup the offset matrix.
    Matrix4 OffsetMatrix;
    if (OffsetEA)
    {
        cellDmaWaitTagStatusAll(g_DMAMaskInput);
        OffsetMatrix = Matrix4FromGrannyMatrix(OffsetBuffer);
    }
    else
    {
        OffsetMatrix = Matrix4::identity();
    }

    Matrix4* WorldBuffer;
    Matrix4* CompositeBuffer;
    ComputeMatrixBufferPtrs(Store->CurveDataBuffer, sizeof(Store->CurveDataBuffer),
                            WorldBuffer, CompositeBuffer);

    // Only compute composites if we've been passed a valid output address
    BuildWorldPoseSPU(Store, SkeletonBones,
                      0, BoneCount, BoneCount,
                      OffsetMatrix,
                      CompositeEA != 0);

    cellDmaLargePut(WorldBuffer, WorldPoseEA, sizeof(Vectormath::Aos::Matrix4) * BoneCount,
                    g_DMATagOutput, 0, 0);

    if (CompositeEA != 0)
    {
        cellDmaLargePut(CompositeBuffer, CompositeEA, sizeof(Vectormath::Aos::Matrix4) * BoneCount,
                        g_DMATagOutput, 0, 0);
    }
}


bool GRANNY
DispatchSampleModelCommand(int         CommandType,
                           void const* CommandBuffer)
{
    uint8* LocalStoreBuffer = (uint8*)alloca(sizeof(curve_sampling_store) + 128);
    curve_sampling_store* LocalStore = (curve_sampling_store*)(((intaddrx)LocalStoreBuffer + 127) & ~127);

    switch (CommandType)
    {
        // These should never come through...
        case SPUCommand_Shutdown:
        case SPUCommand_FenceNOP:
            return false;

        case SPUCommand_SampleModelAnims:
        {
            radspu_command_sma const* CommandSMA = (radspu_command_sma const*)CommandBuffer;
            SampleModelAnimationsSPU(LocalStore,
                                     CommandSMA->InstanceEA,
                                     CommandSMA->BoneCount,
                                     CommandSMA->LocalPoseTransformsEA,
                                     CommandSMA->LocalPoseFillThreshold,
                                     CommandSMA->AllowedError);
            break;
        }

        case SPUCommand_SampleSingleControl:
        {
            radspu_command_ssc const* CommandSSC = (radspu_command_ssc const*)CommandBuffer;
            SampleSingleControlSPU(LocalStore, CommandSSC);
            break;
        }

        case SPUCommand_SampleModelAnimAccelerated:
        {
            radspu_command_sma_accel const* CommandSMAA =
                (radspu_command_sma_accel const*)CommandBuffer;

            SampleModelAnimationsAcceleratedSPU(LocalStore,
                                                CommandSMAA->InstanceEA,
                                                CommandSMAA->BoneCount,
                                                CommandSMAA->OffsetEA,
                                                CommandSMAA->WorldPoseEA,
                                                CommandSMAA->CompositeEA,
                                                CommandSMAA->LocalPoseFillThreshold,
                                                CommandSMAA->AllowedError);
            break;
        }

        default:
            // misunderstood command!
            // InvalidCodePath("nyi");
            return true;
    }

    // Wait for the completion of output dma...
    cellDmaWaitTagStatusAll(g_DMAMaskOutput);
    return true;
}
