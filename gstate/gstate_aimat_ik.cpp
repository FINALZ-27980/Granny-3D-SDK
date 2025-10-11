// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_aimat_ik.cpp $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #5 $
//
// $Notice: $
// ========================================================================
#include "gstate_aimat_ik.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

granny_real32 const* GSTATE
FloatFromAxisEnum(granny_int32x axis)
{
    static const granny_real32 XAxis[3]    = { 1, 0, 0 };
    static const granny_real32 YAxis[3]    = { 0, 1, 0 };
    static const granny_real32 ZAxis[3]    = { 0, 0, 1 };
    static const granny_real32 NegXAxis[3] = { -1,  0,  0 };
    static const granny_real32 NegYAxis[3] = {  0, -1,  0 };
    static const granny_real32 NegZAxis[3] = {  0,  0, -1 };
    switch (axis)
    {
        case eAimAxis_XAxis: return XAxis;
        case eAimAxis_YAxis: return YAxis;
        case eAimAxis_ZAxis: return ZAxis;
        case eAimAxis_NegXAxis: return NegXAxis;
        case eAimAxis_NegYAxis: return NegYAxis;
        case eAimAxis_NegZAxis: return NegZAxis;

        default:
        {
            GS_InvalidCodePath("misunderstood axis enum");
            return ZAxis;
        }
    }
}

struct GSTATE aimat_ik::aimat_ikImpl
{
    granny_real32 DefaultAimPosition[3];

    char* HeadName;
    char* FirstActiveName;
    char* LastActiveName;

    granny_int32 AimAxis;
    granny_int32 EarAxis;
    granny_int32 GroundNormal;
};

granny_data_type_definition GSTATE
aimat_ik::aimat_ikImplType[] =
{
    { GrannyReal32Member, "DefaultAimPosition", 0, 3 },

    { GrannyStringMember, "HeadName" },
    { GrannyStringMember, "FirstActiveName" },
    { GrannyStringMember, "LastActiveName" },

    { GrannyInt32Member, "AimAxis" },
    { GrannyInt32Member, "EarAxis" },
    { GrannyInt32Member, "GroundNormal" },

    { GrannyEndMember },
};

// aimat_ik is a concrete class, so we must create a slotted container
struct aimat_ik_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(aimat_ik);
};

granny_data_type_definition aimat_ik::aimat_ikTokenType[] =
{
    DECL_UID_MEMBER(aimat_ik),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(aimat_ik),

    { GrannyEndMember }
};

void GSTATE
aimat_ik::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(aimat_ik);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneString(m_aimat_ikToken->HeadName,        OldToken->HeadName);
    GStateCloneString(m_aimat_ikToken->FirstActiveName, OldToken->FirstActiveName);
    GStateCloneString(m_aimat_ikToken->LastActiveName,  OldToken->LastActiveName);
}

void GSTATE
aimat_ik::ReleaseOwnedToken_aimat_ik()
{
    GStateDeallocate(m_aimat_ikToken->HeadName);
    GStateDeallocate(m_aimat_ikToken->FirstActiveName);
    GStateDeallocate(m_aimat_ikToken->LastActiveName);
}


IMPL_CREATE_DEFAULT(aimat_ik);

GSTATE
aimat_ik::aimat_ik(token_context*               Context,
                   granny_data_type_definition* TokenType,
                   void*                        TokenObject,
                   token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_aimat_ikToken(0),
    m_tempPose(0),
    m_HeadIndex(-1),
    m_LinkCount(-1),
    m_InactiveLinks(-1)
{
    IMPL_INIT_FROM_TOKEN(aimat_ik);

    SetAimPosition(m_aimat_ikToken->DefaultAimPosition);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInput(PoseEdge, "Pose");
        AddOutput(PoseEdge, "Pose");
    }
}


GSTATE
aimat_ik::~aimat_ik()
{
    DTOR_RELEASE_TOKEN(aimat_ik);
}

bool GSTATE
aimat_ik::FillDefaultToken(granny_data_type_definition* TokenType,
                           void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares aimat_ikImpl*& Slot = // member
    GET_TOKEN_SLOT(aimat_ik);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->DefaultAimPosition[0] = 0;
    Slot->DefaultAimPosition[1] = 0;
    Slot->DefaultAimPosition[2] = 10;

    Slot->HeadName        = 0;
    Slot->FirstActiveName = 0;
    Slot->LastActiveName  = 0;

    Slot->AimAxis      = eAimAxis_YAxis;
    Slot->EarAxis      = eAimAxis_XAxis;
    Slot->GroundNormal = eAimAxis_ZAxis;

    return true;
}

void GSTATE
aimat_ik::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}

void GSTATE
aimat_ik::Activate(granny_int32x OnOutput,
                   granny_real32 AtT)
{
    GStateAssert(OnOutput == 0);
    INPUT_CONNECTION(OnOutput, Pose);
    if (PoseNode)
        PoseNode->Activate(PoseEdge, AtT);
}

bool GSTATE
aimat_ik::DidLoopOccur(granny_int32x OnOutput,
                       granny_real32 AtT,
                       granny_real32 DeltaT)
{
    GStateAssert(OnOutput == 0);
    INPUT_CONNECTION(OnOutput, Pose);
    if (PoseNode)
        return PoseNode->DidLoopOccur(PoseEdge, AtT, DeltaT);

    return false;
}

granny_real32 GSTATE
aimat_ik::GetDuration(granny_int32x OnOutput)
{
    GStateAssert(OnOutput == 0);
    INPUT_CONNECTION(OnOutput, Pose);
    if (!PoseNode)
        return -1;

    return PoseNode->GetDuration(PoseEdge);
}

void GSTATE
aimat_ik::RefreshBoundValues()
{
    m_HeadIndex = -1;
    m_LinkCount = -1;
    m_InactiveLinks = -1;

    gstate_character_instance* Instance = GetBoundCharacter();
    if (!Instance)
        return;

    granny_model* Model = GetSourceModelForCharacter(Instance);
    if (Model == 0)
        return;

    if (m_aimat_ikToken->HeadName)
    {
        GrannyFindBoneByNameLowercase(Model->Skeleton, m_aimat_ikToken->HeadName, &m_HeadIndex);
    }

    if (m_HeadIndex != -1 &&
        m_aimat_ikToken->FirstActiveName != 0 &&
        m_aimat_ikToken->LastActiveName != 0)
    {
        granny_int32x ActiveIndex = -1;
        GrannyFindBoneByNameLowercase(Model->Skeleton, m_aimat_ikToken->FirstActiveName, &ActiveIndex);

        granny_int32x LastIndex = -1;
        GrannyFindBoneByNameLowercase(Model->Skeleton, m_aimat_ikToken->LastActiveName, &LastIndex);

        if (ActiveIndex != -1 && LastIndex != -1)
        {
            granny_int32x LinkCountTemp = 0;
            m_InactiveLinks = 0;

            granny_int32x Walk = m_HeadIndex;
            while (Walk != LastIndex && Walk != GrannyNoParentBone)
            {
                ++LinkCountTemp;

                Walk = Model->Skeleton->Bones[Walk].ParentIndex;
                GStateAssert(Walk != GrannyNoParentBone);
                if (Walk == ActiveIndex)
                    m_InactiveLinks = LinkCountTemp;
            }

            m_LinkCount = LinkCountTemp;
        }
    }
}

bool GSTATE
aimat_ik::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    granny_model* Model = GetSourceModelForCharacter(Instance);
    GStateAssert(Model);
    GStateAssert(Model->Skeleton);

    m_tempPose = GrannyNewWorldPoseNoComposite(Model->Skeleton->BoneCount);

    memcpy(m_AimPosition, m_aimat_ikToken->DefaultAimPosition, sizeof(m_AimPosition));

    RefreshBoundValues();

    return true;
}

void GSTATE
aimat_ik::UnbindFromCharacter()
{
    GrannyFreeWorldPose(m_tempPose);
    m_tempPose = 0;

    m_HeadIndex     = -1;
    m_LinkCount     = -1;
    m_InactiveLinks = -1;

    parent::UnbindFromCharacter();
}

granny_local_pose* GSTATE
aimat_ik::SamplePoseOutput(granny_int32x      OutputIdx,
                           granny_real32      AtT,
                           granny_real32      AllowedError,
                           granny_pose_cache* PoseCache)
{
    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache != 0);
    GStateAssert(m_tempPose != 0);

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

    if (!PoseNode)
        return 0;

    granny_model* Model = GetSourceModelForCharacter(GetBoundCharacter());
    GStateAssert(Model);
    GStateAssert(Model->Skeleton);

    granny_local_pose* Pose = PoseNode->SamplePoseOutput(PoseEdge, AtT, AllowedError, PoseCache);

    if (Pose && IsActive())
    {
        // Build the world pose so we can do ik
        GrannyBuildWorldPose(Model->Skeleton,
                             0, Model->Skeleton->BoneCount,
                             Pose, 0, m_tempPose);

        granny_real32 Offset[3] = { 0, 0, 0 };

        // GrannyIKOrientTowards(m_HeadIndex,
        //                       FloatFromAxisEnum(m_aimat_ikToken->AimAxis),
        //                       m_AimPosition,
        //                       Model->Skeleton,
        //                       Pose,
        //                       0,
        //                       m_tempPose);
        

        GrannyIKAimAt(m_HeadIndex, m_LinkCount, m_InactiveLinks,
                      Offset,
                      FloatFromAxisEnum(m_aimat_ikToken->AimAxis),
                      FloatFromAxisEnum(m_aimat_ikToken->EarAxis),
                      FloatFromAxisEnum(m_aimat_ikToken->GroundNormal),
                      m_AimPosition,
                      Model->Skeleton, Pose, 0, m_tempPose);
    }

    return Pose;
}


bool GSTATE
aimat_ik::GetRootMotionVectors(granny_int32x  OutputIdx,
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

    // Pass through
    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

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

CREATE_SNAPSHOT(aimat_ik)
{
    CREATE_WRITE_REAL32_ARRAY(m_AimPosition, 3);
    CREATE_WRITE_INT32(m_HeadIndex);
    CREATE_WRITE_INT32(m_LinkCount);
    CREATE_WRITE_INT32(m_InactiveLinks);
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(aimat_ik)
{
    RESET_OFFSET_TRACKING();

    RESET_READ_REAL32_ARRAY(m_AimPosition, 3);
    RESET_READ_INT32(m_HeadIndex);
    RESET_READ_INT32(m_LinkCount);
    RESET_READ_INT32(m_InactiveLinks);
    RESET_PASS_TO_PARENT();
}

void GSTATE
aimat_ik::SetAimPosition(granny_real32 const* Position)
{
    for (int i = 0; i < 3; ++i)
        m_AimPosition[i] = Position[i];
}

void GSTATE
aimat_ik::GetAimPosition(granny_real32* Position)
{
    for (int i = 0; i < 3; ++i)
        Position[i] = m_AimPosition[i];
}

void GSTATE
aimat_ik::SetDefaultAimPosition(granny_real32 const* Position)
{
    TakeTokenOwnership();

    for (int i = 0; i < 3; ++i)
        m_aimat_ikToken->DefaultAimPosition[i] = Position[i];

    SetAimPosition(Position);
}

void GSTATE
aimat_ik::GetDefaultAimPosition(granny_real32* Position)
{
    for (int i = 0; i < 3; ++i)
        Position[i] = m_aimat_ikToken->DefaultAimPosition[i];
}

void GSTATE
aimat_ik::SetAimAxis(EAimAxis Axis)
{
    TakeTokenOwnership();

    m_aimat_ikToken->AimAxis = Axis;
}

GSTATE EAimAxis GSTATE
aimat_ik::GetAimAxis()
{
    GStateAssert(m_aimat_ikToken->AimAxis >= eAimAxis_XAxis &&
                 m_aimat_ikToken->AimAxis <= eAimAxis_NegZAxis);

    return EAimAxis(m_aimat_ikToken->AimAxis);
}

granny_real32 const* GSTATE
aimat_ik::GetAimAxisFloat()
{
    return FloatFromAxisEnum(m_aimat_ikToken->AimAxis);
}


void GSTATE
aimat_ik::SetEarAxis(EAimAxis Axis)
{
    TakeTokenOwnership();

    m_aimat_ikToken->EarAxis = Axis;
}

GSTATE EAimAxis GSTATE
aimat_ik::GetEarAxis()
{
    GStateAssert(m_aimat_ikToken->AimAxis >= eAimAxis_XAxis &&
                 m_aimat_ikToken->AimAxis <= eAimAxis_NegZAxis);

    return EAimAxis(m_aimat_ikToken->EarAxis);
}

granny_real32 const* GSTATE
aimat_ik::GetEarAxisFloat()
{
    return FloatFromAxisEnum(m_aimat_ikToken->EarAxis);
}


void GSTATE
aimat_ik::SetGroundNormal(EAimAxis Axis)
{
    TakeTokenOwnership();

    m_aimat_ikToken->GroundNormal = Axis;
}

GSTATE EAimAxis GSTATE
aimat_ik::GetGroundNormal()
{
    GStateAssert(m_aimat_ikToken->AimAxis >= eAimAxis_XAxis &&
                 m_aimat_ikToken->AimAxis <= eAimAxis_NegZAxis);

    return EAimAxis(m_aimat_ikToken->GroundNormal);
}

granny_real32 const* GSTATE
aimat_ik::GetGroundNormalFloat()
{
    return FloatFromAxisEnum(m_aimat_ikToken->GroundNormal);
}

void GSTATE
aimat_ik::SetHeadName(char const* HeadName)
{
    TakeTokenOwnership();

    GStateReplaceString(m_aimat_ikToken->HeadName, HeadName);

    RefreshBoundValues();
}

void GSTATE
aimat_ik::SetFirstActiveName(char const* FirstActive)
{
    TakeTokenOwnership();

    GStateReplaceString(m_aimat_ikToken->FirstActiveName, FirstActive);

    RefreshBoundValues();
}

void GSTATE
aimat_ik::SetLastActiveName(char const* LastActive)
{
    TakeTokenOwnership();

    GStateReplaceString(m_aimat_ikToken->LastActiveName, LastActive);

    RefreshBoundValues();
}

char const* GSTATE
aimat_ik::GetHeadName()
{
    return m_aimat_ikToken->HeadName;
}

char const* GSTATE
aimat_ik::GetFirstActiveName()
{
    return m_aimat_ikToken->FirstActiveName;
}

char const* GSTATE
aimat_ik::GetLastActiveName()
{
    return m_aimat_ikToken->LastActiveName;
}

bool GSTATE
aimat_ik::IsActive() const
{
    return (m_HeadIndex > 0 &&
            m_LinkCount > 0 &&
            m_InactiveLinks >= 0 &&
            m_InactiveLinks < m_LinkCount);
}

granny_int32x GSTATE
aimat_ik::GetBoundHeadIndex()
{
    return m_HeadIndex;
}

granny_int32x GSTATE
aimat_ik::GetBoundLinkCount()
{
    return m_LinkCount;
}

granny_int32x GSTATE
aimat_ik::GetBoundInactiveLinkCount()
{
    return m_InactiveLinks;
}

granny_int32x GSTATE
aimat_ik::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}


