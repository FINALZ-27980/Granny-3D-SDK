// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_selection.cpp $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "gstate_selection.h"

#include "gstate_anim_utils.h"
#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"

#include <string.h>
#include <math.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE selection::selectionImpl
{
    granny_int32 DummyMember;
};


granny_data_type_definition GSTATE
selection::selectionImplType[] =
{
    { GrannyInt32Member, "DummyMember" },
    { GrannyEndMember },
};

// selection is a concrete class, so we must create a slotted container
struct selection_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(selection);
};

granny_data_type_definition selection::selectionTokenType[] =
{
    DECL_UID_MEMBER(selection),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(selection),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(selection);
IMPL_CREATE_DEFAULT(selection);


GSTATE
selection::selection(token_context*               Context,
                     granny_data_type_definition* TokenType,
                     void*                        TokenObject,
                     token_ownership              TokenIsOwned)
  : parent(Context, TokenType, TokenObject, TokenIsOwned),
    m_selectionToken(0)
{
    IMPL_INIT_FROM_TOKEN(selection);

    if (EditorCreated())
    {
        // Add our default output
        AddOutput(PoseEdge,  "Selection");
        AddInput(ScalarEdge, "Param");
        AddInput(PoseEdge,   "Input");
        AddInput(PoseEdge,   "Input");
    }
}


GSTATE
selection::~selection()
{
    DTOR_RELEASE_TOKEN(selection);
}


bool GSTATE
selection::FillDefaultToken(granny_data_type_definition* TokenType,
                            void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares selectionImpl*& Slot = // member
    GET_TOKEN_SLOT(selection);

    // Our slot in this token should be empty.
    // Create a new selection Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


void GSTATE
selection::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}

bool GSTATE
selection::DeleteInput(granny_int32x InputIndex)
{
    if (InputIndex == 0)
        return false;

    return parent::DeleteInput(InputIndex);
}


bool GSTATE
selection::ObtainSampleIndex(granny_real32 AtT, int* Index)
{
    INPUT_CONNECTION(0, Param);


    granny_real32 ParamMin = 0;
    granny_real32 ParamMax = 1;
    granny_real32 Param = 0;
    if (ParamNode == 0)
    {
        // If param is null no return index
        return false;
    }
    else
    {
        if (ParamNode->GetScalarOutputRange(ParamEdge, &ParamMin, &ParamMax) == false ||
            ParamMin == ParamMax)
        {
            ParamMin = 0;
            ParamMax = 1;
        }
        
        // Clamp the parameter
        Param = Clamp(ParamMin, ParamNode->SampleScalarOutput(ParamEdge, AtT), ParamMax);
    }

    // Put the min at 0
    GStateAssert(ParamMin < ParamMax);
    Param    -= ParamMin;
    ParamMax -= ParamMin;
    ParamMin  = 0;

    int NumInputs = GetNumInputs() - 1;
    if (ParamMax == (NumInputs - 1))
    {
        // Special case to ensure that if the parameters are set to integers, we do this perfectly.
        *Index = (int)floor(Param) + 1;
    }
    else
    {
        granny_real32 NormalizedParam = (Param - ParamMin) / (ParamMax - ParamMin);

        // Choose the input that this parameter designates
        *Index = (int)floor(NormalizedParam * (GetNumInputs() - 1) + 1);
        if (*Index == GetNumInputs())
        {
            // Handle closed interval (i.e, 1.0)
            *Index = GetNumInputs() - 1;
        }
    }
    GStateAssert(*Index >= 1 && *Index < GetNumInputs());

    return true;
}

granny_local_pose* GSTATE
selection::SamplePoseOutput(granny_int32x      OutputIdx,
                            granny_real32      AtT,
                            granny_real32      AllowedError,
                            granny_pose_cache* PoseCache)
{
    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache);

    int Index;
    if (!ObtainSampleIndex(AtT, &Index))
        return 0;
    GStateAssert(Index >= 1 && Index < GetNumInputs());

    node* SampleNode = 0;
    granny_int32x SampleEdge = -1;
    GetInputConnection(Index, &SampleNode, &SampleEdge);

    if (!SampleNode)
    {
        // Unconnected
        return 0;
    }

    return SampleNode->SamplePoseOutput(SampleEdge, AtT, AllowedError, PoseCache);
}

bool GSTATE
selection::GetRootMotionVectors(granny_int32x  OutputIdx,
                                granny_real32  AtT,
                                granny_real32  DeltaT,
                                granny_real32* ResultTranslation,
                                granny_real32* ResultRotation,
                                bool           Inverse)
{
    memset(ResultTranslation, 0, sizeof(granny_real32)*3);
    memset(ResultRotation,    0, sizeof(granny_real32)*3);

    int Index;
    if (!ObtainSampleIndex(AtT, &Index))
    {
        return false;
    }
    GStateAssert(Index >= 1 && Index < GetNumInputs());

    node* SampleNode = 0;
    granny_int32x SampleEdge = -1;
    GetInputConnection(Index, &SampleNode, &SampleEdge);

    // Unconnected
    if (!SampleNode)
        return false;

    return SampleNode->GetRootMotionVectors(SampleEdge, AtT, DeltaT,
                                            ResultTranslation, ResultRotation,
                                            Inverse);
}

granny_real32 GSTATE
selection::GetDuration(granny_int32x OnOutput)
{
    return -1;
}


void GSTATE
selection::Activate(granny_int32x OnOutput, granny_real32 AtT)
{
    GStateAssert(OnOutput == 0);

    // Activate all inputs. ?
    {for (int Idx = 0; Idx < GetNumInputs(); ++Idx)
    {
        node* Node = 0;
        granny_int32x Edge = -1;
        GetInputConnection(Idx, &Node, &Edge);

        if (Node != 0)
            Node->Activate(Edge, AtT);
    }}

    parent::Activate(OnOutput, AtT);
}

bool GSTATE
selection::DidLoopOccur(granny_int32x OnOutput,
                        granny_real32 AtT,
                        granny_real32 DeltaT)
{
    GStateAssert(OnOutput == 0);

    int Index;
    if (!ObtainSampleIndex(AtT, &Index))
        return 0;
    GStateAssert(Index >= 1 && Index < GetNumInputs());

    node* SampleNode = 0;
    granny_int32x SampleEdge = -1;
    GetInputConnection(Index, &SampleNode, &SampleEdge);

    if (!SampleNode)
    {
        // Unconnected
        return false;
    }

    return SampleNode->DidLoopOccur(OnOutput, AtT, DeltaT);
}

