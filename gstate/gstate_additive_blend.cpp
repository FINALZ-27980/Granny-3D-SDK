// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_additive_blend.cpp $
// $DateTime: 2012/09/27 11:38:42 $
// $Change: 39585 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#include "gstate_additive_blend.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

enum
{
    eParamEdge = 0,
    eIntoEdge  = 1,
    eBaseEdge  = 2,
    eDeltaEdge = 3
};

struct GSTATE additive_blend::additive_blendImpl
{
    granny_int32 DummyMember;
};

granny_data_type_definition GSTATE
additive_blend::additive_blendImplType[] =
{
    { GrannyInt32Member, "DummyMember" },
    { GrannyEndMember },
};

// additive_blend is a concrete class, so we must create a slotted container
struct additive_blend_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(additive_blend);
};

granny_data_type_definition additive_blend::additive_blendTokenType[] =
{
    DECL_UID_MEMBER(additive_blend),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(additive_blend),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(additive_blend);
IMPL_CREATE_DEFAULT(additive_blend);

GSTATE
additive_blend::additive_blend(token_context*               Context,
                               granny_data_type_definition* TokenType,
                               void*                        TokenObject,
                               token_ownership              TokenIsOwned)
  : parent(Context, TokenType, TokenObject, TokenIsOwned),
    m_additive_blendToken(0)
{
    IMPL_INIT_FROM_TOKEN(additive_blend);

    // Editing context?
    if (EditorCreated())
    {
        // Default is one pose output and the inputs you would expect for additive blending
        AddOutput(PoseEdge,  "Pose");

        granny_int32x ParamInput = AddInput(ScalarEdge, "Amount");
        granny_int32x IntoInput  = AddInput(PoseEdge,   "Into");
        granny_int32x BaseInput  = AddInput(PoseEdge,   "Base");
        granny_int32x DeltaInput = AddInput(PoseEdge,   "Delta");
        GStateAssert(ParamInput == eParamEdge &&
                     IntoInput  == eIntoEdge &&
                     BaseInput  == eBaseEdge &&
                     DeltaInput == eDeltaEdge);
        GStateUnused(ParamInput);
        GStateUnused(IntoInput);
        GStateUnused(BaseInput);
        GStateUnused(DeltaInput);
    }
}


GSTATE
additive_blend::~additive_blend()
{
    DTOR_RELEASE_TOKEN(additive_blend);
}

bool GSTATE
additive_blend::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares additive_blendImpl*& Slot = // member
    GET_TOKEN_SLOT(additive_blend);

    // Our slot in this token should be empty.
    // Create a new additive_blend Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

void GSTATE
additive_blend::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}

// TODO: finish this up...
granny_local_pose* GSTATE
additive_blend::SamplePoseOutput(granny_int32x      OutputIdx,
                                 granny_real32      AtT,
                                 granny_real32      AllowedError,
                                 granny_pose_cache* PoseCache)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(PoseCache);

    // Can't do anything if input is missing
    INPUT_CONNECTION(eIntoEdge, Into);
    if (!IntoNode)
        return 0;

    // We will always use this
    granny_local_pose* IntoPose = IntoNode->SamplePoseOutput(IntoEdge, AtT, AllowedError, PoseCache);

    // With no param, just grab the base bit...
    INPUT_CONNECTION(eParamEdge, Param);
    if (!ParamNode)
        return IntoPose;

    INPUT_CONNECTION(eBaseEdge,  Base);
    INPUT_CONNECTION(eDeltaEdge, Delta);
    if (!BaseNode || !DeltaNode)
        return IntoPose;

    // NOW sample the parameter, since we're probably going to use it...
    granny_real32 Amount = ParamNode->SampleScalarOutput(ParamEdge, AtT);

    // If there is 0 blended in, don't bother to sample the pose...
    if (Amount == 0.0f)
        return IntoPose;

    granny_local_pose* BasePose = BaseNode->SamplePoseOutput(BaseEdge, AtT, AllowedError, PoseCache);
    if (!BasePose)
        return IntoPose;

    granny_local_pose* DeltaPose = DeltaNode->SamplePoseOutput(DeltaEdge, AtT, AllowedError, PoseCache);
    if (!DeltaPose)
    {
        GrannyReleaseCachePose(PoseCache, BasePose);
        return IntoPose;
    }

    granny_model* Model = GetSourceModelForCharacter(GetBoundCharacter());
    GStateAssert(Model);
    granny_skeleton* Skeleton = Model->Skeleton;
    GStateAssert(Skeleton);

    GrannyAdditiveBlend(IntoPose, DeltaPose, BasePose, 0, Skeleton->BoneCount, Amount);
    GrannyReleaseCachePose(PoseCache, BasePose);
    GrannyReleaseCachePose(PoseCache, DeltaPose);

    return IntoPose;
}

bool GSTATE
additive_blend::GetRootMotionVectors(granny_int32x  OutputIdx,
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
    GetInputConnection(eIntoEdge, &PoseNode, &PoseEdge);

    // The assumption is that only the "Into" Edge matters here...
    if (PoseNode)
    {
        return PoseNode->GetRootMotionVectors(PoseEdge, AtT, DeltaT,
                                              ResultTranslation, ResultRotation,
                                              Inverse);
    }
    else
    {
        memset(ResultTranslation, 0, sizeof(granny_real32)*3);
        memset(ResultRotation,    0, sizeof(granny_real32)*3);
        return false;
    }
}

void GSTATE
additive_blend::Activate(granny_int32x OnOutput, granny_real32 AtT)
{
    parent::Activate(OnOutput, AtT);

    INPUT_CONNECTION(eIntoEdge, Into);
    if (IntoNode)
        IntoNode->Activate(IntoEdge, AtT);

    INPUT_CONNECTION(eBaseEdge, Base);
    if (BaseNode)
        BaseNode->Activate(BaseEdge, AtT);

    INPUT_CONNECTION(eDeltaEdge, Delta);
    if (DeltaNode)
        DeltaNode->Activate(DeltaEdge, AtT);
}
