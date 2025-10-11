// ========================================================================
// $File: //jeffr/granny_29/rt/xenon/xenon_granny_file_writer.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "xenon_granny_xtl.h"

#include "granny_assert.h"
#include "granny_crc.h"
#include "granny_file_writer.h"
#include "granny_parameter_checking.h"
#include "granny_memory.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode XenonSubsystemLogMessage

USING_GRANNY_NAMESPACE;
BEGIN_GRANNY_NAMESPACE;

struct xenon_file_writer
{
    file_writer Base;
    HANDLE      XenonFileHandle;
};
CompileAssert(OffsetFromType(xenon_file_writer, Base) == 0);
CompileAssert(SizeOf(xenon_file_writer) == (SizeOf(file_writer) +
                                            SizeOf(HANDLE)));

END_GRANNY_NAMESPACE;


static CALLBACK_FN(void)
XenonDeleteFileWriter(file_writer* Writer)
{
    xenon_file_writer *XenonWriter = (xenon_file_writer*)Writer;
    if(Writer)
    {
        Assert(!XenonWriter->Base.CRCing);

        if(!CloseHandle(XenonWriter->XenonFileHandle))
        {
            Win32LogErrorAsWarning(CloseHandle);
        }

        Deallocate(Writer);
    }
}

static CALLBACK_FN(int32x)
XenonSeekWriter(file_writer*          Writer,
                int32x                OffsetInUInt8s,
                int32x                SeekType)
{
    CheckPointerNotNull(Writer, return 0);
    xenon_file_writer& XenonWriter = *((xenon_file_writer*)Writer);

    Assert(!XenonWriter.Base.CRCing || (SeekType == SeekCurrent && OffsetInUInt8s == 0));
    switch (SeekType)
    {
        case SeekStart:
            return(Win32Seek(XenonWriter.XenonFileHandle, OffsetInUInt8s, FILE_BEGIN));
        case SeekEnd:
            return(Win32Seek(XenonWriter.XenonFileHandle, OffsetInUInt8s, FILE_END));
        case SeekCurrent:
            return(Win32Seek(XenonWriter.XenonFileHandle, OffsetInUInt8s, FILE_CURRENT));

        default:
            InvalidCodePath("Invalid seek type");
            return 0;
    }
}

static CALLBACK_FN(bool)
XenonWrite(file_writer* Writer,
           int32x       UInt8Count,
           void const*  WritePointer)
{
    CheckPointerNotNull(Writer, return 0);
    xenon_file_writer& XenonWriter = *((xenon_file_writer*)Writer);

    if(XenonWriter.Base.CRCing)
    {
        // We're CRCing, so we need to update our current CRC value
        AddToCRC32(XenonWriter.Base.CRC, UInt8Count, WritePointer);
    }

    DWORD UInt8sWritten = 0;
    if(WriteFile(XenonWriter.XenonFileHandle, WritePointer,
                 UInt8Count, &UInt8sWritten, 0) &&
       ((int32x)UInt8sWritten == UInt8Count))
    {
        return(true);
    }
    else
    {
        Win32LogErrorAsWarning(WriteFile);
        return(false);
    }
}

static CALLBACK_FN(void)
XenonBeginWriterCRC(file_writer* Writer)
{
    CheckPointerNotNull(Writer, return);
    xenon_file_writer& XenonWriter = *((xenon_file_writer*)Writer);
    Assert(!XenonWriter.Base.CRCing);

    XenonWriter.Base.CRCing = true;
    BeginCRC32(XenonWriter.Base.CRC);
}

static CALLBACK_FN(uint32)
XenonEndWriterCRC(file_writer* Writer)
{
    CheckPointerNotNull(Writer, return 0);
    xenon_file_writer& XenonWriter = *((xenon_file_writer*)Writer);
    Assert(XenonWriter.Base.CRCing);

    XenonWriter.Base.CRCing = false;
    EndCRC32(XenonWriter.Base.CRC);

    return(XenonWriter.Base.CRC);
}

file_writer* GRANNY
CreatePlatformFileWriterInternal(char const *FileNameToOpen, bool EraseExisting)
{
    HANDLE XenonFileHandle = CreateFile(
        FileNameToOpen, GENERIC_WRITE, 0, 0,
        EraseExisting ? CREATE_ALWAYS : OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, 0);
    if(XenonFileHandle != INVALID_HANDLE_VALUE)
    {
        xenon_file_writer *NewWriter = Allocate(xenon_file_writer, AllocationUnknown);
        if(NewWriter)
        {
            InitializeFileWriter(XenonDeleteFileWriter,
                                 XenonSeekWriter,
                                 XenonWrite,
                                 XenonBeginWriterCRC,
                                 XenonEndWriterCRC,
                                 NewWriter->Base);
            NewWriter->XenonFileHandle = XenonFileHandle;

            return (file_writer*)NewWriter;
        }
        else
        {
            CloseHandle(XenonFileHandle);
        }
    }
    else
    {
        Win32LogErrorAsWarning(CreateFile);
    }

    return 0;
}

open_file_writer_callback* GRANNY OpenFileWriterCallback = CreatePlatformFileWriterInternal;
