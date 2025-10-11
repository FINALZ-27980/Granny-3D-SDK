// ========================================================================
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_threads.h"

#include "granny_assert.h"
#include "granny_parameter_checking.h"

#include <sys/timer.h>

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


//  Yield is sort of like the minimum length sleep that will trigger a thread switch
// some platforms have a no-sleep yield; use it if possible
void GRANNY
ThreadYieldToAny( )
{
    // android
    // linux
    // macosx
    sys_timer_usleep( 1 );
}
