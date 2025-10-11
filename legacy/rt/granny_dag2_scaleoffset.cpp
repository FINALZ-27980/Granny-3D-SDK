// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_scaleoffset.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_scaleoffset.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_dag2.h"
#include "granny_dag2_instance.h"
#include "granny_dag2_node_registry.h"
#include "granny_dag2_utilities.h"
#include "granny_data_type_definition.h"
#include "granny_memory.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"
#include "granny_string.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode BlendDagLogMessage
USING_GRANNY_NAMESPACE;


IMPL_GET_NODEDATA(ScaleOffset, scaleoffset);

data_type_definition GRANNY Dag2NodeDataScaleOffsetType[] =
{
    { Int32Member, "TypeMarker_NodeDataScaleOffset" },
    { Real32Member, "Scale" },
    { Real32Member, "Offset" },
    { EndMember },
};

struct dag2_instancedata_so
{
    real32 Scale;
    real32 Offset;
};

dag2_node* GRANNY
NewScaleOffsetNode(dag2& TheDag, memory_arena& Arena)
{
    AGGR_NODE(TheDag,
              NewNode, 1, 1, 1, 1,
              NodeState, dag2_nodedata_scaleoffset, Dag2NodeDataScaleOffsetType,
              Arena);
    if (NewNode)
    {
        INIT_EXTERNAL_INPUT_EDGE(NewNode, 0, "Input", Float);
        INIT_EXTERNAL_OUTPUT_EDGE(NewNode, 0, "Output", Float);

        NodeState->Scale  = 1.0f;
        NodeState->Offset = 0.0f;
        Assert(GetNodeDataScaleOffset(NewNode));
    }

    return NewNode;
}

dag2_node* GRANNY
MostInfluentialNode_ScaleOffset(dag2_node& Node,
                                dag2_nodedata_scaleoffset& NodeData,
                                dag2_instance& Instance,
                                int32x OnOutput,
                                real32 AtTime,
                                int32x* InfluentialOutput)
{
    if (IsValidNodeIndex(Node.InputEdges[0].InputNodeIndex))
    {
        return FindMostInfluentialNodeInternal(Instance,
                                               Node.InputEdges[0].InputNodeIndex,
                                               Node.InputEdges[0].ConnectedOutput,
                                               AtTime, InfluentialOutput);
    }

    // See node in Blend...
    return 0;
}


real32 GRANNY
SampleDAGScalar_ScaleOffset(dag2_node& Node,
                            dag2_nodedata_scaleoffset& NodeData,
                            dag2_instance& Instance,
                            int32x OnOutput,
                            real32 AtTime)
{
    CheckPointerNotNull(Instance.InstanceData, return 0.0f);
    CheckCondition(OnOutput == 0, return 0.0f);

    dag2_instancedata_so* Data =
        GET_DAG2INST_DATA(dag2_instancedata_so, Instance, Node);

    // Default to 0
    real32 Value = 0.0f;

    if (IsValidNodeIndex(Node.InputEdges[0].InputNodeIndex))
    {
        Value = SampleDAGNodeScalar(Instance,
                                    Node.InputEdges[0].InputNodeIndex,
                                    Node.InputEdges[0].ConnectedOutput,
                                    AtTime);
    }

    return (Value * Data->Scale + Data->Offset);
}

int32 GRANNY
GetInstanceDataSize_ScaleOffset(dag2_node& Node, dag2_nodedata_scaleoffset&,
                                dag2& TheDag, model& FromModel, model& ToModel)
{
    return SizeOf(dag2_instancedata_so);
}

bool GRANNY
BindDAGInstanceData_ScaleOffset(dag2_node&                 Node,
                                dag2_nodedata_scaleoffset& NodeData,
                                dag2_instance&             Instance)
{
    CheckPointerNotNull(Instance.InstanceData, return false);

    dag2_instancedata_so* Data =
        GET_DAG2INST_DATA(dag2_instancedata_so, Instance, Node);

    Data->Scale  = NodeData.Scale;
    Data->Offset = NodeData.Offset;

    return true;
}


bool GRANNY
UnbindDAGInstanceData_ScaleOffset(dag2_node&                  Node,
                                   dag2_nodedata_scaleoffset& NodeData,
                                   dag2_instance&              Instance)
{
    CheckPointerNotNull(Instance.InstanceData, return false);

    // Nothing required yet

    return true;
}

bool GRANNY
BakeOutDAGInstanceData_ScaleOffset(dag2_node&                 Node,
                                   dag2_nodedata_scaleoffset& NodeData,
                                   dag2_instance&             Instance,
                                   memory_arena&              Arena)
{
    CheckPointerNotNull(Instance.InstanceData, return false);

    dag2_instancedata_so* Data = GET_DAG2INST_DATA(dag2_instancedata_so, Instance, Node);

    NodeData.Scale  = Data->Scale;
    NodeData.Offset = Data->Offset;

    return true;
}


bool GRANNY
SetFloat_ScaleOffset(dag2_node& Node,
                     dag2_nodedata_scaleoffset& NodeData,
                     dag2_instance& Instance,
                     char const* ParamName,
                     real32 Value)
{
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);

    dag2_instancedata_so* Data =
        GET_DAG2INST_DATA(dag2_instancedata_so, Instance, Node);

    if (StringsAreEqualLowercase(ParamName, "Scale"))
    {
        Data->Scale = Value;
        return true;
    }
    else if (StringsAreEqualLowercase(ParamName, "Offset"))
    {
        Data->Offset = Value;
        return true;
    }

    return false;
}

bool GRANNY
GetFloat_ScaleOffset(dag2_node& Node,
                     dag2_nodedata_scaleoffset& NodeData,
                     dag2_instance& Instance,
                     char const* ParamName,
                     real32* Value)
{
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);
    CheckPointerNotNull(Value, return false);

    dag2_instancedata_so* Data =
        GET_DAG2INST_DATA(dag2_instancedata_so, Instance, Node);

    if (StringsAreEqualLowercase(ParamName, "Scale"))
    {
        *Value = Data->Scale;
        return true;
    }
    else if (StringsAreEqualLowercase(ParamName, "Offset"))
    {
        *Value = Data->Offset;
        return true;
    }

    return false;
}


