// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_scalarsource.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_scalarsource.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_dag2.h"
#include "granny_dag2_instance.h"
#include "granny_dag2_node_registry.h"
#include "granny_dag2_utilities.h"
#include "granny_data_type_definition.h"
#include "granny_math.h"
#include "granny_memory.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_string.h"

#include "granny_parameter_checking.h"
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

USING_GRANNY_NAMESPACE;

#define SubsystemCode BlendDagLogMessage

IMPL_GET_NODEDATA(ScalarSource, scalarsource);

data_type_definition GRANNY Dag2NodeDataScalarSourceEdgeType[] =
{
    { Real32Member, "Min" },
    { Real32Member, "Max" },
    { Real32Member, "Current" },

    { EndMember },
};

data_type_definition GRANNY Dag2NodeDataScalarSourceType[] =
{
    { Int32Member, "TypeMarker_NodeDataScalarSource" },
    { ReferenceToArrayMember, "Edges", Dag2NodeDataScalarSourceEdgeType },

    { EndMember },
};

struct dag2_instancedata_ss
{
    real32 Current;
};


dag2_node* GRANNY
NewScalarSourceNode(dag2& TheDag, memory_arena& Arena, int32x NumOutputs)
{
    Assert(NumOutputs > 0);

    AGGR_NODE(TheDag,
              NewNode, 0, 0, NumOutputs, NumOutputs,
              NodeState, dag2_nodedata_scalarsource, Dag2NodeDataScalarSourceType,
              Arena);
    if (NewNode)
    {
        NodeState->EdgeCount = NumOutputs;
        NodeState->Edges = ArenaPushArray(Arena, NumOutputs, dag2_nodedata_scalarsource_edge);

        {for (int32x Idx = 0; Idx < NumOutputs; ++Idx)
        {
            INIT_EXTERNAL_OUTPUT_EDGE(NewNode, Idx, "Output", Float);

            NodeState->Edges[Idx].Min = 0.0f;
            NodeState->Edges[Idx].Max = 1.0f;
            NodeState->Edges[Idx].Current = 0.0f;
        }}

        Assert(GetNodeDataScalarSource(NewNode));
    }

    return NewNode;
}

real32 GRANNY
SampleDAGScalar_ScalarSource(dag2_node& Node,
                             dag2_nodedata_scalarsource& NodeData,
                             dag2_instance& Instance,
                             int32x OnOutput,
                             real32 AtTime)
{
    CheckPointerNotNull(Instance.InstanceData, return 0.0f);
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);
    CheckInt32Index(OnOutput, Node.ExternalOutputEdgeCount, return 0.0f);

    dag2_instancedata_ss* Data =
        GET_DAG2INST_DATA(dag2_instancedata_ss, Instance, Node);

    return Data[OnOutput].Current;
}

int32 GRANNY
GetInstanceDataSize_ScalarSource(dag2_node& Node, dag2_nodedata_scalarsource& NodeData,
                                 dag2& TheDag, model& FromModel, model& ToModel)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);

    return (NodeData.EdgeCount * SizeOf(dag2_instancedata_ss));
}


bool GRANNY
BindDAGInstanceData_ScalarSource(dag2_node&                  Node,
                                 dag2_nodedata_scalarsource& NodeData,
                                 dag2_instance&              Instance)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);
    CheckPointerNotNull(Instance.InstanceData, return false);

    dag2_instancedata_ss* Data =
        GET_DAG2INST_DATA(dag2_instancedata_ss, Instance, Node);

    {for(int32x Idx = 0; Idx < NodeData.EdgeCount; ++Idx)
    {
        Data[Idx].Current = NodeData.Edges[Idx].Current;
    }}

    return true;
}


bool GRANNY
UnbindDAGInstanceData_ScalarSource(dag2_node&                  Node,
                                   dag2_nodedata_scalarsource& NodeData,
                                   dag2_instance&              Instance)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);
    CheckPointerNotNull(Instance.InstanceData, return false);

    // Nothing required yet

    return true;
}

bool GRANNY
BakeOutDAGInstanceData_ScalarSource(dag2_node&                  Node,
                                    dag2_nodedata_scalarsource& NodeData,
                                    dag2_instance&              Instance,
                                    memory_arena&               Arena)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);
    CheckPointerNotNull(Instance.InstanceData, return false);

    dag2_instancedata_ss* Data = GET_DAG2INST_DATA(dag2_instancedata_ss, Instance, Node);

    {for(int32x Idx = 0; Idx < NodeData.EdgeCount; ++Idx)
    {
        NodeData.Edges[Idx].Current = Data[Idx].Current;
    }}

    return true;
}


bool GRANNY
SetFloat_ScalarSource(dag2_node& Node,
                      dag2_nodedata_scalarsource& NodeData,
                      dag2_instance& Instance,
                      char const* ParamName,
                      real32 Value)
{
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);

    dag2_instancedata_ss* Data =
        GET_DAG2INST_DATA(dag2_instancedata_ss, Instance, Node);

    if (StringBeginsWithLowercase(ParamName, "Current"))
    {
        char const* Underscore = FindLastInstanceOf(ParamName, '_');
        if (Underscore)
        {
            int32x Index = ConvertToInt32(Underscore + 1);
            CheckInt32Index(Index, NodeData.EdgeCount, return false);

            Data[Index].Current = Clamp(NodeData.Edges[Index].Min,
                                        Value,
                                        NodeData.Edges[Index].Max);
            return true;
        }
        else if (StringsAreEqualLowercase(ParamName, "Current"))
        {
            // Set /all/ of the edges if the parameter contains no index
            {for(int32x Idx = 0; Idx < NodeData.EdgeCount; ++Idx)
            {
                Data[Idx].Current = Clamp(NodeData.Edges[Idx].Min,
                                          Value,
                                          NodeData.Edges[Idx].Max);
            }}
        }
    }

    return false;
}


bool GRANNY
GetFloat_ScalarSource(dag2_node& Node,
                      dag2_nodedata_scalarsource& NodeData,
                      dag2_instance& Instance,
                      char const* ParamName,
                      real32* Value)
{
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);
    CheckPointerNotNull(Value, return false);

    dag2_instancedata_ss* Data =
        GET_DAG2INST_DATA(dag2_instancedata_ss, Instance, Node);

    if (StringBeginsWithLowercase(ParamName, "Current"))
    {
        char const* Underscore = FindLastInstanceOf(ParamName, '_');
        if (Underscore)
        {
            int32x Index = ConvertToInt32(Underscore + 1);
            CheckInt32Index(Index, NodeData.EdgeCount, return false);

            *Value = Data[Index].Current;
            return true;
        }
    }

    return false;
}


