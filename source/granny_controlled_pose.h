#if !defined(GRANNY_CONTROLLED_POSE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_controlled_pose.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(ControlledAnimationGroup);

struct control;
struct local_pose;
struct model_control_binding;
struct model_instance;
struct track_mask;

struct controlled_pose
{
    // These data structures all have to fit inside model_control_binding::Derived[]
    local_pose const *Pose;
    track_mask const *ModelMask;
};


EXPAPI GS_MODIFY control *PlayControlledPose(real32 StartTime, real32 Duration,
                                             local_pose const &Pose,
                                             model_instance &Model,
                                             track_mask *ModelMask);

EXPAPI GS_READ local_pose *GetLocalPoseFromControlBinding(model_control_binding& Binding);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_CONTROLLED_POSE_H
#endif
