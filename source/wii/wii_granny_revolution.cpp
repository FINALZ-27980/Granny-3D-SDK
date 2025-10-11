// ========================================================================
// $File: //jeffr/granny_29/rt/wii/wii_granny_revolution.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "wii_granny_revolution.h"
#include "granny_log.h"

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
WiiLogLastError(bool IsError,
                char const *SourceFile, int32x SourceLineNumber,
                char const *FailedWiiFunction)
{
    Log3(IsError ? ErrorLogMessage : WarningLogMessage,
         ANSISubsystemLogMessage,
         "%s(%d) : %s failed",
         SourceFile, SourceLineNumber,
         FailedWiiFunction);
}
