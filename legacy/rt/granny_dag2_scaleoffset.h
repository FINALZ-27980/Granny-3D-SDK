#if !defined(GRANNY_DAG2_SCALEOFFSET_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_scaleoffset.h $
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

struct dag2_nodedata_scaleoffset
{
    int32 TypeMarker;

    // Scalar sources can operate in one of two modes.  Either a set value from a slider,
    // or an animated value from a vector track
    real32 Scale;
    real32 Offset;
};
EXPCONST EXPGROUP(dag2_nodedata_scaleoffset) extern data_type_definition Dag2NodeDataScaleOffsetType[];

dag2_node* NewScaleOffsetNode(dag2& TheDag, memory_arena& Arena);

dag2_node*
MostInfluentialNode_ScaleOffset(dag2_node& Node,
                                dag2_nodedata_scaleoffset& NodeData,
                                dag2_instance& Instance,
                                int32x OnOutput,
                                real32 AtTime,
                                int32x* InfluentialOutput);
real32
SampleDAGScalar_ScaleOffset(dag2_node& Node,
                             dag2_nodedata_scaleoffset& NodeData,
                             dag2_instance& Instance,
                             int32x OnOutput,
                             real32 AtTime);

int32 GetInstanceDataSize_ScaleOffset(dag2_node& Node, dag2_nodedata_scaleoffset&,
                                      dag2& TheDag, model& FromModel, model& ToModel);
bool BindDAGInstanceData_ScaleOffset(dag2_node&                  Node,
                                      dag2_nodedata_scaleoffset& NodeData,
                                      dag2_instance&              Instance);
bool UnbindDAGInstanceData_ScaleOffset(dag2_node&                  Node,
                                        dag2_nodedata_scaleoffset& NodeData,
                                        dag2_instance&              Instance);
bool BakeOutDAGInstanceData_ScaleOffset(dag2_node&                 Node,
                                        dag2_nodedata_scaleoffset& NodeData,
                                        dag2_instance&             Instance,
                                        memory_arena&              Arena);

bool SetFloat_ScaleOffset(dag2_node& Node,
                           dag2_nodedata_scaleoffset& NodeData,
                           dag2_instance& Instance,
                           char const* ParamName,
                           real32 Value);
bool GetFloat_ScaleOffset(dag2_node& Node,
                           dag2_nodedata_scaleoffset& NodeData,
                           dag2_instance& Instance,
                           char const* ParamName,
                           real32* Value);


END_GRANNY_NAMESPACE;


#include "header_postfix.h"
#define GRANNY_DAG2_SCALEOFFSET_H
#endif
