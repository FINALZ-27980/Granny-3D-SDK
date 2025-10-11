// ========================================================================
// $File: //jeffr/granny_29/rt/granny_controlled_animation.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_controlled_animation.h"

#include "granny_aggr_alloc.h"
#include "granny_animation.h"
#include "granny_animation_binding.h"
#include "granny_bone_operations.h"
#include "granny_control.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_mirror_specification.h"
#include "granny_model.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_periodic_loop.h"
#include "granny_prefetch.h"
#include "granny_retargeter.h"
#include "granny_skeleton.h"
#include "granny_spu_controlled_animation.h"
#include "granny_telemetry.h"
#include "granny_track_group.h"
#include "granny_track_mask.h"
#include "granny_world_pose.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#undef SubsystemCode
#define SubsystemCode ControlLogMessage

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

struct controlled_animation_frame_stats
{
    int32x FullSamples;
    int32x PartialSamples;
    int32x LODSamples;
};
controlled_animation_frame_stats g_ControlledAnimFrameStats = { 0 };

struct track_target
{
    animation_binding *Binding;
    animation_binding_identifier BindingID;

    model_instance *OnInstance;

    accumulation_mode AccumulationMode;
    real32 LODCopyScaler;

    track_mask *TrackMask;
    track_mask *ModelMask;

    mirror_specification* MirrorSpecification;
};

struct controlled_animation_builder
{
    control *Control;

    real32 CurrentClock;
    animation const *Animation;

    int32x TrackCount;
    track_target *Tracks;
};


// Ewwww. A global.
real32 Global_AllowedLODErrorFadingFactor = 0.8f;


END_GRANNY_NAMESPACE;

static bool
AnimationInitializeBindingState(model_control_binding &Binding, void *InitData)
{
    VAR_FROM_DERIVED(controlled_animation, State, Binding);
    Assert(State);

    track_target *TrackTarget = (track_target *)InitData;
    Assert(TrackTarget);

    if(TrackTarget->Binding)
        State->Binding = AcquireAnimationBinding(TrackTarget->Binding);
    else
        State->Binding = AcquireAnimationBindingFromID(TrackTarget->BindingID);

    if (State->Binding)
    {
        State->AccumulationMode = TrackTarget->AccumulationMode;
        State->TrackMask        = TrackTarget->TrackMask;
        State->ModelMask        = TrackTarget->ModelMask;
        State->MirrorSpec       = TrackTarget->MirrorSpecification;

        // Odd, but this can actually happen when UseExistingControlForAnimation
        // is brought into play apparently.
        Assert(State->Binding->TrackBindings || State->Binding->TrackBindingCount == 0);

        Binding.BindingType = ControlledAnim;
    }
    else
    {
        // Zero this out just to be sure...
        ZeroStructure(*State);
    }

    return (State->Binding != 0);
}

void GRANNY
FindAllowedErrorNumbers ( real32 AllowedError, real32 *AllowedErrorEnd, real32 *AllowedErrorScaler )
{
    // So the fade out begins at AllowedError and ends at AllowedErrorEnd - below AllowedErrorEnd, we just sample the first value.
    *AllowedErrorEnd = 0.0f;
    *AllowedErrorScaler = 0.0f;
    if ( AllowedError > 0.0f )
    {
        *AllowedErrorEnd = AllowedError * Global_AllowedLODErrorFadingFactor;
        *AllowedErrorScaler = 1.0f / ( AllowedError - *AllowedErrorEnd );
    }
}

bool GRANNY
CheckLODTransform ( bound_transform_track const &Track, transform_track const &SourceTrack )
{
#if DEBUG && defined(BOUND_TRANSFORM_TRACK_HAS_LOD)
    // To make sure my LOD stuff is working properly.
    transform RealTransform;
    SampleTrackUUULocalAtTime0 ( &SourceTrack, RealTransform );
    transform const &LODTransform = Track.LODTransform;
    triple v;

    Assert ( LODTransform.Flags == RealTransform.Flags );
    if ( RealTransform.Flags & HasOrientation )
    {
        Assert ( InnerProduct4 ( LODTransform.Orientation, RealTransform.Orientation ) > 0.999f );
    }
    if ( RealTransform.Flags & HasPosition )
    {
        VectorSubtract3 ( v, LODTransform.Position, RealTransform.Position );
        Assert ( VectorLengthSquared3 ( v ) < 0.0001f );
    }
    if ( RealTransform.Flags & HasScaleShear )
    {
        VectorSubtract3 ( v, LODTransform.ScaleShear[0], RealTransform.ScaleShear[0] );
        Assert ( VectorLengthSquared3 ( v ) < 0.0001f );
        VectorSubtract3 ( v, LODTransform.ScaleShear[1], RealTransform.ScaleShear[1] );
        Assert ( VectorLengthSquared3 ( v ) < 0.0001f );
        VectorSubtract3 ( v, LODTransform.ScaleShear[2], RealTransform.ScaleShear[2] );
        Assert ( VectorLengthSquared3 ( v ) < 0.0001f );
    }
#endif

    return true;
}


accumulation_mode GRANNY
AccumulationModeFromTrackGroup(track_group& TrackGroup)
{
    uint32 Flags, VDADOFs;
    GetTrackGroupFlags(TrackGroup, Flags, VDADOFs);
    if(Flags & AccumulationExtracted)
    {
        return ConstantExtractionAccumulation;
    }
    else if(Flags & AccumulationIsVDA)
    {
        return VariableDeltaAccumulation;
    }
    else
    {
        return NoAccumulation;
    }
}


bool GRANNY
ControlledAnimIsMirrored(controlled_animation& State)
{
    return (State.MirrorSpec != 0);
}


#define REMAP_INDEX(Index, Remapper) do { if (Remapper) { Index = Remapper[Index]; } } while (false)

void GRANNY
AccumulateControlledAnimation(controlled_animation& State,
                              sample_context& Context,
                              real32 BaseWeight,
                              skeleton& Skeleton,
                              int32x FirstBone,
                              int32x BoneCount,
                              local_pose &Result,
                              real32 AllowedError,
                              int32x const *SparseBoneArray)
{
    Assert(!ControlledAnimIsMirrored(State));
    Assert(&Skeleton != NULL);

    CheckInt32Index(FirstBone, Skeleton.BoneCount, return);
    CheckCondition(FirstBone + BoneCount <= Skeleton.BoneCount, return);

    retargeter* Retargeter  = 0;
    if (State.Binding)
    {
        Retargeter = State.Binding->Retargeter;
    }

    real32 AllowedErrorEnd;
    real32 AllowedErrorScaler;
    FindAllowedErrorNumbers ( AllowedError, &AllowedErrorEnd, &AllowedErrorScaler );

    if (BaseWeight <= TrackWeightEpsilon)
        return;

    {for(int32x LocalPoseBoneIndex = FirstBone;
         LocalPoseBoneIndex < (FirstBone + BoneCount);
         ++LocalPoseBoneIndex)
    {
        // Prefetches
        //  Note that this looks pretty hefty, but here's what's going on.
        //  Fetch the bound_transform_track N indices ahead
        //  Fetch the transform_track N-1 indices ahead
        //  Fetch the curves N-2 indices ahead
        const int32x SampleAhead = 4;
        {
            if (LocalPoseBoneIndex < (FirstBone + BoneCount) - SampleAhead)
            {
                int32x SkeletonBoneIndex = LocalPoseBoneIndex+SampleAhead;
                REMAP_INDEX(SkeletonBoneIndex, SparseBoneArray);

                PrefetchAddress(&State.Binding->TrackBindings[SkeletonBoneIndex], sizeof(bound_transform_track));
            }
            if (LocalPoseBoneIndex < (FirstBone + BoneCount) - (SampleAhead-1))
            {
                int32x SkeletonBoneIndex = LocalPoseBoneIndex+(SampleAhead-1);
                REMAP_INDEX(SkeletonBoneIndex, SparseBoneArray);

                if (SkeletonBoneIndex != -1)
                {
                    bound_transform_track &Track = State.Binding->TrackBindings[SkeletonBoneIndex];
                    if (Track.SourceTrack)
                        PrefetchAddress(Track.SourceTrack, sizeof(*Track.SourceTrack));
                }
            }
            if (LocalPoseBoneIndex < (FirstBone + BoneCount) - (SampleAhead-2))
            {
                int32x SkeletonBoneIndex = LocalPoseBoneIndex+(SampleAhead-2);
                REMAP_INDEX(SkeletonBoneIndex, SparseBoneArray);

                if (SkeletonBoneIndex != -1)
                {
                    bound_transform_track &Track = State.Binding->TrackBindings[SkeletonBoneIndex];
                    if (Track.SourceTrack)
                        PrefetchAddress(Track.SourceTrack->OrientationCurve.CurveData.Object, 384);
                }
            }
        }

        int SkeletonBoneIndex = LocalPoseBoneIndex;
        REMAP_INDEX(SkeletonBoneIndex, SparseBoneArray);

        Assert(SkeletonBoneIndex >= 0 && SkeletonBoneIndex < State.Binding->TrackBindingCount);
        bound_transform_track &Track = State.Binding->TrackBindings[SkeletonBoneIndex];
        if(Track.SourceTrack)
        {
            transform_track const &SourceTrack = *Track.SourceTrack;

            real32 Weight = BaseWeight;

            if(State.TrackMask)
            {
                Weight *= GetTrackMaskBoneWeight(*State.TrackMask, Track.SourceTrackIndex);
            }

            if(State.ModelMask)
            {
                Weight *= GetTrackMaskBoneWeight(*State.ModelMask, SkeletonBoneIndex);
            }

            if(Weight > TrackWeightEpsilon)
            {
                transform SampledTransform;
                // Note careful setting of the == case. If the AllowedError is zero (i.e. they want no LOD),
                // we can still use the LOD if the error is also zero. This happens
                // when the track is constant (which is pretty common), and it's
                // quite a bit faster to do the LOD thing than to actually go
                // and sample it, because the number of calls and memory hits
                // is lower.
                if ( AllowedErrorEnd >= Track.LODError )
                {
                    // Cool - we don't need to sample this track properly,
                    // just use the most convenient (first) value.
                    Assert ( AllowedError >= Track.LODError );

#if BOUND_TRANSFORM_TRACK_HAS_LOD
                    GRANNY_INC_INT_ACCUMULATOR(++g_ControlledAnimFrameStats.LODSamples);
                    Assert(CheckLODTransform(Track, SourceTrack));
                    SampledTransform = Track.LODTransform;
#else
                    SampleTrackUUULocalAtTime0 ( &SourceTrack, SampledTransform );
#endif
                }
                else if ( AllowedError >= Track.LODError )
                {
                    // Some blend between correct and approximated. Doesn't have to be in any way
                    // "right" (we're below the error threshold), just has to be smooth.
                    Assert ( AllowedErrorEnd < Track.LODError );

#if BOUND_TRANSFORM_TRACK_HAS_LOD
                    Assert(CheckLODTransform(Track, SourceTrack));
                    transform const &SampledTransformApprox = Track.LODTransform;
#else
                    transform SampledTransformApprox;
                    SampleTrackUUULocalAtTime0 ( &SourceTrack, SampledTransformApprox );
#endif

                    GRANNY_INC_INT_ACCUMULATOR(++g_ControlledAnimFrameStats.PartialSamples);
                    real32 BlendFactor = ( AllowedError - Track.LODError ) * AllowedErrorScaler;
                    SampleTrackUUULocal(Context, &SourceTrack, &Track,
                                        SampledTransform);

                    LinearBlendTransform ( SampledTransform, SampledTransform, BlendFactor, SampledTransformApprox );
                }
                else
                {
                    // No LOD.
                    Assert ( AllowedError < Track.LODError );
                    Assert ( AllowedErrorEnd < Track.LODError );

                    GRANNY_INC_INT_ACCUMULATOR(++g_ControlledAnimFrameStats.FullSamples);
                    SampledTransform.Flags = 0;
                    SampleTrackUUULocal(Context, &SourceTrack, &Track,
                                        SampledTransform);
                }

                if((State.AccumulationMode == VariableDeltaAccumulation) && (SkeletonBoneIndex == 0))
                {
                    VectorZero3(SampledTransform.Position);
                    VectorEquals4(SampledTransform.Orientation, GlobalWAxis);
                    // But don't kill any scale/shear.
                }

                if (Retargeter)
                {
                    // We need to remap this transform
                    RebasingTransform(
                        SampledTransform,
                        SampledTransform,
                        Retargeter->RetargetingTransforms[SkeletonBoneIndex]);
                }

                AccumulateLocalTransform(Result, LocalPoseBoneIndex, SkeletonBoneIndex, Weight,
                                         Skeleton, quaternion_mode(Track.QuaternionMode),
                                         SampledTransform);
            }
        }
    }}
}

void GRANNY
AccumulateMirroredAnimation(controlled_animation& State,
                            sample_context& Context,
                            real32 BaseWeight,
                            model_instance& Instance,
                            skeleton& Skeleton, int32x FirstBone, int32x BoneCount,
                            local_pose &Result,
                            real32 AllowedError,
                            int32x const* SparseBoneArray)
{
    Assert(ControlledAnimIsMirrored(State));

    real32 AllowedErrorEnd;
    real32 AllowedErrorScaler;
    FindAllowedErrorNumbers ( AllowedError, &AllowedErrorEnd, &AllowedErrorScaler );

    if (BaseWeight <= TrackWeightEpsilon)
        return;

    // For a mirrored animation, we need to sample /everything/ in the skeleton, since we
    // have to mirror in object space.  Sample into the mirror transform cache that should
    // be present on the model_instance...
    CheckPointerNotNull(Instance.MirrorTransformCache, return);

    {for (int32x BoneIdx = 0; BoneIdx < Skeleton.BoneCount; ++BoneIdx)
    {
        Assert(BoneIdx < State.Binding->TrackBindingCount);

        transform& BoneTransform = Instance.MirrorTransformCache[BoneIdx];
        BoneTransform = Skeleton.Bones[BoneIdx].LocalTransform;

        bound_transform_track& Track = State.Binding->TrackBindings[BoneIdx];
        if (Track.SourceTrack)
        {
            transform_track const &SourceTrack = *Track.SourceTrack;

            real32 Weight = BaseWeight;

            if (State.TrackMask)
                Weight *= GetTrackMaskBoneWeight(*State.TrackMask, Track.SourceTrackIndex);

            if (State.ModelMask)
                Weight *= GetTrackMaskBoneWeight(*State.ModelMask, BoneIdx);

            if (Weight > TrackWeightEpsilon)
            {
                // See notes in AccumulateControlledAnimation for LOD details...
                if ( AllowedErrorEnd >= Track.LODError )
                {
                    Assert ( AllowedError >= Track.LODError );
#if BOUND_TRANSFORM_TRACK_HAS_LOD
                    GRANNY_INC_INT_ACCUMULATOR(++g_ControlledAnimFrameStats.LODSamples);
                    Assert(CheckLODTransform(Track, SourceTrack));
                    BoneTransform = Track.LODTransform;
#else
                    SampleTrackUUULocalAtTime0 ( &SourceTrack, BoneTransform );
#endif
                }
                else if ( AllowedError >= Track.LODError )
                {
                    // Some blend between correct and approximated. Doesn't have to be in any way
                    // "right" (we're below the error threshold), just has to be smooth.
                    Assert ( AllowedErrorEnd < Track.LODError );
#if BOUND_TRANSFORM_TRACK_HAS_LOD
                    Assert(CheckLODTransform(Track, SourceTrack));
                    transform const &BoneTransformApprox = Track.LODTransform;
#else
                    transform BoneTransformApprox;
                    SampleTrackUUULocalAtTime0 ( &SourceTrack, BoneTransformApprox );
#endif
                    real32 BlendFactor = ( AllowedError - Track.LODError ) * AllowedErrorScaler;

                    GRANNY_INC_INT_ACCUMULATOR(++g_ControlledAnimFrameStats.PartialSamples);
                    SampleTrackUUULocal(Context, &SourceTrack, &Track, BoneTransform);
                    LinearBlendTransform(BoneTransform, BoneTransform, BlendFactor, BoneTransformApprox);
                }
                else
                {
                    // No LOD.
                    Assert ( AllowedError < Track.LODError );
                    Assert ( AllowedErrorEnd < Track.LODError );

                    GRANNY_INC_INT_ACCUMULATOR(++g_ControlledAnimFrameStats.FullSamples);
                    BoneTransform.Flags = 0;
                    SampleTrackUUULocal(Context, &SourceTrack, &Track, BoneTransform);
                }

                if ((State.AccumulationMode == VariableDeltaAccumulation) && (BoneIdx == 0))
                {
                    VectorZero3(BoneTransform.Position);
                    VectorEquals4(BoneTransform.Orientation, GlobalWAxis);
                    // But don't kill any scale/shear.
                }
            }
        }
    }}

    // Do the mirroring
    MaskedMirrorPoseTransforms(State.MirrorSpec,
                               SizeOf(transform), Instance.MirrorTransformCache,
                               SizeOf(bone), &Skeleton.Bones[0].ParentIndex,
                               Skeleton.BoneCount, State.ModelMask);

    retargeter* Retargeter = State.Binding->Retargeter;
    {for(int32x LocalPoseBoneIndex = FirstBone; LocalPoseBoneIndex < (FirstBone + BoneCount); ++LocalPoseBoneIndex)
    {
        int SkeletonBoneIndex = LocalPoseBoneIndex;
        REMAP_INDEX(SkeletonBoneIndex, SparseBoneArray);

        Assert(SkeletonBoneIndex >= 0 && SkeletonBoneIndex < State.Binding->TrackBindingCount);
        bound_transform_track &Track = State.Binding->TrackBindings[SkeletonBoneIndex];
        if (Track.SourceTrack)
        {
            real32 Weight = BaseWeight;
            if (State.TrackMask)
                Weight *= GetTrackMaskBoneWeight(*State.TrackMask, Track.SourceTrackIndex);

            if (State.ModelMask)
                Weight *= GetTrackMaskBoneWeight(*State.ModelMask, SkeletonBoneIndex);

            if (Weight > TrackWeightEpsilon)
            {
                if (Retargeter)
                {
                    // We need to remap this transform
                    RebasingTransform(
                        Instance.MirrorTransformCache[SkeletonBoneIndex],
                        Instance.MirrorTransformCache[SkeletonBoneIndex],
                        Retargeter->RetargetingTransforms[SkeletonBoneIndex]);
                }

                AccumulateLocalTransform(Result, LocalPoseBoneIndex, SkeletonBoneIndex, Weight,
                                         Skeleton, BlendQuaternionNeighborhooded, //quaternion_mode(Track.QuaternionMode),
                                         Instance.MirrorTransformCache[SkeletonBoneIndex]);
            }
        }
    }}
}


static void
AnimationAccumulateBindingState(model_control_binding& Binding,
                                int32x const           FirstBone,
                                int32x const           BoneCount,
                                local_pose&            Result,
                                real32                 AllowedError,
                                int32x const*          SparseBoneArray)
{
    GRANNY_AUTO_ZONE_FN();

    VAR_FROM_DERIVED(controlled_animation, State, Binding);
    Assert(State);

    real32 const ControlWeight = GetControlEffectiveWeight(*Binding.Control);
    if(ControlWeight <= TrackWeightEpsilon)
        return;

    sample_context Context;
    Context.LocalClock = GetControlClampedLocalClock(*Binding.Control);
    SetContextFrameIndex(Context,
                         Context.LocalClock,
                         State->Binding->ID.Animation->Duration,
                         State->Binding->ID.Animation->TimeStep);

    Context.LocalDuration = GetControlLocalDuration(*Binding.Control);
    GetControlLoopState(*Binding.Control,
                        Context.UnderflowLoop,
                        Context.OverflowLoop);

    if (!ControlledAnimIsMirrored(*State))
    {
        AccumulateControlledAnimation(*State, Context, ControlWeight,
                                      *GetSourceSkeleton(*Binding.ModelInstance),
                                      FirstBone, BoneCount,
                                      Result, AllowedError,
                                      SparseBoneArray);
    }
    else
    {
        Assert(Binding.ModelInstance);

        AccumulateMirroredAnimation(*State, Context, ControlWeight,
                                    *Binding.ModelInstance,
                                    *GetSourceSkeleton(*Binding.ModelInstance),
                                    FirstBone, BoneCount,
                                    Result, AllowedError,
                                    SparseBoneArray);
    }
}


static void
AnimationBuildDirect(model_control_binding& Binding,
                     int32x const           BoneCount,
                     real32 const*          Offset4x4,
                     world_pose&            Result,
                     real32                 AllowedError)
{
    CheckPointerNotNull(Offset4x4, return);
    CheckCondition(IS_ALIGNED_16(Offset4x4), return);

    VAR_FROM_DERIVED(controlled_animation, State, Binding);
    Assert(State);
    Assert(ControlledAnimIsMirrored(*State) == false);

    real32 AllowedErrorEnd;
    real32 AllowedErrorScaler;
    FindAllowedErrorNumbers ( AllowedError, &AllowedErrorEnd, &AllowedErrorScaler );

    sample_context Context;
    Context.LocalClock    = GetControlClampedLocalClockInline(*Binding.Control);
    Context.LocalDuration = GetControlLocalDurationInline(*Binding.Control);
    GetControlLoopStateInline(*Binding.Control,
                              Context.UnderflowLoop,
                              Context.OverflowLoop);
    if (State->Binding && State->Binding->ID.Animation)
    {
        SetContextFrameIndex(Context,
                             Context.LocalClock,
                             State->Binding->ID.Animation->Duration,
                             State->Binding->ID.Animation->TimeStep);
    }
    else
    {
        Context.FrameIndex = 0;
    }

    matrix_4x4 *WorldBuffer = Result.WorldTransformBuffer;

    skeleton *Skeleton = Binding.ModelInstance->Model->Skeleton;
    bone *Bone = Skeleton->Bones;
    matrix_4x4 *World = WorldBuffer;

    int32x TrackBoneCount = BoneCount;
    bound_transform_track *Track = State->Binding->TrackBindings;
    if (TrackBoneCount > State->Binding->TrackBindingCount)
    {
        TrackBoneCount = State->Binding->TrackBindingCount;
    }

    int32x BoneIndex = 0;
    if ((State->AccumulationMode == VariableDeltaAccumulation) && Track->SourceTrack)
    {
        // VDA means we don't want the orientation or position,
        // but we still want the scale factor.
        real32 const* ParentWorld;
        if (Bone->ParentIndex == NoParentBone)
            ParentWorld = Offset4x4;
        else
            ParentWorld = (real32 const*)WorldBuffer[Bone->ParentIndex];

        GetTrackSamplerIIU()(Context, Track->SourceTrack, Track,
                             Bone->LocalTransform,
                             ParentWorld,
                             (real32*)World);

        ++BoneIndex;
        ++Bone;
        ++World;
        ++Track;
    }

    {for(; BoneIndex < TrackBoneCount; ++BoneIndex)
    {
        transform_track const *SourceTrack = Track->SourceTrack;
        real32 const* BoneParentTransform = 0;
        {
            if (Bone->ParentIndex == NoParentBone)
                BoneParentTransform = Offset4x4;
            else
                BoneParentTransform = (real32 const*)WorldBuffer[Bone->ParentIndex];
        }
        Assert(BoneParentTransform != 0);
        Assert(IS_ALIGNED_16(BoneParentTransform));

        // If we have a track_mask or a model mask, we need to check to see if the bone is
        // weighted out, in which case, we use the base transform of the bone, rather than
        // the animated transform.
        if ((State->TrackMask && GetTrackMaskBoneWeight(*State->TrackMask,
                                                        Track->SourceTrackIndex) < TrackWeightEpsilon) ||
            (State->ModelMask && GetTrackMaskBoneWeight(*State->ModelMask,
                                                        BoneIndex) < TrackWeightEpsilon))
        {
            BWP_Dispatch(&Bone->LocalTransform, BoneParentTransform, (real32*)World);
        }
        else
        {
            // Note careful setting of the == case. If the AllowedError is zero (i.e. they want no LOD),
            // we can still use the LOD if the error is also zero. This happens
            // when the track is constant (which is pretty common), and it's
            // quite a bit faster to do the LOD thing than to actually go
            // and sample it, because the number of calls and memory hits
            // is lower.
            if ( AllowedErrorEnd >= Track->LODError )
            {
                Assert ( AllowedError >= Track->LODError );
                // Cool - we don't need to sample this track properly,
                // just use the most convenient (first) value.

                SampleTrackUUUAtTime0(Context, SourceTrack, Track,
                                      Bone->LocalTransform,
                                      BoneParentTransform,
                                      (real32 *)World);
            }
            else if ( AllowedError >= Track->LODError )
            {
                Assert ( AllowedErrorEnd < Track->LODError );
                // Some blend between correct and approximated. Doesn't have to be in any way
                // "right" (we're below the error threshold), just has to be smooth.

                real32 BlendFactor = ( AllowedError - Track->LODError ) * AllowedErrorScaler;
                SampleTrackUUUBlendWithTime0(Context, SourceTrack, Track,
                                             Bone->LocalTransform,
                                             BoneParentTransform,
                                             (real32 *)World,
                                             BlendFactor);
            }
            else
            {
                Assert ( AllowedError < Track->LODError );
                Assert ( AllowedErrorEnd < Track->LODError );
                // No LOD.

                Track->Sampler(Context, SourceTrack, Track,
                               Bone->LocalTransform,
                               BoneParentTransform,
                               (real32 *)World);
            }
        }

        ++Bone;
        ++World;
        ++Track;
    }}

    // Build the composite matrices if we have them
    if (Result.CompositeTransformBuffer != 0)
    {
        BuildWorldPoseComposites(*Skeleton, 0, TrackBoneCount, Result);
    }
}


real32 GRANNY
GetGlobalLODFadingFactor ( void )
{
    return Global_AllowedLODErrorFadingFactor;
}

void GRANNY
SetGlobalLODFadingFactor ( real32 NewValue )
{
    if ( NewValue > 1.0f )
    {
        NewValue = 1.0f;
    }
    else if ( NewValue < 0.0f )
    {
        NewValue = 0.0f;
    }
    Global_AllowedLODErrorFadingFactor = NewValue;
}


void GRANNY
AccumulateControlledAnimationMotionVectors(controlled_animation* State,
                                           real32 StartLocalClock,
                                           real32 EndLocalClock,
                                           real32 LocalDuration,
                                           real32 Weight,
                                           bool   FlipResult,
                                           bound_transform_track *BoundTrack,
                                           transform_track const *SourceTrack,
                                           real32* TotalWeight,
                                           real32* ResultTranslation,
                                           real32* ResultRotation)
{
    // So now clamping has been done, and we can ignore the over/underflow looping.
    // Any loops remaining are real loops and should be performed.
    // Also, start should always be less than end - any flips should be encoded in FlipResult.
    Assert ( StartLocalClock <= EndLocalClock );

    switch (State->AccumulationMode)
    {
        case ConstantExtractionAccumulation:
        {
            // All that CME cares about is actual seconds elapsed.
            // We still need to do all the above work to figure out where it gets clamped.
            float ActualSecondsElapsed = EndLocalClock - StartLocalClock;

            periodic_loop *Loop = GetTrackGroup(State->Binding)->PeriodicLoop;
            if(Loop)
            {
                real32 TheScale = Weight;
                if ( FlipResult )
                {
                    TheScale = -Weight;
                }

                triple Movement;
                ComputePeriodicLoopVector(*Loop, ActualSecondsElapsed, Movement);
                ScaleVectorAdd3(ResultTranslation, TheScale, Movement);

                triple Rotation;
                ComputePeriodicLoopLog(*Loop, ActualSecondsElapsed, Rotation);
                ScaleVectorAdd3(ResultRotation, TheScale, Rotation);
            }
            else
            {
                real32 tWeight = (Weight*ActualSecondsElapsed / LocalDuration);
                if ( FlipResult )
                {
                    tWeight = -tWeight;
                }

                ScaleVectorAdd3(ResultTranslation, tWeight,
                                GetTrackGroup(State->Binding)->LoopTranslation);
            }

            *TotalWeight += Weight;
        } break;

        case VariableDeltaAccumulation:
        {
            // Now move both start and end so they lie in the interval [0,LocalDuration),
            // moving only in multiples of LocalDuration, and recording
            // how many multiples that was.
            int32x LoopCount = 0;
            int32x LocalLoopCount = FloorReal32ToInt32 ( StartLocalClock / LocalDuration );
            StartLocalClock -= LocalLoopCount * LocalDuration;
            LoopCount -= LocalLoopCount;
            StartLocalClock = Clamp(0.0f, StartLocalClock, LocalDuration);
            Assert ( ( StartLocalClock >= 0.0f ) && ( StartLocalClock <= LocalDuration ) );

            LocalLoopCount = FloorReal32ToInt32 ( EndLocalClock / LocalDuration );
            EndLocalClock -= LocalLoopCount * LocalDuration;
            LoopCount += LocalLoopCount;
            EndLocalClock = Clamp(0.0f, EndLocalClock, LocalDuration);
            Assert ( ( EndLocalClock >= 0.0f ) && ( EndLocalClock <= LocalDuration ) );

            // Now sample start and end times from the animation and find the delta between them.
            sample_context Context;
            // NOTE: VDA not supported on keyframed animations
            Context.LocalClock = EndLocalClock;
            Context.LocalDuration = LocalDuration;
            SetContextFrameIndex(Context, Context.LocalClock,
                State->Binding->ID.Animation->Duration,
                State->Binding->ID.Animation->TimeStep);

            Context.UnderflowLoop = false;
            Context.OverflowLoop = false;

            triple EndPosition;
            quad EndOrientation;
            SampleTrackPOLocal(Context, SourceTrack, BoundTrack,
                               EndPosition, EndOrientation);

            Context.LocalClock = StartLocalClock;
            SetContextFrameIndex(Context, Context.LocalClock,
                State->Binding->ID.Animation->Duration,
                State->Binding->ID.Animation->TimeStep);

            triple StartPosition;
            quad StartOrientation;
            SampleTrackPOLocal(Context, SourceTrack, BoundTrack,
                               StartPosition, StartOrientation);

            triple ResidualPositionDelta;
            VectorSubtract3(ResidualPositionDelta, EndPosition, StartPosition);

            triple ResidualOrientationDelta;
            QuaternionDifferenceToAngularVelocity(ResidualOrientationDelta,
                                                  StartOrientation, EndOrientation);
            if ( LoopCount != 0 )
            {
                // TODO: Note that this is not guaranteed to be accurate
                // for rotational motions, because technically you would
                // have to account for the fact that the motion can loop
                // back on itself.  I could look into supporting this later,
                // although it is unlikely anyone will care because it
                // only occurs with severely large timesteps, which could
                // always be subdivided as a workaround.
                triple TotalPositionDelta;
                triple TotalOrientationDelta;
                ComputeTotalDeltasFromBinding(State->Binding,
                                              &TotalPositionDelta,
                                              &TotalOrientationDelta);

                ScaleVectorAdd3(ResidualOrientationDelta, (real32)LoopCount, TotalOrientationDelta);
                ScaleVectorAdd3(ResidualPositionDelta, (real32)LoopCount, TotalPositionDelta);
            }

            if ( FlipResult )
            {
                VectorNegate3 ( ResidualOrientationDelta );
                VectorNegate3 ( ResidualPositionDelta );
            }

            if ( !CurveIsIdentity ( SourceTrack->OrientationCurve ) )
            {
                quad BaseOrientation;
                CurveExtractKnotValue ( SourceTrack->OrientationCurve, 0, BaseOrientation, CurveIdentityOrientation );
                NormalQuaternionTransform3(ResidualPositionDelta, BaseOrientation);
                NormalQuaternionTransform3(ResidualOrientationDelta, BaseOrientation);
            }
            else
            {
                // Orientation curve is identity.
            }

            Conjugate4(StartOrientation);
            NormalQuaternionTransform3(ResidualPositionDelta, StartOrientation);

            ScaleVectorAdd3(ResultRotation, Weight, ResidualOrientationDelta);
            ScaleVectorAdd3(ResultTranslation, Weight, ResidualPositionDelta);

            *TotalWeight += Weight;
        } break;

        case NoAccumulation:
            break;

        default:
        {
            InvalidCodePath("Unrecognized accumulation type");
        } break;
    }
}

static void
AnimationAccumulateLoopTransform(model_control_binding &Binding,
                                 real32 SecondsElapsed,
                                 real32 &TotalWeight,
                                 real32 *ResultTranslation,
                                 real32 *ResultRotation,
                                 bool Inverse)
{
    // Remember that ResultTranslation and ResultRotation are already valid,
    // and we need to accumulate this animation's motion to it.
    // (so it's not a bug that I don't initialize them to zero!)

    // Should also add a bit to the docs saying that these use the control's speed
    // to know how much local time has elapsed. If people are setting the clocks every
    // frame, then normally they don't need to set the speeds - but they still do have to
    // with CME or VDA.

    VAR_FROM_DERIVED(controlled_animation, State, Binding);
    Assert(State);

    if (State->AccumulationMode == NoAccumulation)
    {
        return;
    }

    // Find out what weight this anim is applied at
    real32 const Weight = GetControlEffectiveWeight(*Binding.Control);
    if(Weight <= TrackWeightEpsilon)
    {
        return;
    }

    // Get bone zero
    bound_transform_track *BoundTrack = State->Binding->TrackBindings;
    if(!BoundTrack)
    {
        return;
    }

    transform_track const *SourceTrack = BoundTrack->SourceTrack;
    if(!SourceTrack)
    {
        return;
    }

    real32 LocalDuration = GetControlLocalDuration(*Binding.Control);

    if ( LocalDuration <= TimeEffectivelyZero )
    {
        // Zero-length animations are not going to have any motion.
        return;
    }

    // Adjust the seconds elapsed to be in the local space of the animation
    SecondsElapsed *= GetControlSpeed(*Binding.Control);

    // Find the local clock start and end.
    real32 EndLocalClock = GetControlRawLocalClock(*Binding.Control);
    real32 StartLocalClock = EndLocalClock - SecondsElapsed;

#if DEBUG
    // Before we start modifying them.
    real32 OriginalStartLocalClock = StartLocalClock;
    real32 OriginalEndLocalClock = EndLocalClock;
    OriginalStartLocalClock = OriginalStartLocalClock;      // Stops silly warnings.
    OriginalEndLocalClock = OriginalEndLocalClock;
#endif

    bool FlipResult = Inverse;
    if ( StartLocalClock > EndLocalClock )
    {
        // Negative SecondsElapsed or control speed.
        Swap ( StartLocalClock, EndLocalClock );
        FlipResult = !FlipResult;
    }

    // Clamp the effective start and end times to the animation start
    // and end times if there is no looping on that end.
    bool32 UnderflowLoop;
    bool32 OverflowLoop;
    GetControlLoopState(*Binding.Control,
                        UnderflowLoop,
                        OverflowLoop);
    if ( !UnderflowLoop )
    {
        // Clamp off start.
        if ( EndLocalClock < 0.0f )
        {
            Assert ( StartLocalClock < 0.0f );
            // Easy case - nothing happens.
            return;
        }
        else if ( StartLocalClock < 0.0f )
        {
            StartLocalClock = 0.0f;
        }
    }
    if ( !OverflowLoop )
    {
        // Clamp off end.
        if ( StartLocalClock > LocalDuration )
        {
            Assert ( EndLocalClock > LocalDuration );
            // Easy case - nothing happens.
            return;
        }
        else if ( EndLocalClock > LocalDuration )
        {
            EndLocalClock = LocalDuration;
        }
    }

    // So now clamping has been done, and we can ignore the over/underflow looping.
    // Any loops remaining are real loops and should be performed.
    // Also, start should always be less than end - any flips should be encoded in FlipResult.
    Assert ( StartLocalClock <= EndLocalClock );

    // Use factored out function for this
    AccumulateControlledAnimationMotionVectors(State, StartLocalClock, EndLocalClock,
                                               LocalDuration, Weight, FlipResult,
                                               BoundTrack, SourceTrack,
                                               &TotalWeight, ResultTranslation, ResultRotation);
}

static void
AnimationCleanupBindingState(model_control_binding &Binding)
{
    VAR_FROM_DERIVED(controlled_animation, State, Binding);
    Assert(State);

    ReleaseAnimationBinding(State->Binding);
}

static bool
AnimationControlIsRebased(model_control_binding &Binding)
{
    VAR_FROM_DERIVED(controlled_animation, State, Binding);
    Assert(State);

    return State && State->Binding && State->Binding->Retargeter != 0;
}

static bool
AnimationControlIsMirrored(model_control_binding &Binding)
{
    VAR_FROM_DERIVED(controlled_animation, State, Binding);
    Assert(State);

    return State && State->Binding && ControlledAnimIsMirrored(*State);
}

controlled_animation *AnimationGetControlledAnimation ( model_control_binding &Binding )
{
    VAR_FROM_DERIVED(controlled_animation, State, Binding);
    Assert(State);

    return State;
}

controlled_pose *AnimationGetControlledPose ( model_control_binding &Binding )
{
    // It's not a pose!
    return NULL;
}

spu_controlled_animation* AnimationGetSPUControlledAnimation ( model_control_binding &Binding )
{
    // It's not an SPU animation!
    return NULL;
}


static model_control_callbacks ControlledAnimationCallbacks =
{
    AnimationGetControlledAnimation,
    AnimationGetControlledPose,
    AnimationGetSPUControlledAnimation,
    AnimationInitializeBindingState,
    AnimationAccumulateBindingState,
    AnimationBuildDirect,
    AnimationAccumulateLoopTransform,
    AnimationCleanupBindingState,
    AnimationControlIsRebased,
    AnimationControlIsMirrored
};



control *GRANNY
PlayControlledAnimation(real32 StartTime,
                        animation const &Animation,
                        model_instance &Model)
{
    GRANNY_AUTO_ZONE_FN();

    control *Result = 0;

    int32x TrackGroupIndex;
    if(FindTrackGroupForModel(Animation,
                              GetSourceModel(Model).Name,
                              TrackGroupIndex))
    {
        controlled_animation_builder *Builder =
            BeginControlledAnimation(StartTime, Animation);
        if(Builder)
        {
            SetTrackGroupTarget(*Builder, TrackGroupIndex, Model);
            SetTrackGroupLOD(*Builder, TrackGroupIndex, true, 1.0f);
            Result = EndControlledAnimation(Builder);
        }
    }
    else
    {
        Log2(WarningLogMessage, ControlLogMessage,
             "Unable to find matching track_group for Model: \"%s\" in Animation: \"%s\"\n",
             GetSourceModel(Model).Name, Animation.Name);
    }

    return(Result);
}

control *GRANNY
PlayControlledAnimationBinding(real32 StartTime,
                               animation const &Animation,
                               animation_binding &Binding,
                               model_instance &Model)
{
    control *Result = 0;

    int32x TrackGroupIndex;
    if(FindTrackGroupForModel(Animation,
                              GetSourceModel(Model).Name,
                              TrackGroupIndex))
    {
        controlled_animation_builder *Builder =
            BeginControlledAnimation(StartTime, Animation);
        if(Builder)
        {
            SetTrackGroupBinding(*Builder, TrackGroupIndex, Binding);
            SetTrackGroupTarget(*Builder, TrackGroupIndex, Model);
            SetTrackGroupLOD(*Builder, TrackGroupIndex, true, 1.0f);
            Result = EndControlledAnimation(Builder);
        }
    }

    return(Result);
}

controlled_animation_builder *GRANNY
BeginControlledAnimation(real32 StartTime, animation const &Animation)
{
    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    controlled_animation_builder *Builder;
    AggrAllocPtr(Allocator, Builder);
    AggrAllocOffsetArrayPtr(Allocator, Builder,
                            Animation.TrackGroupCount,
                            TrackCount, Tracks);
    if(EndAggrAlloc(Allocator, AllocationBuilder))
    {
        Builder->Control = 0;
        Builder->CurrentClock = StartTime;
        Builder->Animation = &Animation;

        {for(int32x TrackIndex = 0; TrackIndex < Builder->TrackCount; ++TrackIndex)
        {
            track_target &Track = Builder->Tracks[TrackIndex];

            Track.Binding = 0;
            MakeDefaultAnimationBindingID(Track.BindingID,
                                          &Animation, TrackIndex);

            Track.OnInstance = 0;
            Track.TrackMask = 0;
            Track.ModelMask = 0;
            // Means "no LOD please"
            Track.LODCopyScaler = -1.0f;
            Track.AccumulationMode =
                AccumulationModeFromTrackGroup(*GetTrackGroup(Track.BindingID));

            Track.MirrorSpecification = 0;
        }}
    }

    return(Builder);
}

control *GRANNY
EndControlledAnimation(controlled_animation_builder *Builder)
{
    control* Control = 0;

    if (Builder)
    {
        Control = Builder->Control;

        if (!Control)
        {
            // Create the control
            Control = CreateControl(Builder->CurrentClock, Builder->Animation->Duration);
        }

        if (Control)
        {
            // Adjust the loop count if necessary
            if ((Builder->Animation->Flags & AnimationDefaultLoopCountValid) != 0)
            {
                CheckCondition(Builder->Animation->DefaultLoopCount >= 0, FreeControl(Control); return 0);
                SetControlLoopCount(*Control, Builder->Animation->DefaultLoopCount);
            }

            CheckBoundedReal32(0, Control->LocalDuration, Builder->Animation->Duration, FreeControl(Control); return 0);

            // Create the bindings
            {for (int32x TrackIndex = 0; TrackIndex < Builder->TrackCount; ++TrackIndex)
            {
                track_target &Track = Builder->Tracks[TrackIndex];
                if (Track.OnInstance)
                {
                    model_control_binding *Binding =
                        CreateModelControlBinding(ControlledAnimationCallbacks, *Control,
                                                  *Track.OnInstance,
                                                  ControlIsActive(*Control), &Track);
                    if (Binding)
                    {
                        // All is good
                        if ( Track.LODCopyScaler >= 0.0f )
                        {
                            // Copy over the LOD values.
                            animation_binding *AnimationBinding = GetAnimationBindingFromControlBinding ( *Binding );
                            if (AnimationBinding != NULL)
                            {
                                CopyLODErrorValuesFromAnimation ( *AnimationBinding, Track.LODCopyScaler );
                            }
                            else
                            {
                                InvalidCodePath("AnimationBinding should never be missing here");
                            }
                        }
                    }
                    else
                    {
                        Log0(ErrorLogMessage, ControlLogMessage, "Unable to bind track group");
                    }
                }
            }}
        }
        else
        {
            Log0(ErrorLogMessage, ControlLogMessage, "Unable to create control");
        }

        Deallocate(Builder);
    }

    return Control;
}

void GRANNY
UseExistingControlForAnimation(controlled_animation_builder *Builder, control *Control)
{
    Builder->Control = Control;
}

void GRANNY
SetTrackMatchRule(controlled_animation_builder &Builder,
                  int32x TrackGroupIndex,
                  char const *TrackPattern,
                  char const *BonePattern)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    track_target &Track = Builder.Tracks[TrackGroupIndex];

    Track.BindingID.TrackPattern = TrackPattern;
    Track.BindingID.BonePattern = BonePattern;
}

void GRANNY
SetTrackMatchMapping(controlled_animation_builder &Builder,
                     int32x TrackGroupIndex,
                     int32* MappingFromTrackToBone,
                     int32x MapCount)
{
    // Be super paranoid...
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    CheckPointerNotNull(MappingFromTrackToBone, return);

    track_target& TrackTarget = Builder.Tracks[TrackGroupIndex];

    CheckPointerNotNull(TrackTarget.OnInstance, return);
    CheckPointerNotNull(TrackTarget.BindingID.Animation, return);

    animation const* Animation = TrackTarget.BindingID.Animation;
    CheckInt32Index(TrackGroupIndex, Animation->TrackGroupCount, return);

    track_group const& TrackGroup = *Animation->TrackGroups[TrackGroupIndex];

    skeleton* Skeleton = GetSourceSkeleton(*TrackTarget.OnInstance);
    CheckPointerNotNull(Skeleton, return);

    CheckCondition(MapCount == TrackGroup.TransformTrackCount, return);
    {for (int32x TrackIdx = 0; TrackIdx < TrackGroup.TransformTrackCount; ++TrackIdx)
    {
        CheckCondition(MappingFromTrackToBone[TrackIdx] == -1 ||
                       (MappingFromTrackToBone[TrackIdx] >= 0 &&
                        MappingFromTrackToBone[TrackIdx] < Skeleton->BoneCount), return);
    }}

    TrackTarget.BindingID.TrackMapping = MappingFromTrackToBone;
}

void GRANNY
SetTrackGroupTarget(controlled_animation_builder &Builder, int32x TrackGroupIndex,
                    model_instance &Model)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    track_target &Track = Builder.Tracks[TrackGroupIndex];

    Track.OnInstance = &Model;
    Track.BindingID.OnModel = &GetSourceModel(Model);
}

void GRANNY
SetTrackGroupBinding(controlled_animation_builder &Builder,
                     int32x TrackGroupIndex,
                     animation_binding &Binding)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    track_target &Track = Builder.Tracks[TrackGroupIndex];

    Track.Binding = &Binding;
}

void GRANNY
SetTrackGroupBasisTransform(controlled_animation_builder &Builder, int32x TrackGroupIndex,
                            model &FromModel, model &ToModel)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    track_target &Track = Builder.Tracks[TrackGroupIndex];

    Track.BindingID.FromBasis = &FromModel;
    Track.BindingID.ToBasis = &ToModel;
}

void GRANNY
SetTrackGroupTrackMask(controlled_animation_builder &Builder,
                       int32x TrackGroupIndex, track_mask &TrackMask)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    track_target &Track = Builder.Tracks[TrackGroupIndex];

    Track.TrackMask = &TrackMask;
}


void GRANNY
SetTrackGroupModelMask(controlled_animation_builder &Builder,
                       int32x TrackGroupIndex, track_mask &ModelMask)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    track_target &Track = Builder.Tracks[TrackGroupIndex];

    Track.ModelMask = &ModelMask;
}


void GRANNY
SetTrackGroupAccumulation(controlled_animation_builder &Builder,
                          int32x TrackGroupIndex,
                          accumulation_mode Mode)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    track_target &Track = Builder.Tracks[TrackGroupIndex];

    Track.AccumulationMode = Mode;
}


void GRANNY
SetTrackGroupLOD(controlled_animation_builder &Builder,
                 int32x TrackGroupIndex,
                 bool CopyValuesFromAnimation,
                 real32 ManualScaler)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    track_target &Track = Builder.Tracks[TrackGroupIndex];

    if ( CopyValuesFromAnimation )
    {
        Track.LODCopyScaler = ManualScaler;
    }
    else
    {
        Track.LODCopyScaler = -1.0f;
    }
}


void GRANNY
SetTrackGroupMirrorSpecification(controlled_animation_builder& Builder,
                                 int32x TrackGroupIndex,
                                 mirror_specification& Specification)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    CheckPointerNotNull(Builder.Tracks[TrackGroupIndex].OnInstance, return);

    track_target &Track = Builder.Tracks[TrackGroupIndex];
    Track.MirrorSpecification = &Specification;

    CreateMirrorTransformCache(*Builder.Tracks[TrackGroupIndex].OnInstance);
}


animation_binding *GRANNY
GetAnimationBindingFromControlBinding(model_control_binding &Binding)
{
    controlled_animation *Anim = Binding.Callbacks->GetControlledAnimation ( Binding );
    if ( Anim != NULL )
    {
        return Anim->Binding;
    }
    else
    {
        return NULL;
    }
}


spu_animation_binding* GRANNY
GetSPUAnimationBindingFromControlBinding(model_control_binding &Binding)
{
    spu_controlled_animation *Anim = Binding.Callbacks->GetSPUControlledAnimation ( Binding );
    if ( Anim != NULL )
    {
        return Anim->Binding;
    }
    else
    {
        return NULL;
    }
}


BEGIN_GRANNY_NAMESPACE;
void ControlledAnimationFrameStats()
{
    GRANNY_EMIT_INT_ACCUMULATOR("Granny/TrackSamples/Full",    g_ControlledAnimFrameStats.FullSamples);
    GRANNY_EMIT_INT_ACCUMULATOR("Granny/TrackSamples/Partial", g_ControlledAnimFrameStats.PartialSamples);
    GRANNY_EMIT_INT_ACCUMULATOR("Granny/TrackSamples/LOD",     g_ControlledAnimFrameStats.LODSamples);
}
END_GRANNY_NAMESPACE;
