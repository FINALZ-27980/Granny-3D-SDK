// ========================================================================
// $File: //jeffr/granny_29/rt/granny_mirror_specification.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_mirror_specification.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_memory.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
#include "granny_telemetry.h"
#include "granny_track_mask.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode AnimationLogMessage
USING_GRANNY_NAMESPACE;

#if !PROCESSOR_CELL_SPU

mirror_specification* GRANNY
NewMirrorSpecification(int32x BoneCount, mirror_axis MirrorAround)
{
    CheckCondition(BoneCount > 0, return 0);
    CheckCondition((MirrorAround == MirrorXAxis ||
                    MirrorAround == MirrorYAxis ||
                    MirrorAround == MirrorZAxis), return 0);

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);
    SetAggrAlignment(Allocator, 16); // for the spus...

    mirror_specification* Specification = 0;
    AggrAllocPtr(Allocator, Specification);
    AggrAllocOffsetArrayPtr(Allocator, Specification, BoneCount, RemapCount, Remaps);
    AggrAllocOffsetCountlessArrayPtr(Allocator, Specification, BoneCount, PostFlips);

    if (EndAggrAlloc(Allocator, AllocationInstance))
    {
        Specification->MirrorAxis = MirrorAround;

        {for (int32x Bone = 0; Bone < BoneCount; ++Bone)
        {
            Specification->Remaps[Bone]    = (uint16)Bone;
            Specification->PostFlips[Bone] = (uint8)MirrorAround;
        }}
    }

    return Specification;
}

void GRANNY
FreeMirrorSpecification(mirror_specification* Specification)
{
    Deallocate(Specification);
}

bool GRANNY
BuildMirroringIndices(mirror_specification* Specification,
                      skeleton* Skeleton,
                      mirror_name_callback* NameCallback,
                      void *UserData)
{
    CheckPointerNotNull(Skeleton, return false);
    CheckPointerNotNull(Specification, return false);
    CheckPointerNotNull(NameCallback, return false);
    CheckCondition(Skeleton->BoneCount == Specification->RemapCount, return false);

    char MirrorBuffer[256];

    bool AllSuccessful = true;
    {for (int32x Bone = 0; Bone < Specification->RemapCount; ++Bone)
    {
        MirrorBuffer[0] = 0;
        if (NameCallback(Skeleton->Bones[Bone].Name, MirrorBuffer, ArrayLength(MirrorBuffer), UserData))
        {
            int32x TempIndex;
            if (FindBoneByNameLowercase(Skeleton, MirrorBuffer, TempIndex))
            {
                CheckBoundedInt32(0, TempIndex, Skeleton->BoneCount - 1, return false);
                Specification->Remaps[Bone] = (uint16)TempIndex;
            }
            else
            {
                AllSuccessful = false;
            }
        }
        else
        {
            AllSuccessful = false;
        }
    }}

    if (!AllSuccessful)
    {
        Log1(ErrorLogMessage, AnimationLogMessage,
             "Failed to construct mirroring remap buffer for Skeleton: '%s'\n",
             Skeleton->Name);
        return false;
    }

    transform* Transforms = AllocateArray(Skeleton->BoneCount, transform, AllocationTemporary);
    BuildSkeletonRelativeTransforms(sizeof(bone), &Skeleton->Bones[0].LocalTransform,
                                    sizeof(bone), &Skeleton->Bones[0].ParentIndex,
                                    Skeleton->BoneCount,
                                    sizeof(transform), Transforms);

    {for (int32x Bone = 0; Bone < Skeleton->BoneCount; ++Bone)
    {
        Assert(Specification->Remaps[Bone] < Skeleton->BoneCount);

        real32 MatrixFrom[3][3];
        real32 MatrixTo[3][3];
        BuildCompositeTransform(Transforms[Bone],                        3, (real32 *)MatrixFrom);
        BuildCompositeTransform(Transforms[Specification->Remaps[Bone]], 3, (real32 *)MatrixTo);

        MatrixFrom[0][Specification->MirrorAxis] *= -1;
        MatrixFrom[1][Specification->MirrorAxis] *= -1;
        MatrixFrom[2][Specification->MirrorAxis] *= -1;

        // To figure out which axis to flip, we're going to look at the dot product
        // between the before and after axes.  If the dot is negative, we need to post
        // flip.
        //
        // todo: need better error checking here.  Technically, we should find either one
        // axis has a negative dot, or all three have a negative dot.
        // real32 Dots[3] = { InnerProduct3(MatrixFrom[0], MatrixTo[0]),
        //                    InnerProduct3(MatrixFrom[1], MatrixTo[1]),
        //                    InnerProduct3(MatrixFrom[2], MatrixTo[2]) };
        float Dots[3] = { 0, 0, 0 };
        for (int i = 0; i < 3; ++i)
        {
            Dots[0] += MatrixFrom[0][i] * MatrixTo[0][i];
            Dots[1] += MatrixFrom[1][i] * MatrixTo[1][i];
            Dots[2] += MatrixFrom[2][i] * MatrixTo[2][i];
        }


        // todo: better error checking
        if (Dots[0] < 0 && Dots[1] < 0 && Dots[2] < 0)
            Specification->PostFlips[Bone] = MirrorAllAxes;
        else if (Dots[0] < 0)
            Specification->PostFlips[Bone] = MirrorXAxis;
        else if (Dots[1] < 0)
            Specification->PostFlips[Bone] = MirrorYAxis;
        else
            Specification->PostFlips[Bone] = MirrorZAxis;
    }}

    Deallocate(Transforms);

    return true;
}

#endif // PROCESSOR_CELL_SPU

static transform const sAxisFlipper[4] = {
    { HasScaleShear, { 0, 0, 0 }, { 0, 0, 0, 1 }, { { -1, 0, 0 }, { 0,  1, 0 }, { 0, 0,  1 } } },
    { HasScaleShear, { 0, 0, 0 }, { 0, 0, 0, 1 }, { {  1, 0, 0 }, { 0, -1, 0 }, { 0, 0,  1 } } },
    { HasScaleShear, { 0, 0, 0 }, { 0, 0, 0, 1 }, { {  1, 0, 0 }, { 0,  1, 0 }, { 0, 0, -1 } } },
    { HasScaleShear, { 0, 0, 0 }, { 0, 0, 0, 1 }, { { -1, 0, 0 }, { 0, -1, 0 }, { 0, 0, -1 } } },
};

static void
MirrorTransform(transform& xf, mirror_axis AlongAxis, mirror_axis PostFlipAxis)
{
    transform flipped;
    Multiply(flipped, sAxisFlipper[AlongAxis], xf);

    real32 const PosStore[3] = {
        flipped.Position[0],
        flipped.Position[1],
        flipped.Position[2],
    };
    flipped.Position[0]     =
        flipped.Position[1] =
        flipped.Position[2] = 0.0f;

    Multiply(xf, flipped, sAxisFlipper[PostFlipAxis]);
    xf.Position[0] = PosStore[0];
    xf.Position[1] = PosStore[1];
    xf.Position[2] = PosStore[2];

    // Normalize the transform to pull the rotations out of the scale/shear
    {
        real32 Q[9];
        real32 S[9];
        PolarDecompose((real32 const*)xf.ScaleShear, 0.00001f, Q, S);

        real32 Quat[4];
        QuaternionEqualsMatrix3x3(Quat, Q);
        QuaternionMultiply4(xf.Orientation, xf.Orientation, Quat);
        IntrinsicMemcpy(xf.ScaleShear, S, sizeof(xf.ScaleShear));
        ResetTransformFlags(xf);
    }
}

bool GRANNY
MirrorLocalPose(mirror_specification* Specification,
                skeleton*             Skeleton,
                local_pose*           Pose)
{
    CheckPointerNotNull(Specification, return false);
    CheckPointerNotNull(Pose, return false);
    CheckCondition(Specification->RemapCount == Pose->BoneCount, return false);
    CheckCondition(Specification->RemapCount == Skeleton->BoneCount, return false);

    MirrorPoseTransforms(Specification,
                         sizeof(local_pose_transform), &Pose->Transforms[0].Transform,
                         sizeof(bone), &Skeleton->Bones[0].ParentIndex,
                         Pose->BoneCount);

    return true;
}

void GRANNY
MaskedMirrorPoseTransforms(mirror_specification* Specification,
                           int32x TransformStride, transform* Transforms,
                           int32x SourceParentStride, int32 const *SourceParents,
                           int32x TransformCount,
                           track_mask const* ModelMask)
{
    GRANNY_AUTO_ZONE_FN();

    CheckPointerNotNull(Specification, return);
    CheckPointerNotNull(Transforms, return);
    CheckPointerNotNull(SourceParents, return);
    CheckBoundedInt32(0, TransformCount, Specification->RemapCount, return);

    // Build the object space version of the matrices...
    BuildSkeletonRelativeTransforms(TransformStride, Transforms,
                                    SourceParentStride, SourceParents,
                                    TransformCount,
                                    TransformStride, Transforms);

    // If we have a track mask, we need to know which transforms to compute.  This is any
    // bones with a non-zero weight, plus all their parents.  This is slow to compute in
    // progress, so do it all at once.
    const uint32 HighBit = uint32(1) << 31;
#if !PROCESSOR_CELL_SPU
    if (ModelMask != 0)
    {
        // I'm going to abuse the transform flags variable here to make this thread-safe.
        // It's not possible to alter the specification, or count on the track_mask being
        // constant.  Scratch buffer in the specification won't work either, it may be in use
        // on another sampling thread.  The high-bit flag MUST be pulled out at the end of
        // this function to be 100% safe.
        {for (int32x Idx = TransformCount-1; Idx >= 0; --Idx)
        {
            if (GetTrackMaskBoneWeight(*ModelMask, Idx) == 0.0f)
                continue;

            int32x Walk = Idx;
            while (Walk != NoParentBone)
            {
                uint32& Flags = ((transform*)((uint8*)Transforms + Walk*TransformStride))->Flags;
                if (Flags & HighBit)
                    break;

                Flags |= HighBit;
                Walk = *((int32*)(((uint8*)SourceParents) + SourceParentStride*Walk));
            }
        }}
    }
    else
#endif // PROCESSOR_CELL_SPU
    {
        {for (int32x BoneIdx = TransformCount-1; BoneIdx >= 0; --BoneIdx)
        {
            transform& BoneXF   = *((transform*)(((uint8*)Transforms) + TransformStride*BoneIdx));
            BoneXF.Flags |= HighBit;
        }}
    }

    // Loop through and do the mirroring
    {for (int32x BoneIdx = 0; BoneIdx < TransformCount; ++BoneIdx)
    {
        transform& BoneXF = *((transform*)(((uint8*)Transforms) + TransformStride*BoneIdx));

        if (Specification->Remaps[BoneIdx] == BoneIdx)
        {
            if ((BoneXF.Flags & HighBit) == 0)
                continue;

            // Bone mirrors in place
            MirrorTransform(BoneXF, Specification->MirrorAxis,
                            mirror_axis(Specification->PostFlips[BoneIdx]));

            // Normalize transform turns off this flag
            BoneXF.Flags |= HighBit;
        }
        else
        {
            int32x const OtherIdx = Specification->Remaps[BoneIdx];

            // Bone mirrors to another.  That bone /must/ mirror to this one.  We only do
            // the mirror and swap if BoneIdx refers to the lower indexed version
            Assert(Specification->Remaps[OtherIdx] == BoneIdx);
            if (BoneIdx < OtherIdx)
            {
                transform& OtherXF = *((transform*)(((uint8*)Transforms) + TransformStride*OtherIdx));
                bool const BoneMatters  = (BoneXF.Flags & HighBit) != 0;
                bool const OtherMatters = (OtherXF.Flags & HighBit) != 0;

                if (!BoneMatters && !OtherMatters)
                    continue;

                // Don't do the *mirrored* bone if we only care about the one side.
                if (OtherMatters)
                {
                    MirrorTransform(BoneXF, Specification->MirrorAxis,
                                    mirror_axis(Specification->PostFlips[BoneIdx]));
                }
                if (BoneMatters)
                {
                    MirrorTransform(OtherXF, Specification->MirrorAxis,
                                    mirror_axis(Specification->PostFlips[OtherIdx]));
                }

                // Swap
                transform Temp = OtherXF;
                OtherXF = BoneXF;
                BoneXF  = Temp;

                if (BoneMatters)
                    BoneXF.Flags |= HighBit;
                if (OtherMatters)
                    OtherXF.Flags |= HighBit;
            }
        }
    }}

    // Now put the bones back into parent relative space for sampling...
    {for (int32x BoneIdx = TransformCount-1; BoneIdx >= 0; --BoneIdx)
    {
        int32 const ParentIdx = *((int32*)(((uint8*)SourceParents) + SourceParentStride*BoneIdx));
        transform& BoneXF   = *((transform*)(((uint8*)Transforms) + TransformStride*BoneIdx));
        if (ParentIdx != NoParentBone)
        {
            if ((BoneXF.Flags & HighBit) == 0)
                continue;

            // Remove the flag
            BoneXF.Flags &= ~HighBit;

            transform& ParentXF = *((transform*)(((uint8*)Transforms) + TransformStride*ParentIdx));

            // This happens when we are the mirror of a bone that we actually care about.  Think
            // about the left elbow when only the right arm is masked in.
            if ((ParentXF.Flags & HighBit) == 0)
                continue;

            transform InvParent;
            BuildInverse(InvParent, ParentXF);
            PreMultiplyBy(BoneXF, InvParent);
        }
        else
        {
            // Ensure this is cleared, regardless.
            BoneXF.Flags &= ~HighBit;
        }
    }}

#if DEBUG
    {for (int32x BoneIdx = TransformCount-1; BoneIdx >= 0; --BoneIdx)
    {
        transform& BoneXF   = *((transform*)(((uint8*)Transforms) + TransformStride*BoneIdx));
        Assert((BoneXF.Flags & HighBit) == 0);
    }}
#endif
}

void GRANNY
MirrorPoseTransforms(mirror_specification* Specification,
                     int32x TransformStride, transform* Transforms,
                     int32x SourceParentStride, int32 const *SourceParents,
                     int32x TransformCount)
{
    MaskedMirrorPoseTransforms(Specification,
                               TransformStride, Transforms,
                               SourceParentStride, SourceParents,
                               TransformCount, 0);
}
