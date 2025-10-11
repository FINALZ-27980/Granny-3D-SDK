// ========================================================================
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_threads.h"

#include "granny_assert.h"
#include "granny_parameter_checking.h"

#include <windows.h>

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


void GRANNY
ThreadYieldToAny( )
{
    SwitchToThread();
}

