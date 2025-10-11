#if !defined(GRANNY_DAG2_TIMESHIFT_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_timeshift.h $
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
struct model;


struct dag2_nodedata_timeshift
{
    int32 TypeMarker;

    real32 RateChange;
    real32 Offset;
    bool32 HoldStart;
    bool32 HoldEnd;
    real32 Duration;
};
EXPCONST EXPGROUP(dag2_nodedata_timeshift) extern data_type_definition Dag2NodeDataTimeShiftType[];

dag2_node* NewTimeShift(dag2& TheDag, int32x Edges, memory_arena& Arena);

local_pose*
SampleDAG_TimeShift(dag2_node& Node,
                    dag2_nodedata_timeshift& NodeData,
                    dag2_instance& Instance,
                    int32x OnOutput,
                    real32 AtTime,
                    pose_cache& PoseCache);

dag2_node*
MostInfluentialNode_TimeShift(dag2_node& Node,
                              dag2_nodedata_timeshift& NodeData,
                              dag2_instance& Instance,
                              int32x OnOutput,
                              real32 AtTime,
                              int32x* InfluentialOutput);

real32
SampleDAGScalar_TimeShift(dag2_node& Node,
                          dag2_nodedata_timeshift& NodeData,
                          dag2_instance& Instance,
                          int32x OnOutput,
                          real32 AtTime);

bool
GetRootMotion_TimeShift(dag2_node& Node,
                        dag2_nodedata_timeshift& NodeData,
                        dag2_instance& Instance,
                        int32x OnOutput,
                        real32 SecondsElapsed,
                        real32 AtTime,
                        triple& ResultTranslation,
                        triple& ResultRotation);

int32 GetInstanceDataSize_TimeShift(dag2_node& Node, dag2_nodedata_timeshift&,
                                    dag2& TheDag, model& FromModel, model& ToModel);
bool BindDAGInstanceData_TimeShift(dag2_node&                    Node,
                                   dag2_nodedata_timeshift& NodeData,
                                   dag2_instance&                Instance);
bool UnbindDAGInstanceData_TimeShift(dag2_node&                    Node,
                                     dag2_nodedata_timeshift& NodeData,
                                     dag2_instance&                Instance);
bool BakeOutDAGInstanceData_TimeShift(dag2_node&                    Node,
                                      dag2_nodedata_timeshift& NodeData,
                                      dag2_instance&                Instance,
                                      memory_arena&                 Arena);

bool SetFloat_TimeShift(dag2_node& Node,
                        dag2_nodedata_timeshift& NodeData,
                        dag2_instance& Instance,
                        char const* ParamName,
                        real32 Value);
bool GetFloat_TimeShift(dag2_node& Node,
                        dag2_nodedata_timeshift& NodeData,
                        dag2_instance& Instance,
                        char const* ParamName,
                        real32* Value);

bool SetInt32_TimeShift(dag2_node& Node,
                        dag2_nodedata_timeshift& NodeData,
                        dag2_instance& Instance,
                        char const* ParamName,
                        int32 Value);
bool GetInt32_TimeShift(dag2_node& Node,
                        dag2_nodedata_timeshift& NodeData,
                        dag2_instance& Instance,
                        char const* ParamName,
                        int32* Value);



END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DAG2_TIMESHIFT_H
#endif /* GRANNY_DAG2_TIMESHIFT_H */
