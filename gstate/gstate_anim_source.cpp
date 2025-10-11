// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_anim_source.cpp $
// $DateTime: 2012/10/29 09:39:25 $
// $Change: 40103 $
// $Revision: #10 $
//
// $Notice: $
// ========================================================================
#include "gstate_anim_source.h"
#include "gstate_anim_utils.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"
#include "gstate_quick_vecs.h"

#include <string.h>
#include <math.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

static char const* s_LoopText = "Looped";


struct event_edge
{
    granny_bool32 UseAnimEvents;
    char*         TrackName;  // 0 = No connection if not "AnimationEvents"
};

struct scalar_edge
{
    char*         TrackName;  // 0 = no connection
    granny_uint32 TrackKey;
};

struct morph_edge
{
    char* MeshName;  // 0 = no connection
};

static void DefaultEventEdge(event_edge& Edge)
{
    Edge.TrackName = 0;
}

static void DefaultMorphEdge(morph_edge& Edge)
{
    Edge.MeshName = 0;
}

static void DefaultScalarEdge(scalar_edge& Edge)
{
    Edge.TrackName = 0;
    Edge.TrackKey = 0;
}

struct GSTATE anim_source::anim_sourceImpl
{
    animation_slot* AnimationSlot;

    granny_bool32   ClampedLooping;
    granny_real32   Speed;

    granny_int32 EventEdgeCount;
    event_edge*  EventEdges;

    granny_int32 ScalarEdgeCount;
    scalar_edge* ScalarEdges;

    granny_int32 MorphEdgeCount;
    morph_edge* MorphEdges;
};

granny_data_type_definition EventEdgeType[] =
{
    { GrannyBool32Member, "UseAnimEvents" },
    { GrannyStringMember, "TrackName" },
    { GrannyEndMember },
};

granny_data_type_definition ScalarEdgeType[] =
{
    { GrannyStringMember, "TrackName" },
    { GrannyUInt32Member, "TrackKey" },
    { GrannyEndMember },
};

granny_data_type_definition MorphEdgeType[] =
{
    { GrannyStringMember, "MeshName" },
    { GrannyEndMember },
};

granny_data_type_definition GSTATE
anim_source::anim_sourceImplType[] =
{
    { GrannyReferenceMember, "AnimationSlot", AnimationSlotType },
    { GrannyBool32Member,    "ClampedLooping" },
    { GrannyReal32Member,    "Speed" },

    { GrannyReferenceToArrayMember, "EventEdges",  EventEdgeType  },
    { GrannyReferenceToArrayMember, "ScalarEdges", ScalarEdgeType },
    { GrannyReferenceToArrayMember, "MorphEdges",  MorphEdgeType  },

    { GrannyEndMember },
};

// anim_source is a concrete class, so we must create a slotted container
struct anim_source_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(anim_source);
};

granny_data_type_definition anim_source::anim_sourceTokenType[] =
{
    DECL_UID_MEMBER(anim_source),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(anim_source),

    { GrannyEndMember }
};

void GSTATE
anim_source::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(anim_source);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneArray(m_anim_sourceToken->EventEdges,
                     OldToken->EventEdges,
                     m_anim_sourceToken->EventEdgeCount);
    {for (int Idx = 0; Idx < m_anim_sourceToken->EventEdgeCount; ++Idx)
    {
        GStateCloneString(m_anim_sourceToken->EventEdges[Idx].TrackName,
                          OldToken->EventEdges[Idx].TrackName);
    }}

    GStateCloneArray(m_anim_sourceToken->ScalarEdges,
                     OldToken->ScalarEdges,
                     m_anim_sourceToken->ScalarEdgeCount);
    {for (int Idx = 0; Idx < m_anim_sourceToken->ScalarEdgeCount; ++Idx)
    {
        GStateCloneString(m_anim_sourceToken->ScalarEdges[Idx].TrackName,
                          OldToken->ScalarEdges[Idx].TrackName);
    }}

    GStateCloneArray(m_anim_sourceToken->MorphEdges,
                     OldToken->MorphEdges,
                     m_anim_sourceToken->MorphEdgeCount);
    {for (int Idx = 0; Idx < m_anim_sourceToken->MorphEdgeCount; ++Idx)
    {
        GStateCloneString(m_anim_sourceToken->MorphEdges[Idx].MeshName,
                          OldToken->MorphEdges[Idx].MeshName);
    }}
}

void GSTATE
anim_source::ReleaseOwnedToken_anim_source()
{
    {for (int Idx = 0; Idx < m_anim_sourceToken->EventEdgeCount; ++Idx)
    {
        GStateDeallocate(m_anim_sourceToken->EventEdges[Idx].TrackName);
    }}
    GStateDeallocate(m_anim_sourceToken->EventEdges);
    {for (int Idx = 0; Idx < m_anim_sourceToken->ScalarEdgeCount; ++Idx)
    {
        GStateDeallocate(m_anim_sourceToken->ScalarEdges[Idx].TrackName);
    }}
    GStateDeallocate(m_anim_sourceToken->ScalarEdges);
    {for (int Idx = 0; Idx < m_anim_sourceToken->MorphEdgeCount; ++Idx)
    {
        GStateDeallocate(m_anim_sourceToken->MorphEdges[Idx].MeshName);
    }}
    GStateDeallocate(m_anim_sourceToken->MorphEdges);
}


IMPL_CREATE_DEFAULT(anim_source);

GSTATE
anim_source::anim_source(token_context*               Context,
                         granny_data_type_definition* TokenType,
                         void*                        TokenObject,
                         token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_anim_sourceToken(0),
    m_Animation(0),
    m_EventTracks(0),
    m_ScalarTracks(0),
    m_ScalarDefaults(0),
    m_MorphTracks(0),
    m_LocalOffset(0)
{
    IMPL_INIT_FROM_TOKEN(anim_source);
    {
        // Not setup yet if editor created...
        if (GetNumOutputs() >= 1)
        {
            GStateAllocZeroedStruct(m_Animation);
        }
        if (m_anim_sourceToken->EventEdgeCount != 0)
        {
            GStateAllocZeroedArray(m_EventTracks, m_anim_sourceToken->EventEdgeCount);
        }
        if (m_anim_sourceToken->ScalarEdgeCount != 0)
        {
            GStateAllocZeroedArray(m_ScalarTracks, m_anim_sourceToken->ScalarEdgeCount);
            GStateAllocZeroedArray(m_ScalarDefaults, m_anim_sourceToken->ScalarEdgeCount);
        }
        if (m_anim_sourceToken->MorphEdgeCount != 0)
        {
            GStateAllocZeroedArray(m_MorphTracks, m_anim_sourceToken->MorphEdgeCount);
        }
    }

    if (EditorCreated())
    {
        // Add our default output
        AddOutput(PoseEdge, "Animation");
    }
}


GSTATE
anim_source::~anim_source()
{
    DTOR_RELEASE_TOKEN(anim_source);

    GStateDeallocate(m_Animation);
    GStateDeallocate(m_EventTracks);
    GStateDeallocate(m_MorphTracks);
    GStateDeallocate(m_ScalarTracks);
    GStateDeallocate(m_ScalarDefaults);
}

bool GSTATE
anim_source::FillDefaultToken(granny_data_type_definition* TokenType,
                              void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares anim_sourceImpl*& Slot = // member
    GET_TOKEN_SLOT(anim_source);

    // Our slot in this token should be empty.
    // Create a new anim source Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->AnimationSlot  = 0;
    Slot->ClampedLooping = 0;
    Slot->Speed          = 1.0f;

    Slot->EventEdgeCount = 0;
    Slot->EventEdges     = 0;

    Slot->ScalarEdgeCount = 0;
    Slot->ScalarEdges     = 0;

    Slot->MorphEdgeCount = 0;
    Slot->MorphEdges     = 0;

    return true;
}


bool GSTATE
anim_source::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    // Only output 0 can be an animation
    GStateAssert(GetNumOutputs() >= 1);
    GStateAssert(GetOutputType(0) == PoseEdge);
    GStateAssert(IsOutputExternal(0));

    // Todo: should be returning true in these cases?

    if (m_anim_sourceToken->AnimationSlot == 0)
        return true;

    granny_model* Model = GetSourceModelForCharacter(Instance);
    GStateAssert(Model);

    // We'll use this below in the event bind...

    animation_slot* AnimSlot = m_anim_sourceToken->AnimationSlot;
    if (AnimSlot == 0)
        return true;

    animation_spec* AnimSpec = GetSpecForCharacterSlot(Instance, AnimSlot);
    if (AnimSpec == 0)
        return true;

    source_file_ref* SourceFileRef = AnimSpec->SourceReference;
    if (SourceFileRef == 0)
        return true;

    granny_file_info* Info = SourceFileRef->SourceInfo;
    if (Info == 0)
        return true;

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

    GStateAssert(m_Animation->Binding == 0);
    granny_int32x TrackGroupIndex = -1;
    if (GrannyFindTrackGroupForModel(Animation, Model->Name, &TrackGroupIndex))
    {
        GStateAssert(GS_InRange(TrackGroupIndex, Animation->TrackGroupCount));
        granny_track_group* TrackGroup = Animation->TrackGroups[TrackGroupIndex];

        granny_animation_binding_identifier ID = { 0 };
        ID.Animation = Animation;
        ID.SourceTrackGroupIndex = TrackGroupIndex;
        ID.OnModel = Model;

        m_Animation->Binding          = GrannyAcquireAnimationBindingFromID(&ID);
        m_Animation->AccumulationMode = GrannyAccumulationModeFromTrackGroup(TrackGroup);
    }

    // MorphTracks
    if (Animation)
    {
        {for (int EdgeIdx = 1; EdgeIdx < GetNumOutputs(); ++EdgeIdx)
        {
            int const TrackSlotIdx = EdgeIdx - 1;
            GStateAssert(m_MorphTracks[TrackSlotIdx] == 0);

            if (m_anim_sourceToken->MorphEdges[TrackSlotIdx].MeshName)
            {
                GStateAssert(GetOutputType(EdgeIdx) == MorphEdge);
                GStateAssert(m_anim_sourceToken->EventEdges[TrackSlotIdx].TrackName == 0);
                GStateAssert(m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackName == 0);

                granny_int32x TrackIdx;
                if (GrannyFindMorphTrackGroupForMesh(Animation,
                                                     m_anim_sourceToken->MorphEdges[TrackSlotIdx].MeshName,
                                                     &TrackIdx))
                {
                    m_MorphTracks[TrackSlotIdx] = Animation->TrackGroups[TrackIdx];
                }
            }
        }}
    }


    // Need to bind the text tracks and scalar...
    if (TrackGroupIndex != -1)
    {
        granny_track_group* TrackGroup = Animation->TrackGroups[TrackGroupIndex];

        // Separate out the loops for these, just to keep it simple...  First text_tracks
        {for (int EdgeIdx = 1; EdgeIdx < GetNumOutputs(); ++EdgeIdx)
        {
            int const TrackSlotIdx = EdgeIdx - 1;
            GStateAssert(m_EventTracks[TrackSlotIdx] == 0);

            if (m_anim_sourceToken->EventEdges[TrackSlotIdx].TrackName)
            {
                GStateAssert(GetOutputType(EdgeIdx) == EventEdge);
                GStateAssert(m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackName == 0);
                GStateAssert(m_anim_sourceToken->MorphEdges[TrackSlotIdx].MeshName == 0);

                granny_int32x TextTrackIdx;
                if (GrannyFindTextTrackByName(TrackGroup,
                                              m_anim_sourceToken->EventEdges[TrackSlotIdx].TrackName,
                                              &TextTrackIdx))
                {
                    m_EventTracks[TrackSlotIdx] = &TrackGroup->TextTracks[TextTrackIdx];
                }
            }
        }}

        // ...and then scalar tracks
        {for (int EdgeIdx = 1; EdgeIdx < GetNumOutputs(); ++EdgeIdx)
        {
            int const TrackSlotIdx = EdgeIdx - 1;
            GStateAssert(m_ScalarTracks[TrackSlotIdx] == 0);

            if (m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackName)
            {
                GStateAssert(GetOutputType(EdgeIdx) == ScalarEdge);
                GStateAssert(m_anim_sourceToken->EventEdges[TrackSlotIdx].TrackName == 0);
                GStateAssert(m_anim_sourceToken->MorphEdges[TrackSlotIdx].MeshName == 0);

                granny_int32x TextTrackIdx;
                if (GrannyFindVectorTrack(TrackGroup,
                                          m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackName,
                                          m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackKey,
                                          &TextTrackIdx))
                {
                    m_ScalarTracks[TrackSlotIdx] =
                        &TrackGroup->VectorTracks[TextTrackIdx].ValueCurve;
                }
            }
        }}
    }

    return true;
}


void GSTATE
anim_source::UnbindFromCharacter()
{
    parent::UnbindFromCharacter();

    // Only output 0 can be an animation
    GStateAssert(GetNumOutputs() >= 1);
    GStateAssert(GetOutputType(0) == PoseEdge);
    {
        GStateAssert(IsOutputExternal(0));
        GStateAssert(GetOutputType(0) == PoseEdge);

        if (m_Animation->Binding != 0)
        {
            GrannyReleaseAnimationBinding(m_Animation->Binding);
            m_Animation->Binding = 0;
        }
    }

    // Need to unbind the text/scalar tracks...
    {for (int EdgeIdx = 1; EdgeIdx < GetNumOutputs(); ++EdgeIdx)
    {
        // These are actually quite simple, just zero out the reference.  Note the -1 to
        // account for the fact that event edges start at 1
        m_EventTracks[EdgeIdx - 1] = 0;
        m_MorphTracks[EdgeIdx - 1] = 0;
        m_ScalarTracks[EdgeIdx - 1] = 0;
    }}
}


void GSTATE
anim_source::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}

granny_real32 GSTATE
anim_source::GetDuration(granny_int32x OnOutput)
{
    GStateAssert(OnOutput == 0);

    animation_slot* AnimSlot = m_anim_sourceToken->AnimationSlot;
    if (AnimSlot == 0)
        return -1;

    // After this point, we will return a positive duration, even if we don't have a bound
    // instance.  This is to allow proper treatment of phase-locked blends in the
    // editor...
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance == 0)
        return 1;

    animation_spec* AnimSpec = GetSpecForCharacterSlot(Instance, AnimSlot);
    if (AnimSpec == 0)
        return -1;

    if (AnimSpec->SourceReference == 0)
        return 1;

    granny_file_info* Info = AnimSpec->SourceReference->SourceInfo;
    if (Info == 0)
        return 1;

    if (AnimSpec->AnimationIndex < 0 || AnimSpec->AnimationIndex >= Info->AnimationCount)
        return 1;

    // This is validated in BindCharacter...
    return Info->Animations[AnimSpec->AnimationIndex]->Duration;
}

bool GSTATE
anim_source::GetPhaseBoundaries(granny_int32x  OnOutput,
                                granny_real32  AtT,
                                granny_real32* PhaseStart,
                                granny_real32* PhaseEnd)
{
    GStateAssert(OnOutput == 0);

    if (m_Animation == 0 || m_Animation->Binding == 0)
        return false;

    granny_real32 const Duration = m_Animation->Binding->ID.Animation->Duration;

    granny_int32x FakeLoopCount =
        (m_anim_sourceToken->ClampedLooping ? 1 : 0);
    granny_real32 LocalTime = ComputeModTime(AtT, m_anim_sourceToken->Speed,
                                             m_LocalOffset,
                                             FakeLoopCount,
                                             Duration);

    *PhaseStart = AtT - (LocalTime / m_anim_sourceToken->Speed);
    *PhaseEnd   = AtT + (Clamp(0, (Duration - LocalTime), Duration) / m_anim_sourceToken->Speed);

    return true;
}


granny_int32x GSTATE
anim_source::AddOutput(node_edge_type EdgeType,
                       char const*    EdgeName)
{

    // we can only add scalar/event edges after the first one
    GStateAssert(GetNumOutputs() == 0 ||
                 (EdgeType == EventEdge || EdgeType == ScalarEdge || EdgeType == MorphEdge));

    TakeTokenOwnership();

    int NewOutput = parent::AddOutput(EdgeType, EdgeName);
    GStateAssert(NewOutput == GetNumOutputs() - 1);
    if (NewOutput == 0)
    {
        GStateAssert(m_anim_sourceToken->AnimationSlot == 0);
        GStateAssert(m_Animation == 0);

        // This is the first and only pose edge
        m_anim_sourceToken->AnimationSlot = 0;
        m_anim_sourceToken->ClampedLooping = false;
        m_anim_sourceToken->Speed = 1.0f;

        GStateAllocZeroedStruct(m_Animation);

        return NewOutput;
    }
    else
    {
        // Todo: this is getting unwieldy
        // Need to bump the size of both the event and scalar edges, as well as the
        granny_int32 PreviousCount = m_anim_sourceToken->EventEdgeCount;

        event_edge& NewEventEdge = QVecPushNewElement(m_anim_sourceToken->EventEdgeCount,
                                                      m_anim_sourceToken->EventEdges);
        DefaultEventEdge(NewEventEdge);

        scalar_edge& NewScalarEdge = QVecPushNewElement(m_anim_sourceToken->ScalarEdgeCount,
                                                        m_anim_sourceToken->ScalarEdges);
        DefaultScalarEdge(NewScalarEdge);

        morph_edge& NewMorphEdge = QVecPushNewElement(m_anim_sourceToken->MorphEdgeCount,
                                                      m_anim_sourceToken->MorphEdges);
        DefaultMorphEdge(NewMorphEdge);

        // Dummy counts to let us use the QVec routines

        // EventTracks
        {
            granny_text_track*& NewRef = QVecPushNewElementNoCount(PreviousCount, m_EventTracks);
            NewRef = 0;
        }

        // ScalarTracks
        {
            granny_curve2*& NewRef    = QVecPushNewElementNoCount(PreviousCount, m_ScalarTracks);
            granny_real32& NewDefault = QVecPushNewElementNoCount(PreviousCount, m_ScalarDefaults);
            NewRef = 0;
            NewDefault = 0;
        }

        // MorphTracks
        {
            granny_track_group*& NewRef = QVecPushNewElementNoCount(PreviousCount, m_MorphTracks);
            NewRef = 0;
        }

        return NewOutput;
    }
}

bool GSTATE
anim_source::DeleteOutput(granny_int32x OutputIndex)
{
    GStateAssert(OutputIndex != 0); // cannot delete the animation

    TakeTokenOwnership();

    granny_int32x const ArrayIdx = OutputIndex - 1;

    // Subtlety: no need to rebind the character...

    granny_int32 PreviousCount = m_anim_sourceToken->EventEdgeCount;
    GStateAssert(PreviousCount == m_anim_sourceToken->ScalarEdgeCount);
    GStateAssert(PreviousCount == m_anim_sourceToken->MorphEdgeCount);

    // Pretty easy.  Since we don't own any of the tracks that we're pointing to, we can
    // just nuke everything.  Make sure to release the strings...
    GStateDeallocate(m_anim_sourceToken->EventEdges[ArrayIdx].TrackName);
    GStateDeallocate(m_anim_sourceToken->ScalarEdges[ArrayIdx].TrackName);
    GStateDeallocate(m_anim_sourceToken->MorphEdges[ArrayIdx].MeshName);

    QVecRemoveElement(ArrayIdx,
                      m_anim_sourceToken->EventEdgeCount,
                      m_anim_sourceToken->EventEdges);
    QVecRemoveElement(ArrayIdx,
                      m_anim_sourceToken->ScalarEdgeCount,
                      m_anim_sourceToken->ScalarEdges);
    QVecRemoveElement(ArrayIdx,
                      m_anim_sourceToken->MorphEdgeCount,
                      m_anim_sourceToken->MorphEdges);

    // events
    QVecRemoveElementNoCount(ArrayIdx, PreviousCount, m_EventTracks);

    // scalars
    QVecRemoveElementNoCount(ArrayIdx, PreviousCount, m_ScalarTracks);
    QVecRemoveElementNoCount(ArrayIdx, PreviousCount, m_ScalarDefaults);
        
    // morphs
    QVecRemoveElementNoCount(ArrayIdx, PreviousCount, m_MorphTracks);


    return parent::DeleteOutput(OutputIndex);
}


animation_slot* GSTATE
anim_source::GetAnimationSlot()
{
    return m_anim_sourceToken->AnimationSlot;
}

bool GSTATE
anim_source::GetOutputLoopClamping() const
{
    return (m_anim_sourceToken->ClampedLooping != 0);
}

granny_real32 GSTATE
anim_source::GetOutputSpeed() const
{
    return m_anim_sourceToken->Speed;
}


void GSTATE
anim_source::SetAnimationSlot(animation_slot* NewSpec)
{
    TakeTokenOwnership();

    // We need to unbind, and the rebind once the spec has been reset so we can grab our
    // animation resources, etc.
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    m_anim_sourceToken->AnimationSlot = NewSpec;

    // We need to clear the morphs, events, and scalars for this one, they'll be invalid,
    // in general
    {
        for (int Idx = 0; Idx < m_anim_sourceToken->EventEdgeCount; ++Idx)
        {
            GStateDeallocate(m_anim_sourceToken->EventEdges[Idx].TrackName);
            m_anim_sourceToken->EventEdges[Idx].TrackName = 0;
            m_EventTracks[Idx] = 0;
        }
        for (int Idx = 0; Idx < m_anim_sourceToken->MorphEdgeCount; ++Idx)
        {
            GStateDeallocate(m_anim_sourceToken->MorphEdges[Idx].MeshName);
            m_anim_sourceToken->MorphEdges[Idx].MeshName = 0;
            m_MorphTracks[Idx] = 0;
        }
        for (int Idx = 0; Idx < m_anim_sourceToken->ScalarEdgeCount; ++Idx)
        {
            GStateDeallocate(m_anim_sourceToken->ScalarEdges[Idx].TrackName);
            m_anim_sourceToken->ScalarEdges[Idx].TrackName = 0;
            m_ScalarTracks[Idx] = 0;
            m_ScalarDefaults[Idx] = 0;
        }
    }    

    if (Instance)
        BindToCharacter(Instance);

    // Run through the edges and clear the names back to the defaults...
    for (int Idx = 1; Idx < GetNumOutputs(); ++Idx)
    {
        switch (GetOutputType(Idx))
        {
            case ScalarEdge: SetOutputName(Idx, "Scalar"); break;
            case MorphEdge:  SetOutputName(Idx, "Morph"); break;
            case EventEdge:  SetOutputName(Idx, "Event"); break;
            default: break;
        }
    }
    
}

void GSTATE
anim_source::SetOutputLoopClamping(bool Clamped)
{
    TakeTokenOwnership();

    // No need to rebind for this case...
    m_anim_sourceToken->ClampedLooping = Clamped;
}

void GSTATE
anim_source::SetOutputSpeed(granny_real32 Speed)
{
    TakeTokenOwnership();

    // No need to rebind for this case...
    m_anim_sourceToken->Speed = Speed;
}


bool GSTATE
anim_source::GetEventUseAnimEvents(granny_int32x ForOutput)
{
    GStateAssert(GetOutputType(ForOutput) == EventEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return false;
    }

    // Note the -1!
    return (m_anim_sourceToken->EventEdges[ForOutput - 1].UseAnimEvents != 0);
}

char const* GSTATE
anim_source::GetEventTrackName(granny_int32x ForOutput)
{
    GStateAssert(GetOutputType(ForOutput) == EventEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return 0;
    }

    // Note the -1!
    return m_anim_sourceToken->EventEdges[ForOutput - 1].TrackName;
}

bool GSTATE
anim_source::SetEventTrack(granny_int32x ForOutput,
                           bool          UseAnimEvents,
                           char const*   TrackName)
{
    GStateAssert(GetOutputType(ForOutput) == EventEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return false;
    }

    TakeTokenOwnership();

    // We need to unbind, and the rebind once the spec has been reset so we can grab our
    // animation resources, etc.
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    // Common work
    {
        int const ArrayIndex = ForOutput - 1;

        event_edge& Edge = m_anim_sourceToken->EventEdges[ArrayIndex];

        Edge.UseAnimEvents = (UseAnimEvents ? 1 : 0);
        GStateReplaceString(Edge.TrackName, TrackName);
    }

    if (Instance)
        BindToCharacter(Instance);

    return true;
}

char const* GSTATE
anim_source::GetMorphTrackName(granny_int32x  ForOutput)
{
    GStateAssert(GetOutputType(ForOutput) == MorphEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return NULL;
    }

    // Note the -1!
    return m_anim_sourceToken->MorphEdges[ForOutput - 1].MeshName;
}

bool GSTATE
anim_source::SetMorphTrackName(granny_int32x ForOutput,
                               char const*   TrackName)
{
    GStateAssert(IsEditable());
    GStateAssert(GetOutputType(ForOutput) == MorphEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return false;
    }

    TakeTokenOwnership();

    // We need to unbind, and the rebind once the spec has been reset so we can grab our
    // animation resources, etc.
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    // Common work
    {
        int const ArrayIndex = ForOutput - 1;
        GStateDeallocate(m_anim_sourceToken->MorphEdges[ArrayIndex].MeshName);
        m_anim_sourceToken->MorphEdges[ArrayIndex].MeshName = 0;

        if (TrackName != 0)
            GStateCloneString(m_anim_sourceToken->MorphEdges[ArrayIndex].MeshName, TrackName);
    }

    if (Instance)
        BindToCharacter(Instance);

    return true;
}

bool GSTATE
anim_source::GetScalarTrack(granny_int32x  ForOutput,
                            char const*&   Name,
                            granny_uint32& TrackKey)
{
    GStateAssert(GetOutputType(ForOutput) == ScalarEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        Name = 0;
        TrackKey = 0;
        return false;
    }

    // Note the -1!
    Name = m_anim_sourceToken->ScalarEdges[ForOutput - 1].TrackName;
    TrackKey = m_anim_sourceToken->ScalarEdges[ForOutput - 1].TrackKey;
    return true;
}

bool GSTATE
anim_source::SetScalarTrack(granny_int32x       ForOutput,
                            char const*         TrackName,
                            granny_uint32 const TrackKey)
{
    GStateAssert(IsEditable());
    GStateAssert(GetOutputType(ForOutput) == ScalarEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return false;
    }

    TakeTokenOwnership();

    // We need to unbind, and the rebind once the spec has been reset so we can grab our
    // animation resources, etc.
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    // Common work
    {
        int const ArrayIndex = ForOutput - 1;
        GStateDeallocate(m_anim_sourceToken->ScalarEdges[ArrayIndex].TrackName);
        m_anim_sourceToken->ScalarEdges[ArrayIndex].TrackName = 0;
        m_anim_sourceToken->ScalarEdges[ArrayIndex].TrackKey = 0;

        if (TrackName != 0)
        {
            GStateCloneString(m_anim_sourceToken->ScalarEdges[ArrayIndex].TrackName, TrackName);
            m_anim_sourceToken->ScalarEdges[ArrayIndex].TrackKey = TrackKey;
        }
    }

    if (Instance)
        BindToCharacter(Instance);

    return true;
}

granny_local_pose* GSTATE
anim_source::SamplePoseOutput(granny_int32x OutputIdx,
                              granny_real32 AtT,
                              granny_real32 AllowedError,
                              granny_pose_cache* PoseCache)
{
    GStateAssert(OutputIdx == 0);
    GStateAssert(m_Animation);
    GStateAssert(PoseCache);

    granny_model* Model = GetSourceModelForCharacter(GetBoundCharacter());
    if (!Model)
        return 0;

    granny_skeleton* Skeleton = Model->Skeleton;
    if (m_Animation->Binding == 0)
    {
        // Return null, caller is resposible for building rest pose if necessary
        return 0;
    }

    granny_local_pose* IntoPose = GrannyGetNewLocalPose(PoseCache, Skeleton->BoneCount);
    granny_animation_binding* Binding = m_Animation->Binding;

    granny_int32x FakeLoopCount =
        (m_anim_sourceToken->ClampedLooping ? 1 : 0);
    granny_real32 const LocalSampleTime =
        ComputeModTime(AtT, m_anim_sourceToken->Speed, m_LocalOffset, FakeLoopCount,
                       Binding->ID.Animation->Duration);
    GStateAssert(LocalSampleTime >= 0);  // No negatives allowed!

    granny_sample_context Context;
    Context.UnderflowLoop = !m_anim_sourceToken->ClampedLooping;
    Context.OverflowLoop  = !m_anim_sourceToken->ClampedLooping;
    Context.LocalDuration = Binding->ID.Animation->Duration;
    Context.LocalClock    = Clamp(0.0f, LocalSampleTime, Context.LocalDuration);

    GrannySetContextFrameIndex(&Context,
                               Context.LocalClock,
                               Binding->ID.Animation->Duration,
                               Binding->ID.Animation->TimeStep);

    GrannyBeginLocalPoseAccumulation(IntoPose, 0, Skeleton->BoneCount, 0 );

    granny_int32x LODBoneCount = GrannyGetBoneCountForLOD(Skeleton, AllowedError);
    GrannyAccumulateControlledAnimation(m_Animation,
                                        &Context, 1.0f, Skeleton,
                                        0, LODBoneCount,
                                        IntoPose, AllowedError, 0);

    GrannyEndLocalPoseAccumulationLOD(IntoPose,
                                      0, Skeleton->BoneCount,
                                      0, LODBoneCount,
                                      Skeleton, AllowedError, 0);

    return IntoPose;
}

granny_real32 GSTATE
anim_source::SampleScalarOutput(granny_int32x OutputIdx,
                                granny_real32 AtT)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetOutputType(OutputIdx) == ScalarEdge);

    int EdgeIdx = OutputIdx - 1;
    if (m_ScalarTracks[EdgeIdx] != 0 && m_Animation && m_Animation->Binding)
    {
        granny_real32 Duration = m_Animation->Binding->ID.Animation->Duration;
        granny_int32x FakeLoopCount =
            (m_anim_sourceToken->ClampedLooping ? 1 : 0);
        granny_real32 const LocalSampleTime =
            ComputeModTime(AtT, m_anim_sourceToken->Speed, m_LocalOffset, FakeLoopCount,
                           Duration);

        granny_real32 Value;
        granny_real32 Identity = 0.0f;
        GrannyEvaluateCurveAtT(1, false,
                               !m_anim_sourceToken->ClampedLooping,
                               m_ScalarTracks[EdgeIdx],
                               !m_anim_sourceToken->ClampedLooping,
                               Duration,
                               LocalSampleTime,
                               &Value, &Identity);

        return Value;
    }
    else
    {
        return m_ScalarDefaults[EdgeIdx];
    }
}

granny_int32x GSTATE
anim_source::GetNumMorphChannels(granny_int32x OutputIdx)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);

    int EdgeIdx = OutputIdx - 1;
    if (m_MorphTracks[EdgeIdx] != 0)
        return m_MorphTracks[EdgeIdx]->VectorTrackCount;
    else
        return -1;
}

bool GSTATE
anim_source::SampleMorphOutput(granny_int32x  OutputIdx,
                               granny_real32  AtT,
                               granny_real32* MorphWeights,
                               granny_int32x NumMorphWeights)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);
    GStateAssert(MorphWeights != 0);

    int EdgeIdx = OutputIdx - 1;
    if (m_MorphTracks[EdgeIdx] != 0)
    {
        if (m_MorphTracks[EdgeIdx]->VectorTrackCount > NumMorphWeights)
        {
            GStateError("Too many morph channels to sample into given buffer");
            return false;
        }

        granny_real32 Duration = m_Animation->Binding->ID.Animation->Duration;
        granny_int32x FakeLoopCount =
            (m_anim_sourceToken->ClampedLooping ? 1 : 0);
        granny_real32 const LocalSampleTime =
            ComputeModTime(AtT, m_anim_sourceToken->Speed, m_LocalOffset, FakeLoopCount,
                           Duration);
        
        for (int TrackIdx = 0; TrackIdx < m_MorphTracks[EdgeIdx]->VectorTrackCount; ++TrackIdx)
        {
            granny_real32 Value;
            granny_real32 Identity = 0.0f;
            GrannyEvaluateCurveAtT(1, false,
                                   !m_anim_sourceToken->ClampedLooping,
                                   &m_MorphTracks[EdgeIdx]->VectorTracks[TrackIdx].ValueCurve,
                                   !m_anim_sourceToken->ClampedLooping,
                                   Duration,
                                   LocalSampleTime,
                                   &Value, &Identity);

            // Todo: binding!
            MorphWeights[TrackIdx] = Clamp(0, Value, 1);
        }

        return true;
    }
    else
    {
        return false;
    }
}


// bool GSTATE anim_source::GetScalarOutputRange()
//   for now, use node:: default, which returns false to indicate range query is not
//   supported.

bool GSTATE
anim_source::SampleEventOutput(granny_int32x            OutputIdx,
                               granny_real32            AtT,
                               granny_real32            DeltaT,
                               granny_text_track_entry* EventBuffer,
                               granny_int32x const      EventBufferSize,
                               granny_int32x*           NumEvents)
{
    GStateCheck(GS_InRange(OutputIdx, GetNumOutputs()), return false);
    GStateCheck(GetOutputType(OutputIdx) == EventEdge, return false);
    GStateCheck(DeltaT >= 0, return false);

    int TrackIndex = OutputIdx - 1;
    if (m_anim_sourceToken->EventEdges[TrackIndex].UseAnimEvents)
    {
        // For now, we only support the loop event...
        if (DidLoopOccur(0, AtT, DeltaT))
        {
            if (EventBufferSize > 0)
            {
                EventBuffer[0].TimeStamp = AtT;
                EventBuffer[0].Text      = s_LoopText;
                *NumEvents = 1;
            }

            return (EventBufferSize > 0);
        }
        else
        {
            *NumEvents = 0;
            return true;
        }
    }
    else
    {
        if (m_EventTracks[TrackIndex] == 0)
        {
            *NumEvents = 0;
            return true;
        }

        granny_text_track* TextTrack = m_EventTracks[TrackIndex];

        // Must have this if we have a bound event track...
        GStateAssert(m_Animation->Binding != 0);
        granny_animation_binding* Binding = m_Animation->Binding;
        granny_real32 const LocalDuration = Binding->ID.Animation->Duration;

        // Braindead linear search for right now...
        granny_int32x FakeLoopCount =
            (m_anim_sourceToken->ClampedLooping ? 1 : 0);

        granny_real32 LocalEndTime = ComputeLoopedTime(AtT,
                                                       m_anim_sourceToken->Speed,
                                                       m_LocalOffset, FakeLoopCount,
                                                       LocalDuration);
        granny_real32 LocalStartTime = ComputeLoopedTime(AtT - DeltaT,
                                                         m_anim_sourceToken->Speed,
                                                         m_LocalOffset, FakeLoopCount,
                                                         LocalDuration);
        GStateAssert(LocalEndTime >= LocalStartTime);

        // Find the first spot
        granny_real32 Offset = int(LocalStartTime / LocalDuration) * LocalDuration;
        LocalStartTime -= Offset;
        LocalEndTime   -= Offset;
        while (LocalStartTime < 0.0f)
        {
            LocalStartTime += LocalDuration;
            LocalEndTime   += LocalDuration;
            Offset         -= LocalDuration;
        }
        GStateAssert(LocalStartTime >= 0.0f && LocalStartTime < LocalDuration);

        bool Success = true;
        granny_int32x UsedEvents = 0;
        while(LocalEndTime > 0.0f)
        {
            {for (int EntryIdx = 0; EntryIdx < TextTrack->EntryCount; ++EntryIdx)
            {
                if (TextTrack->Entries[EntryIdx].TimeStamp >= LocalStartTime &&
                    TextTrack->Entries[EntryIdx].TimeStamp <= LocalEndTime)
                {
                    if (UsedEvents < EventBufferSize)
                    {
                        EventBuffer[UsedEvents] = TextTrack->Entries[EntryIdx];
                        EventBuffer[UsedEvents].TimeStamp += Offset - m_LocalOffset;
                    }
                    else
                    {
                        // Bail, buffer too small.
                        return false;
                    }
                    ++UsedEvents;
                }
            }}

            LocalStartTime -= LocalDuration;
            LocalEndTime   -= LocalDuration;
            Offset         += LocalDuration;
        }

        *NumEvents = UsedEvents;
        return Success;
    }
}

bool GSTATE
anim_source::GetAllEvents(granny_int32x            OutputIdx,
                          granny_text_track_entry* EventBuffer,
                          granny_int32x const      EventBufferSize,
                          granny_int32x*           NumEvents)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetOutputType(OutputIdx) == EventEdge);

    int TrackIndex = OutputIdx - 1;
    if (m_anim_sourceToken->EventEdges[TrackIndex].UseAnimEvents)
    {
        if (EventBufferSize > 0)
        {
            EventBuffer[0].TimeStamp = 0;
            EventBuffer[0].Text      = s_LoopText;
            *NumEvents = 1;
        }

        return (EventBufferSize > 0);
    }
    else
    {
        if (m_EventTracks[TrackIndex] == 0)
        {
            *NumEvents = 0;
            return true;
        }

        // Really easy at this level...
        granny_text_track* TextTrack = m_EventTracks[TrackIndex];

        // Enough space?
        if (TextTrack->EntryCount > EventBufferSize)
            return false;

        // Just copy and "unique-ize""
        memcpy(EventBuffer, TextTrack->Entries, sizeof(granny_text_track_entry) * TextTrack->EntryCount);
        *NumEvents = FilterDuplicateEvents(EventBuffer, 0, TextTrack->EntryCount);
    }

    return true;
}

bool GSTATE
anim_source::GetCloseEventTimes(granny_int32x  OutputIdx,
                                granny_real32  AtT,
                                char const*    TextToFind,
                                granny_real32* PrevTime,
                                granny_real32* NextTime)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetOutputType(OutputIdx) == EventEdge);

    int TrackIndex = OutputIdx - 1;

    if (m_anim_sourceToken->EventEdges[TrackIndex].UseAnimEvents)
    {
        // todo: for sync?
        return false;
    }
    
    if (m_EventTracks[TrackIndex] == 0)
        return false;

    granny_text_track* TextTrack = m_EventTracks[TrackIndex];

    // Must have this if we have a bound event track...
    GStateAssert(m_Animation->Binding != 0);
    granny_animation_binding* Binding = m_Animation->Binding;

    // Braindead linear search for right now...
    granny_int32x FakeLoopCount =
        (m_anim_sourceToken->ClampedLooping ? 1 : 0);

    granny_real32 LocalTime = ComputeLoopedTime(AtT,
                                                m_anim_sourceToken->Speed,
                                                m_LocalOffset, FakeLoopCount,
                                                Binding->ID.Animation->Duration);

    // Find the first spot
    granny_real32 const LocalDuration = Binding->ID.Animation->Duration/* / m_anim_sourceToken->Speed*/;
    granny_real32 Offset = int(LocalTime / LocalDuration) * LocalDuration;
    LocalTime -= Offset;
    while (LocalTime < 0.0f)
    {
        LocalTime += LocalDuration;
        Offset    -= LocalDuration;
    }
    if (!(LocalTime < LocalDuration))
    {
        // This can happen when the localtime is something super tiny negative like -1e-8.  It basically means that t = 0;
        LocalTime = 0;
        Offset   += LocalDuration;
    }
    GStateAssert(LocalTime >= 0.0f && LocalTime < LocalDuration);

    // Move this out of bounds...
    *PrevTime = AtT - 3*LocalDuration;

    // Double loop, just for simplicity
    // todo: do this in one loop, no need for this many strcmps...
    bool FoundNext = false;
    bool FoundPrev = false;
    {for (int Idx = 0; Idx < TextTrack->EntryCount; ++Idx)
    {
        granny_text_track_entry const& Entry = TextTrack->Entries[Idx];

        granny_real32 RealStamp = (Entry.TimeStamp + Offset - m_LocalOffset) / m_anim_sourceToken->Speed;

        if (RealStamp >= AtT)
        {
            if (!FoundNext && strcmp(Entry.Text, TextToFind) == 0)
            {
                *NextTime = RealStamp;
                FoundNext = true;
            }
        }

        if (RealStamp < AtT && RealStamp > *PrevTime)
        {
            if (strcmp(Entry.Text, TextToFind) == 0)
            {
                *PrevTime = RealStamp;
                FoundPrev = true;
            }
        }
    }}

    // Bail if we have both...
    if (FoundNext && FoundPrev)
        return true;

    {for (int Idx = 0; Idx < TextTrack->EntryCount; ++Idx)
    {
        granny_text_track_entry const& Entry = TextTrack->Entries[Idx];

        granny_real32 RealNextStamp = (Entry.TimeStamp + Binding->ID.Animation->Duration + Offset - m_LocalOffset) / m_anim_sourceToken->Speed;
        granny_real32 RealPrevStamp = (Entry.TimeStamp - Binding->ID.Animation->Duration + Offset - m_LocalOffset) / m_anim_sourceToken->Speed;

        if (RealNextStamp >= AtT)
        {
            if (!FoundNext && strcmp(Entry.Text, TextToFind) == 0)
            {
                *NextTime = RealNextStamp;
                FoundNext = true;
            }
        }

        if (RealPrevStamp < AtT && RealPrevStamp > *PrevTime)
        {
            if (strcmp(Entry.Text, TextToFind) == 0)
            {
                *PrevTime = RealPrevStamp;
                FoundPrev = true;
            }
        }
    }}

    GStateAssert(FoundNext == FoundPrev);
    GStateAssert(FoundNext == false ||
                 (*PrevTime < AtT && *NextTime >= AtT));
    return FoundNext;
}


bool GSTATE
anim_source::GetRootMotionVectors(granny_int32x  OutputIdx,
                                  granny_real32  AtT,
                                  granny_real32  DeltaT,
                                  granny_real32* ResultTranslation,
                                  granny_real32* ResultRotation,
                                  bool           Inverse)
{
    GStateAssert(OutputIdx == 0);
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

    // Unlike the controlled animation version of this, we /do/ need to ensure that the
    // vectors are zero filled before we start
    memset(ResultTranslation, 0, sizeof(granny_real32) * 3);
    memset(ResultRotation,    0, sizeof(granny_real32) * 3);

    GStateAssert(GetSourceModelForCharacter(GetBoundCharacter()));
    GStateAssert(GetSourceModelForCharacter(GetBoundCharacter())->Skeleton);

    if (m_Animation->Binding == 0 ||
        m_Animation->AccumulationMode == GrannyNoAccumulation)
    {
        // Nothing to accumulate here...
        return true;
    }

    granny_animation_binding* Binding = m_Animation->Binding;
    granny_real32 const LocalDuration = Binding->ID.Animation->Duration;

    granny_int32x FakeLoopCount =
        (m_anim_sourceToken->ClampedLooping ? 1 : 0);

    granny_real32 LocalEndTime = ComputeLoopedTime(AtT,
                                                   m_anim_sourceToken->Speed,
                                                   m_LocalOffset, FakeLoopCount,
                                                   LocalDuration);
    granny_real32 LocalStartTime = ComputeLoopedTime(AtT - DeltaT,
                                                     m_anim_sourceToken->Speed,
                                                     m_LocalOffset, FakeLoopCount,
                                                     LocalDuration);

    bool   FlipResult = false;
    if (LocalStartTime > LocalEndTime)
    {
        granny_real32 Temp    = LocalEndTime;
        LocalEndTime   = LocalStartTime;
        LocalStartTime = Temp;
        FlipResult = true;
    }

    granny_bound_transform_track* BoundTrack = Binding->TrackBindings;
    if(BoundTrack == 0)
        return false;

    granny_transform_track const* SourceTrack = BoundTrack->SourceTrack;
    if(SourceTrack == 0)
        return false;

    granny_real32 IgnoreTotalWeight = 0.0f;
    GrannyAccumulateControlledAnimationMotionVectors(m_Animation,
                                                     LocalStartTime, LocalEndTime, LocalDuration,
                                                     1.0f, FlipResult,
                                                     BoundTrack, SourceTrack,
                                                     &IgnoreTotalWeight,
                                                     (granny_real32*)ResultTranslation,
                                                     (granny_real32*)ResultRotation);
    return true;
}


void GSTATE
anim_source::Activate(granny_int32x OnOutput, granny_real32 AtT)
{
    parent::Activate(OnOutput, AtT);

    granny_animation_binding* Binding = m_Animation->Binding;
    if (Binding == 0)
        return;

    // Capture this for looping calcs...
    granny_real32 Speed       = m_anim_sourceToken->Speed;
    granny_real32 ImpliedTime = (AtT * Speed);
    m_LocalOffset = -ImpliedTime;
}

void GSTATE
anim_source::Synchronize(granny_int32x OnOutput,
                         granny_real32 AtT,
                         granny_real32 ReferenceStart,
                         granny_real32 ReferenceEnd,
                         granny_real32 LocalStart,
                         granny_real32 LocalEnd)
{
    GStateAssert(OnOutput == 0);

    granny_animation_binding* Binding = m_Animation->Binding;
    if (Binding == 0)
        return;

    GStateAssert(ReferenceStart <= AtT);
    GStateAssert(ReferenceEnd   >= AtT);
    GStateAssert(LocalStart     <= AtT);
    GStateAssert(LocalEnd       >= AtT);

    granny_real32 const RefDuration   = ReferenceEnd - ReferenceStart;
    granny_real32 const LocalDuration = LocalEnd - LocalStart;
    GStateAssert(RefDuration > 0);   // implied by above, but check anyways
    GStateAssert(LocalDuration > 0);

    granny_real32 const Frac = (AtT - ReferenceStart) / RefDuration;

    // Graphically, we have something like this:  (' = Local)
    //
    //       s          t       e
    //    s'            t e'
    //
    // where of course e' and s' might be in different positions w.r.t. s and e.  We need
    // to adjust the local offset so that the s' and e' fraction after adjustment exactly
    // matches the fraction from the reference.

    granny_real32 Adjustment = AtT - (LocalStart + (Frac * (LocalDuration)));
    m_LocalOffset -= Adjustment * m_anim_sourceToken->Speed;
}


bool GSTATE
anim_source::DidLoopOccur(granny_int32x OnOutput,
                          granny_real32 AtT,
                          granny_real32 DeltaT)
{
    GStateAssert(OnOutput >= 0 && OnOutput < GetNumOutputs());

    if (m_Animation->Binding == 0)
        return false;

    granny_animation_binding* Binding = m_Animation->Binding;
    granny_real32 Duration = Binding->ID.Animation->Duration;
    granny_real32 Speed = m_anim_sourceToken->Speed;

    // note the lack of clamping here...
    granny_real32 LocalStartTime = ComputeRawTime(AtT - DeltaT, Speed, m_LocalOffset, 0, Duration);
    granny_real32 LocalEndTime   = ComputeRawTime(AtT,          Speed, m_LocalOffset, 0, Duration);

    // todo: This is an annoying way to avoid float imprec.
    LocalStartTime -= fmod(LocalStartTime, Duration);
    LocalEndTime   -= fmod(LocalEndTime,   Duration);

    if ((LocalEndTime - LocalStartTime) > Duration/2)
        return true;

    return false;
}

void GSTATE
anim_source::NoteAnimationSlotDeleted(animation_slot* Slot)
{
    GStateAssert(Slot != 0);

    parent::NoteAnimationSlotDeleted(Slot);

    if (m_anim_sourceToken->AnimationSlot == Slot)
    {
        TakeTokenOwnership();

        m_anim_sourceToken->AnimationSlot = 0;
    }
}


CREATE_SNAPSHOT(anim_source)
{
    CREATE_WRITE_REAL32(m_LocalOffset);
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(anim_source)
{
    RESET_OFFSET_TRACKING();
    RESET_READ_REAL32(m_LocalOffset);
    RESET_PASS_TO_PARENT();
}


