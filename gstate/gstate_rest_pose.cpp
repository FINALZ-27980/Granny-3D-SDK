// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_rest_pose.cpp $
// $DateTime: 2012/03/16 15:41:10 $
// $Change: 36794 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "gstate_rest_pose.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE rest_pose::rest_poseImpl
{
    granny_int32 Dummy;
};

granny_data_type_definition GSTATE
rest_pose::rest_poseImplType[] =
{
    { GrannyInt32Member, "Dummy" },
    { GrannyEndMember },
};

// rest_pose is a concrete class, so we must create a slotted container
struct rest_pose_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(rest_pose);
};

granny_data_type_definition rest_pose::rest_poseTokenType[] =
{
    DECL_UID_MEMBER(rest_pose),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(rest_pose),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(rest_pose);
IMPL_CREATE_DEFAULT(rest_pose);

GSTATE
rest_pose::rest_pose(token_context*               Context,
                           granny_data_type_definition* TokenType,
                           void*                        TokenObject,
                           token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_rest_poseToken(0)
{
    IMPL_INIT_FROM_TOKEN(rest_pose);

    if (EditorCreated())
    {
        // Add our default input/output
        AddOutput(PoseEdge, "Pose");
    }
}


GSTATE
rest_pose::~rest_pose()
{
    DTOR_RELEASE_TOKEN(rest_pose);
}

bool GSTATE
rest_pose::FillDefaultToken(granny_data_type_definition* TokenType,
                               void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares rest_poseImpl*& Slot = // member
    GET_TOKEN_SLOT(rest_pose);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


void GSTATE
rest_pose::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}


granny_local_pose* GSTATE
rest_pose::SamplePoseOutput(granny_int32x OutputIdx,
                               granny_real32 AtT,
                               granny_real32 AllowedError,
                               granny_pose_cache* PoseCache)
{
    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache != 0);

    granny_model* Model = GetSourceModelForCharacter(GetBoundCharacter());
    if (!Model)
        return 0;

    granny_skeleton* Skeleton = Model->Skeleton;
    granny_local_pose* IntoPose = GrannyGetNewLocalPose(PoseCache, Skeleton->BoneCount);
    GStateAssert(GrannyGetLocalPoseBoneCount(IntoPose) == Skeleton->BoneCount);
    
    GrannyBuildRestLocalPose(Skeleton, 0, Skeleton->BoneCount, IntoPose);
    return IntoPose;
}


bool GSTATE
rest_pose::GetRootMotionVectors(granny_int32x  OutputIdx,
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

    // Also easy...
    memset(ResultTranslation, 0, sizeof(granny_real32)*3);
    memset(ResultRotation,    0, sizeof(granny_real32)*3);
    return true;
}
