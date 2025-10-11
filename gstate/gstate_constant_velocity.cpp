// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_constant_velocity.cpp $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#include "gstate_constant_velocity.h"

#include "gstate_node_visitor.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE constant_velocity::constant_velocityImpl
{
    granny_real32 Axis[3];
    granny_real32 Speed;
};

granny_data_type_definition GSTATE
constant_velocity::constant_velocityImplType[] =
{
    { GrannyReal32Member, "Axis", 0, 3 },
    { GrannyReal32Member, "Speed" },
    { GrannyEndMember },
};

// constant_velocity is a concrete class, so we must create a slotted container
struct constant_velocity_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(constant_velocity);
};

granny_data_type_definition constant_velocity::constant_velocityTokenType[] =
{
    DECL_UID_MEMBER(constant_velocity),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(constant_velocity),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(constant_velocity);
IMPL_CREATE_DEFAULT(constant_velocity);

GSTATE
constant_velocity::constant_velocity(token_context*               Context,
                                     granny_data_type_definition* TokenType,
                                     void*                        TokenObject,
                                     token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_constant_velocityToken(0)
{
    IMPL_INIT_FROM_TOKEN(constant_velocity);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInput(PoseEdge, "Pose");
        AddOutput(PoseEdge, "Pose");
    }
}


GSTATE
constant_velocity::~constant_velocity()
{
    DTOR_RELEASE_TOKEN(constant_velocity);
}


bool GSTATE
constant_velocity::FillDefaultToken(granny_data_type_definition* TokenType,
                                    void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares constant_velocityImpl*& Slot = // member
    GET_TOKEN_SLOT(constant_velocity);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->Axis[0] = 0;
    Slot->Axis[1] = 0;
    Slot->Axis[2] = 1;
    Slot->Speed   = 0;

    return true;
}


void GSTATE
constant_velocity::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}

void GSTATE
constant_velocity::Activate(granny_int32x OnOutput,
                            granny_real32 AtT)
{
    GStateAssert(OnOutput == 0);

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

    if (PoseNode)
        PoseNode->Activate(PoseEdge, AtT);
}

bool GSTATE
constant_velocity::DidLoopOccur(granny_int32x OnOutput,
                                granny_real32 AtT,
                                granny_real32 DeltaT)
{
    GStateAssert(OnOutput == 0);

    INPUT_CONNECTION(0, Pose);
    if (!PoseNode)
        return false;

    return PoseNode->DidLoopOccur(PoseEdge, AtT, DeltaT);
}

granny_real32 GSTATE
constant_velocity::GetDuration(granny_int32x OnOutput)
{
    GStateAssert(OnOutput == 0);

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

    if (!PoseNode)
        return -1;

    return PoseNode->GetDuration(PoseEdge);
}

granny_local_pose* GSTATE
constant_velocity::SamplePoseOutput(granny_int32x OutputIdx,
                                    granny_real32 AtT,
                                    granny_real32 AllowedError,
                                    granny_pose_cache* PoseCache)
{
    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache != 0);

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

    if (!PoseNode)
        return 0;

    return PoseNode->SamplePoseOutput(PoseEdge, AtT, AllowedError, PoseCache);
}


bool GSTATE
constant_velocity::GetRootMotionVectors(granny_int32x  OutputIdx,
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

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

    // Rotation always 0
    memset(ResultRotation,    0, sizeof(granny_real32) * 3);

    if (!PoseNode)
    {
        memset(ResultTranslation, 0, sizeof(granny_real32) * 3);
        return true;
    }

    // Yield back the translation
    ResultTranslation[0] = DeltaT * m_constant_velocityToken->Axis[0] * m_constant_velocityToken->Speed;
    ResultTranslation[1] = DeltaT * m_constant_velocityToken->Axis[1] * m_constant_velocityToken->Speed;
    ResultTranslation[2] = DeltaT * m_constant_velocityToken->Axis[2] * m_constant_velocityToken->Speed;

    return true;
}

void GSTATE
constant_velocity::GetAxis(granny_real32* Axis)
{
    memcpy(Axis, m_constant_velocityToken->Axis, sizeof(granny_real32) * 3);
}

granny_real32 GSTATE
constant_velocity::GetSpeed()
{
    return m_constant_velocityToken->Speed;
}

void GSTATE
constant_velocity::SetAxis(granny_real32 const* Axis)
{
    TakeTokenOwnership();

    memcpy(m_constant_velocityToken->Axis, Axis, sizeof(granny_real32) * 3);
}

void GSTATE
constant_velocity::SetSpeed(granny_real32 const NewSpeed)
{
    TakeTokenOwnership();

    m_constant_velocityToken->Speed = NewSpeed;
}

granny_int32x GSTATE
constant_velocity::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}

