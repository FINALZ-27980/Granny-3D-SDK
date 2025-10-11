// ========================================================================
// $File: //jeffr/granny_29/rt/granny_retargeter.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_retargeter.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_model.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode ModelLogMessage
USING_GRANNY_NAMESPACE;

inline intaddrx
RIDDifference(retarget_identifier &A,
              retarget_identifier &B)
{
    intaddrx Diff;

    Diff = PtrDiffSignOnly(A.FromBasis, B.FromBasis);
    if(Diff) return Diff;

    return PtrDiffSignOnly(A.ToBasis, B.ToBasis);
}

#define CONTAINER_NAME retarget_cache
#define CONTAINER_ITEM_TYPE retargeter
#define CONTAINER_COMPARE_RESULT_TYPE intaddrx
#define CONTAINER_COMPARE_ITEMS(Item1, Item2) RIDDifference((Item1)->ID, (Item2)->ID)
#define CONTAINER_FIND_FIELDS retarget_identifier ID
#define CONTAINER_COMPARE_FIND_FIELDS(Item) RIDDifference(ID, (Item)->ID)
#define CONTAINER_SORTED 1
#define CONTAINER_KEEP_LINKED_LIST 0
#define CONTAINER_SUPPORT_DUPES 0
#define CONTAINER_DO_ALLOCATION 0
#include "granny_contain.inl"

static retarget_cache RetargetCache;


retargeter* GRANNY
AcquireRetargeter(model* From, model* To)
{
    CheckPointerNotNull(From, return 0);
    CheckPointerNotNull(To, return 0);
    CheckPointerNotNull(From->Skeleton, return 0);
    CheckPointerNotNull(To->Skeleton, return 0);
    CheckCondition(To->Skeleton->BoneCount != 0, return 0);
    CheckCondition(From->Skeleton->BoneCount != 0, return 0);

    retarget_identifier NewID = { From, To };
    retargeter* Retargeter = Find(&RetargetCache, NewID);
    if (Retargeter)
    {
        Assert(Retargeter->ReferenceCount > 0);
        ++Retargeter->ReferenceCount;
        return Retargeter;
    }

    int32x const NumBones = To->Skeleton->BoneCount;

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);
    SetAggrAlignment(Allocator, 16);  // important for the spu...

    Assert(Retargeter == 0);
    AggrAllocPtr(Allocator, Retargeter);
    AggrAllocOffsetCountlessArrayPtr(Allocator, Retargeter, NumBones, SourceIndices);
    AggrAllocOffsetCountlessArrayPtr(Allocator, Retargeter, NumBones, RetargetingTransforms);
    if (EndAggrAlloc(Allocator, AllocationInstance))
    {
        Retargeter->ID.FromBasis = From;
        Retargeter->ID.ToBasis = To;
        Retargeter->NumBones = NumBones;
        Retargeter->ReferenceCount = 1;

        {for(int32x BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
        {
            bone const& DestBone = To->Skeleton->Bones[BoneIndex];
            char const* BoneName = DestBone.Name;

            int32x FromIndex;
            if (FindBoneByName(From->Skeleton, BoneName, FromIndex))
            {
                Retargeter->SourceIndices[BoneIndex] = FromIndex;
                bone const& FromBone = From->Skeleton->Bones[FromIndex];

                BuildInverse(Retargeter->RetargetingTransforms[BoneIndex],
                             FromBone.LocalTransform);
                PreMultiplyBy(Retargeter->RetargetingTransforms[BoneIndex],
                              DestBone.LocalTransform);
            }
            else
            {
                // no matching bone, use the skeleton base transform
                Retargeter->SourceIndices[BoneIndex] = -1;
                Retargeter->RetargetingTransforms[BoneIndex] = DestBone.LocalTransform;
            }
        }}

        Assert(IS_ALIGNED_16(Retargeter));
        Assert(IS_ALIGNED_16(Retargeter->RetargetingTransforms));

        Add(&RetargetCache, Retargeter);
    }

    return Retargeter;
}


void GRANNY
ReleaseRetargeter(retargeter* Retargeter)
{
    if (Retargeter)
    {
        Assert(Retargeter->ReferenceCount > 0);
        --Retargeter->ReferenceCount;

        if(Retargeter->ReferenceCount == 0)
        {
            Remove(&RetargetCache, Retargeter);
            Deallocate(Retargeter);
        }
    }
}


// Small little utility based on Multiply() since we /know/ that A only has an orientation
// component.
static void
OrientationPreMultiply(transform &Result, real32 const* Orientation, transform const &B)
{
    real32 OrientationA[3][3];
    MatrixEqualsQuaternion3x3((real32 *)OrientationA, Orientation);

    // This does the expanded vector/matrix product of:
    // A.Position + A.Orientation*A.ScaleShear*B.Position
    VectorTransform3(Result.Position, (real32 const *)OrientationA, B.Position);
    QuaternionMultiply4(Result.Orientation, Orientation, B.Orientation);
    Result.Flags = HasOrientation | B.Flags;
}


// Note Source may be the same as Dest, be careful.
//  Commentary: Umm.  Yeah, this is based on the old rebaser, but the math is obviously
//      twitchy, since we need to ensure that the resulting transform is separated into
//      it's s/r/p components.  Tread carefully, etc, etc.
void GRANNY
RebasingTransform(transform& Dest,
                  transform const& Source,
                  transform const& Rebaser)
{
    // Dest.Position = Source.Position * Rebaser.Orientation + Rebaser.Position
    real32 Position[3] = { Source.Position[0], Source.Position[1], Source.Position[2] };
    if (Rebaser.Flags & HasOrientation)
    {
        NormalQuaternionTransform3(Position, Rebaser.Orientation);
    }
    VectorAdd3(Position, Rebaser.Position);

    real32 Orientation[4];
    QuaternionMultiply4(Orientation, Source.Orientation, Rebaser.Orientation);

    if ((Source.Flags | Rebaser.Flags) & HasScaleShear)
    {
        transform Scale;
        MakeIdentity(Scale);
        Scale.Flags = HasScaleShear;
        IntrinsicMemcpy(Scale.ScaleShear, Source.ScaleShear, sizeof(Scale.ScaleShear));

        real32 InvOrientation[4];
        Conjugate4(InvOrientation, Rebaser.Orientation);

        OrientationPreMultiply(Scale, InvOrientation, Scale);
        PreMultiplyBy(Scale, Rebaser);

        SetTransform(Dest, Position, Orientation, (real32*)Scale.ScaleShear);
    }
    else
    {
        SetTransform(Dest, Position, Orientation, (real32*)GlobalIdentityTransform.ScaleShear);
        Dest.Flags &= ~HasScaleShear;
    }
}


bool GRANNY
RetargetPose(local_pose const& FromPose,
             local_pose& DestPose,
             int32x DestBoneStart,
             int32x DestBoneCount,
             retargeter& Retargeter)
{
    CheckCondition(DestBoneStart >= 0, return false);
    CheckCondition(DestBoneCount >= 0, return false);
    CheckCondition(DestBoneStart + DestBoneCount <= DestPose.BoneCount, return false);

    {for(int32x BoneIndex = DestBoneStart; BoneIndex < (DestBoneStart + DestBoneCount); ++BoneIndex)
    {
        transform& DestPoseTransform = DestPose.Transforms[BoneIndex].Transform;
        transform const& RetargetingTransform = Retargeter.RetargetingTransforms[BoneIndex];

        int32 SourceIndex = Retargeter.SourceIndices[BoneIndex];
        if (SourceIndex != -1)
        {
            transform const& PoseTransform = FromPose.Transforms[SourceIndex].Transform;
            RebasingTransform(DestPoseTransform, PoseTransform, RetargetingTransform);
        }
        else
        {
            // In this case, we've stored the rest pose of the destination skeleton in the
            // rebasing slot.
            DestPoseTransform = RetargetingTransform;
        }
    }}

    return true;
}

int32* GRANNY
GetRetargeterSourceIndices(retargeter& Retargeter)
{
    return Retargeter.SourceIndices;
}

model* GRANNY
GetRetargeterSourceModel(retargeter& Retargeter)
{
    return Retargeter.ID.FromBasis;
}

model* GRANNY
GetRetargeterTargetModel(retargeter& Retargeter)
{
    return Retargeter.ID.ToBasis;
}

