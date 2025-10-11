#if !defined(GRANNY_DAG2_UTILITIES_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_utilities.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

struct dag2;
struct dag2_node;
struct local_pose;
struct memory_arena;
struct model;
struct model_instance;
struct skeleton;

local_pose* MakePoseFromArena(skeleton* Skeleton, memory_arena& Arena);

local_pose* MakeRestPoseFromArena(dag2_node& Node,
                                  model_instance& ForModel,
                                  int32x OnOutput,
                                  real32 AtTime,
                                  memory_arena& Arena);
bool Identity_MotionVectors(dag2_node& AtNode,
                            model_instance& ForModel,
                            int32x OnOutput, real32 SecondsElapsed, real32 AtTime,
                            real32 *ResultTranslation3, real32 *ResultRotation3,
                            bool Inverse, memory_arena& Arena);

int32x GetInstanceDataSize(dag2& TheDag, dag2_node& Node, model& FromModel, model& ToModel);
bool FreezeDAGInstanceOffsets(dag2& TheDag, dag2_node& Node, int32& CurrentOffset,
                              model& FromModel, model& ToModel);

bool DAGContainsLoops(dag2& DAG);

#define AGGR_NODE(Dag,                                                                              \
                  NodeVar, NumExInputs, NumInputs, NumExOutputs, NumOutputs,                        \
                  DataVar, datavar_type, DataType, Arena)                                           \
    dag2_node* NodeVar = 0;                                                                         \
    datavar_type* DataVar = 0;                                                                      \
    do {                                                                                            \
        dag2_node** NewNodeArray = 0;                                                               \
                                                                                                    \
        aggr_allocator Allocator;                                                                   \
        InitializeAggrAlloc(Allocator);                                                             \
                                                                                                    \
        /* For node */                                                                              \
        AggrAllocPtr(Allocator, NodeVar);                                                           \
        AggrAllocOffsetArrayPtr(Allocator, NodeVar, NumInputs, TotalInputEdgeCount, InputEdges);    \
        AggrAllocOffsetArrayPtr(Allocator, NodeVar, NumOutputs, TotalOutputEdgeCount, OutputEdges); \
                                                                                                    \
        /* For the dag */                                                                           \
        AggrAllocArrayPtr(Allocator, TheDag.NodeCount + 1, NewNodeArray);                           \
                                                                                                    \
        /* For the dag_nodedata */                                                                  \
        AggrAllocPtr(Allocator, DataVar);                                                           \
                                                                                                    \
        if (EndAggrToArena(Allocator, Arena))                                                       \
        {                                                                                           \
            /* Initialize the node */                                                               \
            {                                                                                       \
                NodeVar->ExternalInputEdgeCount = NumExInputs;                                      \
                NodeVar->ExternalOutputEdgeCount = NumExOutputs;                                    \
                if (NumInputs != 0)  SetPtrNULL(NumInputs, NodeVar->InputEdges);                    \
                if (NumOutputs != 0) SetPtrNULL(NumOutputs, NodeVar->OutputEdges);                  \
                NodeVar->Name = 0;                                                                  \
                NodeVar->ParentIndex = NoNodeIndex;                                                 \
                NodeVar->NodeXPos = 0;                                                              \
                NodeVar->NodeYPos = 0;                                                              \
                NodeVar->NodeData.Type   = 0;                                                       \
                NodeVar->NodeData.Object = 0;                                                       \
            }                                                                                       \
                                                                                                    \
            Copy(sizeof(dag2_node*) * Dag.NodeCount, TheDag.Nodes, NewNodeArray);                   \
            NewNodeArray[Dag.NodeCount++] = NewNode;                                                \
            Dag.Nodes = NewNodeArray;                                                               \
                                                                                                    \
            /* Initialize the nodedata */                                                           \
            {                                                                                       \
                Zero(sizeof(datavar_type), DataVar);                                                \
                DataVar->TypeMarker = GetDag2NodeDataTypeIndexFromTypeMarker(DataType[0].Name);     \
                NodeVar->NodeData.Type   = DataType;                                                \
                NodeVar->NodeData.Object = DataVar;                                                 \
            }                                                                                       \
        }                                                                                           \
    } while (false)


#define TYPE_DISPATCH(AtNode)                   \
      {                                         \
      dag2_node* DispatchNode = &(AtNode);      \
      bool Dispatched = false;

#define NODE_DISPATCH(Name, suffix, Fn)                                         \
  dag2_nodedata_ ## suffix * Data ## Name = GetNodeData ## Name (DispatchNode); \
  if (!Dispatched && Data ## Name != 0)                                         \
  {                                                                             \
      DISPATCH_RESULT =                                                         \
          Fn(*DispatchNode, *(Data ## Name), DISPATCH_ARGS);                    \
      Dispatched = true;                                                        \
  }

#define NODE_DISPATCH_NOARG(Name, suffix, Fn)                                   \
  dag2_nodedata_ ## suffix * Data ## Name = GetNodeData ## Name (DispatchNode); \
  if (!Dispatched && Data ## Name != 0)                                         \
  {                                                                             \
      DISPATCH_RESULT =                                                         \
          Fn(*DispatchNode, *(Data ## Name));                                   \
      Dispatched = true;                                                        \
  }


#define END_TYPE_DISPATCH()   }

#define END_TYPE_DISPATCH_DEFAULT(DefaultFn)        \
  if (!Dispatched)                                  \
  {                                                 \
      DISPATCH_RESULT =                             \
          DefaultFn(*DispatchNode, DISPATCH_ARGS);  \
  }                                                 \
  }


#define INIT_EXTERNAL_INPUT_EDGE(Node, Idx, Name, Type)             \
  {                                                                 \
      Node->InputEdges[Idx].EdgeName = Name;                        \
      Node->InputEdges[Idx].ParameterType = Type ## ParameterType;  \
      Node->InputEdges[Idx].InputNodeIndex = NoNodeIndex;           \
      Node->InputEdges[Idx].ConnectedOutput = -1;                   \
      Node->InputEdges[Idx].Internal = 0;                           \
  } typedef int __require_semicolon

#define INIT_EXTERNAL_OUTPUT_EDGE(Node, Idx, Name, Type)            \
  {                                                                 \
      Node->OutputEdges[Idx].ParameterType = Type ## ParameterType;   \
      Node->OutputEdges[Idx].EdgeName = Name;                         \
      Node->OutputEdges[Idx].Internal = 0;                            \
      Node->OutputEdges[Idx].Forwarded = 0;                           \
  } typedef int __require_semicolon

#define INIT_INTERNAL_INPUT_EDGE(Node, Idx, Name, Type) \
  INIT_EXTERNAL_INPUT_EDGE(Node, Idx, Name, Type);      \
  Node->InputEdges[Idx].Internal = 1

#define INIT_INTERNAL_OUTPUT_EDGE(Node, Idx, Name, Type)    \
  INIT_EXTERNAL_OUTPUT_EDGE(Node, Idx, Name, Type);         \
  Node->OutputEdges[Idx].Internal = 1


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_UTILITIES_H
#endif
