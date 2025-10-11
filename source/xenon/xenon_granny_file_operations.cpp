// ========================================================================
// $File: //jeffr/granny_29/rt/xenon/xenon_granny_file_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_file_operations.h"
#include "xenon_granny_xtl.h"
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
    DeleteFile(FileName);
}

inline DWORD
GetFileSizeProxy(HANDLE XenonFileHandle, DWORD *HighSize)
{
    return(GetFileSize(XenonFileHandle, HighSize));
}

#undef DeleteFile
#undef GetFileSize

static bool
XenonFindFile(char const * const FileName, WIN32_FIND_DATA &Data)
{
    HANDLE Handle = FindFirstFile(FileName, &Data);
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
    return("./");
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

    WIN32_FIND_DATA Data;
    if(XenonFindFile(FileName, Data))
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
