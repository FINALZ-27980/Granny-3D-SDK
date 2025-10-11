// ========================================================================
// $File: //jeffr/granny_29/rt/granny_world_pose.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_world_pose.h"

#include "granny_aggr_alloc.h"
#include "granny_bone_operations.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_memory.h"
#include "granny_parameter_checking.h"
#include "granny_prefetch.h"
#include "granny_skeleton.h"
#include "granny_telemetry.h"

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
#define SubsystemCode WorldPoseLogMessage

USING_GRANNY_NAMESPACE;

world_pose *GRANNY
NewWorldPose(int32 BoneCount)
{
    world_pose *Pose =
        NewWorldPoseInPlace(
            BoneCount, IncludeComposites,
            AllocateSizeAligned(MatrixBufferAlignment,
                                GetResultingWorldPoseSize(BoneCount, IncludeComposites),
                                AllocationInstance));

    return(Pose);
}

world_pose *GRANNY
NewWorldPoseNoComposite(int32 BoneCount)
{
    world_pose *Pose =
        NewWorldPoseInPlace(
            BoneCount, ExcludeComposites,
            AllocateSizeAligned(MatrixBufferAlignment,
                                GetResultingWorldPoseSize(BoneCount, ExcludeComposites),
                                AllocationInstance));

    return(Pose);
}

void GRANNY
FreeWorldPose(world_pose *WorldPose)
{
    Deallocate(WorldPose);
}

real32* GRANNY
AllocateCompositeBuffer(int32x BoneCount)
{
    CheckCondition(BoneCount > 0, return 0);

    real32* CompositeBuffer = (real32*)AllocateArrayAligned(16, BoneCount, matrix_4x4, AllocationInstance);
    Assert(IS_ALIGNED_16(CompositeBuffer));

    return CompositeBuffer;
}

real32* GRANNY
AllocateCompositeBufferTransposed(int32x BoneCount)
{
    CheckCondition(BoneCount > 0, return 0);

    real32* CompositeBuffer = (real32*)AllocateArrayAligned(16, BoneCount, matrix_3x4, AllocationInstance);
    Assert(IS_ALIGNED_16(CompositeBuffer));

    return CompositeBuffer;
}

void GRANNY
FreeCompositeBuffer(void* CompositeBuffer)
{
    Deallocate(CompositeBuffer);
}


int32x GRANNY
GetWorldPoseBoneCount(world_pose const &WorldPose)
{
    return(WorldPose.BoneCount);
}

static void
AggrWorldPose(aggr_allocator &Allocator,
              int32x BoneCount,
              composite_flag CompositeFlag,
              world_pose *&Pose)
{
    AggrAllocPtr(Allocator, Pose);

    SetAggrAlignment(Allocator, MatrixBufferAlignment);
    AggrAllocOffsetArrayPtr(Allocator, Pose, BoneCount, BoneCount,
                            WorldTransformBuffer);
    if (CompositeFlag == IncludeComposites)
    {
        AggrAllocOffsetArrayPtr(Allocator, Pose, BoneCount, BoneCount,
                                CompositeTransformBuffer);
    }
}

int32x GRANNY
GetResultingWorldPoseSize(int32x BoneCount, composite_flag CompositeFlag)
{
    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    world_pose *Pose;
    AggrWorldPose(Allocator, BoneCount, CompositeFlag, Pose);

    int32x ResultingSize;
    CheckConvertToInt32(ResultingSize, EndAggrSize(Allocator), return 0);
    return ResultingSize;
}

world_pose *GRANNY
NewWorldPoseInPlace(int32x         BoneCount,
                    composite_flag CompositeFlag,
                    void*          Memory)
{
    world_pose *Pose = 0;

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    AggrWorldPose(Allocator, BoneCount, CompositeFlag, Pose);
    if(EndAggrPlacement(Allocator, Memory))
    {
        // Make sure the pointer is nulled if we don't have
        // composites...
        if (CompositeFlag == ExcludeComposites)
        {
            Pose->CompositeTransformBuffer = 0;
        }

        // All is good.
        if(((uintaddrx)Pose->WorldTransformBuffer % MatrixBufferAlignment) != 0)
        {
            Log0(WarningLogMessage, WorldPoseLogMessage,
                 "world_pose WorldTransformBuffer is unaligned");
        }

        if(((uintaddrx)Pose->CompositeTransformBuffer % MatrixBufferAlignment) != 0)
        {
            Log0(WarningLogMessage, WorldPoseLogMessage,
                 "world_pose CompositeTransformBuffer is unaligned");
        }
    }

    return Pose;
}

real32* GRANNY
GetWorldPose4x4(world_pose const &WorldPose, int32x BoneIndex)
{
    if (BoneIndex >= WorldPose.BoneCount)
        return 0;

    return (real32*)WorldPose.WorldTransformBuffer[BoneIndex];
}

real32* GRANNY
GetWorldPoseComposite4x4(world_pose const &WorldPose, int32x BoneIndex)
{
    // Shouldn't call this if this is a "without composites" world pose, but don't assert,
    // we use this to test in various places
    if (WorldPose.CompositeTransformBuffer == NULL)
        return 0;

    if (BoneIndex >= WorldPose.BoneCount)
        return 0;

    return (real32*)WorldPose.CompositeTransformBuffer[BoneIndex];
}

matrix_4x4* GRANNY
GetWorldPose4x4Array(world_pose const &WorldPose)
{
    return WorldPose.WorldTransformBuffer;
}

matrix_4x4* GRANNY
GetWorldPoseComposite4x4Array(world_pose const &WorldPose)
{
    // Shouldn't call this if this is a "without composites" world pose, but don't assert,
    // we use this to test in various places
    return WorldPose.CompositeTransformBuffer;
}


void GRANNY
BuildWorldPose(skeleton const&   Skeleton,
               int32x            FirstBone,
               int32x            BoneCount,
               local_pose const& LocalPose,
               real32 const*     Offset4x4,
               world_pose&       Result)
{
    BuildWorldPoseLOD(Skeleton,
                      FirstBone, BoneCount,
                      FirstBone, BoneCount,  // entire range is valid in this case
                      LocalPose,
                      Offset4x4,
                      Result);
}

void GRANNY
BuildWorldPoseNoComposite(skeleton const&   Skeleton,
                          int32x            FirstBone,
                          int32x            BoneCount,
                          local_pose const& LocalPose,
                          real32 const*     Offset4x4,
                          world_pose&       Result)
{
    BuildWorldPoseNoCompositeLOD(Skeleton,
                                 FirstBone, BoneCount,
                                 FirstBone, BoneCount,  // entire range is valid in this case
                                 LocalPose,
                                 Offset4x4,
                                 Result);
}

void GRANNY
BuildWorldPoseLOD(skeleton const&   Skeleton,
                  int32x            FirstBone,
                  int32x            BoneCount,
                  int32x            FirstValidLocalBone,
                  int32x            ValidLocalBoneCount,
                  local_pose const& LocalPose,
                  real32 const*     Offset4x4,
                  world_pose&       Result)
{
    GRANNY_AUTO_ZONE_FN();

    CheckInt32Index(FirstBone, Skeleton.BoneCount, return);
    CheckCondition(BoneCount >= 0, return);
    CheckCondition(FirstBone + BoneCount <= Skeleton.BoneCount, return);

    BuildWorldPoseNoCompositeLOD(Skeleton, FirstBone, BoneCount,
                                 FirstValidLocalBone,
                                 ValidLocalBoneCount,
                                 LocalPose, Offset4x4, Result);

    // Build the composite matrices if we have them
    if (Result.CompositeTransformBuffer != 0)
    {
        BuildWorldPoseComposites(Skeleton, FirstBone, BoneCount, Result);
    }
}

void GRANNY
BuildWorldPoseNoCompositeLOD(skeleton const&   Skeleton,
                             int32x            FirstBone,
                             int32x            BoneCount,
                             int32x            FirstValidLocalBone,
                             int32x            ValidLocalBoneCount,
                             local_pose const& LocalPose,
                             real32 const*     Offset4x4,
                             world_pose&       Result)
{
    GRANNY_AUTO_ZONE_FN();

    int32x const OnePastLastBone = FirstBone + BoneCount;
    int32x const OnePastLastValidLocalBone = FirstValidLocalBone + ValidLocalBoneCount;

    CheckCondition(FirstBone >= 0, return);
    CheckCondition(BoneCount >= 0, return);
    CheckBoundedInt32(0, OnePastLastValidLocalBone, GetLocalPoseBoneCount(LocalPose), return);
    CheckBoundedInt32(0, OnePastLastBone, Skeleton.BoneCount, return);
    CheckBoundedInt32(0, OnePastLastBone, Result.BoneCount, return);

    if (!Offset4x4)
    {
        Offset4x4 = (real32 const *)GlobalIdentity4x4;
    }

    ALIGN16_STACK(real32, OffsetBuffer, 16);
    if(!IS_ALIGNED_16(Offset4x4))
    {
        MatrixEquals4x4(OffsetBuffer, Offset4x4);
        Offset4x4 = OffsetBuffer;
    }

    matrix_4x4* WorldBuffer = GetWorldPose4x4Array(Result);
    Assert(IS_ALIGNED_16(WorldBuffer));

    local_pose_transform* Local = LocalPose.Transforms;
    bone* Bone = Skeleton.Bones;
    matrix_4x4* World = WorldBuffer;

    Bone  += FirstBone;
    World += FirstBone;
    Local += FirstBone;

    Assert(IS_ALIGNED_16(WorldBuffer));
    Assert(IS_ALIGNED_16(Offset4x4));
    {for(int32x CurrBone = FirstBone; CurrBone < OnePastLastBone; ++CurrBone)
    {
        real32 const* ParentWorld;
        {
            if (Bone->ParentIndex == NoParentBone)
                ParentWorld = Offset4x4;
            else
                ParentWorld = (real32 const*)WorldBuffer[Bone->ParentIndex];
        }

        transform* Transform;
        {
            if (CurrBone < OnePastLastValidLocalBone)
                Transform = &Local->Transform;
            else
                Transform = &Bone->LocalTransform;
        }

        BWP_Dispatch(Transform, ParentWorld, (real32*)*World);

        ++Bone;
        ++World;
        ++Local;
    }}
}


void GRANNY
BuildWorldPoseSparse(skeleton const&   Skeleton,
                     int32x            FirstBone,
                     int32x            BoneCount,
                     int32x const*     SparseBoneArray,
                     int32x const*     SparseBoneArrayReverse,
                     local_pose const& LocalPose,
                     real32 const*     Offset4x4,
                     world_pose&       Result)
{
    ALIGN16_STACK(real32, OffsetBuffer, 16);
    int32x const OnePastLastBone = FirstBone + BoneCount;

    CheckCondition(FirstBone >= 0, return);
    CheckCondition(BoneCount >= 0, return);
    CheckBoundedInt32(0, OnePastLastBone, Skeleton.BoneCount, return);
    CheckBoundedInt32(0, OnePastLastBone, Result.BoneCount, return);

    if (!Offset4x4)
    {
        Offset4x4 = (real32 const *)GlobalIdentity4x4;
    }
    else if (!IS_ALIGNED_16(Offset4x4))
    {
        MatrixEquals4x4(OffsetBuffer, Offset4x4);
        Offset4x4 = OffsetBuffer;
    }
    Assert(IS_ALIGNED_16(Offset4x4));

    matrix_4x4 *WorldBuffer = GetWorldPose4x4Array(Result);
    local_pose_transform *FirstLocal = LocalPose.Transforms;
    bone* Bone = Skeleton.Bones;

    matrix_4x4* World = WorldBuffer;
    Assert(IS_ALIGNED_16(World));

    Bone += FirstBone;
    World += FirstBone;
    int32x const* SparseBoneArrayReverseCurrent = SparseBoneArrayReverse + FirstBone;

    {for(int32x CurrBone = FirstBone; CurrBone < OnePastLastBone; ++CurrBone)
    {
        real32 const* ParentWorld;
        {
            if (Bone->ParentIndex == NoParentBone)
                ParentWorld = Offset4x4;
            else
                ParentWorld = (real32 const *)WorldBuffer[Bone->ParentIndex];
        }

        int32x LocalPoseBoneNum = SparseBoneArrayReverseCurrent[0];

        // If there is no data for this bone in the local pose, use the skeleton's default pose.
        transform *BoneTransform = &(Bone->LocalTransform);
        if ( LocalPoseBoneNum != NoSparseBone )
        {
            // But we have data. Excellent.
            Assert ( LocalPoseBoneNum >= 0 );
            Assert ( LocalPoseBoneNum < LocalPose.BoneCount );
            BoneTransform = &(FirstLocal[LocalPoseBoneNum].Transform);
        }

        BWP_Dispatch(BoneTransform, ParentWorld, (real32*)*World);

        ++Bone;
        ++World;
        ++SparseBoneArrayReverseCurrent;
    }}

    // Build the composite matrices if we have them
    if (Result.CompositeTransformBuffer != 0)
    {
        BuildWorldPoseComposites(Skeleton, FirstBone, BoneCount, Result);
    }
}


void GRANNY
BuildRestWorldPose(skeleton const& Skeleton,
                   int32x          FirstBone,
                   int32x          BoneCount,
                   real32 const*   Offset4x4,
                   world_pose&     Result)
{
    int32x OnePastLastBone = FirstBone + BoneCount;
    CheckCondition(FirstBone >= 0, return);
    CheckCondition(BoneCount >= 0, return);
    CheckBoundedInt32(0, OnePastLastBone, Skeleton.BoneCount, return);
    CheckBoundedInt32(0, OnePastLastBone, Result.BoneCount, return);

    ALIGN16_STACK(real32, OffsetBuffer, 16);
    if(!IS_ALIGNED_16(Offset4x4))
    {
        MatrixEquals4x4(OffsetBuffer, Offset4x4);
        Offset4x4 = OffsetBuffer;
    }
    if(!Offset4x4)
    {
        Offset4x4 = (real32 const *)GlobalIdentity4x4;
    }

    matrix_4x4 *WorldBuffer = GetWorldPose4x4Array(Result);

    bone*       Bone      = Skeleton.Bones;
    matrix_4x4* World     = WorldBuffer;

    Bone += FirstBone;
    World += FirstBone;

    {for(int32x CurrBone = FirstBone; CurrBone < OnePastLastBone; ++CurrBone)
    {
        real32 const *ParentWorld;
        {
            if (Bone->ParentIndex == NoParentBone)
                ParentWorld = Offset4x4;
            else
                ParentWorld = (real32*)WorldBuffer[Bone->ParentIndex];
        }

        BWP_Dispatch(&Bone->LocalTransform, ParentWorld, (real32*)*World);

        ++Bone;
        ++World;
    }}

    // Build the composite matrices if we have them
    if (Result.CompositeTransformBuffer != 0)
    {
        BuildWorldPoseComposites(Skeleton, FirstBone, BoneCount, Result);
    }
}


void GRANNY
BuildWorldPoseComposites(skeleton const& Skeleton,
                         int32x          FirstBone,
                         int32x          BoneCount,
                         world_pose&     Result)
{
    GRANNY_AUTO_ZONE_FN();

    CheckPointerNotNull(Result.CompositeTransformBuffer, return);

    int32x OnePastLastBone = FirstBone + BoneCount;
    CheckCondition(FirstBone >= 0, return);
    CheckCondition(BoneCount >= 0, return);
    CheckBoundedInt32(0, OnePastLastBone, Result.BoneCount, return);

    matrix_4x4 *WorldBuffer = GetWorldPose4x4Array(Result);

    bone *Bone = Skeleton.Bones;
    matrix_4x4 *World = WorldBuffer;
    matrix_4x4 *Composite = GetWorldPoseComposite4x4Array(Result);  // never null, checked above
    Assert(Composite);

    Bone += FirstBone;
    World += FirstBone;
    Composite += FirstBone;

    // TODO: optimize
    {for(int32x CurrBone = FirstBone; CurrBone < OnePastLastBone; ++CurrBone)
    {
        BuildSingleCompositeFromWorldPose((real32*)Bone->InverseWorld4x4,
                                          (real32*)*World,
                                          (real32*)*Composite);
        ++Bone;
        ++World;
        ++Composite;
    }}
}


void GRANNY
BuildCompositeBuffer(skeleton const&   Skeleton,
                     int32x            FirstBone,
                     int32x            BoneCount,
                     world_pose const& Pose,
                     matrix_4x4*       CompositeBuffer)
{
    GRANNY_AUTO_ZONE_FN();

    CheckPointerNotNull(CompositeBuffer, return);
    CheckBoundedInt32(0, FirstBone, Skeleton.BoneCount, return);
    CheckBoundedInt32(0, FirstBone, Pose.BoneCount, return);
    CheckCondition(BoneCount >= 0, return);

    int32x OnePastLastBone = FirstBone + BoneCount;
    CheckBoundedInt32(0, OnePastLastBone, Skeleton.BoneCount, return);
    CheckBoundedInt32(0, OnePastLastBone, Pose.BoneCount, return);

    matrix_4x4 *WorldBuffer = GetWorldPose4x4Array(Pose);
    bone *Bone = Skeleton.Bones;
    matrix_4x4 *World = WorldBuffer;
    matrix_4x4 *Composite = CompositeBuffer;

    CheckCondition(IS_ALIGNED_16(WorldBuffer), return);
    CheckCondition(IS_ALIGNED_16(CompositeBuffer), return);

    Bone += FirstBone;
    World += FirstBone;
    Composite += FirstBone;

    {for(int32x CurrBone = FirstBone; CurrBone < OnePastLastBone; ++CurrBone)
    {
        BuildSingleCompositeFromWorldPose((real32*)Bone->InverseWorld4x4,
                                          (real32*)*World,
                                          (real32*)*Composite);
        ++Bone;
        ++World;
        ++Composite;
    }}
}

void GRANNY
BuildCompositeBufferTransposed(skeleton const&   Skeleton,
                               int32x            FirstBone,
                               int32x            BoneCount,
                               world_pose const& Pose,
                               matrix_3x4*       CompositeBuffer)
{
    GRANNY_AUTO_ZONE_FN();

    CheckPointerNotNull(CompositeBuffer, return);
    CheckCondition(FirstBone >= 0, return);
    CheckCondition(BoneCount >= 0, return);
    CheckBoundedInt32(0, FirstBone, Skeleton.BoneCount, return);
    CheckBoundedInt32(0, FirstBone, Pose.BoneCount, return);

    int32x OnePastLastBone = FirstBone + BoneCount;
    CheckBoundedInt32(0, OnePastLastBone, Skeleton.BoneCount, return);
    CheckBoundedInt32(0, OnePastLastBone, Pose.BoneCount, return);


    matrix_4x4* WorldBuffer = GetWorldPose4x4Array(Pose);

    bone* Bone = Skeleton.Bones;
    matrix_4x4* World = WorldBuffer;
    matrix_3x4* Composite = CompositeBuffer;

    CheckCondition(IS_ALIGNED_16(WorldBuffer), return);
    CheckCondition(IS_ALIGNED_16(CompositeBuffer), return);

    Bone += FirstBone;
    World += FirstBone;
    Composite += FirstBone;

    {for(int32x CurrBone = FirstBone; CurrBone < OnePastLastBone; ++CurrBone)
    {
        BuildSingleCompositeFromWorldPoseTranspose((real32*)Bone->InverseWorld4x4,
                                                   (real32*)*World,
                                                   (real32*)*Composite);
        ++Bone;
        ++World;
        ++Composite;
    }}
}

void GRANNY
BuildIndexedCompositeBuffer(skeleton const&   Skeleton,
                            world_pose const& Pose,
                            int32x const*     Indices,
                            int32x            IndexCount,
                            matrix_4x4*       CompositeBuffer)
{
    GRANNY_AUTO_ZONE_FN();

    CheckPointerNotNull(CompositeBuffer, return);
    CheckPointerNotNull(Indices, return);
    CheckBoundedInt32(0, IndexCount, Skeleton.BoneCount, return);
    CheckBoundedInt32(0, IndexCount, Pose.BoneCount, return);
    CheckCondition(IS_ALIGNED_16(CompositeBuffer), return);

    matrix_4x4 *WorldBuffer = GetWorldPose4x4Array(Pose);

    bone *Bones = Skeleton.Bones;
    matrix_4x4 *Composite = CompositeBuffer;

    {for(int32x CurrIdx = 0; CurrIdx < IndexCount; ++CurrIdx)
    {
        int32x Index = Indices[CurrIdx];
        Assert(Index >= 0 && Index < Skeleton.BoneCount);
        Assert(Index < Pose.BoneCount);

        bone* ThisBone = Bones + Index;
        matrix_4x4* ThisWorld = WorldBuffer + Index;

        BuildSingleCompositeFromWorldPose((real32*)ThisBone->InverseWorld4x4,
                                          (real32*)*ThisWorld,
                                          (real32*)*Composite);
        ++Composite;
    }}
}

void GRANNY
BuildIndexedCompositeBufferTransposed(skeleton const&   Skeleton,
                                      world_pose const& Pose,
                                      int32x const*     Indices,
                                      int32x            IndexCount,
                                      matrix_3x4*       CompositeBuffer)
{
    GRANNY_AUTO_ZONE_FN();

    CheckPointerNotNull(CompositeBuffer, return);
    CheckPointerNotNull(Indices, return);
    CheckBoundedInt32(0, IndexCount, Skeleton.BoneCount, return);
    CheckBoundedInt32(0, IndexCount, Pose.BoneCount, return);
    CheckCondition(IS_ALIGNED_16(CompositeBuffer), return);

    matrix_4x4 *WorldBuffer = GetWorldPose4x4Array(Pose);

    bone *Bones = Skeleton.Bones;
    matrix_3x4 *Composite = CompositeBuffer;

    {for(int32x CurrIdx = 0; CurrIdx < IndexCount; ++CurrIdx)
    {
        int32x Index = Indices[CurrIdx];
        Assert(Index >= 0 && Index < Skeleton.BoneCount);
        Assert(Index < Pose.BoneCount);

        bone* ThisBone = Bones + Index;
        matrix_4x4* ThisWorld = WorldBuffer + Index;

        BuildSingleCompositeFromWorldPoseTranspose((real32*)ThisBone->InverseWorld4x4,
                                                   (real32*)*ThisWorld,
                                                   (real32*)*Composite);
        ++Composite;
    }}
}


void GRANNY
UpdateWorldPoseChildren(skeleton const &Skeleton,
                        int32x ParentBone,
                        local_pose &LocalPose,
                        real32 const *Offset4x4,
                        world_pose &Result)
{
    GRANNY_AUTO_ZONE_FN();

    int32x BoneCount = Skeleton.BoneCount;
    CheckBoundedInt32(0, ParentBone, BoneCount, return);

    // Set the bone's traversal ID to this, then go down
    // the list, updating any bone whose parent matches the ID.
    LocalPose.TraversalID++;
    int32x TraversalID = LocalPose.TraversalID;

    ALIGN16_STACK(real32, OffsetBuffer, 16);
    if(!Offset4x4)
    {
        Offset4x4 = (real32 const *)GlobalIdentity4x4;
    }

    if(!IS_ALIGNED_16(Offset4x4))
    {
        MatrixEquals4x4(OffsetBuffer, Offset4x4);
        Offset4x4 = OffsetBuffer;
    }

    matrix_4x4 *WorldBuffer = GetWorldPose4x4Array(Result);
    local_pose_transform *FirstLocal = LocalPose.Transforms;
    local_pose_transform *Local = FirstLocal;

    bone*       Bone  = Skeleton.Bones;
    matrix_4x4* World = WorldBuffer;

    Bone      += ParentBone;
    World     += ParentBone;
    Local     += ParentBone;
    BoneCount -= ParentBone;

    // Slightly odd way to do this loop, because we want to always do
    // ParentBone, its parent does _not_ have the magic Traversal ID :-)
    {for(int32x RemainingBoneCount = BoneCount;
         RemainingBoneCount > 0;
         /* Manual advance --RemainingBoneCount */)
    {
        // Redo the current bone.
        real32 const *ParentWorld;
        {
            if (Bone->ParentIndex == NoParentBone)
                ParentWorld = Offset4x4;
            else
                ParentWorld = (real32 const *)WorldBuffer[Bone->ParentIndex];
        }

        transform &Transform = Local->Transform;
        BWP_Dispatch(&Transform, ParentWorld, (real32*)*World);

        Local->TraversalID = TraversalID;
        ++Bone;
        ++World;
        ++Local;
        --RemainingBoneCount;

        // Find the next bone that has a parent with the magic ID.
        while ( RemainingBoneCount > 0 )
        {
            if ( Bone->ParentIndex != NoParentBone )
            {
                // This bone's parent has the magic ID, so we need to re-do it.
                if ( FirstLocal[Bone->ParentIndex].TraversalID == TraversalID )
                    break;
            }

            ++Bone;
            ++World;
            ++Local;
            --RemainingBoneCount;
        }
    }}


    // Build the composite matrices if we have them
    //
    // Note: this is slightly inefficient, in that we're updating some composites for
    // bones that haven't been touched...
    if (Result.CompositeTransformBuffer != 0)
    {
        BuildWorldPoseComposites(Skeleton,
                                 ParentBone,                        // start at the parent
                                 Skeleton.BoneCount - ParentBone,   // do everything south
                                 Result);
    }
}

