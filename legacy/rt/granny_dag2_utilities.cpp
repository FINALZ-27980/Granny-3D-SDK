// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_utilities.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_utilities.h"

#include "granny_dag2_blend.h"
#include "granny_dag2_locked_blend.h"
#include "granny_dag2_callback.h"
#include "granny_dag2_poseanimsource.h"
#include "granny_dag2_scalarsource.h"
#include "granny_dag2_scaleoffset.h"
#include "granny_dag2_selection.h"
#include "granny_dag2_sequence.h"
#include "granny_dag2_subgraph.h"
#include "granny_dag2_timeshift.h"
#include "granny_dag2_trackmask.h"

#include "granny_assert.h"
#include "granny_dag2.h"
#include "granny_local_pose.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode BlendDagLogMessage
USING_GRANNY_NAMESPACE;


local_pose* GRANNY
MakePoseFromArena(skeleton* Skeleton, memory_arena& Arena)
{
    int32x const LocalPoseSize = GetResultingLocalPoseSize(Skeleton->BoneCount);
    local_pose* Pose =
        NewLocalPoseInPlace(Skeleton->BoneCount,
                            ArenaPushSize(Arena, LocalPoseSize));

    return Pose;
}

local_pose* GRANNY
MakeRestPoseFromArena(dag2_node& Node,
                      model_instance& ForModel,
                      int32x OnOutput,
                      real32 AtTime,
                      memory_arena& Arena)
{
    skeleton* Skeleton = GetSourceSkeleton(ForModel);
    CheckPointerNotNull(Skeleton, return 0);

    local_pose* Pose = MakePoseFromArena(Skeleton, Arena);
    BuildRestLocalPose(*Skeleton, 0, Skeleton->BoneCount, *Pose);

    return Pose;
}

bool GRANNY
Identity_MotionVectors(dag2_node& AtNode,
                       model_instance& ForModel,
                       int32x OnOutput,
                       real32 SecondsElapsed,
                       real32 AtTime,
                       real32 *ResultTranslation3,
                       real32 *ResultRotation3,
                       bool Inverse,
                       memory_arena& Arena)

{
    ZeroStructure(ResultTranslation3);
    ZeroStructure(ResultRotation3);

    return false;
}

int32x GRANNY
GetInstanceDataSize(dag2& TheDag,
                    dag2_node& Node,
                    model& FromModel,
                    model& ToModel)
{
#define DISPATCH_ARGS TheDag, FromModel, ToModel
#define DISPATCH_RESULT Result
    int32x Result = 0;
    TYPE_DISPATCH(Node)
        NODE_DISPATCH(Callback,       callback,       GetInstanceDataSize_Callback)
        NODE_DISPATCH(PoseAnimSource, poseanimsource, GetInstanceDataSize_PoseAnimSource)
        NODE_DISPATCH(LockedBlend,    locked_blend,   GetInstanceDataSize_LockedBlend)
        NODE_DISPATCH(ScalarSource,   scalarsource,   GetInstanceDataSize_ScalarSource)
        NODE_DISPATCH(ScaleOffset,    scaleoffset,    GetInstanceDataSize_ScaleOffset)
        NODE_DISPATCH(Selection,      selection,      GetInstanceDataSize_Selection)
        NODE_DISPATCH(Sequence,       sequence,       GetInstanceDataSize_Sequence)
        NODE_DISPATCH(Subgraph,       subgraph,       GetInstanceDataSize_Subgraph)
        NODE_DISPATCH(TimeShift,      timeshift,      GetInstanceDataSize_TimeShift)
        NODE_DISPATCH(Trackmask,      trackmask,      GetInstanceDataSize_Trackmask)
        END_TYPE_DISPATCH();
#undef DISPATCH_RESULT
#undef DISPATCH_ARGS

    return Result;
}


static bool
FreezeDAGInstanceOffsets_SimpleNode(dag2_node& Node,
                                    dag2& TheDag,
                                    int32& CurrentOffset,
                                    model& FromModel,
                                    model& ToModel)
{
    CheckCondition(CurrentOffset >= 0, return false);

    Node.InstanceOffset = CurrentOffset;
    CurrentOffset += GetInstanceDataSize(TheDag, Node, FromModel, ToModel);

    return true;
}

bool GRANNY
FreezeDAGInstanceOffsets(dag2& TheDag,
                         dag2_node& Node,
                         int32&     CurrentOffset,
                         model&     FromModel,
                         model&     ToModel)
{
    Assert(CurrentOffset >= 0);

    // First, grab our own offset, and advance the pointer
    Node.InstanceOffset = CurrentOffset;

    // Next, if we are a node that has children, we recurse...
#define DISPATCH_ARGS TheDag, CurrentOffset, FromModel, ToModel
#define DISPATCH_RESULT Result
    bool Result = true;
    TYPE_DISPATCH(Node)
        NODE_DISPATCH(Subgraph,       subgraph,       FreezeDAGInstanceOffsets_Subgraph)
        END_TYPE_DISPATCH_DEFAULT(FreezeDAGInstanceOffsets_SimpleNode);
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}


// ---------------------------------------------------------------------------------------
// Loop detection.  Mostly useful at edit time, if there are any loops at runtime, then
// something has gone seriously wrong...
// ---------------------------------------------------------------------------------------
struct LoopDetectEntry
{
    dag2_node*       Node;
    LoopDetectEntry* Next;
};
static bool DAGContainsLoopsInternal(dag2& TheDag, int32x NodeIndex);


static bool
ContainsLoops_Subgraph(dag2_node& Node,
                       dag2_nodedata_subgraph& Subgraph,
                       dag2& TheDag)
{
    {for(subgraph_node_iterator Itr = FirstSubgraphNode(Subgraph);
         Itr != 0; Itr = NextSubgraphNode(Subgraph, Itr))
    {
        if (DAGContainsLoopsInternal(TheDag, NodeIndexFromIterator(Itr)))
            return true;
    }}

    return false;
}

static bool
WalkConnectionsForLoop(dag2& TheDag, int32x NodeIndex, LoopDetectEntry* List)
{
    if (!IsValidNodeIndex(NodeIndex))
        return false;

    dag2_node* Node = GetDag2Node(TheDag, NodeIndex);
    if (!Node)
        return false;

    LoopDetectEntry* Walk = List;
    while (Walk)
    {
        if (Walk->Node == Node)
            return true;

        Walk = Walk->Next;
    }

    bool Result = false;
    LoopDetectEntry NewEntry = { Node, List };
    {for(int32x ConnIdx = 0; ConnIdx < Node->TotalInputEdgeCount && !Result; ++ConnIdx)
    {
        if (!IsValidNodeIndex(Node->InputEdges[ConnIdx].InputNodeIndex))
            continue;

        if (Node->InputEdges[ConnIdx].Internal)
            continue;

        Result = WalkConnectionsForLoop(TheDag, Node->InputEdges[ConnIdx].InputNodeIndex, &NewEntry);
    }}

    return Result;
}

static bool
DAGContainsLoopsInternal(dag2& TheDag, int32x NodeIndex)
{
    dag2_node* Node = GetDag2Node(TheDag, NodeIndex);
    if (Node == 0)
        return false;

#define DISPATCH_ARGS TheDag
#define DISPATCH_RESULT Result
    bool Result = false;
    TYPE_DISPATCH(*Node)
        NODE_DISPATCH(Subgraph, subgraph, ContainsLoops_Subgraph)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    if (!Result)
        Result = WalkConnectionsForLoop(TheDag, NodeIndex, NULL);

    return Result;
}

bool GRANNY
DAGContainsLoops(dag2& DAG)
{
    CheckCondition(IsValidNodeIndex(DAG.RootNodeIndex), return false);

    return DAGContainsLoopsInternal(DAG, DAG.RootNodeIndex);
}
