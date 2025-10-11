// ========================================================================
// $File: //jeffr/granny_29/rt/ps3/ps3_granny_file_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "ps3_granny_std.h"
#include "granny_file_operations.h"
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


// GRANNY_PORTING
// This enables the standard unix IO stuff.
// We recommend NOT doing this - instead, install your own file readers etc
#define PS3_UNIX_IO_ENABLED 1


void GRANNY
DeleteFile(char const *FileName)
{
#if !PS3_UNIX_IO_ENABLED
    Assert ( !"PS3_UNIX_IO_ENABLED set to 0 - no Granny IO available" );
#else
    Assert ( !"How do you delete files on a PS3?" );
    //remove(FileName);
#endif
}

int32x GRANNY
GetFileSize(char const* FileName)
{
#if !PS3_UNIX_IO_ENABLED
    Assert ( !"PS3_UNIX_IO_ENABLED set to 0 - no Granny IO available" );
    return 0;
#else
    int32x FileSize = 0;

    FILE *File = fopen(FileName, "rb");
    if(File)
    {
        if(PS3Seek(File, 0, SEEK_END))
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
#endif
}

char const *GRANNY
GetTemporaryDirectory(void)
{
#if !PS3_UNIX_IO_ENABLED
    Assert ( !"PS3_UNIX_IO_ENABLED set to 0 - no Granny IO available" );
    return NULL;
#else
    // TODO: Would it perhaps be more proper to query tempnam() or something here and then
    // just rip out the path portion?
    return("/tmp/");
#endif
}


int32x GRANNY
PS3Seek(FILE *PS3FileHandle, int32x Offset, int32x MoveMethod)
{
#if !PS3_UNIX_IO_ENABLED
    Assert ( !"PS3_UNIX_IO_ENABLED set to 0 - no Granny IO available" );
    return 0;
#else
    if(fseek(PS3FileHandle, (long)Offset, MoveMethod))
    {
        PS3LogErrorAsWarning(fseek);
    }
    return(ftell(PS3FileHandle));
#endif
}
