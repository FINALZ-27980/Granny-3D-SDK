#if !defined(GRANNY_DAG2_TRACKMASK_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_trackmask.h $
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
struct data_type_definition;
struct local_pose;
struct memory_arena;
struct model;
struct track_mask;
struct unbound_track_mask;

struct dag2_nodedata_trackmask
{
    int32       TypeMarker;

    real32      DefaultWeight;
    real32      TargetWeight;
    char const* BoneName;
};
EXPCONST EXPGROUP(dag2_nodedata_trackmask) extern data_type_definition Dag2NodeDataTrackmaskType[];

dag2_node* NewTrackmaskNode(dag2& TheDag, memory_arena& Arena);

int32 GetInstanceDataSize_Trackmask(dag2_node& Node, dag2_nodedata_trackmask&,
                                    dag2& TheDag, model& FromModel, model& ToModel);
bool BindDAGInstanceData_Trackmask(dag2_node&               Node,
                                   dag2_nodedata_trackmask& NodeData,
                                   dag2_instance&           Instance);
bool UnbindDAGInstanceData_Trackmask(dag2_node&               Node,
                                     dag2_nodedata_trackmask& NodeData,
                                     dag2_instance&           Instance);
bool BakeOutDAGInstanceData_Trackmask(dag2_node&               Node,
                                      dag2_nodedata_trackmask& NodeData,
                                      dag2_instance&           Instance,
                                      memory_arena&            Arena);

track_mask* SampleDAGTrackmask_Trackmask(dag2_node& Node,
                                         dag2_nodedata_trackmask& NodeData,
                                         dag2_instance& Instance,
                                         int32x OnOutput,
                                         real32 AtTime);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_TRACKMASK_H
#endif
