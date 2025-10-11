// ========================================================================
// $File: //jeffr/granny_29/rt/xenon/xenon_granny_file_reader.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "xenon_granny_xtl.h"
#include "granny_file_reader.h"
#include "granny_memory.h"
#include "granny_assert.h"
#include "granny_parameter_checking.h"

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
BEGIN_GRANNY_NAMESPACE;

struct win32_file_reader
{
    file_reader Base;
    HANDLE Win32FileHandle;
};

END_GRANNY_NAMESPACE;

static CALLBACK_FN(void)
Win32CloseFileReader(file_reader* ReaderInit)
{
    win32_file_reader *Reader = (win32_file_reader *)ReaderInit;

    if(Reader)
    {
        if(!CloseHandle(Reader->Win32FileHandle))
        {
            Win32LogErrorAsWarning(CloseHandle);
        }

        Deallocate(Reader);
    }
}

static CALLBACK_FN(int32x)
Win32ReadAtMost(file_reader* ReaderInit, int32x FilePosition,
                int32x UInt8Count, void *Buffer)
{
    CheckPointerNotNull(ReaderInit, return 0);
    win32_file_reader &Reader = *((win32_file_reader*)ReaderInit);

    DWORD UInt8sRead = 0;
    if(Win32Seek(Reader.Win32FileHandle, FilePosition, FILE_BEGIN)
       == FilePosition)
    {
        if(!ReadFile(Reader.Win32FileHandle, Buffer,
                     UInt8Count, &UInt8sRead, 0))
        {
            Win32LogErrorAsWarning(ReadFile);
        }
    }

    return(UInt8sRead);
}

static CALLBACK_FN(bool)
Win32GetReaderSize(file_reader* ReaderInit, int32x* SizeVar)
{
    CheckPointerNotNull(ReaderInit, return 0);
    win32_file_reader &Reader = *((win32_file_reader*)ReaderInit);

    CheckPointerNotNull(SizeVar, return false);

    DWORD FileSize = GetFileSize(Reader.Win32FileHandle, NULL);
    if (FileSize != -1)
    {
        CheckConvertToInt32(*SizeVar, FileSize, return false);
        return true;
    }

    return false;
}

file_reader* GRANNY
CreatePlatformFileReaderInternal(char const *FileNameToOpen)
{
    win32_file_reader* Reader = 0;

    HANDLE Win32FileHandle =
        CreateFile(FileNameToOpen, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL, 0);
    if (Win32FileHandle != INVALID_HANDLE_VALUE)
    {
        Reader = Allocate(win32_file_reader, AllocationUnknown);
        if(Reader)
        {
            InitializeFileReader(Win32CloseFileReader,
                                 Win32ReadAtMost,
                                 Win32GetReaderSize,
                                 Reader->Base);
            Reader->Win32FileHandle = Win32FileHandle;
        }
        else
        {
            CloseHandle(Win32FileHandle);
        }
    }
    else
    {
        Win32LogErrorAsWarning(CreateFile);
    }

    return((file_reader *)Reader);
}

open_file_reader_callback *GRANNY OpenFileReaderCallback = CreatePlatformFileReaderInternal;
