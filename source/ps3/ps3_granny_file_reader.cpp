// ========================================================================
// $File: //jeffr/granny_29/rt/ps3/ps3_granny_file_reader.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_assert.h"
#include "granny_file_reader.h"
#include "granny_memory.h"
#include "granny_parameter_checking.h"
#include "ps3_granny_std.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode FileReadingLogMessage

USING_GRANNY_NAMESPACE;

// This enables the standard unix IO stuff.
// We recommend NOT doing this - instead, install your own file readers etc
// at runtime using GrannySetDefaultFileReaderOpenCallback
#define PS3_UNIX_IO_READER_ENABLED 1

#if PS3_UNIX_IO_READER_ENABLED

struct ps3_file_reader
{
    file_reader Base;
    FILE *PS3FileHandle;
};

static CALLBACK_FN(void)
PS3CloseFileReader(file_reader *ReaderInit)
{
    ps3_file_reader *Reader = (ps3_file_reader *)ReaderInit;

    if(Reader)
    {
        if(fclose(Reader->PS3FileHandle))
        {
            PS3LogErrorAsWarning(fclose);
        }

        Deallocate(Reader);
    }
}

static CALLBACK_FN(int32x)
PS3ReadAtMost(file_reader* ReaderInit, int32x FilePosition,
              int32x UInt8Count, void *Buffer)
{
    int32x UInt8sRead = 0;

    CheckPointerNotNull(ReaderInit, return 0);
    ps3_file_reader &Reader = *((ps3_file_reader*)ReaderInit);

    if(PS3Seek(Reader.PS3FileHandle, FilePosition, SEEK_SET) ==
       FilePosition)
    {
        UInt8sRead = fread(Buffer, 1, UInt8Count, Reader.PS3FileHandle);
        if(UInt8sRead < UInt8Count)
        {
            if(ferror(Reader.PS3FileHandle))
            {
                PS3LogErrorAsWarning(fread);
            }
        }
    }

    return(UInt8sRead);
}

static CALLBACK_FN(bool)
PS3GetReaderSize(file_reader* ReaderInit, int32x* ReaderSize)
{
    CheckPointerNotNull(ReaderSize, return false);

    CheckPointerNotNull(ReaderInit, return false);
    ps3_file_reader &Reader = *((ps3_file_reader*)ReaderInit);

    long cur = ftell(Reader.PS3FileHandle);
    fseek(Reader.PS3FileHandle, 0, SEEK_END);
    *ReaderSize = ftell(Reader.PS3FileHandle);
    fseek(Reader.PS3FileHandle, cur, SEEK_SET);

    return true;
}


file_reader* GRANNY
CreatePlatformFileReaderInternal(char const *FileNameToOpen)
{
    ps3_file_reader *Reader = 0;

    FILE *PS3FileHandle = fopen(FileNameToOpen, "rb");
    if(PS3FileHandle)
    {
        Reader = Allocate(ps3_file_reader, AllocationUnknown);
        if(Reader)
        {
            InitializeFileReader(PS3CloseFileReader,
                                 PS3ReadAtMost,
                                 PS3GetReaderSize,
                                 Reader->Base);
            Reader->PS3FileHandle = PS3FileHandle;
        }
        else
        {
            fclose(PS3FileHandle);
        }
    }
    else
    {
        PS3LogErrorAsWarning(fopen);
    }

    return((file_reader *)Reader);
}

#else //#if PS3_UNIX_IO_READER_ENABLED


file_reader* GRANNY
CreatePlatformFileReaderInternal(char const *FileNameToOpen)
{
    Assert ( !"You need to set up your own file reader with GrannySetDefaultFileReaderOpenCallback before calling any Granny file functions" );
    return NULL;
}

#endif //#else //#if PS3_UNIX_IO_READER_ENABLED

open_file_reader_callback *GRANNY OpenFileReaderCallback = CreatePlatformFileReaderInternal;
