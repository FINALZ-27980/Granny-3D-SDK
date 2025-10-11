// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_sequence.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_sequence.h"

#include "granny_aggr_alloc.h"
#include "granny_animation.h"
#include "granny_animation_binding.h"
#include "granny_assert.h"
#include "granny_controlled_animation.h"
#include "granny_dag2.h"
#include "granny_dag2_instance.h"
#include "granny_dag2_node_registry.h"
#include "granny_dag2_utilities.h"
#include "granny_pose_cache.h"
#include "granny_file_info.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_model.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
#include "granny_string.h"

#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

USING_GRANNY_NAMESPACE;
#define SubsystemCode BlendDagLogMessage

IMPL_GET_NODEDATA(Sequence, sequence);


static data_type_definition
Dag2NodeDataSequenceEdgeType[] =
{
    { Int32Member, "SourceIndex" },
    { Int32Member, "AnimationIndex" },
    { Int32Member, "TrackGroupIndex" },

    { Real32Member, "Speed" },

    { EndMember },
};

data_type_definition GRANNY
Dag2NodeDataSequenceType[] =
{
    { Int32Member, "TypeMarker_NodeDataSequence" },
    { ReferenceToArrayMember, "Edges", Dag2NodeDataSequenceEdgeType },

    { EndMember },
};

struct dag2_instancedata_seq
{
    controlled_animation ControlledAnim;
    real32 Speed;
    real32 Offset;
};


dag2_node* GRANNY
NewSequenceNode(dag2& TheDag, memory_arena& Arena)
{
    AGGR_NODE(TheDag,
              NewNode, 0, 0, 1, 1,
              NodeState, dag2_nodedata_sequence, Dag2NodeDataSequenceType,
              Arena);
    if (NewNode)
    {
        NewNode->Name = 0;

        NodeState->EdgeCount = 1;
        NodeState->Edges = ArenaPushArray(Arena, 1, dag2_nodedata_sequence_edge);
        ZeroStructure(NodeState->Edges[0]);
        NodeState->Edges[0].SourceIndex = -1;
        NodeState->Edges[0].Speed = 1.0f;

        INIT_EXTERNAL_OUTPUT_EDGE(NewNode, 0, "Output", Pose);
        Assert(GetNodeDataSequence(NewNode));
    }

    return NewNode;
}

static bool
GetSequenceIndexAndLocalTime(dag2_node& Node,
                             dag2_nodedata_sequence& NodeData,
                             dag2_instance& Instance,
                             real32 AtTime,
                             int32x* OutputIndex,
                             real32* OutputTime)
{
    Assert(Instance.InstanceData);
    Assert(Instance.SourceModel);
    CheckPointerNotNull(OutputIndex, return false);
    CheckPointerNotNull(OutputTime, return false);

    dag2_instancedata_seq* DataArray =
        GET_DAG2INST_DATA(dag2_instancedata_seq, Instance, Node);
    Assert(DataArray);

    // cache?
    real32 TotalDuration = 0.0f;
    {for(int32x Idx = 0; Idx < NodeData.EdgeCount; ++Idx)
    {
        dag2_instancedata_seq& Data = DataArray[Idx];

        animation_binding* Binding = Data.ControlledAnim.Binding;
        if (Binding)
            TotalDuration += Binding->ID.Animation->Duration * Data.Speed;
    }}

    if (TotalDuration <= 0.0f)
        return false;

    real32 SampleTime = Modulus(AtTime, TotalDuration);
    if (SampleTime < 0.0f)
        SampleTime += TotalDuration;

    Assert(SampleTime >= 0.0f && SampleTime <= TotalDuration);
    {
        real32 LocalTime = SampleTime;

        int32x UseIdx = -1;
        {for(UseIdx = 0; UseIdx < NodeData.EdgeCount; ++UseIdx)
        {
            animation_binding* Binding = DataArray[UseIdx].ControlledAnim.Binding;
            if (Binding)
            {
                if (LocalTime < Binding->ID.Animation->Duration)
                    break;

                LocalTime -= Binding->ID.Animation->Duration;
            }
        }}
        Assert(UseIdx < NodeData.EdgeCount);

        *OutputIndex = UseIdx;
        *OutputTime  = LocalTime;
    }

    return true;
}

local_pose* GRANNY
SampleDAG_Sequence(dag2_node& Node,
                   dag2_nodedata_sequence& NodeData,
                   dag2_instance& Instance,
                   int32x OnOutput,
                   real32 AtTime,
                   pose_cache& PoseCache)
{
    // TODO: factor sample time from here and _RootMotion
    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckCondition(OnOutput == 0, return 0);

    dag2_instancedata_seq* DataArray =
        GET_DAG2INST_DATA(dag2_instancedata_seq, Instance, Node);
    Assert(DataArray);

    skeleton* Skeleton = Instance.SourceModel->Skeleton;
    Assert(Skeleton);

    int32x UseIndex;
    real32 LocalTime;
    if (!GetSequenceIndexAndLocalTime(Node, NodeData, Instance, AtTime,
                                      &UseIndex, &LocalTime))
    {
        return 0;
    }

    local_pose* RetValue = GetNewLocalPose(PoseCache, Skeleton->BoneCount);
    if (RetValue)
    {
        dag2_instancedata_seq& Data = DataArray[UseIndex];

        animation_binding* Binding = Data.ControlledAnim.Binding;
        Assert(Binding);

        BeginLocalPoseAccumulation(*RetValue, 0, Skeleton->BoneCount, 0 );

        sample_context Context;
        Context.LocalDuration = Binding->ID.Animation->Duration;

        Context.LocalClock = Modulus(LocalTime / Data.Speed, Context.LocalDuration);
        if (Context.LocalClock < 0.0f)
            Context.LocalClock += Context.LocalDuration;
        Context.LocalClock = Clamp(0, Context.LocalClock, Context.LocalDuration);

        SetContextFrameIndex(Context,
                             Context.LocalClock,
                             Binding->ID.Animation->Duration,
                             Binding->ID.Animation->TimeStep);

        Context.UnderflowLoop = true;
        Context.OverflowLoop = true;

        AccumulateControlledAnimation(Data.ControlledAnim,
                                      Context, 1.0f, *Skeleton,
                                      0, Skeleton->BoneCount,
                                      *RetValue, 0.0f, 0);

        EndLocalPoseAccumulationLOD(*RetValue, 0, Skeleton->BoneCount, 0, Skeleton->BoneCount,
                                    *Skeleton, 0.0f, 0);
    }

    return RetValue;
}


bool GRANNY
GetRootMotion_Sequence(dag2_node& Node,
                       dag2_nodedata_sequence& NodeData,
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
    CheckCondition(OnOutput == 0, return 0);

    dag2_instancedata_seq* DataArray =
        GET_DAG2INST_DATA(dag2_instancedata_seq, Instance, Node);
    Assert(DataArray);


    int32x UseIndex;
    real32 LocalTime;
    if (!GetSequenceIndexAndLocalTime(Node, NodeData, Instance, AtTime,
                                      &UseIndex, &LocalTime))
    {
        return false;
    }

    // Assert, but make sure we don't crash in extremis...
    Assert(UseIndex < NodeData.EdgeCount);
    UseIndex = ClampInt32(0, UseIndex, NodeData.EdgeCount);

    dag2_instancedata_seq& Data = DataArray[UseIndex];
    {
        real32 LocalStartTime = LocalTime;
        real32 LocalEndTime   = LocalTime + SecondsElapsed;
        bool   FlipResult = false;
        if (LocalStartTime > LocalEndTime)
        {
            real32 Temp    = LocalEndTime;
            LocalEndTime   = LocalStartTime;
            LocalStartTime = Temp;
            FlipResult = true;
        }

        // If we're not actually bound to an animation, return a rest pose...
        if (Data.ControlledAnim.Binding == 0 ||
            Data.ControlledAnim.AccumulationMode == NoAccumulation )
        {
            return true;
        }

        animation_binding* Binding = Data.ControlledAnim.Binding;
        real32 LocalDuration = Binding->ID.Animation->Duration;

        bound_transform_track* BoundTrack = Binding->TrackBindings;
        if(BoundTrack == 0)
            return false;

        transform_track const* SourceTrack = BoundTrack->SourceTrack;
        if(SourceTrack == 0)
            return false;

        real32 IgnoreTotalWeight = 0.0f;
        AccumulateControlledAnimationMotionVectors(&Data.ControlledAnim, LocalStartTime, LocalEndTime, LocalDuration,
                                                   1.0f, FlipResult,
                                                   BoundTrack, SourceTrack,
                                                   &IgnoreTotalWeight,
                                                   (real32*)ResultTranslation,
                                                   (real32*)ResultRotation);
        return true;
    }
}


int32 GRANNY
GetInstanceDataSize_Sequence(dag2_node& Node,
                             dag2_nodedata_sequence& Sequence,
                             dag2& TheDag, model& FromModel, model& ToModel)
{
    return (SizeOf(dag2_instancedata_seq) * Sequence.EdgeCount);
}


bool GRANNY
BindDAGInstanceData_Sequence(dag2_node&              Node,
                             dag2_nodedata_sequence& NodeData,
                             dag2_instance&          Instance)
{
    CheckPointerNotNull(Instance.InstanceData, return false);

    dag2_instancedata_seq* DataArray =
        GET_DAG2INST_DATA(dag2_instancedata_seq, Instance, Node);

    {for(int32x Idx = 0; Idx < NodeData.EdgeCount; ++Idx)
    {
        dag2_instancedata_seq& ThisData = DataArray[Idx];
        dag2_nodedata_sequence_edge& Edge = NodeData.Edges[Idx];

        ZeroStructure(DataArray[Idx]);
        ThisData.Speed  = Edge.Speed;

        // But only bind if we have a source
        if (!IsDag2SourceIndexValid(Edge.SourceIndex))
            continue;

        CheckPointerNotNull(Instance.InstanceData, return false);
        CheckPointerNotNull(Instance.SourceDAG, return false);
        CheckInt32Index(Edge.SourceIndex, Instance.SourceDAG->SourceCount, return false);

        dag2_source* Source = Instance.SourceDAG->Sources[Edge.SourceIndex];
        CheckPointerNotNull(Source, return false);

        model* SourceModel = Instance.SourceModel;
        CheckPointerNotNull(SourceModel, return true);

        file_info* SourceInfo = Source->CachedSource;
        CheckPointerNotNull(SourceInfo, return false);
        CheckCondition(SourceInfo->AnimationCount > Edge.AnimationIndex, return false);

        animation* Animation = SourceInfo->Animations[Edge.AnimationIndex];
        CheckPointerNotNull(Animation, return false);

        int32x TrackGroupIndex = Edge.TrackGroupIndex;
        if (TrackGroupIndex == -1)
        {
            if (!FindTrackGroupForModel(*Animation, SourceModel->Name, TrackGroupIndex))
            {
                // todo: log
                return false;
            }
        }
        Assert(TrackGroupIndex != -1);
        CheckInt32Index(TrackGroupIndex, Animation->TrackGroupCount, return false);

        track_group* TrackGroup = Animation->TrackGroups[TrackGroupIndex];
        Assert(TrackGroup);

        animation_binding_identifier ID;
        ZeroStructure(ID);
        ID.Animation = Animation;
        ID.SourceTrackGroupIndex = TrackGroupIndex;
        ID.OnModel = SourceModel;

        ThisData.ControlledAnim.Binding = AcquireAnimationBindingFromID(ID);
        ThisData.ControlledAnim.AccumulationMode =
            AccumulationModeFromTrackGroup(*TrackGroup);
    }}

    return true;
}


bool GRANNY
UnbindDAGInstanceData_Sequence(dag2_node&              Node,
                               dag2_nodedata_sequence& NodeData,
                               dag2_instance&          Instance)
{
    CheckPointerNotNull(Instance.InstanceData, return false);

    dag2_instancedata_seq* Data = GET_DAG2INST_DATA(dag2_instancedata_seq, Instance, Node);
    {for(int32x EdgeIdx = 0; EdgeIdx < NodeData.EdgeCount; ++EdgeIdx)
    {
        dag2_instancedata_seq& ThisData = Data[EdgeIdx];
        dag2_nodedata_sequence_edge& Edge = NodeData.Edges[EdgeIdx];

        if (!IsDag2SourceIndexValid(Edge.SourceIndex))
            continue;

        if (ThisData.ControlledAnim.Binding)
        {
            ReleaseAnimationBinding(ThisData.ControlledAnim.Binding);
            DebugFillStructure(ThisData.ControlledAnim);
        }
    }}

    return true;
}


bool GRANNY
BakeOutDAGInstanceData_Sequence(dag2_node&              Node,
                                dag2_nodedata_sequence& NodeData,
                                dag2_instance&          Instance,
                                memory_arena&           Arena)
{
    CheckPointerNotNull(Instance.InstanceData, return false);

    dag2_instancedata_seq* Data = GET_DAG2INST_DATA(dag2_instancedata_seq, Instance, Node);
    {for(int32x EdgeIdx = 0; EdgeIdx < NodeData.EdgeCount; ++EdgeIdx)
    {
        dag2_instancedata_seq& ThisData   = Data[EdgeIdx];
        dag2_nodedata_sequence_edge& Edge = NodeData.Edges[EdgeIdx];

        Edge.Speed = ThisData.Speed;
    }}

    return true;
}


bool GRANNY
GetFloat_Sequence(dag2_node& Node,
                  dag2_nodedata_sequence& NodeData,
                  dag2_instance& Instance,
                  char const* ParamName,
                  real32* Value)
{
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);
    CheckPointerNotNull(Value, return false);

    dag2_instancedata_seq* DataArray =
        GET_DAG2INST_DATA(dag2_instancedata_seq, Instance, Node);
    Assert(DataArray);

    if (StringsAreEqualLowercase(ParamName, "Duration") ||
        StringsAreEqualLowercase(ParamName, "Duration_0"))
    {
        bool SomeBound = false;
        real32 TotalDuration = 0.0f;
        {for(int32x Idx = 0; Idx < NodeData.EdgeCount; ++Idx)
        {
            dag2_instancedata_seq& Data = DataArray[Idx];

            animation_binding* Binding = Data.ControlledAnim.Binding;
            if (Binding)
            {
                TotalDuration += Binding->ID.Animation->Duration * Data.Speed;
                SomeBound = true;
            }
        }}

        *Value = TotalDuration;
        return SomeBound;
    }

    return false;
}

