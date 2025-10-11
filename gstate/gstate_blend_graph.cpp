// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_blend_graph.cpp $
// $DateTime: 2012/10/22 16:29:12 $
// $Change: 39907 $
// $Revision: #5 $
//
// $Notice: $
// ========================================================================
#include "gstate_blend_graph.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE blend_graph::blend_graphImpl
{
    granny_int32 DummyMember;
};

granny_data_type_definition GSTATE
blend_graph::blend_graphImplType[] =
{
    { GrannyInt32Member, "DummyMember" },
    { GrannyEndMember },
};

// blend_graph is a concrete class, so we must create a slotted container
struct blend_graph_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_OPAQUE_TOKEN_SLOT(container);
    DECL_TOKEN_SLOT(blend_graph);
};

granny_data_type_definition blend_graph::blend_graphTokenType[] =
{
    DECL_UID_MEMBER(blend_graph),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(container),
    DECL_TOKEN_MEMBER(blend_graph),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(blend_graph);

IMPL_CREATE_DEFAULT(blend_graph);

GSTATE
blend_graph::blend_graph(token_context*                Context,
                         granny_data_type_definition* TokenType,
                         void*                        TokenObject,
                         token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_blend_graphToken(0)
{
    IMPL_INIT_FROM_TOKEN(blend_graph);
    {
        LinkContainerChildren();
    }

    if (EditorCreated())
    {
        AddOutput(PoseEdge, "Pose");
    }
}


GSTATE
blend_graph::~blend_graph()
{
    DTOR_RELEASE_TOKEN(blend_graph);
}


bool GSTATE
blend_graph::FillDefaultToken(granny_data_type_definition* TokenType,
                              void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares blend_graphImpl*& Slot = // member
    GET_TOKEN_SLOT(blend_graph);

    // Our slot in this token should be empty.
    // Create a new blend_graph Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    // Initialize to default.  In a blendgraph, not much here...
    Slot->DummyMember = 0;

    return true;
}

void GSTATE
blend_graph::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}

granny_real32 GSTATE
blend_graph::SampleScalarOutput(granny_int32x OutputIdx,
                                granny_real32 AtT)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == ScalarEdge);

    INPUT_CONNECTION(OutputIdx, Scalar);
    if (!ScalarNode)
        return 0.0f;

    return ScalarNode->SampleScalarOutput(ScalarEdge, AtT);
}

granny_int32x GSTATE
blend_graph::GetNumMorphChannels(granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);

    INPUT_CONNECTION(OutputIdx, Morph);
    if (!MorphNode)
        return -1;

    return MorphNode->GetNumMorphChannels(MorphEdge);
}

bool GSTATE
blend_graph::SampleMorphOutput(granny_int32x  OutputIdx,
                               granny_real32  AtT,
                               granny_real32* MorphWeights,
                               granny_int32x NumMorphWeights)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);

    INPUT_CONNECTION(OutputIdx, Morph);
    if (!MorphNode)
        return false;

    return MorphNode->SampleMorphOutput(MorphEdge, AtT, MorphWeights, NumMorphWeights);
}

bool GSTATE
blend_graph::GetScalarOutputRange(granny_int32x OutputIdx,
                                  granny_real32* OutMin,
                                  granny_real32* OutMax)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == ScalarEdge);
    
    INPUT_CONNECTION(OutputIdx, Scalar);
    if (!ScalarNode)
        return 0.0f;

    return ScalarNode->GetScalarOutputRange(ScalarEdge, OutMin, OutMax);
}


granny_local_pose* GSTATE
blend_graph::SamplePoseOutput(granny_int32x      OutputIdx,
                              granny_real32      AtT,
                              granny_real32      AllowedError,
                              granny_pose_cache* PoseCache)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(PoseCache);

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx) || Node == 0)
    {
        // Return null, caller is resposible for building rest pose if necessary
        return 0;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == PoseEdge);
    return Node->SamplePoseOutput(EdgeIdx, AtT, AllowedError, PoseCache);
}

bool GSTATE
blend_graph::SampleMaskOutput(granny_int32x      OutputIdx,
                              granny_real32      AtT,
                              granny_track_mask* ModelMask)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
    {
        // todo: is this a warning?  full mask?
        return 0;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == MaskEdge);
    return Node->SampleMaskOutput(EdgeIdx, AtT, ModelMask);
}


bool GSTATE
blend_graph::SampleEventOutput(granny_int32x            OutputIdx,
                               granny_real32            AtT,
                               granny_real32            DeltaT,
                               granny_text_track_entry* EventBuffer,
                               granny_int32x const      EventBufferSize,
                               granny_int32x*           NumEvents)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    if (EventBuffer == 0 || EventBufferSize < 0 || NumEvents == 0)
    {
        GS_PreconditionFailed;
        return false;
    }

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
    {
        *NumEvents = 0;
        return true;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == EventEdge);
    return Node->SampleEventOutput(EdgeIdx, AtT, DeltaT, EventBuffer, EventBufferSize, NumEvents);
}


bool GSTATE
blend_graph::GetAllEvents(granny_int32x            OutputIdx,
                          granny_text_track_entry* EventBuffer,
                          granny_int32x const      EventBufferSize,
                          granny_int32x*           NumEvents)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
    {
        *NumEvents = 0;
        return true;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == EventEdge);
    return Node->GetAllEvents(EdgeIdx, EventBuffer, EventBufferSize, NumEvents);
}


bool GSTATE
blend_graph::GetCloseEventTimes(granny_int32x  OutputIdx,
                                granny_real32  AtT,
                                char const*    TextToFind,
                                granny_real32* PreviousTime,
                                granny_real32* NextTime)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
    {
        return false;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == EventEdge);
    return Node->GetCloseEventTimes(EdgeIdx, AtT, TextToFind, PreviousTime, NextTime);
}

bool GSTATE
blend_graph::GetRootMotionVectors(granny_int32x  OutputIdx,
                                  granny_real32  AtT,
                                  granny_real32  DeltaT,
                                  granny_real32* Translation,
                                  granny_real32* Rotation,
                                  bool Inverse)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx) || Node == 0)
    {
        return false;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == PoseEdge);
    return Node->GetRootMotionVectors(EdgeIdx, AtT, DeltaT, Translation, Rotation, Inverse);
}


void GSTATE
blend_graph::Activate(granny_int32x OnOutput, granny_real32 AtT)
{
    GStateAssert(OnOutput >= 0 && OnOutput < GetNumOutputs());

    // Find the input that corresponds to this output.
    node* Node = 0;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OnOutput, &Node, &EdgeIdx) || Node == 0)
    {
        return;
    }

    Node->Activate(EdgeIdx, AtT);
}


bool GSTATE
blend_graph::DidLoopOccur(granny_int32x OnOutput,
                          granny_real32 AtT,
                          granny_real32 DeltaT)
{
    GStateAssert(OnOutput >= 0 && OnOutput < GetNumOutputs());

    // Find the input that corresponds to this output.
    node* Node = 0;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OnOutput, &Node, &EdgeIdx) || Node == 0)
    {
        return false;
    }

    return Node->DidLoopOccur(EdgeIdx, AtT, DeltaT);
}

bool GSTATE
blend_graph::DidSubLoopOccur(node*         SubNode,
                             granny_int32  OnOutput,
                             granny_real32 AtT,
                             granny_real32 DeltaT)
{
    GStateAssert(SubNode != 0 && GetChildIdx(SubNode) != eInvalidChild);

    GStateAssert(OnOutput >= 0 && OnOutput < SubNode->GetNumOutputs());
    return SubNode->DidLoopOccur(OnOutput, AtT, DeltaT);
}

