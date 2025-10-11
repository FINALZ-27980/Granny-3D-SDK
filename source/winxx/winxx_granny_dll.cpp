// ========================================================================
// $File: //jeffr/granny_29/rt/winxx/winxx_granny_dll.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "winxx_granny_dll.h"
#include "granny_assert.h"
#include "granny_limits.h"
#include "granny_memory.h"
#include "granny_string.h"
#include "granny_string_formatting.h"

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

bool GRANNY
DLLIsNotInWindowsPath(HANDLE DllHandle)
{
    bool Result = true;

    char DLLPath[MaximumSystemFileNameSize];
    UINT ResultLen = GetModuleFileNameA((HINSTANCE)DllHandle, DLLPath, SizeOf(DLLPath));
    if (ResultLen == 0 || ResultLen > MaximumSystemFileNameSize)
    {
        // Failed to get module name, trust.
        return true;
    }

    char *DLLFileName = DLLPath;
    {for(char *Scan = DLLFileName;
         *Scan;
         ++Scan)
    {
        if((*Scan == '\\') || (*Scan == '/'))
        {
            DLLFileName = Scan + 1;
        }
    }}

    char WindowsDirectory[MaximumSystemFileNameSize];
    ResultLen = GetWindowsDirectoryA(WindowsDirectory, SizeOf(WindowsDirectory));
    if (ResultLen == 0 || ResultLen > MaximumSystemFileNameSize)
    {
        // Failed to get windows dir name, trust.
        return true;
    }

    char SystemDirectory[MaximumSystemFileNameSize];
    ResultLen = GetSystemDirectoryA(SystemDirectory, SizeOf(SystemDirectory));

    if (ResultLen == 0 || ResultLen > MaximumSystemFileNameSize)
    {
        // Failed to get sys dir name, trust.
        return true;
    }

    if (StringBeginsWithLowercase(DLLPath, WindowsDirectory) ||
        StringBeginsWithLowercase(DLLPath, SystemDirectory))
    {
        static char ErrorMessage[MaximumMessageBufferSize];
        ConvertToStringVar(sizeof(ErrorMessage), ErrorMessage,
                           "%s has been incorrectly installed as "
                           "%s.  To avoid versioning problems, "
                           "%s must always be installed in an "
                           "application-specific directory, and never "
                           "in a system directory.",
                           DLLFileName, DLLPath, DLLFileName);

        MessageBoxA(0, ErrorMessage, "Fatal Granny Installation Error",
                    MB_OK | MB_ICONERROR);

        Result = false;
    }

    return Result;
}
