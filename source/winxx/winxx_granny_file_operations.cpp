// ========================================================================
// $File: //jeffr/granny_29/rt/winxx/winxx_granny_file_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_file_operations.h"
#include "winxx_granny_windows.h"
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

static void
DeleteFileProxy(char const *FileName)
{
    DeleteFileA(FileName);
}

inline DWORD
GetFileSizeProxy(HANDLE WinXXFileHandle, DWORD *HighSize)
{
    return(GetFileSize(WinXXFileHandle, HighSize));
}

#undef DeleteFile
#undef GetFileSize

static bool
WinXXFindFile(char const * const FileName, WIN32_FIND_DATAA &Data)
{
    HANDLE Handle = FindFirstFileA(FileName, &Data);
    if(Handle != INVALID_HANDLE_VALUE)
    {
        FindClose(Handle);
        return(true);
    }

    return(false);
}

char const *GRANNY
GetTemporaryDirectory(void)
{
    static char TempDirectoryBuffer[MAX_PATH] = { 0 };

    if (TempDirectoryBuffer[0] == '\0')
    {
        GetTempPathA(sizeof(TempDirectoryBuffer) - 1, TempDirectoryBuffer);
        {for(char *Replace = TempDirectoryBuffer;
             *Replace;
             ++Replace)
        {
            if(*Replace == '\\')
            {
                *Replace = '/';
            }
        }}
    }

    return (TempDirectoryBuffer);
}

void GRANNY
DeleteFile(char const *FileName)
{
    DeleteFileProxy(FileName);
}

int32x GRANNY
GetFileSize(char const *FileName)
{
    int32x FileSize = 0;

    WIN32_FIND_DATAA Data;
    if(WinXXFindFile(FileName, Data))
    {
        DWORD HighSize = Data.nFileSizeHigh;
        DWORD LowSize = Data.nFileSizeLow;
        if(HighSize == 0)
        {
            FileSize = LowSize;
        }
        else
        {
            Log1(ErrorLogMessage, FileReadingLogMessage,
                 "File %s is too long (> 4 gigs)", FileName);
        }
    }
    else
    {
        Log1(ErrorLogMessage, FileReadingLogMessage,
             "Unable to get file size for %s", FileName);
    }

    return(FileSize);
}
