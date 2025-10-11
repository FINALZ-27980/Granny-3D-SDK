#if !defined(GRANNY_DAG2_INSTANCE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_instance.h $
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
struct dag2_node;
struct pose_cache;
struct local_pose;
struct memory_arena;
struct model;
struct retargeter;
struct text_track_entry;
struct track_mask;

EXPTYPE struct dag2_instance;
struct dag2_instance
{
    dag2*       SourceDAG;

    model*      SourceModel;
    model*      TargetModel;

    retargeter* Retargeter;

    uint8*  InstanceData;
};


EXPAPI GS_MODIFY dag2_instance *InstantiateDAG(dag2& DAG,
                                               model &FromModel,
                                               model &ToModel);
EXPAPI GS_MODIFY void FreeDAGInstance(dag2_instance *Instance);

EXPAPI GS_READ dag2*  GetSourceDAG(dag2_instance* Instance);
EXPAPI GS_READ model* GetDAGTargetModel(dag2_instance* Instance);


EXPAPI GS_MODIFY bool SetDag2NodeFloat(dag2_instance& Instance,
                                       dag2_node& Node,
                                       char const* ParamName,
                                       real32 Value);
EXPAPI GS_MODIFY bool GetDag2NodeFloat(dag2_instance& Instance,
                                       dag2_node& Node,
                                       char const* ParamName,
                                       real32* Value);
EXPAPI GS_MODIFY bool SetDag2NodeInt32(dag2_instance& Instance,
                                       dag2_node& Node,
                                       char const* ParamName,
                                       int32 Value);
EXPAPI GS_MODIFY bool GetDag2NodeInt32(dag2_instance& Instance,
                                       dag2_node& Node,
                                       char const* ParamName,
                                       int32* Value);

EXPAPI GS_PARAM dag2_node* FindMostInfluentialNode(dag2_instance& Instance,
                                                   int32x OnOutput,
                                                   real32 AtTime,
                                                   int32x* InfluentialOutput);
dag2_node* FindMostInfluentialNodeInternal(dag2_instance& Instance,
                                           int32x NodeIndex,
                                           int32x OnOutput,
                                           real32 AtTime,
                                           int32x* InfluentialOutput);

EXPAPI GS_MODIFY local_pose* SampleDAG(dag2_instance& Instance,
                                       int32x OnOutput,
                                       real32 AtTime,
                                       pose_cache& PoseCache);
EXPAPI GS_MODIFY local_pose* SampleDAGNode(dag2_instance& Instance,
                                           int32x NodeIndex,
                                           int32x OnOutput,
                                           real32 AtTime,
                                           pose_cache& PoseCache);

EXPAPI GS_MODIFY track_mask* SampleDAGTrackmask(dag2_instance& Instance,
                                                int32x OnOutput,
                                                real32 AtTime);
EXPAPI GS_MODIFY track_mask* SampleDAGNodeTrackmask(dag2_instance& Instance,
                                                    int32x NodeIndex,
                                                    int32x OnOutput,
                                                    real32 AtTime);


EXPAPI GS_MODIFY real32 SampleDAGScalar(dag2_instance& Instance,
                                        int32x OnOutput,
                                        real32 AtTime);
EXPAPI GS_MODIFY real32 SampleDAGNodeScalar(dag2_instance& Instance,
                                            int32x NodeIndex,
                                            int32x OnOutput,
                                            real32 AtTime);

EXPAPI GS_MODIFY bool UpdateDAGModelMatrix(dag2_instance& Instance,
                                           int32x OnOutput,
                                           real32 SecondsElapsed,
                                           real32 AtTime,
                                           real32 const *ModelMatrix4x4,
                                           real32 *DestMatrix4x4);

bool BindDAGInstanceData(dag2_instance& Instance, int32x NodeIndex);
bool UnbindDAGInstanceData(dag2_instance& Instance, int32x NodeIndex);

// The opposite of Bind, basically.  During editing, we often need to take data from the
// instance, and move it back into the source dag.
bool BakeOutDAGInstanceData(dag2_instance& Instance, int32x NodeIndex, memory_arena& Arena);

#define GET_DAG2INST_DATA(Type, Instance, Node) \
    ((Type*)((Instance).InstanceData + (Node).InstanceOffset))


bool GetDagNodeRootMotionVectors(dag2_instance& Instance,
                                 int32x NodeIndex,
                                 int32x OnOutput,
                                 real32 SecondsElapsed,
                                 real32 AtTime,
                                 triple& ResultTranslation,
                                 triple& ResultRotation);

bool SampleDAGLockParamsInternal(dag2_instance& Instance,
                                 int32x NodeIndex,
                                 int32x OnOutput,
                                 real32 AtTime,
                                 real32& Duration);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_INSTANCE_H
#endif
