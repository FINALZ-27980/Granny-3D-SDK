// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_instance.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_instance.h"

#include "granny_dag2_additive_blend.h"
#include "granny_dag2_blend.h"
#include "granny_dag2_callback.h"
#include "granny_dag2_locked_blend.h"
#include "granny_dag2_nway_blend.h"
#include "granny_dag2_poseanimsource.h"
#include "granny_dag2_scalarsource.h"
#include "granny_dag2_scaleoffset.h"
#include "granny_dag2_selection.h"
#include "granny_dag2_sequence.h"
#include "granny_dag2_subgraph.h"
#include "granny_dag2_timeshift.h"
#include "granny_dag2_trackmask.h"
#include "granny_dag2_utilities.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_dag2.h"
#include "granny_pose_cache.h"
#include "granny_local_pose.h"
#include "granny_memory.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_model.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_retargeter.h"
#include "granny_skeleton.h"
#include "granny_track_group.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode BlendDagLogMessage
USING_GRANNY_NAMESPACE;


// -----------------------------------------------------------------------------
dag2_instance* GRANNY
InstantiateDAG(dag2& TheDag,
               model& FromModel,
               model& ToModel)
{
    CheckCondition(IsValidNodeIndex(TheDag.RootNodeIndex), return 0);
    dag2_instance* Instance = 0;

    dag2_node* RootNode = GetDag2Root(TheDag);
    CheckPointerNotNull(RootNode, return 0);

    // See if we need to set the offsets.  DAGs on export have this member set to 0
    int32x InstanceDataSize = GetInstanceDataSize(TheDag, *RootNode, FromModel, ToModel);
    if (InstanceDataSize != 0 && TheDag.InstanceDataSize != InstanceDataSize)
    {
        int32 CurrentOffset = 0;
        FreezeDAGInstanceOffsets(TheDag, *RootNode, CurrentOffset, FromModel, ToModel);
        TheDag.InstanceDataSize = InstanceDataSize;
        Assert(CurrentOffset == InstanceDataSize);
    }

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);
    AggrAllocPtr(Allocator, Instance);
    AggrAllocOffsetCountlessArrayPtr(Allocator, Instance, TheDag.InstanceDataSize, InstanceData);
    if (EndAggrAlloc(Allocator, AllocationInstance))
    {
        Instance->SourceDAG   = &TheDag;
        Instance->SourceModel = &FromModel;
        Instance->TargetModel = &ToModel;
        Instance->Retargeter  = 0;

        // Handle the instance data
        ZeroArray(InstanceDataSize, Instance->InstanceData);
        if (!BindDAGInstanceData(*Instance, Instance->SourceDAG->RootNodeIndex))
        {
            FreeDAGInstance(Instance);
            return 0;
        }

        // Handle the retargeter, if necessary
        if (Instance->SourceModel != Instance->TargetModel)
        {
            Instance->Retargeter =
                AcquireRetargeter(Instance->SourceModel, Instance->TargetModel);
            if (Instance->Retargeter == 0)
            {
                FreeDAGInstance(Instance);
                return 0;
            }
        }
    }

    return Instance;
}

void GRANNY
FreeDAGInstance(dag2_instance* Instance)
{
    if (Instance == 0)
        return;

    CheckPointerNotNull(Instance->SourceDAG, return);
    CheckCondition(IsValidNodeIndex(Instance->SourceDAG->RootNodeIndex), return);
    UnbindDAGInstanceData(*Instance, Instance->SourceDAG->RootNodeIndex);

    // Release the retargeter (ok if null)
    ReleaseRetargeter(Instance->Retargeter);

    Deallocate(Instance);
}

dag2* GRANNY
GetSourceDAG(dag2_instance* Instance)
{
    CheckPointerNotNull(Instance, return 0);

    return Instance->SourceDAG;
}

model* GRANNY
GetDAGTargetModel(dag2_instance* Instance)
{
    CheckPointerNotNull(Instance, return 0);

    return Instance->TargetModel;
}


bool GRANNY
SetDag2NodeFloat(dag2_instance& Instance, dag2_node& Node,
                 char const* ParamName, real32 Value)
{
#define DISPATCH_ARGS Instance, ParamName, Value
#define DISPATCH_RESULT Result
    bool Result = false;
    TYPE_DISPATCH(Node)
        NODE_DISPATCH(ScalarSource,   scalarsource,   SetFloat_ScalarSource)
        NODE_DISPATCH(ScaleOffset,    scaleoffset,    SetFloat_ScaleOffset)
        NODE_DISPATCH(PoseAnimSource, poseanimsource, SetFloat_PoseAnimSource)
        NODE_DISPATCH(TimeShift,      timeshift,      SetFloat_TimeShift)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}

bool GRANNY
GetDag2NodeFloat(dag2_instance& Instance, dag2_node& Node,
                 char const* ParamName, real32* Value)
{
#define DISPATCH_ARGS Instance, ParamName, Value
#define DISPATCH_RESULT Result
    bool Result = false;
    TYPE_DISPATCH(Node)
        NODE_DISPATCH(ScalarSource,   scalarsource,   GetFloat_ScalarSource)
        NODE_DISPATCH(ScaleOffset,    scaleoffset,    GetFloat_ScaleOffset)
        NODE_DISPATCH(PoseAnimSource, poseanimsource, GetFloat_PoseAnimSource)
        NODE_DISPATCH(Sequence,       sequence,       GetFloat_Sequence)
        NODE_DISPATCH(TimeShift,      timeshift,      GetFloat_TimeShift)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}


bool GRANNY
SetDag2NodeInt32(dag2_instance& Instance, dag2_node& Node,
                 char const* ParamName, int32 Value)
{
#define DISPATCH_ARGS Instance, ParamName, Value
#define DISPATCH_RESULT Result
    bool Result = false;
    TYPE_DISPATCH(Node)
        NODE_DISPATCH(TimeShift, timeshift, SetInt32_TimeShift)
        NODE_DISPATCH(Selection, selection, SetInt32_Selection)
        NODE_DISPATCH(PoseAnimSource, poseanimsource, SetInt32_PoseAnimSource)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}


bool GRANNY
GetDag2NodeInt32(dag2_instance& Instance, dag2_node& Node,
                 char const* ParamName, int32* Value)
{
#define DISPATCH_ARGS Instance, ParamName, Value
#define DISPATCH_RESULT Result
    bool Result = false;
    TYPE_DISPATCH(Node)
        NODE_DISPATCH(TimeShift,      timeshift,      GetInt32_TimeShift)
        NODE_DISPATCH(Selection,      selection,      GetInt32_Selection)
        NODE_DISPATCH(PoseAnimSource, poseanimsource, GetInt32_PoseAnimSource)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}


dag2_node* GRANNY
FindMostInfluentialNodeInternal(dag2_instance& Instance,
                                int32x         NodeIndex,
                                int32x         OnOutput,
                                real32         AtTime,
                                int32x*        InfluentialOutput)
{
    CheckPointerNotNull(Instance.SourceDAG, return 0);

    dag2_node* Node = GetDag2Node(*Instance.SourceDAG, NodeIndex);
    if (!Node)
        return 0;
    CheckInt32Index(OnOutput, Node->TotalOutputEdgeCount, return 0);

    // If this is a forwarded output, check the corresponding input.  If the forwarded
    // input isn't connected, just return a rest pose
    if (Node->OutputEdges[OnOutput].Forwarded)
    {
        dag2_input_edge const& ForwardedInput =
            Node->InputEdges[Node->OutputEdges[OnOutput].ForwardIndex];
        if (IsValidNodeIndex(ForwardedInput.InputNodeIndex))
        {
            return FindMostInfluentialNodeInternal(Instance,
                                                   ForwardedInput.InputNodeIndex,
                                                   ForwardedInput.ConnectedOutput,
                                                   AtTime, InfluentialOutput);
        }
        else
        {
            return 0;
        }
    }

    // Default to this node, on the specified output
    dag2_node* Result = Node;
    SetByValueNotNull(InfluentialOutput, OnOutput);

#define DISPATCH_ARGS Instance, OnOutput, AtTime, InfluentialOutput
#define DISPATCH_RESULT Result
    TYPE_DISPATCH(*Node)
        NODE_DISPATCH(Blend,       blend,        MostInfluentialNode_Blend)
        NODE_DISPATCH(LockedBlend, locked_blend, MostInfluentialNode_LockedBlend)
        NODE_DISPATCH(ScaleOffset, scaleoffset,  MostInfluentialNode_ScaleOffset)
        NODE_DISPATCH(Selection,   selection,    MostInfluentialNode_Selection)
        NODE_DISPATCH(Subgraph,    subgraph,     MostInfluentialNode_Subgraph)
        NODE_DISPATCH(TimeShift,   timeshift,    MostInfluentialNode_TimeShift)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}



dag2_node* GRANNY
FindMostInfluentialNode(dag2_instance& Instance,
                        int32x OnOutput,
                        real32 AtTime,
                        int32x* InfluentialOutput)
{
    CheckPointerNotNull(Instance.SourceDAG, return 0);
    CheckCondition(OnOutput >= 0, return 0);

    return FindMostInfluentialNodeInternal(Instance,
                                           Instance.SourceDAG->RootNodeIndex,
                                           OnOutput, AtTime, InfluentialOutput);
}


local_pose* GRANNY
SampleDAGNode(dag2_instance& Instance,
              int32x NodeIndex,
              int32x OnOutput,
              real32 AtTime,
              pose_cache& PoseCache)
{
    CheckCondition(IsValidNodeIndex(NodeIndex), return 0);
    CheckInt32Index(NodeIndex, Instance.SourceDAG->NodeCount, return 0);

    dag2_node* AtNode = GetDag2Node(*Instance.SourceDAG, NodeIndex);
    CheckPointerNotNull(AtNode, return 0);

    CheckInt32Index(OnOutput, AtNode->TotalOutputEdgeCount, return 0);
    CheckCondition(AtNode->OutputEdges[OnOutput].ParameterType == PoseParameterType,
                   return 0);

    // If this is a forwarded output, check the corresponding input.  If the forwarded
    // input isn't connected, just return a rest pose
    if (AtNode->OutputEdges[OnOutput].Forwarded)
    {
        dag2_input_edge const& ForwardedInput = AtNode->InputEdges[AtNode->OutputEdges[OnOutput].ForwardIndex];
        if (IsValidNodeIndex(ForwardedInput.InputNodeIndex))
        {
            local_pose* Result = SampleDAGNode(Instance,
                                               ForwardedInput.InputNodeIndex,
                                               ForwardedInput.ConnectedOutput,
                                               AtTime, PoseCache);
            if (!Result)
                Result = GetNewRestLocalPose(PoseCache, *Instance.SourceModel);
            return Result;
        }
        else
        {
            return GetNewRestLocalPose(PoseCache, *Instance.SourceModel);
        }
    }

#define DISPATCH_ARGS Instance, OnOutput, AtTime, PoseCache
#define DISPATCH_RESULT Result
    local_pose* Result = 0;
    TYPE_DISPATCH(*AtNode)
        NODE_DISPATCH(AdditiveBlend,  additiveblend,  SampleDAG_AdditiveBlend)
        NODE_DISPATCH(Blend,          blend,          SampleDAG_Blend)
        NODE_DISPATCH(Callback,       callback,       SampleDAG_Callback)
        NODE_DISPATCH(LockedBlend,    locked_blend,   SampleDAG_LockedBlend)
        NODE_DISPATCH(NWayBlend,      nwayblend,      SampleDAG_NWayBlend)
        NODE_DISPATCH(PoseAnimSource, poseanimsource, SampleDAG_PoseAnimSource)
        NODE_DISPATCH(Selection,      selection,      SampleDAG_Selection)
        NODE_DISPATCH(Sequence,       sequence,       SampleDAG_Sequence)
        NODE_DISPATCH(Subgraph,       subgraph,       SampleDAG_Subgraph)
        NODE_DISPATCH(TimeShift,      timeshift,      SampleDAG_TimeShift)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    if (!Result)
        Result = GetNewRestLocalPose(PoseCache, *Instance.SourceModel);

    return Result;
}


track_mask* GRANNY
SampleDAGTrackmask(dag2_instance& Instance,
                   int32x OnOutput,
                   real32 AtTime)
{
    InvalidCodePath("nyi");
    return 0;
}


track_mask* GRANNY
SampleDAGNodeTrackmask(dag2_instance& Instance,
                       int32x NodeIndex,
                       int32x OnOutput,
                       real32 AtTime)
{
    CheckCondition(IsValidNodeIndex(NodeIndex), return 0);
    CheckInt32Index(NodeIndex, Instance.SourceDAG->NodeCount, return 0);

    dag2_node* AtNode = GetDag2Node(*Instance.SourceDAG, NodeIndex);
    CheckPointerNotNull(AtNode, return 0);

    CheckInt32Index(OnOutput, AtNode->TotalOutputEdgeCount, return 0);
    CheckCondition(AtNode->OutputEdges[OnOutput].ParameterType == TrackmaskParameterType,
                   return 0);

    // If this is a forwarded output, check the corresponding input.  If the forwarded
    // input isn't connected, just return a rest pose
    if (AtNode->OutputEdges[OnOutput].Forwarded)
    {
        dag2_input_edge const& ForwardedInput = AtNode->InputEdges[AtNode->OutputEdges[OnOutput].ForwardIndex];
        if (IsValidNodeIndex(ForwardedInput.InputNodeIndex))
        {
            return SampleDAGNodeTrackmask(Instance,
                                          ForwardedInput.InputNodeIndex,
                                          ForwardedInput.ConnectedOutput,
                                          AtTime);
        }
        else
        {
            return 0;
        }
    }

#define DISPATCH_ARGS Instance, OnOutput, AtTime
#define DISPATCH_RESULT Result
    track_mask* Result = 0;
    TYPE_DISPATCH(*AtNode)
        NODE_DISPATCH(Trackmask,      trackmask,      SampleDAGTrackmask_Trackmask)
        NODE_DISPATCH(Subgraph,       subgraph,       SampleDAGTrackmask_Subgraph)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}


local_pose* GRANNY
SampleDAG(dag2_instance& Instance,
          int32x OnOutput,
          real32 AtTime,
          pose_cache& PoseCache)
{
    CheckPointerNotNull(Instance.SourceDAG, return 0);
    CheckCondition(IsValidNodeIndex(Instance.SourceDAG->RootNodeIndex), return 0);
    CheckPointerNotNull(Instance.TargetModel, return 0);
    CheckPointerNotNull(Instance.TargetModel->Skeleton, return 0);
    CheckCondition(OnOutput >= 0, return 0);

    local_pose* SampledPose = SampleDAGNode(Instance, Instance.SourceDAG->RootNodeIndex,
                                            OnOutput, AtTime, PoseCache);
    if (SampledPose)
    {
        if (Instance.Retargeter)
        {
            local_pose* RetargetedPose =
                GetNewLocalPose(PoseCache, (int32x)Instance.Retargeter->NumBones);
            if (RetargetedPose)
            {
                if (RetargetPose(*SampledPose, *RetargetedPose,
                                 0, (int32x)Instance.Retargeter->NumBones,
                                 *Instance.Retargeter))
                {
                    ReleaseCachePose(PoseCache, SampledPose);
                    SampledPose = RetargetedPose;
                }
                else
                {
                    // todo: Error!
                    ReleaseCachePose(PoseCache, RetargetedPose);
                }
            }
            else
            {
                // todo: Error!
            }
        }

        // Lifetime of this pose isn't guaranteed, release it.
        ReleaseCachePose(PoseCache, SampledPose);
    }

    return SampledPose;
}


bool GRANNY
GetDagNodeRootMotionVectors(dag2_instance& Instance,
                            int32x NodeIndex,
                            int32x OnOutput,
                            real32 SecondsElapsed,
                            real32 AtTime,
                            triple& ResultTranslation,
                            triple& ResultRotation)
{
    CheckCondition(IsValidNodeIndex(NodeIndex), return 0);
    CheckInt32Index(NodeIndex, Instance.SourceDAG->NodeCount, return 0);

    dag2_node* AtNode = GetDag2Node(*Instance.SourceDAG, NodeIndex);
    CheckPointerNotNull(AtNode, return 0);

    CheckInt32Index(OnOutput, AtNode->TotalOutputEdgeCount, return 0);
    CheckCondition(AtNode->OutputEdges[OnOutput].ParameterType == PoseParameterType, return 0);

    // If this is a forwarded output, check the corresponding input.  If the forwarded
    // input isn't connected, just return a rest pose
    if (AtNode->OutputEdges[OnOutput].Forwarded)
    {
        dag2_input_edge const& ForwardedInput = AtNode->InputEdges[AtNode->OutputEdges[OnOutput].ForwardIndex];
        if (IsValidNodeIndex(ForwardedInput.InputNodeIndex))
        {
            return GetDagNodeRootMotionVectors(Instance, ForwardedInput.InputNodeIndex,
                                               ForwardedInput.ConnectedOutput,
                                               SecondsElapsed, AtTime,
                                               ResultTranslation,
                                               ResultRotation);
        }
        else
        {
            return 0.0f;
        }
    }

#define DISPATCH_ARGS Instance, OnOutput, SecondsElapsed, AtTime, ResultTranslation, ResultRotation
#define DISPATCH_RESULT Result
    bool Result = false;
    TYPE_DISPATCH(*AtNode)
        NODE_DISPATCH(AdditiveBlend,  additiveblend,  GetRootMotion_AdditiveBlend)
        NODE_DISPATCH(Blend,          blend,          GetRootMotion_Blend)
        NODE_DISPATCH(NWayBlend,      nwayblend,      GetRootMotion_NWayBlend)
        NODE_DISPATCH(LockedBlend,    locked_blend,   GetRootMotion_LockedBlend)
        NODE_DISPATCH(Callback,       callback,       GetRootMotion_Callback)
        NODE_DISPATCH(PoseAnimSource, poseanimsource, GetRootMotion_PoseAnimSource)
        NODE_DISPATCH(Selection,      selection,      GetRootMotion_Selection)
        NODE_DISPATCH(Sequence,       sequence,       GetRootMotion_Sequence)
        NODE_DISPATCH(Subgraph,       subgraph,       GetRootMotion_Subgraph)
        NODE_DISPATCH(TimeShift,      timeshift,      GetRootMotion_TimeShift)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}


bool GRANNY
UpdateDAGModelMatrix(dag2_instance& Instance,
                     int32x OnOutput,
                     real32 SecondsElapsed,
                     real32 AtTime,
                     real32 const *ModelMatrix4x4,
                     real32 *DestMatrix4x4)
{
    triple ResultTranslation;
    triple ResultRotation;

    if (GetDagNodeRootMotionVectors(Instance, Instance.SourceDAG->RootNodeIndex,
                                    OnOutput, SecondsElapsed, AtTime,
                                    ResultTranslation, ResultRotation))
    {
        ApplyRootMotionVectorsToMatrix(ModelMatrix4x4,
                                       ResultTranslation, ResultRotation,
                                       DestMatrix4x4);
        return true;
    }

    return false;
}


real32 GRANNY
SampleDAGNodeScalar(dag2_instance& Instance,
                    int32x NodeIndex,
                    int32x OnOutput,
                    real32 AtTime)
{
    CheckCondition(IsValidNodeIndex(NodeIndex), return 0);
    CheckInt32Index(NodeIndex, Instance.SourceDAG->NodeCount, return 0);

    dag2_node* AtNode = GetDag2Node(*Instance.SourceDAG, NodeIndex);
    CheckPointerNotNull(AtNode, return 0);

    CheckInt32Index(OnOutput, AtNode->TotalOutputEdgeCount, return 0);
    CheckCondition(AtNode->OutputEdges[OnOutput].ParameterType == FloatParameterType, return 0);

    // If this is a forwarded output, check the corresponding input.  If the forwarded
    // input isn't connected, just return a rest pose
    if (AtNode->OutputEdges[OnOutput].Forwarded)
    {
        dag2_input_edge const& ForwardedInput = AtNode->InputEdges[AtNode->OutputEdges[OnOutput].ForwardIndex];
        if (IsValidNodeIndex(ForwardedInput.InputNodeIndex))
        {
            return SampleDAGNodeScalar(Instance, ForwardedInput.InputNodeIndex,
                                       ForwardedInput.ConnectedOutput, AtTime);
        }
        else
        {
            return 0.0f;
        }
    }

#define DISPATCH_ARGS Instance, OnOutput, AtTime
#define DISPATCH_RESULT Result
    real32 Result = 0;
    TYPE_DISPATCH(*AtNode)
        NODE_DISPATCH(Callback,       callback,      SampleDAGScalar_Callback)
        NODE_DISPATCH(ScalarSource,   scalarsource,  SampleDAGScalar_ScalarSource)
        NODE_DISPATCH(ScaleOffset,    scaleoffset,   SampleDAGScalar_ScaleOffset)
        NODE_DISPATCH(Subgraph,       subgraph,      SampleDAGScalar_Subgraph)
        NODE_DISPATCH(TimeShift,      timeshift,     SampleDAGScalar_TimeShift)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}


real32 GRANNY
SampleDAGScalar(dag2_instance& Instance,
                int32x OnOutput,
                real32 AtTime)
{
    CheckPointerNotNull(Instance.SourceDAG, return false);
    CheckCondition(IsValidNodeIndex(Instance.SourceDAG->RootNodeIndex), return false);
    CheckPointerNotNull(Instance.TargetModel, return false);
    CheckPointerNotNull(Instance.TargetModel->Skeleton, return false);
    CheckCondition(OnOutput >= 0, return false);

    return SampleDAGNodeScalar(Instance, Instance.SourceDAG->RootNodeIndex,
                               OnOutput, AtTime);
}


bool GRANNY
SampleDAGLockParamsInternal(dag2_instance& Instance,
                            int32x NodeIndex,
                            int32x OnOutput,
                            real32 AtTime,
                            real32& Duration)
{
    CheckCondition(IsValidNodeIndex(NodeIndex), return 0);
    CheckInt32Index(NodeIndex, Instance.SourceDAG->NodeCount, return 0);

    dag2_node* AtNode = GetDag2Node(*Instance.SourceDAG, NodeIndex);
    CheckPointerNotNull(AtNode, return 0);

    CheckInt32Index(OnOutput, AtNode->TotalOutputEdgeCount, return 0);
    CheckCondition(AtNode->OutputEdges[OnOutput].ParameterType == PoseParameterType, return 0);

    // If this is a forwarded output, check the corresponding input.  If the forwarded
    // input isn't connected, just return a rest pose
    if (AtNode->OutputEdges[OnOutput].Forwarded)
    {
        dag2_input_edge const& ForwardedInput = AtNode->InputEdges[AtNode->OutputEdges[OnOutput].ForwardIndex];
        if (IsValidNodeIndex(ForwardedInput.InputNodeIndex))
        {
            return SampleDAGLockParamsInternal(Instance, ForwardedInput.InputNodeIndex,
                                               ForwardedInput.ConnectedOutput, AtTime, Duration);
        }
        else
        {
            return false;
        }
    }

#define DISPATCH_ARGS Instance, OnOutput, AtTime, Duration
#define DISPATCH_RESULT Result
    bool Result = false;
    TYPE_DISPATCH(*AtNode)
        NODE_DISPATCH(PoseAnimSource, poseanimsource, SampleDAGLockParams_PoseAnimSource)
        NODE_DISPATCH(LockedBlend,    locked_blend,   SampleDAGLockParams_LockedBlend)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}


bool GRANNY
BindDAGInstanceData(dag2_instance& Instance, int32x NodeIndex)
{
    dag2_node* Node = GetDag2Node(*Instance.SourceDAG, NodeIndex);
    if (!Node)
        return true;

#define DISPATCH_ARGS Instance
#define DISPATCH_RESULT Result
    bool Result = true;
    TYPE_DISPATCH(*Node)
        NODE_DISPATCH(Callback,       callback,       BindDAGInstanceData_Callback)
        NODE_DISPATCH(PoseAnimSource, poseanimsource, BindDAGInstanceData_PoseAnimSource)
        NODE_DISPATCH(LockedBlend,    locked_blend,   BindDAGInstanceData_LockedBlend)
        NODE_DISPATCH(ScalarSource,   scalarsource,   BindDAGInstanceData_ScalarSource)
        NODE_DISPATCH(ScaleOffset,    scaleoffset,    BindDAGInstanceData_ScaleOffset)
        NODE_DISPATCH(Selection,      selection,      BindDAGInstanceData_Selection)
        NODE_DISPATCH(Sequence,       sequence,       BindDAGInstanceData_Sequence)
        NODE_DISPATCH(Subgraph,       subgraph,       BindDAGInstanceData_Subgraph)
        NODE_DISPATCH(TimeShift,      timeshift,      BindDAGInstanceData_TimeShift)
        NODE_DISPATCH(Trackmask,      trackmask,      BindDAGInstanceData_Trackmask)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}


bool GRANNY
UnbindDAGInstanceData(dag2_instance& Instance, int32x NodeIndex)
{
    dag2_node* Node = GetDag2Node(*Instance.SourceDAG, NodeIndex);
    if (!Node)
        return true;

#define DISPATCH_ARGS Instance
#define DISPATCH_RESULT Result
    bool Result = true;
    TYPE_DISPATCH(*Node)
        NODE_DISPATCH(Callback,       callback,       UnbindDAGInstanceData_Callback)
        NODE_DISPATCH(PoseAnimSource, poseanimsource, UnbindDAGInstanceData_PoseAnimSource)
        NODE_DISPATCH(LockedBlend,    locked_blend,   UnbindDAGInstanceData_LockedBlend)
        NODE_DISPATCH(ScalarSource,   scalarsource,   UnbindDAGInstanceData_ScalarSource)
        NODE_DISPATCH(ScaleOffset,    scaleoffset,    UnbindDAGInstanceData_ScaleOffset)
        NODE_DISPATCH(Selection,      selection,      UnbindDAGInstanceData_Selection)
        NODE_DISPATCH(Sequence,       sequence,       UnbindDAGInstanceData_Sequence)
        NODE_DISPATCH(Subgraph,       subgraph,       UnbindDAGInstanceData_Subgraph)
        NODE_DISPATCH(TimeShift,      timeshift,      UnbindDAGInstanceData_TimeShift)
        NODE_DISPATCH(Trackmask,      trackmask,      UnbindDAGInstanceData_Trackmask)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}

bool GRANNY
BakeOutDAGInstanceData(dag2_instance& Instance, int32x NodeIndex,
                       memory_arena& Arena)
{
    dag2_node* Node = GetDag2Node(*Instance.SourceDAG, NodeIndex);
    if (!Node)
        return true;

#define DISPATCH_ARGS Instance, Arena
#define DISPATCH_RESULT Result
    bool Result = true;
    TYPE_DISPATCH(*Node)
        NODE_DISPATCH(PoseAnimSource, poseanimsource, BakeOutDAGInstanceData_PoseAnimSource)
        NODE_DISPATCH(ScalarSource,   scalarsource,   BakeOutDAGInstanceData_ScalarSource)
        NODE_DISPATCH(ScaleOffset,    scaleoffset,    BakeOutDAGInstanceData_ScaleOffset)
        NODE_DISPATCH(Selection,      selection,      BakeOutDAGInstanceData_Selection)
        NODE_DISPATCH(Sequence,       sequence,       BakeOutDAGInstanceData_Sequence)
        NODE_DISPATCH(Subgraph,       subgraph,       BakeOutDAGInstanceData_Subgraph)
        NODE_DISPATCH(TimeShift,      timeshift,      BakeOutDAGInstanceData_TimeShift)
        NODE_DISPATCH(Trackmask,      trackmask,      BakeOutDAGInstanceData_Trackmask)
        END_TYPE_DISPATCH();
#undef DISPATCH_ARGS
#undef DISPATCH_RESULT

    return Result;
}

