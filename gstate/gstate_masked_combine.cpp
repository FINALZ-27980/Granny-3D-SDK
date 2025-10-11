// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_masked_combine.cpp $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#include "gstate_masked_combine.h"
#include "gstate_node_visitor.h"
#include "gstate_character_instance.h"
#include "gstate_token_context.h"

#include <string.h>
#include <math.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE masked_combine::masked_combineImpl
{
    granny_int32 Dummy;
};

granny_data_type_definition GSTATE
masked_combine::masked_combineImplType[] =
{
    { GrannyInt32Member, "Dummy" },
    { GrannyEndMember },
};

// masked_combine is a concrete class, so we must create a slotted container
struct masked_combine_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(masked_combine);
};

granny_data_type_definition masked_combine::masked_combineTokenType[] =
{
    DECL_UID_MEMBER(masked_combine),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(masked_combine),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(masked_combine);
IMPL_CREATE_DEFAULT(masked_combine);


GSTATE
masked_combine::masked_combine(token_context*               Context,
                               granny_data_type_definition* TokenType,
                               void*                        TokenObject,
                               token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_masked_combineToken(0)
{
    IMPL_INIT_FROM_TOKEN(masked_combine);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInput(PoseEdge,  "From Pose");
        AddInput(PoseEdge,  "To Pose");
        AddInput(MaskEdge,  "Mask");
        AddOutput(PoseEdge, "Output");
    }
}


GSTATE
masked_combine::~masked_combine()
{
    DTOR_RELEASE_TOKEN(masked_combine);
}

bool GSTATE
masked_combine::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares masked_combineImpl*& Slot = // member
    GET_TOKEN_SLOT(masked_combine);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


void GSTATE
masked_combine::AcceptNodeVisitor(node_visitor* Visitor)
{
    Visitor->VisitNode(this);
}

granny_local_pose* GSTATE
masked_combine::SamplePoseOutput(granny_int32x OutputIdx,
                                 granny_real32 AtT,
                                 granny_real32 AllowedError,
                                 granny_pose_cache* PoseCache)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(PoseCache);

    INPUT_CONNECTION(0, From);
    INPUT_CONNECTION(1, To);
    INPUT_CONNECTION(2, Mask);
    if (!(FromNode && ToNode && MaskNode))
        return 0;

    gstate_character_instance* Instance = GetBoundCharacter();
    granny_model* Model = GetSourceModelForCharacter(Instance);
    granny_track_mask* Mask = GrannyNewTrackMask(0.0f, Model->Skeleton->BoneCount);

    granny_local_pose* FromPose = FromNode->SamplePoseOutput(FromEdge, AtT, AllowedError, PoseCache);
    granny_local_pose* ToPose   = ToNode->SamplePoseOutput(ToEdge, AtT, AllowedError, PoseCache);
    if (MaskNode->SampleMaskOutput(MaskEdge, AtT, Mask) && FromPose && ToPose)
    {
        GrannyModulationCompositeLocalPose(FromPose, 0, 1, Mask, ToPose);
    }

    if (ToPose)
        GrannyReleaseCachePose(PoseCache, ToPose);

    GrannyFreeTrackMask(Mask);
    return FromPose;
}

bool GSTATE
masked_combine::GetRootMotionVectors(granny_int32x OutputIdx,
                                     granny_real32 AtT,
                                     granny_real32 DeltaT,
                                     granny_real32* Translation,
                                     granny_real32* Rotation,
                                     bool Inverse)
{
    GStateAssert(GetBoundCharacter());

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

    node* FromNode = 0;
    node* ToNode  = 0;
    node* MaskNode    = 0;
    granny_int32x FromEdge  = -1;
    granny_int32x ToEdge    = -1;
    granny_int32x MaskEdge  = -1;

    GetInputConnection(0, &FromNode, &FromEdge);
    GetInputConnection(1, &ToNode,   &ToEdge);
    GetInputConnection(2, &MaskNode, &MaskEdge);

    if (!(FromNode && ToNode && MaskNode))
        return false;

    bool Success = false;
    gstate_character_instance* Instance = GetBoundCharacter();
    granny_model* Model = GetSourceModelForCharacter(Instance);
    granny_track_mask* Mask = GrannyNewTrackMask(0.0f, Model->Skeleton->BoneCount);
    if (MaskNode->SampleMaskOutput(MaskEdge, AtT, Mask))
    {
        // Might have some work to do here...
        granny_real32 RootWeight = GrannyGetTrackMaskBoneWeight(Mask, 0);
        if (RootWeight == 0)
        {
            Success = FromNode->GetRootMotionVectors(FromEdge, AtT, DeltaT,
                                                     Translation, Rotation,
                                                     Inverse);
        }
        else if (RootWeight == 1)
        {
            Success = ToNode->GetRootMotionVectors(ToEdge, AtT, DeltaT,
                                                   Translation, Rotation,
                                                   Inverse);
        }
        else
        {
            // Gots to blend.
            granny_real32 FT[3] = { 0, 0, 0 };
            granny_real32 FR[3] = { 0, 0, 0 };
            granny_real32 TT[3] = { 0, 0, 0 };
            granny_real32 TR[3] = { 0, 0, 0 };

            Success = (FromNode->GetRootMotionVectors(FromEdge, AtT, DeltaT,
                                                      FT, FR, Inverse) &&
                       ToNode->GetRootMotionVectors(ToEdge, AtT, DeltaT,
                                                    TT, TR, Inverse));
            if (Success)
            {
                {for (int Idx = 0; Idx < 3; ++Idx)
                {
                    Translation[Idx] = FT[Idx] * (1 - RootWeight) + TT[Idx] * RootWeight;
                    Rotation[Idx]    = FR[Idx] * (1 - RootWeight) + TR[Idx] * RootWeight;
                }}
            }
            else
            {
                memcpy(Translation, FT, sizeof(FT));
                memcpy(Rotation,    FR, sizeof(FR));
            }
        }
    }
    else
    {
        Success = FromNode->GetRootMotionVectors(FromEdge, AtT, DeltaT,
                                                 Translation, Rotation,
                                                 Inverse);
    }

    GrannyFreeTrackMask(Mask);
    return Success;
}

void GSTATE
masked_combine::Activate(granny_int32x OutputIdx,
                         granny_real32 AtT)
{
    GStateAssert(OutputIdx == 0);

    INPUT_CONNECTION(0, From);
    if (FromNode)
        FromNode->Activate(FromEdge, AtT);

    INPUT_CONNECTION(1, To);
    if (ToNode)
        ToNode->Activate(FromEdge, AtT);

    // todo: Not activating the mask node for now, revisit.
    // INPUT_CONNECTION(2, Mask);
    // if (!(FromNode && ToNode && MaskNode))
    //     return 0;
}

