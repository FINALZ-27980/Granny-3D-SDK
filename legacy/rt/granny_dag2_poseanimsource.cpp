// ========================================================================
// $File: //jeffr/granny_29/legacy/rt/granny_dag2_poseanimsource.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_dag2_poseanimsource.h"

#include "granny_aggr_alloc.h"
#include "granny_animation.h"
#include "granny_animation_binding.h"
#include "granny_assert.h"
#include "granny_control.h"
#include "granny_controlled_animation.h"
#include "granny_dag2.h"
#include "granny_dag2_instance.h"
#include "granny_dag2_node_registry.h"
#include "granny_dag2_utilities.h"
#include "granny_pose_cache.h"
#include "granny_data_type_definition.h"
#include "granny_file_info.h"
#include "granny_intrinsics.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_memory.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_model.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
#include "granny_string.h"
#include "granny_track_group.h"
#include "granny_track_sampler.h"

#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode BlendDagLogMessage

USING_GRANNY_NAMESPACE;

IMPL_GET_NODEDATA(PoseAnimSource, poseanimsource);

static data_type_definition
Dag2NodeDataPoseAnimSourceEdgeType[] =
{
    { Int32Member, "SourceIndex" },
    { Int32Member, "AnimationIndex" },
    { Int32Member, "TrackGroupIndex" },

    { Int32Member,  "EventTrackIndex" },
    { Bool32Member, "EventTrackSpecified" },

    { Real32Member, "Speed" },
    { Real32Member, "Offset" },
    { Int32Member,  "LoopCount" },
    { Bool32Member, "ClampedLoop" },

    { EndMember },
};

data_type_definition GRANNY
Dag2NodeDataPoseAnimSourceType[] =
{
    { Int32Member, "TypeMarker_NodeDataPoseAnimSource" },
    { ReferenceToArrayMember, "Edges", Dag2NodeDataPoseAnimSourceEdgeType },

    { EndMember },
};


struct dag2_instancedata_pas
{
    controlled_animation ControlledAnim;
    real32 Speed;
    real32 Offset;
    int32  LoopCount;
    bool32 ClampedLoop;
};


dag2_node* GRANNY
NewPoseAnimSourceNode(dag2& TheDag, memory_arena& Arena)
{
    AGGR_NODE(TheDag,
              NewNode, 0, 0, 1, 1,
              NodeState, dag2_nodedata_poseanimsource, Dag2NodeDataPoseAnimSourceType,
              Arena);
    if (NewNode)
    {
        INIT_EXTERNAL_OUTPUT_EDGE(NewNode, 0, "Output", Pose);

        NodeState->EdgeCount = 1;
        NodeState->Edges = ArenaPushArray(Arena, 1, dag2_nodedata_poseanimsource_edge);
        NodeState->Edges[0].SourceIndex = -1;
        NodeState->Edges[0].AnimationIndex  = -1;
        NodeState->Edges[0].TrackGroupIndex = -1;
        NodeState->Edges[0].EventTrackIndex = 0;
        NodeState->Edges[0].EventTrackSpecified = false;
        NodeState->Edges[0].Speed  = 1.0f;
        NodeState->Edges[0].Offset = 0.0f;
        NodeState->Edges[0].LoopCount = 0;
        NodeState->Edges[0].ClampedLoop = 0;

        Assert(!IsDag2SourceIndexValid(NodeState->Edges[0].SourceIndex));
        Assert(GetNodeDataPoseAnimSource(NewNode));
    }

    return NewNode;
}

static real32
ComputeRawTime(real32 AtTime,
               real32 Speed,
               real32 Offset,
               int32x LoopCount,
               real32 AnimDuration)
{
    real32 Unclamped = (AtTime * Speed) + Offset;
    if (LoopCount <= 0)
        return Unclamped;

    real32 ClampStart = Offset;
    real32 ClampEnd   = ((AnimDuration * LoopCount) * Speed) + Offset;

    return Clamp(ClampStart, Unclamped, ClampEnd);
}

static real32
ComputeLoopedTime(real32 AtTime,
                  real32 Speed,
                  real32 Offset,
                  int32x LoopCount,
                  real32 AnimDuration)
{
    real32 LocalUnclamped = (AtTime * Speed);
    if (LoopCount <= 0)
        return Modulus(LocalUnclamped, AnimDuration);

    real32 LocalClampEnd = ((AnimDuration * LoopCount) * Speed);

    if (LocalUnclamped <= 0)
        return 0;
    else if (LocalUnclamped >= LocalClampEnd)
        return AnimDuration;

    return Modulus(LocalUnclamped, AnimDuration);
}

local_pose* GRANNY
SampleDAG_PoseAnimSource(dag2_node& Node,
                         dag2_nodedata_poseanimsource& NodeData,
                         dag2_instance& Instance,
                         int32x OnOutput,
                         real32 AtTime,
                         pose_cache& PoseCache)
{
    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckInt32Index(OnOutput, NodeData.EdgeCount, return 0);

    skeleton* Skeleton = Instance.SourceModel->Skeleton;
    Assert(Skeleton);

    dag2_instancedata_pas* DataArray =
        GET_DAG2INST_DATA(dag2_instancedata_pas, Instance, Node);
    Assert(DataArray);

    dag2_instancedata_pas& Data = DataArray[OnOutput];

    // If we're not actually bound to an animation, bail.
    if (Data.ControlledAnim.Binding == 0)
        return 0;

    local_pose* RetValue = GetNewLocalPose(PoseCache, Skeleton->BoneCount);
    if (RetValue)
    {
        animation_binding* Binding = Data.ControlledAnim.Binding;

        real32 const LocalSampleTime =
            ComputeLoopedTime(AtTime,
                              Data.Speed, Data.Offset, Data.LoopCount,
                              Binding->ID.Animation->Duration);

        sample_context Context;
        Context.UnderflowLoop = !Data.ClampedLoop;
        Context.OverflowLoop  = !Data.ClampedLoop;
        Context.LocalDuration = Binding->ID.Animation->Duration;
        Context.LocalClock    = LocalSampleTime;
        if (Context.LocalClock < 0.0f)
            Context.LocalClock += Context.LocalDuration;
        Context.LocalClock    = Clamp(0.0f, Context.LocalClock, Context.LocalDuration);
        Assert(Context.LocalClock >= 0.0f && Context.LocalClock <= Context.LocalDuration);

        SetContextFrameIndex(Context,
                             Context.LocalClock,
                             Binding->ID.Animation->Duration,
                             Binding->ID.Animation->TimeStep);

        BeginLocalPoseAccumulation(*RetValue, 0, Skeleton->BoneCount, 0 );

        AccumulateControlledAnimation(Data.ControlledAnim,
                                      Context, 1.0f, *Skeleton,
                                      0, Skeleton->BoneCount,
                                      *RetValue, 0.0f, 0);

        EndLocalPoseAccumulationLOD(*RetValue, 0, Skeleton->BoneCount, 0, Skeleton->BoneCount,
                                    *Skeleton, 0.0f, 0);
    }

    // Default, or uninitialized var
    return RetValue;
}

bool GRANNY
GetRootMotion_PoseAnimSource(dag2_node& Node,
                             dag2_nodedata_poseanimsource& NodeData,
                             dag2_instance& Instance,
                             int32x OnOutput,
                             real32 SecondsElapsed,
                             real32 AtTime,
                             triple& ResultTranslation,
                             triple& ResultRotation)
{
    CheckPointerNotNull(Instance.InstanceData, return 0);
    CheckPointerNotNull(Instance.SourceModel, return 0);
    CheckInt32Index(OnOutput, NodeData.EdgeCount, return 0);

    // Unlike the controlled animation version of this, we /do/ need to ensure that the
    // vectors are zero filled before we start
    VectorZero3(ResultTranslation);
    VectorZero3(ResultRotation);

    skeleton* Skeleton = Instance.SourceModel->Skeleton;
    Assert(Skeleton);

    dag2_instancedata_pas* DataArray =
        GET_DAG2INST_DATA(dag2_instancedata_pas, Instance, Node);
    Assert(DataArray);

    dag2_instancedata_pas& Data = DataArray[OnOutput];

    // If we're not actually bound to an animation, return a rest pose...
    if (Data.ControlledAnim.Binding == 0 ||
        Data.ControlledAnim.AccumulationMode == NoAccumulation )
    {
        return true;
    }

    animation_binding* Binding = Data.ControlledAnim.Binding;
    real32 const LocalDuration = Binding->ID.Animation->Duration;

    real32 LocalEndTime = ComputeRawTime(AtTime,
                                         Data.Speed, Data.Offset, Data.LoopCount,
                                         LocalDuration);
    real32 LocalStartTime = ComputeRawTime(AtTime - SecondsElapsed,
                                           Data.Speed, Data.Offset, Data.LoopCount,
                                           LocalDuration);
    bool   FlipResult = false;
    if (LocalStartTime > LocalEndTime)
    {
        real32 Temp    = LocalEndTime;
        LocalEndTime   = LocalStartTime;
        LocalStartTime = Temp;
        FlipResult = true;
    }

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


bool GRANNY
SampleDAGLockParams_PoseAnimSource(dag2_node& Node,
                                   dag2_nodedata_poseanimsource& NodeData,
                                   dag2_instance& Instance,
                                   int32x OnOutput,
                                   real32 AtTime,
                                   real32& Duration)
{
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(Instance.SourceModel, return false);
    CheckInt32Index(OnOutput, Node.ExternalOutputEdgeCount, return false);
    CheckCondition(Node.ExternalOutputEdgeCount == NodeData.EdgeCount, return false);

    dag2_instancedata_pas* Data =
        GET_DAG2INST_DATA(dag2_instancedata_pas, Instance, Node);

    if (Data->ControlledAnim.Binding == 0)
        return false;

    animation_binding* Binding = Data->ControlledAnim.Binding;
    animation const* Animation = Binding->ID.Animation;

    // Copy out the animation duration
    Duration  = Animation->Duration;

    return true;
}


int32 GRANNY
GetInstanceDataSize_PoseAnimSource(dag2_node& Node, dag2_nodedata_poseanimsource& NodeData,
                                   dag2& TheDag, model& FromModel, model& ToModel)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);

    return (NodeData.EdgeCount * SizeOf(dag2_instancedata_pas));
}


bool GRANNY
BindDAGInstanceData_PoseAnimSource(dag2_node&                    Node,
                                   dag2_nodedata_poseanimsource& NodeData,
                                   dag2_instance&                Instance)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);

    dag2_instancedata_pas* Data = GET_DAG2INST_DATA(dag2_instancedata_pas, Instance, Node);
    {for(int32x EdgeIdx = 0; EdgeIdx < NodeData.EdgeCount; ++EdgeIdx)
    {
        dag2_instancedata_pas& ThisData = Data[EdgeIdx];
        dag2_nodedata_poseanimsource_edge& Edge = NodeData.Edges[EdgeIdx];

        ZeroStructure(ThisData);

        // Always do the speed and offset, regardless of whether or not we have a source
        ThisData.Speed       = Edge.Speed;
        ThisData.Offset      = Edge.Offset;
        ThisData.LoopCount   = Edge.LoopCount;
        ThisData.ClampedLoop = Edge.ClampedLoop;

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
BakeOutDAGInstanceData_PoseAnimSource(dag2_node&                    Node,
                                      dag2_nodedata_poseanimsource& NodeData,
                                      dag2_instance&                Instance,
                                      memory_arena&                 Arena)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);
    CheckPointerNotNull(Instance.InstanceData, return false);

    dag2_instancedata_pas* Data = GET_DAG2INST_DATA(dag2_instancedata_pas, Instance, Node);

    {for(int32x EdgeIdx = 0; EdgeIdx < NodeData.EdgeCount; ++EdgeIdx)
    {
        dag2_instancedata_pas& ThisData = Data[EdgeIdx];
        dag2_nodedata_poseanimsource_edge& Edge = NodeData.Edges[EdgeIdx];

        Edge.Speed       = ThisData.Speed;
        Edge.Offset      = ThisData.Offset;
        Edge.LoopCount   = ThisData.LoopCount;
        Edge.ClampedLoop = ThisData.ClampedLoop;
    }}

    return true;
}


bool GRANNY
UnbindDAGInstanceData_PoseAnimSource(dag2_node&                    Node,
                                     dag2_nodedata_poseanimsource& NodeData,
                                     dag2_instance&                Instance)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);
    CheckPointerNotNull(Instance.InstanceData, return false);

    CheckPointerNotNull(Instance.InstanceData, return false);
    dag2_instancedata_pas* Data = GET_DAG2INST_DATA(dag2_instancedata_pas, Instance, Node);

    {for(int32x EdgeIdx = 0; EdgeIdx < NodeData.EdgeCount; ++EdgeIdx)
    {
        dag2_instancedata_pas& ThisData = Data[EdgeIdx];
        dag2_nodedata_poseanimsource_edge& Edge = NodeData.Edges[EdgeIdx];

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
SetFloat_PoseAnimSource(dag2_node& Node,
                        dag2_nodedata_poseanimsource& NodeData,
                        dag2_instance& Instance,
                        char const* ParamName,
                        real32 Value)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);

    dag2_instancedata_pas* Data =
        GET_DAG2INST_DATA(dag2_instancedata_pas, Instance, Node);

    char const* Underscore = FindLastInstanceOf(ParamName, '_');

    // If there is no underscore indicator, we'll set all of the values...
    int32x Index = -1;
    if (Underscore)
    {
        Index = ConvertToInt32(Underscore + 1);
        CheckInt32Index(Index, NodeData.EdgeCount, return false);
    }

    if (StringBeginsWithLowercase(ParamName, "Speed"))
    {
        if (Index != -1)
        {
            Data[Index].Speed = Value;
        }
        else if (StringsAreEqualLowercase(ParamName, "Speed"))
        {
            {for(int32x Idx = 0; Idx < NodeData.EdgeCount; ++Idx)
            {
                Data[Idx].Speed = Value;
            }}
        }
    }
    if (StringsAreEqualLowercase(ParamName, "Offset"))
    {
        if (Index != -1)
        {
            Data[Index].Offset = Value;
        }
        else if (StringsAreEqualLowercase(ParamName, "Offset"))
        {
            {for(int32x Idx = 0; Idx < NodeData.EdgeCount; ++Idx)
            {
                Data[Idx].Offset = Value;
            }}
        }
    }
    else
    {
        // Log?
        return false;
    }

    return true;
}

bool GRANNY
GetFloat_PoseAnimSource(dag2_node& Node,
                        dag2_nodedata_poseanimsource& NodeData,
                        dag2_instance& Instance,
                        char const* ParamName,
                        real32* Value)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);
    CheckPointerNotNull(Value, return false);

    dag2_instancedata_pas* Data =
        GET_DAG2INST_DATA(dag2_instancedata_pas, Instance, Node);

    char const* Underscore = FindLastInstanceOf(ParamName, '_');

    // If there is no underscore indicator, we'll set all of the values...
    int32x Index = -1;
    if (Underscore)
    {
        Index = ConvertToInt32(Underscore + 1);
        CheckInt32Index(Index, NodeData.EdgeCount, return false);
    }

    if (StringBeginsWithLowercase(ParamName, "Speed"))
    {
        if (Index != -1)
        {
            *Value = Data[Index].Speed;
            return true;
        }
    }
    if (StringBeginsWithLowercase(ParamName, "Offset"))
    {
        if (Index != -1)
        {
            *Value = Data[Index].Offset;
            return true;
        }
    }
    else if (StringBeginsWithLowercase(ParamName, "Duration"))
    {
        if (Index != -1)
        {
            if (Data[Index].ControlledAnim.Binding == 0)
                return false;

            animation_binding* Binding = Data->ControlledAnim.Binding;
            real32 const LocalDuration = Binding->ID.Animation->Duration;

            *Value = LocalDuration;
            return true;
        }
    }

    return false;
}


bool GRANNY
SetInt32_PoseAnimSource(dag2_node& Node,
                        dag2_nodedata_poseanimsource& NodeData,
                        dag2_instance& Instance,
                        char const* ParamName,
                        int32 Value)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);
    CheckCondition(Value >= 0, return false);

    dag2_instancedata_pas* Data =
        GET_DAG2INST_DATA(dag2_instancedata_pas, Instance, Node);

    char const* Underscore = FindLastInstanceOf(ParamName, '_');

    // If there is no underscore indicator, we'll set all of the values...
    int32x Index = -1;
    if (Underscore)
    {
        Index = ConvertToInt32(Underscore + 1);
        CheckInt32Index(Index, NodeData.EdgeCount, return false);
    }

    if (StringBeginsWithLowercase(ParamName, "LoopCount"))
    {
        if (Index != -1)
        {
            Data[Index].LoopCount = Value;
        }
        else if (StringsAreEqualLowercase(ParamName, "LoopCount"))
        {
            {for(int32x Idx = 0; Idx < NodeData.EdgeCount; ++Idx)
            {
                Data[Idx].LoopCount = Value;
            }}
        }
    }
    else if (StringBeginsWithLowercase(ParamName, "ClampedLoop"))
    {
        bool32 NewValue = (Value != 0) ? 1 : 0;
        if (Index != -1)
        {
            Data[Index].ClampedLoop = NewValue;
        }
        else if (StringsAreEqualLowercase(ParamName, "ClampedLoop"))
        {
            {for(int32x Idx = 0; Idx < NodeData.EdgeCount; ++Idx)
            {
                Data[Idx].ClampedLoop = NewValue;
            }}
        }
    }
    else
    {
        // Log?
        return false;
    }

    return true;
}

bool GRANNY
GetInt32_PoseAnimSource(dag2_node& Node,
                        dag2_nodedata_poseanimsource& NodeData,
                        dag2_instance& Instance,
                        char const* ParamName,
                        int32* Value)
{
    Assert(Node.ExternalOutputEdgeCount == NodeData.EdgeCount);
    CheckPointerNotNull(Instance.InstanceData, return false);
    CheckPointerNotNull(ParamName, return false);
    CheckPointerNotNull(Value, return false);

    dag2_instancedata_pas* Data =
        GET_DAG2INST_DATA(dag2_instancedata_pas, Instance, Node);

    char const* Underscore = FindLastInstanceOf(ParamName, '_');

    // If there is no underscore indicator, we'll set all of the values...
    int32x Index = -1;
    if (Underscore)
    {
        Index = ConvertToInt32(Underscore + 1);
        CheckInt32Index(Index, NodeData.EdgeCount, return false);
    }

    if (StringBeginsWithLowercase(ParamName, "LoopCount"))
    {
        if (Index != -1)
        {
            *Value = Data[Index].LoopCount;
            return true;
        }
    }
    else if (StringBeginsWithLowercase(ParamName, "ClampedLoop"))
    {
        if (Index != -1)
        {
            return true;
        }
    }

    return false;
}

