// ========================================================================
// $File: //jeffr/granny_29/rt/ansi/ansi_granny_file_writer.cpp $
// $DateTime: 2011/12/06 13:55:23 $
// $Change: 35921 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "ansi_granny_std.h"
#include "granny_assert.h"
#include "granny_crc.h"
#include "granny_file_writer.h"
#include "granny_memory.h"
#include "granny_parameter_checking.h"

// Prevent deprecated warnings in VC, we don't want to use the
//  new fopen_s, etc, api.
#if COMPILER_MSVC
#pragma warning(disable : 4996)
#endif

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode FileWritingLogMessage

USING_GRANNY_NAMESPACE;
BEGIN_GRANNY_NAMESPACE;

struct ansi_file_writer
{
    file_writer Base;
    FILE *ANSIFileHandle;
};
CompileAssert(OffsetFromType(ansi_file_writer, Base) == 0);
CompileAssert(SizeOf(ansi_file_writer) == (SizeOf(file_writer) +
                                           SizeOf(FILE*)));

END_GRANNY_NAMESPACE;

static CALLBACK_FN(void)
AnsiDeleteFileWriter(file_writer *Writer)
{
    ansi_file_writer *AnsiWriter = (ansi_file_writer*)Writer;
    if(Writer)
    {
        Assert(!AnsiWriter->Base.CRCing);

        if(fclose(AnsiWriter->ANSIFileHandle))
        {
            ANSILogErrorAsWarning(fclose);
        }

        Deallocate(Writer);
    }
}

static CALLBACK_FN(int32x)
AnsiSeekWriter(file_writer* WriterInit,
               int32x       OffsetInUInt8s,
               int32x       SeekType)
{
    CheckPointerNotNull(WriterInit, return 0);
    ansi_file_writer &AnsiWriter = *((ansi_file_writer*)WriterInit);
    Assert(!AnsiWriter.Base.CRCing || (SeekType == SeekCurrent && OffsetInUInt8s == 0));

    switch (SeekType)
    {
        case SeekStart:
            return(ANSISeek(AnsiWriter.ANSIFileHandle, OffsetInUInt8s, SEEK_SET));
        case SeekEnd:
            return(ANSISeek(AnsiWriter.ANSIFileHandle, OffsetInUInt8s, SEEK_END));
        case SeekCurrent:
            return(ANSISeek(AnsiWriter.ANSIFileHandle, OffsetInUInt8s, SEEK_CUR));

        default:
            InvalidCodePath("Invalid seek type");
            return 0;
    }
}

static CALLBACK_FN(bool)
AnsiWrite(file_writer* WriterInit, int32x UInt8Count, void const *WritePointer)
{
    CheckPointerNotNull(WriterInit, return false);
    ansi_file_writer &AnsiWriter = *((ansi_file_writer*)WriterInit);
    if(AnsiWriter.Base.CRCing)
    {
        // We're CRCing, so we need to update our current CRC value
        AddToCRC32(AnsiWriter.Base.CRC, UInt8Count, WritePointer);
    }

    if(fwrite(WritePointer, UInt8Count, 1, AnsiWriter.ANSIFileHandle) == 1)
    {
        return(true);
    }
    else
    {
        ANSILogErrorAsWarning(fwrite);
        return(false);
    }
}

static CALLBACK_FN(void)
AnsiBeginWriterCRC(file_writer* WriterInit)
{
    CheckPointerNotNull(WriterInit, return);
    ansi_file_writer &AnsiWriter = *((ansi_file_writer*)WriterInit);
    Assert(!AnsiWriter.Base.CRCing);

    AnsiWriter.Base.CRCing = true;
    BeginCRC32(AnsiWriter.Base.CRC);
}

static CALLBACK_FN(uint32)
AnsiEndWriterCRC(file_writer* WriterInit)
{
    CheckPointerNotNull(WriterInit, return 0);
    ansi_file_writer &AnsiWriter = *((ansi_file_writer*)WriterInit);
    Assert(AnsiWriter.Base.CRCing);

    AnsiWriter.Base.CRCing = false;
    EndCRC32(AnsiWriter.Base.CRC);

    return(AnsiWriter.Base.CRC);
}


file_writer* GRANNY
CreatePlatformFileWriterInternal(char const *FileNameToOpen, bool EraseExisting)
{
    FILE *ANSIFileHandle = 0;
    if (EraseExisting)
    {
        ANSIFileHandle = fopen(FileNameToOpen, "wb");
    }
    else
    {
        ANSIFileHandle = fopen(FileNameToOpen, "r+b");
        if (!ANSIFileHandle)
        {
            // Try in 'a' mode, since 'r+' must exist
            ANSIFileHandle = fopen(FileNameToOpen, "ab");
        }
    }

    if(ANSIFileHandle)
    {
        ansi_file_writer *NewWriter = Allocate(ansi_file_writer, AllocationUnknown);
        if(NewWriter)
        {
            InitializeFileWriter(AnsiDeleteFileWriter,
                                 AnsiSeekWriter,
                                 AnsiWrite,
                                 AnsiBeginWriterCRC,
                                 AnsiEndWriterCRC,
                                 NewWriter->Base);
            NewWriter->ANSIFileHandle = ANSIFileHandle;

            return (file_writer*)NewWriter;
        }
        else
        {
            fclose(ANSIFileHandle);
        }
    }
    else
    {
        ANSILogErrorAsWarning(fopen);
    }

    return 0;
}

open_file_writer_callback *GRANNY OpenFileWriterCallback = CreatePlatformFileWriterInternal;
