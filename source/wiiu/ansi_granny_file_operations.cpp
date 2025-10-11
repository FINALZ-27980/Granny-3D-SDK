// ========================================================================
// $File: //jeffr/granny_29/rt/ansi/ansi_granny_file_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "ansi_granny_std.h"
#include "granny_file_operations.h"
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

// Prevent deprecated warnings in VC, we don't want to use the
//  new fopen_s, etc, api.
#if COMPILER_MSVC
#pragma warning(disable : 4996)
#endif


void GRANNY
DeleteFile(char const *FileName)
{
    remove(FileName);
}

int32x GRANNY
GetFileSize(char const *FileName)
{
    int32x FileSize = 0;

    FILE *File = fopen(FileName, "rb");
    if(File)
    {
        if(ANSISeek(File, 0, SEEK_END))
        {
            FileSize = ftell(File);
        }

        fclose(File);
    }
    else
    {
        Log1(ErrorLogMessage, FileReadingLogMessage,
             "Unable to get file size for %s.", FileName);
    }

    return(FileSize);
}

char const *GRANNY
GetTemporaryDirectory(void)
{
    // TODO: Would it perhaps be more proper to query tempnam() or something here and then
    // just rip out the path portion?
    return("/tmp/");
}

int32x GRANNY
ANSISeek(FILE *ANSIFileHandle, int32x Offset, int32x MoveMethod)
{
    if(fseek(ANSIFileHandle, (long)Offset, MoveMethod))
    {
        ANSILogErrorAsWarning(fseek);
    }

    return(ftell(ANSIFileHandle));
}



