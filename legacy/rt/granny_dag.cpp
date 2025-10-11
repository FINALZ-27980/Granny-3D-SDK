// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag.h"

#include "granny_fixed_allocator.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_track_mask.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

USING_GRANNY_NAMESPACE;

#undef SubsystemCode
#define SubsystemCode AnimationLogMessage

static fixed_allocator BlendDagNodeAllocator = {SizeOf(blend_dag_node)};


blend_dag_node_type GRANNY
GetBlendDagNodeType ( blend_dag_node const *Node )
{
    return Node->Type;
}


// Handy for knowing whether the ChildNodes list is even valid or not.
bool GRANNY
IsBlendDagLeafType ( blend_dag_node const *Node )
{
    if ( Node->Type < DagNodeType_OnePastLastLeafType )
    {
        Assert ( Node->NumChildNodes == 0 );
        return true;
    }
    else
    {
        return false;
    }
}



// Core functions.

static blend_dag_node *
CreateBlendDagNodeGeneric ( void )
{
    blend_dag_node *Node = (blend_dag_node *)AllocateFixed ( BlendDagNodeAllocator );

    // Only one good way to initialise Node->TypeUnion to sensible values - sorry for the perversion.
    ZeroStructure(*Node);

    Node->ParentNode = NULL;
    Node->NumChildNodes = 0;
    Node->ChildNodes = NULL;
    Node->Weight = 1.0f;
    Node->TrackMask = NULL;
    Node->AutoFreeTrackMask = false;
    Node->Type = DagNodeType_OnePastLast;

    return Node;
}


blend_dag_node *GRANNY
CreateBlendDagNodeAnimationBlend    ( model_instance *ModelInstance, bool AutoFree )
{
    CheckCondition ( ModelInstance != NULL, return NULL );

    blend_dag_node *Node = CreateBlendDagNodeGeneric();
    Node->Type = DagNodeType_Leaf_AnimationBlend;

    SetBlendDagNodeAnimationBlend ( Node, ModelInstance, DefaultLocalPoseFillThreshold, AutoFree );

    return Node;
}

blend_dag_node *GRANNY
CreateBlendDagNodeLocalPose     ( local_pose *LocalPose, bool AutoFree )
{
    CheckCondition ( LocalPose != NULL, return NULL );

    blend_dag_node *Node = CreateBlendDagNodeGeneric();
    Node->Type = DagNodeType_Leaf_LocalPose;

    SetBlendDagNodeLocalPose ( Node, LocalPose, AutoFree );

    return Node;
}

blend_dag_node *GRANNY
CreateBlendDagNodeCallback      ( blend_dag_leaf_callback_sample_callback *SampleCallback,
                                  blend_dag_leaf_callback_set_clock_callback *SetClockCallback,
                                  blend_dag_leaf_callback_motion_vectors_callback *MotionVectorsCallback,
                                  void *UserData )
{
    CheckCondition ( SampleCallback != NULL, return NULL );
    // SetClockCallback may be NULL if desired.
    // MotionVectorsCallback may be NULL if desired.

    blend_dag_node *Node = CreateBlendDagNodeGeneric();
    Node->Type = DagNodeType_Leaf_Callback;

    SetBlendDagNodeCallbacks ( Node, SampleCallback, SetClockCallback, MotionVectorsCallback, UserData );

    return Node;
}


blend_dag_node *GRANNY
CreateBlendDagNodeCrossfade     ( blend_dag_node *DagNode0, blend_dag_node *DagNode1, real32 WeightNone, real32 WeightAll, track_mask *TrackMask, bool AutoFreeTrackMask, int32x BoneCount )
{
    // If they're NULL, they're NULL. Some people like to set them up later.
    //Assert ( DagNode0 != NULL );
    //Assert ( DagNode1 != NULL );

    blend_dag_node *Node = CreateBlendDagNodeGeneric();
    Node->Type = DagNodeType_Node_Crossfade;

    SetBlendDagNodeCrossfade ( Node, WeightAll, WeightNone, TrackMask, AutoFreeTrackMask );

    if ( ( DagNode0 != NULL )&& ( DagNode1 != NULL ) )
    {
        blend_dag_node *Children[2] = { DagNode0, DagNode1 };
        SetBlendDagNodeChildren ( Node, 2, Children );
    }
    else
    {
        if ( DagNode0 != NULL )
        {
            AddBlendDagNodeChild ( Node, DagNode0 );
        }
        if ( DagNode1 != NULL )
        {
            AddBlendDagNodeChild ( Node, DagNode1 );
        }
    }

    return Node;
}

blend_dag_node *GRANNY
CreateBlendDagNodeWeightedBlend ( skeleton *ReferenceSkeleton, real32 FillThreshold, quaternion_mode QuaternionBlendingMode, int32x BoneCount )
{
    CheckCondition ( ReferenceSkeleton != NULL, return NULL );

    blend_dag_node *Node = CreateBlendDagNodeGeneric();
    Node->Type = DagNodeType_Node_WeightedBlend;

    SetBlendDagNodeWeightedBlend ( Node, ReferenceSkeleton, FillThreshold, QuaternionBlendingMode );

    return Node;
}


static void
SetBlendDagNodeAnimationBlend_Internal( blend_dag_node *Node, model_instance *ModelInstance, real32 FillThreshold, bool AutoFree )
{
    if ( Node->TypeUnion.AnimationBlend.AutoFreeModelInstance
        && ( Node->TypeUnion.AnimationBlend.ModelInstance != NULL )
        && ( Node->TypeUnion.AnimationBlend.ModelInstance != ModelInstance ) )
    {
        FreeModelInstance ( Node->TypeUnion.AnimationBlend.ModelInstance );
        Node->TypeUnion.AnimationBlend.ModelInstance = NULL;
    }

    Node->TypeUnion.AnimationBlend.ModelInstance = ModelInstance;
    Node->TypeUnion.AnimationBlend.FillThreshold = FillThreshold;
    Node->TypeUnion.AnimationBlend.AutoFreeModelInstance = AutoFree;
}

void GRANNY
SetBlendDagNodeAnimationBlend ( blend_dag_node *Node, model_instance *ModelInstance, real32 FillThreshold, bool AutoFree )
{
    CheckCondition ( Node->Type == DagNodeType_Leaf_AnimationBlend, return );
    CheckCondition ( ModelInstance != NULL, return );

    SetBlendDagNodeAnimationBlend_Internal(Node, ModelInstance, FillThreshold, AutoFree);
}


static void
SetBlendDagNodeLocalPose_Internal ( blend_dag_node *Node, local_pose *LocalPose, bool AutoFree )
{
    local_pose *&ThisLocalPose = Node->TypeUnion.LocalPose.LocalPose;
    bool32& ThisAutoFreeLocalPose = Node->TypeUnion.LocalPose.AutoFreeLocalPose;
    if ( ThisAutoFreeLocalPose
        && ( ThisLocalPose != NULL )
        && ( ThisLocalPose != LocalPose ) )
    {
        FreeLocalPose ( ThisLocalPose );
        ThisLocalPose = NULL;
    }
    ThisLocalPose = LocalPose;
    ThisAutoFreeLocalPose = AutoFree;
}

void GRANNY
SetBlendDagNodeLocalPose( blend_dag_node *Node, local_pose *LocalPose, bool AutoFree )
{
    CheckCondition ( Node->Type == DagNodeType_Leaf_LocalPose, return );
    CheckCondition ( LocalPose != NULL, return );

    SetBlendDagNodeLocalPose_Internal(Node, LocalPose, AutoFree);
}


static void
SetBlendDagNodeCallbacks_Internal ( blend_dag_node *Node,
                                    blend_dag_leaf_callback_sample_callback *SampleCallback,
                                    blend_dag_leaf_callback_set_clock_callback *SetClockCallback,
                                    blend_dag_leaf_callback_motion_vectors_callback *MotionVectorsCallback,
                                    void *UserData )
{
    Node->TypeUnion.Callback.SampleCallback = SampleCallback;
    Node->TypeUnion.Callback.SetClockCallback = SetClockCallback;
    Node->TypeUnion.Callback.MotionVectorsCallback = MotionVectorsCallback;
    Node->TypeUnion.Callback.UserData = UserData;
}


void GRANNY
SetBlendDagNodeCallbacks ( blend_dag_node *Node,
                           blend_dag_leaf_callback_sample_callback *SampleCallback,
                           blend_dag_leaf_callback_set_clock_callback *SetClockCallback,
                           blend_dag_leaf_callback_motion_vectors_callback *MotionVectorsCallback,
                           void *UserData )
{
    CheckCondition ( Node->Type == DagNodeType_Leaf_Callback, return );
    CheckCondition ( SampleCallback != NULL, return );
    // MotionVectorsCallback may be NULL
    // SetClockCallback may be NULL

    SetBlendDagNodeCallbacks_Internal(Node, SampleCallback,
                                      SetClockCallback, MotionVectorsCallback,
                                      UserData );
}

static void
SetBlendDagNodeCrossfade_Internal( blend_dag_node *Node, real32 WeightNone, real32 WeightAll,
                                   track_mask *TrackMask, bool AutoFreeTrackMask )
{
    if ( Node->TypeUnion.Crossfade.AutoFreeTrackMask
        && ( Node->TypeUnion.Crossfade.TrackMask != NULL )
        && ( Node->TypeUnion.Crossfade.TrackMask != TrackMask ) )
    {
        FreeTrackMask ( Node->TypeUnion.Crossfade.TrackMask );
        Node->TypeUnion.Crossfade.TrackMask = NULL;
    }

    Node->TypeUnion.Crossfade.WeightAll = WeightAll;
    Node->TypeUnion.Crossfade.WeightNone = WeightNone;
    Node->TypeUnion.Crossfade.TrackMask = TrackMask;
    Node->TypeUnion.Crossfade.AutoFreeTrackMask = AutoFreeTrackMask;

}

void GRANNY
SetBlendDagNodeCrossfade ( blend_dag_node *Node, real32 WeightNone, real32 WeightAll, track_mask *TrackMask, bool AutoFreeTrackMask )
{
    CheckCondition ( Node->Type == DagNodeType_Node_Crossfade, return );

    SetBlendDagNodeCrossfade_Internal(Node, WeightNone, WeightAll,
                                      TrackMask, AutoFreeTrackMask);
}


void GRANNY
SetBlendDagNodeCrossfadeWeights ( blend_dag_node *Node, real32 WeightNone, real32 WeightAll )
{
    Node->TypeUnion.Crossfade.WeightAll = WeightAll;
    Node->TypeUnion.Crossfade.WeightNone = WeightNone;
}


static void
SetBlendDagNodeWeightedBlend_Internal ( blend_dag_node *Node, skeleton *ReferenceSkeleton, real32 FillThreshold,
                                        quaternion_mode QuaternionBlendingMode )
{
    Node->TypeUnion.WeightedBlend.Skeleton = ReferenceSkeleton;
    Node->TypeUnion.WeightedBlend.QuatMode = QuaternionBlendingMode;
    Node->TypeUnion.WeightedBlend.FillThreshold = FillThreshold;
}

void GRANNY
SetBlendDagNodeWeightedBlend ( blend_dag_node *Node, skeleton *ReferenceSkeleton, real32 FillThreshold, quaternion_mode QuaternionBlendingMode )
{
    CheckCondition ( Node->Type == DagNodeType_Node_WeightedBlend, return );
    CheckCondition ( ReferenceSkeleton != NULL, return );

    SetBlendDagNodeWeightedBlend_Internal(Node, ReferenceSkeleton, FillThreshold, QuaternionBlendingMode);
}

model_instance *GRANNY
GetBlendDagNodeAnimationBlend       ( blend_dag_node *Node )
{
    CheckCondition ( Node->Type == DagNodeType_Leaf_AnimationBlend, return NULL );
    return Node->TypeUnion.AnimationBlend.ModelInstance;
}

local_pose *GRANNY
GetBlendDagNodeLocalPose            ( blend_dag_node *Node )
{
    CheckCondition ( Node->Type == DagNodeType_Leaf_LocalPose, return NULL );
    return Node->TypeUnion.LocalPose.LocalPose;
}

blend_dag_leaf_callback_sample_callback *GRANNY
GetBlendDagNodeCallbackSampleCallback       ( blend_dag_node *Node )
{
    CheckCondition ( Node->Type == DagNodeType_Leaf_Callback, return NULL );
    return Node->TypeUnion.Callback.SampleCallback;
}

blend_dag_leaf_callback_set_clock_callback *GRANNY
GetBlendDagNodeCallbackSetClockCallback     ( blend_dag_node *Node )
{
    CheckCondition ( Node->Type == DagNodeType_Leaf_Callback, return NULL );
    return Node->TypeUnion.Callback.SetClockCallback;
}

blend_dag_leaf_callback_motion_vectors_callback *GRANNY
GetBlendDagNodeCallbackMotionVectorsCallback        ( blend_dag_node *Node )
{
    CheckCondition ( Node->Type == DagNodeType_Leaf_Callback, return NULL );
    return Node->TypeUnion.Callback.MotionVectorsCallback;
}

void *GRANNY
GetBlendDagNodeCallbackUserData     ( blend_dag_node *Node )
{
    CheckCondition ( Node->Type == DagNodeType_Leaf_Callback, return NULL );
    return Node->TypeUnion.Callback.UserData;
}


track_mask *GRANNY
GetBlendDagNodeCrossfadeTrackMask   ( blend_dag_node *Node )
{
    CheckCondition ( Node->Type == DagNodeType_Node_Crossfade, return NULL );
    return Node->TypeUnion.Crossfade.TrackMask;
}

bool GRANNY
GetBlendDagNodeCrossfadeWeights(blend_dag_node const *Node,
                                real32* WeightNone,
                                real32* WeightAll)
{
    CheckCondition ( Node->Type == DagNodeType_Node_Crossfade, return false );
    CheckCondition ( WeightNone != 0 || WeightAll != 0, return false );

    if (WeightNone != 0)
        *WeightNone = Node->TypeUnion.Crossfade.WeightNone;

    if (WeightAll != 0)
        *WeightAll = Node->TypeUnion.Crossfade.WeightAll;

    return true;
}



skeleton *GRANNY
GetBlendDagNodeWeightedBlendSkeleton    ( blend_dag_node *Node )
{
    CheckCondition ( Node->Type == DagNodeType_Node_WeightedBlend, return NULL );
    return Node->TypeUnion.WeightedBlend.Skeleton;
}


void GRANNY
FreeBlendDagNode ( blend_dag_node *Node )
{
    switch ( Node->Type )
    {
    case DagNodeType_Leaf_AnimationBlend:
        SetBlendDagNodeAnimationBlend_Internal( Node, NULL, 0.0f, false );
        break;
    case DagNodeType_Leaf_LocalPose:
        SetBlendDagNodeLocalPose_Internal( Node, NULL, false );
        break;
    case DagNodeType_Leaf_Callback:
        // Nothing to do.
        break;
    case DagNodeType_Node_Crossfade:
        SetBlendDagNodeCrossfade_Internal ( Node, 0.0f, 1.0f, NULL, false );
        break;
    case DagNodeType_Node_WeightedBlend:
        // Nothing to do.
        break;
    default:
        InvalidCodePath ( "Unknown BlendDagNode->Type" );
        break;
    }

    SetBlendDagNodeResultTrackMask ( Node, NULL, false );
    if ( !IsBlendDagLeafType ( Node ) )
    {
        ClearBlendDagNodeChildren ( Node );
    }

    if ( Node->ParentNode != NULL )
    {
        RemoveBlendDagNodeChild ( Node->ParentNode, Node );
    }

    Node->Type = DagNodeType_OnePastLast;

    DeallocateFixed ( BlendDagNodeAllocator, Node );
}


// Before operating on the kids, unlink them.
static void
UnlinkBlendDagNodeChildren ( blend_dag_node *Node )
{
    Assert ( !IsBlendDagLeafType ( Node ) );
    for ( int32x i = 0; i < Node->NumChildNodes; i++ )
    {
        Assert ( Node->ChildNodes[i] != NULL );
        Assert ( Node->ChildNodes[i]->ParentNode == Node );
        Node->ChildNodes[i]->ParentNode = NULL;
    }
}

// After, relink them.
static void
RelinkBlendDagNodeChildren ( blend_dag_node *Node )
{
    Assert ( !IsBlendDagLeafType ( Node ) );
    for ( int32x i = 0; i < Node->NumChildNodes; i++ )
    {
        Assert ( Node->ChildNodes[i] != NULL );
        Assert ( Node->ChildNodes[i]->ParentNode == NULL );
        Node->ChildNodes[i]->ParentNode = Node;
    }
}



void GRANNY
ClearBlendDagNodeChildren ( blend_dag_node *Node )
{
    CheckCondition ( !IsBlendDagLeafType ( Node ), return );
    UnlinkBlendDagNodeChildren ( Node );

    DeallocateSafe ( Node->ChildNodes );
    Node->NumChildNodes = 0;
}

int32x GRANNY
GetBlendDagNodeChildrenCount ( blend_dag_node const *Node )
{
    CheckCondition ( !IsBlendDagLeafType ( Node ), return -1 );
    return Node->NumChildNodes;
}

void GRANNY
SetBlendDagNodeChildren ( blend_dag_node *Node, int32x NumChildren, blend_dag_node **ArrayOfChildren )
{
    CheckCondition ( !IsBlendDagLeafType ( Node ), return );
    CheckCondition ( ArrayOfChildren != NULL, return );
    UnlinkBlendDagNodeChildren ( Node );

    DeallocateSafe ( Node->ChildNodes );
    Node->ChildNodes = AllocateArray ( NumChildren, blend_dag_node *, AllocationBuilder );
    Node->NumChildNodes = NumChildren;

    for ( int32x i = 0; i < NumChildren; i++ )
    {
        if ( ( ArrayOfChildren[i] == NULL ) || ( ArrayOfChildren[i]->ParentNode != NULL ) )
        {
            // Too late to do much about it now - the tree is toast. But at least
            // we can warn the user.
            Log1 ( ErrorLogMessage, SubsystemCode, "SetBlendDagNodeChildren: Child %i is invalid", i );
        }
        Node->ChildNodes[i] = ArrayOfChildren[i];
    }

    RelinkBlendDagNodeChildren ( Node );
}

void GRANNY
SetBlendDagNodeChild ( blend_dag_node *Node, int32x ChildNumber, blend_dag_node *Child )
{
    CheckCondition ( !IsBlendDagLeafType ( Node ), return );
    CheckCondition ( ChildNumber < Node->NumChildNodes, return );
    CheckCondition ( Child != NULL, return );
    CheckCondition ( Child->ParentNode == NULL, return );

    UnlinkBlendDagNodeChildren ( Node );
    Node->ChildNodes[ChildNumber] = Child;
    RelinkBlendDagNodeChildren ( Node );
}

blend_dag_node *GRANNY
GetBlendDagNodeChild ( blend_dag_node *Node, int32x ChildNumber )
{
    CheckCondition ( !IsBlendDagLeafType ( Node ), return NULL );
    CheckCondition ( ChildNumber < Node->NumChildNodes, return NULL );
    return ( Node->ChildNodes[ChildNumber] );
}

void GRANNY
AddBlendDagNodeChild ( blend_dag_node *Node, blend_dag_node *Child )
{
    CheckCondition ( !IsBlendDagLeafType ( Node ), return );
    CheckCondition ( Child != NULL, return );
    CheckCondition ( Child->ParentNode == NULL, return );
    UnlinkBlendDagNodeChildren ( Node );

    blend_dag_node **OldChildNodes = Node->ChildNodes;

    Node->ChildNodes = AllocateArray ( Node->NumChildNodes + 1, blend_dag_node *, AllocationBuilder );
    for ( int32x i = 0; i < Node->NumChildNodes; i++ )
    {
        Node->ChildNodes[i] = OldChildNodes[i];
    }

    Node->ChildNodes[Node->NumChildNodes] = Child;

    ++Node->NumChildNodes;
    DeallocateSafe ( OldChildNodes );

    RelinkBlendDagNodeChildren ( Node );
}

void GRANNY
RemoveBlendDagNodeChild ( blend_dag_node *Node, blend_dag_node *Child )
{
    CheckCondition ( !IsBlendDagLeafType ( Node ), return );
    CheckCondition ( Child != NULL, return );
    CheckCondition ( Child->ParentNode == Node, return );
    UnlinkBlendDagNodeChildren ( Node );

    // Find the child.
    int32x ChildNumber = -1;
    for ( int32x i = 0; i < Node->NumChildNodes; i++ )
    {
        if ( Node->ChildNodes[i] == Child )
        {
            ChildNumber = i;
            break;
        }
    }
    if ( ChildNumber < 0 )
    {
        Log0 ( ErrorLogMessage, SubsystemCode, "RemoveBlendDagNodeChild couldn't find the specified child" );
    }
    else
    {
        // Found the child
        blend_dag_node **OldChildNodes = Node->ChildNodes;

        if ( Node->NumChildNodes == 1 )
        {
            // No children left.
            Node->NumChildNodes = 0;
            Node->ChildNodes = NULL;
        }
        else
        {
            Node->ChildNodes = AllocateArray ( Node->NumChildNodes - 1, blend_dag_node *, AllocationBuilder );
            int32x NewChildNum = 0;
            for ( int32x i = 0; i < Node->NumChildNodes; i++ )
            {
                if ( i != ChildNumber )
                {
                    Node->ChildNodes[NewChildNum++] = OldChildNodes[i];
                }
            }
            Assert ( NewChildNum == Node->NumChildNodes - 1 );
            --Node->NumChildNodes;
        }
        DeallocateSafe ( OldChildNodes );
    }

    RelinkBlendDagNodeChildren ( Node );
}



blend_dag_node *GRANNY
GetBlendDagNodeParent ( blend_dag_node *Node )
{
    return Node->ParentNode;
}


void GRANNY
SetBlendDagNodeResultTrackMask ( blend_dag_node *Node, track_mask *TrackMask, bool AutoFree )
{
    if ( Node->AutoFreeTrackMask
        && ( Node->TrackMask != NULL )
        && ( Node->TrackMask != TrackMask ) )
    {
        FreeTrackMask ( Node->TrackMask );
        Node->TrackMask = NULL;
    }
    Node->TrackMask = TrackMask;
    Node->AutoFreeTrackMask = AutoFree;
}

track_mask *GRANNY
GetBlendDagNodeResultTrackMask ( blend_dag_node *Node )
{
    return Node->TrackMask;
}

void GRANNY
SetBlendDagNodeResultWeight ( blend_dag_node *Node, real32 Weight )
{
    Node->Weight = Weight;
}

real32 GRANNY
GetBlendDagNodeResultWeight ( blend_dag_node const *Node )
{
    return Node->Weight;
}


#define WITHIN_BLEND_EPSILON_OF(val1,val2) ( AbsoluteValue ( val1 - val2 ) < TrackWeightEpsilon )


// And this is where all the work gets done!
// Return value is:
// true = LocalPoseResult is a local_pose that can be modified at will.
// false = LocalPoseResult comes from other source (e.g. the calling app) and may not be modified.
local_pose *GRANNY
SampleBlendDagTreeInternal (bool *ResultIsScratch,
                            blend_dag_node const *Node,
                            int32x BoneCount,
                            real32 AllowedError,
                            int32x const *SparseBoneArray,
                            pose_cache *PoseCache)
{
    CheckPointerNotNull(PoseCache, return NULL);
    // TODO: don't go any further if Weight==0

    // TODO: this is a nightmare of a case statement.  Rewrite to be a
    // little more clear
    switch ( Node->Type )
    {
        case DagNodeType_Leaf_AnimationBlend:
        {
            local_pose *ThisLocalPose = GetNewLocalPose(*PoseCache, BoneCount);
            // TODO: add a granny_quaternion_mode to the AnimationBlend and obey it?
            ThisLocalPose->FillThreshold = Node->TypeUnion.AnimationBlend.FillThreshold;
            SampleModelAnimationsLODSparse ( *Node->TypeUnion.AnimationBlend.ModelInstance, 0, BoneCount, *ThisLocalPose, AllowedError, SparseBoneArray );
            *ResultIsScratch = true;
            return ThisLocalPose;
        } //break;

        case DagNodeType_Leaf_LocalPose:
        {
            if ( SparseBoneArray != NULL )
            {
                // Need to copy only the required bones.
                local_pose * const&ThisLocalPose = Node->TypeUnion.LocalPose.LocalPose;
                Assert ( ThisLocalPose != NULL );
                Assert ( BoneCount <= ThisLocalPose->BoneCount );

                local_pose *ResultLocalPose = GetNewLocalPose(*PoseCache, BoneCount);
                local_pose_transform *DstTransform = ResultLocalPose->Transforms;
                {for ( int32x BoneIndex = 0; BoneIndex < BoneCount; BoneIndex++ )
                {
                    int32x SkeletonBoneIndex = SparseBoneArray[BoneIndex];
                    local_pose_transform const *SrcTransform = &ThisLocalPose->Transforms[SkeletonBoneIndex];
                    Copy32 ( sizeof(*DstTransform) / 4, SrcTransform, DstTransform );
                    ++DstTransform;
                }}
                *ResultIsScratch = true;
                return ResultLocalPose;
            }
            else
            {
                local_pose * const&ThisLocalPose = Node->TypeUnion.LocalPose.LocalPose;
                Assert ( ThisLocalPose != NULL );
                Assert ( BoneCount <= ThisLocalPose->BoneCount );
                *ResultIsScratch = false;
                return ThisLocalPose;
            }
        } //break;

        case DagNodeType_Leaf_Callback:
            *ResultIsScratch = false;
            return ( Node->TypeUnion.Callback.SampleCallback ( Node->TypeUnion.Callback.UserData, BoneCount, AllowedError, SparseBoneArray ) );

        case DagNodeType_Node_Crossfade:
        {
            Assert ( Node->NumChildNodes <= 2 );
            Assert ( Node->NumChildNodes >= 1 );
            if ( Node->NumChildNodes == 1 )
            {
                // Only one child - curious, but not lethal.
                return SampleBlendDagTreeInternal ( ResultIsScratch, Node->ChildNodes[0], BoneCount, AllowedError, SparseBoneArray, PoseCache );
            }

            if ( Node->TypeUnion.Crossfade.TrackMask == NULL )
            {
                // Only using WeightAll
                if ( Node->TypeUnion.Crossfade.WeightAll < BlendEffectivelyZero )
                {
                    return SampleBlendDagTreeInternal ( ResultIsScratch, Node->ChildNodes[0], BoneCount, AllowedError, SparseBoneArray, PoseCache );
                }
                else if ( Node->TypeUnion.Crossfade.WeightAll > BlendEffectivelyOne )
                {
                    return SampleBlendDagTreeInternal ( ResultIsScratch, Node->ChildNodes[1], BoneCount, AllowedError, SparseBoneArray, PoseCache );
                }
            }
            else
            {
                // Using WeightAll and WeightNone
                if ( ( Node->TypeUnion.Crossfade.WeightNone < BlendEffectivelyZero ) &&
                     ( Node->TypeUnion.Crossfade.WeightAll  < BlendEffectivelyZero ) )
                {
                    return SampleBlendDagTreeInternal ( ResultIsScratch, Node->ChildNodes[0], BoneCount, AllowedError, SparseBoneArray, PoseCache );
                }
                else if ( ( Node->TypeUnion.Crossfade.WeightNone > BlendEffectivelyOne ) &&
                          ( Node->TypeUnion.Crossfade.WeightAll  > BlendEffectivelyOne ) )
                {
                    return SampleBlendDagTreeInternal ( ResultIsScratch, Node->ChildNodes[1], BoneCount, AllowedError, SparseBoneArray, PoseCache );
                }
            }

            bool Pose0Scratch;
            bool Pose1Scratch;
            local_pose *LocalPose0 = SampleBlendDagTreeInternal ( &Pose0Scratch, Node->ChildNodes[0],
                                                                  BoneCount, AllowedError,
                                                                  SparseBoneArray, PoseCache );
            local_pose *LocalPose1 = SampleBlendDagTreeInternal ( &Pose1Scratch, Node->ChildNodes[1],
                                                                  BoneCount, AllowedError,
                                                                  SparseBoneArray, PoseCache );
            Assert ( LocalPose0 != NULL );
            Assert ( LocalPose1 != NULL );
            Assert ( LocalPose0->BoneCount >= BoneCount );
            Assert ( LocalPose1->BoneCount >= BoneCount );

            if ( Pose0Scratch )
            {
                // Can just composite onto Pose0.
                ModulationCompositeLocalPoseSparse (
                    *LocalPose0,
                    Node->TypeUnion.Crossfade.WeightNone,
                    Node->TypeUnion.Crossfade.WeightAll,
                    Node->TypeUnion.Crossfade.TrackMask,
                    *LocalPose1,
                    SparseBoneArray );
                *ResultIsScratch = true;

                // Free local pose 1 iff it's scratch...
                if (Pose1Scratch)
                    ReleaseCachePose(*PoseCache, LocalPose1);

                return LocalPose0;
            }
            else if ( Pose1Scratch )
            {
                // We can composite onto Pose1 by flipping the sense of the blend numbers.
                ModulationCompositeLocalPoseSparse (
                    *LocalPose1,
                    1.0f - Node->TypeUnion.Crossfade.WeightNone,
                    1.0f - Node->TypeUnion.Crossfade.WeightAll,
                    Node->TypeUnion.Crossfade.TrackMask,
                    *LocalPose0,
                    SparseBoneArray );
                *ResultIsScratch = true;

                // Free local pose 0 iff it's scratch...
                if (Pose0Scratch)
                    ReleaseCachePose(*PoseCache, LocalPose0);

                return LocalPose1;
            }
            else
            {
                // Neither source pose can be modified - going to have to copy one of them then blend to it.
                local_pose* ThisLocalPose = GetNewLocalPose(*PoseCache, BoneCount);

                Copy(sizeof(ThisLocalPose->Transforms[0]) * BoneCount,
                     LocalPose0->Transforms,
                     ThisLocalPose->Transforms);

                ModulationCompositeLocalPoseSparse (
                    *ThisLocalPose,
                    Node->TypeUnion.Crossfade.WeightNone,
                    Node->TypeUnion.Crossfade.WeightAll,
                    Node->TypeUnion.Crossfade.TrackMask,
                    *LocalPose1,
                    SparseBoneArray );

                *ResultIsScratch = true;
                return ThisLocalPose;
            }
        }

        case DagNodeType_Node_WeightedBlend:
        {
            if ( Node->NumChildNodes == 1 )
            {
                // Well that's dull.
                return SampleBlendDagTreeInternal ( ResultIsScratch, Node->ChildNodes[0],
                                                    BoneCount, AllowedError,
                                                    SparseBoneArray, PoseCache );
            }
            else
            {
                local_pose *ThisLocalPose = GetNewLocalPose(*PoseCache, BoneCount);

                ThisLocalPose->FillThreshold = Node->TypeUnion.WeightedBlend.FillThreshold;
                BeginLocalPoseAccumulation ( *ThisLocalPose, 0, BoneCount, SparseBoneArray );
                for ( int32x ChildNum = 0; ChildNum < Node->NumChildNodes; ChildNum++ )
                {
                    blend_dag_node *Child = Node->ChildNodes[ChildNum];

                    if ( WITHIN_BLEND_EPSILON_OF ( Child->Weight, 0.0f ) )
                    {
                        // Ignore this child.
                    }
                    else
                    {
                        if ( Child->TrackMask != NULL )
                        {
                            Assert ( Child->TrackMask->BoneWeightCount >= BoneCount );
                        }

                        bool ChildIsScratch;
                        local_pose *ChildLocalPose = SampleBlendDagTreeInternal ( &ChildIsScratch, Child,
                                                                                  BoneCount, AllowedError,
                                                                                  SparseBoneArray, PoseCache );
                        Assert ( ChildLocalPose->BoneCount >= BoneCount );

                        for ( int32x BoneNum = 0; BoneNum < BoneCount; BoneNum++ )
                        {
                            real32 Weight = Child->Weight;
                            int32x SkeletonBoneNum = BoneNum;
                            if ( SparseBoneArray != NULL )
                            {
                                SkeletonBoneNum = SparseBoneArray[BoneNum];
                            }

                            if ( Child->TrackMask != NULL )
                            {
                                Weight *= Child->TrackMask->BoneWeights[SkeletonBoneNum];
                            }
                            AccumulateLocalTransform (
                                *ThisLocalPose,
                                BoneNum, SkeletonBoneNum,
                                Weight,
                                *Node->TypeUnion.WeightedBlend.Skeleton,
                                Node->TypeUnion.WeightedBlend.QuatMode,
                                ChildLocalPose->Transforms[BoneNum].Transform );
                        }

                        if (ChildIsScratch && ChildLocalPose)
                            ReleaseCachePose(*PoseCache, ChildLocalPose);
                    }
                }
                // dmmnote: this seems like it should be in the if case, investigate
                EndLocalPoseAccumulationLOD( *ThisLocalPose, 0, BoneCount, 0, BoneCount,
                                             *Node->TypeUnion.WeightedBlend.Skeleton, AllowedError,
                                             SparseBoneArray );

                *ResultIsScratch = true;
                return ThisLocalPose;
            }
        }

        default:
            InvalidCodePath ( "Unknown blend_dag_node type!" );
            return NULL;
    }
}


local_pose *GRANNY
SampleBlendDagTreeLODSparseReentrant(blend_dag_node const *Node,
                                     int32x BoneCount,
                                     real32 AllowedError,
                                     int32x const *SparseBoneArray,
                                     pose_cache *PoseCache)
{
    CheckPointerNotNull(PoseCache, return NULL);
    CheckPointerNotNull(Node, return NULL);

    bool ResultIgnored;
    local_pose* ReturnPose =
        SampleBlendDagTreeInternal ( &ResultIgnored, Node,
                                     BoneCount, AllowedError,
                                     SparseBoneArray, PoseCache );

    // First, let's make sure that the pose won't be freed as soon as we return it.  This
    // is already sketchy enough, we don't want to make it worse.  The semantics for the
    // Blend Dag v1 is that poses are valid until the pose cache is touched again.
    //
    // This assert guards the assumption that nothing else was calling into the cache with
    // higher bone counts while the traversal started by the call above was in progress.
    // This is certainly true for all internal granny nodes, but could theoretically be
    // incorrect for callbacks?
    Assert(!ReturnPose || !PoseWillBeImmediatelyFreed(*PoseCache, ReturnPose));

    // Ok, move all the poses to the free list for reclaimation
    ClearPoseCache(*PoseCache);

    return ReturnPose;
}


void GRANNY
GetBlendDagTreeMotionVectors(blend_dag_node const *Node,
                             real32 SecondsElapsed,
                             real32 *ResultTranslation,
                             real32 *ResultRotation,
                             bool Inverse)
{
    CheckPointerNotNull ( ResultRotation, return );
    CheckPointerNotNull ( ResultTranslation, return );

    // TODO: don't go any further if Weight==0
    switch ( Node->Type )
    {
    case DagNodeType_Leaf_AnimationBlend:
        {
            GetRootMotionVectors ( *Node->TypeUnion.AnimationBlend.ModelInstance, SecondsElapsed, ResultTranslation, ResultRotation, Inverse );
            return;
        } //break;

    case DagNodeType_Leaf_LocalPose:
        // Poses don't do motion extraction!
        VectorZero3(ResultTranslation);
        VectorZero3(ResultRotation);
        return;

    case DagNodeType_Leaf_Callback:
        if ( Node->TypeUnion.Callback.MotionVectorsCallback == NULL )
        {
            // No callback, so no root motion vectors.
            VectorZero3(ResultTranslation);
            VectorZero3(ResultRotation);
        }
        else
        {
            Node->TypeUnion.Callback.MotionVectorsCallback (Node->TypeUnion.Callback.UserData, SecondsElapsed,
                                                            ResultTranslation, ResultRotation, Inverse );
        }
        return;

    case DagNodeType_Node_Crossfade:
        {
            Assert ( Node->NumChildNodes <= 2 );
            Assert ( Node->NumChildNodes >= 1 );
            if ( Node->NumChildNodes == 1 )
            {
                // Only one child - curious, but not lethal.
                GetBlendDagTreeMotionVectors ( Node->ChildNodes[0], SecondsElapsed, ResultTranslation, ResultRotation, Inverse );
                return;
            }


            real32 Weight1;
            if ( Node->TypeUnion.Crossfade.TrackMask != NULL )
            {
                // Taken from the root bone - naturally!
                real32 TrackMaskWeight = Node->TypeUnion.Crossfade.TrackMask->BoneWeights[0];
                Weight1 = ( Node->TypeUnion.Crossfade.WeightNone * ( 1.0f - TrackMaskWeight ) ) + ( Node->TypeUnion.Crossfade.WeightAll * TrackMaskWeight );
            }
            else
            {
                Weight1 = Node->TypeUnion.Crossfade.WeightAll;
            }


            if ( Weight1 < BlendEffectivelyZero )
            {
                // Entirely child 0.
                GetBlendDagTreeMotionVectors ( Node->ChildNodes[0], SecondsElapsed, ResultTranslation, ResultRotation, Inverse );
                return;
            }
            else if ( Weight1 > BlendEffectivelyOne )
            {
                // Entirely child 1.
                GetBlendDagTreeMotionVectors ( Node->ChildNodes[1], SecondsElapsed, ResultTranslation, ResultRotation, Inverse );
                return;
            }


            real32 Weight0 = 1.0f - Weight1;

            GetBlendDagTreeMotionVectors ( Node->ChildNodes[0], SecondsElapsed, ResultTranslation, ResultRotation, Inverse );
            VectorScale3 ( ResultTranslation, Weight0 );
            VectorScale3 ( ResultRotation,    Weight0 );
            triple ThisTranslation;
            triple ThisRotation;
            GetBlendDagTreeMotionVectors ( Node->ChildNodes[1], SecondsElapsed, ThisTranslation, ThisRotation, Inverse );
            ScaleVectorAdd3 ( ResultTranslation, Weight1, ThisTranslation );
            ScaleVectorAdd3 ( ResultRotation,    Weight1, ThisRotation );
            // ...and we know those two weights add to 1.0, so no need to normalise.
            return;
        } // break;

    case DagNodeType_Node_WeightedBlend:
        if ( Node->NumChildNodes == 1 )
        {
            // Well that's dull.
            GetBlendDagTreeMotionVectors ( Node->ChildNodes[0], SecondsElapsed, ResultTranslation, ResultRotation, Inverse );
        }
        else
        {
            real32 TotalWeight = 0.0f;
            VectorZero3(ResultTranslation);
            VectorZero3(ResultRotation);
            for ( int32x ChildNum = 0; ChildNum < Node->NumChildNodes; ChildNum++ )
            {
                blend_dag_node *Child = Node->ChildNodes[ChildNum];
                real32 Weight = Child->Weight;
                if ( Child->TrackMask != NULL )
                {
                    // Modulate by the root bone weight.
                    Weight *= Child->TrackMask->BoneWeights[0];
                }

                if ( WITHIN_BLEND_EPSILON_OF ( Weight, 0.0f ) )
                {
                    // Ignore this child.
                }
                else
                {
                    triple ThisTranslation;
                    triple ThisRotation;
                    GetBlendDagTreeMotionVectors ( Child, SecondsElapsed, ThisTranslation, ThisRotation, Inverse );
                    ScaleVectorAdd3 ( ResultTranslation, Weight, ThisTranslation );
                    ScaleVectorAdd3 ( ResultRotation,    Weight, ThisRotation );
                    TotalWeight += Weight;
                }
            }

            if(TotalWeight > TrackWeightEpsilon)
            {
                real32 const InverseTotalWeight = 1.0f / TotalWeight;
                VectorScale3(ResultTranslation, InverseTotalWeight);
                VectorScale3(ResultRotation, InverseTotalWeight);
            }
        }
        break;  // Note that this one is necessary!

    default:
        InvalidCodePath ( "Unknown blend_dag_node type!" );
        break;
    }
}



void GRANNY
UpdateBlendDagTreeMatrix(blend_dag_node const *Node,
                         real32 SecondsElapsed,
                         real32 const *ModelMatrix4x4,
                         real32 *DestMatrix4x4, bool Inverse)
{
    CheckPointerNotNull ( ModelMatrix4x4, return );
    CheckPointerNotNull ( DestMatrix4x4, return );
    triple DeltaTranslation;
    triple DeltaRotation;
    GetBlendDagTreeMotionVectors(Node, SecondsElapsed,
                                 DeltaTranslation, DeltaRotation, Inverse);
    ApplyRootMotionVectorsToMatrix(ModelMatrix4x4,
                                   DeltaTranslation, DeltaRotation,
                                   DestMatrix4x4);
}



void GRANNY
SetBlendDagTreeClock(blend_dag_node *RootNode, real32 NewClock)
{
    switch ( RootNode->Type )
    {
    case DagNodeType_Leaf_AnimationBlend:
        SetModelClock ( *RootNode->TypeUnion.AnimationBlend.ModelInstance, NewClock );
        break;
    case DagNodeType_Leaf_LocalPose:
        // Nothing to do.
        break;
    case DagNodeType_Leaf_Callback:
        if ( RootNode->TypeUnion.Callback.SetClockCallback != NULL )
        {
            RootNode->TypeUnion.Callback.SetClockCallback ( RootNode->TypeUnion.Callback.UserData, NewClock );
        }
        break;
    case DagNodeType_Node_Crossfade:
    case DagNodeType_Node_WeightedBlend:
        {for ( int32x ChildNum = 0; ChildNum < RootNode->NumChildNodes; ChildNum++ )
        {
            blend_dag_node *Child = RootNode->ChildNodes[ChildNum];
            SetBlendDagTreeClock(Child, NewClock);
        }}
        break;
    default:
        InvalidCodePath ( "Unknown blend_dag_node type!" );
        break;
    }
}




void GRANNY
BlendDagNodeAnimationBlendFreeCompletedControls ( blend_dag_node *Node )
{
    CheckCondition( Node->Type == DagNodeType_Leaf_AnimationBlend, return );
    FreeCompletedModelControls ( *(Node->TypeUnion.AnimationBlend.ModelInstance) );
}


// Utility functions.

void GRANNY
GetBlendDagNodeChildren ( blend_dag_node *Node, int32x MaxArraySize, blend_dag_node **ArrayOfChildren )
{
    CheckCondition( !IsBlendDagLeafType ( Node ), return );
    CheckPointerNotNull ( ArrayOfChildren, return );
    int32x NumChildren = Node->NumChildNodes;
    if ( NumChildren > MaxArraySize )
    {
        Log0 ( WarningLogMessage, SubsystemCode, "GetBlendDagNodeChildren: Array not large enough to hold all children" );
        NumChildren = MaxArraySize;
    }
    for ( int32x i = 0; i < NumChildren; i++ )
    {
        ArrayOfChildren[i] = Node->ChildNodes[i];
    }
}

blend_dag_node *GRANNY
CreateBlendDagNodeWeightedBlendChildren(skeleton *ReferenceSkeleton,
                                        real32 FillThreshold,
                                        quaternion_mode QuaternionBlendingMode,
                                        int32x BoneCount,
                                        int32x NumChildren,
                                        blend_dag_node **ArrayOfChildren )
{
    CheckPointerNotNull ( ArrayOfChildren, return NULL );
    blend_dag_node *Node = CreateBlendDagNodeWeightedBlend ( ReferenceSkeleton, FillThreshold, QuaternionBlendingMode, BoneCount );
    SetBlendDagNodeChildren ( Node, NumChildren, ArrayOfChildren );
    return Node;
}

void GRANNY
FreeBlendDagEntireTree ( blend_dag_node *Node )
{
    if ( !IsBlendDagLeafType ( Node ) )
    {
        UnlinkBlendDagNodeChildren ( Node );
        // It's a parent - free the kids.
        for ( int32x i = 0; i < Node->NumChildNodes; i++ )
        {
            FreeBlendDagNode ( Node->ChildNodes[i] );
        }
        // And then pretend we didn't have any, so that FreeBlendDagNode doesn't freak.
        DeallocateSafe ( Node->ChildNodes );
        Node->NumChildNodes = 0;
    }
    FreeBlendDagNode ( Node );
}


void GRANNY
BlendDagFreeCompletedControlsEntireTree(blend_dag_node *Node)
{
    if ( !IsBlendDagLeafType ( Node ) )
    {
        for ( int32x i = 0; i < Node->NumChildNodes; i++ )
        {
            BlendDagFreeCompletedControlsEntireTree ( Node->ChildNodes[i] );
        }
    }
    else if ( Node->Type == DagNodeType_Leaf_AnimationBlend )
    {
        BlendDagNodeAnimationBlendFreeCompletedControls ( Node );
    }
}


blend_dag_node *GRANNY
DuplicateBlendDagTree(blend_dag_node const *CurrentNode,
                      blend_dag_node const **SourceNodeList,
                      blend_dag_node **DestNodeList,
                      int32 SizeOfNodeList,
                      bool AutoFreeCreatedModelInstances)
{
    if ( SizeOfNodeList > 0 )
    {
        CheckPointerNotNull ( SourceNodeList, return NULL );
        CheckPointerNotNull ( DestNodeList, return NULL );
    }

    blend_dag_node *NewNode = NULL;
    bool CopyTheKids = false;
    switch ( CurrentNode->Type )
    {
    case DagNodeType_Leaf_AnimationBlend:
    {
        model_instance *ModelInstance = InstantiateModel ( *CurrentNode->TypeUnion.AnimationBlend.ModelInstance->Model );
        NewNode = CreateBlendDagNodeAnimationBlend ( ModelInstance, AutoFreeCreatedModelInstances );
        NewNode->TypeUnion.AnimationBlend.FillThreshold = CurrentNode->TypeUnion.AnimationBlend.FillThreshold;
        break;
    }
    case DagNodeType_Leaf_LocalPose:
    {
        if ( CurrentNode->TypeUnion.LocalPose.LocalPose != NULL )
        {
            if ( CurrentNode->TypeUnion.LocalPose.AutoFreeLocalPose )
            {
                // Since it's auto-freeing, and we don't refcount, we need to copy this local pose.
                local_pose *NewLocal = NewLocalPose ( CurrentNode->TypeUnion.LocalPose.LocalPose->BoneCount );
                CopyLocalPose ( *CurrentNode->TypeUnion.LocalPose.LocalPose, *NewLocal );
                SetBlendDagNodeLocalPose ( NewNode, NewLocal, true );
            }
            else
            {
                // They can both just reference the same one.
                NewNode = CreateBlendDagNodeLocalPose ( CurrentNode->TypeUnion.LocalPose.LocalPose, false );
            }
        }
        else
        {
            // That's very strange - no local pose.
            NewNode = CreateBlendDagNodeLocalPose ( NULL, false );
        }
        break;
    }
    case DagNodeType_Leaf_Callback:
    {
        NewNode = CreateBlendDagNodeCallback ( CurrentNode->TypeUnion.Callback.SampleCallback, CurrentNode->TypeUnion.Callback.SetClockCallback, CurrentNode->TypeUnion.Callback.MotionVectorsCallback, CurrentNode->TypeUnion.Callback.UserData );
        break;
    }
    case DagNodeType_Node_Crossfade:
    {
        // Copy the kids.
        Assert ( CurrentNode->NumChildNodes <= 2 );
        blend_dag_node *Child[2];
        Child[0] = NULL;
        Child[1] = NULL;
        {for ( int i = 0; i < CurrentNode->NumChildNodes; i++ )
        {
            Child[i] = DuplicateBlendDagTree ( CurrentNode->ChildNodes[i], SourceNodeList, DestNodeList, SizeOfNodeList, AutoFreeCreatedModelInstances );
        }}

        // Copy the crossfade track mask.
        track_mask *NewCrossfadeTrackMask = NULL;
        if ( CurrentNode->TypeUnion.Crossfade.TrackMask != NULL )
        {
            if ( CurrentNode->TypeUnion.Crossfade.AutoFreeTrackMask )
            {
                // It's autofreed, so it just needs to be copied.
                NewCrossfadeTrackMask = CloneTrackMask(*CurrentNode->TypeUnion.Crossfade.TrackMask);
            }
            else
            {
                // Can share.
                NewCrossfadeTrackMask = CurrentNode->TypeUnion.Crossfade.TrackMask;
            }
        }

        // And we don't copy the kids the general way, because we need to pass them to the creation function.
        int32x BoneCount = 0;
        NewNode = CreateBlendDagNodeCrossfade (
            Child[0], Child[1],
            CurrentNode->TypeUnion.Crossfade.WeightNone,
            CurrentNode->TypeUnion.Crossfade.WeightAll,
            NewCrossfadeTrackMask,
            CurrentNode->TypeUnion.Crossfade.AutoFreeTrackMask != 0,
            BoneCount );
        break;
    }
    case DagNodeType_Node_WeightedBlend:
    {
        int32x BoneCount = 0;
        NewNode = CreateBlendDagNodeWeightedBlend (
                    CurrentNode->TypeUnion.WeightedBlend.Skeleton,
                    CurrentNode->TypeUnion.WeightedBlend.FillThreshold,
                    CurrentNode->TypeUnion.WeightedBlend.QuatMode,
                    BoneCount );

        CopyTheKids = true;
        break;
    }
    default:
        InvalidCodePath ( "Unknown BlendDAG type" );
        break;
    }

    if ( NewNode == NULL )
    {
        // That was bad.
        Assert ( !"Failed to copy BlendDAG node" );
        return NULL;
    }

    NewNode->Weight = CurrentNode->Weight;

    NewNode->TrackMask = NULL;
    NewNode->AutoFreeTrackMask = false;
    if ( CurrentNode->TrackMask != NULL )
    {
        if ( CurrentNode->AutoFreeTrackMask )
        {
            // In general, if you are going to be copying DAG trees, you do not want to AutoFree track masks -
            // there is no reference counting system, they just have to get duplicated. Normally you
            // would not use AutoFreeing track masks though, since they usually come from files or other
            // static sources like that.
            NewNode->TrackMask = CloneTrackMask(*CurrentNode->TrackMask);
            NewNode->AutoFreeTrackMask = true;
        }
        else
        {
            NewNode->TrackMask = CurrentNode->TrackMask;
            NewNode->AutoFreeTrackMask = false;
        }
    }


    if ( CopyTheKids )
    {
        Assert ( !IsBlendDagLeafType ( NewNode ) );
        UnlinkBlendDagNodeChildren ( NewNode );
        DeallocateSafe ( NewNode->ChildNodes );
        NewNode->ChildNodes = AllocateArray ( CurrentNode->NumChildNodes, blend_dag_node *, AllocationBuilder );
        NewNode->NumChildNodes = CurrentNode->NumChildNodes;
        {for ( int32x ChildNum = 0; ChildNum < CurrentNode->NumChildNodes; ChildNum++ )
        {
            NewNode->ChildNodes[ChildNum] = DuplicateBlendDagTree ( CurrentNode->ChildNodes[ChildNum], SourceNodeList, DestNodeList, SizeOfNodeList, AutoFreeCreatedModelInstances );
        }}
        RelinkBlendDagNodeChildren ( NewNode );
    }

    // We the app interested in this node?
    for ( int32x NodeNum = 0; NodeNum < SizeOfNodeList; NodeNum++ )
    {
        if ( SourceNodeList[NodeNum] == CurrentNode )
        {
            DestNodeList[NodeNum] = NewNode;
        }
    }

    return NewNode;
}




// Handy debug functions
bool GRANNY
IsBlendDagNodeValid ( blend_dag_node const *Node,
                      char const** ReasonFailed,
                      blend_dag_node const **NodeFailed )
{
    // Here's where all the paranoia happens.
    // I use an Assert where the error can only really happen if I code something wrong.
    // I use the full reporting mechanism where the error can be because the calling app did it wrong.

    char const* DummyReasonFailed = NULL;
    blend_dag_node const *DummyNodeFailed = NULL;
    if ( ReasonFailed == NULL )
    {
        ReasonFailed = &DummyReasonFailed;
    }
    if ( NodeFailed == NULL )
    {
        NodeFailed = &DummyNodeFailed;
    }


    if ( Node->Type != DagNodeType_Leaf_Callback )
    {
        // Nothing to check.
    }


    switch ( Node->Type )
    {
    case DagNodeType_Leaf_AnimationBlend:
        if ( Node->TypeUnion.AnimationBlend.ModelInstance == NULL )
        {
            *ReasonFailed = "AnimationBlend node has a NULL ModelInstance";
            *NodeFailed = Node;
            return false;
        }
        break;

    case DagNodeType_Leaf_LocalPose:
        if ( Node->TypeUnion.LocalPose.LocalPose == NULL )
        {
            *ReasonFailed = "LocalPose node has a NULL LocalPose";
            *NodeFailed = Node;
            return false;
        }
        break;

    case DagNodeType_Leaf_Callback:
        if ( Node->TypeUnion.Callback.SampleCallback == NULL )
        {
            *ReasonFailed = "Callback node has a NULL SampleCallback";
            *NodeFailed = Node;
            return false;
        }
        // SetClockCallback may be NULL.
        // MotionVevtorsCallback may be NULL.
        break;

    case DagNodeType_Node_Crossfade:
        if ( Node->NumChildNodes != 2 )
        {
            *ReasonFailed = "Crossfade node does not have exactly two children";
            *NodeFailed = Node;
            return false;
        }
        if ( Node->TypeUnion.Crossfade.TrackMask != NULL )
        {
            // Nothing.
        }
        break;

    case DagNodeType_Node_WeightedBlend:
        if ( Node->TypeUnion.WeightedBlend.Skeleton == NULL )
        {
            *ReasonFailed = "WeightedBlend node has a NULL skeleton";
            *NodeFailed = Node;
            return false;
        }
        else
        {
            // Nothing.
        }
        break;

    default:
        *ReasonFailed = "Unknown node type";
        *NodeFailed = Node;
        return false;
    }



    if ( IsBlendDagLeafType ( Node ) )
    {
        if ( ( Node->NumChildNodes != 0 ) || ( Node->ChildNodes != NULL ) )
        {
            *ReasonFailed = "Leaf type node has children";
            *NodeFailed = Node;
            return false;
        }
    }
    else
    {
        if ( Node->NumChildNodes > 0 )
        {
            Assert ( Node->ChildNodes != NULL );
            for ( int32x i = 0; i < Node->NumChildNodes; i++ )
            {
                blend_dag_node *Child = Node->ChildNodes[i];
                if ( Child == NULL )
                {
                    *ReasonFailed = "Node has a NULL child";
                    *NodeFailed = Node;
                    return false;
                }
                else
                {
                    if ( Child->ParentNode != Node )
                    {
                        *ReasonFailed = "A child node does not point back to its parent";
                        *NodeFailed = Node;
                        return false;
                    }
                }
            }
        }
        else
        {
            Assert ( Node->ChildNodes == NULL );
        }
    }

    return true;
}


bool GRANNY
IsBlendDagTreeValid(blend_dag_node const *Node,
                    char const**ReasonFailed,
                    blend_dag_node const **NodeFailed)
{
    if ( !IsBlendDagLeafType ( Node ) )
    {
        for ( int32x i = 0; i < Node->NumChildNodes; i++ )
        {
            if ( !IsBlendDagTreeValid ( Node->ChildNodes[i], ReasonFailed, NodeFailed ) )
            {
                return false;
            }
        }
    }
    return IsBlendDagNodeValid ( Node, ReasonFailed, NodeFailed );
}



int32x GRANNY
FindBlendDagTreeDepth(blend_dag_node const *Node)
{
    int32x Depth = 1;
    if ( !IsBlendDagLeafType ( Node ) )
    {
        for ( int32x i = 0; i < Node->NumChildNodes; i++ )
        {
            int32x ChildDepth = 1 + FindBlendDagTreeDepth ( Node->ChildNodes[i] );
            if ( Depth < ChildDepth )
            {
                Depth = ChildDepth;
            }
        }
    }
    return Depth;
}



