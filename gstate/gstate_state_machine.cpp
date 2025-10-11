// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_state_machine.cpp $
// $DateTime: 2012/10/26 17:02:46 $
// $Change: 40087 $
// $Revision: #17 $
//
// $Notice: $
// ========================================================================
#include "gstate_state_machine.h"

#include "gstate_anim_utils.h"
#include "gstate_blend_graph.h"
#include "gstate_character_instance.h"
#include "gstate_conditional.h"
#include "gstate_event_source.h"
#include "gstate_node_visitor.h"
#include "gstate_parameters.h"
#include "gstate_quick_vecs.h"
#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"
#include "gstate_transition.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct edge_map
{
    granny_int32  EntryCount;
    granny_int32* Entries;
};
granny_data_type_definition EdgeMapType[] =
{
    { GrannyReferenceToArrayMember, "Entries", GrannyInt32Type },
    { GrannyEndMember }
};

struct GSTATE state_machine::state_machineImpl
{
    granny_int32 StartState;

    // One of these for each output of the state machine to relate them to the child node
    // outputs...
    granny_int32 OutputMapCount;
    edge_map*    OutputMaps;

    granny_int32    ConditionalCount;
    granny_variant* Conditionals;
};

granny_data_type_definition GSTATE
state_machine::state_machineImplType[] =
{
    { GrannyInt32Member,            "StartState" },
    { GrannyReferenceToArrayMember, "OutputMaps", EdgeMapType },

    { GrannyReferenceToArrayMember, "Conditionals", GrannyVariantType },

    { GrannyEndMember }
};

// state_machine is a concrete class, so we must create a slotted container
struct state_machine_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_OPAQUE_TOKEN_SLOT(container);
    DECL_TOKEN_SLOT(state_machine);
};

granny_data_type_definition state_machine::state_machineTokenType[] =
{
    DECL_UID_MEMBER(state_machine),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(container),
    DECL_TOKEN_MEMBER(state_machine),

    { GrannyEndMember }
};

bool GSTATE
state_machine::IsStateNode(node* Node)
{
    return ((GSTATE_DYNCAST(Node, parameters) == 0) &&
            (GSTATE_DYNCAST(Node, event_source) == 0));
}

static bool
IsParameterNode(node* Node)
{
    return !state_machine::IsStateNode(Node);
}

static edge_map&
EdgeMapForRawOutput(state_machine* Machine,
                    state_machine::state_machineImpl* Token,
                    granny_int32x RawOutputIndex)
{
    granny_int32x ExtOutput = Machine->WhichExternalOutput(RawOutputIndex);
    GStateAssert(ExtOutput != -1);
    GStateAssert(GS_InRange(ExtOutput, Token->OutputMapCount));

    return Token->OutputMaps[ExtOutput];
}


static granny_int32x
SampleIndexForNode(state_machine* Machine,
                   state_machine::state_machineImpl* Token,
                   node* Node,
                   granny_int32x RawOutputIdx)
{
    // Special case the pose output...
    if (RawOutputIdx == 0)
        return 0;

    granny_int32x ExtOutput   = Machine->WhichExternalOutput(RawOutputIdx);
    granny_int32x ExternalIdx = Node->GetNthExternalOutput(ExtOutput);

#if defined(DEBUG) && DEBUG
    edge_map& EdgeMap = EdgeMapForRawOutput(Machine, Token, RawOutputIdx);
    granny_int32x CachedIdx = EdgeMap.Entries[Machine->GetChildIdx(Node)];

    // No mismatches allowed!
    GStateAssert(CachedIdx == ExternalIdx);
#endif

    return ExternalIdx;
}

static granny_int32x
SampleIndexForTransition(state_machine* Machine,
                         state_machine::state_machineImpl* Token,
                         granny_int32x RawOutputIdx)
{
    return Machine->WhichExternalOutput(RawOutputIdx);
}



void GSTATE
state_machine::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(state_machine);

    GStateCloneArray(m_state_machineToken->OutputMaps,
                     OldToken->OutputMaps,
                     m_state_machineToken->OutputMapCount);
    {for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
    {
        GStateCloneArray(m_state_machineToken->OutputMaps[Idx].Entries,
                         OldToken->OutputMaps[Idx].Entries,
                         m_state_machineToken->OutputMaps[Idx].EntryCount);
    }}

    GStateCloneArray(m_state_machineToken->Conditionals,
                     OldToken->Conditionals,
                     m_state_machineToken->ConditionalCount);
}

void GSTATE
state_machine::ReleaseOwnedToken_state_machine()
{
    {for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
    {
        GStateDeallocate(m_state_machineToken->OutputMaps[Idx].Entries);
    }}

    GStateDeallocate(m_state_machineToken->OutputMaps);
    GStateDeallocate(m_state_machineToken->Conditionals);
}

IMPL_CREATE_DEFAULT(state_machine);

void GSTATE
state_machine::DefaultStartState()
{
    TakeTokenOwnership();

    // All equally goofy, pick the non parameter node if possible, mark invalid if
    // not.
    m_state_machineToken->StartState = eInvalidChild;
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        if (IsStateNode(GetChild(Idx)))
        {
            m_state_machineToken->StartState = Idx;
            break;
        }
    }
}

GSTATE
state_machine::state_machine(token_context*               Context,
                             granny_data_type_definition* TokenType,
                             void*                        TokenObject,
                             token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_state_machineToken(0),
    m_Active(0),
    m_Conditionals(0)
{
    // @@ compliated enough that this should be it's own function...
    IMPL_INIT_FROM_TOKEN(state_machine);
    {
        // Init our conditional cache...
        if (m_state_machineToken->ConditionalCount)
            GStateAllocZeroedArray(m_Conditionals, m_state_machineToken->ConditionalCount);

        // Now, create all of the conditionals.  Note that like
        // container::container we have to handle deleted node types.  See the
        // comment in that function for more info.
        {for (int Idx = 0; Idx < m_state_machineToken->ConditionalCount; /*++Idx*/)
        {
            tokenized* Product =
                GetTokenContext()->CreateFromToken(m_state_machineToken->Conditionals[Idx].Type,
                                                   m_state_machineToken->Conditionals[Idx].Object);
            if (Product != 0)
            {
                m_Conditionals[Idx] = GSTATE_DYNCAST(Product, conditional);
                GStateAssert(m_Conditionals[Idx]);
                GStateAssert(m_Conditionals[Idx]->GetOwner() == this);

                // All of the nodes referred to should be valid at this point...
                m_Conditionals[Idx]->CaptureNodeLinks();

                // Advance
                ++Idx;
            }
            else
            {
                GStateWarning("state_machine::state_machine: Found an invalid conditional, possibly obsolete class?\n");
                if ((Idx + 1) < m_state_machineToken->ConditionalCount)
                {
                    // Shift the rest of the array down
                    memmove(m_state_machineToken->Conditionals + Idx,
                            m_state_machineToken->Conditionals + (Idx+1),
                            (sizeof(*m_state_machineToken->Conditionals) *
                             (m_state_machineToken->ConditionalCount - (Idx+1))));
                }

                --m_state_machineToken->ConditionalCount;

                // This can cause ChildTokenCount to go to zero, if that occurs, NULL the
                // array pointer, we have no children.  Do NOT free.
                if (m_state_machineToken->ConditionalCount == 0)
                {
                    m_state_machineToken->Conditionals = 0;
                    GStateDeallocate(m_Conditionals);
                }
            }
        }}

        LinkContainerChildren();
        {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);
            GStateAssert(Child);

            Child->CaptureSiblingData();
        }}

        // Note that there is a rare case in the container ctor in which tokens might not
        // be properly created, causing StartState to be out of range.  Detect and correct
        // here.  Subtlety: don't test against zero, because eInvalidChild (== -1) is a
        // valid entry for this field.
        if (m_state_machineToken->StartState >= GetNumChildren())
        {
            GStateWarning("state_machine::state_machine: StartState invalid, resetting to valid entry\n");
            DefaultStartState();
        }
    }

    // @@ don't let this get lost!
    if (EditorCreated())
    {
        AddOutput(PoseEdge, "Pose Output");
    }

    // Special case for upgrading old state_machines.  Remove this after a little bit.
    if (m_state_machineToken->OutputMapCount == 0)
    {
        // We can only do this in editable contexts.  The good news is that in
        // non-editable contexts, this shouldn't cause problems for sampling.
        if (IsEditable())
        {
            // Only this config is valid for upgrades...
            GStateAssert(GetNumOutputs() == 1 && GetOutputType(0) == PoseEdge);

            TakeTokenOwnership();

            edge_map& LastMap = QVecPushNewElement(m_state_machineToken->OutputMapCount,
                                                   m_state_machineToken->OutputMaps);
            LastMap.EntryCount = GetNumChildren();
            LastMap.Entries    = LastMap.EntryCount ? GStateAllocArray(granny_int32, LastMap.EntryCount) : 0;

            {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
            {
                node* Child = GetChild(Idx);
                LastMap.Entries[Idx] = Child->GetNthExternalOutput(0);
            }}
        }
        else
        {
            GStateWarning("Detected an old-style state_machine Token(%s) use the studio to upgrade", GetName());
        }
    }
}


GSTATE
state_machine::~state_machine()
{
    // Delete our conditionals...
    {for (int Idx = 0; Idx < m_state_machineToken->ConditionalCount; ++ Idx)
    {
        GStateDelete<conditional>(m_Conditionals[Idx]);
        m_Conditionals[Idx] = 0;
    }}
    GStateDeallocate(m_Conditionals);

    DTOR_RELEASE_TOKEN(state_machine);
}


bool GSTATE
state_machine::FillDefaultToken(granny_data_type_definition* TokenType,
                                    void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares state_machineImpl*& Slot = // member
    GET_TOKEN_SLOT(state_machine);

    // Our slot in this token should be empty.
    // Create a new state_machine token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    // Initialize to default (note that by default, we have no states)
    Slot->StartState = eInvalidChild;
    Slot->OutputMapCount = 0;
    Slot->OutputMaps = 0;
    Slot->ConditionalCount = 0;
    Slot->Conditionals = 0;

    return true;
}

bool GSTATE
state_machine::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);
        GStateAssert(Child);

        Child->CaptureSiblingData();
    }}

    return true;
}

void GSTATE
state_machine::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}


void GSTATE
state_machine::NoteParameterNameChange(node* Param, int OutputIdx)
{
    GStateAssert(Param);
    GStateAssert(IsParameterNode(Param));
    GStateAssert(OutputIdx >= 0 && OutputIdx < Param->GetNumOutputs());

    char const* NewName = Param->GetOutputName(OutputIdx);

    // This one is actually really easy too.  Loop through, find connections, setinputname.  Done!
    {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);

        {for (int InputIndex = Child->GetNumInputs() - 1; InputIndex >= 0; --InputIndex)
        {
            if (Child->IsInputInternal(InputIndex))
                continue;

            node* OtherNode;
            granny_int32x OtherIdx;
            Child->GetInputConnection(InputIndex, &OtherNode, &OtherIdx);
            if (OtherNode == Param && OutputIdx == OtherIdx)
            {
                Child->SetInputName(InputIndex, NewName);
            }
        }}
    }}
}

bool GSTATE
state_machine::AddOutputToChildren(int NewOutputIdx)
{
    TakeTokenOwnership();

    edge_map& LastMap = QVecPushNewElement(m_state_machineToken->OutputMapCount,
                                           m_state_machineToken->OutputMaps);
    LastMap.EntryCount = GetNumChildren();
    LastMap.Entries = LastMap.EntryCount ? GStateAllocArray(granny_int32, LastMap.EntryCount) : 0;

    // We have to special case the 0th input, since pretty much everything pops out of
    // it's box with a pose output as it's 0th external, and we don't want to re-add it

    if (NewOutputIdx == 0)
    {
        GStateAssert(GetOutputType(NewOutputIdx) == PoseEdge);

        {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);
            LastMap.Entries[Idx] = Child->GetNthExternalOutput(0);
        }}
    }
    else
    {
        char const* Name = GetOutputName(NewOutputIdx);
        node_edge_type EdgeType = GetOutputType(NewOutputIdx);
        GStateAssert(EdgeType == ScalarEdge || EdgeType == EventEdge || EdgeType == MorphEdge);

        {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);

            // Do *not* add outputs to parameter nodes, they handle their own stuff, and
            // we won't be (directly) querying them for scalar/event output
            if (IsParameterNode(Child))
            {
                LastMap.Entries[Idx] = -1;
                continue;
            }

            LastMap.Entries[Idx] = Child->AddOutput(EdgeType, Name);
        }}
    }

    return true;
}

void GSTATE
state_machine::RefreshChildOutputName(granny_int32x OutputIdx)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    char const* OutputName = GetOutputName(OutputIdx);

    if (IsOutputInternal(OutputIdx))
    {
        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);
            for (int InputIdx = 0; InputIdx < Child->GetNumInputs(); InputIdx++)
            {
                INPUT_CONNECTION_ON(Child, InputIdx, Test);
                if (TestNode == this && TestEdge == OutputIdx)
                    Child->SetInputName(InputIdx, OutputName);
            }
        }
    }
    else
    {
        edge_map& EdgeMap = EdgeMapForRawOutput(this, m_state_machineToken, OutputIdx);

        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);

            if (IsParameterNode(Child))
            {
                GStateAssert(EdgeMap.Entries[Idx] == -1);
            }
            else
            {
                GStateAssert(GS_InRange(EdgeMap.Entries[Idx], Child->GetNumOutputs()));
                Child->SetOutputName(EdgeMap.Entries[Idx], OutputName);
            }
        }
    }
}



bool GSTATE
state_machine::RemoveOutputFromChildren(int DelOutputIndex)
{
    TakeTokenOwnership();

    if (IsOutputInternal(DelOutputIndex))
        return true;

    edge_map OldMap;
    {
        // Normally would use the EdgeMapForRawOutput, but we need that index in this case...
        granny_int32x ExtOutput = WhichExternalOutput(DelOutputIndex);
        GStateAssert(ExtOutput != -1 && ExtOutput != 0);
        GStateAssert(GS_InRange(ExtOutput-1, m_state_machineToken->OutputMapCount));

        OldMap = m_state_machineToken->OutputMaps[ExtOutput];
        QVecRemoveElement(ExtOutput,
                          m_state_machineToken->OutputMapCount,
                          m_state_machineToken->OutputMaps);
    }

    // We're going to make the following assumption.  When we remove an output from a
    // Child, all of its outputs *above* that index shift down by one.  This is true as of
    // right now, but it's very difficult to verify in general.  Note that
    // container::NoteOutputRemoval depends on this assumption as well, so there are
    // larger problems if that is ever not true.  It will get noticed.
    GStateAssert(GetNumChildren() == OldMap.EntryCount);
    {for (int Idx = 0; Idx < OldMap.EntryCount; ++Idx)
    {
        node* Child = GetChild(Idx);
        granny_int32x ChildOutputIdx = OldMap.Entries[Idx];

        // for whatever reason...
        if (ChildOutputIdx == -1)
            continue;

        GStateAssert(Child->IsOutputExternal(ChildOutputIdx));
        GStateAssert(Child->GetOutputType(ChildOutputIdx) == GetOutputType(DelOutputIndex));
        GStateAssert(strcmp(Child->GetOutputName(ChildOutputIdx), GetOutputName(DelOutputIndex)) == 0);

        // Delete that output from the child
        Child->DeleteOutput(ChildOutputIdx);

        // That will have cascaded through NoteOutputRemoval, all sibs are updated
        // already.

        // Now adjust the remaining OutputMaps...
        {for (int MapIdx = 0; MapIdx < m_state_machineToken->OutputMapCount; ++MapIdx)
        {
            edge_map& ThisMap = m_state_machineToken->OutputMaps[MapIdx];
            GStateAssert(ThisMap.EntryCount == GetNumChildren());

            //GStateAssert(ThisMap.Entries[Idx] != ChildOutputIdx);  // that would be bad, of course...

            if (ThisMap.Entries[Idx] > ChildOutputIdx)
                --ThisMap.Entries[Idx];
        }}
    }}

    // Free the arrays for OutputMap
    GStateDeallocate(OldMap.Entries);
    OldMap.EntryCount = -1;

    return true;
}


void GSTATE
state_machine::RemoveOutputFromConditionals(node* AffectedNode, int DelOutputIdx)
{
    // Substantially easier than the RemoveFromChildren method...

    // Shouldn't ever remove the pose output...
    GStateAssert(DelOutputIdx > 0 && DelOutputIdx < GetNumOutputs());

    {for (int Idx = 0; Idx < GetNumConditionals(); ++Idx)
    {
        conditional* Conditional = GetConditional(Idx);
        Conditional->Note_OutputEdgeDelete(AffectedNode, DelOutputIdx);
    }}
}

void GSTATE
state_machine::NoteOutputRemoval_Pre(node* AffectedNode, granny_int32x ToBeRemoved)
{
    TakeTokenOwnership();

    // We have to give any of our conditionals a change to see this change...
    {for (int Idx = 0; Idx < m_state_machineToken->ConditionalCount; ++Idx)
    {
        m_Conditionals[Idx]->Note_OutputEdgeDelete(AffectedNode, ToBeRemoved);
    }}

    // Adjust our output maps for this node...
    {
        int ChildIdx = GetChildIdx(AffectedNode);
        GStateAssert(ChildIdx != eInvalidChild);

        // Adjust the output maps...
        for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
        {
            edge_map& OutputMap = m_state_machineToken->OutputMaps[Idx];
            GStateAssert(OutputMap.EntryCount == GetNumChildren());

            if (OutputMap.Entries[ChildIdx] == ToBeRemoved)
                OutputMap.Entries[ChildIdx] = -1;
            else if (OutputMap.Entries[ChildIdx] > ToBeRemoved)
                --OutputMap.Entries[ChildIdx];
        }
    }

    parent::NoteOutputRemoval_Pre(AffectedNode, ToBeRemoved);
}

void GSTATE
state_machine::NoteOutputRemoval_Post(node* AffectedNode, bool WasExternal)
{
    parent::NoteOutputRemoval_Post(AffectedNode, WasExternal);

    if (WasExternal)
        AdjustChildInputs();
}

void GSTATE
state_machine::NoteOutputAddition(node* AffectedNode, granny_int32x InsertionIndex)
{
    parent::NoteOutputAddition(AffectedNode, InsertionIndex);

    if (IsParameterNode(AffectedNode))
        AdjustChildInputs();
}

void GSTATE
state_machine::NoteDeleteTransition(transition* Transition)
{
    if (!Transition)
        return;

    // if not, don't care.
    if (m_Active != Transition)
        return;

    // Just force this into the end state...
    ForceState(0, Transition->GetEndNode());
}

int GSTATE
state_machine::AddChild(node* Child)
{
    TakeTokenOwnership();

    int NewIndex = parent::AddChild(Child);

    // Modify the output edge maps...
    GStateAssert(NewIndex == GetNumChildren() - 1);  // other cases not yet handled.
    {for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
    {
        granny_int32& NewEntry = QVecPushNewElement(m_state_machineToken->OutputMaps[Idx].EntryCount,
                                                    m_state_machineToken->OutputMaps[Idx].Entries);
        NewEntry = -1;
    }}

    if (IsParameterNode(Child))
    {
        AdjustChildInputs();

        // It doesn't alter the output map, the entry is set to -1 already, and we don't
        // add outputs to the parameter based on the state machine output.
    }
    else
    {
        // Add any parameters to this child
        blend_graph*   AsGraph   = GSTATE_DYNCAST(Child, blend_graph);
        state_machine* AsMachine = GSTATE_DYNCAST(Child, state_machine);
        if (AsGraph != 0 || AsMachine != 0)
        {
            AdjustChildInputs();
#if 0
            {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
            {
                node* MyChild = GetChild(Idx);
                if (IsParameterNode(MyChild) == false)
                    continue;

                // Wire it!
                {for (int OutputIdx = 0; OutputIdx < MyChild->GetNumOutputs(); ++OutputIdx)
                {
                    if (MyChild->IsOutputExternal(OutputIdx) == false)
                        continue;

                    granny_int32x NewIdx = Child->AddInput(ScalarEdge, MyChild->GetOutputName(OutputIdx));
                    Child->SetInputConnection(NewIdx, MyChild, OutputIdx);
                }}
            }}
#endif
        }

        // Add output edges other than the first pose.  This also alters the output map, of course...
        {
            {for (int Idx = 0; Idx < GetNumOutputs(); ++Idx)
            {
                if (Idx == 0)
                    continue;  // skip that first pose edge, arg.

                // And any internal outputs...
                if (IsOutputInternal(Idx))
                    continue;

                edge_map& EdgeMap = EdgeMapForRawOutput(this, m_state_machineToken, Idx);
                EdgeMap.Entries[NewIndex] = Child->AddOutput(GetOutputType(Idx), GetOutputName(Idx));
            }}
        }

        // Normal state...
        if (NewIndex != eInvalidChild && m_state_machineToken->StartState == eInvalidChild)
        {
            TakeTokenOwnership();

            m_state_machineToken->StartState = NewIndex;

            // We can also just make this the active element...
            m_Active = Child;
        }
    }

    return NewIndex;
}

bool GSTATE
state_machine::RemoveChildByIdx(int ChildIdx)
{
    TakeTokenOwnership();

    // We need to get this by pointer before removing to check against active state
    node* RemChild = GetChild(ChildIdx);

    // Further, if this is a parameter, we need to nuke the connected inputs of our children
    if (IsStateNode(RemChild))
    {
        // This is *technically* not strictly necessary, since removing the child right
        // now *only* means deleting it, but in case we add copy/paste later, we might be
        // stuffing this child back in somewhere *else*.  So be a good citizen.  Note that
        // normally we would have to jump through numerous hoops, but since we know that
        // we'll be peeling this down to just a pose output, this is easy.
        bool Removed;
        do
        {
            Removed = false;
            {for (int Idx = 0; Idx < RemChild->GetNumOutputs(); ++Idx)
            {
                if (Idx == RemChild->GetNthExternalOutput(0) || RemChild->IsOutputInternal(Idx))
                    continue;

                Removed = true;
                RemChild->DeleteOutput(Idx);
                break;
            }}
        } while (Removed);

        // Delete any parameter edges on this node
        {for (int Idx = RemChild->GetNumInputs() - 1; Idx >= 0; --Idx)
        {
            if (RemChild->IsInputInternal(Idx))
                continue;

            node* OtherNode;
            granny_int32x OtherIdx;
            RemChild->GetInputConnection(Idx, &OtherNode, &OtherIdx);
            if (IsParameterNode(OtherNode))
                RemChild->DeleteInput(Idx);
        }}
    }
    else
    {
        // It's a parameter or event node, let the conditionals know that it's going
        // away...
        for (int Idx = 0; Idx < GetNumConditionals(); ++Idx)
        {
            conditional* Conditional = GetConditional(Idx);
            Conditional->Note_NodeDelete(RemChild);
        }
    }

    // Remove the child from our output maps...
    {for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
    {
        edge_map& Map = m_state_machineToken->OutputMaps[Idx];
        QVecRemoveElement(ChildIdx, Map.EntryCount, Map.Entries);
    }}

    bool RemoveSuccess = parent::RemoveChildByIdx(ChildIdx);

    // Make sure that we don't have to remove any inputs from our children
    AdjustChildInputs();

    // Either the start state went away, or the array will shift down, and we have to
    // account for that.
    if (m_state_machineToken->StartState == ChildIdx)
    {
        // Totally gone.  Find the first one that isn't a parameter node
        DefaultStartState();
    }
    else if (m_state_machineToken->StartState > ChildIdx)
    {
        // Just shift down...
        GStateAssert(GetNumChildren() != 0);
        m_state_machineToken->StartState -= 1;
    }

    if (m_Active == RemChild)
    {
        if (m_state_machineToken->StartState != eInvalidChild)
            m_Active = GetChild(m_state_machineToken->StartState);
        else
            m_Active = 0;
    }

    return RemoveSuccess;
}


int GSTATE
state_machine::GetStartStateIdx() const
{
    return m_state_machineToken->StartState;
}


void GSTATE
state_machine::SetStartStateIdx(int StartState)
{
    GStateAssert(StartState == eInvalidChild ||
                 (StartState >= 0 && StartState < GetNumChildren()));

    TakeTokenOwnership();
    m_state_machineToken->StartState = StartState;
}

void GSTATE
state_machine::SetStartState(node* State)
{
    if (IsStateNode(State) == false)
    {
        GS_InvalidCodePath("tried to set start state to a non-State class");
        return;
    }

    int ChildIdx = GetChildIdx(State);
    GStateAssert(ChildIdx != eInvalidChild);
    SetStartStateIdx(ChildIdx);
}


tokenized* GSTATE
state_machine::GetActiveElement()
{
    return m_Active;
}

void GSTATE
state_machine::CheckConnections()
{
    parent::CheckConnections();

    // Enforce the output map constraints
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);
        if (IsParameterNode(Child))
            continue;

        for (int OutputIdx = 1; OutputIdx < GetNumOutputs(); ++OutputIdx)
        {
            if (IsOutputInternal(OutputIdx))
                continue;

            granny_int32x ChildOutputIdx = SampleIndexForNode(this, m_state_machineToken, Child, OutputIdx);

            GStateAssert(Child->IsOutputExternal(ChildOutputIdx));
            GStateAssert(Child->GetOutputType(ChildOutputIdx) == GetOutputType(OutputIdx));
            GStateAssert(strcmp(Child->GetOutputName(ChildOutputIdx), GetOutputName(OutputIdx)) == 0);
            GStateUnused(ChildOutputIdx);
        }
    }
}

bool GSTATE
state_machine::MoveNodeToFront(node* Node)
{
    int ChildIdx = GetChildIdx(Node);
    GStateAssert(ChildIdx != eInvalidChild);

    TakeTokenOwnership();

    if (m_state_machineToken->StartState == ChildIdx)
    {
        m_state_machineToken->StartState = 0;
    }
    else if (m_state_machineToken->StartState != eInvalidChild &&
             m_state_machineToken->StartState < ChildIdx)
    {
        ++m_state_machineToken->StartState;
    }

    bool RetVal = parent::MoveNodeToFront(Node);

    // Adjust the output maps...
    for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
    {
        edge_map& OutputMap = m_state_machineToken->OutputMaps[Idx];
        GStateAssert(OutputMap.EntryCount == GetNumChildren());

        granny_int32 StoreOutput = OutputMap.Entries[ChildIdx];
        for (int Idx = ChildIdx; Idx >= 1; --Idx)
        {
            OutputMap.Entries[Idx] = OutputMap.Entries[Idx-1];
        }
        OutputMap.Entries[0] = StoreOutput;
    }

    return RetVal;
}


bool GSTATE
state_machine::RequestChangeTo(granny_real32 AtT,
                               granny_real32 DeltaT,
                               node*         State)
{
    if (m_Active == 0)
        return false;

    if (GSTATE_DYNCAST(m_Active, transition))
        return false;

    GStateAssert(GSTATE_DYNCAST(m_Active, node));
    node* ActiveNode = static_cast<node*>(m_Active);

    // Look for a transition we can use...
    {for (int Pass = 0; Pass < 2; ++Pass)
    {
        {for (int Idx = 0; Idx < ActiveNode->GetNumTransitions(); ++Idx)
        {
            transition* Transition = ActiveNode->GetTransition(Idx);
            if (Transition->GetEndNode() == State)
            {
                if (Transition->ShouldActivate(Pass, Trigger_Requested, AtT, DeltaT))
                {
                    return StartTransition(AtT, Transition);
                }
            }
        }}
    }}

    return false;
}

bool GSTATE
state_machine::RequestChangeToState(granny_real32 AtT,
                                    granny_real32 DeltaT,
                                    char const* StateName)
{
    if (m_Active == 0)
        return false;

    if (GSTATE_DYNCAST(m_Active, transition))
        return false;

    GStateAssert(GSTATE_DYNCAST(m_Active, node));
    node* ActiveNode = static_cast<node*>(m_Active);

    // Look for a transition we can use...
    {for (int Pass = 0; Pass < 2; ++Pass)
    {
        {for (int Idx = 0; Idx < ActiveNode->GetNumTransitions(); ++Idx)
        {
            transition* Transition = ActiveNode->GetTransition(Idx);
            if (_stricmp(StateName, Transition->GetEndNode()->GetName()) == 0)
            {
                if (Transition->ShouldActivate(Pass, Trigger_Requested, AtT, DeltaT))
                {
                    return StartTransition(AtT, Transition);
                }
            }
        }}
    }}

    return false;
}

bool GSTATE
state_machine::ForceChangeToState(granny_real32 AtT, char const* StateName)
{
    if (m_Active == 0)
        return false;

    if (GSTATE_DYNCAST(m_Active, transition))
        return false;

    GStateAssert(GSTATE_DYNCAST(m_Active, node));
    node* ActiveNode = static_cast<node*>(m_Active);
    ActivateConditionals(AtT);

    // Look for a transition we can use...
    {for (int Idx = 0; Idx < ActiveNode->GetNumTransitions(); ++Idx)
    {
        transition* Transition = ActiveNode->GetTransition(Idx);
        if (_stricmp(StateName, Transition->GetEndNode()->GetName()) == 0)
        {
            // No asking!
            return StartTransition(AtT, Transition);
        }
    }}

    // Otherwise, look for a node that matches the name, and just switch to it.
    {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);
        GStateAssert(Child);

        if (_stricmp(StateName, Child->GetName()) == 0)
            return ForceState(AtT, Child);
    }}

    return false;
}


bool GSTATE
state_machine::StartTransitionByName(granny_real32 AtT,
                                     char const* TransitionName)
{
    if (TransitionName == 0)
        return false;

    node* ActiveNode = GSTATE_DYNCAST(m_Active, node);
    if (ActiveNode == 0)
        return false;

    // Look for a transition we can use...
    {for (int Pass = 0; Pass < 2; ++Pass)
    {
        int const NumTransitions = ActiveNode->GetNumTransitions();
        {for (int Idx = 0; Idx < NumTransitions; ++Idx)
        {
            transition* Transition = ActiveNode->GetTransition(Idx);
            if (_stricmp(TransitionName, Transition->GetName()) == 0)
            {
                if (Transition->ShouldActivate(Pass, Trigger_Requested, AtT, 0.0f))
                {
                    return StartTransition(AtT, Transition);
                }
            }
        }}
    }}

    return false;
}

bool GSTATE
state_machine::StartTransition(granny_real32 AtT, transition* Transition)
{
    if (m_Active && Transition->GetStartNode() == m_Active)
    {
        m_Active = Transition;
        Transition->Activate(AtT);
        ActivateConditionals(AtT);
        return true;
    }

    return false;
}


bool GSTATE
state_machine::ForceStartTransition(granny_real32 AtT, transition* Transition)
{
    if (m_Active != Transition)
    {
        m_Active = Transition;
        Transition->Activate(AtT);
        ActivateConditionals(AtT);
    }

    return true;
}


bool GSTATE
state_machine::ForceState(granny_real32 AtT, node* State)
{
    GStateAssert(IsStateNode(State));

    // If there is currently an active transition, we need to forcibly deactivate it to
    // kill any superfluous resources
    transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
    if (AsTransition)
        AsTransition->Deactivate();

    if (State == 0)
    {
        // should we allow this?
        m_Active = 0;
        return true;
    }

    GStateAssert(State->GetParent() == this);
    GStateAssert(State->GetNumOutputs() > 0);

    // check that it's a pose? technically, we need to pair up our externals with the
    // externals of the states...
    GStateAssert(State->GetOutputType(State->GetNthExternalOutput(0)) == PoseEdge);

    m_Active = State;

    granny_int32x ExternalIdx = State->GetNthExternalOutput(0);
    State->Activate(ExternalIdx, AtT);
    ActivateConditionals(AtT);

    return true;
}

bool GSTATE
state_machine::ForceTransition(granny_real32 AtT, transition* Transition)
{
    // todo: code dupe!

    // If there is currently an active transition, we need to forcibly deactivate it to
    // kill any superfluous resources
    transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
    if (AsTransition)
        AsTransition->Deactivate();

    if (Transition == 0)
    {
        // should we allow this?
        m_Active = 0;
        return true;
    }

    GStateAssert(Transition->GetStartNode() && Transition->GetStartNode()->GetParent() == this);
    GStateAssert(Transition->GetEndNode() && Transition->GetEndNode()->GetParent() == this);

    m_Active = Transition;
    Transition->Activate(AtT);
    ActivateConditionals(AtT);

    return true;
}


void GSTATE
state_machine::AdvanceT(granny_real32 AtT, granny_real32 DeltaT)
{
    GStateAssert(DeltaT >= 0);

    node* Node = GSTATE_DYNCAST(m_Active, node);
    transition* Transition = GSTATE_DYNCAST(m_Active, transition);
    if (Node)
    {
        // Let's take a look at this nodes transitions, and see if we want to activate any
        // of them...
        transition* Activated = 0;
        {for (int Pass = 0; Pass < 2 && Activated == 0; ++Pass)
        {
            {for (int Idx = 0; Idx < Node->GetNumTransitions(); ++Idx)
            {
                transition* Possible = Node->GetTransition(Idx);
                if (Possible->ShouldActivate(Pass, Trigger_Automatic, AtT, DeltaT))
                {
                    //int External = Node->GetNthExternalOutput(0);
                    m_Active = Activated = Possible;
                    Possible->Activate(AtT);
                    ActivateConditionals(AtT);
                    break;
                }
            }}
        }}
    }
    else if (Transition)
    {
        Transition->AdvanceT(DeltaT);
        if (Transition->IsActive() == false)
        {
            m_Active = Transition->GetEndNode();
            ActivateConditionals(AtT);
            // m_Active->Activate(AtT);  // the end node is actually triggered by the transition itself...
        }
    }
    else
    {
        // Well, shoot.  There's nothing active.  Try the start state?
        if (m_state_machineToken->StartState != eInvalidChild)
        {
            node* State = GetChild(m_state_machineToken->StartState);
            m_Active = State;
            if (State)
            {
                int External = State->GetNthExternalOutput(0);
                State->Activate(External, AtT);
                ActivateConditionals(AtT);
            }
        }
    }

    parent::AdvanceT(AtT, DeltaT);
}

granny_real32 GSTATE
state_machine::SampleScalarOutput(granny_int32x OutputIdx,
                                  granny_real32 AtT)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    if (IsOutputExternal(OutputIdx))
    {
        if (m_Active != 0)
        {
            node* AsNode = GSTATE_DYNCAST(m_Active, node);
            if (AsNode)
            {
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
                return AsNode->SampleScalarOutput(SampleIdx, AtT);
            }
            else
            {
                transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                granny_int32x SampleIdx = SampleIndexForTransition(this, m_state_machineToken, OutputIdx);
                return AsTransition->SampleScalarOutput(SampleIdx, AtT);
            }
        }

        // No active element, that's bad.  AdvanceT covers this though, not us...
        return 0;
    }
    else
    {
        // Sample the external scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return 0;
        }

        return Node->SampleScalarOutput(EdgeIdx, AtT);
    }
}

granny_int32x GSTATE
state_machine::GetNumMorphChannels(granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);

    if (IsOutputExternal(OutputIdx))
    {
        if (m_Active != 0)
        {
            node* AsNode = GSTATE_DYNCAST(m_Active, node);
            if (AsNode)
            {
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
                return AsNode->GetNumMorphChannels(SampleIdx);
            }
            else
            {
                transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                granny_int32x SampleIdx = SampleIndexForTransition(this, m_state_machineToken, OutputIdx);
                return AsTransition->GetStartNode()->GetNumMorphChannels(SampleIdx);
            }
        }

        // No active element, that's bad.  AdvanceT covers this though, not us...
        return -1;
    }
    else
    {
        // Sample the internal scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return false;
        }

        return Node->GetNumMorphChannels(EdgeIdx);
    }
}

bool GSTATE
state_machine::SampleMorphOutput(granny_int32x  OutputIdx,
                                 granny_real32  AtT,
                                 granny_real32* MorphWeights,
                                 granny_int32x  NumMorphWeights)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    if (IsOutputExternal(OutputIdx))
    {
        if (m_Active != 0)
        {
            node* AsNode = GSTATE_DYNCAST(m_Active, node);
            if (AsNode)
            {
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
                return AsNode->SampleMorphOutput(SampleIdx, AtT, MorphWeights, NumMorphWeights);
            }
            else
            {
                transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                granny_int32x SampleIdx = SampleIndexForTransition(this, m_state_machineToken, OutputIdx);
                return AsTransition->SampleMorphOutput(SampleIdx, AtT, MorphWeights, NumMorphWeights);
            }
        }

        // No active element, that's bad.  AdvanceT covers this though, not us...
        return false;
    }
    else
    {
        // Sample the internal scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return false;
        }

        return Node->SampleMorphOutput(EdgeIdx, AtT, MorphWeights, NumMorphWeights);
    }
}

// bool GSTATE state_machine::GetScalarOutputRange()
//    state machine doesn't support range queries, since range queries must be static
//    across state changes.  Theoretically we could query every subnode, but... no.

granny_local_pose* GSTATE
state_machine::SamplePoseOutput(granny_int32x      OutputIdx,
                                granny_real32      AtT,
                                granny_real32      AllowedError,
                                granny_pose_cache* PoseCache)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(IsOutputExternal(OutputIdx));
    GStateAssert(OutputIdx == 0); // will have to handle this if this ever changes...
    GStateAssert(PoseCache);

    if (m_Active != 0)
    {
        node* AsNode = GSTATE_DYNCAST(m_Active, node);
        if (AsNode)
        {
            granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
            return AsNode->SamplePoseOutput(SampleIdx, AtT, AllowedError, PoseCache);
        }
        else
        {
            transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
            return AsTransition->SamplePose(AtT, AllowedError, PoseCache);
        }
    }

    // Return null, caller is resposible for building rest pose if necessary
    return 0;
}

bool GSTATE
state_machine::SampleMaskOutput(granny_int32x      OutputIdx,
                                granny_real32      AtT,
                                granny_track_mask* ModelMask)
{
    GS_InvalidCodePath("state machine has no mask output!");
    return 0;
}

bool GSTATE
state_machine::SampleEventOutput(granny_int32x            OutputIdx,
                                 granny_real32            AtT,
                                 granny_real32            DeltaT,
                                 granny_text_track_entry* EventBuffer,
                                 granny_int32x const      EventBufferSize,
                                 granny_int32x*           NumEvents)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    if (EventBuffer == 0 || EventBufferSize < 0 || NumEvents == 0)
    {
        GS_PreconditionFailed;
        return false;
    }

    if (IsOutputExternal(OutputIdx))
    {
        if (m_Active != 0)
        {
            node* AsNode = GSTATE_DYNCAST(m_Active, node);
            if (AsNode)
            {
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
                return AsNode->SampleEventOutput(SampleIdx, AtT, DeltaT,
                                                 EventBuffer, EventBufferSize, NumEvents);
            }
            else
            {
                transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                granny_int32x SampleIdx = SampleIndexForTransition(this, m_state_machineToken, OutputIdx);
                return AsTransition->SampleEventOutput(SampleIdx, AtT, DeltaT,
                                                       EventBuffer, EventBufferSize, NumEvents);
            }
        }

        return false;
    }
    else
    {
        // Sample the external scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return 0;
        }

        return Node->SampleEventOutput(EdgeIdx, AtT, DeltaT,
                                       EventBuffer, EventBufferSize, NumEvents);
    }
}

bool GSTATE
state_machine::GetAllEvents(granny_int32x            OutputIdx,
                            granny_text_track_entry* EventBuffer,
                            granny_int32x const      EventBufferSize,
                            granny_int32x*           NumEvents)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    // This one is a bit tricky, since we have multiple sub-nodes that all export event
    // tracks.  We have to sample them and combine the results into the single output
    // buffer.

    // @@todo need to look at the transitions?

    if (IsOutputExternal(OutputIdx))
    {
        int EventPos = 0;
        {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);

            // Ignore parameter nodes
            if (IsParameterNode(Child))
                continue;

            granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, Child, OutputIdx);

            granny_int32x UsedByChild;
            if (Child->GetAllEvents(SampleIdx,
                                    EventBuffer + EventPos,
                                    EventBufferSize - EventPos,
                                    &UsedByChild) == false)
            {
                return false;
            }

            EventPos = FilterDuplicateEvents(EventBuffer, EventPos, EventPos + UsedByChild);
        }}

        *NumEvents = EventPos;
        return true;
    }
    else
    {
        // Sample the external scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return false;
        }

        return Node->GetAllEvents(EdgeIdx,
                                  EventBuffer,
                                  EventBufferSize,
                                  NumEvents);
    }
}

bool GSTATE
state_machine::GetCloseEventTimes(granny_int32x  OutputIdx,
                                  granny_real32  AtT,
                                  char const*    TextToFind,
                                  granny_real32* PreviousTime,
                                  granny_real32* NextTime)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    if (IsOutputExternal(OutputIdx))
    {
        if (m_Active != 0)
        {
            node* AsNode = GSTATE_DYNCAST(m_Active, node);
            if (AsNode)
            {
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
                return AsNode->GetCloseEventTimes(SampleIdx, AtT, TextToFind, PreviousTime, NextTime);
            }
            else
            {
                transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                granny_int32x SampleIdx = SampleIndexForTransition(this, m_state_machineToken, OutputIdx);
                return AsTransition->GetCloseEventTimes(SampleIdx, AtT, TextToFind, PreviousTime, NextTime);
            }
        }

        // No active element, that's kinda bad.  AdvanceT covers this though, not us...
        return false;
    }
    else
    {
        // Sample the external scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return false;
        }

        return Node->GetCloseEventTimes(EdgeIdx, AtT, TextToFind, PreviousTime, NextTime);
    }
}


bool GSTATE
state_machine::GetRootMotionVectors(granny_int32x  OutputIdx,
                                    granny_real32  AtT,
                                    granny_real32  DeltaT,
                                    granny_real32* Translation,
                                    granny_real32* Rotation,
                                    bool Inverse)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(OutputIdx == 0); // will have to handle this if this ever changes...

    if (m_Active != 0)
    {
        node* AsNode = GSTATE_DYNCAST(m_Active, node);
        if (AsNode)
        {
            granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
            return AsNode->GetRootMotionVectors(SampleIdx, AtT, DeltaT, Translation, Rotation, Inverse);
        }
        else
        {
            transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
            GStateAssert(SampleIndexForTransition(this, m_state_machineToken, OutputIdx) == 0);
            return AsTransition->GetRootMotionVectors(AtT, DeltaT, Translation, Rotation, Inverse);
        }
    }

    // Return null, caller is resposible for building rest pose if necessary
    return false;
}

void GSTATE
state_machine::ActivateConditionals(granny_real32 AtT)
{
    {for (int Idx = 0; Idx < m_state_machineToken->ConditionalCount; ++Idx)
    {
        m_Conditionals[Idx]->Activate(AtT);
    }}
}


void GSTATE
state_machine::Activate(granny_int32x OnOutput, granny_real32 AtT)
{
    // todo: activate on parameter edges?
    if (OnOutput == 0)
    {
        if (m_state_machineToken->StartState != eInvalidChild)
        {
            node* State = GetChild(m_state_machineToken->StartState);
            ForceState(AtT, State);
        }

        // Push the activation down to the conditionals...
        // No, don't.  The node will handle this...
        // {for (int Idx = 0; Idx < m_state_machineToken->ConditionalCount; ++Idx)
        // {
        //     m_Conditionals[Idx]->Activate(AtT);
        // }}
    }
}

bool GSTATE
state_machine::DidSubLoopOccur(node*         SubNode,
                               granny_int32  OnOutput,
                               granny_real32 AtT,
                               granny_real32 DeltaT)
{
    GStateAssert(SubNode != 0 && GetChildIdx(SubNode) != eInvalidChild);

    // Only check if the node is active
    if (SubNode == m_Active)
    {
        return SubNode->DidLoopOccur(OnOutput, AtT, DeltaT);
    }

    return false;
}

// =============================================================================
// Published parameters (corresponds to edges other than 0)
// =============================================================================
granny_int32x GSTATE
state_machine::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    TakeTokenOwnership();

    if (GetNumOutputs() != 0 && (EdgeType != ScalarEdge && EdgeType != EventEdge && EdgeType != MorphEdge))
    {
        GS_InvalidCodePath("For now, not allowed");
        return -1;
    }

    // Pass to the parent, we'll work with our children after this...
    granny_int32x NewEdgeIdx = parent::AddOutput(EdgeType, EdgeName);

    // We now need to add this output to all of our children except for parameter nodes.
    // Note that this can cascade down to sub-statemachines...
    AddOutputToChildren(NewEdgeIdx);

    return NewEdgeIdx;
}

bool GSTATE
state_machine::SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName)
{
    if (parent::SetOutputName(OutputIdx, NewEdgeName) == false)
        return false;

    // We now need to add this output to all of our children except for parameter nodes.
    // Note that this can cascade down to sub-statemachines...
    RefreshChildOutputName(OutputIdx);
    return true;
}


bool GSTATE
state_machine::DeleteOutput(granny_int32x OutputIndex)
{
    TakeTokenOwnership();

    RemoveOutputFromChildren(OutputIndex);
    RemoveOutputFromConditionals(this, OutputIndex);

    bool RetVal = parent::DeleteOutput(OutputIndex);
    return RetVal;
}


granny_int32x GSTATE
state_machine::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    granny_int32x RetVal = parent::AddInput(EdgeType, EdgeName);

    AdjustChildInputs();

    GStateAssert(GetInputType(RetVal) == EdgeType);
    GStateAssert(strcmp(GetInputName(RetVal), EdgeName) == 0);

    return RetVal;
}

bool GSTATE
state_machine::DeleteInput(granny_int32x InputIndex)
{
    // Huge trouble if this fails...
    bool RetVal = parent::DeleteInput(InputIndex);

    AdjustChildInputs();

    return RetVal;
}


void GSTATE
state_machine::AddInputsForNode(node* Node)
{
    GStateAssert(Node == this || IsParameterNode(Node));

    int const NumChildren = GetNumChildren();
    int const NumOutputs  = Node->GetNumOutputs();

    for (int OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
    {
        // We wire up our internal outputs, but parameter nodes wire their external outputs.
        if ((Node == this && Node->IsOutputExternal(OutputIdx)) ||
            (Node != this && Node->IsOutputInternal(OutputIdx)))
        {
            continue;
        }

        for (int ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
        {
            node* Child = GetChild(ChildIdx);

            // We only care about complicated children...
            if ((GSTATE_DYNCAST(Child, blend_graph) == 0) &&
                (GSTATE_DYNCAST(Child, state_machine) == 0))
            {
                continue;
            }

            bool Found = false;
            int const NumChildInputs = Child->GetNumInputs();
            for (int ChildInputIdx = 0; ChildInputIdx < NumChildInputs && !Found; ++ChildInputIdx)
            {
                if (Child->IsInputExternal(ChildInputIdx) == false)
                    continue;

                INPUT_CONNECTION_ON(Child, ChildInputIdx, Test);
                if (TestNode == Node && TestEdge == OutputIdx)
                {
                    GStateAssert(Node->GetOutputType(OutputIdx) == Child->GetInputType(ChildInputIdx));
                    Found = true;
                }
            }

            if (Found == false)
            {
                int NewInputIdx = Child->AddInput(Node->GetOutputType(OutputIdx),
                                                  Node->GetOutputName(OutputIdx));
                Child->SetInputConnection(NewInputIdx, Node, OutputIdx);
            }
        }
    }
}

void GSTATE
state_machine::AdjustChildInputs()
{
    // First, loop through all children.  Look for external inputs on blend_graphs and
    // state_machines that are not connected to us or one of our children anymore.  Delete
    // those.
    int const NumChildren = GetNumChildren();
    for (int Idx = 0; Idx < NumChildren; ++Idx)
    {
        node* Child = GetChild(Idx);

        // We only care about complicated children...
        if ((GSTATE_DYNCAST(Child, blend_graph) == 0) &&
            (GSTATE_DYNCAST(Child, state_machine) == 0))
        {
            continue;
        }

        for (int InputIdx = Child->GetNumInputs() - 1; InputIdx >= 0; --InputIdx)
        {
            if (Child->IsInputExternal(InputIdx) == false)
                continue;

            INPUT_CONNECTION_ON(Child, InputIdx, Test);
            if (TestNode == 0)
            {
                Child->DeleteInput(InputIdx);
            }
            else
            {
                GStateAssert(TestNode->GetOutputType(TestEdge) == Child->GetInputType(InputIdx));
                GStateAssert(TestNode == this ||
                             (IsParameterNode(TestNode) && TestNode->GetParent() == this));
            }
        }
    }

    // Now.  Loop through all of our internal outputs.  Make sure each complicated child
    // has an input connected to these.  Then loop for all parameter nodes.  Similarly for
    // those.
    AddInputsForNode(this);
    for (int Idx = 0; Idx < NumChildren; ++Idx)
    {
        node* Child = GetChild(Idx);
        if (IsParameterNode(Child))
            AddInputsForNode(Child);
    }
}


// =============================================================================
// Conditionals for transitions, etc.
// =============================================================================
granny_int32x GSTATE
state_machine::GetNumConditionals()
{
    return m_state_machineToken->ConditionalCount;
}


conditional* GSTATE
state_machine::GetConditional(granny_int32x Index)
{
    GStateCheckIndex(Index, m_state_machineToken->ConditionalCount, return 0);

    return m_Conditionals[Index];
}

granny_int32x GSTATE
state_machine::AddConditional(conditional* NewCondition)
{
    GStateAssert(NewCondition->GetOwner() == 0);
    GStateAssert(NewCondition->GetTokenContext() == GetTokenContext());

    if (NewCondition == 0)
    {
        GS_InvalidCodePath("adding null condition?");
        return -1;
    }

    // Just make sure that this isn't already in the conditional array...
    {for (int Idx = 0; Idx < m_state_machineToken->ConditionalCount; ++Idx)
    {
        if (NewCondition == m_Conditionals[Idx])
        {
            GS_InvalidCodePath("adding existing conditional?");
            return Idx;
        }
    }}

    // Ok, add it.
    TakeTokenOwnership();

    // Do the m_NewConditionNodes first so we can use the token count, which isn't updated
    conditional*& NewEntry   = QVecPushNewElementNoCount(m_state_machineToken->ConditionalCount, m_Conditionals);
    granny_variant& NewToken = QVecPushNewElement(m_state_machineToken->ConditionalCount,
                                                  m_state_machineToken->Conditionals);
    // Set the new entries
    if (!NewCondition->GetTypeAndToken(&NewToken))
    {
        GS_InvalidCodePath("oooh, that's bad, couldn't get child token");
        return -1;
    }

    // Set the parent field
    NewCondition->SetOwner(this);

    // And our cache of created conditionals
    NewEntry = NewCondition;

    // We added it at the back...
    return (m_state_machineToken->ConditionalCount - 1);
}

bool GSTATE
state_machine::ReplaceConditional(granny_int32x ConditionalIndex, conditional* NewCondition)
{
    GStateCheckIndex(ConditionalIndex, GetNumConditionals(), return false);
    GStateCheckPtrNotNULL(NewCondition, return false);
    GStateCheck(NewCondition->GetOwner() == 0, return false);

    // Do this in one operation to avoid disturbing the indices...
    TakeTokenOwnership();

    granny_variant NewToken;
    if (NewCondition->GetTypeAndToken(&NewToken) == false)
    {
        GS_InvalidCodePath("Unable to get type/token, really bad.");
        return false;
    }

    m_state_machineToken->Conditionals[ConditionalIndex] = NewToken;

    // Remove the old entry...
    conditional* OldConditional = m_Conditionals[ConditionalIndex];
    OldConditional->SetOwner(0);

    // We need to flush out the conditional from any transitions that refer to it...
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);

        for (int TransitionIdx = 0; TransitionIdx < Child->GetNumTransitions(); ++TransitionIdx)
        {
            transition* Transition = Child->GetTransition(TransitionIdx);

            for (int CondIdx = 0; CondIdx < Transition->GetNumConditionals(); ++CondIdx)
            {
                if (Transition->GetConditional(CondIdx) == OldConditional)
                    Transition->SetConditional(CondIdx, NewCondition);
            }
        }
    }


    // Now we can nuke it
    GStateDelete<conditional>(OldConditional);

    // And in with the new...
    NewCondition->SetOwner(this);
    m_Conditionals[ConditionalIndex] = NewCondition;

    return true;
}

void GSTATE
state_machine::DeleteConditional(granny_int32x ConditionalIndex)
{
    GStateAssert(GS_InRange(ConditionalIndex, GetNumConditionals()));

    TakeTokenOwnership();

    conditional* Remove = m_Conditionals[ConditionalIndex];

    // Remove the conditional from any transitions in our children
    {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);

        {for (int TransitionIdx = 0; TransitionIdx < Child->GetNumTransitions(); ++TransitionIdx)
        {
            transition* Transition = Child->GetTransition(TransitionIdx);
            Transition->Note_ConditionalDelete(Remove);
        }}
    }}

    QVecRemoveElementNoCount(ConditionalIndex, m_state_machineToken->ConditionalCount, m_Conditionals);
    QVecRemoveElement(ConditionalIndex,
                      m_state_machineToken->ConditionalCount,
                      m_state_machineToken->Conditionals);

    GStateDelete<conditional>(Remove);
}


CREATE_SNAPSHOT(state_machine)
{
    // m_Active can be:
    //  - NULL (no children)
    //  - a node
    //  - a transition
    granny_int32 ActiveNonNull = m_Active != 0 ? 1 : 0;
    CREATE_WRITE_INT32(ActiveNonNull);
    if (ActiveNonNull)
    {
        granny_uint32 UID = m_Active->GetUID();
        CREATE_WRITE_INT32(UID);
    }

    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(state_machine)
{
    RESET_OFFSET_TRACKING();

    granny_int32 ActiveNonNull;
    RESET_READ_INT32(ActiveNonNull);

    if (ActiveNonNull)
    {
        granny_int32 UID;
        RESET_READ_INT32(UID);

        m_Active = GetTokenContext()->GetProductForUID(UID);
    }
    else
    {
        m_Active = 0;
    }

    RESET_PASS_TO_PARENT();
}


