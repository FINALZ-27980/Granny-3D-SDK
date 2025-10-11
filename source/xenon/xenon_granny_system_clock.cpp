// ========================================================================
// $File: //jeffr/granny_29/rt/xenon/xenon_granny_system_clock.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "xenon_granny_xtl.h"
#include "granny_system_clock.h"
#include "granny_conversions.h"
#include "granny_memory.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

USING_GRANNY_NAMESPACE;

void GRANNY
RequeryTimerFrequency()
{
    // NOP on xenon
}


void GRANNY
GetSystemSeconds(system_clock* Result)
{
    Assert(Result);
#if 0
    Result->Data[0] = GetTickCount();
#else
    Assert(SizeOf(LARGE_INTEGER) <= SizeOf(system_clock));
    QueryPerformanceCounter((LARGE_INTEGER *)Result);
#endif
}

real32 GRANNY
GetSecondsElapsed(system_clock const &StartClock,
                  system_clock const &EndClock)
{
#if 0
    DWORD Start = StartClock.Data[0];
    DWORD End = EndClock.Data[0];
    if(Start < End)
    {
        return((real32)(End - Start) / 1000.0f);
    }
    else
    {
        // TODO: Proper code for handling wrapping
        return(0);
    }
#else
    static real64x TimerSecondsConversion = 0;
    if(TimerSecondsConversion == 0)
    {
        LARGE_INTEGER Frequency;
        QueryPerformanceFrequency(&Frequency);
        TimerSecondsConversion = 1.0 / (real64x)Frequency.QuadPart;
    }

    LARGE_INTEGER *Start = (LARGE_INTEGER *)&StartClock;
    LARGE_INTEGER *End = (LARGE_INTEGER *)&EndClock;

    if(Start->QuadPart < End->QuadPart)
    {
        real64x const Difference = (real64x)(End->QuadPart - Start->QuadPart);
        real32 const SecondsElapsed = (real32)(Difference * TimerSecondsConversion);
        return(SecondsElapsed);
    }
    else
    {
        // TODO: Proper code for handling wrapping
        return(0);
    }
#endif
}
