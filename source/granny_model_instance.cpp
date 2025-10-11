// ========================================================================
// $File: //jeffr/granny_29/rt/granny_model_instance.cpp $
// $DateTime: 2012/03/21 14:46:58 $
// $Change: 36831 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_model_instance.h"

#include "granny_control.h"
#include "granny_degree_of_freedom.h"
#include "granny_fixed_allocator.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_matrix_operations.h"
#include "granny_memory.h"
#include "granny_model.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
#include "granny_telemetry.h"
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
#define SubsystemCode AnimationLogMessage

USING_GRANNY_NAMESPACE;

struct model_instance_status
{
    int32x TotalInstanceCount;       // value
    int32x TotalInstancedBoneCount;  // value
    int32x MaxBoneCount;             // maximum
};
static model_instance_status g_ModelInstanceStatus = { 0 };


CompileAssert(IS_ALIGNED_16(SizeOf(model_instance)));
static fixed_allocator Allocator = {SizeOf(model_instance)};

static model_instance &
GetGlobalModelInstanceSentinel(void)
{
    static model_instance Sentinel;
    if(!Sentinel.Next)
    {
        Sentinel.Next = Sentinel.Prev = &Sentinel;
    }

    return(Sentinel);
}

static void
LinkInstance(model_instance* ModelInstance)
{
    ModelInstance->Next = GetGlobalModelInstanceSentinel().Next;
    ModelInstance->Prev = &GetGlobalModelInstanceSentinel();
    ModelInstance->Next->Prev = ModelInstance;
    ModelInstance->Prev->Next = ModelInstance;
}

model_instance *GRANNY
InstantiateModel(model const &Model)
{
    CheckPointerNotNull(Model.Skeleton, return 0);

    model_instance *ModelInstance = (model_instance *)AllocateFixed(Allocator);

    if(ModelInstance)
    {
        CheckCondition(IS_ALIGNED_16(ModelInstance),
                       FreeModelInstance(ModelInstance); return NULL);

        ModelInstance->Model = &Model;

        ModelInstance->CachedSkeleton       = Model.Skeleton;
        ModelInstance->CachedBones          = Model.Skeleton->Bones;
        ModelInstance->CachedBoneCount      = Model.Skeleton->BoneCount;
        ModelInstance->MirrorTransformCache = 0;

        ModelInstance->Reserved0 = 0;

        InitializeSentinel(ModelInstance->BindingSentinel);
        LinkInstance(ModelInstance);

        GRANNY_INC_INT_VALUE(g_ModelInstanceStatus.TotalInstanceCount);
        GRANNY_ADD_INT_VALUE(g_ModelInstanceStatus.TotalInstancedBoneCount, Model.Skeleton->BoneCount);
        GRANNY_NOTE_INT_MAXVALUE(g_ModelInstanceStatus.MaxBoneCount, Model.Skeleton->BoneCount);
    }

    return ModelInstance;
}

void GRANNY
CreateMirrorTransformCache(model_instance& Instance)
{
    CheckPointerNotNull(Instance.Model, return);

    // Already created?
    if (Instance.MirrorTransformCache != 0)
        return;

    Instance.MirrorTransformCache =
        AllocateArray(Instance.Model->Skeleton->BoneCount, transform, AllocationInstance);
    if (Instance.MirrorTransformCache == 0)
    {
        Log1(ErrorLogMessage, AnimationLogMessage,
             "CreateMirrorTransformCache: Unable to allocate a mirror cache of %d bones",
             Instance.Model->Skeleton->BoneCount);
    }
}


void GRANNY
FreeModelInstance(model_instance *Instance)
{
    if (Instance)
    {
        FreeModelRing(GetModelBindingSentinel(*Instance));

        Instance->Next->Prev = Instance->Prev;
        Instance->Prev->Next = Instance->Next;

        if (Instance->MirrorTransformCache)
        {
            Deallocate(Instance->MirrorTransformCache);
            Instance->MirrorTransformCache = 0;
        }

        GRANNY_DEC_INT_VALUE(g_ModelInstanceStatus.TotalInstanceCount);
        GRANNY_SUB_INT_VALUE(g_ModelInstanceStatus.TotalInstancedBoneCount, (int32x)Instance->CachedBoneCount);
    }

    DeallocateFixed(Allocator, Instance);
}

model &GRANNY
GetSourceModel(model_instance const &Model)
{
    Assert(Model.Model);
    return(*(model *)Model.Model);
}

skeleton *GRANNY
GetSourceSkeleton(model_instance const &Model)
{
    Assert(Model.CachedSkeleton == Model.Model->Skeleton);
    return (skeleton*)Model.CachedSkeleton;
}

model_control_binding &GRANNY
GetModelBindingSentinel(model_instance const &ModelInstance)
{
    return((model_control_binding &)ModelInstance.BindingSentinel);
}

void GRANNY
SetModelClock(model_instance const &ModelInstance, real32 NewClock)
{
    model_control_binding *Sentinel = &GetModelBindingSentinel(ModelInstance);
    {for(model_control_binding *Iterator = Sentinel->ModelNext;
         Iterator != Sentinel;
         Iterator = Iterator->ModelNext)
    {
        Assert(Iterator->Control);
        SetControlClockInline(*Iterator->Control, NewClock);
    }}
}

void GRANNY
FreeCompletedModelControls(model_instance const &ModelInstance)
{
    model_control_binding *Sentinel = &GetModelBindingSentinel(ModelInstance);
    {for(model_control_binding *Iterator = Sentinel->ModelNext;
         Iterator != Sentinel;)
    {
        control *Control = Iterator->Control;
        Iterator = Iterator->ModelNext;

        FreeControlIfComplete(Control);
    }}
}

void GRANNY
AccumulateModelAnimationsLODSparse(model_instance const &ModelInstance,
                                    int32x FirstBone, int32x BoneCount,
                                    local_pose &Result, real32 AllowedError,
                                    int32x const *SparseBoneArray)
{
    skeleton const *Skeleton = GetSourceSkeleton(ModelInstance);
    Assert(Skeleton);

    CheckInt32Index(FirstBone, Skeleton->BoneCount, return);
    CheckCondition(BoneCount >= 0, return);
    CheckCondition(FirstBone + BoneCount <= Skeleton->BoneCount, return);

    model_control_binding *Sentinel = &GetModelBindingSentinel(ModelInstance);
    {for(model_control_binding *Iterator = Sentinel->ModelNext;
         Iterator != Sentinel;
         Iterator = Iterator->ModelNext)
    {
        if (ControlHasEffect(*Iterator->Control))
        {
            SampleModelControlBindingLODSparse(*Iterator, FirstBone, BoneCount, Result, AllowedError, SparseBoneArray);
        }
        else
        {
            // Bindings are sorted by their state, so as soon as
            // we hit one that is inactive, we stop.
            break;
        }
    }}
}

void GRANNY
SampleModelAnimationsLODSparse(model_instance const &ModelInstance,
                               int32x FirstBone, int32x BoneCount,
                               local_pose &Result, real32 AllowedError,
                               int32x const *SparseBoneArray)
{
    GRANNY_AUTO_ZONE(SampleModelAnimations);

    skeleton const *Skeleton = GetSourceSkeleton(ModelInstance);
    Assert(Skeleton);

    CheckInt32Index(FirstBone, Skeleton->BoneCount, return);
    CheckCondition(BoneCount >= 0, return);
    CheckCondition(FirstBone + BoneCount <= Skeleton->BoneCount, return);

    BeginLocalPoseAccumulation(Result, FirstBone, BoneCount, SparseBoneArray);

    AccumulateModelAnimationsLODSparse(ModelInstance, FirstBone, BoneCount, Result, AllowedError, SparseBoneArray);


    EndLocalPoseAccumulationLOD(Result,
                                FirstBone, BoneCount,
                                FirstBone, BoneCount,
                                *Skeleton, AllowedError, SparseBoneArray);
}

bool GRANNY
SampleSingleModelAnimationLODSparse(model_instance const &ModelInstance,
                                    control const &Control,
                                    int32x FirstBone, int32x BoneCount,
                                    local_pose &Result, real32 AllowedError,
                                    int32x const *SparseBoneArray)
{
    GRANNY_AUTO_ZONE(SampleSingleModelAnimations);

    skeleton const *Skeleton = GetSourceSkeleton(ModelInstance);
    Assert(Skeleton);

    CheckInt32Index(FirstBone, Skeleton->BoneCount, return false);
    CheckCondition(FirstBone + BoneCount <= Skeleton->BoneCount, return false);

    bool FoundAnimation = false;
    BeginLocalPoseAccumulation(Result, FirstBone, BoneCount, SparseBoneArray);

    model_control_binding *Sentinel = &GetControlBindingSentinel(Control);
    {for(model_control_binding *Iterator = Sentinel->ControlNext;
         Iterator != Sentinel;
         Iterator = Iterator->ControlNext)
    {
        Assert(Iterator->Control == &Control);
        if(Iterator->ModelInstance == &ModelInstance)
        {
            if(ControlHasEffect(*Iterator->Control))
            {
                SampleModelControlBindingLODSparse(*Iterator, FirstBone, BoneCount, Result, AllowedError, SparseBoneArray);
            }

            FoundAnimation = true;
            break;
        }
    }}

    EndLocalPoseAccumulationLOD(Result,
                                FirstBone, BoneCount,
                                FirstBone, BoneCount, *Skeleton, AllowedError, SparseBoneArray);

    return FoundAnimation;
}


void GRANNY
SampleModelAnimationsAcceleratedLOD(model_instance const &ModelInstance,
                                    int32x BoneCount,
                                    real32 const *Offset4x4,
                                    local_pose &Scratch,
                                    world_pose &Result,
                                    real32 AllowedError)
{
    GRANNY_AUTO_ZONE(SampleModelAnimationsAccelerated);

    // Some people don't realise you MUST pass me a local_pose, even if I don't use it in the fast path.
    CheckPointerNotNull ( &Scratch, return );

    skeleton const *Skeleton = GetSourceSkeleton(ModelInstance);
    if (BoneCount > Skeleton->BoneCount)
    {
        BoneCount = Skeleton->BoneCount;
    }

    ALIGN16_STACK(real32, OffsetBuffer, 16);
    if (!Offset4x4)
    {
        Offset4x4 = (real32 const *)GlobalIdentity4x4;
    }
    else
    {
        if (!IS_ALIGNED_16(Offset4x4))
        {
            MatrixEquals4x4(OffsetBuffer, Offset4x4);
            Offset4x4 = OffsetBuffer;
        }
    }

    model_control_binding *Sentinel = &GetModelBindingSentinel(ModelInstance);
    if((Sentinel->ModelNext != Sentinel) &&
       (ControlHasEffect(*Sentinel->ModelNext->Control)))
    {
        // How many bones to sample for this LOD?  Only compute this if
        //  we're going to use it...
        int32x LODBoneCount = GetBoneCountForLOD(Skeleton, AllowedError);

        // Note the last test here take into account the ease out curve.  We need to check
        // that the effective weight isn't less than the local pose fill threshold, which
        // can happen on ease out curves.  Normally, you wouldn't get into this path when
        // an ease out was active, since it will drop to rest pose, but let's be
        // consistent.
        control* SingleControl = Sentinel->ModelNext->Control;
        if (((Sentinel->ModelNext->ModelNext != Sentinel) &&
             (ControlHasEffect(*Sentinel->ModelNext->ModelNext->Control))) ||
            ControlIsRetargeted(*SingleControl) ||
            ControlIsMirrored(*SingleControl) ||
            GetControlEffectiveWeight(*SingleControl) < GetLocalPoseFillThreshold(Scratch))
        {
            SampleModelAnimationsLODSparse(ModelInstance,
                                           0, LODBoneCount,
                                           Scratch,
                                           AllowedError, NULL);
            BuildWorldPoseLOD(*Skeleton,
                              0, BoneCount,
                              0, LODBoneCount,
                              Scratch, Offset4x4, Result);
        }
        else
        {
            BuildBindingDirect(*Sentinel->ModelNext, BoneCount,
                               Offset4x4, Result, AllowedError);
        }
    }
    else
    {
        BuildRestWorldPose(*Skeleton, 0, BoneCount, Offset4x4, Result);
    }
}


void GRANNY
GetUnnormalizedRootMotionVectors(model_instance const &ModelInstance,
                     real32 SecondsElapsed,
                     real32 &TotalWeight,
                     real32 *ResultTranslation,
                     real32 *ResultRotation,
                     bool Inverse)
{
    model_control_binding *Sentinel = &GetModelBindingSentinel(ModelInstance);
    {for(model_control_binding *Iterator = Sentinel->ModelNext;
         Iterator != Sentinel;
         Iterator = Iterator->ModelNext)
    {
        if(ControlHasEffect(*Iterator->Control))
        {
            AccumulateModelControlBindingLoopTransform(
                *Iterator, SecondsElapsed, TotalWeight,
                ResultTranslation, ResultRotation, Inverse);
        }
        else
        {
            // Bindings are sorted by their state, so as soon as
            // we hit one that is inactive, we stop.
            break;
        }
    }}
}


void GRANNY
GetRootMotionVectors(model_instance const &ModelInstance,
                     real32 SecondsElapsed,
                     real32 *ResultTranslation,
                     real32 *ResultRotation,
                     bool Inverse)
{
    real32 TotalWeight = 0.0f;

    VectorZero3(ResultTranslation);
    VectorZero3(ResultRotation);

    GetUnnormalizedRootMotionVectors ( ModelInstance, SecondsElapsed, TotalWeight, ResultTranslation, ResultRotation, Inverse );

    if(TotalWeight > TrackWeightEpsilon)
    {
        real32 const InverseTotalWeight = 1.0f / TotalWeight;
        VectorScale3(ResultTranslation, InverseTotalWeight);
        VectorScale3(ResultRotation, InverseTotalWeight);
    }
}

void GRANNY
ClipRootMotionVectors(real32 const *Translation3,
                      real32 const *Rotation3,
                      uint32 AllowableDOFs,
                      real32 *AllowedTranslation3,
                      real32 *AllowedRotation3,
                      real32 *DisallowedTranslation3,
                      real32 *DisallowedRotation3)
{
    VectorEquals3(AllowedTranslation3, Translation3);
    VectorEquals3(DisallowedTranslation3, Translation3);
    VectorEquals3(AllowedRotation3, Rotation3);
    VectorEquals3(DisallowedRotation3, Rotation3);

    ClipPositionDOFs(AllowedTranslation3, AllowableDOFs);
    ClipAngularVelocityDOFs(AllowedRotation3, AllowableDOFs);

    ClipPositionDOFs(DisallowedTranslation3, ~AllowableDOFs);
    ClipAngularVelocityDOFs(DisallowedRotation3, ~AllowableDOFs);
}

void GRANNY
ApplyRootMotionVectorsToMatrix(real32 const *ModelMatrix4x4,
                               real32 const *Translation,
                               real32 const *Rotation,
                               real32 *DestMatrix4x4)
{
    quad UpdateMatrix[4];
    UpdateMatrix[0][3] = UpdateMatrix[1][3] = UpdateMatrix[2][3] = 0.0f;
    UpdateMatrix[3][3] = 1.0f;

    UpdateMatrix[3][0] = Translation[0];
    UpdateMatrix[3][1] = Translation[1];
    UpdateMatrix[3][2] = Translation[2];

    real32 RotationLength = VectorLength3(Rotation);
    if(RotationLength > 0.0f)
    {
        triple ScaledRotation;
        VectorScale3(ScaledRotation, 1.0f / RotationLength, Rotation);

        triple RotationMatrix[3];
        AxisRotationColumns3x3(RotationMatrix, ScaledRotation,
                               RotationLength);
        UpdateMatrix[0][0] = RotationMatrix[0][0];
        UpdateMatrix[0][1] = RotationMatrix[1][0];
        UpdateMatrix[0][2] = RotationMatrix[2][0];
        UpdateMatrix[1][0] = RotationMatrix[0][1];
        UpdateMatrix[1][1] = RotationMatrix[1][1];
        UpdateMatrix[1][2] = RotationMatrix[2][1];
        UpdateMatrix[2][0] = RotationMatrix[0][2];
        UpdateMatrix[2][1] = RotationMatrix[1][2];
        UpdateMatrix[2][2] = RotationMatrix[2][2];
    }
    else
    {
        UpdateMatrix[0][0] = UpdateMatrix[1][1] = UpdateMatrix[2][2] = 1.0f;
        UpdateMatrix[0][1] = UpdateMatrix[0][2] =
            UpdateMatrix[1][0] = UpdateMatrix[1][2] =
            UpdateMatrix[2][0] = UpdateMatrix[2][1] = 0.0f;
    }

    real32 Temp[16];
    if(DestMatrix4x4 == ModelMatrix4x4)
    {
        DestMatrix4x4 = Temp;
    }

    ColumnMatrixMultiply4x3(DestMatrix4x4, (real32 *)UpdateMatrix, ModelMatrix4x4);

    if(DestMatrix4x4 == Temp)
    {
        MatrixEquals4x4((real32 *)ModelMatrix4x4, Temp);
    }
}

static void
AngularVelocityToQuaternion(real32 *Rotation4, real32 const *Rotation3)
{
    real32 Angle = VectorLength3(Rotation3);
    triple Normalized;
    VectorScale3(Normalized, 1.0f / Angle, Rotation3);
    ConstructQuaternion4(Rotation4, Normalized, Angle);
}

void GRANNY
ApplyRootMotionVectorsToLocalPose(local_pose &Pose,
                                  real32 const *Translation3,
                                  real32 const *Rotation3)
{
    transform *Root = GetLocalPoseTransform(Pose, 0);
    if(Root)
    {
        VectorAdd3(Root->Position, Translation3);
        quad Rotation4;
        AngularVelocityToQuaternion(Rotation4, Rotation3);
        QuaternionMultiply4(Root->Orientation,
                            Rotation4,
                            Root->Orientation);
    }
}

void GRANNY
UpdateModelMatrix(model_instance const &ModelInstance,
                  real32 SecondsElapsed,
                  real32 const *ModelMatrix4x4,
                  real32 *DestMatrix4x4,
                  bool Inverse)
{
    triple ResultTranslation;
    triple ResultRotation;
    GetRootMotionVectors(ModelInstance, SecondsElapsed,
                         ResultTranslation, ResultRotation, Inverse);
    ApplyRootMotionVectorsToMatrix(ModelMatrix4x4,
                                   ResultTranslation, ResultRotation,
                                   DestMatrix4x4);
}

void **GRANNY
GetModelUserDataArray(model_instance const &ModelInstance)
{
    return((void **)ModelInstance.UserData);
}

model_instance *GRANNY
GetGlobalModelInstancesBegin(void)
{
    return(GetGlobalModelInstanceSentinel().Next);
}

model_instance *GRANNY
GetGlobalModelInstancesEnd(void)
{
    return(&GetGlobalModelInstanceSentinel());
}

model_instance *GRANNY
GetGlobalNextModelInstance(model_instance *Instance)
{
    return(Instance ? Instance->Next : 0);
}


BEGIN_GRANNY_NAMESPACE;
void ModelInstanceFrameStats()
{
#define PRE "Granny/ModelInstance/"
    GRANNY_EMIT_INT_VALUE(PRE "Total",          g_ModelInstanceStatus.TotalInstanceCount);
    GRANNY_EMIT_INT_VALUE(PRE "InstancedBones", g_ModelInstanceStatus.TotalInstancedBoneCount);
    GRANNY_EMIT_INT_VALUE(PRE "MaxModelBones",       g_ModelInstanceStatus.MaxBoneCount);
}
END_GRANNY_NAMESPACE;

