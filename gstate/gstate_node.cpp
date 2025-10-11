// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_node.cpp $
// $DateTime: 2012/10/22 16:29:12 $
// $Change: 39907 $
// $Revision: #13 $
//
// $Notice: $
// ========================================================================
#include "gstate_node.h"

#include "gstate_blend_graph.h"
#include "gstate_container.h"
#include "gstate_node_visitor.h"
#include "gstate_quick_vecs.h"
#include "gstate_state_machine.h"
#include "gstate_token_context.h"
#include "gstate_transition.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct node_edge
{
    char*          EdgeName;
    granny_int32   EdgeType;      // int32(node_edge_type)
    granny_int32   IsInternal;
    granny_int32   ForwardEdge;
    granny_variant OtherNode;     // type/token
    granny_int32   OtherNodeEdge;
};

struct GSTATE node::nodeImpl
{
    char*           Name;
    granny_bool32   NameIsDefault;

    granny_real32    EditingPosX;
    granny_real32    EditingPosY;

    granny_int32    InputEdgeCount;
    node_edge*      InputEdges;

    granny_int32    OutputEdgeCount;
    node_edge*      OutputEdges;

    granny_int32    TransitionCount;
    granny_variant* Transitions;

    granny_variant  ParentToken;
};

granny_data_type_definition NodeEdgeType[] =
{
    { GrannyStringMember, "EdgeName" },
    { GrannyInt32Member,  "EdgeType" },
    { GrannyInt32Member,  "IsInternal" },
    { GrannyInt32Member,  "ForwardEdge" },
    { GrannyVariantReferenceMember, "OtherNode" },
    { GrannyInt32Member, "OtherNodeEdge" },

    { GrannyEndMember }
};


granny_data_type_definition GSTATE
node::nodeImplType[] =
{
    { GrannyStringMember, "Name" },
    { GrannyBool32Member, "NameIsDefault" },
    { GrannyReal32Member, "EditingPosX" },
    { GrannyReal32Member, "EditingPosY" },

    { GrannyReferenceToArrayMember, "InputEdges",  NodeEdgeType },
    { GrannyReferenceToArrayMember, "OutputEdges", NodeEdgeType },

    { GrannyReferenceToArrayMember, "Transitions", GrannyVariantType },

    { GrannyVariantReferenceMember, "ParentToken" },

    { GrannyEndMember },
};

// node is a pure virtual class, so there is no need to create a slotted token container

void GSTATE
node::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(node);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneString(m_nodeToken->Name, OldToken->Name);
    GStateCloneArray(m_nodeToken->InputEdges,
                     OldToken->InputEdges,
                     m_nodeToken->InputEdgeCount);
    {for (int Idx = 0; Idx < m_nodeToken->InputEdgeCount; ++Idx)
    {
        GStateCloneString(m_nodeToken->InputEdges[Idx].EdgeName,
                          OldToken->InputEdges[Idx].EdgeName);
    }}

    GStateCloneArray(m_nodeToken->OutputEdges,
                     OldToken->OutputEdges,
                     m_nodeToken->OutputEdgeCount);
    {for (int Idx = 0; Idx < m_nodeToken->OutputEdgeCount; ++Idx)
    {
        GStateCloneString(m_nodeToken->OutputEdges[Idx].EdgeName,
                          OldToken->OutputEdges[Idx].EdgeName);
    }}

    GStateCloneArray(m_nodeToken->Transitions,
                     OldToken->Transitions,
                     m_nodeToken->TransitionCount);
}

void GSTATE
node::ReleaseOwnedToken_node()
{
    GStateDeallocate(m_nodeToken->Name);
    {for (int Idx = 0; Idx < m_nodeToken->InputEdgeCount; ++Idx)
    {
        GStateDeallocate(m_nodeToken->InputEdges[Idx].EdgeName);
    }}
    GStateDeallocate(m_nodeToken->InputEdges);

    {for (int Idx = 0; Idx < m_nodeToken->OutputEdgeCount; ++Idx)
    {
        GStateDeallocate(m_nodeToken->OutputEdges[Idx].EdgeName);
    }}
    GStateDeallocate(m_nodeToken->OutputEdges);
    GStateDeallocate(m_nodeToken->Transitions);
}


GSTATE
node::node(token_context*               Context,
           granny_data_type_definition* TokenType,
           void*                        TokenObject,
           token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_nodeToken(0),
    m_Inputs(0),
    m_Transitions(0),
    m_BoundCharacter(0),
    m_Parent(0)
{
    IMPL_INIT_FROM_TOKEN(node);
    {
        if (m_nodeToken->ParentToken.Object)
        {
            // This really isn't optional, if our parent was never created, we should not have
            // been created either.
            tokenized* Parent = GetTokenContext()->GetProductForToken(m_nodeToken->ParentToken.Object);
            m_Parent = GSTATE_DYNCAST(Parent, container);
            GStateAssert(m_Parent != 0);
        }

        // Note that we *create* these arrays, but the siblings aren't guaranteed to exist
        // until after all of our parent's children have been created.  If we have a
        // parent (which is the only way we can have siblings), it will follow through
        // with a "CaptureSiblings" call
        if (m_nodeToken->InputEdgeCount)
        {
            m_Inputs = GStateAllocArray(node*, m_nodeToken->InputEdgeCount);
            memset(m_Inputs, 0, sizeof(node*)*m_nodeToken->InputEdgeCount);
        }

        if (m_nodeToken->TransitionCount)
        {
            m_Transitions = GStateAllocArray(transition*, m_nodeToken->TransitionCount);
            memset(m_Transitions, 0, sizeof(node*)*m_nodeToken->TransitionCount);

            // Create all of the transitions.  Do NOT wire them up at this point.  Our parent
            // container must create the other nodes before the links are valid.
            {for (int Idx = 0; Idx < m_nodeToken->TransitionCount; ++Idx)
            {
                tokenized* Product =
                    GetTokenContext()->CreateFromToken(m_nodeToken->Transitions[Idx].Type,
                                                       m_nodeToken->Transitions[Idx].Object);
                m_Transitions[Idx] = GSTATE_DYNCAST(Product, transition);
                GStateAssert(m_Transitions[Idx]);
            }}
        }
    }
}

GSTATE
node::~node()
{
    GStateAssert(m_BoundCharacter == 0);

    if (m_Inputs)
    {
        GStateDeallocate(m_Inputs);
        m_Inputs = 0;
    }

    if (m_Transitions)
    {
        {for (int Idx = 0; Idx < m_nodeToken->TransitionCount; ++Idx)
        {
            GStateDelete<transition>(m_Transitions[Idx]);
            m_Transitions[Idx] = 0;
        }}

        GStateDeallocate(m_Transitions);
        m_Transitions = 0;
    }

    DTOR_RELEASE_TOKEN(node);
}


bool GSTATE
node::BindToCharacter(gstate_character_instance* Instance)
{
    GStateAssert(m_BoundCharacter == 0);

    m_BoundCharacter = Instance;
    return (m_BoundCharacter != 0);
}


void GSTATE
node::UnbindFromCharacter()
{
    m_BoundCharacter = 0;
}

gstate_character_instance* GSTATE
node::GetBoundCharacter()
{
    return m_BoundCharacter;
}

void GSTATE
node::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}


char const* GSTATE
node::GetName() const
{
    return m_nodeToken->Name;
}

bool GSTATE
node::SetName(char const* NewName)
{
    TakeTokenOwnership();

    if (NewName == 0)
        return false;


    // replace with copy of NewName
    GStateReplaceString(m_nodeToken->Name, NewName);

    return true;
}

bool GSTATE
node::IsNameUserSpecified()
{
    // Note that this is inverted so that upgraded graphs start with all of their names
    // protected.  Legacy!
    return !m_nodeToken->NameIsDefault;
}

void GSTATE
node::MarkNameUserSpecified()
{
    TakeTokenOwnership();

    m_nodeToken->NameIsDefault = false;
}



container* GSTATE
node::GetParent()
{
    return m_Parent;
}

void GSTATE
node::SetParent(container* NewParent)
{
    // You're not supposed to set this if you're in a non-editable context
    GStateAssert(IsEditable());
    TakeTokenOwnership();

    if (NewParent)
    {
        GStateAssert(m_Parent == 0);

        m_Parent = NewParent;

        // Set the token slot
        if (!m_Parent->GetTypeAndToken(&m_nodeToken->ParentToken))
        {
            GS_InvalidCodePath("That's very bad.");
        }
    }
    else
    {
        // This is a request to clear the parent (we may be moving to a different
        // container)

        // Can't have any transitions
        GStateAssert(m_nodeToken->TransitionCount == 0);
        // todo: GStateAssert(m_inputs) disconnected
        // todo: no siblings refer to us...

        m_Parent = 0;
        m_nodeToken->ParentToken.Type   = 0;
        m_nodeToken->ParentToken.Object = 0;
    }
}

bool GSTATE
node::GetPosition(granny_int32x& x, granny_int32x& y)
{
    x = granny_int32x(m_nodeToken->EditingPosX);
    y = granny_int32x(m_nodeToken->EditingPosY);

    return true;
}

bool GSTATE
node::SetPosition(int x, int y)
{
    // You're not supposed to set this if you're in a non-editable context
    GStateAssert(IsEditable());
    TakeTokenOwnership();

    m_nodeToken->EditingPosX = granny_real32(x);
    m_nodeToken->EditingPosY = granny_real32(y);
    return true;
}

granny_real32 GSTATE
node::GetDuration(granny_int32x OnOutput)
{
    GStateAssert(OnOutput >= 0 && OnOutput < GetNumOutputs());

    return -1;
}

bool GSTATE
node::GetPhaseBoundaries(granny_int32x OnOutput,
                         granny_real32 AtT,
                         granny_real32* PhaseStart,
                         granny_real32* PhaseEnd)
{
    return false;
}


int GSTATE
node::GetNumInputs() const
{
    return m_nodeToken->InputEdgeCount;
}

int GSTATE
node::GetNumOutputs() const
{
    return m_nodeToken->OutputEdgeCount;
}


granny_int32x GSTATE
node::GetNthExternalInput(granny_int32x n) const
{
    if (n < 0 || n > m_nodeToken->InputEdgeCount)
        return -1;

    {for (int Idx = 0; Idx < m_nodeToken->InputEdgeCount; ++Idx)
    {
        if (m_nodeToken->InputEdges[Idx].IsInternal == 0)
        {
            if (n-- == 0)
                return Idx;
        }
    }}

    return -1;
}


granny_int32x GSTATE
node::GetNthExternalOutput(granny_int32x n) const
{
    if (n < 0 || n > m_nodeToken->OutputEdgeCount)
        return -1;

    {for (int Idx = 0; Idx < m_nodeToken->OutputEdgeCount; ++Idx)
    {
        if (m_nodeToken->OutputEdges[Idx].IsInternal == 0)
        {
            if (n-- == 0)
                return Idx;
        }
    }}

    return -1;
}

granny_int32x GSTATE
node::GetNthInternalInput(granny_int32x n) const
{
    if (n < 0 || n > m_nodeToken->InputEdgeCount)
        return -1;

    {for (int Idx = 0; Idx < m_nodeToken->InputEdgeCount; ++Idx)
    {
        if (m_nodeToken->InputEdges[Idx].IsInternal != 0)
        {
            if (n-- == 0)
                return Idx;
        }
    }}

    return -1;
}


granny_int32x GSTATE
node::GetNthInternalOutput(granny_int32x n) const
{
    if (n < 0 || n > m_nodeToken->OutputEdgeCount)
        return -1;

    {for (int Idx = 0; Idx < m_nodeToken->OutputEdgeCount; ++Idx)
    {
        if (m_nodeToken->OutputEdges[Idx].IsInternal != 0)
        {
            if (n-- == 0)
                return Idx;
        }
    }}

    return -1;
}

granny_int32x GSTATE
node::WhichExternalInput(granny_int32x RawIndex) const
{
    GStateAssert(GS_InRange(RawIndex, m_nodeToken->InputEdgeCount));

    int ExtIndex = 0;
    {for (int Idx = 0; Idx < m_nodeToken->InputEdgeCount; ++Idx)
    {
        if (Idx == RawIndex)
        {
            GStateAssert(GetNthExternalInput(ExtIndex) == RawIndex);
            return ExtIndex;
        }

        if (m_nodeToken->InputEdges[Idx].IsInternal == 0)
            ++ExtIndex;
    }}

    GS_InvalidCodePath("Should not reach this point");
    return -1;
}

granny_int32x GSTATE
node::WhichExternalOutput(granny_int32x RawIndex) const
{
    GStateAssert(GS_InRange(RawIndex, m_nodeToken->OutputEdgeCount));

    int ExtIndex = 0;
    {for (int Idx = 0; Idx < m_nodeToken->OutputEdgeCount; ++Idx)
    {
        if (Idx == RawIndex)
        {
            GStateAssert(GetNthExternalOutput(ExtIndex) == RawIndex);
            return ExtIndex;
        }

        if (m_nodeToken->OutputEdges[Idx].IsInternal == 0)
            ++ExtIndex;
    }}

    GS_InvalidCodePath("Should not reach this point");
    return -1;
}

// =============================================================================
// Input interface
// =============================================================================
void GSTATE
node::GetInputConnection(granny_int32x   InputIdx,
                         node**          OtherNode,
                         granny_int32x*  NodeOutputIdx) const
{
    GStateAssert(InputIdx >= 0 && InputIdx < m_nodeToken->InputEdgeCount);
    GStateAssert(OtherNode && NodeOutputIdx);
    GStateAssert(m_Inputs);

    *OtherNode     = m_Inputs[InputIdx];
    *NodeOutputIdx = m_nodeToken->InputEdges[InputIdx].OtherNodeEdge;
}

bool GSTATE
node::SetInputConnection(granny_int32x InputIdx,
                         node*         OtherNode,
                         granny_int32x OtherOutputIdx)
{
    GStateAssert(InputIdx >= 0 && InputIdx < m_nodeToken->InputEdgeCount);

    TakeTokenOwnership();

    if (OtherNode)
    {
        GStateAssert(OtherOutputIdx >= 0 && OtherOutputIdx < OtherNode->GetNumOutputs());
        GStateAssert(OtherNode->GetOutputType(OtherOutputIdx) == GetInputType(InputIdx));

        if (!OtherNode->GetTypeAndToken(&m_nodeToken->InputEdges[InputIdx].OtherNode))
        {
            GS_InvalidCodePath("That's very bad.");
            return false;
        }

        m_nodeToken->InputEdges[InputIdx].OtherNodeEdge = OtherOutputIdx;
        m_Inputs[InputIdx] = OtherNode;
    }
    else
    {
        // Clearing out the reference.
        m_nodeToken->InputEdges[InputIdx].OtherNode.Type   = 0;
        m_nodeToken->InputEdges[InputIdx].OtherNode.Object = 0;
        m_nodeToken->InputEdges[InputIdx].OtherNodeEdge    = -1;
        m_Inputs[InputIdx] = 0;
    }

    NoteInputChange(InputIdx);

    return true;
}

char const* GSTATE
node::GetInputName(granny_int32x InputIdx) const
{
    GStateAssert(InputIdx >= 0 && InputIdx < m_nodeToken->InputEdgeCount);

    return m_nodeToken->InputEdges[InputIdx].EdgeName;
}

node_edge_type GSTATE
node::GetInputType(granny_int32x InputIdx) const
{
    GStateAssert(InputIdx >= 0 && InputIdx < m_nodeToken->InputEdgeCount);

    return (node_edge_type)m_nodeToken->InputEdges[InputIdx].EdgeType;
}

bool GSTATE
node::IsInputExternal(granny_int32x InputIdx) const
{
    GStateAssert(InputIdx >= 0 && InputIdx < m_nodeToken->InputEdgeCount);

    return (m_nodeToken->InputEdges[InputIdx].IsInternal == 0);
}

bool GSTATE
node::SetInputName(granny_int32x InputIdx,
                   char const*   NewEdgeName)

{
    GStateAssert(InputIdx >= 0 && InputIdx < m_nodeToken->InputEdgeCount);
    GStateAssert(NewEdgeName);

    TakeTokenOwnership();

    GStateReplaceString(m_nodeToken->InputEdges[InputIdx].EdgeName, NewEdgeName);
    return m_nodeToken->InputEdges[InputIdx].EdgeName != 0;
}


granny_int32x GSTATE
node::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    GStateAssert(EdgeName && EdgeName[0]);

    // You're not supposed to set this if you're in a non-editable context
    GStateAssert(IsEditable());

    TakeTokenOwnership();

    granny_int32x NewInputIndex = m_nodeToken->InputEdgeCount;

    // Update the cache first so we can use that count...
    node*& NewCacheEntry = QVecPushNewElementNoCount(m_nodeToken->InputEdgeCount, m_Inputs);
    NewCacheEntry = 0;

    // And then the token
    node_edge& NewEdge = QVecPushNewElement(m_nodeToken->InputEdgeCount, m_nodeToken->InputEdges);
    {
        GStateCloneString(NewEdge.EdgeName, EdgeName);
        NewEdge.EdgeType = EdgeType;
        NewEdge.IsInternal = 0;
        NewEdge.ForwardEdge = -1;
        NewEdge.OtherNode.Type   = 0;
        NewEdge.OtherNode.Object = 0;
        NewEdge.OtherNodeEdge = -1;
    }

    // Note the change...
    NoteInputChange(NewInputIndex);

    return NewInputIndex;
}

bool GSTATE
node::DeleteInput(granny_int32x InputIndex)
{
    GStateAssert(InputIndex >= 0 && InputIndex < GetNumInputs());

    // You're not supposed to set this if you're in a non-editable context
    GStateAssert(IsEditable());

    // todo: should this call SetInputConnection to 0 to give derived classes a change to
    // note the change?

    TakeTokenOwnership();

    // We need to free the name pointer of the deleted input first
    {
        GStateDeallocate(m_nodeToken->InputEdges[InputIndex].EdgeName);
        m_nodeToken->InputEdges[InputIndex].EdgeName = 0;
    }

    QVecRemoveElementNoCount(InputIndex, m_nodeToken->InputEdgeCount, m_Inputs);
    QVecRemoveElement(InputIndex, m_nodeToken->InputEdgeCount, m_nodeToken->InputEdges);

    // We do need to look at all of the outputs to see if anything was forwarded either to
    // this input, or a higher index.  Must adjust those, of course...
    {for (int Idx = 0; Idx < m_nodeToken->OutputEdgeCount; ++Idx)
    {
        if (m_nodeToken->OutputEdges[Idx].ForwardEdge >= InputIndex)
        {
            // subtlety: if the input edge that is vanishing is 0, then anything above
            // that will be fine, the edge that refers to this will switch to -1 (not
            // forwarded), and will go away after this anyways.
            --m_nodeToken->OutputEdges[Idx].ForwardEdge;
        }
    }}

    return true;
}

// =============================================================================
// Output interface
// =============================================================================
char const* GSTATE
node::GetOutputName(granny_int32x OutputIdx) const
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < m_nodeToken->OutputEdgeCount);

    return m_nodeToken->OutputEdges[OutputIdx].EdgeName;
}

node_edge_type GSTATE
node::GetOutputType(granny_int32x OutputIdx) const
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < m_nodeToken->OutputEdgeCount);

    return (node_edge_type)m_nodeToken->OutputEdges[OutputIdx].EdgeType;
}

bool GSTATE
node::IsOutputExternal(granny_int32x OutputIdx) const
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < m_nodeToken->OutputEdgeCount);

    return (m_nodeToken->OutputEdges[OutputIdx].IsInternal == 0);
}

bool GSTATE
node::SetOutputName(granny_int32x OutputIdx,
                    char const*   NewEdgeName)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < m_nodeToken->OutputEdgeCount);
    GStateAssert(NewEdgeName);

    TakeTokenOwnership();

    GStateReplaceString(m_nodeToken->OutputEdges[OutputIdx].EdgeName, NewEdgeName);
    return m_nodeToken->OutputEdges[OutputIdx].EdgeName != 0;
}

granny_int32x GSTATE
node::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    // You're not supposed to set this if you're in a non-editable context
    GStateAssert(IsEditable());
    GStateAssert(EdgeName && EdgeName[0]);

    // Take ownership of token
    TakeTokenOwnership();

    granny_int32x NewOutputIndex = m_nodeToken->OutputEdgeCount;

    // And then the token
    node_edge& NewEdge = QVecPushNewElement(m_nodeToken->OutputEdgeCount, m_nodeToken->OutputEdges);
    {
        GStateCloneString(NewEdge.EdgeName, EdgeName);
        NewEdge.EdgeType = EdgeType;
        NewEdge.IsInternal = 0;
        NewEdge.ForwardEdge = 0;
        NewEdge.OtherNode.Type   = 0;
        NewEdge.OtherNode.Object = 0;
        NewEdge.OtherNodeEdge = -1;
    }

    if (GetParent())
        GetParent()->NoteOutputAddition(this, NewOutputIndex);

    return NewOutputIndex;
}

bool GSTATE
node::DeleteOutput(granny_int32x OutputIndex)
{
    GStateAssert(OutputIndex >= 0 && OutputIndex < GetNumOutputs());

    // You're not supposed to set this if you're in a non-editable context
    GStateAssert(IsEditable());

    // Let our parent do any disconnections or adjustments necessary after we nuke
    // this edge...
    bool WasExternal = IsOutputExternal(OutputIndex);
    if (GetParent())
    {
        GetParent()->NoteOutputRemoval_Pre(this, OutputIndex);
    }

    TakeTokenOwnership();

    // We need to free the name pointer of the deleted output first
    {
        GStateDeallocate(m_nodeToken->OutputEdges[OutputIndex].EdgeName);
        m_nodeToken->OutputEdges[OutputIndex].EdgeName = 0;
    }

    QVecRemoveElement(OutputIndex, m_nodeToken->OutputEdgeCount, m_nodeToken->OutputEdges);

    // We do need to look at all of the outputs to see if anything was forwarded either to
    // this input, or a higher index.  Must adjust those, of course...
    {for (int Idx = 0; Idx < m_nodeToken->InputEdgeCount; ++Idx)
    {
        if (m_nodeToken->InputEdges[Idx].ForwardEdge >= OutputIndex)
        {
            // subtlety: if the input edge that is vanishing is 0, then anything above
            // that will be fine, the edge that refers to this will switch to -1 (not
            // forwarded), and will go away after this anyways.
            --m_nodeToken->InputEdges[Idx].ForwardEdge;
        }
    }}

    if (GetParent())
    {
        GetParent()->NoteOutputRemoval_Post(this, WasExternal);
    }

    return true;
}



// =============================================================================
// Initialization
// =============================================================================
bool GSTATE
node::FillDefaultToken(granny_data_type_definition* TokenType,
                       void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares nodeImpl*& Slot = // member
    GET_TOKEN_SLOT(node);

    // Our slot in this token should be empty.
    // Create a new nodeToken
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    // Initialize to default name.  Note that the first slot in the TokenType is the name
    // marker, which will always begin with ContextUID_, and end withe name of the
    // concrete class this node represents.
    GStateAssert(strncmp(TokenType[0].Name, "ContextUID_", strlen("ContextUID_")) == 0);

    char const* TypenameStr = TokenType[0].Name + strlen("ContextUID_");
    GStateCloneString(Slot->Name, TypenameStr);

    // By default the name is, well, default.
    Slot->NameIsDefault = true;

    // At creation, the node has no parent
    Slot->ParentToken.Type   = 0;
    Slot->ParentToken.Object = 0;

    // And it begins at the origin
    Slot->EditingPosX = 0;
    Slot->EditingPosY = 0;

    // With zero input/output edges
    Slot->InputEdgeCount  = 0;
    Slot->InputEdges      = 0;
    Slot->OutputEdgeCount = 0;
    Slot->OutputEdges     = 0;

    // And no transitions
    Slot->TransitionCount = 0;
    Slot->Transitions = 0;

    return true;
}


void GSTATE
node::Activate(granny_int32x OnOutput, granny_real32 AtT)
{
    // nada
}


void GSTATE
node::Synchronize(granny_int32x OnOutput,
                  granny_real32 AtT,
                  granny_real32 ReferenceStart,
                  granny_real32 ReferenceEnd,
                  granny_real32 LocalStart,
                  granny_real32 LocalEnd)
{
    // nothing by default
}


void GSTATE
node::CaptureSiblingLinks()
{
    // Input caching
    {for (int Idx = 0; Idx < m_nodeToken->InputEdgeCount; ++Idx)
    {
        GStateAssert(m_Inputs && m_Inputs[Idx] == 0);

        node_edge& Edge = m_nodeToken->InputEdges[Idx];
        if (Edge.OtherNode.Object)
        {
            tokenized* Sibling = GetTokenContext()->GetProductForToken(Edge.OtherNode.Object);
            if (Sibling)
            {
                m_Inputs[Idx] = GSTATE_DYNCAST(Sibling, node);
                GStateAssert(m_Inputs[Idx] != 0);
                GStateAssert(Edge.OtherNodeEdge >= 0 && Edge.OtherNodeEdge < m_Inputs[Idx]->GetNumOutputs());
            }
            else
            {
                GStateWarning("node::CaptureSiblingLinks: Unable to capture input %d\n", Idx);

                m_Inputs[Idx] = 0;

                // Nuke the token reference as well...
                Edge.OtherNode.Type   = 0;
                Edge.OtherNode.Object = 0;
            }
        }
    }}

    // Our transitions are valid at this point...
    {for (int Idx = 0; Idx < m_nodeToken->TransitionCount; /*++Idx*/)
    {
        transition* Transition = m_Transitions[Idx];
        GStateAssert(Transition);

        if (Transition->CaptureNodeLinks())
        {
            GStateAssert(Transition->GetStartNode() == this);
            GStateAssert(Transition->GetEndNode() != 0 &&
                         Transition->GetEndNode() != this);

            // Advance
            ++Idx;
        }
        else
        {
            GStateWarning("node::CaptureSiblingLinks: Transition %d failed to initialize, removing\n", Idx);
            m_Transitions[Idx] = 0;

            if ((Idx + 1) < m_nodeToken->TransitionCount)
            {
                // Shift the rest of the arrays down
                memmove(m_nodeToken->Transitions + Idx,
                        m_nodeToken->Transitions + (Idx+1),
                        (sizeof(*m_nodeToken->Transitions) *
                         (m_nodeToken->TransitionCount - (Idx+1))));

                memmove(m_Transitions + Idx,
                        m_Transitions + (Idx+1),
                        (sizeof(*m_Transitions) *
                         (m_nodeToken->TransitionCount - (Idx+1))));
            }

            --m_nodeToken->TransitionCount;
            if (m_nodeToken->TransitionCount == 0)
            {
                m_nodeToken->Transitions = 0;
                GStateDeallocate(m_Transitions);
                m_Transitions = 0;
            }

            // Remove the bad transition
            GStateDelete<transition>(Transition);
        }
    }}
}

void GSTATE
node::CaptureSiblingData()
{
    GStateAssert(m_nodeToken->InputEdgeCount == 0 || m_Inputs != 0);

    // Input caching
    {for (int Idx = 0; Idx < m_nodeToken->InputEdgeCount; ++Idx)
    {
        INPUT_CONNECTION(Idx, Input);
        if (InputNode != 0)
        {
            // Go until we find the actual connected node.  Note that we can probably just
            // ignore internal outputs of containers, but walk everything for completeness
            // for now.
            container* Container;
            while ((Container = GSTATE_DYNCAST(InputNode, container)) != 0)
            {
                INPUT_CONNECTION_ON(Container, InputEdge, Cont);

                InputNode = ContNode;
                InputEdge = ContEdge;
            }

            // May have nulled in the loop above...
            if (InputNode)
                InputNode->CaptureSiblingData();
        }
    }}
}


bool GSTATE
node::GetOutputForward(granny_int32x OutputIdx, granny_int32x& ResultIdx) const
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(m_nodeToken->OutputEdges[OutputIdx].OtherNode.Object == 0);
    GStateAssert(m_nodeToken->OutputEdges[OutputIdx].ForwardEdge != -1);

    ResultIdx = m_nodeToken->OutputEdges[OutputIdx].ForwardEdge;
    return true;
}

bool GSTATE
node::GetInputForward(granny_int32x InputIdx, granny_int32x& ResultIdx) const
{
    GStateAssert(InputIdx >= 0 && InputIdx < GetNumInputs());
    GStateAssert(m_nodeToken->InputEdges[InputIdx].ForwardEdge != -1);

    ResultIdx = m_nodeToken->InputEdges[InputIdx].ForwardEdge;
    return true;
}

void GSTATE
node::TransferTransitionsTo(node* OtherNode)
{
    if (m_nodeToken->TransitionCount == 0)
        return;

    TakeTokenOwnership();

    // It would be weird if this were null...
    state_machine* ParentAsSM = GSTATE_DYNCAST(GetParent(), state_machine);

    while (m_nodeToken->TransitionCount != 0)
    {
        int RemoveIdx = m_nodeToken->TransitionCount-1;

        transition* Transition = m_Transitions[RemoveIdx];
        GStateAssert(this == Transition->GetStartNode());

        // Give the parent an opportunity to disable this transition
        if (ParentAsSM != 0)
            ParentAsSM->NoteDeleteTransition(Transition);

        // Remove it from the lists
        QVecRemoveElementNoCount(RemoveIdx, m_nodeToken->TransitionCount, m_Transitions);
        QVecRemoveElement(RemoveIdx, m_nodeToken->TransitionCount, m_nodeToken->Transitions);


        node* EndNode = Transition->GetEndNode();
        granny_int32x StartX;
        granny_int32x StartY;
        granny_int32x EndX;
        granny_int32x EndY;
        Transition->GetDrawingHints(StartX, StartY, EndX,   EndY);

        // Transfer...
        Transition->SetNodes(OtherNode, StartX, StartY,
                             EndNode,   EndX,   EndY);
        OtherNode->AddTransition(Transition);
    }
}

void GSTATE
node::RetargetTransitionsTo(node* OtherNode)
{
    container* Parent = GetParent();
    GStateAssert(OtherNode->GetParent() == Parent);

    for (int Idx = 0; Idx < Parent->GetNumChildren(); ++Idx)
    {
        node* Consider = Parent->GetChild(Idx);
        if (Consider == this || Consider == OtherNode)
            continue;

        for (int TransitionIdx = 0; TransitionIdx < Consider->GetNumTransitions(); ++TransitionIdx)
        {
            transition* Transition = Consider->GetTransition(TransitionIdx);
            if (Transition->GetEndNode() == this)
            {
                granny_int32x StartX;
                granny_int32x StartY;
                granny_int32x EndX;
                granny_int32x EndY;
                Transition->GetDrawingHints(StartX, StartY, EndX,   EndY);
                Transition->SetNodes(Consider,  StartX, StartY,
                                     OtherNode, EndX,   EndY);
            }
        }
    }
}

granny_int32x GSTATE
node::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return -1;
}

void GSTATE
node::SetInputInternal(granny_int32x InputIdx, granny_int32x ExternalOutput)
{
    GStateAssert(InputIdx >= 0 && InputIdx < GetNumInputs());
    GStateAssert(ExternalOutput >= 0 && ExternalOutput < GetNumOutputs());
    GStateAssert(m_nodeToken->InputEdges[InputIdx].OtherNode.Type == 0 &&
                 m_nodeToken->InputEdges[InputIdx].OtherNode.Object == 0);
    GStateAssert(m_nodeToken->InputEdges[InputIdx].IsInternal == 0);
    GStateAssert(m_nodeToken->InputEdges[InputIdx].ForwardEdge == -1);

    TakeTokenOwnership();

    m_nodeToken->InputEdges[InputIdx].IsInternal  = 1;
    m_nodeToken->InputEdges[InputIdx].ForwardEdge = ExternalOutput;

    m_nodeToken->OutputEdges[ExternalOutput].ForwardEdge = InputIdx;

    // todo: no NoteInputChange here, correct?
}

void GSTATE
node::SetOutputInternal(granny_int32x OutputIdx, granny_int32x ExternalInput)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(ExternalInput >= 0 && ExternalInput < GetNumInputs());
    GStateAssert(m_nodeToken->OutputEdges[OutputIdx].OtherNode.Type == 0 &&
                 m_nodeToken->OutputEdges[OutputIdx].OtherNode.Object == 0);
    GStateAssert(m_nodeToken->OutputEdges[OutputIdx].IsInternal == 0);

    TakeTokenOwnership();

    m_nodeToken->OutputEdges[OutputIdx].IsInternal = 1;
    m_nodeToken->OutputEdges[OutputIdx].ForwardEdge = ExternalInput;

    m_nodeToken->InputEdges[ExternalInput].ForwardEdge = OutputIdx;
}

void GSTATE
node::NoteInputChange(granny_int32x InputIndex)
{
    GStateAssert(InputIndex >= 0 && InputIndex < GetNumInputs());

    // nop in node::
}

void GSTATE
node::NoteAnimationSlotDeleted(animation_slot* Slot)
{
    GStateAssert(Slot != 0);

    // nop in node::
}



granny_int32x GSTATE
node::GetNumTransitions() const
{
    return m_nodeToken->TransitionCount;
}

transition* GSTATE
node::GetTransition(granny_int32x TransitionIdx)
{
    GStateAssert(TransitionIdx >= 0 && TransitionIdx < m_nodeToken->TransitionCount);
    GStateAssert(m_Transitions[TransitionIdx] != 0);
    GStateAssert(m_Transitions[TransitionIdx]->GetStartNode() == this);
    GStateAssert(m_Transitions[TransitionIdx]->GetEndNode() != 0 &&
                 m_Transitions[TransitionIdx]->GetEndNode() != this);

    return m_Transitions[TransitionIdx];
}

granny_int32x GSTATE
node::AddTransition(transition* NewTransition)
{
    GStateAssert(IsEditable());

    TakeTokenOwnership();

    // We should be the child of a state machine if this is being called...
    GStateAssert(GetParent() != 0 && GSTATE_DYNCAST(GetParent(), state_machine) != 0);

    granny_int32x NewTransitionIndex = m_nodeToken->TransitionCount;

    transition*& NewCacheEntry = QVecPushNewElementNoCount(m_nodeToken->TransitionCount, m_Transitions);
    NewCacheEntry = 0;

    granny_variant& NewToken = QVecPushNewElement(m_nodeToken->TransitionCount, m_nodeToken->Transitions);
    GStateAssert(&NewToken == m_nodeToken->Transitions + NewTransitionIndex);

    if (!NewTransition->GetTypeAndToken(&NewToken))
    {
        GS_InvalidCodePath("unable to obtain type and token");
        NewToken.Type   = 0;
        NewToken.Object = 0;
        return 0;
    }

    // Grab the cached pointer.  Note that we waited until after the above branch cleared
    // to set this to non-null...
    NewCacheEntry = NewTransition;

    return NewTransitionIndex;
}

bool GSTATE
node::DeleteTransitionByIdx(granny_int32x TransitionIdx)
{
    GStateAssert(TransitionIdx >= 0 && TransitionIdx < m_nodeToken->TransitionCount);

    // Grab the ownership
    TakeTokenOwnership();

    if (m_nodeToken->TransitionCount == 0)
        return false;
    GStateAssert(m_nodeToken->Transitions);

    transition* DeadTransition = m_Transitions[TransitionIdx];

    // If we're actually owned by a state_machine, it does need to have an opportunity to
    // disable this transition
    {
        state_machine* ParentAsSM = GSTATE_DYNCAST(GetParent(), state_machine);
        if (ParentAsSM != 0)
            ParentAsSM->NoteDeleteTransition(DeadTransition);
    }

    // Remove it from the lists
    QVecRemoveElementNoCount(TransitionIdx, m_nodeToken->TransitionCount, m_Transitions);
    QVecRemoveElement(TransitionIdx, m_nodeToken->TransitionCount, m_nodeToken->Transitions);

    // Nuke it.
    GStateDelete<transition>(DeadTransition);

    return true;
}

bool GSTATE
node::DeleteTransition(transition* Transition)
{
    GStateAssert(Transition);

    // Find the transition in our list.  Better be there...
    {for (int Idx = 0; Idx < m_nodeToken->TransitionCount; ++Idx)
    {
        if (m_Transitions[Idx] == Transition)
            return DeleteTransitionByIdx(Idx);
    }}

    // bad.
    return false;
}


bool GSTATE
node::DidLoopOccur(granny_int32x OnOutput,
                   granny_real32 AtT,
                   granny_real32 DeltaT)
{
    return false;
}

granny_real32 GSTATE
node::SampleScalarOutput(granny_int32x OutputIdx,
                         granny_real32 AtT)
{
    GS_InvalidCodePath("this node doesn't sample scalar");
    return 0;
}

bool GSTATE
node::GetScalarOutputRange(granny_int32x  OutputIdx,
                           granny_real32* OutMin,
                           granny_real32* OutMax)
{
    // Allow nodes to use this as the default, i.e, no InvalidCodePath like above...
    return false;
}


granny_local_pose* GSTATE
node::SamplePoseOutput(granny_int32x OutputIdx,
                       granny_real32 AtT,
                       granny_real32 AllowedError,
                       granny_pose_cache* PoseCache)
{
    GS_InvalidCodePath("this node doesn't sample pose");
    return 0;
}


granny_int32x GSTATE
node::GetNumMorphChannels(granny_int32x OutputIdx)
{
    GS_InvalidCodePath("this node doesn't sample morph");
    return -1;
}

bool GSTATE
node::SampleMorphOutput(granny_int32x  OutputIdx,
                        granny_real32  AtT,
                        granny_real32* MorphWeights,
                        granny_int32x NumMorphWeights)
{
    GS_InvalidCodePath("this node doesn't sample morph");
    return false;
}

bool GSTATE
node::SampleMaskOutput(granny_int32x OutputIdx,
                       granny_real32 AtT,
                       granny_track_mask*)
{
    GS_InvalidCodePath("this node doesn't sample mask");
    return false;
}

bool GSTATE
node::SampleEventOutput(granny_int32x            OutputIdx,
                        granny_real32            AtT,
                        granny_real32            DeltaT,
                        granny_text_track_entry* EventBuffer,
                        granny_int32x const      EventBufferSize,
                        granny_int32x*           NumEvents)
{
    GS_InvalidCodePath("this node doesn't sample events");
    return false;
}

bool GSTATE
node::GetNextEvent(granny_int32x            OutputIdx,
                   granny_real32            AtT,
                   granny_text_track_entry* Event)
{
    GS_InvalidCodePath("this node doesn't sample events");
    return false;
}


bool GSTATE
node::GetAllEvents(granny_int32x            OutputIdx,
                   granny_text_track_entry* EventBuffer,
                   granny_int32x const      EventBufferSize,
                   granny_int32x*           NumEvents)
{
    GS_InvalidCodePath("this node doesn't sample events");
    return false;
}

bool GSTATE
node::GetCloseEventTimes(granny_int32x  OutputIdx,
                         granny_real32  AtT,
                         char const*    TextToFind,
                         granny_real32* PreviousTime,
                         granny_real32* NextTime)
{
    GS_InvalidCodePath("this node doesn't sample events");
    return 0.0f;
}


bool GSTATE
node::GetRootMotionVectors(granny_int32x  OutputIdx,
                           granny_real32  AtT,
                           granny_real32  DeltaT,
                           granny_real32* Translation,
                           granny_real32* Rotation,
                           bool Inverse)
{
    memset(Translation, 0, sizeof(granny_real32)*3);
    memset(Rotation,    0, sizeof(granny_real32)*3);
    return true;
}

void GSTATE
node::CheckConnections()
{
#ifndef NDEBUG
    for (int Idx = 0; Idx < GetNumInputs(); ++Idx)
    {
        INPUT_CONNECTION(Idx, Test);

        if (TestNode)
        {
            GStateAssert(TestNode->GetOutputType(TestEdge) == GetInputType(Idx));
        }
    }
#endif
}

