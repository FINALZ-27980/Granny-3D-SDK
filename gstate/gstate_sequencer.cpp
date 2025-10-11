// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_sequencer.cpp $
// $DateTime: 2012/03/16 15:41:10 $
// $Change: 36794 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "gstate_sequencer.h"

#define GSTATE_INTERNAL_HEADER 1

#include "gstate_character_instance.h"
#include "gstate_character_internal.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"

#include <string.h>
USING_GSTATE_NAMESPACE;

struct GSTATE sequencer::sequencerImpl
{
    granny_int32     UsedAnimationSlotCount;
    animation_slot** UsedAnimationSlots;
};

granny_data_type_definition GSTATE
sequencer::sequencerImplType[] =
{
    { GrannyArrayOfReferencesMember, "UsedAnimationSlots", AnimationSlotType },
    { GrannyEndMember },
};

// sequencer is a concrete class, so we must create a slotted container
struct sequencer_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(sequencer);
};

granny_data_type_definition sequencer::sequencerTokenType[] =
{
    DECL_UID_MEMBER(sequencer),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(sequencer),

    { GrannyEndMember }
};

void GSTATE
sequencer::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(sequencer);

    GStateCloneArray(m_sequencerToken->UsedAnimationSlots,
                     OldToken->UsedAnimationSlots,
                     m_sequencerToken->UsedAnimationSlotCount);
    
    GS_NotYetImplemented;
}

void GSTATE
sequencer::ReleaseOwnedToken_sequencer()
{
    GStateDeallocate(m_sequencerToken->UsedAnimationSlots);
}

IMPL_CREATE_DEFAULT(sequencer);

GSTATE
sequencer::sequencer(token_context*               Context,
                     granny_data_type_definition* TokenType,
                     void*                        TokenObject,
                     token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_sequencerToken(0),
    m_Animations(0)
{
    IMPL_INIT_FROM_TOKEN(sequencer);

    if (EditorCreated())
    {
        AddOutput(PoseEdge, "Pose");
    }
}


GSTATE
sequencer::~sequencer()
{
    for (int Idx = 0; Idx < m_sequencerToken->UsedAnimationSlotCount; ++Idx)
    {
        GStateDeallocate(m_Animations[Idx]);
    }
    GStateDeallocate(m_Animations);
    
    DTOR_RELEASE_TOKEN(sequencer);
}

bool GSTATE
sequencer::FillDefaultToken(granny_data_type_definition* TokenType,
                            void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares sequencerImpl*& Slot = // member
    GET_TOKEN_SLOT(sequencer);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


bool GSTATE
sequencer::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    granny_model* Model = GetSourceModelForCharacter(Instance);
    GStateAssert(Model);

    GStateAllocZeroedArray(m_Animations, m_sequencerToken->UsedAnimationSlotCount);
    if (!m_Animations)
        return false;

    for (int Idx = 0; Idx < m_sequencerToken->UsedAnimationSlotCount; ++Idx)
    {
        if (m_sequencerToken->UsedAnimationSlots[Idx] == 0)
            continue;

        animation_slot* AnimSlot = m_sequencerToken->UsedAnimationSlots[Idx];
        if (AnimSlot == 0)
            continue;

        animation_spec* AnimSpec = GetSpecForCharacterSlot(Instance, AnimSlot);
        if (AnimSpec == 0)
            continue;

        source_file_ref* SourceFileRef = AnimSpec->SourceReference;
        if (SourceFileRef == 0)
            continue;

        granny_file_info* Info = SourceFileRef->SourceInfo;
        if (Info == 0)
            continue;

        // Check the animation as best we can.
        if (GS_InRange(AnimSpec->AnimationIndex, Info->AnimationCount) == false)
        {
            GStateError("Out of range animation index in gstate_anim_source.cpp");
            return false;
        }

        granny_animation* Animation = Info->Animations[AnimSpec->AnimationIndex];
        if (AnimSpec->ExpectedName && _stricmp(AnimSpec->ExpectedName, Animation->Name) != 0)
        {
            GStateError("Found incorrect name in BindCharacter\n");
            GStateError("  expected ('%s', found '%s')\n", AnimSpec->ExpectedName, Animation->Name);
            return false;
        }

        
        // Allocate the controlled animation...
        GStateAllocZeroedStruct(m_Animations[Idx]);

        GStateAssert(m_Animations[Idx]->Binding == 0);

        granny_int32x TrackGroupIndex = -1;
        if (GrannyFindTrackGroupForModel(Animation, Model->Name, &TrackGroupIndex))
        {
            GStateAssert(GS_InRange(TrackGroupIndex, Animation->TrackGroupCount));
            granny_track_group* TrackGroup = Animation->TrackGroups[TrackGroupIndex];

            granny_animation_binding_identifier ID = { 0 };
            ID.Animation = Animation;
            ID.SourceTrackGroupIndex = TrackGroupIndex;
            ID.OnModel = Model;

            m_Animations[Idx]->Binding          = GrannyAcquireAnimationBindingFromID(&ID);
            m_Animations[Idx]->AccumulationMode = GrannyAccumulationModeFromTrackGroup(TrackGroup);
        }
    }
    

    return true;
}


void GSTATE
sequencer::UnbindFromCharacter()
{
    parent::UnbindFromCharacter();

    for (int Idx = 0; Idx < m_sequencerToken->UsedAnimationSlotCount; ++Idx)
    {
        if (m_Animations[Idx] && m_Animations[Idx]->Binding != 0)
        {
            GrannyReleaseAnimationBinding(m_Animations[Idx]->Binding);
            m_Animations[Idx]->Binding = 0;
        }

        GStateDeallocate(m_Animations[Idx]);
    }
    GStateDeallocate(m_Animations);
}

void GSTATE
sequencer::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}

void GSTATE
sequencer::Activate(granny_int32x OnOutput,
                    granny_real32 AtT)
{
    GStateAssert(OnOutput == 0);
    GS_NotYetImplemented;
}

bool GSTATE
sequencer::DidLoopOccur(granny_int32x OnOutput, granny_real32 AtT, granny_real32 DeltaT)
{
    GStateAssert(OnOutput == 0);

    GS_NotYetImplemented;
    return false;
}    

granny_real32 GSTATE
sequencer::GetDuration(granny_int32x OnOutput)
{
    GStateAssert(OnOutput == 0);

    GS_NotYetImplemented;
    return -1;
}

granny_local_pose* GSTATE
sequencer::SamplePoseOutput(granny_int32x OutputIdx,
                            granny_real32 AtT,
                            granny_real32 AllowedError,
                            granny_pose_cache* PoseCache)
{
    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache != 0);

    GS_NotYetImplemented;
    return 0;
}


bool GSTATE
sequencer::GetRootMotionVectors(granny_int32x  OutputIdx,
                                granny_real32  AtT,
                                granny_real32  DeltaT,
                                granny_real32* ResultTranslation,
                                granny_real32* ResultRotation,
                                bool           Inverse)
{
    GStateAssert(GetBoundCharacter());

    if (DeltaT < 0)
    {
        GS_PreconditionFailed;
        return false;
    }
    if (!ResultTranslation || !ResultRotation)
    {
        GS_PreconditionFailed;
        return false;
    }

    GS_NotYetImplemented;
    memset(ResultTranslation, 0, sizeof(granny_real32)*3);
    memset(ResultRotation,    0, sizeof(granny_real32)*3);
    return true;
}

