#if !defined(GRANNY_DAG2_SEQUENCE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_sequence.h $
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


struct dag2_nodedata_sequence_edge
{
    int32 SourceIndex;
    int32 AnimationIndex;
    int32 TrackGroupIndex;

    real32 Speed;
};

struct dag2_nodedata_sequence
{
    int32 TypeMarker;

    int32 EdgeCount;
    dag2_nodedata_sequence_edge* Edges;
};
EXPCONST EXPGROUP(dag2_nodedata_sequence) extern data_type_definition Dag2NodeDataSequenceType[];

dag2_node* NewSequenceNode(dag2& TheDag, memory_arena& Arena);

local_pose* SampleDAG_Sequence(dag2_node& Node,
                               dag2_nodedata_sequence& NodeData,
                               dag2_instance& Instance,
                               int32x OnOutput,
                               real32 AtTime,
                               pose_cache& PoseCache);

bool GetRootMotion_Sequence(dag2_node& Node,
                            dag2_nodedata_sequence& NodeData,
                            dag2_instance& Instance,
                            int32x OnOutput,
                            real32 SecondsElapsed,
                            real32 AtTime,
                            triple& ResultTranslation,
                            triple& ResultRotation);

int32 GetInstanceDataSize_Sequence(dag2_node& Node,
                                   dag2_nodedata_sequence& Sequence,
                                   dag2& TheDag, model& FromModel, model& ToModel);
bool BindDAGInstanceData_Sequence(dag2_node&              Node,
                                  dag2_nodedata_sequence& NodeData,
                                  dag2_instance&          Instance);

bool UnbindDAGInstanceData_Sequence(dag2_node&              Node,
                                    dag2_nodedata_sequence& NodeData,
                                    dag2_instance&          Instance);
bool BakeOutDAGInstanceData_Sequence(dag2_node&              Node,
                                     dag2_nodedata_sequence& NodeData,
                                     dag2_instance&          Instance,
                                     memory_arena&           Arena);

bool GetFloat_Sequence(dag2_node& Node,
                       dag2_nodedata_sequence& NodeData,
                       dag2_instance& Instance,
                       char const* ParamName,
                       real32* Value);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_SEQUENCE_H
#endif
