// ========================================================================
// $File: //jeffr/granny_29/rt/winxx/winxx_granny_system_clock.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_system_clock.h"
#include "granny_assert.h"
#include "granny_memory.h"
#include "winxx_granny_windows.h"
#include "granny_conversions.h"
#include "granny_parameter_checking.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode WinXXSubsystemLogMessage

USING_GRANNY_NAMESPACE;


static uint32 LastTickCount;
static int64  LastMicroseconds;
static int64  CurrentAdjustment = 0;

static int64  QPC_InitialVal;
static int64  QPC_Frequency = 0;

static void
CheckTimerInit()
{
    if(QPC_Frequency == 0)
    {
        // Init our timer variables
        QueryPerformanceFrequency((LARGE_INTEGER*)&QPC_Frequency);
        QueryPerformanceCounter((LARGE_INTEGER*)&QPC_InitialVal);
        LastMicroseconds = 0;
        LastTickCount    = GetTickCount();
    }
}

void GRANNY
RequeryTimerFrequency()
{
    // Make sure all of the values are up to date.
    CheckTimerInit();

    int64 OldFrequency = QPC_Frequency;
    int64 CurrentVal;
    QueryPerformanceFrequency((LARGE_INTEGER*)&QPC_Frequency);
    QueryPerformanceCounter((LARGE_INTEGER*)&CurrentVal);

    // We need to reset the InitialVal such that it represents the
    // same distance from the current value that it used to with the
    // old clock frequency.
    real64x Time = real64x(CurrentVal - QPC_InitialVal) / real64x(OldFrequency);
    real64x NewInitial = real64x(CurrentVal) - Time * QPC_Frequency;
    real64x Difference = NewInitial - QPC_InitialVal;

    CurrentAdjustment += RoundReal64ToInt64((Difference * 1000000) / real64x(QPC_Frequency));
}


void GRANNY
GetSystemSeconds(system_clock* Result)
{
    CheckPointerNotNull(Result, return);
    CompileAssert(SizeOf(int64) <= SizeOf(system_clock));

    // Ensure the variable are setup...
    CheckTimerInit();

    // Read the new times.  Note that we're doing some math in
    //  real64x here to prevent VC from trying to link in 64-bit
    //  math routines from the CRT.
    int64 count;
    CompileAssert(sizeof(count) == sizeof(LARGE_INTEGER));
    QueryPerformanceCounter((LARGE_INTEGER*)&count);
    uint32 TickCount = GetTickCount();
    int64x CurrMicroseconds  = (CurrentAdjustment +
                                RoundReal64ToInt64((real64x(count - QPC_InitialVal) * 1000000) /
                                                   real64x(QPC_Frequency)));

    // see how much each has moved
    int64x deltatime = RoundReal64ToInt64((TickCount - LastTickCount) * 1000.0);
    int64x deltarad  = CurrMicroseconds - LastMicroseconds;

    // check the query against GetTickCount to make sure it hasn't
    //   jumped ahead...

    const int64x JumpCheck = deltatime - deltarad;
    if ( JumpCheck > 200000 || JumpCheck < -200000 )
    {
        deltatime         -= deltarad;
        CurrentAdjustment += deltatime;
        CurrMicroseconds  += deltatime;
    }


    // now check to see if it went backwards?
    if ( CurrMicroseconds < LastMicroseconds )
    {
        // yup, just return the old timer value then
        *((int64*)Result) = LastMicroseconds;
    }
    else
    {
        LastTickCount     = TickCount;
        LastMicroseconds  = CurrMicroseconds;

        *((int64*)Result) = CurrMicroseconds;
    }
}

real32 GRANNY
GetSecondsElapsed(system_clock const &StartClock,
                  system_clock const &EndClock)
{
    int64* Start = (int64 *)&StartClock;
    int64* End   = (int64 *)&EndClock;

    if(*Start < *End)
    {
        real64x const Difference = (real64x)(*End - *Start);

        // Difference is in microseconds now...
        real32 const SecondsElapsed = (real32)(Difference / 1000000.0);
        return SecondsElapsed;
    }
    else
    {
        // TODO: Proper code for handling wrapping
        // (TomF thinks that actually the above code handles wrapping just fine)
        return(0);
    }
}
