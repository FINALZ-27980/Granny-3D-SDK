// ========================================================================
// $File: //jeffr/granny_29/rt/granny_telemetry.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_telemetry.h"

#include "granny_assert.h"
#include "granny_parameter_checking.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode Undefined_LogMessage
USING_GRANNY_NAMESPACE;

#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY

extern "C" { void* global_Granny_TmContext; }

#endif

#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY
static bool
VersionCheck()
{
    if (global_Granny_TmContext &&
        tmCheckVersion((HTELEMETRY)global_Granny_TmContext,
                       TelemetryMajorVersion,
                       TelemetryMinorVersion,
                       TelemetryBuildNumber,
                       TelemetryCustomization ) != TM_OK)
    {
        Log0(ErrorLogMessage, TelemetryLogMessage, "Mismatched Granny/Telemetry versions.");
        global_Granny_TmContext = 0;

        return false;
    }

    return true;
}
#endif



bool GRANNY
UseTmLite(void* Context)
{
#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY
#if GRANNY_REAL_TELEMETRY
    InvalidCodePath("You should be using real telemetry!");
#else
    global_Granny_TmContext = TM_CONTEXT_LITE( Context );
#endif

    return VersionCheck();
    #else
        return false;

#endif
}

bool GRANNY
UseTelemetry(void* Context)
{
#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY

#if GRANNY_REAL_TELEMETRY
    global_Granny_TmContext = Context;
#else
    global_Granny_TmContext = TM_CONTEXT_FULL( Context );
#endif

    return VersionCheck();
#else
    return false;
#endif
}


// We do it this way to avoid bringing in the headers for all of these
#define DeclStats(Name, Type) extern void Name ## Type ## Stats()
#define InvokeStats(Name, Type) Name ## Type ## Stats()

BEGIN_GRANNY_NAMESPACE;

DeclStats(AnimationBindingCache, Frame);
DeclStats(ControlledAnimation, Frame);
DeclStats(LocalPose, Frame);
DeclStats(ModelInstance, Frame);
DeclStats(PointerHash, Frame);
DeclStats(Control, Complex);

END_GRANNY_NAMESPACE;

void GRANNY
TelemetryFrameStats()
{
#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY
    InvokeStats(AnimationBindingCache, Frame);
    InvokeStats(ControlledAnimation, Frame);
    InvokeStats(LocalPose, Frame);
    InvokeStats(ModelInstance, Frame);
    InvokeStats(PointerHash, Frame);
#endif
}

void GRANNY
TelemetryComplexStats()
{
#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY
    InvokeStats(Control, Complex);
#endif
}

