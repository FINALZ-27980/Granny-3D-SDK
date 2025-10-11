// ========================================================================
// $File: //jeffr/granny_29/rt/granny_controlled_pose.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_controlled_pose.h"

#include "granny_bone_operations.h"
#include "granny_control.h"
#include "granny_local_pose.h"
#include "granny_log.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
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

USING_GRANNY_NAMESPACE;
#define SubsystemCode AnimationLogMessage

static bool
PoseInitializeBindingState(model_control_binding &Binding,
                           void *InitData)
{
    VAR_FROM_DERIVED(controlled_pose, State, Binding);
    Assert(State);

    controlled_pose *Source = (controlled_pose *)InitData;
    Assert(Source);

    State->Pose = Source->Pose;
    State->ModelMask = Source->ModelMask;

    Binding.BindingType = ControlledPose;

    return(true);
}

static void
PoseAccumulateBindingState(model_control_binding& Binding,
                           int32x const           FirstBone,
                           int32x                 BoneCount,
                           local_pose&            Result,
                           real32                 AllowedError,
                           int32x const*          SparseBoneArray)
{
    VAR_FROM_DERIVED(controlled_pose, State, Binding);
    Assert(State);

    real32 const ControlWeight = GetControlEffectiveWeight(*Binding.Control);
    if(ControlWeight > TrackWeightEpsilon)
    {
        int32x PoseBoneCount = GetLocalPoseBoneCount(*State->Pose);
        int32x OnePastLastBone = FirstBone + BoneCount;
        if(FirstBone < PoseBoneCount)
        {
            if(OnePastLastBone > PoseBoneCount)
            {
                OnePastLastBone = PoseBoneCount;
            }

            {for(int32x LocalPoseBoneIndex = FirstBone;
                 LocalPoseBoneIndex < OnePastLastBone;
                 ++LocalPoseBoneIndex)
            {
                real32 Weight = ControlWeight;
                int32x SkeletonBoneIndex = LocalPoseBoneIndex;
                if ( SparseBoneArray != NULL )
                {
                    SkeletonBoneIndex = SparseBoneArray[LocalPoseBoneIndex];
                }

                if(State->ModelMask)
                {
                    Weight *= GetTrackMaskBoneWeight(*State->ModelMask, SkeletonBoneIndex);
                }

                if(Weight > TrackWeightEpsilon)
                {
                    transform &Transform =
                        *GetLocalPoseTransform(*State->Pose, SkeletonBoneIndex);
                    AccumulateLocalTransform(
                        Result, LocalPoseBoneIndex, SkeletonBoneIndex, Weight,
                        *GetSourceSkeleton(*Binding.ModelInstance),
                        BlendQuaternionNeighborhooded, Transform);
                }
            }
        }}
    }
}

static void
PoseBuildDirect(model_control_binding& Binding,
                int32x const           BoneCount,
                real32 const*          Offset4x4,
                world_pose&            Result,
                real32                 AllowedError)
{
    CheckPointerNotNull(Offset4x4, return);
    CheckCondition(IS_ALIGNED_16(Offset4x4), return);
    CheckCondition(BoneCount >= 0, return);

    VAR_FROM_DERIVED(controlled_pose, State, Binding);
    Assert(State);

    int32x PoseBoneCount = GetLocalPoseBoneCount(*State->Pose);
    if (PoseBoneCount > BoneCount)
    {
        PoseBoneCount = BoneCount;
    }

    matrix_4x4* WorldBuffer = Result.WorldTransformBuffer;
    CheckCondition(IS_ALIGNED_16(WorldBuffer), return);

    skeleton* Skeleton = GetSourceSkeleton(*Binding.ModelInstance);
    Assert(Skeleton);

    bone* Bone = Skeleton->Bones;
    matrix_4x4* World = WorldBuffer;

    {for(int32x BoneIndex = 0; BoneIndex < PoseBoneCount; ++BoneIndex)
    {
        real32 const* BoneParentTransform = 0;
        {
            if (Bone->ParentIndex == NoParentBone)
                BoneParentTransform = Offset4x4;
            else
                BoneParentTransform = (real32 const*)WorldBuffer[Bone->ParentIndex];
        }
        Assert(BoneParentTransform != 0);
        Assert(IS_ALIGNED_16(BoneParentTransform));

        BWP_Dispatch(GetLocalPoseTransform(*State->Pose, BoneIndex),
                     BoneParentTransform, (real32*)World);

        ++Bone;
        ++World;
    }}

    // Build the composite matrices if we have them
    if (Result.CompositeTransformBuffer != 0)
    {
        BuildWorldPoseComposites(*Skeleton, 0, PoseBoneCount, Result);
    }
}


static controlled_animation*
PoseGetControlledAnimation ( model_control_binding &Binding )
{
    // It's not an animation!
    return NULL;
}

spu_controlled_animation* PoseGetSPUControlledAnimation ( model_control_binding &Binding )
{
    // It's not an SPU animation!
    return NULL;
}

static controlled_pose*
PoseGetControlledPose ( model_control_binding &Binding )
{
    VAR_FROM_DERIVED(controlled_pose, State, Binding);
    Assert(State);

    return State;
}


static model_control_callbacks ControlledPoseCallbacks =
{
    PoseGetControlledAnimation,
    PoseGetControlledPose,
    PoseGetSPUControlledAnimation,
    PoseInitializeBindingState,
    PoseAccumulateBindingState,
    PoseBuildDirect,
};

control* GRANNY
PlayControlledPose(real32 CurrentSeconds,
                   real32 Duration,
                   local_pose const &Pose,
                   model_instance &Model,
                   track_mask *ModelMask)
{
    // Create the control
    control *Control = CreateControl(CurrentSeconds, Duration);
    if(Control)
    {
        controlled_pose ControlledPose;
        ControlledPose.Pose = &Pose;
        ControlledPose.ModelMask = ModelMask;

        model_control_binding *Binding =
            CreateModelControlBinding(
                ControlledPoseCallbacks, *Control,
                Model, ControlIsActive(*Control),
                &ControlledPose);
        if(Binding)
        {
            // All is good
        }
        else
        {
            FreeControl(Control);
            Control = 0;

            Log0(ErrorLogMessage, ControlLogMessage,
                 "Unable to bind track group");
        }
    }
    else
    {
        Log0(ErrorLogMessage, ControlLogMessage,
             "Unable to create control");
    }

    return(Control);
}


local_pose *GRANNY
GetLocalPoseFromControlBinding(model_control_binding &Binding)
{
    controlled_pose *ControlledPose = Binding.Callbacks->GetControlledPose ( Binding );
    if ( ControlledPose != NULL )
    {
        return (local_pose *)ControlledPose->Pose;
    }
    else
    {
        return NULL;
    }
}



