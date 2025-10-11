// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_timeshift.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_timeshift.h"

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
#include "granny_string.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode BlendDagLogMessage
USING_GRANNY_NAMESPACE;

IMPL_GET_NODEDATA(TimeShift, timeshift);

data_type_definition GRANNY
Dag2NodeDataTimeShiftType[] =
{
    { Int32Member, "TypeMarker_TimeShift" },

    { Real32Member, "RateChange" },
    { Real32Member, "Offset" },

    { Bool32Member, "HoldStart" },
    { Bool32Member, "HoldEnd" },
    { Real32Member, "Duration" },

    { EndMember },
};

struct dag2_instancedata_timeshift
{
    // Essentially the same as the node data minus the type marker
    real32 RateChange;
    real32 Offset;

    bool32 HoldStart;
    bool32 HoldEnd;
    real32 Duration;
};


dag2_node* GRANNY
MostInfluentialNode_TimeShift(dag2_node& Node,
                              dag2_nodedata_timeshift& NodeData,
                              dag2_instance& Instance,
                              int32x OnOutput,
                              real32 AtTime,
                              int32x* InfluentialOutput)
{
    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckCondition(OnOutput == 0, return 0);

    if (IsValidNodeIndex(Node.InputEdges[OnOutput].InputNodeIndex))
    {
        return FindMostInfluentialNodeInternal(Instance,
                                               Node.InputEdges[OnOutput].InputNodeIndex,
                                               Node.InputEdges[OnOutput].ConnectedOutput,
                                               AtTime, InfluentialOutput);
    }

    return 0;
}


dag2_node* GRANNY
NewTimeShift(dag2& TheDag, int32x Edges, memory_arena& Arena)
{
    AGGR_NODE(TheDag, NewNode, Edges, Edges, Edges, Edges,
              NodeState, dag2_nodedata_timeshift, Dag2NodeDataTimeShiftType,
              Arena);
    if (NewNode)
    {
        {for (int32x EdgeIdx = 0; EdgeIdx < Edges; ++EdgeIdx)
        {
            INIT_EXTERNAL_OUTPUT_EDGE(NewNode, EdgeIdx, "Output", Pose);
            INIT_EXTERNAL_INPUT_EDGE(NewNode, EdgeIdx, "Input", Pose);
        }}

        NodeState->RateChange = 1;
        NodeState->Offset     = 0;
        NodeState->HoldStart  = 0;
        NodeState->HoldEnd    = 0;
        NodeState->Duration   = 1;

        Assert(GetNodeDataTimeShift(NewNode));
    }

    return NewNode;
}


static real32
ComputeAdjustedTime(real32 AtTime,
                    dag2_instancedata_timeshift& Data)
{
    real32 Shifted = AtTime * Data.RateChange + Data.Offset;

    if (Data.HoldStart && Shifted < Data.Offset)
        Shifted = Data.Offset;

    if (Data.HoldEnd && Shifted > Data.Offset + Data.Duration*Data.RateChange)
        Shifted = Data.Offset + Data.Duration*Data.RateChange;

    return Shifted;
}


local_pose* GRANNY
SampleDAG_TimeShift(dag2_node& Node,
                    dag2_nodedata_timeshift& NodeData,
                    dag2_instance& Instance,
                    int32x OnOutput,
                    real32 AtTime,
                    pose_cache& PoseCache)
{
    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckInt32Index(OnOutput, Node.TotalOutputEdgeCount, return 0);
    CheckCondition(Node.OutputEdges[OnOutput].ParameterType == PoseParameterType, return 0);

    dag2_instancedata_timeshift* Data =
        GET_DAG2INST_DATA(dag2_instancedata_timeshift, Instance, Node);
    Assert(Data);

    if (!IsValidNodeIndex(Node.InputEdges[OnOutput].InputNodeIndex))
        return 0;

    real32 const Adjusted = ComputeAdjustedTime(AtTime, *Data);

    return SampleDAGNode(Instance,
                         Node.InputEdges[OnOutput].InputNodeIndex,
                         Node.InputEdges[OnOutput].ConnectedOutput,
                         Adjusted, PoseCache);
}


real32 GRANNY
SampleDAGScalar_TimeShift(dag2_node& Node,
                          dag2_nodedata_timeshift& NodeData,
                          dag2_instance& Instance,
                          int32x OnOutput,
                          real32 AtTime)
{
    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckInt32Index(OnOutput, Node.TotalOutputEdgeCount, return 0);
    CheckCondition(Node.OutputEdges[OnOutput].ParameterType == FloatParameterType, return 0);

    dag2_instancedata_timeshift* Data =
        GET_DAG2INST_DATA(dag2_instancedata_timeshift, Instance, Node);
    Assert(Data);

    if (!IsValidNodeIndex(Node.InputEdges[OnOutput].InputNodeIndex))
        return 0;

    real32 const Adjusted = ComputeAdjustedTime(AtTime, *Data);

    return SampleDAGNodeScalar(Instance,
                               Node.InputEdges[OnOutput].InputNodeIndex,
                               Node.InputEdges[OnOutput].ConnectedOutput,
                               Adjusted);
}


bool GRANNY
GetRootMotion_TimeShift(dag2_node& Node,
                        dag2_nodedata_timeshift& NodeData,
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
    CheckCondition(Node.OutputEdges[OnOutput].ParameterType == PoseParameterType, return 0);

    dag2_instancedata_timeshift* Data =
        GET_DAG2INST_DATA(dag2_instancedata_timeshift, Instance, Node);
    Assert(Data);

    if (!IsValidNodeIndex(Node.InputEdges[OnOutput].InputNodeIndex))
        return false;

    real32 const AdjustedAt = ComputeAdjustedTime(AtTime, *Data);
    real32 const AdjustedElapsed =
        AdjustedAt - ComputeAdjustedTime(AtTime - SecondsElapsed, *Data);

    return GetDagNodeRootMotionVectors(Instance,
                                       Node.InputEdges[OnOutput].InputNodeIndex,
                                       Node.InputEdges[OnOutput].ConnectedOutput,
                                       AdjustedElapsed, AdjustedAt,
                                       ResultTranslation, ResultRotation);
}


int32 GRANNY
GetInstanceDataSize_TimeShift(dag2_node& Node, dag2_nodedata_timeshift&,
                              dag2& TheDag, model& FromModel, model& ToModel)
{
    return SizeOf(dag2_instancedata_timeshift);
}


bool GRANNY
BindDAGInstanceData_TimeShift(dag2_node&               Node,
                              dag2_nodedata_timeshift& NodeData,
                              dag2_instance&           Instance)
{
    dag2_instancedata_timeshift* Data =
        GET_DAG2INST_DATA(dag2_instancedata_timeshift, Instance, Node);

    Data->RateChange = NodeData.RateChange;
    Data->Offset     = NodeData.Offset;
    Data->HoldStart  = NodeData.HoldStart;
    Data->HoldEnd    = NodeData.HoldEnd;
    Data->Duration   = NodeData.Duration;

    return true;
}


bool GRANNY
UnbindDAGInstanceData_TimeShift(dag2_node&                    Node,
                                dag2_nodedata_timeshift& NodeData,
                                dag2_instance&                Instance)
{
    // Nothing to do
    return true;
}


bool GRANNY
BakeOutDAGInstanceData_TimeShift(dag2_node&               Node,
                                 dag2_nodedata_timeshift& NodeData,
                                 dag2_instance&           Instance,
                                 memory_arena&            Arena)
{
    dag2_instancedata_timeshift* Data =
        GET_DAG2INST_DATA(dag2_instancedata_timeshift, Instance, Node);

    NodeData.RateChange = Data->RateChange;
    NodeData.Offset     = Data->Offset;
    NodeData.HoldStart  = Data->HoldStart;
    NodeData.HoldEnd    = Data->HoldEnd;
    NodeData.Duration   = Data->Duration;

    return true;
}


bool GRANNY
SetFloat_TimeShift(dag2_node& Node,
                   dag2_nodedata_timeshift& NodeData,
                   dag2_instance& Instance,
                   char const* ParamName,
                   real32 Value)
{
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);

    dag2_instancedata_timeshift* Data =
        GET_DAG2INST_DATA(dag2_instancedata_timeshift, Instance, Node);

    if (StringsAreEqualLowercase(ParamName, "Rate Change"))
    {
        Data->RateChange = Value;
        return true;
    }
    else if (StringsAreEqualLowercase(ParamName, "Offset"))
    {
        Data->Offset = Value;
        return true;
    }
    else if (StringsAreEqualLowercase(ParamName, "Duration"))
    {
        Data->Duration = Value;
        return true;
    }

    return false;
}


bool GRANNY
GetFloat_TimeShift(dag2_node& Node,
                   dag2_nodedata_timeshift& NodeData,
                   dag2_instance& Instance,
                   char const* ParamName,
                   real32* Value)
{
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);
    CheckPointerNotNull(Value, return false);

    dag2_instancedata_timeshift* Data =
        GET_DAG2INST_DATA(dag2_instancedata_timeshift, Instance, Node);

    if (StringsAreEqualLowercase(ParamName, "Rate Change"))
    {
        *Value = Data->RateChange;
        return true;
    }
    else if (StringsAreEqualLowercase(ParamName, "Offset"))
    {
        *Value = Data->Offset;
        return true;
    }
    else if (StringsAreEqualLowercase(ParamName, "Duration"))
    {
        *Value = Data->Duration;
        return true;
    }

    return false;
}


bool GRANNY
SetInt32_TimeShift(dag2_node& Node,
                   dag2_nodedata_timeshift& NodeData,
                   dag2_instance& Instance,
                   char const* ParamName,
                   int32 Value)
{
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);

    dag2_instancedata_timeshift* Data =
        GET_DAG2INST_DATA(dag2_instancedata_timeshift, Instance, Node);

    if (StringsAreEqualLowercase(ParamName, "HoldStart"))
    {
        Data->HoldStart = Value ? 1 : 0;
        return true;
    }
    else if (StringsAreEqualLowercase(ParamName, "HoldEnd"))
    {
        Data->HoldEnd = Value ? 1 : 0;
        return true;
    }

    return false;
}


bool GRANNY
GetInt32_TimeShift(dag2_node& Node,
                   dag2_nodedata_timeshift& NodeData,
                   dag2_instance& Instance,
                   char const* ParamName,
                   int32* Value)
{
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);
    CheckPointerNotNull(Value, return false);

    dag2_instancedata_timeshift* Data =
        GET_DAG2INST_DATA(dag2_instancedata_timeshift, Instance, Node);

    if (StringsAreEqualLowercase(ParamName, "HoldStart"))
    {
        *Value = Data->HoldStart ? 1: 0;
        return true;
    }
    else if (StringsAreEqualLowercase(ParamName, "HoldEnd"))
    {
        *Value = Data->HoldEnd ? 1 : 0;
        return true;
    }

    return false;
}

