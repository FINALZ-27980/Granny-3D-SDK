#if !defined(GRANNY_DAG2_SELECTION_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_selection.h $
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

struct dag2_nodedata_selection
{
    int32 TypeMarker;
    int32 DefaultEdge;
};
EXPCONST EXPGROUP(dag2_nodedata_selection) extern data_type_definition Dag2NodeDataSelectionType[];

dag2_node* NewSelectionNode(dag2& TheDag, memory_arena& Arena);

dag2_node*
MostInfluentialNode_Selection(dag2_node& Node,
                              dag2_nodedata_selection& NodeData,
                              dag2_instance& Instance,
                              int32x OnOutput,
                              real32 AtTime,
                              int32x* InfluentialOutput);

local_pose* SampleDAG_Selection(dag2_node& Node,
                                dag2_nodedata_selection& NodeData,
                                dag2_instance& Instance,
                                int32x OnOutput,
                                real32 AtTime,
                                pose_cache& PoseCache);

int32 GetInstanceDataSize_Selection(dag2_node& Node,
                                    dag2_nodedata_selection& Selection,
                                    dag2& TheDag, model& FromModel, model& ToModel);

bool BindDAGInstanceData_Selection(dag2_node&              Node,
                                   dag2_nodedata_selection& NodeData,
                                   dag2_instance&          Instance);
bool UnbindDAGInstanceData_Selection(dag2_node&              Node,
                                     dag2_nodedata_selection& NodeData,
                                     dag2_instance&          Instance);
bool BakeOutDAGInstanceData_Selection(dag2_node&              Node,
                                      dag2_nodedata_selection& NodeData,
                                      dag2_instance&          Instance,
                                      memory_arena&           Arena);

bool GetRootMotion_Selection(dag2_node& Node,
                             dag2_nodedata_selection& NodeData,
                             dag2_instance& Instance,
                             int32x OnOutput,
                             real32 SecondsElapsed,
                             real32 AtTime,
                             triple& ResultTranslation,
                             triple& ResultRotation);

int32x GetCurrentInput_Selection(dag2_node& Node,
                                 dag2_nodedata_selection& NodeData,
                                 dag2_instance& Instance);

bool SetInt32_Selection(dag2_node& Node,
                        dag2_nodedata_selection& NodeData,
                        dag2_instance& Instance,
                        char const* ParamName,
                        int32 Value);
bool GetInt32_Selection(dag2_node& Node,
                        dag2_nodedata_selection& NodeData,
                        dag2_instance& Instance,
                        char const* ParamName,
                        int32* Value);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_SELECTION_H
#endif /* GRANNY_DAG2_SELECTION_H */
