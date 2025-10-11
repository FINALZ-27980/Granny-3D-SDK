#if !defined(GRANNY_SYSTEM_CLOCK_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_system_clock.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(ClockGroup);

EXPTYPE_EPHEMERAL struct system_clock
{
    uint32 Data[4];
};

EXPAPI GS_MODIFY void GetSystemSeconds(system_clock* Result);
EXPAPI GS_READ real32 GetSecondsElapsed(system_clock const &StartClock,
                                        system_clock const &EndClock);

// Not yet exported pending tests...
void RequeryTimerFrequency();

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_SYSTEM_CLOCK_H
#endif
