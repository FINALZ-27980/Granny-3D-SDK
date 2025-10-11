// ========================================================================
// $File: //jeffr/granny_29/rt/granny_control.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_control.h"

#include "granny_animation.h"
#include "granny_animation_binding.h"
#include "granny_controlled_animation.h"
#include "granny_controlled_pose.h"
#include "granny_data_type_definition.h"
#include "granny_fixed_allocator.h"
#include "granny_floats.h"
#include "granny_math.h"
#include "granny_parameter_checking.h"
#include "granny_spu_animation.h"
#include "granny_spu_animation_binding.h"
#include "granny_spu_controlled_animation.h"
#include "granny_telemetry.h"

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


// Purely a debugging function to make sure these two are in valid ranges.
static void
CheckLocalClockAndLoopIndex ( control &Control )
{
#if DEBUG
#if !COMPILER_GCC
    // Due to small precision errors when doing modulo arithmetic,
    // these values can sometimes be epsilon out. You may need to
    // tweak epsilon according to your app (or processor!)
    real32 const Epsilon = 1e-6f;
    if ( Control.LoopCount == 0 )
    {
        // Infinite looping.
        Assert ( Control.LocalClock >= 0.0f - Epsilon );
        Assert ( Control.LocalClock < Control.LocalDuration + Epsilon );
    }
    else
    {
        // Fixed number of loops.
        Assert ( ( Control.LoopIndex >= 0 ) && ( Control.LoopIndex < Control.LoopCount ) );
        Assert ( ( Control.LoopIndex == 0 ) || ( Control.LocalClock >= 0.0f - Epsilon ) );
        Assert ( ( Control.LoopIndex == ( Control.LoopCount - 1 ) ) || ( Control.LocalClock < Control.LocalDuration + Epsilon ) );
    }
#endif
#endif
}

inline void
ModulusLocalClock(control &Control)
{
#if 0       // I never like doing this - the precision issues scare me.
    Control.LoopIndex += TruncateReal32ToInt32(Control.LocalClock /
                                               Control.LocalDuration);
    Control.LocalClock = (real32)Modulus(Control.LocalClock,
                                         Control.LocalDuration);
    if(Control.LocalClock < 0.0f)
    {
        Control.LocalClock += Control.LocalDuration;
    }
#else       // Whereas this is robust - index and clock will never go "out of sync"
    int32x NewLoopIndex = Control.LoopIndex + TruncateReal32ToInt32(Control.LocalClock / Control.LocalDuration);
    if ( Control.LocalClock < 0.0f )
    {
        // -0.1 gets truncated to 0.0f for example.
        NewLoopIndex--;
    }
    // Now clamp to the valid ranges of LoopIndex
    if ( Control.LoopCount != 0 )
    {
        if ( NewLoopIndex < 0 )
        {
            // Underflow.
            NewLoopIndex = 0;
        }
        else if ( NewLoopIndex >= Control.LoopCount )
        {
            // Overflow.
            NewLoopIndex = Control.LoopCount - 1;
        }
    }

    // And apply to the actual values.
    int32x LoopIndexDelta = NewLoopIndex - Control.LoopIndex;
    if ( LoopIndexDelta != 0 )
    {
        Control.LoopIndex += LoopIndexDelta;
        Control.LocalClock -= (real32)LoopIndexDelta * Control.LocalDuration;
    }
#endif

    CheckLocalClockAndLoopIndex ( Control );
}



// Organize the control functions into the bits that we need for the SPU, and those we
// don't.  This avoids some linking hassles caused by the fact that GCC is a stupid piece
// of poo with respect to unused code elimination.

#if !PROCESSOR_CELL_SPU

// Make sure that we align everything to 16byte blocks for the SPU optimizations
#define ControlAlignedSize ((SizeOf(control) + 0xf) & ~0xf)
static fixed_allocator Allocator = { ControlAlignedSize };


inline void
PackEaseCurve(uint32x &Result, real32 Af, real32 Bf, real32 Cf, real32 Df)
{
    int32x Ai = RoundReal32ToInt32(Af * 255.0f);
    int32x Bi = RoundReal32ToInt32(Bf * 255.0f);
    int32x Ci = RoundReal32ToInt32(Cf * 255.0f);
    int32x Di = RoundReal32ToInt32(Df * 255.0f);

    Assert((Ai >= 0) && (Ai <= 255));
    Assert((Bi >= 0) && (Bi <= 255));
    Assert((Ci >= 0) && (Ci <= 255));
    Assert((Di >= 0) && (Di <= 255));

    Result = (Ai << 0) | (Bi << 8) | (Ci << 16) | (Di << 24);
}

static control &
GetGlobalControlSentinel(void)
{
    static control Sentinel;
    if(!Sentinel.Next)
    {
        Sentinel.Next = Sentinel.Prev = &Sentinel;
    }

    return(Sentinel);
}

static void
SortBindings(control &Control)
{
    bool Active = ControlHasEffect(Control);

    model_control_binding *Sentinel = &GetControlBindingSentinel(Control);
    {for(model_control_binding *Iterator = Sentinel->ControlNext;
         Iterator != Sentinel;
         Iterator = Iterator->ControlNext)
    {
        Assert(Iterator->ModelInstance);
        RelinkModelSideOfControlBinding(*Iterator, Active);
    }}
}

model_control_binding &GRANNY
GetControlBindingSentinel(control const &Control)
{
    return((model_control_binding &)Control.BindingSentinel);
}

control *GRANNY
CreateControl(real32 CurrentClock, real32 LocalDuration)
{
    control *Control = (control *)AllocateFixed(Allocator);
    if(Control)
    {
        InitializeSentinel(Control->BindingSentinel);

        Control->Flags = control::ActiveFlag;

        Control->dTLocalClockPending = 0.0f;
        Control->CurrentClock = CurrentClock;
        Control->LocalClock = 0.0f;

        Control->Speed = 1.0f;

        Control->LocalDuration = LocalDuration;
        Control->LoopIndex = 0;
        Control->LoopCount = 1;

        Control->KillClock = 0.0f;

        Control->CurrentWeight = 1.0f;

        Control->Next = GetGlobalControlSentinel().Next;
        Control->Prev = &GetGlobalControlSentinel();
        Control->Next->Prev = Control;
        Control->Prev->Next = Control;

        // SUBTLETY: These have to be initialized because otherwise
        // you can FPU-exception in the clock recentering code, which
        // doesn't know if they've been initialized or not
        Control->EaseInStartClock =
            Control->EaseInEndClock =
            Control->EaseOutStartClock =
            Control->EaseOutEndClock = 0.0f;

        // Set some sensible defaults.
        Control->EaseInValues = (0 << 0) | (0 << 8) | (0xff << 16) | ((uint32)0xff << 24);
        Control->EaseOutValues = (0xff << 0) | (0xff << 8) | (0 << 16) | (0 << 24);
    }

    return(Control);
}

void GRANNY
FreeControl(control *Control)
{
    if(Control)
    {
        // NOTE: I set these all to false, that way if some other
        // deletion would have triggerred during out FreeControlRing()
        // call, it won't trigger, so we'll avoid the double delete.
        SetFlags(*Control,
                 control::ActiveFlag |
                 control::KillOnceCompleteFlag |
                 control::KillOnceUnusedFlag,
                 false);

        Control->Next->Prev = Control->Prev;
        Control->Prev->Next = Control->Next;

        FreeControlRing(GetControlBindingSentinel(*Control));
        DeallocateFixed(Allocator, Control);
    }
}

void GRANNY
FreeControlOnceUnused(control &Control)
{
    SetFlags(Control, control::KillOnceUnusedFlag, true);
    FreeControlIfUnused(&Control);
}

void GRANNY
CompleteControlAt(control &Control, real32 AtSeconds)
{
    SetFlags(Control, control::KillOnceCompleteFlag, true);
    SetFlags(Control, control::KillClockIsLocal, false);
    Control.KillClock = AtSeconds;
}

real32 GRANNY
GetControlCompletionClock(control const &Control)
{
    return Control.KillClock;
}

bool GRANNY
GetControlCompletionCheckFlag(control const &Control)
{
    return(GetFlags(Control, control::KillOnceCompleteFlag));
}

bool GRANNY
GetControlLocalCompletion(control const &Control)
{
    return GetFlags(Control, control::KillClockIsLocal);
}

void GRANNY
SetControlCompletionCheckFlag(control &Control, bool CheckComplete)
{
    SetFlags(Control, control::KillOnceCompleteFlag, CheckComplete);
}

void GRANNY
SetControlLocalCompletion(control& Control, bool CompleteIsLocal)
{
    // Note that this reifies the current control speed.  Changes *after* this call will
    // adjust the kill clock...
    SetFlags(Control, control::KillClockIsLocal, CompleteIsLocal);
}

real32 GRANNY
GetControlClock(control const &Control)
{
    return(Control.CurrentClock);
}

void GRANNY
SetControlClock(control &Control, real32 NewClock)
{
    SetControlClockInline(Control, NewClock);
}

void GRANNY
SetControlClockOnly(control &Control, real32 NewClock)
{
    Control.CurrentClock = NewClock;
}

bool GRANNY
ControlIsComplete(control const &Control)
{
    if (!GetFlags(Control, control::KillOnceCompleteFlag))
        return false;

    return (Control.CurrentClock >= Control.KillClock);
}

bool GRANNY
FreeControlIfComplete(control *Control)
{
    bool Freed = false;

    if(Control)
    {
        if(ControlIsComplete(*Control))
        {
            FreeControl(Control);
            Freed = true;
        }
    }

    return(Freed);
}

bool GRANNY
ControlIsUnused(control const &Control)
{
    model_control_binding *Sentinel = &GetControlBindingSentinel(Control);
    return(Sentinel == Sentinel->ControlNext);
}

bool GRANNY
ControlIsRetargeted(control const& Control)
{
    model_control_binding *Sentinel = &GetControlBindingSentinel(Control);

    if (Sentinel == Sentinel->ControlNext)
        return false;

    if (Sentinel != Sentinel->ControlNext->ControlNext)
        return false;

    model_control_binding* Binding = Sentinel->ControlNext;
    if (Binding->Callbacks->ControlIsRebased)
    {
        return (*Binding->Callbacks->ControlIsRebased)(*Binding);
    }
    else
    {
        return false;
    }
}

bool GRANNY
ControlIsMirrored(control const& Control)
{
    model_control_binding *Sentinel = &GetControlBindingSentinel(Control);

    if (Sentinel == Sentinel->ControlNext)
        return false;

    if (Sentinel != Sentinel->ControlNext->ControlNext)
        return false;

    model_control_binding* Binding = Sentinel->ControlNext;
    if (Binding->Callbacks->ControlIsMirrored)
    {
        return (*Binding->Callbacks->ControlIsMirrored)(*Binding);
    }
    else
    {
        return false;
    }
}

bool GRANNY
FreeControlIfUnused(control *Control)
{
    bool Freed = false;

    if(Control)
    {
        if(GetFlags(*Control, control::KillOnceUnusedFlag) &&
           ControlIsUnused(*Control))
        {
            FreeControl(Control);
            Freed = true;
        }
    }

    return(Freed);
}

real32 GRANNY
GetControlWeight(control const &Control)
{
    return(Control.CurrentWeight);
}


bool GRANNY
SetControlModelMask(control const &Control,
                    model_instance const* ModelInstance,
                    track_mask* TrackMask)
{
    CheckPointerNotNull(ModelInstance, return false);

    model_control_binding *Sentinel = &GetControlBindingSentinel(Control);
    {for(model_control_binding *Iterator = Sentinel->ControlNext;
         Iterator != Sentinel;
         Iterator = Iterator->ControlNext)
    {
        if (Iterator->ModelInstance == ModelInstance)
        {
            Assert(Iterator->Callbacks->GetControlledAnimation &&
                   Iterator->Callbacks->GetControlledPose &&
                   Iterator->Callbacks->GetSPUControlledAnimation);
            controlled_animation*     ControlledAnim    = Iterator->Callbacks->GetControlledAnimation ( *Iterator );
            controlled_pose*          ControlledPose    = Iterator->Callbacks->GetControlledPose ( *Iterator );
            spu_controlled_animation* SPUControlledAnim = Iterator->Callbacks->GetSPUControlledAnimation ( *Iterator );
            if (ControlledAnim != NULL)
            {
                ControlledAnim->ModelMask = TrackMask;
                return true;
            }
            else if (ControlledPose != NULL)
            {
                ControlledPose->ModelMask = TrackMask;
                return true;
            }
            else if (SPUControlledAnim != NULL)
            {
                SPUControlledAnim->ModelMask = TrackMask;
                return true;
            }
            else
            {
                // Probably bad, but no action at present.
            }
        }
    }}

    return false;
}

track_mask const* GRANNY
GetControlTrackGroupModelMask(control const& Control,
                              model_instance const* ModelInstance)
{
    CheckPointerNotNull(ModelInstance, return NULL);

    model_control_binding *Sentinel = &GetControlBindingSentinel(Control);
    {for(model_control_binding *Iterator = Sentinel->ControlNext;
         Iterator != Sentinel;
         Iterator = Iterator->ControlNext)
    {
        if (Iterator->ModelInstance == ModelInstance)
        {
            Assert(Iterator->Callbacks->GetControlledAnimation &&
                   Iterator->Callbacks->GetControlledPose &&
                   Iterator->Callbacks->GetSPUControlledAnimation);
            controlled_animation*     ControlledAnim    = Iterator->Callbacks->GetControlledAnimation ( *Iterator );
            controlled_pose*          ControlledPose    = Iterator->Callbacks->GetControlledPose ( *Iterator );
            spu_controlled_animation* SPUControlledAnim = Iterator->Callbacks->GetSPUControlledAnimation ( *Iterator );
            if (ControlledAnim != NULL)
            {
                return ControlledAnim->ModelMask;
            }
            else if (ControlledPose != NULL)
            {
                return ControlledPose->ModelMask;
            }
            else if (SPUControlledAnim != NULL)
            {
                return SPUControlledAnim->ModelMask;
            }
            else
            {
                // Probably bad, but no action at present.
            }
        }
    }}

    return NULL;
}

bool GRANNY
SetControlTrackMask(control const &Control,
                    animation const* Animation,
                    int32x TrackGroupIndex,
                    track_mask* TrackMask)
{
    CheckPointerNotNull(Animation, return false);
    CheckInt32Index(TrackGroupIndex, Animation->TrackGroupCount, return false);

    model_control_binding *Sentinel = &GetControlBindingSentinel(Control);
    {for(model_control_binding *Iterator = Sentinel->ControlNext;
         Iterator != Sentinel;
         Iterator = Iterator->ControlNext)
    {
        // This is the binding to examine.  Note that poses obviously don't have
        // trackgroups, so always return null
        Assert(Iterator->Callbacks->GetControlledAnimation);
        controlled_animation* ControlledAnim = Iterator->Callbacks->GetControlledAnimation ( *Iterator );
        if (ControlledAnim != NULL)
        {
            animation_binding *Binding = ControlledAnim->Binding;
            if (Binding &&
                Binding->ID.Animation == Animation &&
                Binding->ID.SourceTrackGroupIndex == TrackGroupIndex)
            {
                ControlledAnim->TrackMask = TrackMask;
                return true;
            }
        }
    }}

    return false;
}


track_mask const* GRANNY
GetControlTrackGroupTrackMask(control const&   Control,
                              animation const* Animation,
                              int32x TrackGroupIndex)
{
    CheckPointerNotNull(Animation, return NULL);
    CheckInt32Index(TrackGroupIndex, Animation->TrackGroupCount, return NULL);

    model_control_binding *Sentinel = &GetControlBindingSentinel(Control);
    {for(model_control_binding *Iterator = Sentinel->ControlNext;
         Iterator != Sentinel;
         Iterator = Iterator->ControlNext)
    {
        // This is the binding to examine.  Note that poses obviously don't have
        // trackgroups, so always return null
        Assert(Iterator->Callbacks->GetControlledAnimation);
        controlled_animation* ControlledAnim = Iterator->Callbacks->GetControlledAnimation ( *Iterator );
        if (ControlledAnim != NULL)
        {
            animation_binding *Binding = ControlledAnim->Binding;
            if (Binding &&
                Binding->ID.Animation == Animation &&
                Binding->ID.SourceTrackGroupIndex == TrackGroupIndex)
            {
                return ControlledAnim->TrackMask;
            }
        }
    }}

    return NULL;
}

bool GRANNY
SetSPUControlTrackMask(control const &Control,
                       spu_animation const* Animation,
                       int32x TrackGroupIndex,
                       track_mask* TrackMask)
{
    CheckPointerNotNull(Animation, return false);
    CheckInt32Index(TrackGroupIndex, Animation->TrackGroupCount, return false);

    model_control_binding *Sentinel = &GetControlBindingSentinel(Control);
    {for(model_control_binding *Iterator = Sentinel->ControlNext;
         Iterator != Sentinel;
         Iterator = Iterator->ControlNext)
    {
        // This is the binding to examine.  Note that poses obviously don't have
        // trackgroups, so always return null
        Assert(Iterator->Callbacks->GetSPUControlledAnimation);
        spu_controlled_animation* SPUControlledAnim = Iterator->Callbacks->GetSPUControlledAnimation ( *Iterator );
        if (SPUControlledAnim)
        {
            spu_animation_binding* Binding = SPUControlledAnim->Binding;

            // We are going to assume that (Binding->ID.SourceTrackGroupIndex == TrackGroupIndex)
            // is true if the animation matches.
            if (Binding && Binding->ID.Animation == Animation)
            {
                SPUControlledAnim->TrackMask = TrackMask;
                return true;
            }
        }
    }}

    return false;
}

track_mask const* GRANNY
GetSPUControlTrackGroupTrackMask(control const&   Control,
                                 spu_animation const* Animation,
                                 int32x TrackGroupIndex)
{
    CheckPointerNotNull(Animation, return NULL);
    CheckInt32Index(TrackGroupIndex, Animation->TrackGroupCount, return NULL);

    model_control_binding *Sentinel = &GetControlBindingSentinel(Control);
    {for(model_control_binding *Iterator = Sentinel->ControlNext;
         Iterator != Sentinel;
         Iterator = Iterator->ControlNext)
    {
        // This is the binding to examine.  Note that poses obviously don't have
        // trackgroups, so always return null
        Assert(Iterator->Callbacks->GetSPUControlledAnimation);
        spu_controlled_animation* SPUControlledAnim = Iterator->Callbacks->GetSPUControlledAnimation ( *Iterator );
        if (SPUControlledAnim)
        {
            spu_animation_binding* Binding = SPUControlledAnim->Binding;

            // We are going to assume that (Binding->ID.SourceTrackGroupIndex == TrackGroupIndex)
            // is true if the animation matches.
            if (Binding && Binding->ID.Animation == Animation)
            {
                return SPUControlledAnim->TrackMask;
            }
        }
    }}

    return NULL;
}


void GRANNY
SetControlWeight(control &Control, real32 Weight)
{
    real32 OldWeight = Control.CurrentWeight;
    Control.CurrentWeight = Weight;

    if(((OldWeight == 0.0f) && (Control.CurrentWeight != 0.0f)) ||
       ((OldWeight != 0.0f) && (Control.CurrentWeight == 0.0f)))
    {
        SortBindings(Control);
    }
}

int32x GRANNY
GetControlLoopCount(control const &Control)
{
    return(Control.LoopCount);
}

void GRANNY
SetControlLoopCount(control &Control, int32x LoopCount)
{
    Control.LoopCount = LoopCount;
}

int32x GRANNY
GetControlLoopIndex(control &Control)
{
    // This has to be done here, to ensure that the loop index
    // has been updated
    GetControlRawLocalClock(Control);

    return(Control.LoopIndex);
}

void GRANNY
SetControlLoopIndex(control &Control, int32x LoopIndex)
{
    Control.LoopIndex = LoopIndex;
}

real32 GRANNY
GetControlSpeed(control const &Control)
{
    return Control.Speed;
}

static real32
AdjustClock(real32 const CurrentClock,
            real32 const ClockToAdjust,
            real32 const OldSpeed,
            real32 const NewSpeed)
{
    // Need to adjust the kill clock based on the current clock and speed.
    real32 const BaseDelta     = ClockToAdjust - CurrentClock;
    real32 const AbsoluteDelta = BaseDelta * OldSpeed;

    real32 NewDelta;
    if (NewSpeed == 0)
    {
        // There's really no good way to deal with this...
        Log0(WarningLogMessage, ControlLogMessage, "Control speed of 0, cannot properly recompute local clock!\n");
        NewDelta = AbsoluteDelta;
    }
    else
    {
        NewDelta = AbsoluteDelta / NewSpeed;
    }

    return CurrentClock + NewDelta;
}

void GRANNY
SetControlSpeed(control &Control, real32 Speed)
{
    if (GetFlags(Control, (control::KillOnceCompleteFlag | control::KillClockIsLocal)))
    {
        Control.KillClock = AdjustClock(Control.CurrentClock, Control.KillClock, Control.Speed, Speed);
    }

    if (GetFlags(Control, control::EaseClockIsLocal))
    {
        if (GetFlags(Control, control::EaseInFlag))
        {
            Control.EaseInStartClock = AdjustClock(Control.CurrentClock, Control.EaseInStartClock, Control.Speed, Speed);
            Control.EaseInEndClock   = AdjustClock(Control.CurrentClock, Control.EaseInEndClock,   Control.Speed, Speed);
        }

        if (GetFlags(Control, control::EaseOutFlag))
        {
            Control.EaseOutStartClock = AdjustClock(Control.CurrentClock, Control.EaseOutStartClock, Control.Speed, Speed);
            Control.EaseOutEndClock   = AdjustClock(Control.CurrentClock, Control.EaseOutEndClock,   Control.Speed, Speed);
        }
    }

    Control.Speed = Speed;
}

real32 GRANNY
GetControlDuration(control const &Control)
{
    real32 Duration = GetReal32VeryLarge();

    if (Control.LoopCount == 0)
    {
        // Loops forever.
        Duration = GetReal32VeryLarge();
    }
    else if ( Control.Speed > 0.0f )
    {
        Duration = (Control.LocalDuration / Control.Speed) *
            (real32)Control.LoopCount;
    }
    else if ( Control.Speed < 0.0f )
    {
        Duration = (Control.LocalDuration / -Control.Speed) *
            (real32)Control.LoopCount;
    }
    else
    {
        // Speed is 0.0
        Duration = GetReal32VeryLarge();
    }

    return(Duration);
}

real32 GRANNY
GetControlDurationLeft(control &Control)
{
    real32 Duration = GetReal32VeryLarge();

    // NOTE: This has to be done here, to ensure that the loop index
    // and such are set properly
    real32 LocalClock = GetControlRawLocalClock(Control);

    if(Control.LoopCount == 0)
    {
        // Loops forever.
        Duration = GetReal32VeryLarge();
    }
    else if(Control.Speed > 0.0f )
    {
        // Going forwards.
        if(Control.LoopIndex >= Control.LoopCount)
        {
            Duration = 0.0f;
        }
        else
        {
            Duration =
                // The remainder of this loop:
                ((Control.LocalDuration - LocalClock) / Control.Speed) +
                // The rest of the loops:
                ((Control.LocalDuration / Control.Speed) *
                (real32)(Control.LoopCount - Control.LoopIndex - 1));
        }
    }
    else if(Control.Speed < 0.0f )
    {
        // Going backwards.
        if(Control.LoopIndex < 0)
        {
            Duration = 0.0f;
        }
        else
        {
            Duration =
                // The remainder of this loop:
                (LocalClock / -Control.Speed) +
                // The rest of the loops:
                ((Control.LocalDuration / -Control.Speed) *
                (real32)(Control.LoopIndex));
        }
    }
    else
    {
        // Control speed is zero, so the animation is going to last forever.
        Duration = GetReal32VeryLarge();
    }

    return(Duration);
}

real32 GRANNY
GetControlCycleDuration(control const &Control)
{
    if (Control.Speed != 0)
        return (Control.LocalDuration / AbsoluteValue(Control.Speed));
    else
        return GetReal32VeryLarge();
}


void GRANNY
SetControlActive(control &Control, bool Active)
{
    if(GetFlags(Control, control::ActiveFlag) != Active)
    {
        SetFlags(Control, control::ActiveFlag, Active);
        SortBindings(Control);
    }
}

void GRANNY
SetControlEaseIn(control &Control, bool EaseIn)
{
    SetFlags(Control, control::EaseInFlag, EaseIn);
}

void GRANNY
SetControlEaseInCurve(control &Control,
                      real32 StartSeconds, real32 EndSeconds,
                      real32 StartValue, real32 StartTangent,
                      real32 EndTangent, real32 EndValue)
{
    Control.EaseInStartClock = StartSeconds;
    Control.EaseInEndClock = EndSeconds;

    PackEaseCurve(Control.EaseInValues,
                  StartValue,
                  StartTangent,
                  EndTangent,
                  EndValue);
}

void GRANNY
SetControlEaseOut(control &Control, bool EaseOut)
{
    SetFlags(Control, control::EaseOutFlag, EaseOut);
}

void GRANNY
SetControlEaseOutCurve(control &Control,
                       real32 StartSeconds, real32 EndSeconds,
                       real32 StartValue, real32 StartTangent,
                       real32 EndTangent, real32 EndValue)
{
    Control.EaseOutStartClock = StartSeconds;
    Control.EaseOutEndClock   = EndSeconds;

    PackEaseCurve(Control.EaseOutValues,
                  StartValue,
                  StartTangent,
                  EndTangent,
                  EndValue);
}

bool GRANNY
GetControlLocalEase(control const &Control)
{
    return GetFlags(Control, control::EaseClockIsLocal);
}

void GRANNY
SetControlLocalEase(control& Control, bool EaseIsLocal)
{
    // Note that this reifies the current control speed.  Changes *after* this call will
    // adjust the ease clocks...
    SetFlags(Control, control::EaseClockIsLocal, EaseIsLocal);
}


void GRANNY
SetControlRawLocalClock(control &Control, real32 LocalClock)
{
    Control.dTLocalClockPending = 0.0f;
    Control.LocalClock = LocalClock;
}

real32 GRANNY
EaseControlIn(control &Control, real32 Duration, bool FromCurrent)
{
    real32 const CurrentClock = GetControlClock(Control);
    real32 Multiplier = 0.0f;
    if(FromCurrent)
    {
        Multiplier = GetControlEaseCurveMultiplier(Control);
    }

    SetControlEaseIn(Control, true);
    SetControlEaseOut(Control, false);

    SetControlEaseInCurve(Control, CurrentClock, CurrentClock + Duration,
                          Multiplier, Multiplier, 1.0f, 1.0f);

    return(CurrentClock + Duration);
}

// Note: this is not consistent with the API for EaseIn.  Should really have a
// "FromCurrent", but not in the current API version, introducing that change would be
// somewhat disruptive.
real32 GRANNY
EaseControlOut(control &Control, real32 Duration)
{
    real32 const CurrentClock = GetControlClock(Control);
    real32 const Multiplier = GetControlEaseCurveMultiplier(Control);

    SetControlEaseIn(Control, false);
    SetControlEaseOut(Control, true);

    SetControlEaseOutCurve(Control, CurrentClock, CurrentClock + Duration,
                           Multiplier, Multiplier, 0.0f, 0.0f);

    return (CurrentClock + Duration);
}

void **GRANNY
GetControlUserDataArray(control const &Control)
{
    return((void **)Control.UserData);
}

control *GRANNY
GetGlobalControlsBegin(void)
{
    return(GetGlobalControlSentinel().Next);
}

control *GRANNY
GetGlobalControlsEnd(void)
{
    return(&GetGlobalControlSentinel());
}

control *GRANNY
GetGlobalNextControl(control *Control)
{
    return(Control ? Control->Next : 0);
}

void GRANNY
RecenterControlClocks(control *Control, real32 dCurrentClock)
{
    Control->CurrentClock += dCurrentClock;
    Control->KillClock += dCurrentClock;

    Control->EaseInStartClock += dCurrentClock;
    Control->EaseInEndClock += dCurrentClock;
    Control->EaseOutStartClock += dCurrentClock;
    Control->EaseOutEndClock += dCurrentClock;
}

void GRANNY
RecenterAllControlClocks(real32 dCurrentClock)
{
    {for(control *Control = GetGlobalControlSentinel().Next;
         Control != &GetGlobalControlSentinel();
         Control = Control->Next)
    {
        RecenterControlClocks ( Control, dCurrentClock );
    }}
}

void GRANNY
RecenterAllModelInstanceControlClocks(model_instance *ModelInstance, real32 dCurrentClock)
{
    // WARNING - this isn't 100% safe because a control can be controlling
    // multiple model_instances. However, very few people use this functionality,
    // so in most cases it is safe.
    model_control_binding *BindingEnd = &(ModelControlsEnd(*ModelInstance));
    for(model_control_binding *Binding = &(ModelControlsBegin(*ModelInstance));
        Binding != BindingEnd;
        Binding = &(ModelControlsNext(*Binding)))
    {
        RecenterControlClocks ( GetControlFromBinding(*Binding), dCurrentClock );
    }
}


bool GRANNY
GetControlForceClampedLooping(control &Control)
{
    return GetFlags(Control, control::ForceClampedLoopsFlag);
}

void GRANNY
SetControlForceClampedLooping(control &Control, bool Clamp)
{
    SetFlags(Control, control::ForceClampedLoopsFlag, Clamp);
}


void GRANNY
SetControlTargetState(control &Control, real32 CurrentGlobalTime, real32 TargetGlobalTime, real32 LocalTime, int32x LoopIndex)
{
    Control.dTLocalClockPending = 0.0f;
    Control.CurrentClock = CurrentGlobalTime;

    real32 DeltaGlobalTime = TargetGlobalTime - CurrentGlobalTime;

    Control.LoopIndex = LoopIndex;
    Control.LocalClock = LocalTime - DeltaGlobalTime * Control.Speed;

    // And move the local clock back into the right ranges by adjusting the loop index appropriately.
    ModulusLocalClock ( Control );
}

#endif

// =============================================================================
// Functions after this point are used on the SPU
// =============================================================================

inline void
UnpackEaseCurve(uint32 Value, real32 *Result)
{
    Result[0] = (real32)((Value >>  0) & 0xFF) * (1.0f / 255.0f);
    Result[1] = (real32)((Value >>  8) & 0xFF) * (1.0f / 255.0f);
    Result[2] = (real32)((Value >> 16) & 0xFF) * (1.0f / 255.0f);
    Result[3] = (real32)((Value >> 24) & 0xFF) * (1.0f / 255.0f);
}

static real32
ComputeEaseCurve(real32 StartClock, real32 CurrentClock, real32 EndClock,
                 real32 *Bezier)
{
    real32 Range = EndClock - StartClock;
    real32 RelativeClock = CurrentClock - StartClock;

    real32 t;
    if(Range != 0.0f)
    {
        t = (RelativeClock / Range);
        if(t < 0.0f)
        {
            t = 0.0f;
        }
        else if(t > 1.0f)
        {
            t = 1.0f;
        }
    }
    else
    {
        Assert(StartClock == EndClock);
        t = (CurrentClock < StartClock) ? 0.0f : 1.0f;
    }

    real32 const u = (1.0f - t);

    // This is just a simple bezier curve.
    return(1*u*u*u*Bezier[0] +
           3*u*u*t*Bezier[1] +
           3*u*t*t*Bezier[2] +
           1*t*t*t*Bezier[3]);
}

real32 GRANNY
GetControlRawLocalClock(control &Control)
{
    if(Control.dTLocalClockPending != 0.0f)
    {
        Control.LocalClock += Control.dTLocalClockPending * Control.Speed;

        if(Control.LocalClock < 0)
        {
            if((Control.LoopCount == 0) ||
               (Control.LoopIndex > 0))
            {
                ModulusLocalClock(Control);
            }
        }
        else if(Control.LocalClock >= Control.LocalDuration)
        {
            if((Control.LoopCount == 0) ||
               (Control.LoopIndex < (Control.LoopCount - 1)))
            {
                ModulusLocalClock(Control);
            }
        }

        Control.dTLocalClockPending = 0.0f;
    }

    // TODO: do we need to always call ModulusLocalClock
    // - what if they set the loop index to be something strange, or the
    // local time out of bounds or something?
    CheckLocalClockAndLoopIndex ( Control );

    return(Control.LocalClock);
}

real32 GRANNY
GetControlEaseCurveMultiplier(control const &Control)
{
    real32 EaseInValues[4];
    real32 EaseOutValues[4];
    UnpackEaseCurve(Control.EaseInValues, EaseInValues);
    UnpackEaseCurve(Control.EaseOutValues, EaseOutValues);

    real32 Weight = 1.0f;


    if((Control.EaseInStartClock < Control.EaseOutStartClock) ||
       !(GetFlags(Control, control::EaseOutFlag) &&
         GetFlags(Control, control::EaseInFlag)))
    {
        // This is the ease-in, ease-out scenario
        if(GetFlags(Control, control::EaseOutFlag))
        {
            if(Control.CurrentClock >= Control.EaseOutStartClock)
            {
                Weight = ComputeEaseCurve(Control.EaseOutStartClock,
                                          Control.CurrentClock,
                                          Control.EaseOutEndClock,
                                          EaseOutValues);
            }
        }

        if(GetFlags(Control, control::EaseInFlag))
        {
            if(Control.CurrentClock <= Control.EaseInEndClock)
            {
                Weight = ComputeEaseCurve(Control.EaseInStartClock,
                                          Control.CurrentClock,
                                          Control.EaseInEndClock,
                                          EaseInValues);
            }
        }
    }
    else
    {
        // This is the ease-out, ease-back-in scenario
        if(Control.CurrentClock <= Control.EaseOutEndClock)
        {
            Weight = ComputeEaseCurve(Control.EaseOutStartClock,
                                      Control.CurrentClock,
                                      Control.EaseOutEndClock,
                                      EaseOutValues);
        }
        else if(Control.CurrentClock <= Control.EaseInStartClock)
        {
            real32 t = ((Control.CurrentClock - Control.EaseOutEndClock) /
                        (Control.EaseInStartClock - Control.EaseOutEndClock));
            Weight = ((1.0f - t)*EaseOutValues[3] +
                      t*EaseInValues[0]);
        }
        else if(Control.CurrentClock <= Control.EaseInEndClock)
        {
            Weight = ComputeEaseCurve(Control.EaseInStartClock,
                                      Control.CurrentClock,
                                      Control.EaseInEndClock,
                                      EaseInValues);
        }
    }

    return(Weight);
}

void GRANNY
GetControlLoopState(control &Control,
                    bool32& UnderflowLoop, bool32& OverflowLoop)
{
    // This has to be done here, to ensure that the loop index
    // has been updated
    GetControlRawLocalClock(Control);

    GetControlLoopStateInline(Control, UnderflowLoop, OverflowLoop);
}

bool GRANNY
ControlHasEffect(control &Control)
{
    return(GetFlags(Control, control::ActiveFlag) &&
           (Control.CurrentWeight != 0.0f));
}

bool GRANNY
ControlIsActive(control const &Control)
{
    return(GetFlags(Control, control::ActiveFlag));
}

real32 GRANNY
GetControlLocalDuration(control const &Control)
{
    return(GetControlLocalDurationInline(Control));
}

real32 GRANNY
GetControlClampedLocalClock(control &Control)
{
    return(GetControlClampedLocalClockInline(Control));
}

real32 GRANNY
GetControlEffectiveWeight(control const &Control)
{
    if(ControlIsActive(Control))
    {
        return(GetControlEaseCurveMultiplier(Control) *
               Control.CurrentWeight);
    }
    else
    {
        return(0);
    }
}

BEGIN_GRANNY_NAMESPACE;
void ControlComplexStats()
{
#define PRE "Granny/Controls/"
    real32 MinClock = 0;
    real32 MaxClock = 0;
    real32 MinCompletionClock = 0;
    real32 MaxCompletionClock = 0;
    if(GetGlobalControlsBegin() != GetGlobalControlsEnd())
    {
        MinClock           = Real32Maximum;
        MaxClock           = -Real32Maximum;
        MinCompletionClock = Real32Maximum;
        MaxCompletionClock = -Real32Maximum;
    }    

    int32x TotalControlCount = 0;
    int32x ActiveControlCount = 0;
    int32x ActiveButEasedOutControlCount = 0;
    int32x UnusedControlCount = 0;
    int32x CompletedControlCount = 0;
    int32x CompletableControlCount = 0;

    {for(control *Control = GetGlobalControlsBegin();
         Control != GetGlobalControlsEnd();
         Control = GetGlobalNextControl(Control))
    {
        ++TotalControlCount;
        if (ControlIsActive(*Control))
        {
            ++ActiveControlCount;
            if (ControlHasEffect(*Control))
            {
                if(GetControlEffectiveWeight(*Control) == 0.0f)
                    ++ActiveButEasedOutControlCount;
            }
        }

        real32 const ControlClock = GetControlClock(*Control);
        MinClock = Minimum(ControlClock, MinClock);
        MaxClock = Maximum(ControlClock, MaxClock);

        if (GetControlCompletionCheckFlag(*Control))
        {
            real32 const CompletionClock = GetControlCompletionClock(*Control);
            MinCompletionClock = Minimum(CompletionClock, MinCompletionClock);
            MaxCompletionClock = Maximum(CompletionClock, MaxCompletionClock);

            ++CompletableControlCount;
        }

        if(ControlIsComplete(*Control))
        {
            ++CompletedControlCount;
        }

        if(ControlIsUnused(*Control))
        {
            ++UnusedControlCount;
        }
    }}

    GRANNY_EMIT_INT_VALUE(PRE "Counts/TotalControls(ccount)", TotalControlCount);
    GRANNY_EMIT_INT_VALUE(PRE "Counts/ActiveControls(ccount)", ActiveControlCount);
    GRANNY_EMIT_INT_VALUE(PRE "Counts/ActiveButEasedOutControls(ccount)", ActiveButEasedOutControlCount);
    GRANNY_EMIT_INT_VALUE(PRE "Counts/UnusedControls(ccount)", UnusedControlCount);
    GRANNY_EMIT_INT_VALUE(PRE "Counts/CompletedControls(ccount)", CompletedControlCount);
    GRANNY_EMIT_INT_VALUE(PRE "Counts/CompletableControls(ccount)", CompletableControlCount);

    GRANNY_EMIT_FLOAT(PRE "Time/Min(ctime)", MinClock);
    GRANNY_EMIT_FLOAT(PRE "Time/Max(ctime)", MaxClock);
    if (CompletableControlCount != 0)
    {
        GRANNY_EMIT_FLOAT(PRE "Time/MinCompletion(ctime)", MinCompletionClock);
        GRANNY_EMIT_FLOAT(PRE "Time/MaxCompletion(ctime)", MaxCompletionClock);
    }
}
END_GRANNY_NAMESPACE;


// stat_hud *GRANNY
// CaptureCurrentStats(int32x FramesSinceLastCapture)
// {
//     memory_arena *Arena = NewMemoryArena();

//     {for(control *Control = GetGlobalControlsBegin();
//          Control != GetGlobalControlsEnd();
//          Control = GetGlobalNextControl(Control))
//     {
//         ++Controls.TotalControlCount;
//         if(ControlIsActive(*Control))
//         {
//             ++Controls.ActiveControlCount;
//             if(ControlHasEffect(*Control))
//             {
//                 ++Controls.ActiveAndWeightedControlCount;
//                 if(GetControlEffectiveWeight(*Control) == 0.0f)
//                 {
//                     ++Controls.ActiveAndWeightedButEasedOutControlCount;
//                 }
//             }
//         }

//         real32 ControlClock = GetControlClock(*Control);
//         Controls.MinClockTime = Minimum(ControlClock, Controls.MinClockTime);
//         Controls.MaxClockTime = Maximum(ControlClock, Controls.MaxClockTime);

//         if(GetControlCompletionCheckFlag(*Control))
//         {
//             real32 CompletionTime = GetControlCompletionClock(*Control);
//             Controls.MinCompletionTime =
//                 Minimum(CompletionTime, Controls.MinCompletionTime);
//             Controls.MaxCompletionTime =
//                 Maximum(CompletionTime, Controls.MaxCompletionTime);

//             ++Controls.CompletableControlCount;
//         }

//         if(ControlIsComplete(*Control))
//         {
//             ++Controls.CompletedControlCount;
//         }

//         if(ControlIsUnused(*Control))
//         {
//             ++Controls.UnusedControlCount;
//         }
//     }}

//     stat_hud_animation_types AnimTypes = {0};
//     {for(animation_binding *Binding = GetFirstAnimationBinding();
//          Binding;
//          Binding = GetNextAnimationBinding(Binding))
//     {
//         bool Rebased = (Binding->Retargeter != 0);

//         {for(int32x TrackIndex = 0;
//              TrackIndex < Binding->TrackBindingCount;
//              ++TrackIndex)
//         {
//             bound_transform_track &Track = Binding->TrackBindings[TrackIndex];
//             if(Track.SourceTrack)
//             {
//                 transform_track const &SourceTrack = *Track.SourceTrack;

//                 ++AnimTypes.TrackTypes
//                     [CurveKnotCharacter(SourceTrack.PositionCurve)]
//                     [CurveKnotCharacter(SourceTrack.OrientationCurve)]
//                     [CurveKnotCharacter(SourceTrack.ScaleShearCurve)]
//                     [Rebased];
//                 ++AnimTypes.TotalTrackCount;
//             }
//         }}
//     }}

//     //
//     // Gathering
//     //
//     aggr_allocator Allocator;
//     InitializeAggrAlloc(Allocator);

//     stat_hud *HUD;
//     AggrAllocPtr(Allocator, HUD);
//     if(EndAggrAlloc(Allocator, AllocationUnknown))
//     {
//         HUD->FrameCount = FramesSinceLastCapture;
//         HUD->Controls = Controls;
//         HUD->AnimTypes = AnimTypes;
//     }

//     //
//     // Cleanup
//     //
//     FreeMemoryArena(Arena);

//     return(HUD);
// }

// void GRANNY
// FreeStats(stat_hud *StatHUD)
// {
//     Deallocate(StatHUD);
// }

// char **GRANNY
// DumpStatHUD(stat_hud &StatHUD)
// {
//     int32x LineCount = 256;
//     int32x LineLength = 256;

//     char **StatHUDDump;
//     char *CharArray;

//     aggr_allocator Allocator;
//     InitializeAggrAlloc(Allocator);

//     AggrAllocArrayPtr(Allocator, LineCount, StatHUDDump);
//     AggrAllocArrayPtr(Allocator, LineCount*LineLength, CharArray);
//     if(EndAggrAlloc(Allocator, AllocationUnknown))
//     {
//         {for(int32x LineIndex = 0;
//              LineIndex < LineCount;
//              ++LineIndex)
//         {
//             StatHUDDump[LineIndex] = CharArray;
//             CharArray += LineLength;
//         }}

//         char **Line = StatHUDDump;

//         //
//         // Animations
//         //
//         stat_hud_model_controls &Controls = StatHUD.Controls;
//         ConvertToStringVar(LineLength, *Line++,
//                            "Controls:");
//         ConvertToStringVar(LineLength, *Line++,
//                            "  Total controls: %d   Active: %d   A/Weighted: %d",
//                            Controls.TotalControlCount,
//                            Controls.ActiveControlCount,
//                            Controls.ActiveAndWeightedControlCount);
//         ConvertToStringVar(LineLength, *Line++,
//                            "  Completable: %d   Completed: %d   Unused: %d",
//                            Controls.CompletableControlCount,
//                            Controls.CompletedControlCount,
//                            Controls.UnusedControlCount);
//         ConvertToStringVar(LineLength, *Line++,
//                            "  Clock range: %fs - %fs",
//                            Controls.MinClockTime,
//                            Controls.MaxClockTime);
//         if(Controls.MinCompletionTime == Controls.MaxCompletionTime)
//         {
//             ConvertToStringVar(LineLength, *Line++,
//                                "  Completion range: (none)");
//         }
//         else if(Controls.MaxCompletionTime == Real32Maximum)
//         {
//             ConvertToStringVar(LineLength, *Line++,
//                                "  Completion range: %fs - Infinity",
//                                Controls.MinCompletionTime);
//         }
//         else
//         {
//             ConvertToStringVar(LineLength, *Line++,
//                                "  Completion range: %fs - %fs",
//                                Controls.MinCompletionTime,
//                                Controls.MaxCompletionTime);
//         }

//         stat_hud_animation_types &AnimTypes = StatHUD.AnimTypes;
//         {for(int32x P = 0;
//              P < 3;
//              ++P)
//         {
//             {for(int32x R = 0;
//                  R < 3;
//                  ++R)
//             {
//                 {for(int32x S = 0;
//                      S < 3;
//                      ++S)
//                 {
//                     int32x Count = AnimTypes.TrackTypes[P][R][S][0];
//                     int32x RebasedCount = AnimTypes.TrackTypes[P][R][S][1];
//                     int32x TotalCount = Count + RebasedCount;
//                     if(TotalCount)
//                     {
//                         char Letter[] = {'I', 'C', 'A'};
//                         ConvertToStringVar(
//                             LineLength, *Line++,
//                             "  (%d%%) %c%c%c bindings: %4d + %4dr = %4d",
//                             (100 * TotalCount) / AnimTypes.TotalTrackCount,
//                             Letter[P],
//                             Letter[R],
//                             Letter[S],
//                             Count,
//                             RebasedCount,
//                             TotalCount);
//                     }
//                 }}
//             }}
//         }}

//         *Line = 0;
//     }

//     return(StatHUDDump);
// }

