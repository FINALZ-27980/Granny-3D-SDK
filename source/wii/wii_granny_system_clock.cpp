// ========================================================================
// $File: //jeffr/granny_29/rt/wii/wii_granny_system_clock.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_system_clock.h"
#include "granny_memory.h"
#include "granny_assert.h"
#include "granny_parameter_checking.h"

#include <revolution/os.h>

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode WiiSubsystemLogMessage

USING_GRANNY_NAMESPACE;

void GRANNY
RequeryTimerFrequency()
{
    // NOP on wii
}

void GRANNY
GetSystemSeconds(system_clock* Result)
{
    CheckPointerNotNull(Result, return);
    CompileAssert(SizeOf(OSTime) <= SizeOf(system_clock));

    // TODO: Not implemented
    *(OSTime *)Result = OSGetTime();
}

real32 GRANNY
GetSecondsElapsed(system_clock const &StartClock,
                  system_clock const &EndClock)
{
    return((real32)(*(OSTime *)&EndClock - *(OSTime *)&StartClock) /
           (real32)OS_TIMER_CLOCK);
}
