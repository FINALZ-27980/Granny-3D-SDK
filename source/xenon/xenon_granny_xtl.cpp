// ========================================================================
// $File: //jeffr/granny_29/rt/xenon/xenon_granny_xtl.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "xenon_granny_xtl.h"
#include "granny_log.h"
#include "granny_assert.h"

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
Win32LogLastError_(bool IsError,
                   char const *SourceFile, int32x SourceLineNumber,
                   char const *FailedWin32Function)
{
    DWORD LastError = GetLastError();
    if(LastError != NO_ERROR)
    {
        Log4(IsError ? ErrorLogMessage : WarningLogMessage,
             Win32SubsystemLogMessage,
             "%s(%d) : "
             "%s failed with error %d",
             SourceFile, SourceLineNumber,
             FailedWin32Function, LastError);
    }
}

int32x GRANNY
Win32Seek(HANDLE Win32FileHandle, int32x Offset, DWORD MoveMethod)
{
    int32x Result = SetFilePointer(Win32FileHandle, Offset, 0, MoveMethod);
    if((Result == INVALID_SET_FILE_POINTER) &&
       (GetLastError() != 0))
    {
        Win32LogErrorAsWarning(SetFilePointer);
    }

    return(Result);
}
