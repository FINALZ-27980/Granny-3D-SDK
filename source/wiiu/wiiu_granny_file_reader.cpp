// ========================================================================
// $File: //jeffr/granny_29/rt/wiiu/wiiu_granny_file_reader.cpp $
// $DateTime: 2012/10/14 13:36:21 $
// $Change: 39767 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_assert.h"
#include "granny_file_reader.h"
#include "granny_log.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"

#include <cafe.h>
#include <cafe/fs.h>

// Should always be the last header included
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

struct wiiu_file_reader
{
    file_reader  Base;

    FSClient     Client;
    FSCmdBlock   CommandBlock;

    FSFileHandle Handle;
};

static void
LogFSErrorAsWarning(FSStatus Status, char const* Location)
{
    char const* errorStr = "UNKNOWN";
    switch (Status)
    {
        case FS_STATUS_ERROR_BASE:       errorStr = "FS_STATUS_ERROR_BASE";       break;
        case FS_STATUS_CANCELLED:        errorStr = "FS_STATUS_CANCELLED";       break;
        case FS_STATUS_END:              errorStr = "FS_STATUS_END";              break;
        case FS_STATUS_MAX:              errorStr = "FS_STATUS_MAX";              break;
        case FS_STATUS_ALREADY_OPEN:     errorStr = "FS_STATUS_ALREADY_OPEN";     break;
        case FS_STATUS_EXISTS:           errorStr = "FS_STATUS_EXISTS";           break;
        case FS_STATUS_NOT_FOUND:        errorStr = "FS_STATUS_NOT_FOUND";        break;
        case FS_STATUS_NOT_FILE:         errorStr = "FS_STATUS_NOT_FILE";         break;
        case FS_STATUS_NOT_DIR:          errorStr = "FS_STATUS_NOT_DIR";          break;
        case FS_STATUS_ACCESS_ERROR:     errorStr = "FS_STATUS_ACCESS_ERROR";     break;
        case FS_STATUS_PERMISSION_ERROR: errorStr = "FS_STATUS_PERMISSION_ERROR"; break;
        case FS_STATUS_FILE_TOO_BIG:     errorStr = "FS_STATUS_FILE_TOO_BIG";     break;
        case FS_STATUS_STORAGE_FULL:     errorStr = "FS_STATUS_STORAGE_FULL";     break;
        case FS_STATUS_JOURNAL_FULL:     errorStr = "FS_STATUS_JOURNAL_FULL";     break;
        case FS_STATUS_UNSUPPORTED_CMD:  errorStr = "FS_STATUS_UNSUPPORTED_CMD";  break;
        case FS_STATUS_MEDIA_NOT_READY:  errorStr = "FS_STATUS_MEDIA_NOT_READY";  break;
        case FS_STATUS_INVALID_MEDIA:    errorStr = "FS_STATUS_INVALID_MEDIA";    break;
        case FS_STATUS_MEDIA_ERROR:      errorStr = "FS_STATUS_MEDIA_ERROR";      break;
        case FS_STATUS_DATA_CORRUPTED:   errorStr = "FS_STATUS_DATA_CORRUPTED";   break;
        case FS_STATUS_WRITE_PROTECTED:  errorStr = "FS_STATUS_WRITE_PROTECTED";  break;
        case FS_STATUS_FATAL_ERROR:      errorStr = "FS_STATUS_FATAL_ERROR";      break;
    }

    Log2(WarningLogMessage, SubsystemCode,
         "FS Error: %s in %s\n", errorStr,
         Location ? Location : "Unknown");
}

END_GRANNY_NAMESPACE;

static CALLBACK_FN(void)
WiiUCloseFileReader(file_reader *ReaderInit)
{
    wiiu_file_reader *Reader = (wiiu_file_reader *)ReaderInit;

    if (Reader)
    {
        FSStatus CloseStatus = FSCloseFile(&Reader->Client, &Reader->CommandBlock,
                                           Reader->Handle, FS_RET_ALL_ERROR);
        if (CloseStatus != FS_STATUS_OK)
            LogFSErrorAsWarning(CloseStatus, "Close: FileHandle");

        FSStatus ClientStatus = FSDelClient(&Reader->Client, FS_RET_ALL_ERROR);
        if (ClientStatus != FS_STATUS_OK)
            LogFSErrorAsWarning(ClientStatus, "Close: Client");

        Deallocate(Reader);
    }
}

static CALLBACK_FN(int32x)
WiiUReadAtMost(file_reader* ReaderInit, int32x FilePosition,
               int32x UInt8Count, void *Buffer)
{
    CheckPointerNotNull(ReaderInit, return 0);
    CheckCondition(Buffer || UInt8Count == 0, return 0);
    CheckCondition(FilePosition >= 0, return 0);
    
    wiiu_file_reader& Reader = *((wiiu_file_reader*)ReaderInit);

    void* ReadBuffer = Buffer;

    void* AllocatedBuffer = 0;
    if (IS_ALIGNED_N(Buffer, FS_IO_BUFFER_ALIGN) == false)
    {
        ReadBuffer = AllocatedBuffer =
            AllocateSizeAligned(FS_IO_BUFFER_ALIGN,
                                UInt8Count + FS_IO_BUFFER_ALIGN,
                                AllocationTemporary);
        if (!AllocatedBuffer)
        {
            // log error
            Log0(ErrorLogMessage, SubsystemCode, "Unable to allocate aligned tempbuffer");
            return 0;
        }
    }

    FSStatus StatusOrBytes = FSReadFileWithPos(&Reader.Client, &Reader.CommandBlock,
                                               ReadBuffer, 1, UInt8Count,
                                               FilePosition,
                                               Reader.Handle, 0, FS_RET_ALL_ERROR);
    if (StatusOrBytes >= 0)
    {
        if (ReadBuffer != Buffer)
            Copy(StatusOrBytes, ReadBuffer, Buffer);
    }
    else
    {
        LogFSErrorAsWarning(StatusOrBytes, "Read");
    }

    if (AllocatedBuffer)
        DeallocateSafe(AllocatedBuffer);

    return StatusOrBytes >= 0 ? StatusOrBytes : 0;
}

static CALLBACK_FN(bool)
WiiUGetReaderSize(file_reader* ReaderInit, int32x* ResultSize)
{
    CheckPointerNotNull(ReaderInit, return false);
    CheckPointerNotNull(ResultSize, return false);
    wiiu_file_reader& Reader = *((wiiu_file_reader*)ReaderInit);

    FSStat Stat;
    FSStatus Status = FSGetStatFile(&Reader.Client, &Reader.CommandBlock,
                                    Reader.Handle, &Stat, FS_RET_ALL_ERROR);
    if (Status != FS_STATUS_OK)
    {
        LogFSErrorAsWarning(Status, "GetReaderSize");
        return false;
    }

    *ResultSize = Stat.size;
    return true;
}


file_reader* GRANNY
CreatePlatformFileReaderInternal(char const *FileNameToOpen)
{
    wiiu_file_reader *Reader = Allocate(wiiu_file_reader, AllocationUnknown);
    ZeroStructure(*Reader);

    // First, create the client
    FSStatus ClientStat = FSAddClient(&Reader->Client, FS_RET_ALL_ERROR);
    if (ClientStat != FS_STATUS_OK)
    {
        LogFSErrorAsWarning(ClientStat, "CreatePlatformFileReaderInternal: Client");
        Deallocate(Reader);
        return 0;
    }

    // Init the command block...
    FSInitCmdBlock(&Reader->CommandBlock);
    
    // And then open the file...
    FSStatus Status = FSOpenFile(&Reader->Client, &Reader->CommandBlock,
                                 FileNameToOpen, "r",
                                 &Reader->Handle, FS_RET_ALL_ERROR);
    if (Status != FS_STATUS_OK)
    {
        LogFSErrorAsWarning(Status, "CreatePlatformFileReaderInternal: FileHandle");
        FSDelClient(&Reader->Client, FS_RET_ALL_ERROR);
        Deallocate(Reader);
        Reader = 0;
    }
    else
    {
        InitializeFileReader(WiiUCloseFileReader,
                             WiiUReadAtMost,
                             WiiUGetReaderSize,
                             Reader->Base);
    }

    return (file_reader*)Reader;
}

open_file_reader_callback *GRANNY OpenFileReaderCallback = CreatePlatformFileReaderInternal;
