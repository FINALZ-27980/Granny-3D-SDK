#if !defined(GRANNY_LOOKAT_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_lookat.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(IKGroup);

struct local_pose;
struct skeleton;
struct world_pose;

EXPAPI GS_PARAM bool IKAimAt(int32x          HeadBoneIndex,
                             int32x          LinkCount,
                             int32x          InactiveLinkCount,
                             real32 const*   OSOffset3,
                             real32 const*   OSAimVec3,
                             real32 const*   OSLevelVec3,
                             real32 const*   WSGroundNormal,
                             real32 const*   TargetPosition,
                             skeleton const& Skeleton,
                             local_pose&     LocalPose,
                             real32*         Offset4x4,
                             world_pose&     WorldPose);


EXPAPI GS_PARAM bool IKOrientTowards(int32x          BoneIndex,
                                     real32 const*   OrientAxis,
                                     real32 const*   TargetPosition,
                                     skeleton const& Skeleton,
                                     local_pose&     LocalPose,
                                     real32*         Offset4x4,
                                     world_pose&     WorldPose);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_LOOKAT_H
#endif /* GRANNY_LOOKAT_H */
