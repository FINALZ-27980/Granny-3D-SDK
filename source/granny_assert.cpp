// ========================================================================
// $File: //jeffr/granny_29/rt/granny_assert.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_assert.h"
#include "granny_parameter_checking.h"

#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode AssertionLogMessage

USING_GRANNY_NAMESPACE;


#if DEBUG

BEGIN_GRANNY_NAMESPACE;

assertion_callback AssertionCallback = { DisplayAssertion, 0 };

END_GRANNY_NAMESPACE;

void GRANNY
GetAssertionCallback(assertion_callback* Result)
{
    CheckPointerNotNull(Result, return);
    Result->Function = AssertionCallback.Function;
    Result->UserData = AssertionCallback.UserData;
}

void GRANNY
SetAssertionCallback(assertion_callback const &CallbackInit)
{
    AssertionCallback.Function = CallbackInit.Function;
    AssertionCallback.UserData = CallbackInit.UserData;
}

#else

BEGIN_GRANNY_NAMESPACE;

assertion_callback AssertionCallback = { 0, 0 };

END_GRANNY_NAMESPACE;

void GRANNY
GetAssertionCallback(assertion_callback* Result)
{
    CheckPointerNotNull(Result, return);
    Result->Function = 0;
    Result->UserData = 0;
}

void GRANNY
SetAssertionCallback(assertion_callback const &CallbackInit)
{
    // Nothing, asserts disabled.
}

#endif
