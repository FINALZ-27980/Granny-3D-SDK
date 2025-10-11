// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_additive_blend.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_additive_blend.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_dag2.h"
#include "granny_dag2_instance.h"
#include "granny_dag2_node_registry.h"
#include "granny_dag2_utilities.h"
#include "granny_pose_cache.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_memory_ops.h"
#include "granny_model.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
#include "granny_track_mask.h"

#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

USING_GRANNY_NAMESPACE;
#define SubsystemCode BlendDagLogMessage

IMPL_GET_NODEDATA(AdditiveBlend, additiveblend);

real32 const AdditiveBlendNodeDefaultWeight = 1.0f;

data_type_definition GRANNY Dag2NodeDataAdditiveBlendType[] =
{
    { Int32Member, "TypeMarker_NodeDataAdditiveBlend" },
    { EndMember },
};

enum AdditiveBlendInputEdges
{
    eParam  = 0,
    eInto   = 1,
    eBase   = 2,
    eDelta  = 3,
    eMask   = 4,
};


dag2_node* GRANNY
NewAdditiveBlendNode(dag2& TheDag, memory_arena& Arena)
{
    AGGR_NODE(TheDag,
              NewNode, 5, 5, 1, 1,
              NodeState, dag2_nodedata_additiveblend, Dag2NodeDataAdditiveBlendType,
              Arena);
    if (NewNode)
    {
        NewNode->Name = 0;

        INIT_EXTERNAL_INPUT_EDGE(NewNode, eParam, "Amount", Float);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, eInto,  "Into",   Pose);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, eBase,  "Base",   Pose);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, eDelta, "Delta",  Pose);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, eMask,  "Mask",   Trackmask);

        INIT_EXTERNAL_OUTPUT_EDGE(NewNode, 0, "Output", Pose);

        Assert(GetNodeDataAdditiveBlend(NewNode));
    }

    return NewNode;
}


local_pose* GRANNY
SampleDAG_AdditiveBlend(dag2_node& Node,
                        dag2_nodedata_additiveblend& NodeData,
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

    dag2_node* IntoNode  = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[eInto].InputNodeIndex);
    dag2_node* BaseNode  = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[eBase].InputNodeIndex);
    dag2_node* DeltaNode = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[eDelta].InputNodeIndex);
    dag2_node* ParamNode = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[eParam].InputNodeIndex);
    dag2_node* MaskNode  = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[eMask].InputNodeIndex);

    if (!IntoNode)
    {
        return 0;
    }

    if (!BaseNode || !DeltaNode)
    {
        // Just return the result of sampling "From"
        return SampleDAGNode(Instance,
                             Node.InputEdges[eInto].InputNodeIndex,
                             Node.InputEdges[eInto].ConnectedOutput,
                             AtTime, PoseCache);
    }

    // Sample the weight first, since we may be able to skip sampling one or the other
    // pose nodes...
    real32 Weight = AdditiveBlendNodeDefaultWeight;
    if (ParamNode)
    {
        Weight = SampleDAGNodeScalar(Instance,
                                     Node.InputEdges[eParam].InputNodeIndex,
                                     Node.InputEdges[eParam].ConnectedOutput,
                                     AtTime);

        // Clamp the weight from 0 to 1, to make the rest of this bit sensible.
        Weight = Clamp(0.0f, Weight, 1.0f);
    }

    local_pose* IntoPose = SampleDAGNode(Instance,
                                         Node.InputEdges[eInto].InputNodeIndex,
                                         Node.InputEdges[eInto].ConnectedOutput,
                                         AtTime, PoseCache);
    local_pose* BasePose = SampleDAGNode(Instance,
                                         Node.InputEdges[eBase].InputNodeIndex,
                                         Node.InputEdges[eBase].ConnectedOutput,
                                         AtTime, PoseCache);
    local_pose* DeltaPose = SampleDAGNode(Instance,
                                          Node.InputEdges[eDelta].InputNodeIndex,
                                          Node.InputEdges[eDelta].ConnectedOutput,
                                          AtTime, PoseCache);

    track_mask* Mask = 0;
    if (MaskNode)
    {
        Mask = SampleDAGNodeTrackmask(Instance,
                                      Node.InputEdges[eMask].InputNodeIndex,
                                      Node.InputEdges[eMask].ConnectedOutput,
                                      AtTime);
    }

    // Do the blend
    MaskedAdditiveBlend(IntoPose, DeltaPose, BasePose, 0, Skeleton->BoneCount, Mask, Weight);

    // Release the intermediates
    ReleaseCachePose(PoseCache, BasePose);
    ReleaseCachePose(PoseCache, DeltaPose);

    return IntoPose;
}

bool GRANNY
GetRootMotion_AdditiveBlend(dag2_node& Node,
                            dag2_nodedata_additiveblend& NodeData,
                            dag2_instance& Instance,
                            int32x OnOutput,
                            real32 SecondsElapsed,
                            real32 AtTime,
                            triple& ResultTranslation,
                            triple& ResultRotation)
{
    VectorZero3(ResultTranslation);
    VectorZero3(ResultRotation);

    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckCondition(OnOutput == 0, return false);

    int32x IntoNodeIndex  = Node.InputEdges[eInto].InputNodeIndex;
    int32x IntoOutput     = Node.InputEdges[eInto].ConnectedOutput;
    int32x BaseNodeIndex  = Node.InputEdges[eBase].InputNodeIndex;
    int32x BaseOutput     = Node.InputEdges[eBase].ConnectedOutput;
    int32x DeltaNodeIndex = Node.InputEdges[eDelta].InputNodeIndex;
    int32x DeltaOutput    = Node.InputEdges[eDelta].ConnectedOutput;

    track_mask* Mask = 0;
    if (IsValidNodeIndex(Node.InputEdges[eMask].InputNodeIndex))
    {
        Mask = SampleDAGNodeTrackmask(Instance,
                                      Node.InputEdges[eMask].InputNodeIndex,
                                      Node.InputEdges[eMask].ConnectedOutput,
                                      AtTime);
    }

    if (!IsValidNodeIndex(IntoNodeIndex))
    {
        return false;
    }

    // Handle the base motion, which is always sampled
    if (!GetDagNodeRootMotionVectors(Instance, IntoNodeIndex, IntoOutput,
                                     SecondsElapsed, AtTime,
                                     ResultTranslation, ResultRotation))
    {
        return false;
    }

    if (IsValidNodeIndex(BaseNodeIndex) && IsValidNodeIndex(DeltaNodeIndex))
    {
        real32 Weight = AdditiveBlendNodeDefaultWeight;
        if (IsValidNodeIndex(Node.InputEdges[eParam].InputNodeIndex))
        {
            Weight = SampleDAGNodeScalar(Instance,
                                         Node.InputEdges[eParam].InputNodeIndex,
                                         Node.InputEdges[eParam].ConnectedOutput,
                                         AtTime);

            // Clamp the weight from 0 to 1, to make the rest of this bit sensible.
            Weight = Clamp(0.0f, Weight, 1.0f);
        }
        if (Mask != 0)
        {
            Weight *= GetTrackMaskBoneWeight(*Mask, 0);
        }

        if (Weight > BlendEffectivelyZero)
        {
            // There's work to do!
            real32 BaseTranslation[3]  = { 0, 0, 0 };
            real32 BaseRotation[3]     = { 0, 0, 0 };
            real32 DeltaTranslation[3] = { 0, 0, 0 };
            real32 DeltaRotation[3]    = { 0, 0, 0 };

            bool BaseSuccess =
                GetDagNodeRootMotionVectors(Instance, BaseNodeIndex, BaseOutput,
                                            SecondsElapsed, AtTime,
                                            BaseTranslation, BaseRotation);
            bool DeltaSuccess =
                GetDagNodeRootMotionVectors(Instance, DeltaNodeIndex, DeltaOutput,
                                            SecondsElapsed, AtTime,
                                            DeltaTranslation, DeltaRotation);
            if (BaseSuccess && DeltaSuccess)
            {
                ScaleVectorAdd3(ResultTranslation, -Weight, BaseTranslation);
                ScaleVectorAdd3(ResultRotation,    -Weight, BaseRotation);
                ScaleVectorAdd3(ResultTranslation, Weight,  DeltaTranslation);
                ScaleVectorAdd3(ResultRotation,    Weight,  DeltaRotation);
            }
        }
    }

    return true;
}

