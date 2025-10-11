// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_blend.cpp $
// $DateTime: 2012/09/27 11:38:42 $
// $Change: 39585 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#include "gstate_blend.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE blend::blendImpl
{
    granny_bool32 PhaseLocked;
    granny_bool32 UseNeighborhood;
};

granny_data_type_definition GSTATE
blend::blendImplType[] =
{
    { GrannyBool32Member, "PhaseLocked" },
    { GrannyBool32Member, "UseNeighborhood" },

    { GrannyEndMember },
};

// blend is a concrete class, so we must create a slotted container
struct blend_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(blend);
};

granny_data_type_definition blend::blendTokenType[] =
{
    DECL_UID_MEMBER(blend),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(blend),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(blend);


IMPL_CREATE_DEFAULT(blend);

GSTATE
blend::blend(token_context*               Context,
             granny_data_type_definition* TokenType,
             void*                        TokenObject,
             token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_DurationFrom(-1),
    m_DurationTo(-1),
    m_LocalOffset(0),
    m_LastObservedParam(0),  // default to  "from"
    m_blendToken(0)
{
    IMPL_INIT_FROM_TOKEN(blend);

    if (EditorCreated())
    {
        // Default is one pose output and the inputs you would expect
        AddOutput(PoseEdge,  "Pose");
        granny_int32x ParamInput = AddInput(ScalarEdge, "Parameter");
        granny_int32x FromInput  = AddInput(PoseEdge,   "From");
        granny_int32x ToInput    = AddInput(PoseEdge,   "To");
        GStateAssert(ParamInput == 0);
        GStateAssert(FromInput  == 1);
        GStateAssert(ToInput    == 2);
        GStateUnused(ParamInput);
        GStateUnused(FromInput);
        GStateUnused(ToInput);
    }
}


GSTATE
blend::~blend()
{
    DTOR_RELEASE_TOKEN(blend);
}

void GSTATE
blend::NoteInputChange(granny_int32x InputIndex)
{
    GStateAssert(InputIndex >= 0 && InputIndex < GetNumInputs());

    parent::NoteInputChange(InputIndex);

    if (InputIndex == 0) // param
        return;
    GStateAssert(InputIndex == 1 || InputIndex == 2);

    granny_real32 Duration = -1;

    node* Node = 0;
    granny_int32x Edge  = -1;
    GetInputConnection(InputIndex, &Node, &Edge);
    if (Node != 0)
    {
        // < 0 for unsupported
        Duration = Node->GetDuration(Edge);
    }

    // todo: FFS, these should be constants.
    if (InputIndex == 1)
    {
        m_DurationFrom = Duration;
    }
    else
    {
        m_DurationTo = Duration;
    }
}

bool GSTATE
blend::FillDefaultToken(granny_data_type_definition* TokenType,
                        void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares blendImpl*& Slot = // member
    GET_TOKEN_SLOT(blend);

    // Our slot in this token should be empty.
    // Create a new blend Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->PhaseLocked = false;

    return true;
}


void GSTATE
blend::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}


granny_local_pose* GSTATE
blend::SamplePoseOutput(granny_int32x      OutputIdx,
                        granny_real32      AtT,
                        granny_real32      AllowedError,
                        granny_pose_cache* PoseCache)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(PoseCache);

    node* ParamNode = 0;
    node* FromNode  = 0;
    node* ToNode    = 0;
    granny_int32x ParamEdge = -1;
    granny_int32x FromEdge  = -1;
    granny_int32x ToEdge    = -1;

    GetInputConnection(0, &ParamNode, &ParamEdge);
    GetInputConnection(1, &FromNode, &FromEdge);
    GetInputConnection(2, &ToNode, &ToEdge);

    granny_local_pose* FromPose = 0;
    granny_local_pose* ToPose   = 0;

    granny_real32 Param = 0;
    if (ParamNode == 0)
    {
        // If param is null no return pose
        goto HandleReturn;  // Oh, yes I did.
    }
    else
    {
        Param = ParamNode->SampleScalarOutput(ParamEdge, AtT);
        // Clamp the parameter from [0,1]
        if (Param <= 0)
            Param = 0;
        else if (Param >= 1)
            Param = 1;
    }

    if (!FromNode || !ToNode)
    {
        // Is one of these NULL?  That simplifies things...
        if (FromNode)
            FromPose = FromNode->SamplePoseOutput(FromEdge, AtT, AllowedError, PoseCache);
        else if (ToNode)
            ToPose   = ToNode->SamplePoseOutput(ToEdge, AtT, AllowedError, PoseCache);

        goto HandleReturn;
    }

    // Ok, we have some real work to do.
    granny_real32 FromT, ToT;
    if (m_blendToken->PhaseLocked == false || CanPhaseLock(false) == false)
    {
        // No phase locking to do, From and To times are the same.
        FromT = AtT;
        ToT   = AtT;
    }
    else
    {
        GStateAssert(m_DurationTo > 0 && m_DurationFrom > 0);

        // Compute the phase lock.
        granny_real32 Pos = (AtT - m_LocalOffset) / (m_DurationFrom + m_LastObservedParam * (m_DurationTo - m_DurationFrom));

        if (Param != m_LastObservedParam)
        {
            // Readjust the phase offset.
            //
            // Without laying out the math too much, we know that:
            // Pos    = (AtT - OffOld) / [m_DurationFrom + OldParam(m_DurationTo - m_DurationFrom)]   (1)
            // OffNew = (Pos[(OldParam - NewParam)(m_DurationTo - m_DurationFrom)] + OffOld)  (2)
            //
            // Note that you can derive (2) from replacing the Old with New in (1), and
            // holding Pos constant.

            granny_real32 NewOff  = (Pos * ((m_LastObservedParam - Param) * (m_DurationTo - m_DurationFrom)) + m_LocalOffset);
            m_LocalOffset = NewOff;
        }

        FromT = Pos * m_DurationFrom;
        ToT   = Pos * m_DurationTo;
    }

    // Make sure to cull one side if the param is locked to the edge
    if (Param < 1.0f)
        FromPose = FromNode->SamplePoseOutput(FromEdge, FromT, AllowedError, PoseCache);

    if (Param > 0.0f)
        ToPose = ToNode->SamplePoseOutput(ToEdge, ToT, AllowedError, PoseCache);

    // These can still be NULL, if they are connected to a blend graph that's not
    // internally wired.  Check for that here...
HandleReturn:
    m_LastObservedParam = Param;

    if (!FromPose || !ToPose)
    {
        if (FromPose)
            return FromPose;
        else if (ToPose)
            return ToPose;
        else
            return 0;
    }

    // Blend, and release endpose before we return
    if (m_blendToken->UseNeighborhood)
    {
        granny_model* Model = GetSourceModelForCharacter(GetBoundCharacter());
        GStateAssert(Model->Skeleton);
        GrannyLinearBlendNeighborhood(FromPose, FromPose, ToPose, Param, Model->Skeleton);
    }
    else
    {
        GrannyLinearBlend(FromPose, FromPose, ToPose, Param);
    }
    
    GrannyReleaseCachePose(PoseCache, ToPose);
    return FromPose;
}


bool GSTATE
blend::GetRootMotionVectors(granny_int32x  OutputIdx,
                            granny_real32  AtT,
                            granny_real32  DeltaT,
                            granny_real32* Translation,
                            granny_real32* Rotation,
                            bool Inverse)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    bool RetVal = false;

    node* ParamNode = 0;
    node* FromNode  = 0;
    node* ToNode    = 0;
    granny_int32x ParamEdge = -1;
    granny_int32x FromEdge  = -1;
    granny_int32x ToEdge    = -1;

    GetInputConnection(0, &ParamNode, &ParamEdge);
    GetInputConnection(1, &FromNode, &FromEdge);
    GetInputConnection(2, &ToNode, &ToEdge);

    granny_real32 Param = 0;
    if (ParamNode == 0)
    {
        return false;
    }
    else
    {
        Param = ParamNode->SampleScalarOutput(ParamEdge, AtT);
        // Clamp the parameter from [0,1]
        if (Param < 0)
            Param = 0;
        else if (Param > 1)
            Param = 1;
    }

    if (!FromNode || !ToNode)
    {
        // Is one of these NULL?  That simplifies things...
        if (FromNode)
        {
            m_LastObservedParam = Param;
            return FromNode->GetRootMotionVectors(FromEdge, AtT, DeltaT, Translation, Rotation, Inverse);
        }
        else if (ToNode)
        {
            m_LastObservedParam = Param;
            return ToNode->GetRootMotionVectors(ToEdge, AtT, DeltaT, Translation, Rotation, Inverse);
        }

        memset(Translation, 0, sizeof(granny_real32)*3);
        memset(Rotation,    0, sizeof(granny_real32)*3);
        return false;
    }

    // TODO: !!!FACTOR THIS OUT!!!
    // Ok, we have some real work to do.
    granny_real32 FromT, FromDelta;
    granny_real32 ToT, ToDelta;
    if (m_blendToken->PhaseLocked == false || CanPhaseLock(false) == false)
    {
        // No phase locking to do, From and To times are the same.
        FromT     = AtT;
        FromDelta = DeltaT;

        ToT     = AtT;
        ToDelta = DeltaT;
    }
    else
    {
        GStateAssert(m_DurationTo > 0 && m_DurationFrom > 0);

        // Compute the phase lock.
        granny_real32 Pos =
            (AtT - m_LocalOffset) / (m_DurationFrom + m_LastObservedParam * (m_DurationTo - m_DurationFrom));
        granny_real32 DeltaPos =
            ((AtT - DeltaT) - m_LocalOffset) / (m_DurationFrom + m_LastObservedParam * (m_DurationTo - m_DurationFrom));

        if (Param != m_LastObservedParam)
        {
            // Readjust the phase offset.
            //
            // Without laying out the math too much, we know that:
            // Pos    = (AtT - OffOld) / [m_DurationFrom + OldParam(m_DurationTo - m_DurationFrom)]   (1)
            // OffNew = (Pos[(OldParam - NewParam)(m_DurationTo - m_DurationFrom)] + OffOld)  (2)
            //
            // Note that you can derive (2) from replacing the Old with New in (1), and
            // holding Pos constant.

            granny_real32 NewOff  =
                (Pos * ((m_LastObservedParam - Param) * (m_DurationTo - m_DurationFrom)) + m_LocalOffset);
            m_LocalOffset = NewOff;
        }

        FromT     = Pos * m_DurationFrom;
        FromDelta = (Pos - DeltaPos) * m_DurationFrom;

        ToT     = Pos * m_DurationTo;
        ToDelta = (Pos - DeltaPos) * m_DurationTo;
    }

    granny_real32 FromTrans[3], FromRot[3];
    granny_real32 ToTrans[3], ToRot[3];

    bool FromValid = FromNode->GetRootMotionVectors(FromEdge, FromT, FromDelta, FromTrans, FromRot, Inverse);
    bool ToValid   = ToNode->GetRootMotionVectors(ToEdge, ToT, ToDelta, ToTrans, ToRot, Inverse);

    m_LastObservedParam = Param;

    if (!FromValid || !ToValid)
    {
        RetVal = FromValid || ToValid;

        if (FromValid)
        {
            memcpy(Translation, FromTrans, sizeof(granny_real32)*3);
            memcpy(Rotation, FromRot, sizeof(granny_real32)*3);
            return true;
        }
        else if (ToValid)
        {
            memcpy(Translation, ToTrans, sizeof(granny_real32)*3);
            memcpy(Rotation, ToRot, sizeof(granny_real32)*3);
            return true;
        }
    }
    else
    {
        {for (int Idx = 0; Idx < 3; ++Idx)
        {
            Translation[Idx] = FromTrans[Idx] * (1 - Param) + ToTrans[Idx] * Param;
            Rotation[Idx]    = FromRot[Idx]   * (1 - Param) + ToRot[Idx]   * Param;
        }}
        RetVal = true;
    }

    return RetVal;
}

granny_real32 GSTATE
blend::GetDuration(granny_int32x OnOutput)
{
    GStateAssert(OnOutput == 0);

    // Only return a duration if we are phase locked.  Note that we know
    // CanPhaseLock(false) doesn't modify our state....
    if (m_blendToken->PhaseLocked == false || const_cast<blend*>(this)->CanPhaseLock(false) == false)
        return -1;

    // Compute from last observed parameter
    // todo: hm, this can lag a frame, correctable?

    GStateAssert(m_DurationTo >= 0);
    GStateAssert(m_DurationFrom >= 0);
    return (m_DurationFrom + m_LastObservedParam * (m_DurationTo - m_DurationFrom));
}


void GSTATE
blend::Activate(granny_int32x OnOutput, granny_real32 AtT)
{

    parent::Activate(OnOutput, AtT);

    m_LocalOffset = AtT;

    {for (int Idx = 0; Idx < GetNumInputs(); ++Idx)
    {
        node* Node = 0;
        granny_int32x Edge = -1;
        GetInputConnection(Idx, &Node, &Edge);
        if (Node != 0)
        {
            // todo: This is going to cause problems if the node is connected to anything
            // *else*.  Should disallow that?
            if (m_blendToken->PhaseLocked)
                Node->Activate(Edge, 0);
            else
                Node->Activate(Edge, AtT);
        }
    }}
}

bool GSTATE
blend:: CanPhaseLock(bool RefreshCache)
{

    if (RefreshCache)
    {
        NoteInputChange(1);
        NoteInputChange(2);
    }

    return (m_DurationFrom > 0 && m_DurationTo > 0);
}

void GSTATE
blend::CaptureSiblingData()
{
    parent::CaptureSiblingData();

    // Refresh the duration cache.  All of our inputs are flushed at this point.
    CanPhaseLock(true);
}


bool GSTATE
blend::GetPhaseLocked() const
{
    return m_blendToken->PhaseLocked != 0;
}

void GSTATE
blend::SetPhaseLocked(bool Lock)
{
    TakeTokenOwnership();

    m_blendToken->PhaseLocked = Lock;

    // Should reactivate sub nodes?  For now, make the caller do it...
}

bool GSTATE
blend::GetNeighborhooded() const
{
    return m_blendToken->UseNeighborhood != 0;
}

void GSTATE
blend::SetNeighborhooded(bool Neighboorhood)
{
    TakeTokenOwnership();

    m_blendToken->UseNeighborhood = Neighboorhood;
}



CREATE_SNAPSHOT(blend)
{
    CREATE_WRITE_REAL32(m_DurationFrom);
    CREATE_WRITE_REAL32(m_DurationTo);
    CREATE_WRITE_REAL32(m_LocalOffset);
    CREATE_WRITE_REAL32(m_LastObservedParam);

    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(blend)
{
    RESET_OFFSET_TRACKING();

    RESET_READ_REAL32(m_DurationFrom);
    RESET_READ_REAL32(m_DurationTo);
    RESET_READ_REAL32(m_LocalOffset);
    RESET_READ_REAL32(m_LastObservedParam);

    RESET_PASS_TO_PARENT();
}


