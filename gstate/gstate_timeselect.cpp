// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_timeselect.cpp $
// $DateTime: 2012/01/25 13:33:09 $
// $Change: 36369 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#include "gstate_timeselect.h"

#include "gstate_anim_utils.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"

#include <string.h>
#include <stdlib.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE timeselect::timeselectImpl
{
    granny_bool32 TimeIsUnitRelative;
};

granny_data_type_definition GSTATE
timeselect::timeselectImplType[] =
{
    { GrannyInt32Member, "TimeIsUnitRelative" },
    { GrannyEndMember },
};

// timeselect is a concrete class, so we must create a slotted container
struct timeselect_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(timeselect);
};

granny_data_type_definition timeselect::timeselectTokenType[] =
{
    DECL_UID_MEMBER(timeselect),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(timeselect),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(timeselect);
IMPL_CREATE_DEFAULT(timeselect);

GSTATE
timeselect::timeselect(token_context*               Context,
                       granny_data_type_definition* TokenType,
                       void*                        TokenObject,
                       token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_timeselectToken(0)
{
    IMPL_INIT_FROM_TOKEN(timeselect);

    if (EditorCreated())
    {
        // Add our default input/output
        parent::AddInput(ScalarEdge, "Time");
        parent::AddInput(PoseEdge,   "Pose");
        parent::AddOutput(PoseEdge,  "Pose");
    }
}

GSTATE
timeselect::~timeselect()
{
    DTOR_RELEASE_TOKEN(timeselect);
}

bool GSTATE
timeselect::FillDefaultToken(granny_data_type_definition* TokenType,
                             void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares timeselectImpl*& Slot = // member
    GET_TOKEN_SLOT(timeselect);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

void GSTATE
timeselect::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}

granny_real32 GSTATE
timeselect::GetPosition(granny_real32 AtT)
{
    node* PositionNode = 0;
    granny_int32x PositionEdge = -1;
    GetInputConnection(0, &PositionNode, &PositionEdge);

    granny_real32 Result = 0.0f;
    if (PositionNode != 0)
        Result = PositionNode->SampleScalarOutput(PositionEdge, AtT);

    if (m_timeselectToken->TimeIsUnitRelative)
        Result = Clamp(0, Result, 1.0f);

    return Result;
}

granny_local_pose* GSTATE
timeselect::SamplePoseOutput(granny_int32x      OutputIdx,
                             granny_real32      AtT,
                             granny_real32      AllowedError,
                             granny_pose_cache* PoseCache)
{
    GStateAssert(PoseCache != 0);

    // 0 input is the time, advance one for the correct wireup...

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(OutputIdx + 1, &PoseNode, &PoseEdge);
    if (!PoseNode)
        return 0;

    granny_real32 Position = GetPosition(AtT);
    if (m_timeselectToken->TimeIsUnitRelative)
    {
        granny_real32 Duration = PoseNode->GetDuration(PoseEdge);
        if (Duration >= 0.0f)
            Position *= Duration;
        else
            Position = 0;
    }

    return PoseNode->SamplePoseOutput(PoseEdge, Position, AllowedError, PoseCache);
}

bool GSTATE
timeselect::GetRootMotionVectors(granny_int32x  OutputIdx,
                                 granny_real32  AtT,
                                 granny_real32  DeltaT,
                                 granny_real32* Translation,
                                 granny_real32* Rotation,
                                 bool Inverse)
{
    if (DeltaT < 0)
    {
        GS_PreconditionFailed;
        return false;
    }
    if (!Translation || !Rotation)
    {
        GS_PreconditionFailed;
        return false;
    }

    // This is a bit of a philisophical issue.  For the moment, I'm going to assume that
    // for a positionally selected animation, there is no need to maintain continuity
    // between samples.  This may wind up being false in the future, TODO: revisit.
    memset(Translation, 0, sizeof(granny_real32)*3);
    memset(Rotation, 0, sizeof(granny_real32)*3);

    return true;
}

granny_int32x GSTATE
timeselect::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    GStateAssert(EdgeType == PoseEdge);
    GStateAssert(EdgeName);

    granny_int32x NewInput = AddInput(EdgeType, EdgeName);
    granny_int32x NewOutput = parent::AddOutput(EdgeType, EdgeName);
    GStateAssert(NewInput == NewOutput + 1);
    GStateUnused(NewInput);
    GStateUnused(NewOutput);

    return NewOutput;
}

bool GSTATE
timeselect::DeleteOutput(granny_int32x OutputIndex)
{
    GStateAssert(GS_InRange(OutputIndex, GetNumOutputs()));
    GStateAssert(GS_InRange(OutputIndex+1, GetNumInputs()));

    if (DeleteInput(OutputIndex+1) == false)
    {
        GS_InvalidCodePath("catastrohpic");
    }

    return parent::DeleteOutput(OutputIndex);
}

granny_int32x GSTATE
timeselect::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    return OutputIdx + 1;
}


bool GSTATE
timeselect::GetTimeIsRelative() const
{
    return m_timeselectToken->TimeIsUnitRelative != 0;
}

void GSTATE
timeselect::SetTimeIsRelative(bool IsRelative)
{
    TakeTokenOwnership();

    m_timeselectToken->TimeIsUnitRelative = (granny_bool32)IsRelative;
}
