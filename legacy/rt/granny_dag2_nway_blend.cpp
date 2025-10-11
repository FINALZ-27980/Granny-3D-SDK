// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_nway_blend.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_nway_blend.h"

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

#define SubsystemCode BlendDagLogMessage
USING_GRANNY_NAMESPACE;

IMPL_GET_NODEDATA(NWayBlend, nwayblend);

data_type_definition GRANNY Dag2NodeDataNWayBlendType[] =
{
    { Int32Member, "TypeMarker_NodeDataNWayBlend" },
    { EndMember },
};

dag2_node* GRANNY
NewNWayBlendNode(dag2& TheDag, memory_arena& Arena)
{
    AGGR_NODE(TheDag,
              NewNode, 7, 7, 1, 1,
              NodeState, dag2_nodedata_nwayblend, Dag2NodeDataNWayBlendType,
              Arena);
    if (NewNode)
    {
        NewNode->Name = 0;

        INIT_EXTERNAL_INPUT_EDGE(NewNode, 0, "Pose",         Pose);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, 1, "Contribution", Float);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, 2, "Mask",         Trackmask);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, 3, "Pose",         Pose);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, 4, "Contribution", Float);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, 5, "Pose",         Pose);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, 6, "Contribution", Float);

        INIT_EXTERNAL_OUTPUT_EDGE(NewNode, 0, "Output", Pose);
        Assert(GetNodeDataNWayBlend(NewNode));
    }

    return NewNode;
}


local_pose* GRANNY
SampleDAG_NWayBlend(dag2_node& Node,
                    dag2_nodedata_nwayblend& NodeData,
                    dag2_instance& Instance,
                    int32x OnOutput,
                    real32 AtTime,
                    pose_cache& PoseCache)
{
    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckCondition(OnOutput == 0, return 0);
    CheckCondition(Node.TotalInputEdgeCount >= 3, return 0);

    skeleton* Skeleton = Instance.SourceModel->Skeleton;
    Assert(Skeleton);

    dag2_node* PoseBaseNode = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[0].InputNodeIndex);
    dag2_node* MaskNode     = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[2].InputNodeIndex);

    // Must have at least the pose base
    if (!PoseBaseNode)
        return 0;

    track_mask* Mask = 0;
    if (MaskNode)
    {
        Mask = SampleDAGNodeTrackmask(Instance,
                                      Node.InputEdges[2].InputNodeIndex,
                                      Node.InputEdges[2].ConnectedOutput,
                                      AtTime);
    }

    local_pose* RetValue = GetNewLocalPose(PoseCache, Skeleton->BoneCount);
    if (RetValue)
    {
        BeginLocalPoseAccumulation(*RetValue, 0, Skeleton->BoneCount, 0);

        // Handle the base case specially, since we treat the mask differently.  Also,
        // hang onto this pose.  More on that later...
        local_pose* BasePose = SampleDAGNode(Instance,
                                         Node.InputEdges[0].InputNodeIndex,
                                         Node.InputEdges[0].ConnectedOutput,
                                         AtTime, PoseCache);
        {
            real32 Weight = 1.0f;
            if (GetDag2Node(*Instance.SourceDAG, Node.InputEdges[1].InputNodeIndex))
            {
                Weight = Clamp(0,
                               SampleDAGNodeScalar(Instance,
                                                   Node.InputEdges[1].InputNodeIndex,
                                                   Node.InputEdges[1].ConnectedOutput,
                                                   AtTime),
                               1);
            }

            {for (int32x Idx = 0; Idx < Skeleton->BoneCount; ++Idx)
            {
                real32 BoneWeight = Weight;
                if (Mask)
                {
                    BoneWeight *= 1.0f - Clamp(0, GetTrackMaskBoneWeight(*Mask, Idx), 1);
                }

                AccumulateLocalTransform(*RetValue, Idx, Idx,
                                         BoneWeight,
                                         *Skeleton,
                                         BlendQuaternionAccumNeighborhooded,
                                         *GetLocalPoseTransform(*BasePose, Idx));
            }}
        }

        // And now loop over the secondary inputs
        bool SomeWeightAdded = false;
        {for (int32x Idx = 3; Idx < Node.TotalInputEdgeCount; Idx += 2)
        {
            Assert(Node.TotalInputEdgeCount > Idx + 1);

            // No pose here?
            if (!GetDag2Node(*Instance.SourceDAG, Node.InputEdges[Idx].InputNodeIndex))
                continue;

            local_pose* Pose = SampleDAGNode(Instance,
                                             Node.InputEdges[Idx].InputNodeIndex,
                                             Node.InputEdges[Idx].ConnectedOutput,
                                             AtTime, PoseCache);

            real32 Weight = 1.0f;
            if (GetDag2Node(*Instance.SourceDAG, Node.InputEdges[Idx+1].InputNodeIndex))
            {
                Weight = Clamp(0,
                               SampleDAGNodeScalar(Instance,
                                                   Node.InputEdges[Idx+1].InputNodeIndex,
                                                   Node.InputEdges[Idx+1].ConnectedOutput,
                                                   AtTime),
                               1);
            }

            {for (int32x BoneIdx = 0; BoneIdx < Skeleton->BoneCount; ++BoneIdx)
            {
                real32 BoneWeight = Weight;
                if (Mask)
                {
                    BoneWeight *= Clamp(0, GetTrackMaskBoneWeight(*Mask, BoneIdx), 1);
                }

                SomeWeightAdded = SomeWeightAdded || (BoneWeight != 0);
                AccumulateLocalTransform(*RetValue, BoneIdx, BoneIdx,
                                         BoneWeight,
                                         *Skeleton,
                                         BlendQuaternionAccumNeighborhooded,
                                         *GetLocalPoseTransform(*Pose, BoneIdx));
            }}

            ReleaseCachePose(PoseCache, Pose);
        }}
        EndLocalPoseAccumulationLOD(*RetValue, 0, Skeleton->BoneCount, 0, Skeleton->BoneCount, *Skeleton, 0.0f, 0);

        // Did any of the nways add to the blend?
        if (SomeWeightAdded)
        {
            ReleaseCachePose(PoseCache, BasePose);
        }
        else
        {
            ReleaseCachePose(PoseCache, RetValue);
            RetValue = BasePose;
        }
    }

    return RetValue;
}

bool GRANNY
GetRootMotion_NWayBlend(dag2_node& Node,
                        dag2_nodedata_nwayblend& NodeData,
                        dag2_instance& Instance,
                        int32x OnOutput,
                        real32 SecondsElapsed,
                        real32 AtTime,
                        triple& ResultTranslation,
                        triple& ResultRotation)
{
    VectorZero3(ResultTranslation);
    VectorZero3(ResultRotation);
    real32 TotalWeight = 0;

    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckCondition(OnOutput == 0, return false);

    dag2_node* PoseBaseNode = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[0].InputNodeIndex);
    dag2_node* MaskNode     = GetDag2Node(*Instance.SourceDAG, Node.InputEdges[2].InputNodeIndex);

    // Must have at least the pose base
    if (!PoseBaseNode)
        return 0;

    // Note that we only use this if MaskNode != NULL
    real32 RootBoneMaskWeight = 1.0;
    if (MaskNode)
    {
        track_mask* Mask = SampleDAGNodeTrackmask(Instance,
                                                  Node.InputEdges[2].InputNodeIndex,
                                                  Node.InputEdges[2].ConnectedOutput,
                                                  AtTime);
        if (Mask)
            RootBoneMaskWeight = Clamp(0, GetTrackMaskBoneWeight(*Mask, 0), 1);
    }

    // First node is special, as always...
    triple BaseTranslation = { 0, 0, 0 };
    triple BaseRotation    = { 0, 0, 0 };
    {
        bool Success =
            GetDagNodeRootMotionVectors(Instance,
                                        Node.InputEdges[0].InputNodeIndex,
                                        Node.InputEdges[0].ConnectedOutput,
                                        SecondsElapsed, AtTime,
                                        BaseTranslation, BaseRotation);

        if (Success)
        {
            real32 Weight = 1.0f;
            if (GetDag2Node(*Instance.SourceDAG, Node.InputEdges[1].InputNodeIndex))
            {
                Weight = Clamp(0,
                               SampleDAGNodeScalar(Instance,
                                                   Node.InputEdges[1].InputNodeIndex,
                                                   Node.InputEdges[1].ConnectedOutput,
                                                   AtTime),
                               1);
            }

            if (MaskNode)
                Weight *= (1.0f - RootBoneMaskWeight);

            ScaleVectorAdd3(ResultTranslation, Weight, BaseTranslation);
            ScaleVectorAdd3(ResultRotation,    Weight, BaseRotation);
            TotalWeight += Weight;
        }
        else
        {
            // Bad, can't continue
            return false;
        }
    }

    {for (int32x Idx = 3; Idx < Node.TotalInputEdgeCount; Idx += 2)
    {
        Assert(Node.TotalInputEdgeCount > Idx + 1);

        // No pose here?
        if (!GetDag2Node(*Instance.SourceDAG, Node.InputEdges[Idx].InputNodeIndex))
            continue;

        triple Translation = { 0, 0, 0 };
        triple Rotation    = { 0, 0, 0 };
        bool Success =
            GetDagNodeRootMotionVectors(Instance,
                                        Node.InputEdges[Idx].InputNodeIndex,
                                        Node.InputEdges[Idx].ConnectedOutput,
                                        SecondsElapsed, AtTime,
                                        Translation, Rotation);

        if (Success)
        {
            real32 Weight = 1.0f;
            if (GetDag2Node(*Instance.SourceDAG, Node.InputEdges[Idx + 1].InputNodeIndex))
            {
                Weight = Clamp(0,
                               SampleDAGNodeScalar(Instance,
                                                   Node.InputEdges[Idx + 1].InputNodeIndex,
                                                   Node.InputEdges[Idx + 1].ConnectedOutput,
                                                   AtTime),
                               1);
            }

            if (MaskNode)
                Weight *= RootBoneMaskWeight;

            ScaleVectorAdd3(ResultTranslation, Weight, Translation);
            ScaleVectorAdd3(ResultRotation,    Weight, Rotation);
            TotalWeight += Weight;
        }
    }}

    if (TotalWeight != 0)
    {
        real32 const InverseTotalWeight = 1.0f / TotalWeight;
        VectorScale3(ResultTranslation, InverseTotalWeight);
        VectorScale3(ResultRotation,    InverseTotalWeight);
    }

    return true;
}

