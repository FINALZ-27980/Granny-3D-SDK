// ========================================================================
// $File: //jeffr/granny_29/rt/granny_file_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_file_operations.h"

#include "granny_file_reader.h"
#include "granny_file_writer.h"
#include "granny_memory.h"
#include "granny_string.h"
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

#define SubsystemCode FileOperationLogMessage
USING_GRANNY_NAMESPACE;

#define DefaultCopyBufferSize (1 << 16)

bool GRANNY
AppendStringToFile(char const* Filename, char const* String)
{
    CheckPointerNotNull(Filename, return false);
    CheckPointerNotNull(String, return false);

    file_writer *Writer = NewFileWriter(Filename, false);
    if (Writer == 0)
        return false;

    SeekWriterFromEnd(Writer, 0);
    WriteBytes(Writer, StringLength(String), String);
    WriteBytes(Writer, 1, "\n");
    CloseFileWriter(Writer);

    return true;
}



bool GRANNY
ConcatenateFile(file_writer* Writer, char const *FileName,
                uint32x* BytesCopied)
{
    CheckPointerNotNull(Writer, return false);
    CheckPointerNotNull(FileName, return false);

    uint32x DummyBytesCopied;
    if (BytesCopied == 0)
        BytesCopied = &DummyBytesCopied;
    *BytesCopied = 0;

    bool Result = false;
    file_reader *Reader = OpenFileReader(FileName);
    if (Reader)
    {
        Result = ConcatenateFileReader(Writer, Reader, BytesCopied);
        CloseFileReader(Reader);
    }

    return Result;
}


bool GRANNY
ConcatenateFileReader(file_writer* Writer,
                      file_reader* Reader,
                      uint32x* BytesCopied)
{
    CheckPointerNotNull(Reader, return false);
    CheckPointerNotNull(Writer, return false);

    uint32x DummyBytesCopied;
    if (BytesCopied == 0)
        BytesCopied = &DummyBytesCopied;
    *BytesCopied = 0;

    void* CopyBuffer = AllocateSize(DefaultCopyBufferSize, AllocationTemporary);
    if (CopyBuffer == 0)
        return false;

    bool Result = true;
    while (Result)
    {
        int32x const ReadSize = ReadAtMost(Reader, *BytesCopied,
                                           DefaultCopyBufferSize, CopyBuffer);
        if (ReadSize == 0)
            break;

        Result = Result && WriteBytes(Writer, ReadSize, CopyBuffer);
        *BytesCopied += ReadSize;
    }

    Deallocate(CopyBuffer);
    return Result;
}
