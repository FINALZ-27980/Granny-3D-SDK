// ========================================================================
// $File: //jeffr/granny_29/rt/granny_spu_controlled_animation.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_spu_controlled_animation.h"

#include "granny_aggr_alloc.h"
#include "granny_control.h"
#include "granny_math.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_model.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_retargeter.h"
#include "granny_spu_animation.h"
#include "granny_spu_animation_binding.h"
#include "granny_spu_track_group.h"
#include "granny_track_group.h"
#include "granny_track_sampler.h"

#if PLATFORM_PS3
#include "granny_spu_curve_sample_ppu.h"
#endif

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#undef SubsystemCode
#define SubsystemCode ControlLogMessage

USING_GRANNY_NAMESPACE;
BEGIN_GRANNY_NAMESPACE;

struct spu_track_target
{
    spu_animation_binding* Binding;
    spu_animation_binding_id ID;

    model_instance *OnInstance;
    accumulation_mode AccumulationMode;

    track_mask *TrackMask;
    track_mask *ModelMask;

    mirror_specification* MirrorSpecification;
};

struct controlled_spu_animation_builder
{
    real32 CurrentClock;
    spu_animation const *Animation;

    int32x TrackCount;
    spu_track_target *Tracks;
};

END_GRANNY_NAMESPACE;

controlled_animation* SPUAnimationGetControlledAnimation ( model_control_binding &Binding )
{
    // It's not a controlled_animation!  (It *sorta* is, but not really.)
    return NULL;
}

controlled_pose* SPUAnimationGetControlledPose ( model_control_binding &Binding )
{
    // It's not a pose!
    return NULL;
}

spu_controlled_animation* SPUAnimationGetSPUControlledAnimation ( model_control_binding &Binding )
{
    VAR_FROM_DERIVED(spu_controlled_animation, State, Binding);
    Assert(State);

    return State;
}

static bool
SPUAnimationInitializeBindingState(model_control_binding &Binding,
                                   void *InitData)
{
    VAR_FROM_DERIVED(spu_controlled_animation, State, Binding);
    Assert(State);

    spu_track_target *TrackTarget = (spu_track_target *)InitData;
    Assert(TrackTarget);

    State->Binding = AcquireSPUAnimationBindingFromID(TrackTarget->ID);
    State->AccumulationMode = TrackTarget->AccumulationMode;
    State->TrackMask = TrackTarget->TrackMask;
    State->ModelMask = TrackTarget->ModelMask;
    State->MirrorSpecification = TrackTarget->MirrorSpecification;

    Assert(State->Binding);
    Assert(State->Binding->TrackNameRemaps);

    Binding.BindingType = SPUControlledAnim;

    return(State->Binding != 0);
}


static void
ComputeTotalDeltasFromSPUBinding(spu_animation_binding* Binding,
                                 triple*                TotalPositionDelta,
                                 triple*                TotalOrientationDelta)
{
#if PLATFORM_PS3
    CheckPointerNotNull(Binding, return);
    CheckPointerNotNull(TotalPositionDelta, return);
    CheckPointerNotNull(TotalOrientationDelta, return);

    triple& PositionDelta    = *TotalPositionDelta;
    triple& OrientationDelta = *TotalOrientationDelta;

    spu_animation_binding_id const& BindingID = Binding->ID;
    if (Binding->RootBoneTrack < 0)
    {
        VectorZero3(PositionDelta);
        VectorZero3(OrientationDelta);
        return;
    }

    spu_transform_track const& RootTrack = BindingID.TransformTracks[Binding->RootBoneTrack];
    if (RootTrack.PosCurveOffset == SPUTransformTrackNoCurve)
    {
        VectorZero3 ( PositionDelta );
    }
    else
    {
        triple FirstPosition;
        triple LastPosition;

        // Question - is this correct? These are the control points,
        // but that's not 100% guaranteed to be where the _curve_ goes through!
        ExtractFirstAndLastKnots(BindingID.CurveBytes + RootTrack.PosCurveOffset,
                                 3, FirstPosition, LastPosition);
        VectorSubtract3(PositionDelta, LastPosition, FirstPosition);
    }

    if (RootTrack.OriCurveOffset == SPUTransformTrackNoCurve)
    {
        VectorZero3 ( OrientationDelta );
    }
    else
    {
        quad FirstQuaternion;
        quad LastQuaternion;
        // Question - is this correct? These are the control points,
        // but that's not 100% guaranteed to be where the _curve_ goes through!
        ExtractFirstAndLastKnots(BindingID.CurveBytes + RootTrack.OriCurveOffset,
                                 4, FirstQuaternion, LastQuaternion);
        QuaternionDifferenceToAngularVelocity(OrientationDelta, FirstQuaternion, LastQuaternion);
    }
#else
    VectorZero3(*TotalPositionDelta);
    VectorZero3(*TotalOrientationDelta);
#endif
}

static void
AccumulateSPUControlledAnimationMotionVectors(spu_controlled_animation* State,
                                              real32 StartLocalClock,
                                              real32 EndLocalClock,
                                              real32 LocalDuration,
                                              real32 Weight,
                                              bool   FlipResult,
                                              spu_transform_track const& Track,
                                              real32* TotalWeight,
                                              real32* ResultTranslation,
                                              real32* ResultRotation)
{
    // So now clamping has been done, and we can ignore the over/underflow looping.
    // Any loops remaining are real loops and should be performed.
    // Also, start should always be less than end - any flips should be encoded in FlipResult.
    Assert ( StartLocalClock <= EndLocalClock );

    switch (State->AccumulationMode)
    {
        case ConstantExtractionAccumulation:
        {
            InvalidCodePath("CME not supported in SPU track_groups");
        } break;

        case VariableDeltaAccumulation:
        {
            // Now move both start and end so they lie in the interval [0,LocalDuration),
            // moving only in multiples of LocalDuration, and recording
            // how many multiples that was.
            int32x LoopCount = 0;
            int32x LocalLoopCount = FloorReal32ToInt32 ( StartLocalClock / LocalDuration );
            StartLocalClock -= LocalLoopCount * LocalDuration;
            LoopCount -= LocalLoopCount;
            StartLocalClock = Clamp(0.0f, StartLocalClock, LocalDuration);
            Assert ( ( StartLocalClock >= 0.0f ) && ( StartLocalClock <= LocalDuration ) );

            LocalLoopCount = FloorReal32ToInt32 ( EndLocalClock / LocalDuration );
            EndLocalClock -= LocalLoopCount * LocalDuration;
            LoopCount += LocalLoopCount;
            EndLocalClock = Clamp(0.0f, EndLocalClock, LocalDuration);
            Assert ( ( EndLocalClock >= 0.0f ) && ( EndLocalClock <= LocalDuration ) );

            // Now sample start and end times from the animation and find the delta between them.
            sample_context Context;
            // NOTE: VDA not supported on keyframed animations
            Context.LocalClock = EndLocalClock;
            Context.LocalDuration = LocalDuration;
            SetContextFrameIndex(Context, Context.LocalClock,
                                 State->Binding->ID.Animation->Duration,
                                 State->Binding->ID.Animation->TimeStep);

            Context.UnderflowLoop = false;
            Context.OverflowLoop = false;

            ALIGN16_STACK(real32, EndPosition, 3);
            ALIGN16_STACK(real32, EndOrientation, 4);
            SPUSampleTrackPOLocal(Context, *State->Binding->ID.TrackGroup, Track, EndPosition, EndOrientation);

            Context.LocalClock = StartLocalClock;
            SetContextFrameIndex(Context, Context.LocalClock,
                                 State->Binding->ID.Animation->Duration,
                                 State->Binding->ID.Animation->TimeStep);

            ALIGN16_STACK(real32, StartPosition,    3);
            ALIGN16_STACK(real32, StartOrientation, 4);
            SPUSampleTrackPOLocal(Context, *State->Binding->ID.TrackGroup, Track, StartPosition, StartOrientation);

            triple ResidualPositionDelta;
            VectorSubtract3(ResidualPositionDelta, EndPosition, StartPosition);

            triple ResidualOrientationDelta;
            QuaternionDifferenceToAngularVelocity(ResidualOrientationDelta,
                                                  StartOrientation, EndOrientation);
            if ( LoopCount != 0 )
            {
                // TODO: Note that this is not guaranteed to be accurate
                // for rotational motions, because technically you would
                // have to account for the fact that the motion can loop
                // back on itself.  I could look into supporting this later,
                // although it is unlikely anyone will care because it
                // only occurs with severely large timesteps, which could
                // always be subdivided as a workaround.
                triple TotalPositionDelta;
                triple TotalOrientationDelta;
                ComputeTotalDeltasFromSPUBinding(State->Binding,
                                                 &TotalPositionDelta,
                                                 &TotalOrientationDelta);

                ScaleVectorAdd3(ResidualOrientationDelta, (real32)LoopCount, TotalOrientationDelta);
                ScaleVectorAdd3(ResidualPositionDelta, (real32)LoopCount, TotalPositionDelta);
            }

            if ( FlipResult )
            {
                VectorNegate3 ( ResidualOrientationDelta );
                VectorNegate3 ( ResidualPositionDelta );
            }

            // Factor base orientation
            {
                quad BaseOrientation;
                VectorEquals4(BaseOrientation, Track.LODTransform.Orientation);
                NormalQuaternionTransform3(ResidualPositionDelta, BaseOrientation);
                NormalQuaternionTransform3(ResidualOrientationDelta, BaseOrientation);
            }

            Conjugate4(StartOrientation);
            NormalQuaternionTransform3(ResidualPositionDelta, StartOrientation);

            ScaleVectorAdd3(ResultRotation, Weight, ResidualOrientationDelta);
            ScaleVectorAdd3(ResultTranslation, Weight, ResidualPositionDelta);

            *TotalWeight += Weight;
        } break;

        case NoAccumulation:
        default:
        {
            InvalidCodePath("Unrecognized accumulation type");
        } break;
    }
}

// NB: Essentially the same as animationaccumlooptransform.  Factor common time math?
static void
SPUAnimationAccumulateLoopTransform(model_control_binding &Binding,
                                    real32 SecondsElapsed,
                                    real32 &TotalWeight,
                                    real32 *ResultTranslation,
                                    real32 *ResultRotation,
                                    bool Inverse)
{
    // Remember that ResultTranslation and ResultRotation are already valid,
    // and we need to accumulate this animation's motion to it.
    // (so it's not a bug that I don't initialize them to zero!)

    // Should also add a bit to the docs saying that these use the control's speed
    // to know how much local time has elapsed. If people are setting the clocks every
    // frame, then normally they don't need to set the speeds - but they still do have to
    // with CME or VDA.

    VAR_FROM_DERIVED(spu_controlled_animation, State, Binding);
    Assert(State);

    if (State->AccumulationMode == NoAccumulation)
    {
        return;
    }

    // Find out what weight this anim is applied at
    real32 const Weight = GetControlEffectiveWeight(*Binding.Control);
    if(Weight <= TrackWeightEpsilon)
    {
        return;
    }

    spu_animation_binding_id const& BindingID = State->Binding->ID;
    if (State->Binding->RootBoneTrack < 0)
        return;

    if (!BindingID.TransformTracks)
        return;

    Assert(State->Binding->RootBoneTrack >= 0 && State->Binding->RootBoneTrack < BindingID.TransformTrackCount);
    spu_transform_track& RootTrack =
        BindingID.TransformTracks[State->Binding->RootBoneTrack];

    real32 LocalDuration = GetControlLocalDuration(*Binding.Control);

    if ( LocalDuration <= TimeEffectivelyZero )
    {
        // Zero-length animations are not going to have any motion.
        return;
    }

    // Adjust the seconds elapsed to be in the local space of the animation
    SecondsElapsed *= GetControlSpeed(*Binding.Control);

    // Find the local clock start and end.
    real32 EndLocalClock = GetControlRawLocalClock(*Binding.Control);
    real32 StartLocalClock = EndLocalClock - SecondsElapsed;

#if DEBUG
    // Before we start modifying them.
    real32 OriginalStartLocalClock = StartLocalClock;
    real32 OriginalEndLocalClock = EndLocalClock;
    OriginalStartLocalClock = OriginalStartLocalClock;      // Stops silly warnings.
    OriginalEndLocalClock = OriginalEndLocalClock;
#endif

    bool FlipResult = Inverse;
    if ( StartLocalClock > EndLocalClock )
    {
        // Negative SecondsElapsed or control speed.
        Swap ( StartLocalClock, EndLocalClock );
        FlipResult = !FlipResult;
    }

    // Clamp the effective start and end times to the animation start
    // and end times if there is no looping on that end.
    bool32 UnderflowLoop;
    bool32 OverflowLoop;
    GetControlLoopState(*Binding.Control,
                        UnderflowLoop,
                        OverflowLoop);
    if ( !UnderflowLoop )
    {
        // Clamp off start.
        if ( EndLocalClock < 0.0f )
        {
            Assert ( StartLocalClock < 0.0f );
            // Easy case - nothing happens.
            return;
        }
        else if ( StartLocalClock < 0.0f )
        {
            StartLocalClock = 0.0f;
        }
    }
    if ( !OverflowLoop )
    {
        // Clamp off end.
        if ( StartLocalClock > LocalDuration )
        {
            Assert ( EndLocalClock > LocalDuration );
            // Easy case - nothing happens.
            return;
        }
        else if ( EndLocalClock > LocalDuration )
        {
            EndLocalClock = LocalDuration;
        }
    }

    // So now clamping has been done, and we can ignore the over/underflow looping.
    // Any loops remaining are real loops and should be performed.
    // Also, start should always be less than end - any flips should be encoded in FlipResult.
    Assert ( StartLocalClock <= EndLocalClock );

    // Use factored out function for this
    AccumulateSPUControlledAnimationMotionVectors(State, StartLocalClock, EndLocalClock,
                                                  LocalDuration, Weight, FlipResult,
                                                  RootTrack,
                                                  &TotalWeight, ResultTranslation, ResultRotation);
}

static void
SPUAnimationCleanupBindingState(model_control_binding &Binding)
{
    VAR_FROM_DERIVED(spu_controlled_animation, State, Binding);
    Assert(State);

    ReleaseSPUAnimationBinding(State->Binding);
}

static bool
SPUAnimationControlIsRebased(model_control_binding &Binding)
{
    VAR_FROM_DERIVED(spu_controlled_animation, State, Binding);
    Assert(State);

    return State && State->Binding && State->Binding->Retargeter != 0;
}


static model_control_callbacks ControlledAnimationCallbacks =
{
    SPUAnimationGetControlledAnimation,
    SPUAnimationGetControlledPose,
    SPUAnimationGetSPUControlledAnimation,
    SPUAnimationInitializeBindingState,
    NULL,                                // AnimationAccumulateBindingState,
    NULL,                                // AnimationBuildDirect,
    SPUAnimationAccumulateLoopTransform, // AnimationAccumulateLoopTransform,
    SPUAnimationCleanupBindingState,     // AnimationCleanupBindingState,
    SPUAnimationControlIsRebased
};


control* GRANNY
PlayControlledSPUAnimation(real32 StartTime,
                           spu_animation const& Animation,
                           model_instance& Model)
{
    control *Result = 0;

    int32x TrackGroupIndex;
    if (FindSPUTrackGroupForModel(Animation,
                                  GetSourceModel(Model).Name,
                                  TrackGroupIndex))
    {
        controlled_spu_animation_builder *Builder =
            BeginControlledSPUAnimation(StartTime, Animation);
        if(Builder)
        {
            SetSPUTrackGroupTarget(*Builder, TrackGroupIndex, Model);
            Result = EndControlledSPUAnimation(Builder);
        }
    }

    return Result;
}


static accumulation_mode
AccumulationModeFromSPUTrackGroup(spu_track_group& TrackGroup)
{
    if (TrackGroup.Flags & AccumulationExtracted)
        return ConstantExtractionAccumulation;
    else if (TrackGroup.Flags & AccumulationIsVDA)
        return VariableDeltaAccumulation;
    else
        return NoAccumulation;
}


controlled_spu_animation_builder *GRANNY
BeginControlledSPUAnimation(real32 StartTime, spu_animation const &Animation)
{
    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    controlled_spu_animation_builder* Builder;
    AggrAllocPtr(Allocator, Builder);
    AggrAllocOffsetArrayPtr(Allocator, Builder,
                            Animation.TrackGroupCount,
                            TrackCount, Tracks);
    if(EndAggrAlloc(Allocator, AllocationInstance))
    {
        Builder->CurrentClock = StartTime;
        Builder->Animation = &Animation;

        {for(int32x TrackIndex = 0; TrackIndex < Builder->TrackCount; ++TrackIndex)
        {
            spu_track_target &Track = Builder->Tracks[TrackIndex];
            ZeroStructure(Track);

            Track.AccumulationMode =
                AccumulationModeFromSPUTrackGroup(*Animation.TrackGroups[TrackIndex]);
        }}
    }

    return Builder;
}


control *GRANNY
EndControlledSPUAnimation(controlled_spu_animation_builder *Builder)
{
    control* Control = 0;
    if(Builder)
    {
        // Create the control
        Control = CreateControl(Builder->CurrentClock, Builder->Animation->Duration);
        if(Control)
        {
            // Create the bindings
            {for (int32x TrackIndex = 0; TrackIndex < Builder->TrackCount; ++TrackIndex)
            {
                spu_track_target &Track = Builder->Tracks[TrackIndex];
                if (Track.OnInstance)
                {
                     model_control_binding *Binding =
                         CreateModelControlBinding(ControlledAnimationCallbacks, *Control,
                                                   *Track.OnInstance,
                                                   ControlIsActive(*Control), &Track);
                     if (!Binding)
                         Log0(ErrorLogMessage, ControlLogMessage,
                              "Unable to bind track group");
                }
            }}
        }
        else
        {
            Log0(ErrorLogMessage, ControlLogMessage,
                 "Unable to create control");
        }

        Deallocate(Builder);
    }

    return Control;
}

void GRANNY
SetSPUTrackGroupTarget(controlled_spu_animation_builder& Builder,
                       int32x TrackGroupIndex,
                       model_instance& Model)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    spu_track_target &Track = Builder.Tracks[TrackGroupIndex];

    Track.OnInstance = &Model;

    MakeDefaultSPUAnimationBindingID(Track.ID, Builder.Animation, TrackGroupIndex, &GetSourceModel(Model));
//     Track.ID.Model = &GetSourceModel(Model);
//     Track.ID.Animation = Builder.Animation;
//     Track.ID.TrackGroup = Builder.Animation->TrackGroups[TrackGroupIndex];
//     Track.ID.TransformTrackCount = Track.ID.TrackGroup->TransformTrackCount;
//     Track.ID.TransformTracks     = Track.ID.TrackGroup->TransformTracks;
//     Track.ID.CurveByteCount      = Track.ID.TrackGroup->CurveByteCount;
//     Track.ID.CurveBytes          = Track.ID.TrackGroup->CurveBytes;
}

void GRANNY
SetSPUTrackGroupTrackMask(controlled_spu_animation_builder &Builder,
                          int32x TrackGroupIndex,
                          track_mask &TrackMask)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    spu_track_target &Track = Builder.Tracks[TrackGroupIndex];

    Track.TrackMask = &TrackMask;
}

void GRANNY
SetSPUTrackGroupModelMask(controlled_spu_animation_builder &Builder,
                          int32x TrackGroupIndex,
                          track_mask &ModelMask)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    spu_track_target &Track = Builder.Tracks[TrackGroupIndex];

    Track.ModelMask = &ModelMask;
}

void GRANNY
SetSPUTrackGroupBasisTransform(controlled_spu_animation_builder &Builder,
                               int32x TrackGroupIndex,
                               model &FromModel, model &ToModel)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    spu_track_target &Track = Builder.Tracks[TrackGroupIndex];

    Track.ID.FromBasis = &FromModel;
    Track.ID.ToBasis = &ToModel;
}

void GRANNY
SetSPUTrackGroupMirrorSpecification(controlled_spu_animation_builder& Builder,
                                    int32x TrackGroupIndex,
                                    mirror_specification& Specification)
{
    CheckInt32Index(TrackGroupIndex, Builder.TrackCount, return);
    CheckPointerNotNull(Builder.Tracks[TrackGroupIndex].OnInstance, return);

    spu_track_target &Track = Builder.Tracks[TrackGroupIndex];
    Track.MirrorSpecification = &Specification;

    // Not necessary for SPU animations, the mirror cache is on the SPU local store...
    //CreateMirrorTransformCache(*Builder.Tracks[TrackGroupIndex].OnInstance);
}

