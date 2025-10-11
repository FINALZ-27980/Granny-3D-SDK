#if !defined(GRANNY_POSE_CACHE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_pose_cache.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(DagGroup);

EXPTYPE struct pose_cache;
struct local_pose;
struct model;

// Local caches for worker threads
EXPAPI GS_SAFE pose_cache* NewPoseCache();

EXPAPI GS_PARAM void ClearPoseCache(pose_cache &Cache);
EXPAPI GS_PARAM void FreePoseCache(pose_cache *Cache);
EXPAPI GS_PARAM void ReleaseCachePose(pose_cache &Cache, local_pose* Pose);

EXPAPI GS_PARAM local_pose* GetNewLocalPose(pose_cache& PoseCache, uint32x NumBones);

local_pose* GetNewRestLocalPose(pose_cache& PoseCache, model& ForModel);
bool PoseWillBeImmediatelyFreed(pose_cache& PoseCache, local_pose* Pose);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_POSE_CACHE_H
#endif
