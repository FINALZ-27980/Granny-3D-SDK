// ========================================================================
// $File: //jeffr/granny_29/rt/winxx/winxx_granny_file_writer.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_assert.h"
#include "granny_crc.h"
#include "granny_file_writer.h"
#include "granny_limits.h"
#include "granny_memory.h"
#include "granny_parameter_checking.h"
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

#define SubsystemCode WinXXSubsystemLogMessage


USING_GRANNY_NAMESPACE;
BEGIN_GRANNY_NAMESPACE;

struct winxx_file_writer
{
    file_writer Base;
    HANDLE      WinXXFileHandle;
};
CompileAssert(OffsetFromType(winxx_file_writer, Base) == 0);
CompileAssert(SizeOf(winxx_file_writer) == (SizeOf(file_writer) +
                                            SizeOf(HANDLE)));

END_GRANNY_NAMESPACE;

static CALLBACK_FN(void)
WinXXDeleteFileWriter(file_writer* Writer)
{
    winxx_file_writer* WinXXWriter = (winxx_file_writer*)Writer;
    if(Writer)
    {
        Assert(!WinXXWriter->Base.CRCing);

        if(!CloseHandle(WinXXWriter->WinXXFileHandle))
        {
            WinXXLogErrorAsWarning(CloseHandle);
        }

        Deallocate(Writer);
    }
}

static CALLBACK_FN(int32x)
WinXXSeekWriter(file_writer* Writer,
                int32x OffsetInUInt8s,
                int32x SeekType)
{
    CheckPointerNotNull(Writer, return 0);
    winxx_file_writer& WinXXWriter = *((winxx_file_writer*)Writer);

    Assert(!WinXXWriter.Base.CRCing || (SeekType == SeekCurrent && OffsetInUInt8s == 0));

    switch (SeekType)
    {
        case SeekStart:
            return(WinXXSeek(WinXXWriter.WinXXFileHandle, OffsetInUInt8s, FILE_BEGIN));
        case SeekEnd:
            return(WinXXSeek(WinXXWriter.WinXXFileHandle, OffsetInUInt8s, FILE_END));
        case SeekCurrent:
            return(WinXXSeek(WinXXWriter.WinXXFileHandle, OffsetInUInt8s, FILE_CURRENT));

        default:
            InvalidCodePath("Invalid seek type");
            return 0;
    }
}

static CALLBACK_FN(bool)
WinXXWrite(file_writer* Writer,
           int32x UInt8Count,
           void const *WritePointer)
{
    CheckPointerNotNull(Writer, return 0);
    winxx_file_writer& WinXXWriter = *((winxx_file_writer*)Writer);

    if(WinXXWriter.Base.CRCing)
    {
        // We're CRCing, so we need to update our current CRC value
        AddToCRC32(WinXXWriter.Base.CRC, UInt8Count, WritePointer);
    }

    DWORD UInt8sWritten = 0;
    if(WriteFile(WinXXWriter.WinXXFileHandle, WritePointer,
                 UInt8Count, &UInt8sWritten, 0) &&
       ((int32x)UInt8sWritten == UInt8Count))
    {
        return(true);
    }
    else
    {
        WinXXLogErrorAsWarning(WriteFile);
        return(false);
    }
}

static CALLBACK_FN(void)
WinXXBeginWriterCRC(file_writer* Writer)
{
    CheckPointerNotNull(Writer, return);
    winxx_file_writer& WinXXWriter = *((winxx_file_writer*)Writer);

    Assert(!WinXXWriter.Base.CRCing);

    WinXXWriter.Base.CRCing = true;
    BeginCRC32(WinXXWriter.Base.CRC);
}

static CALLBACK_FN(uint32)
WinXXEndWriterCRC(file_writer* Writer)
{
    CheckPointerNotNull(Writer, return 0);
    winxx_file_writer& WinXXWriter = *((winxx_file_writer*)Writer);

    Assert(WinXXWriter.Base.CRCing);

    WinXXWriter.Base.CRCing = false;
    EndCRC32(WinXXWriter.Base.CRC);

    return(WinXXWriter.Base.CRC);
}

CALLBACK_FN(file_writer*) GRANNY
CreatePlatformFileWriterInternal(char const *FileNameToOpen, bool EraseExisting)
{
    HANDLE WinXXFileHandle =
        CreateFileA(FileNameToOpen,
                    GENERIC_WRITE, 0, 0,
                    EraseExisting ? CREATE_ALWAYS : OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, 0);
    if(WinXXFileHandle != INVALID_HANDLE_VALUE)
    {
        winxx_file_writer *NewWriter = Allocate(winxx_file_writer, AllocationUnknown);
        if(NewWriter)
        {
            InitializeFileWriter(WinXXDeleteFileWriter,
                                 WinXXSeekWriter,
                                 WinXXWrite,
                                 WinXXBeginWriterCRC,
                                 WinXXEndWriterCRC,
                                 NewWriter->Base);
            NewWriter->WinXXFileHandle = WinXXFileHandle;

            return (file_writer*)NewWriter;
        }
        else
        {
            CloseHandle(WinXXFileHandle);
        }
    }
    else
    {
        WinXXLogErrorAsWarning(CreateFile);
    }

    return 0;
}

open_file_writer_callback *GRANNY OpenFileWriterCallback = CreatePlatformFileWriterInternal;
