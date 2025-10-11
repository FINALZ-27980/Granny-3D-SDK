#if !defined(GRANNY_DAG2_SCALARSOURCE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_scalarsource.h $
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

struct dag2_nodedata_scalarsource_edge
{
    real32 Min;
    real32 Max;
    real32 Current;
};
EXPCONST EXPGROUP(dag2_nodedata_scalarsource) extern data_type_definition Dag2NodeDataScalarSourceEdgeType[];

struct dag2_nodedata_scalarsource
{
    int32 TypeMarker;

    // Scalar sources can operate in one of two modes.  Either a set value from a slider,
    // or an animated value from a vector track
    int32 EdgeCount;
    dag2_nodedata_scalarsource_edge* Edges;
};
EXPCONST EXPGROUP(dag2_nodedata_scalarsource) extern data_type_definition Dag2NodeDataScalarSourceType[];

dag2_node* NewScalarSourceNode(dag2& TheDag, memory_arena& Arena, int32x NumOutputs);

real32
SampleDAGScalar_ScalarSource(dag2_node& Node,
                             dag2_nodedata_scalarsource& NodeData,
                             dag2_instance& Instance,
                             int32x OnOutput,
                             real32 AtTime);

int32 GetInstanceDataSize_ScalarSource(dag2_node& Node, dag2_nodedata_scalarsource&,
                                       dag2& TheDag, model& FromModel, model& ToModel);
bool BindDAGInstanceData_ScalarSource(dag2_node&                  Node,
                                      dag2_nodedata_scalarsource& NodeData,
                                      dag2_instance&              Instance);
bool UnbindDAGInstanceData_ScalarSource(dag2_node&                  Node,
                                        dag2_nodedata_scalarsource& NodeData,
                                        dag2_instance&              Instance);
bool BakeOutDAGInstanceData_ScalarSource(dag2_node&                  Node,
                                         dag2_nodedata_scalarsource& NodeData,
                                         dag2_instance&              Instance,
                                         memory_arena&               Arena);

bool SetFloat_ScalarSource(dag2_node& Node,
                           dag2_nodedata_scalarsource& NodeData,
                           dag2_instance& Instance,
                           char const* ParamName,
                           real32 Value);
bool GetFloat_ScalarSource(dag2_node& Node,
                           dag2_nodedata_scalarsource& NodeData,
                           dag2_instance& Instance,
                           char const* ParamName,
                           real32* Value);


END_GRANNY_NAMESPACE;


#include "header_postfix.h"
#define GRANNY_DAG2_SCALARSOURCE_H
#endif
