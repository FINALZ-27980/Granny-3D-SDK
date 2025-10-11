// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_locked_blend.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_locked_blend.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_dag2.h"
#include "granny_dag2_blend.h"
#include "granny_dag2_instance.h"
#include "granny_dag2_node_registry.h"
#include "granny_dag2_utilities.h"
#include "granny_pose_cache.h"
#include "granny_limits.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_memory_ops.h"
#include "granny_model.h"
#include "granny_parameter_checking.h"

#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

USING_GRANNY_NAMESPACE;
#define SubsystemCode BlendDagLogMessage

IMPL_GET_NODEDATA(LockedBlend, locked_blend);

real32 const LockedBlendNodeDefaultWeight = 0.0f;

data_type_definition GRANNY Dag2NodeDataLockedBlendType[] =
{
    { Int32Member, "TypeMarker_NodeDataLockedBlend" },
    { EndMember },
};

// State in the DAG.  This makes me a sad panda.
struct dag2_instancedata_lb
{
    real32 ObservedWeight;  // if this is -1, it's implied that we haven't seen a valid sample yet
    real32 ObservedTime;

    real32 CompensatingOffset;
};

enum LockedBlendInputEdges
{
    eParam = 0,
    eFrom  = 1,
    eTo    = 2,
};

// We're going to be doing quite a bit of this
static inline real32
Interpolate(real32 From, real32 To, real32 Param)
{
    return From * (1 - Param) + To * Param;
}


dag2_node* GRANNY
NewLockedBlendNode(dag2& TheDag, memory_arena& Arena)
{
    AGGR_NODE(TheDag,
              NewNode, 4, 4, 1, 1,
              NodeState, dag2_nodedata_locked_blend, Dag2NodeDataLockedBlendType,
              Arena);
    if (NewNode)
    {
        NewNode->Name = 0;

        INIT_EXTERNAL_INPUT_EDGE(NewNode, eParam, "Param", Float);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, eFrom,  "From",  Pose);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, eTo,    "To",    Pose);

        INIT_EXTERNAL_OUTPUT_EDGE(NewNode, 0, "Output", Pose);

        Assert(GetNodeDataLockedBlend(NewNode));
    }

    return NewNode;
}


dag2_node* GRANNY
MostInfluentialNode_LockedBlend(dag2_node& Node,
                                dag2_nodedata_locked_blend& NodeData,
                                dag2_instance& Instance,
                                int32x OnOutput,
                                real32 AtTime,
                                int32x* InfluentialOutput)
{
    int32x FromOutput;
    dag2_node* FromNode =
        FindMostInfluentialNodeInternal(Instance,
                                        Node.InputEdges[eFrom].InputNodeIndex,
                                        Node.InputEdges[eFrom].ConnectedOutput,
                                        AtTime, &FromOutput);
    int32x ToOutput;
    dag2_node* ToNode =
        FindMostInfluentialNodeInternal(Instance,
                                        Node.InputEdges[eTo].InputNodeIndex,
                                        Node.InputEdges[eTo].ConnectedOutput,
                                        AtTime, &ToOutput);
    if (FromNode && ToNode)
    {
        real32 Weight = LockedBlendNodeDefaultWeight;
        if (IsValidNodeIndex(Node.InputEdges[eParam].InputNodeIndex))
        {
            Weight = SampleDAGNodeScalar(Instance,
                                         Node.InputEdges[eParam].InputNodeIndex,
                                         Node.InputEdges[eParam].ConnectedOutput,
                                         AtTime);

            // Clamp the weight from 0 to 1, to make the rest of this bit sensible.
            Weight = Clamp(0.0f, Weight, 1.0f);
        }

        if (Weight < 0.5f)
            ToNode = 0;
        else
            FromNode = 0;
    }

    if (FromNode)
    {
        SetByValueNotNull(InfluentialOutput, FromOutput);
        return FromNode;
    }
    else if (ToNode)
    {
        SetByValueNotNull(InfluentialOutput, ToOutput);
        return ToNode;
    }

    // For now, rest-pose sources don't count, if that changes, return "this"
    return 0;
}

static void
UpdateObservedWeight(dag2_instancedata_lb* LockedBlendData,
                     real32 const DurationFrom,
                     real32 const DurationTo,
                     real32 const NewWeight,
                     real32 const NewTime)
{
    Assert(LockedBlendData);
    Assert(DurationFrom > 0);
    Assert(DurationTo > 0);
    Assert(NewWeight >=0 && NewWeight <= 1);

    if (LockedBlendData->ObservedWeight < 0)
    {
        // Brand new.
        LockedBlendData->ObservedWeight     = NewWeight;
        LockedBlendData->ObservedTime       = NewTime;
        LockedBlendData->CompensatingOffset = 0;
        return;
    }

    Assert(LockedBlendData->ObservedWeight >= 0 && LockedBlendData->ObservedWeight <= 1.0f);

    if (NewWeight != LockedBlendData->ObservedWeight)
    {
        real32 const OldDuration = Interpolate(DurationFrom, DurationTo, LockedBlendData->ObservedWeight);
        real32 const OldLoopPos  = (LockedBlendData->ObservedTime + LockedBlendData->CompensatingOffset) / OldDuration;

        real32 const NewDuration = Interpolate(DurationFrom, DurationTo, NewWeight);

        // NewLoopPos = (OldTime + NewCompensate) / NewDuration

        //                               NewLoopPos = OldLoopPos
        //  (OldTime + NewCompensate) / NewDuration = OldLoopPos
        //                            NewCompensate = (OldLoopPos * NewDuration) - OldTime
        real32 NewCompensate = (OldLoopPos * NewDuration) - LockedBlendData->ObservedTime;

        // Q: Should we really recenter here?  NewCompensate may be adjusted by
        // NewDuration without altering the effective position *in the loop*, but we are
        // altering the position in loop count.
        NewCompensate = Modulus(NewCompensate, NewDuration);

        // Update the data
        LockedBlendData->ObservedWeight     = NewWeight;
        LockedBlendData->ObservedTime       = NewTime;
        LockedBlendData->CompensatingOffset = NewCompensate;
    }
}

static void
ComputeSampleTimes(real32 const DurationFrom,
                   real32 const DurationTo,
                   real32 const Weight,
                   real32 const AtTime,
                   real32 const CompensatingOffset,
                   real32*      SampleTimeFrom,
                   real32*      SampleTimeTo)
{
    Assert(SampleTimeFrom);
    Assert(SampleTimeTo);
    Assert(DurationFrom > 0);
    Assert(DurationTo > 0);
    Assert(Weight >= 0 && Weight <= 1);

    real32 const Duration = Interpolate(DurationFrom, DurationTo, Weight);

    real32 LoopPos = (AtTime + CompensatingOffset) / Duration;

    *SampleTimeFrom = DurationFrom * LoopPos;
    *SampleTimeTo   = DurationTo   * LoopPos;
}


local_pose* GRANNY
SampleDAG_LockedBlend(dag2_node& Node,
                      dag2_nodedata_locked_blend& NodeData,
                      dag2_instance& Instance,
                      int32x OnOutput,
                      real32 AtTime,
                      pose_cache& PoseCache)
{
    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckCondition(OnOutput == 0, return 0);

    skeleton* Skeleton = Instance.SourceModel->Skeleton;
    Assert(Skeleton);

    dag2_instancedata_lb* LockedBlendData =
        GET_DAG2INST_DATA(dag2_instancedata_lb, Instance, Node);

    dag2_node* FromNode = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[eFrom].InputNodeIndex);
    dag2_node* ToNode = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[eTo].InputNodeIndex);

    // If one or both of these is null, then we'll be sampling only one, so we can skip
    // the weight work...
    if (FromNode && ToNode)
    {
        // Sample the weight first, since we may be able to skip sampling one or the other
        // pose nodes...
        real32 Weight = LockedBlendNodeDefaultWeight;
        if (IsValidNodeIndex(Node.InputEdges[eParam].InputNodeIndex))
        {
            Weight = SampleDAGNodeScalar(Instance,
                                         Node.InputEdges[eParam].InputNodeIndex,
                                         Node.InputEdges[eParam].ConnectedOutput,
                                         AtTime);

            // Clamp the weight from 0 to 1, to make the rest of this bit sensible.
            Weight = Clamp(0.0f, Weight, 1.0f);
            if (Weight < 0.01f)
                Weight = 0;
            else if (Weight > 0.99f)
                Weight = 1;
        }

        // Update the compensating offset, if necessary.
        real32 DurationFrom, DurationTo;
        real32 SampleTimeFrom, SampleTimeTo;
        if (SampleDAGLockParamsInternal(Instance,
                                        Node.InputEdges[eFrom].InputNodeIndex,
                                        Node.InputEdges[eFrom].ConnectedOutput,
                                        AtTime,
                                        DurationFrom)
            && SampleDAGLockParamsInternal(Instance,
                                           Node.InputEdges[eTo].InputNodeIndex,
                                           Node.InputEdges[eTo].ConnectedOutput,
                                           AtTime,
                                           DurationTo))
        {
            UpdateObservedWeight(LockedBlendData, DurationFrom, DurationTo, Weight, AtTime);

            // Compute the lerped duration, and compute the sample times from the implied position
            ComputeSampleTimes(DurationFrom, DurationTo, Weight,
                               AtTime, LockedBlendData->CompensatingOffset,
                               &SampleTimeFrom, &SampleTimeTo);
        }
        else
        {
            // Leave the offset alone.  Not entirely sure what the right move is here.
            // Should we disallow these connections?

            SampleTimeFrom = AtTime;
            SampleTimeTo   = AtTime;
        }

        if (Weight > BlendEffectivelyZero && Weight < (1.0f - BlendEffectivelyZero))
        {
            // There's work to do!
            local_pose* FromPose = SampleDAGNode(Instance,
                                                 Node.InputEdges[eFrom].InputNodeIndex,
                                                 Node.InputEdges[eFrom].ConnectedOutput,
                                                 SampleTimeFrom, PoseCache);
            local_pose* ToPose = SampleDAGNode(Instance,
                                               Node.InputEdges[eTo].InputNodeIndex,
                                               Node.InputEdges[eTo].ConnectedOutput,
                                               SampleTimeTo, PoseCache);
            Assert(FromPose && ToPose);

            // We'll just use the "From" pose to blend together.
            LinearBlend(*FromPose, *FromPose, *ToPose, Weight);

            // At this point, we can release the ToPose, and return
            ReleaseCachePose(PoseCache, ToPose);
            return FromPose;
        }
        else if (Weight <= BlendEffectivelyZero)
        {
            // Only the "From" node contributes, fall through to that case below
            ToNode = 0;
        }
        else
        {
            // Only the "To" node contributes, fall through to that case below
            FromNode = 0;
        }
        Assert(FromNode || ToNode);
    }

    if (FromNode)
    {
        // Just return the result of sampling "From"
        local_pose* Return = SampleDAGNode(Instance,
                                           Node.InputEdges[eFrom].InputNodeIndex,
                                           Node.InputEdges[eFrom].ConnectedOutput,
                                           AtTime, PoseCache);
        return Return;
    }
    else if (ToNode)
    {
        // Just return the result of sampling "To"
        local_pose* Return = SampleDAGNode(Instance,
                                           Node.InputEdges[eTo].InputNodeIndex,
                                           Node.InputEdges[eTo].ConnectedOutput,
                                           AtTime, PoseCache);
        return Return;
    }
    else
    {
        // Completely unconnected.  Bail.  Caller constructs rest pose...
        return 0;
    }
}

bool GRANNY
GetRootMotion_LockedBlend(dag2_node& Node,
                          dag2_nodedata_locked_blend& NodeData,
                          dag2_instance& Instance,
                          int32x OnOutput,
                          real32 SecondsElapsed,
                          real32 AtTime,
                          triple& ResultTranslation,
                          triple& ResultRotation)
{

    VectorZero3(ResultTranslation);
    VectorZero3(ResultRotation);

    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(Instance.SourceModel, return false);
    CheckCondition(OnOutput == 0, return false);

    int32x FromNodeIndex = Node.InputEdges[eFrom].InputNodeIndex;
    int32x FromOutput    = Node.InputEdges[eFrom].ConnectedOutput;
    int32x ToNodeIndex = Node.InputEdges[eTo].InputNodeIndex;
    int32x ToOutput    = Node.InputEdges[eTo].ConnectedOutput;

    dag2_instancedata_lb* LockedBlendData =
        GET_DAG2INST_DATA(dag2_instancedata_lb, Instance, Node);
    Assert(LockedBlendData);

    // If one or both of these is null, then we'll be sampling only one, so we can skip
    // the weight work...
    if (IsValidNodeIndex(FromNodeIndex) && IsValidNodeIndex(ToNodeIndex))
    {
        // Sample the weight first, since we may be able to skip sampling one or the other
        // pose nodes...
        real32 Weight = LockedBlendNodeDefaultWeight;
        if (IsValidNodeIndex(Node.InputEdges[eParam].InputNodeIndex))
        {
            Weight = SampleDAGNodeScalar(Instance,
                                         Node.InputEdges[eParam].InputNodeIndex,
                                         Node.InputEdges[eParam].ConnectedOutput,
                                         AtTime);

            // Clamp the weight from 0 to 1, to make the rest of this bit sensible.
            Weight = Clamp(0.0f, Weight, 1.0f);
        }

        // Update the compensating offset, if necessary.
        real32 DurationFrom, DurationTo;
        real32 SampleTimeFrom, SampleTimeTo;
        real32 BaseTimeFrom, BaseTimeTo;
        if (SampleDAGLockParamsInternal(Instance,
                                        Node.InputEdges[eFrom].InputNodeIndex,
                                        Node.InputEdges[eFrom].ConnectedOutput,
                                        AtTime,
                                        DurationFrom)
            && SampleDAGLockParamsInternal(Instance,
                                           Node.InputEdges[eTo].InputNodeIndex,
                                           Node.InputEdges[eTo].ConnectedOutput,
                                           AtTime,
                                           DurationTo))
        {
            UpdateObservedWeight(LockedBlendData, DurationFrom, DurationTo, Weight, AtTime);

            // Compute the lerped duration, and compute the sample times from the implied position
            ComputeSampleTimes(DurationFrom, DurationTo, Weight,
                               AtTime, LockedBlendData->CompensatingOffset,
                               &SampleTimeFrom, &SampleTimeTo);
            ComputeSampleTimes(DurationFrom, DurationTo, Weight,
                               AtTime - SecondsElapsed, LockedBlendData->CompensatingOffset,
                               &BaseTimeFrom, &BaseTimeTo);
        }
        else
        {
            // Leave the offset alone.  Not entirely sure what the right move is here.
            // Should we disallow these connections?

            SampleTimeFrom = AtTime;
            SampleTimeTo   = AtTime;
            BaseTimeFrom   = AtTime - SecondsElapsed;
            BaseTimeTo     = AtTime - SecondsElapsed;
        }

        if (Weight > BlendEffectivelyZero && Weight < (1.0f - BlendEffectivelyZero))
        {
            // There's work to do!
            real32 FromTranslation[3] = { 0, 0, 0 };
            real32 FromRotation[3]    = { 0, 0, 0 };
            real32 ToTranslation[3]   = { 0, 0, 0 };
            real32 ToRotation[3]      = { 0, 0, 0 };

            bool FromSuccess =
                GetDagNodeRootMotionVectors(Instance, FromNodeIndex, FromOutput,
                                            SampleTimeFrom - BaseTimeFrom, SampleTimeFrom,
                                            FromTranslation, FromRotation);
            bool ToSuccess =
                GetDagNodeRootMotionVectors(Instance, ToNodeIndex, ToOutput,
                                            SampleTimeTo - BaseTimeTo, SampleTimeTo,
                                            ToTranslation, ToRotation);
            if (FromSuccess && ToSuccess)
            {
                ScaleVectorAdd3(ResultTranslation, 1.0f - Weight, FromTranslation);
                ScaleVectorAdd3(ResultRotation,    1.0f - Weight, FromRotation);
                ScaleVectorAdd3(ResultTranslation, Weight, ToTranslation);
                ScaleVectorAdd3(ResultRotation,    Weight, ToRotation);
                return true;
            }
            else if (FromSuccess)
            {
                Copy32(3, FromTranslation, ResultTranslation);
                Copy32(3, FromRotation, ResultRotation);
                return true;
            }
            else if (ToSuccess)
            {
                Copy32(3, ToTranslation, ResultTranslation);
                Copy32(3, ToRotation, ResultRotation);
                return true;
            }
            else
            {
                // TODO: log, Um, that's bad.
                return false;
            }
        }
        else if (Weight <= BlendEffectivelyZero)
        {
            // Only the "From" node contributes, fall through to that case below
            ToNodeIndex = NoNodeIndex;
        }
        else
        {
            // Only the "To" node contributes, fall through to that case below
            FromNodeIndex = NoNodeIndex;
        }
        Assert(IsValidNodeIndex(FromNodeIndex) || IsValidNodeIndex(ToNodeIndex));
    }

    if (IsValidNodeIndex(FromNodeIndex))
    {
        // Just return the result of sampling "From"
        return GetDagNodeRootMotionVectors(Instance, FromNodeIndex, FromOutput,
                                           SecondsElapsed, AtTime,
                                           ResultTranslation, ResultRotation);
    }
    else if (IsValidNodeIndex(ToNodeIndex))
    {
        // Just return the result of sampling "To"
        return GetDagNodeRootMotionVectors(Instance, ToNodeIndex, ToOutput,
                                           SecondsElapsed, AtTime,
                                           ResultTranslation, ResultRotation);
    }
    else
    {
        // Completely unconnected.  Return rest pose...
        return true;
    }
}


bool GRANNY
SampleDAGLockParams_LockedBlend(dag2_node& Node,
                                dag2_nodedata_locked_blend& NodeData,
                                dag2_instance& Instance,
                                int32x OnOutput,
                                real32 AtTime,
                                real32& Duration)
{
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(Instance.SourceModel, return false);
    CheckInt32Index(OnOutput, Node.ExternalOutputEdgeCount, return false);

    dag2_node* FromNode = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[eFrom].InputNodeIndex);
    dag2_node* ToNode = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[eTo].InputNodeIndex);

    // If both of these are null, then we can't possibly compute a valid duration
    // the weight work...
    if (!FromNode && !ToNode)
        return false;

    if (FromNode && ToNode)
    {
        real32 Weight = LockedBlendNodeDefaultWeight;
        if (IsValidNodeIndex(Node.InputEdges[eParam].InputNodeIndex))
        {
            Weight = SampleDAGNodeScalar(Instance,
                                         Node.InputEdges[eParam].InputNodeIndex,
                                         Node.InputEdges[eParam].ConnectedOutput,
                                         AtTime);

            // Clamp the weight from 0 to 1, to make the rest of this bit sensible.
            Weight = Clamp(0.0f, Weight, 1.0f);
        }

        // Update the compensating offset, if necessary.
        real32 DurationFrom, DurationTo;
        if (SampleDAGLockParamsInternal(Instance,
                                        Node.InputEdges[eFrom].InputNodeIndex,
                                        Node.InputEdges[eFrom].ConnectedOutput,
                                        AtTime,
                                        DurationFrom)
            && SampleDAGLockParamsInternal(Instance,
                                           Node.InputEdges[eTo].InputNodeIndex,
                                           Node.InputEdges[eTo].ConnectedOutput,
                                           AtTime,
                                           DurationTo))
        {
            Duration = Interpolate(DurationFrom, DurationTo, Weight);
            return true;
        }
        else
        {
            // Can't compute the duration
            return false;
        }
    }
    else if (FromNode)
    {
        return SampleDAGLockParamsInternal(Instance,
                                           Node.InputEdges[eFrom].InputNodeIndex,
                                           Node.InputEdges[eFrom].ConnectedOutput,
                                           AtTime,
                                           Duration);
    }
    else
    {
        Assert(ToNode);
        return SampleDAGLockParamsInternal(Instance,
                                           Node.InputEdges[eTo].InputNodeIndex,
                                           Node.InputEdges[eTo].ConnectedOutput,
                                           AtTime,
                                           Duration);
    }
}

int32 GRANNY
GetInstanceDataSize_LockedBlend(dag2_node& Node, dag2_nodedata_locked_blend& NodeData,
                                dag2& TheDag, model& FromModel, model& ToModel)
{
    return SizeOf(dag2_instancedata_lb);
}


bool GRANNY
BindDAGInstanceData_LockedBlend(dag2_node&                  Node,
                                dag2_nodedata_locked_blend& NodeData,
                                dag2_instance&              Instance)
{
    dag2_instancedata_lb* Data = GET_DAG2INST_DATA(dag2_instancedata_lb, Instance, Node);

    // Default init
    ZeroStructure(*Data);
    Data->ObservedWeight = -1;

    return true;
}


bool GRANNY
UnbindDAGInstanceData_LockedBlend(dag2_node&                  Node,
                                  dag2_nodedata_locked_blend& NodeData,
                                  dag2_instance&              Instance)
{
    // Nothing to do

    return true;
}


