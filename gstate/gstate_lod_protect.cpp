// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_lod_protect.cpp $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#include "gstate_lod_protect.h"

#include "gstate_node_visitor.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE lod_protect::lod_protectImpl
{
    granny_real32 Axis[3];
    granny_real32 Speed;
};

granny_data_type_definition GSTATE
lod_protect::lod_protectImplType[] =
{
    { GrannyReal32Member, "Axis", 0, 3 },
    { GrannyReal32Member, "Speed" },
    { GrannyEndMember },
};

// lod_protect is a concrete class, so we must create a slotted container
struct lod_protect_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(lod_protect);
};

granny_data_type_definition lod_protect::lod_protectTokenType[] =
{
    DECL_UID_MEMBER(lod_protect),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(lod_protect),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(lod_protect);
IMPL_CREATE_DEFAULT(lod_protect);

GSTATE
lod_protect::lod_protect(token_context*               Context,
                         granny_data_type_definition* TokenType,
                         void*                        TokenObject,
                         token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_lod_protectToken(0)
{
    IMPL_INIT_FROM_TOKEN(lod_protect);

    if (EditorCreated())
    {
    // Add our default input/output
    AddInput(PoseEdge, "Pose");
    AddOutput(PoseEdge, "Pose");
    }
}


GSTATE
lod_protect::~lod_protect()
{
    DTOR_RELEASE_TOKEN(lod_protect);
}


bool GSTATE
lod_protect::FillDefaultToken(granny_data_type_definition* TokenType,
                              void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares lod_protectImpl*& Slot = // member
    GET_TOKEN_SLOT(lod_protect);

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
lod_protect::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}

void GSTATE
lod_protect::Activate(granny_int32x OnOutput,
                      granny_real32 AtT)
{
    GStateAssert(OnOutput == 0);

    INPUT_CONNECTION(0, Pose);
    if (PoseNode)
        PoseNode->Activate(PoseEdge, AtT);
}


bool GSTATE
lod_protect::DidLoopOccur(granny_int32x OnOutput,
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
lod_protect::GetDuration(granny_int32x OnOutput)
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
lod_protect::SamplePoseOutput(granny_int32x      OutputIdx,
                              granny_real32      AtT,
                              granny_real32      AllowedError,
                              granny_pose_cache* PoseCache)
{
    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache != 0);

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

    if (!PoseNode)
        return 0;

    // Turn off the LOD
    return PoseNode->SamplePoseOutput(PoseEdge, AtT, 0.0f, PoseCache);
}


bool GSTATE
lod_protect::GetRootMotionVectors(granny_int32x  OutputIdx,
                                  granny_real32  AtT,
                                  granny_real32  DeltaT,
                                  granny_real32* ResultTranslation,
                                  granny_real32* ResultRotation,
                                  bool           Inverse)
{
    GStateAssert(GetBoundCharacter());

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

    if (!PoseNode)
        return false;

    return PoseNode->GetRootMotionVectors(PoseEdge, AtT, DeltaT, ResultTranslation, ResultRotation, Inverse);
}

granny_int32x GSTATE
lod_protect::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}

