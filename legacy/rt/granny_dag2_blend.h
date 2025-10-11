#if !defined(GRANNY_DAG2_BLEND_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_blend.h $
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
struct pose_cache;
struct data_type_definition;
struct local_pose;
struct memory_arena;
struct skeleton;
struct track_mask;

struct dag2_nodedata_blend
{
    int32 TypeMarker;
};
EXPCONST EXPGROUP(dag2_nodedata_blend) extern data_type_definition Dag2NodeDataBlendType[];

dag2_node* NewBlendNode(dag2& TheDag, memory_arena& Arena);

dag2_node*
MostInfluentialNode_Blend(dag2_node& Node,
                          dag2_nodedata_blend& NodeData,
                          dag2_instance& Instance,
                          int32x OnOutput,
                          real32 AtTime,
                          int32x* InfluentialOutput);


// Fast, runtime api
local_pose*
SampleDAG_Blend(dag2_node& Node,
                dag2_nodedata_blend& NodeData,
                dag2_instance& Instance,
                int32x OnOutput,
                real32 AtTime,
                pose_cache& PoseCache);

bool GetRootMotion_Blend(dag2_node& Node,
                         dag2_nodedata_blend& NodeData,
                         dag2_instance& Instance,
                         int32x OnOutput,
                         real32 SecondsElapsed,
                         real32 AtTime,
                         triple& ResultTranslation,
                         triple& ResultRotation);

// Utility function, shared across a couple of other dag nodes
void MaskedBlend(skeleton const& Skeleton,
                 local_pose& FromPose,
                 local_pose const& ToPose,
                 real32 Weight,
                 track_mask const* Mask);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_BLEND_H
#endif
