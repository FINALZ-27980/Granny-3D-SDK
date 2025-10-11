#if !defined(GRANNY_DAG2_ADDITIVE_BLEND_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_additive_blend.h $
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
struct local_pose;
struct memory_arena;

struct dag2_nodedata_additiveblend
{
    int32 TypeMarker;
};
EXPCONST EXPGROUP(dag2_nodedata_blend) extern data_type_definition Dag2NodeDataAdditiveBlendType[];

dag2_node* NewAdditiveBlendNode(dag2& TheDag, memory_arena& Arena);

// Fast, runtime api
local_pose*
SampleDAG_AdditiveBlend(dag2_node& Node,
                        dag2_nodedata_additiveblend& NodeData,
                        dag2_instance& Instance,
                        int32x OnOutput,
                        real32 AtTime,
                        pose_cache& PoseCache);

bool GetRootMotion_AdditiveBlend(dag2_node& Node,
                            dag2_nodedata_additiveblend& NodeData,
                            dag2_instance& Instance,
                            int32x OnOutput,
                            real32 SecondsElapsed,
                            real32 AtTime,
                            triple& ResultTranslation,
                            triple& ResultRotation);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_ADDITIVE_BLEND_H
#endif /* GRANNY_DAG2_ADDITIVE_BLEND_H */
