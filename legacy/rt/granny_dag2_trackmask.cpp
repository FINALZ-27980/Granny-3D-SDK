// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_trackmask.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_trackmask.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_dag2.h"
#include "granny_dag2_instance.h"
#include "granny_dag2_node_registry.h"
#include "granny_dag2_utilities.h"
#include "granny_data_type_definition.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_model.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
#include "granny_track_mask.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode BlendDagLogMessage
USING_GRANNY_NAMESPACE;

IMPL_GET_NODEDATA(Trackmask, trackmask);

data_type_definition GRANNY Dag2NodeDataTrackmaskType[] =
{
    { Int32Member,     "TypeMarker_NodeDataTrackmask" },
    { Real32Member,    "DefaultWeight" },
    { Real32Member,    "TargetWeight" },
    { StringMember,    "BoneName" },
    { EndMember },
};

dag2_node* GRANNY
NewTrackmaskNode(dag2& TheDag, memory_arena& Arena)
{
    AGGR_NODE(TheDag,
              NewNode, 0, 0, 1, 1,
              NodeState, dag2_nodedata_trackmask, Dag2NodeDataTrackmaskType,
              Arena);
    if (NewNode)
    {
        NewNode->Name = 0;
        NewNode->OutputEdges[0].ParameterType = TrackmaskParameterType;
        NewNode->OutputEdges[0].EdgeName = "Output";
        NewNode->OutputEdges[0].Internal = 0;
        NewNode->OutputEdges[0].Forwarded = 0;

        NodeState->TargetWeight  = 0.0f;
        NodeState->DefaultWeight = 1.0f;
        NodeState->BoneName      = 0;

        Assert(GetNodeDataTrackmask(NewNode));
    }

    return NewNode;
}


int32 GRANNY
GetInstanceDataSize_Trackmask(dag2_node& Node,
                              dag2_nodedata_trackmask& NodeData,
                              dag2& TheDag, model& FromModel, model& ToModel)
{
    // The track_mask is bound to the FromModel, of course.
    return GetTrackMaskSize(FromModel.Skeleton->BoneCount);
}


bool GRANNY
BindDAGInstanceData_Trackmask(dag2_node&               Node,
                              dag2_nodedata_trackmask& NodeData,
                              dag2_instance&           Instance)
{
    track_mask* Trackmask = GET_DAG2INST_DATA(track_mask, Instance, Node);

    Trackmask = NewTrackMaskInPlace(NodeData.DefaultWeight, Instance.SourceModel->Skeleton->BoneCount, Trackmask);

    int32x BoneIndex;
    if (NodeData.BoneName &&
        FindBoneByNameLowercase(Instance.SourceModel->Skeleton,
                                NodeData.BoneName, BoneIndex))
    {
        SetSkeletonTrackMaskChainDownwards(*Trackmask, *Instance.SourceModel->Skeleton,
                                           BoneIndex, NodeData.TargetWeight);
    }

    return true;
}


bool GRANNY
UnbindDAGInstanceData_Trackmask(dag2_node&               Node,
                                dag2_nodedata_trackmask& NodeData,
                                dag2_instance&           Instance)
{
    // Nothing to do here...
    return true;
}


bool GRANNY
BakeOutDAGInstanceData_Trackmask(dag2_node&               Node,
                                 dag2_nodedata_trackmask& NodeData,
                                 dag2_instance&           Instance,
                                 memory_arena&            Arena)
{
//     if (!NodeData.Trackmask ||
//         NodeData.Trackmask->WeightCount != Instance.SourceModel->Skeleton->BoneCount)
//     {
//         // Need to create a proper track mask for this model.
//         // Todo: wastes space if this node doesn't need a trackmask.  Do we care?
//         int32x RequiredSize =
//             GetUnboundTrackMaskSize(Instance.SourceModel->Skeleton->BoneCount);

//         uint8* Buffer = ArenaPushArray(Arena, RequiredSize, uint8);
//         NodeData.Trackmask =
//             NewUnboundTrackMaskInPlace(1.0f,
//                                        Instance.SourceModel->Skeleton->BoneCount,
//                                        Buffer);
//     }
//     Assert(NodeData.Trackmask != NULL);

//     // Copy out the new weight data from the bound trackmask
//     track_mask* Trackmask = GET_DAG2INST_DATA(track_mask, Instance, Node);
//     Assert(Trackmask->BoneWeightCount == NodeData.Trackmask->WeightCount);

//     NodeData.Trackmask->DefaultWeight = Trackmask->DefaultWeight;
//     {for(int32x Idx = 0; Idx < Trackmask->BoneWeightCount; ++Idx)
//     {
//         NodeData.Trackmask->Weights[Idx].Name = Instance.SourceModel->Skeleton->Bones[Idx].Name;
//         NodeData.Trackmask->Weights[Idx].Weight = Trackmask->BoneWeights[Idx];
//     }}

    return true;
}


track_mask* GRANNY
SampleDAGTrackmask_Trackmask(dag2_node& Node,
                             dag2_nodedata_trackmask& NodeData,
                             dag2_instance& Instance,
                             int32x OnOutput,
                             real32 /*AtTime*/)
{
    CheckCondition(OnOutput == 0, return 0);

    return GET_DAG2INST_DATA(track_mask, Instance, Node);
}
