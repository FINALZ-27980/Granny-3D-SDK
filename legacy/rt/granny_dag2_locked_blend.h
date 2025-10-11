#if !defined(GRANNY_DAG2_LOCKED_BLEND_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_locked_blend.h $
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
struct dag2_instance;
struct pose_cache;
struct data_type_definition;
struct model;
struct local_pose;
struct memory_arena;

struct dag2_nodedata_locked_blend
{
    int32 TypeMarker;
};
EXPCONST EXPGROUP(dag2_nodedata_locked_blend) extern data_type_definition Dag2NodeDataLockedBlendType[];

dag2_node* NewLockedBlendNode(dag2& TheDag, memory_arena& Arena);

dag2_node*
MostInfluentialNode_LockedBlend(dag2_node& Node,
                                dag2_nodedata_locked_blend& NodeData,
                                dag2_instance& Instance,
                                int32x OnOutput,
                                real32 AtTime,
                                int32x* InfluentialOutput);

// Fast, runtime api
local_pose*
SampleDAG_LockedBlend(dag2_node& Node,
                      dag2_nodedata_locked_blend& NodeData,
                      dag2_instance& Instance,
                      int32x OnOutput,
                      real32 AtTime,
                      pose_cache& PoseCache);

bool GetRootMotion_LockedBlend(dag2_node& Node,
                               dag2_nodedata_locked_blend& NodeData,
                               dag2_instance& Instance,
                               int32x OnOutput,
                               real32 SecondsElapsed,
                               real32 AtTime,
                               triple& ResultTranslation,
                               triple& ResultRotation);

bool SampleDAGLockParams_LockedBlend(dag2_node& Node,
                                     dag2_nodedata_locked_blend& NodeData,
                                     dag2_instance& Instance,
                                     int32x OnOutput,
                                     real32 AtTime,
                                     real32& Duration);

int32 GetInstanceDataSize_LockedBlend(dag2_node& Node, dag2_nodedata_locked_blend&,
                                      dag2& TheDag, model& FromModel, model& ToModel);
bool BindDAGInstanceData_LockedBlend(dag2_node&                    Node,
                                     dag2_nodedata_locked_blend& NodeData,
                                     dag2_instance&                Instance);
bool UnbindDAGInstanceData_LockedBlend(dag2_node&                    Node,
                                       dag2_nodedata_locked_blend& NodeData,
                                       dag2_instance&                Instance);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_LOCKED_BLEND_H
#endif /* GRANNY_DAG2_LOCKED_BLEND_H */
