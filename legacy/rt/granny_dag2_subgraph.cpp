// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_subgraph.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_subgraph.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_dag2.h"
#include "granny_dag2_instance.h"
#include "granny_dag2_node_registry.h"
#include "granny_dag2_utilities.h"
#include "granny_pose_cache.h"
#include "granny_data_type_definition.h"
#include "granny_local_pose.h"
#include "granny_memory.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_model.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode BlendDagLogMessage
USING_GRANNY_NAMESPACE;

data_type_definition GRANNY Dag2NodeDataSubgraphType[] =
{
    { Int32Member,            "TypeMarker_NodeDataSubgraph" },
    { ReferenceToArrayMember, "SubnodeIndices", Int32Type },
    { EndMember },
};

IMPL_GET_NODEDATA(Subgraph, subgraph);


dag2_node* GRANNY
NewSubgraph(dag2& TheDag, int32x InputEdges, int32x OutputEdges, memory_arena& Arena)
{
    // TODO: Sub aggr?  Blech.

    // Ok, the allocation here requires a little explanation.  For each /external input/
    // edge, there is a corresponding /internal output/ edge, and vice versa.  We'll
    // arrange the inputs and outputs such that the internal nodes always strictly follow
    // the internal nodes.
    int32x const TotalEdges = InputEdges + OutputEdges;

    AGGR_NODE(TheDag,
              NewNode, InputEdges, TotalEdges, OutputEdges, TotalEdges,
              NodeState, dag2_nodedata_subgraph, Dag2NodeDataSubgraphType,
              Arena);
    if (NewNode)
    {
        {for (int32x Edge = 0; Edge < TotalEdges; ++Edge)
        {
            // Setup the internal flag if necessary
            if (Edge >= InputEdges)
            {
                INIT_INTERNAL_INPUT_EDGE(NewNode, Edge, "", Float);
            }
            else
            {
                INIT_EXTERNAL_INPUT_EDGE(NewNode, Edge, "", Float);
            }

            if (Edge < OutputEdges)
            {
                // the nth /external/ output edge is forwarded to the nth /internal/ input edge
                INIT_INTERNAL_OUTPUT_EDGE(NewNode, Edge, "", Float);
                NewNode->OutputEdges[Edge].Forwarded = 1;
                NewNode->OutputEdges[Edge].ForwardIndex = Edge + InputEdges;
            }
            else
            {
                // the nth /internal/ output edge is forwarded to the nth /external/ input edge
                INIT_INTERNAL_OUTPUT_EDGE(NewNode, Edge, "", Float);
                NewNode->OutputEdges[Edge].Forwarded = 1;
                NewNode->OutputEdges[Edge].ForwardIndex = OutputEdges - Edge;
            }
        }}

        NodeState->SubnodeIndexCount = 0;
        NodeState->SubnodeIndices = 0;
        Assert(GetNodeDataSubgraph(NewNode));
    }

    return NewNode;
}


bool GRANNY
AddSubgraphSubnode(dag2& TheDag, dag2_node* Node, dag2_node* Subnode, memory_arena& Arena)
{
    CheckPointerNotNull(Node, return false);
    CheckPointerNotNull(Subnode, return false);
    CheckCondition(!IsValidNodeIndex(Subnode->ParentIndex), return false);

    dag2_nodedata_subgraph* NodeData = GetNodeDataSubgraph(Node);
    CheckPointerNotNull(NodeData, return false);

    Assert(NodeData->SubnodeIndexCount >= 0);
    int32* NewSubnodeIndices =
        ArenaPushArray(Arena, NodeData->SubnodeIndexCount + 1, int32);
    if (NewSubnodeIndices)
    {
        if (NodeData->SubnodeIndexCount)
        {
            Copy32(NodeData->SubnodeIndexCount, NodeData->SubnodeIndices, NewSubnodeIndices);

            // Not necessary to deallocate the subnodes, the memory belongs either to the
            // arena, or the file this graph came from...
            // -.- Deallocate(NodeData->Subnodes);
        }

        Subnode->ParentIndex = GetDag2NodeIndex(TheDag, Node);
        NewSubnodeIndices[NodeData->SubnodeIndexCount] = GetDag2NodeIndex(TheDag, Subnode);
        NodeData->SubnodeIndices = NewSubnodeIndices;
        ++NodeData->SubnodeIndexCount;

        return true;
    }
    else
    {
        Log0(ErrorLogMessage, BlendDagLogMessage,
             "Unable to allocate a new array for subgraph node");
        return false;
    }
}

bool GRANNY
RemoveSubgraphSubnode(dag2& TheDag, dag2_node* Node, dag2_node* Subnode, memory_arena& Arena)
{
    CheckPointerNotNull(Node, return false);
    CheckPointerNotNull(Subnode, return false);
    CheckCondition(Subnode->ParentIndex == GetDag2NodeIndex(TheDag, Node), return false);

    dag2_nodedata_subgraph* NodeData = GetNodeDataSubgraph(Node);
    CheckPointerNotNull(NodeData, return false);
    CheckCondition(NodeData->SubnodeIndexCount >= 1, return false);

    int32x SubnodeIndex = GetDag2NodeIndex(TheDag, Subnode);

    int32x FoundIndex = -1;
    {for(int32x Idx = 0; Idx < NodeData->SubnodeIndexCount; ++Idx)
    {
        if (NodeData->SubnodeIndices[Idx] == SubnodeIndex)
        {
            FoundIndex = Idx;
            break;
        }
    }}
    if (FoundIndex == -1)
    {
        // Tried to delete a non-subnode
        return false;
    }

    // We now need to break any links between the deleted node and our subnodes.
    {
        {for(int32x InputEdge = 0; InputEdge < Subnode->TotalInputEdgeCount; ++InputEdge)
        {
            if (Subnode->InputEdges[InputEdge].Internal)
                continue;

            BreakNodeInputConnection(TheDag, Subnode, InputEdge);
        }}

        {for(int32x OutputEdge = 0; OutputEdge < Subnode->TotalOutputEdgeCount; ++OutputEdge)
        {
            if (Subnode->OutputEdges[OutputEdge].Internal)
                continue;

            BreakNodeOutputConnection(TheDag, Subnode, OutputEdge);
        }}
    }

    int32x NewSubnodeIndexCount = NodeData->SubnodeIndexCount - 1;
    if (NewSubnodeIndexCount != 0)
    {
        int32* NewSubnodeIndices = ArenaPushArray(Arena, NewSubnodeIndexCount, int32);
        int32x Curr = 0;
        {for(int32x Idx = 0; Idx < NodeData->SubnodeIndexCount; ++Idx)
        {
            if (NodeData->SubnodeIndices[Idx] != SubnodeIndex)
                NewSubnodeIndices[Curr++] = NodeData->SubnodeIndices[Idx];
        }}
        Assert(Curr == NewSubnodeIndexCount);

        NodeData->SubnodeIndexCount = NewSubnodeIndexCount;
        NodeData->SubnodeIndices = NewSubnodeIndices;
    }
    else
    {
        NodeData->SubnodeIndexCount = NewSubnodeIndexCount;
        NodeData->SubnodeIndices = 0;
    }

    return true;
}

dag2_node* GRANNY
NodeFromIterator(dag2& TheDag, subgraph_node_iterator Iter)
{
    CheckPointerNotNull(Iter, return 0);

    return GetDag2Node(TheDag, *Iter);
}

int32x GRANNY
NodeIndexFromIterator(subgraph_node_iterator Iter)
{
    CheckPointerNotNull(Iter, return 0);

    return *Iter;
}

subgraph_node_iterator GRANNY
FirstSubgraphNode(dag2_nodedata_subgraph& Subgraph)
{
    if (!Subgraph.SubnodeIndexCount)
        return 0;

    return Subgraph.SubnodeIndices;
}

subgraph_node_iterator GRANNY
NextSubgraphNode(dag2_nodedata_subgraph& Subgraph,
                 subgraph_node_iterator Iter)
{
    CheckPointerNotNull(Iter, return 0);

    // Compute the index
    intaddrx NewIndex = (Iter - Subgraph.SubnodeIndices) + 1;
    if (NewIndex < Subgraph.SubnodeIndexCount)
        return (Iter + 1);
    else
        return 0;
}


int32 GRANNY
GetInstanceDataSize_Subgraph(dag2_node& Node,
                             dag2_nodedata_subgraph& Subgraph,
                             dag2& TheDag,
                             model& FromModel,
                             model& ToModel)
{
    int32 Sum = 0;
    {for(int32x Idx = 0; Idx < Subgraph.SubnodeIndexCount; ++Idx)
    {
        if (IsValidNodeIndex(Subgraph.SubnodeIndices[Idx]))
        {
            dag2_node* ItrNode = GetDag2Node(TheDag, Subgraph.SubnodeIndices[Idx]);
            Assert(ItrNode);

            Sum += GetInstanceDataSize(TheDag, *ItrNode, FromModel, ToModel);
        }
    }}

    // We don't have any instance data ourselves...
    return Sum;
}


bool GRANNY
FreezeDAGInstanceOffsets_Subgraph(dag2_node& Node,
                                  dag2_nodedata_subgraph& Subgraph,
                                  dag2& TheDag,
                                  int32& CurrentOffset,
                                  model& FromModel,
                                  model& ToModel)
{
    CheckCondition(CurrentOffset >= 0, return false);

    {for(int32x Idx = 0; Idx < Subgraph.SubnodeIndexCount; ++Idx)
    {
        if (!IsValidNodeIndex(Subgraph.SubnodeIndices[Idx]))
            return false;

        dag2_node* ItrNode = GetDag2Node(TheDag, Subgraph.SubnodeIndices[Idx]);
        Assert(ItrNode);

        FreezeDAGInstanceOffsets(TheDag, *ItrNode, CurrentOffset, FromModel, ToModel);
    }}

    return true;
}


bool GRANNY
BindDAGInstanceData_Subgraph(dag2_node&              Node,
                             dag2_nodedata_subgraph& Subgraph,
                             dag2_instance&          Instance)
{
    bool AllSuccessful = true;
    {for(int32x Idx = 0; Idx < Subgraph.SubnodeIndexCount && AllSuccessful; ++Idx)
    {
        if (IsValidNodeIndex(Subgraph.SubnodeIndices[Idx]))
        {
            AllSuccessful = BindDAGInstanceData(Instance, Subgraph.SubnodeIndices[Idx]);
        }
    }}

    // We don't have any instance data ourselves...
    return AllSuccessful;
}


bool GRANNY
UnbindDAGInstanceData_Subgraph(dag2_node&              Node,
                               dag2_nodedata_subgraph& Subgraph,
                               dag2_instance&          Instance)
{
    bool AllSuccessful = true;
    {for(int32x Idx = 0; Idx < Subgraph.SubnodeIndexCount && AllSuccessful; ++Idx)
    {
        if (IsValidNodeIndex(Subgraph.SubnodeIndices[Idx]))
            AllSuccessful = UnbindDAGInstanceData(Instance, Subgraph.SubnodeIndices[Idx]);
    }}

    // We don't have any instance data ourselves...
    return AllSuccessful;
}

bool GRANNY
BakeOutDAGInstanceData_Subgraph(dag2_node&              Node,
                                dag2_nodedata_subgraph& Subgraph,
                                dag2_instance&          Instance,
                                memory_arena&           Arena)
{
    bool AllSuccessful = true;
    {for(int32x Idx = 0; Idx < Subgraph.SubnodeIndexCount && AllSuccessful; ++Idx)
    {
        if (IsValidNodeIndex(Subgraph.SubnodeIndices[Idx]))
            AllSuccessful = BakeOutDAGInstanceData(Instance, Subgraph.SubnodeIndices[Idx], Arena);
    }}

    // We don't have any instance data ourselves...
    return AllSuccessful;
}


dag2_node* GRANNY
MostInfluentialNode_Subgraph(dag2_node& Node,
                             dag2_nodedata_subgraph& NodeData,
                             dag2_instance& Instance,
                             int32x OnOutput,
                             real32 AtTime,
                             int32x* InfluentialOutput)
{
    CheckPointerNotNull(Instance.SourceDAG, return 0);
    CheckInt32Index(OnOutput, Node.TotalOutputEdgeCount, return 0);

    // If we're actually here, then the output is not actually connected to anything.
    return 0;
}


local_pose* GRANNY
SampleDAG_Subgraph(dag2_node& Node,
                   dag2_nodedata_subgraph& Subgraph,
                   dag2_instance& Instance,
                   int32x OnOutput,
                   real32 AtTime,
                   pose_cache& PoseCache)
{
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckPointerNotNull(Instance.SourceModel->Skeleton, return 0);

    // If we're actually here, then the output is not actually connected to anything.
    return 0;
}

track_mask* GRANNY
SampleDAGTrackmask_Subgraph(dag2_node& Node,
                            dag2_nodedata_subgraph& Subgraph,
                            dag2_instance& Instance,
                            int32x OnOutput,
                            real32 AtTime)
{
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckPointerNotNull(Instance.SourceModel->Skeleton, return 0);

    // If we're actually here, then the output is not actually connected to anything.
    return 0;
}

real32 GRANNY
SampleDAGScalar_Subgraph(dag2_node& Node,
                         dag2_nodedata_subgraph& NodeData,
                         dag2_instance& Instance,
                         int32x OnOutput,
                         real32 AtTime)
{
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckPointerNotNull(Instance.SourceModel->Skeleton, return 0);

    // If we're actually here, then the output is not actually connected to anything.
    // Return 0?
    return 0.0f;
}

bool GRANNY
GetRootMotion_Subgraph(dag2_node& Node,
                       dag2_nodedata_subgraph& NodeData,
                       dag2_instance& Instance,
                       int32x OnOutput,
                       real32 SecondsElapsed,
                       real32 AtTime,
                       triple& ResultTranslation,
                       triple& ResultRotation)
{
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckPointerNotNull(Instance.SourceModel->Skeleton, return 0);

    // If we're actually here, then the output is not actually connected to anything.
    // Make rest motion vectors
    ZeroStructure(ResultTranslation);
    ZeroStructure(ResultRotation);
    return true;
}

