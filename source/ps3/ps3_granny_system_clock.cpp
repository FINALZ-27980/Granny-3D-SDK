// ========================================================================
// $File: //jeffr/granny_29/rt/ps3/ps3_granny_system_clock.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "ps3_granny_std.h"
#include "granny_system_clock.h"
#include "granny_assert.h"
#include "granny_memory.h"
#include "granny_parameter_checking.h"

#define SubsystemCode PS3SubsystemLogMessage

#include <sys/sys_time.h>
#include <sys/time_util.h>

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
    // NOP on ps3
}


void GRANNY
GetSystemSeconds(system_clock* Result)
{
    CheckPointerNotNull(Result, return);
    CompileAssert(sizeof(system_clock) >= sizeof(uint64_t));

    // SYS_TIMEBASE_GET is a macro that inserts inline asm,
    //  so I'm going to be a bit paranoid about this here,
    //  rather than passing in *Result.
    uint64_t cur_tb;
    SYS_TIMEBASE_GET(cur_tb);

    memcpy(Result, &cur_tb, sizeof(uint64_t));
}

real32 GRANNY
GetSecondsElapsed(system_clock const &StartClock,
                  system_clock const &EndClock)
{
    // Going to be paranoid about alignment here...
    uint64_t start_tb, end_tb;
    memcpy(&start_tb, &StartClock, sizeof(uint64_t));
    memcpy(&end_tb,   &EndClock,   sizeof(uint64_t));

    return (double(end_tb - start_tb) /
            sys_time_get_timebase_frequency());
}

