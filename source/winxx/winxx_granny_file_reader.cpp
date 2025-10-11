// ========================================================================
// $File: //jeffr/granny_29/rt/winxx/winxx_granny_file_reader.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_file_reader.h"

#include "granny_assert.h"
#include "granny_limits.h"
#include "granny_memory.h"
#include "granny_parameter_checking.h"
#include "granny_telemetry.h"
#include "winxx_granny_windows.h"

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


struct winxx_file_reader
{
    file_reader Base;
    HANDLE WinXXFileHandle;
};

END_GRANNY_NAMESPACE;

static CALLBACK_FN(void)
WinXXCloseFileReader(file_reader* ReaderInit)
{
    GRANNY_AUTO_IDLE_ZONE(CloseFile);
    winxx_file_reader *Reader = (winxx_file_reader *)ReaderInit;

    if(Reader)
    {
        if(!CloseHandle(Reader->WinXXFileHandle))
        {
            WinXXLogErrorAsWarning(CloseHandle);
        }

        Deallocate(Reader);
    }
}

static CALLBACK_FN(int32x)
WinXXReadAtMost(file_reader* ReaderInit,
                int32x FilePosition,
                int32x UInt8Count, void *Buffer)
{
    GRANNY_AUTO_IDLE_ZONE(ReadAtMost);
    CheckPointerNotNull(ReaderInit, return 0);

    DWORD UInt8sRead = 0;
    winxx_file_reader &Reader = *((winxx_file_reader*)ReaderInit);

    if(WinXXSeek(Reader.WinXXFileHandle, FilePosition, FILE_BEGIN)
       == FilePosition)
    {
        GRANNY_ENTER_ZONE(read);
        if(!ReadFile(Reader.WinXXFileHandle, Buffer,
                     UInt8Count, &UInt8sRead, 0))
        {
            WinXXLogErrorAsWarning(ReadFile);
        }
        GRANNY_LEAVE_ZONE();
    }

    return(UInt8sRead);
}

static CALLBACK_FN(bool)
WinXXGetReaderSize(file_reader* ReaderInit, int32x* SizeVar)
{
    GRANNY_AUTO_IDLE_ZONE(GetReaderSize);
    CheckPointerNotNull(ReaderInit, return false);
    CheckPointerNotNull(SizeVar, return false);

    winxx_file_reader& Reader = *((winxx_file_reader*)ReaderInit);

    DWORD FileSize = GetFileSize(Reader.WinXXFileHandle, NULL);
    if (FileSize != -1)
    {
        CheckConvertToInt32(*SizeVar, FileSize, return false);
        return true;
    }

    return false;
}

CALLBACK_FN(file_reader*) GRANNY
CreatePlatformFileReaderInternal(char const *FileNameToOpen)
{
    GRANNY_AUTO_IDLE_ZONE(CreateFile);
    winxx_file_reader *Reader = 0;

    HANDLE WinXXFileHandle =
        CreateFileA(FileNameToOpen,
                    GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, 0);
    if (WinXXFileHandle != INVALID_HANDLE_VALUE)
    {
        Reader = Allocate(winxx_file_reader, AllocationUnknown);
        if(Reader)
        {
            InitializeFileReader(WinXXCloseFileReader,
                                 WinXXReadAtMost,
                                 WinXXGetReaderSize,
                                 Reader->Base);
            Reader->WinXXFileHandle = WinXXFileHandle;
        }
        else
        {
            CloseHandle(WinXXFileHandle);
        }
    }
    else
    {
        // TODO: We decided to stop logging this, since files
        // often cannot be opened and it is not considered an
        // error condition.
//        WinXXLogErrorAsWarning(CreateFile);
    }

    return (file_reader *)Reader;
}

open_file_reader_callback *GRANNY OpenFileReaderCallback = CreatePlatformFileReaderInternal;
