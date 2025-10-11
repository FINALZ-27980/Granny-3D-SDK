// ========================================================================
// $File: //jeffr/granny_29/rt/granny_string_comparison_callback.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_string_comparison_callback.h"

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

USING_GRANNY_NAMESPACE;
BEGIN_GRANNY_NAMESPACE;

string_comparison_callback *StringComparisonCallback = 0;

END_GRANNY_NAMESPACE;

void GRANNY
SetStringComparisonCallback(string_comparison_callback* Callback)
{
    StringComparisonCallback = Callback;
}

