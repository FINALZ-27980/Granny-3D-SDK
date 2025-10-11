// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_callback.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_callback.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_dag2.h"
#include "granny_dag2_instance.h"
#include "granny_dag2_node_registry.h"
#include "granny_dag2_utilities.h"
#include "granny_data_type_definition.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode BlendDagLogMessage
USING_GRANNY_NAMESPACE;

IMPL_GET_NODEDATA(Callback, callback);

data_type_definition GRANNY
Dag2NodeDataCallbackType[] =
{
    { Int32Member, "TypeMarker_Callback" },
    { EndMember },
};

struct dag2_instancedata_callback
{
    dag2_callbacks Callbacks;
    void*          UserData;
};


dag2_node* GRANNY
NewCallback(dag2& TheDag, memory_arena& Arena)
{
    AGGR_NODE(TheDag, NewNode, 1, 1, 1, 1,
              NodeState, dag2_nodedata_callback, Dag2NodeDataCallbackType,
              Arena);
    if (NewNode)
    {
        INIT_EXTERNAL_OUTPUT_EDGE(NewNode, 0, "Pose out",  Pose);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, 0, "DefPose in", Pose);

        Assert(GetNodeDataCallback(NewNode));
    }

    return NewNode;
}


bool GRANNY
InstallNodeCallback(dag2_instance&  Instance,
                    dag2_node&      Node,
                    dag2_callbacks* Callbacks,
                    void*           UserData)
{
    dag2_nodedata_callback* Callback = GetNodeDataCallback(&Node);
    CheckPointerNotNull(Callback, return false);

    dag2_instancedata_callback* CallbackData =
        GET_DAG2INST_DATA(dag2_instancedata_callback, Instance, Node);
    CheckPointerNotNull(CallbackData, return false);

    CallbackData->Callbacks = *Callbacks;
    CallbackData->UserData  = UserData;

    return true;
}


local_pose* GRANNY
SampleDAG_Callback(dag2_node& Node,
                   dag2_nodedata_callback& NodeData,
                   dag2_instance& Instance,
                   int32x OnOutput,
                   real32 AtTime,
                   pose_cache& PoseCache)
{
    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckInt32Index(OnOutput, Node.TotalOutputEdgeCount, return 0);
    CheckCondition(Node.TotalOutputEdgeCount == Node.TotalInputEdgeCount, return 0);
    CheckCondition(Node.OutputEdges[OnOutput].ParameterType == PoseParameterType, return 0);

    dag2_instancedata_callback* CallbackData =
        GET_DAG2INST_DATA(dag2_instancedata_callback, Instance, Node);
    CheckPointerNotNull(CallbackData, return 0);

    if (CallbackData->Callbacks.SamplePoseCallback)
    {
        return CallbackData->Callbacks.SamplePoseCallback(&Node, &Instance,
                                                           OnOutput, AtTime, &PoseCache,
                                                           CallbackData->UserData);
    }
    else if (IsValidNodeIndex(Node.InputEdges[OnOutput].InputNodeIndex))
    {
        return SampleDAGNode(Instance,
                             Node.InputEdges[OnOutput].InputNodeIndex,
                             Node.InputEdges[OnOutput].ConnectedOutput,
                             AtTime, PoseCache);
    }

    // Not wired, and no callback
    return 0;
}


real32 GRANNY
SampleDAGScalar_Callback(dag2_node& Node,
                         dag2_nodedata_callback& NodeData,
                         dag2_instance& Instance,
                         int32x OnOutput,
                         real32 AtTime)
{
    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckInt32Index(OnOutput, Node.TotalOutputEdgeCount, return 0);
    CheckCondition(Node.TotalOutputEdgeCount == Node.TotalInputEdgeCount, return 0);
    CheckCondition(Node.OutputEdges[OnOutput].ParameterType == FloatParameterType, return 0);

    dag2_instancedata_callback* CallbackData =
        GET_DAG2INST_DATA(dag2_instancedata_callback, Instance, Node);
    CheckPointerNotNull(CallbackData, return 0);

    if (CallbackData->Callbacks.SampleScalarCallback)
    {
        return
            CallbackData->Callbacks.SampleScalarCallback(&Node, &Instance,
                                                         OnOutput, AtTime,
                                                         CallbackData->UserData);
    }
    else if (IsValidNodeIndex(Node.InputEdges[OnOutput].InputNodeIndex))
    {
        return SampleDAGNodeScalar(Instance,
                                   Node.InputEdges[OnOutput].InputNodeIndex,
                                   Node.InputEdges[OnOutput].ConnectedOutput,
                                   AtTime);
    }

    // Not wired, and no callback
    return 0;
}


bool GRANNY
GetRootMotion_Callback(dag2_node& Node,
                       dag2_nodedata_callback& NodeData,
                       dag2_instance& Instance,
                       int32x OnOutput,
                       real32 SecondsElapsed,
                       real32 AtTime,
                       triple& ResultTranslation,
                       triple& ResultRotation)
{
    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckInt32Index(OnOutput, Node.TotalOutputEdgeCount, return 0);
    CheckCondition(Node.TotalOutputEdgeCount == Node.TotalInputEdgeCount, return 0);
    CheckCondition(Node.OutputEdges[OnOutput].ParameterType == PoseParameterType, return 0);

    dag2_instancedata_callback* CallbackData =
        GET_DAG2INST_DATA(dag2_instancedata_callback, Instance, Node);
    CheckPointerNotNull(CallbackData, return 0);

    if (CallbackData->Callbacks.GetRootMotionCallback)
    {
        return CallbackData->Callbacks.GetRootMotionCallback(&Node, &Instance,
                                                             OnOutput,
                                                             SecondsElapsed, AtTime,
                                                             ResultTranslation, ResultRotation,
                                                             CallbackData->UserData);
    }
    else if (IsValidNodeIndex(Node.InputEdges[OnOutput].InputNodeIndex))
    {
        return GetDagNodeRootMotionVectors(Instance,
                                           Node.InputEdges[OnOutput].InputNodeIndex,
                                           Node.InputEdges[OnOutput].ConnectedOutput,
                                           SecondsElapsed, AtTime,
                                           ResultTranslation, ResultRotation);
    }

    // Not wired, and no callback
    return false;
}


dag2_node* GRANNY
MostInfluentialNode_Callback(dag2_node& Node,
                             dag2_nodedata_callback& NodeData,
                             dag2_instance& Instance,
                             int32x OnOutput,
                             real32 AtTime,
                             int32x* InfluentialOutput)
{
    dag2_instancedata_callback* CallbackData =
        GET_DAG2INST_DATA(dag2_instancedata_callback, Instance, Node);
    CheckPointerNotNull(CallbackData, return 0);

    // If we have a valid callback, then /we/ are the most influential, otherwise pass to default input edge.
    if (CallbackData->Callbacks.GetRootMotionCallback)
    {
        SetByValueNotNull(InfluentialOutput, OnOutput);
        return &Node;
    }
    else if (IsValidNodeIndex(Node.InputEdges[OnOutput].InputNodeIndex))
    {
        return Instance.SourceDAG->Nodes[Node.InputEdges[OnOutput].InputNodeIndex];
    }

    return 0;
}


int32 GRANNY
GetInstanceDataSize_Callback(dag2_node& Node, dag2_nodedata_callback&,
                             dag2& TheDag, model& FromModel, model& ToModel)
{
    return SizeOf(dag2_instancedata_callback);
}

bool GRANNY
BindDAGInstanceData_Callback(dag2_node&              Node,
                             dag2_nodedata_callback& NodeData,
                             dag2_instance&          Instance)
{
    dag2_instancedata_callback* Data =
        GET_DAG2INST_DATA(dag2_instancedata_callback, Instance, Node);
    Assert(Data);

    ZeroStructure(*Data);
    return true;
}

bool GRANNY
UnbindDAGInstanceData_Callback(dag2_node&              Node,
                               dag2_nodedata_callback& NodeData,
                               dag2_instance&          Instance)
{
    dag2_instancedata_callback* Data =
        GET_DAG2INST_DATA(dag2_instancedata_callback, Instance, Node);
    Assert(Data);

    ZeroStructure(*Data);
    return true;
}


