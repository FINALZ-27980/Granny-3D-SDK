// ========================================================================
// $File: //jeffr/granny_29/rt/granny_pose_cache.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_pose_cache.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_local_pose.h"
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


#define SubsystemCode BlendDagLogMessage
USING_GRANNY_NAMESPACE;

// Stores a pose of NumBones size.  DMMNOTE: Should we reach into the local pose on return
// and alter the size reported by the Count member?
struct pose_cache_entry
{
    pose_cache_entry* Next;
    pose_cache_entry* Prev;

    // We have to pad out to 16 bytes anyways, so we might as well store this here so we
    // don't have to look in the pose for it.  Stored as an intaddrx so this will 16-byte
    // align on both 32 and 64-bit builds.
    uintaddrx   NumBones;

    // Prevents 32/64 dependent math below...
    local_pose* Pose;
};
CompileAssert(IS_ALIGNED_N(sizeof(pose_cache_entry), LocalPoseAlignment));

struct GRANNY pose_cache
{
    uint32x NumBones;
    uint32x PoseSize;

    pose_cache_entry* FreeList;
    pose_cache_entry* AllocatedList;
};

void FreeLocalPoseChain(pose_cache_entry* Start)
{
    if (!Start)
        return;

    FreeLocalPoseChain(Start->Next);
    Deallocate(Start);
}

local_pose* GRANNY
GetNewLocalPose(pose_cache& PoseCache, uint32x NumBones)
{
    Assert( NumBones > 0);

    if (NumBones > PoseCache.NumBones)
    {
        // Free anything on the free list
        FreeLocalPoseChain(PoseCache.FreeList);
        PoseCache.FreeList = 0;

        // Now, reset the num bones and PoseSize parameters
        PoseCache.NumBones = NumBones;
        PoseCache.PoseSize = GetResultingLocalPoseSize(NumBones);
    }

    pose_cache_entry* Entry = 0;
    if (PoseCache.FreeList)
    {
        Assert(PoseCache.FreeList->NumBones >= NumBones);
        Assert(PoseCache.FreeList->NumBones == (uint32x)GetLocalPoseBoneCount(*PoseCache.FreeList->Pose));

        Entry = PoseCache.FreeList;
        PoseCache.FreeList = PoseCache.FreeList->Next;
    }
    else
    {
        // Allocate a new local pose, and put it on the allocated list
        aggr_allocator Allocator;
        InitializeAggrAlloc(Allocator);
        SetAggrAlignment(Allocator, LocalPoseAlignment);

        AggrAllocPtr(Allocator, Entry);
        AggrAllocOffsetSizePtr(Allocator, Entry, PoseCache.PoseSize, Pose);
        if (EndAggrAlloc(Allocator, AllocationLongTerm))
        {
            Entry->NumBones = PoseCache.NumBones;
            NewLocalPoseInPlace(PoseCache.NumBones, Entry->Pose);

            // Abuse the next pose entry to save some pointer math below...
            Entry->Pose->NextPose = (local_pose*)Entry;
        }
        else
        {
            Log0(ErrorLogMessage, BlendDagLogMessage, "Failed to allocate a new pose cache entry");
            return 0;
        }
    }
    Assert(Entry != 0);
    Assert((void*)Entry->Pose->NextPose == (void*)Entry);

    // Place the new entry on the allocated list
    {
        Entry->Prev = 0;
        Entry->Next = PoseCache.AllocatedList;
        if (Entry->Next)
        {
            Entry->Next->Prev = Entry;
        }
        PoseCache.AllocatedList = Entry;
    }

    // Ok, we're going to return the pose with /exactly/ the requested bone count.  If the
    // entry hasn't yet been returned, it should exactly match the entry numbones member.
    {
        Assert(Entry->Pose->BoneCount >= (int32x)NumBones);
        Assert(Entry->Pose->BoneCount == (int32x)Entry->NumBones);
        Entry->Pose->BoneCount = NumBones;
    }

    return Entry->Pose;
}

local_pose* GRANNY
GetNewRestLocalPose(pose_cache& PoseCache, model& ForModel)
{
    skeleton* Skeleton = ForModel.Skeleton;
    CheckPointerNotNull(Skeleton, return 0);

    local_pose* NewPose = GetNewLocalPose(PoseCache, Skeleton->BoneCount);
    if (NewPose != 0)
    {
        BuildRestLocalPose(*Skeleton, 0, Skeleton->BoneCount, *NewPose);
    }

    return NewPose;
}

pose_cache *GRANNY
NewPoseCache()
{
    pose_cache *Cache = Allocate(pose_cache, AllocationLongTerm);
    CheckPointerNotNull(Cache, return 0);

    ZeroStructure(*Cache);
    return Cache;
}

void GRANNY
ClearPoseCache(pose_cache &Cache)
{
    // Move all poses from the allocated list to the free list.  Note that we have to
    // free any of them that have less than the required number of bones...
    while (Cache.AllocatedList)
    {
        // Do it this way to ensure that we get the proper asserts called, and they exist
        // only in one place
        ReleaseCachePose(Cache, Cache.AllocatedList->Pose);
    }

    // All poses gone at this point...
    Assert(Cache.AllocatedList == 0);
}

void GRANNY
FreePoseCache(pose_cache *Cache)
{
    if (Cache)
    {
        FreeLocalPoseChain(Cache->FreeList);
        FreeLocalPoseChain(Cache->AllocatedList);
        Deallocate(Cache);
    }
}

bool GRANNY
PoseWillBeImmediatelyFreed(pose_cache& PoseCache, local_pose* Pose)
{
    CheckPointerNotNull(Pose, return true);

    // Check that the NextPose pointer points to a valid pose_cache_entry member /right/ behind the pose...
    pose_cache_entry* Entry = (pose_cache_entry*)Pose->NextPose;

    // This is probably a memory leak, and is /really/ bad.
    CheckCondition((char*)Entry == (((char*)Pose) - sizeof(pose_cache_entry)), return true);
    Assert(Entry->Pose == Pose);

    return Entry->NumBones < PoseCache.NumBones;
}

void GRANNY
ReleaseCachePose(pose_cache &Cache, local_pose* Pose)
{
    if (Pose == 0)
        return;

    // Check that the NextPose pointer points to a valid pose_cache_entry member /right/ behind the pose...
    pose_cache_entry* Entry = (pose_cache_entry*)Pose->NextPose;

    // This is probably a memory leak, and is /really/ bad.
    CheckCondition((char*)Entry ==
                   (((char*)Pose) - sizeof(pose_cache_entry)), return);
    Assert(Entry->Pose == Pose);

    // This should be non-null if we're on the allocated list.
    Assert(Entry->Prev != 0 || Entry == Cache.AllocatedList);

    // Remove it from the allocation chain
    {
        if (Entry->Next)
            Entry->Next->Prev = Entry->Prev;

        // First entry?  If so, we need to alter the head pointer
        if (Entry->Prev)
            Entry->Prev->Next = Entry->Next;
        else
            Cache.AllocatedList = Entry->Next;

        Entry->Next = Entry->Prev = 0;
    }

    if (Cache.NumBones > Entry->NumBones)
    {
        // The pose cannot satisfy a new call to allocate, so dump it
        Deallocate(Entry);
    }
    else
    {
        // Put it on the free list
        Entry->Next = Cache.FreeList;
        Cache.FreeList = Entry;

        // Also, since we may have altered the bonecount /in the pose/ on return, we need
        // to set that field back to the originally allocated value.
        Assert((int32x)Entry->NumBones >= Entry->Pose->BoneCount);
        Entry->Pose->BoneCount = (int32)Entry->NumBones;
    }
}
