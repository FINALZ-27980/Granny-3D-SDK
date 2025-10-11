#if !defined(GRANNY_DAG2_SUBGRAPH_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_subgraph.h $
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
struct dag2_instance;
struct dag2_node;
struct dag2_source;
struct pose_cache;
struct data_type_definition;
struct local_pose;
struct memory_arena;
struct model;
struct model_instance;
struct track_mask;

struct dag2_nodedata_subgraph
{
    int32   TypeMarker;

    int32   SubnodeIndexCount;
    int32*  SubnodeIndices;
};
EXPCONST EXPGROUP(dag2_nodedata_subgraph) extern data_type_definition Dag2NodeDataSubgraphType[];

dag2_node* NewSubgraph(dag2& TheDag, int32x InputEdges, int32x OutputEdges, memory_arena& Arena);
bool AddSubgraphSubnode(dag2& TheDag, dag2_node* Node, dag2_node* Subnode, memory_arena& Arena);
bool RemoveSubgraphSubnode(dag2& TheDag, dag2_node* Node, dag2_node* Subnode, memory_arena& Arena);

// Iteration API
typedef int32* subgraph_node_iterator;
dag2_node* NodeFromIterator(dag2& TheDag, subgraph_node_iterator);
int32x NodeIndexFromIterator(subgraph_node_iterator);
subgraph_node_iterator FirstSubgraphNode(dag2_nodedata_subgraph& Subgraph);
subgraph_node_iterator NextSubgraphNode(dag2_nodedata_subgraph& Subgraph, subgraph_node_iterator);


// Runtime, fast api
int32 GetInstanceDataSize_Subgraph(dag2_node& Node, dag2_nodedata_subgraph&,
                                   dag2& TheDag,
                                   model& FromModel, model& ToModel);

bool FreezeDAGInstanceOffsets_Subgraph(dag2_node& Node,
                                       dag2_nodedata_subgraph& Subgraph,
                                       dag2& TheDag,
                                       int32& CurrentOffset,
                                       model& FromModel,
                                       model& ToModel);

bool BindDAGInstanceData_Subgraph(dag2_node&              Node,
                                  dag2_nodedata_subgraph& NodeData,
                                  dag2_instance&          Instance);
bool UnbindDAGInstanceData_Subgraph(dag2_node&              Node,
                                    dag2_nodedata_subgraph& NodeData,
                                    dag2_instance&          Instance);
bool BakeOutDAGInstanceData_Subgraph(dag2_node&              Node,
                                     dag2_nodedata_subgraph& NodeData,
                                     dag2_instance&          Instance,
                                     memory_arena&           Arena);

dag2_node*
MostInfluentialNode_Subgraph(dag2_node& Node,
                             dag2_nodedata_subgraph& NodeData,
                             dag2_instance& Instance,
                             int32x OnOutput,
                             real32 AtTime,
                             int32x* InfluentialOutput);

local_pose* SampleDAG_Subgraph(dag2_node& Node,
                               dag2_nodedata_subgraph& Subgraph,
                               dag2_instance& Instance,
                               int32x OnOutput,
                               real32 AtTime,
                               pose_cache& PoseCache);

track_mask* SampleDAGTrackmask_Subgraph(dag2_node& Node,
                                        dag2_nodedata_subgraph& Subgraph,
                                        dag2_instance& Instance,
                                        int32x OnOutput,
                                        real32 AtTime);

real32 SampleDAGScalar_Subgraph(dag2_node& Node,
                                dag2_nodedata_subgraph& NodeData,
                                dag2_instance& Instance,
                                int32x OnOutput,
                                real32 AtTime);

bool GetRootMotion_Subgraph(dag2_node& Node,
                            dag2_nodedata_subgraph& NodeData,
                            dag2_instance& Instance,
                            int32x OnOutput,
                            real32 SecondsElapsed,
                            real32 AtTime,
                            triple& ResultTranslation,
                            triple& ResultRotation);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_SUBGRAPH_H
#endif
