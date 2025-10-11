// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_character_instance.cpp $
// $DateTime: 2012/07/26 17:17:41 $
// $Change: 38535 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#include "gstate_character_instance.h"
#include "gstate_character_info.h"
#include "gstate_token_context.h"
#include "gstate_state_machine.h"
#include <string.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;
using namespace std;



gstate_character_instance*
GStateInstantiateRetargetedCharacter(gstate_character_info* Info,
                                     granny_real32          CurrentTime,
                                     granny_int32x          AnimationSet,
                                     granny_model*          SourceModel,
                                     granny_model*          TargetModel)
{
    if (!Info || Info->StateMachine.Object == 0)
    {
        GStateError("Invalid arguments to InstantiateCharacter");
        return 0;
    }

    GStateCheckIndex(AnimationSet, Info->AnimationSetCount, return 0);

    // todo: check that Info's file references have been successfully loaded.

    gstate_character_instance* Result;
    GStateAllocZeroedStruct(Result);
    if (Result)
    {
        // TODO: error handling
        Result->TargetModel  = TargetModel;
        Result->SourceModel  = SourceModel;
        Result->AnimationSet = AnimationSet;
        Result->CurrentTime  = CurrentTime;
        Result->DeltaT       = -1;

        if (TargetModel != SourceModel)
        {
            Result->Retargeter = GrannyAcquireRetargeter(SourceModel, TargetModel);
            if (Result->Retargeter == 0)
            {
                GStateError("Unable to create retargeter for Source(%s) -> Target(%s)",
                            SourceModel->Name, TargetModel->Name);
                GStateDeallocate(Result);
                return 0;
            }
        }
        else
        {
            Result->Retargeter = 0;
        }

        Result->TokenContext  = GStateNew<token_context>(Info->NumUniqueTokenized);
        Result->CharacterInfo = Info;
        {
            tokenized* Product =
                Result->TokenContext->CreateFromToken(Info->StateMachine.Type,
                                                      Info->StateMachine.Object);
            Result->StateMachine = GSTATE_DYNCAST(Product, state_machine);
            if (Result->StateMachine == 0)
            {
                GS_PreconditionFailed;
                GStateDelete<tokenized>(Product);
            }
            else
            {
                Result->StateMachine->CaptureSiblingData();
                Result->StateMachine->BindToCharacter(Result);
                Result->StateMachine->Activate(0, Result->CurrentTime);
            }
        }
    }

    return Result;
}

gstate_character_instance*
GStateInstantiateCharacter(gstate_character_info* Info,
                           granny_real32          CurrentTime,
                           granny_int32x          AnimationSet,
                           granny_model*          Model)
{
    return GStateInstantiateRetargetedCharacter(Info, CurrentTime, AnimationSet, Model, Model);
}

gstate_character_instance* GSTATE
InstantiateEditCharacter(gstate_character_info* Info,
                         granny_int32x AnimationSet,
                         granny_model* Model)
{
    if (!Info)
    {
        GStateError("Invalid arguments to InstantiateEditCharacter");
        return 0;
    }

    // todo: check that Info's file references have been successfully loaded.

    gstate_character_instance* Result;
    GStateAllocZeroedStruct(Result);
    if (Result)
    {
        // TODO: error handling
        Result->TargetModel = Model;
        Result->SourceModel = Model;
        Result->Retargeter  = 0;

        Result->TokenContext  = GStateNew<token_context>();
        Result->CharacterInfo = Info;
        Result->StateMachine = 0;
        Result->AnimationSet = AnimationSet;
        Result->CurrentTime  = 0;
        Result->DeltaT       = -1;
    }

    return Result;
}

void GSTATE
UnbindEditCharacter(gstate_character_instance* Instance)
{
    GStateAssert(Instance);
    GStateAssert(Instance->CharacterInfo);

    if (Instance->StateMachine)
    {
        Instance->StateMachine->UnbindFromCharacter();
        Instance->StateMachine = 0;
    }
}

bool GSTATE
ReplaceStateMachine(gstate_character_instance* Instance,
                    state_machine*             NewMachine)
{
    GStateAssert(Instance);
    GStateAssert(Instance->CharacterInfo);

    UnbindEditCharacter(Instance);
    Instance->StateMachine = NewMachine;

    bool Success = true;
    if (Instance->StateMachine)
    {
        GStateAssert(Instance->AnimationSet >= 0 &&
                     Instance->AnimationSet < Instance->CharacterInfo->AnimationSetCount);

        Success = Instance->StateMachine->BindToCharacter(Instance);
        Instance->StateMachine->Activate(0, Instance->CurrentTime);
    }

    return Success;
}

void GSTATE
SetCurrentAnimationSet(gstate_character_instance* Instance, granny_int32x Set)
{
    GStateAssert(Instance);
    GStateAssert(Instance->CharacterInfo);
    GStateAssert(Set >= 0 && Set < Instance->CharacterInfo->AnimationSetCount);

    Instance->AnimationSet = Set;
}

void
GStateFreeCharacterInstance(gstate_character_instance* Instance)
{
    if (!Instance)
        return;

    GrannyReleaseRetargeter(Instance->Retargeter);
    Instance->Retargeter = 0;

    // If we are an edit character, and are not supposed to delete this, it will already
    // be gone...
    if (Instance->StateMachine)
    {
        Instance->StateMachine->UnbindFromCharacter();
        GStateDelete<state_machine>(Instance->StateMachine);
        Instance->StateMachine = 0;
    }

    GStateDelete<token_context>(Instance->TokenContext);
    GStateDeallocate(Instance);
}

state_machine*
GStateGetStateMachine(gstate_character_instance* Instance)
{
    if (!Instance)
        return 0;

    return Instance->StateMachine;
}


gstate_character_info*
GStateGetInfoForCharacter(gstate_character_instance* Instance)
{
    if (!Instance)
        return 0;

    return Instance->CharacterInfo;
}

granny_model* GSTATE
GetTargetModelForCharacter(gstate_character_instance* Instance)
{
    if (!Instance)
        return 0;

    return Instance->TargetModel;
}

granny_model* GSTATE
GetSourceModelForCharacter(gstate_character_instance* Instance)
{
    if (!Instance)
        return 0;

    return Instance->SourceModel;
}

granny_retargeter* GSTATE
GetCharacterRetargeter(gstate_character_instance* Instance)
{
    if (!Instance)
    {
        GStateError("Invalid Argument to GetCharacterRetargeter");
        return 0;
    }

    return Instance->Retargeter;
}


bool
GStateUpdateModelMatrix(gstate_character_instance*  Instance,
                        granny_real32 const*        ModelMatrix4x4,
                        granny_real32*              DestMatrix4x4)
{
    GStateCheckPtrNotNULL(Instance, return false);
    GStateCheckPtrNotNULL(ModelMatrix4x4, return false);
    GStateCheckPtrNotNULL(DestMatrix4x4, return false);
    GStateCheckPtrNotNULL(Instance->StateMachine, return false);

    GStateCheck(Instance->DeltaT >= 0.0f && "Must call GStateAdvanceTime", return false);

    granny_triple ResultTranslation = { 0, 0, 0 };
    granny_triple ResultRotation = { 0, 0, 0 };

    if (!Instance->StateMachine->GetRootMotionVectors(0,
                                                      Instance->CurrentTime,
                                                      Instance->DeltaT,
                                                      ResultTranslation,
                                                      ResultRotation,
                                                      false))
    {
        return false;
    }

    GrannyApplyRootMotionVectorsToMatrix(ModelMatrix4x4,
                                         ResultTranslation, ResultRotation,
                                         DestMatrix4x4);

    return true;
}

void
GStateAdvanceTime(gstate_character_instance* Instance,
                  granny_real32              DeltaT)
{
    GStateCheckPtrNotNULL(Instance, return);
    GStateCheckPtrNotNULL(Instance->StateMachine, return);
    GStateCheck(DeltaT >= 0, return);

    Instance->CurrentTime += DeltaT;
    Instance->DeltaT       = DeltaT;

    Instance->StateMachine->AdvanceT(Instance->CurrentTime, Instance->DeltaT);
}


granny_real32
GStateInstanceTime(EXPIN gstate_character_instance* Instance)
{
    GStateCheckPtrNotNULL(Instance, return 0);

    return Instance->CurrentTime;
}


granny_real32
GStateInstanceLastDeltaT(EXPIN gstate_character_instance* Instance)
{
    GStateCheckPtrNotNULL(Instance, return 0);

    return Instance->DeltaT;
}


granny_local_pose*
GStateSampleAnimationLOD(gstate_character_instance* Instance,
                         granny_real32              AllowedError,
                         granny_pose_cache*         PoseCache)
{
    GStateCheckPtrNotNULL(Instance, return 0);
    GStateCheckPtrNotNULL(PoseCache, return 0);
    GStateCheckPtrNotNULL(Instance->StateMachine, return 0);
    GStateCheck(Instance->DeltaT >= 0 && "Must call GStateAdvanceTime", return 0);

    granny_local_pose* Pose =
        Instance->StateMachine->SamplePoseOutput(0, Instance->CurrentTime, AllowedError, PoseCache);
    if (Pose && Instance->Retargeter)
    {
        granny_int32 BoneCount = Instance->TargetModel->Skeleton->BoneCount;
        granny_local_pose* TargPose =
            GrannyGetNewLocalPose(PoseCache, BoneCount);

        if (GrannyRetargetPose(Pose, TargPose, 0, BoneCount, Instance->Retargeter))
        {
            // Success!
            GrannyReleaseCachePose(PoseCache, Pose);
            Pose = TargPose;
        }
        else
        {
            // Well, that's bad. um
            GrannyReleaseCachePose(PoseCache, TargPose);
            GrannyReleaseCachePose(PoseCache, Pose);
            return 0;
        }
    }

    return Pose;
}

granny_local_pose*
GStateSampleAnimation(gstate_character_instance* Instance,
                      granny_pose_cache*         PoseCache)
{
    return GStateSampleAnimationLOD(Instance, 0, PoseCache);
}

animation_spec* GSTATE
GetSpecForCharacterSlot(gstate_character_instance* Instance, animation_slot* AnimSlot)
{
    GStateCheckPtrNotNULL(Instance, return 0);
    GStateCheckPtrNotNULL(AnimSlot, return 0);

    gstate_character_info* Info = Instance->CharacterInfo;
    GStateCheckIndex(AnimSlot->Index, Instance->CharacterInfo->AnimationSlotCount, return 0);
    GStateCheckIndex(Instance->AnimationSet, Instance->CharacterInfo->AnimationSetCount, return 0);

    return &Info->AnimationSets[Instance->AnimationSet]->AnimationSpecs[AnimSlot->Index];
}
