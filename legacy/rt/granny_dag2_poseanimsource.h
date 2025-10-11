#if !defined(GRANNY_DAG2_POSEANIMSOURCE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_poseanimsource.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(DagGroup);

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
struct text_track_entry;


struct dag2_nodedata_poseanimsource_edge
{
    int32 SourceIndex;
    int32 AnimationIndex;
    int32 TrackGroupIndex;

    int32  EventTrackIndex;
    bool32 EventTrackSpecified;

    real32 Speed;
    real32 Offset;
    int32  LoopCount;
    bool32 ClampedLoop;
};

struct dag2_nodedata_poseanimsource
{
    int32 TypeMarker;

    int32 EdgeCount;
    dag2_nodedata_poseanimsource_edge* Edges;
};
EXPCONST EXPGROUP(dag2_nodedata_poseanimsource) extern data_type_definition Dag2NodeDataPoseAnimSourceType[];

dag2_node* NewPoseAnimSourceNode(dag2& TheDag, memory_arena& Arena);

local_pose*
SampleDAG_PoseAnimSource(dag2_node& Node,
                         dag2_nodedata_poseanimsource& NodeData,
                         dag2_instance& Instance,
                         int32x OnOutput,
                         real32 AtTime,
                         pose_cache& PoseCache);
bool
SampleDAGLockParams_PoseAnimSource(dag2_node& Node,
                                   dag2_nodedata_poseanimsource& NodeData,
                                   dag2_instance& Instance,
                                   int32x OnOutput,
                                   real32 AtTime,
                                   real32& Duration);
bool
GetRootMotion_PoseAnimSource(dag2_node& Node,
                             dag2_nodedata_poseanimsource& NodeData,
                             dag2_instance& Instance,
                             int32x OnOutput,
                             real32 SecondsElapsed,
                             real32 AtTime,
                             triple& ResultTranslation,
                             triple& ResultRotation);

int32 GetInstanceDataSize_PoseAnimSource(dag2_node& Node, dag2_nodedata_poseanimsource&,
                                         dag2& TheDag, model& FromModel, model& ToModel);
bool BindDAGInstanceData_PoseAnimSource(dag2_node&                    Node,
                                        dag2_nodedata_poseanimsource& NodeData,
                                        dag2_instance&                Instance);
bool UnbindDAGInstanceData_PoseAnimSource(dag2_node&                    Node,
                                          dag2_nodedata_poseanimsource& NodeData,
                                          dag2_instance&                Instance);
bool BakeOutDAGInstanceData_PoseAnimSource(dag2_node&                    Node,
                                           dag2_nodedata_poseanimsource& NodeData,
                                           dag2_instance&                Instance,
                                           memory_arena&                 Arena);

bool SetFloat_PoseAnimSource(dag2_node& Node,
                             dag2_nodedata_poseanimsource& NodeData,
                             dag2_instance& Instance,
                             char const* ParamName,
                             real32 Value);
bool GetFloat_PoseAnimSource(dag2_node& Node,
                             dag2_nodedata_poseanimsource& NodeData,
                             dag2_instance& Instance,
                             char const* ParamName,
                             real32* Value);
bool SetInt32_PoseAnimSource(dag2_node& Node,
                             dag2_nodedata_poseanimsource& NodeData,
                             dag2_instance& Instance,
                             char const* ParamName,
                             int32 Value);
bool GetInt32_PoseAnimSource(dag2_node& Node,
                             dag2_nodedata_poseanimsource& NodeData,
                             dag2_instance& Instance,
                             char const* ParamName,
                             int32* Value);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_POSEANIMSOURCE_H
#endif
