// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_blend.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_blend.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_dag2.h"
#include "granny_dag2_instance.h"
#include "granny_dag2_node_registry.h"
#include "granny_dag2_utilities.h"
#include "granny_pose_cache.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_model.h"
#include "granny_memory_ops.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
#include "granny_track_mask.h"

#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

USING_GRANNY_NAMESPACE;
#define SubsystemCode BlendDagLogMessage

IMPL_GET_NODEDATA(Blend, blend);

real32 const BlendNodeDefaultWeight = 0.5f;

data_type_definition GRANNY Dag2NodeDataBlendType[] =
{
    { Int32Member, "TypeMarker_NodeDataBlend" },
    { EndMember },
};

enum BlendInputEdges
{
    eParam = 0,
    eFrom  = 1,
    eTo    = 2,
    eMask  = 3
};


dag2_node* GRANNY
NewBlendNode(dag2& TheDag, memory_arena& Arena)
{
    AGGR_NODE(TheDag,
              NewNode, 4, 4, 1, 1,
              NodeState, dag2_nodedata_blend, Dag2NodeDataBlendType,
              Arena);
    if (NewNode)
    {
        NewNode->Name = 0;

        INIT_EXTERNAL_INPUT_EDGE(NewNode, eParam, "Param", Float);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, eFrom,  "From",  Pose);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, eTo,    "To",    Pose);
        INIT_EXTERNAL_INPUT_EDGE(NewNode, eMask,  "Mask",  Trackmask);

        INIT_EXTERNAL_OUTPUT_EDGE(NewNode, 0, "Output", Pose);

        Assert(GetNodeDataBlend(NewNode));
    }

    return NewNode;
}


void GRANNY
MaskedBlend(skeleton const& Skeleton,
            local_pose& FromPose,
            local_pose const& ToPose,
            real32 Weight,
            track_mask const* Mask)
{
    int32x const BoneCount = Skeleton.BoneCount;

    Assert(Mask);
    CheckBoundedReal32(0.0f, Weight, 1.0f, return);
    CheckBoundedInt32(0, BoneCount, GetLocalPoseBoneCount(FromPose), return);
    CheckBoundedInt32(0, BoneCount, GetLocalPoseBoneCount(ToPose), return);
    CheckBoundedInt32(0, BoneCount, Mask->BoneWeightCount, return);

    local_pose_transform *FromXForms = FromPose.Transforms;
    local_pose_transform *ToXForms   = ToPose.Transforms;

    // Is there any work to be done?
    if (Weight < BlendEffectivelyZero)
        return;

    {for(int32x BoneIndex = 0; BoneIndex < BoneCount; BoneIndex++)
    {
        real32 const MaskWeight = GetTrackMaskBoneWeight(*Mask, BoneIndex);

        local_pose_transform& From = FromXForms[BoneIndex];
        local_pose_transform& To   = ToXForms[BoneIndex];

        if (MaskWeight <= BlendEffectivelyZero)
            continue;

        real32 const Lerp    = MaskWeight * Weight;
        real32 const InvLerp = 1.0f - Lerp;

        From.Transform.Flags |= To.Transform.Flags;

        if (From.Transform.Flags & HasPosition)
        {
            ScaleVectorPlusScaleVector3(From.Transform.Position,
                                        InvLerp, From.Transform.Position,
                                        Lerp,    To.Transform.Position);
        }

        if (From.Transform.Flags & HasOrientation)
        {
            real32 const IP =
                InnerProduct4(From.Transform.Orientation, To.Transform.Orientation);

            ScaleVectorPlusScaleVector4(From.Transform.Orientation,
                                        InvLerp, From.Transform.Orientation,
                                        (IP >= 0) ? Lerp : -Lerp, To.Transform.Orientation);
            NormalizeOrZero4(From.Transform.Orientation);
        }

        if (From.Transform.Flags & HasScaleShear)
        {
            ScaleMatrixPlusScaleMatrix3x3((real32 *)From.Transform.ScaleShear,
                                          InvLerp, (real32 *)From.Transform.ScaleShear,
                                          Lerp,    (real32 *)To.Transform.ScaleShear);
        }
    }}
}

dag2_node* GRANNY
MostInfluentialNode_Blend(dag2_node& Node,
                          dag2_nodedata_blend& NodeData,
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
        real32 Weight = BlendNodeDefaultWeight;
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


local_pose* GRANNY
SampleDAG_Blend(dag2_node& Node,
                dag2_nodedata_blend& NodeData,
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

    dag2_node* FromNode = GetDag2Node(*Instance.SourceDAG,
                                      Node.InputEdges[eFrom].InputNodeIndex);
    dag2_node* ToNode = GetDag2Node(*Instance.SourceDAG,
                                    Node.InputEdges[eTo].InputNodeIndex);

    // If one or both of these is null, then we'll be sampling only one, so we can skip
    // the weight work...
    if (FromNode && ToNode)
    {
        // Sample the weight first, since we may be able to skip sampling one or the other
        // pose nodes...
        real32 Weight = BlendNodeDefaultWeight;
        if (IsValidNodeIndex(Node.InputEdges[eParam].InputNodeIndex))
        {
            Weight = SampleDAGNodeScalar(Instance,
                                         Node.InputEdges[eParam].InputNodeIndex,
                                         Node.InputEdges[eParam].ConnectedOutput,
                                         AtTime);

            // Clamp the weight from 0 to 1, to make the rest of this bit sensible.
            Weight = Clamp(0.0f, Weight, 1.0f);
        }

        track_mask* Mask = 0;
        if (IsValidNodeIndex(Node.InputEdges[eMask].InputNodeIndex))
        {
            Mask = SampleDAGNodeTrackmask(Instance,
                                          Node.InputEdges[eMask].InputNodeIndex,
                                          Node.InputEdges[eMask].ConnectedOutput,
                                          AtTime);
        }

        if (Mask || (Weight > BlendEffectivelyZero &&
                     Weight < (1.0f - BlendEffectivelyZero)))
        {
            // There's work to do!
            local_pose* FromPose = SampleDAGNode(Instance,
                                                 Node.InputEdges[eFrom].InputNodeIndex,
                                                 Node.InputEdges[eFrom].ConnectedOutput,
                                                 AtTime, PoseCache);
            local_pose* ToPose = SampleDAGNode(Instance,
                                               Node.InputEdges[eTo].InputNodeIndex,
                                               Node.InputEdges[eTo].ConnectedOutput,
                                               AtTime, PoseCache);
            Assert(FromPose && ToPose);

            if (Mask == NULL)
            {
                // We'll just use the "From" pose to blend together.
                LinearBlend(*FromPose, *FromPose, *ToPose, Weight);
            }
            else
            {
                // ModulationComposite doesn't quite capture what we want here, so we have
                // to do a bit of custom work.
                // We'll just use the "From" pose to blend together.
                MaskedBlend(*Skeleton, *FromPose, *ToPose, Weight, Mask);
            }

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
GetRootMotion_Blend(dag2_node& Node,
                    dag2_nodedata_blend& NodeData,
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

    int32x FromNodeIndex = Node.InputEdges[eFrom].InputNodeIndex;
    int32x FromOutput    = Node.InputEdges[eFrom].ConnectedOutput;
    int32x ToNodeIndex = Node.InputEdges[eTo].InputNodeIndex;
    int32x ToOutput    = Node.InputEdges[eTo].ConnectedOutput;

    track_mask* Mask = 0;
    if (IsValidNodeIndex(Node.InputEdges[eMask].InputNodeIndex))
    {
        Mask = SampleDAGNodeTrackmask(Instance,
                                      Node.InputEdges[eMask].InputNodeIndex,
                                      Node.InputEdges[eMask].ConnectedOutput,
                                      AtTime);
    }

    // If one or both of these is null, then we'll be sampling only one, so we can skip
    // the weight work...
    if (IsValidNodeIndex(FromNodeIndex) && IsValidNodeIndex(ToNodeIndex))
    {
        // Sample the weight first, since we may be able to skip sampling one or the other
        // pose nodes...
        real32 Weight = BlendNodeDefaultWeight;
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

        if (Weight > BlendEffectivelyZero && Weight < (1.0f - BlendEffectivelyZero))
        {
            // There's work to do!
            real32 FromTranslation[3] = { 0, 0, 0 };
            real32 FromRotation[3]    = { 0, 0, 0 };
            real32 ToTranslation[3] = { 0, 0, 0 };
            real32 ToRotation[3]    = { 0, 0, 0 };

            bool FromSuccess =
                GetDagNodeRootMotionVectors(Instance, FromNodeIndex, FromOutput,
                                            SecondsElapsed, AtTime,
                                            FromTranslation, FromRotation);
            bool ToSuccess =
                GetDagNodeRootMotionVectors(Instance, ToNodeIndex, ToOutput,
                                            SecondsElapsed, AtTime,
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

