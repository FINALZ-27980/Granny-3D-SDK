// ========================================================================
// $File: //jeffr/granny_29/rt/granny_memory_file_reader.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_memory_file_reader.h"

#include "granny_file_reader.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
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

struct memory_file_reader
{
    file_reader  Base;
    int32x       MemorySize;
    uint8 const* Memory;
};

END_GRANNY_NAMESPACE;

static CALLBACK_FN(void)
MemoryCloseFileReader(file_reader* Reader)
{
    Deallocate(Reader);
}

static CALLBACK_FN(int32x)
MemoryReadAtMost(file_reader* ReaderInit,
                 int32x FilePosition,
                 int32x UInt8Count,
                 void *Buffer)
{
    CheckPointerNotNull(ReaderInit, return 0);

    memory_file_reader &Reader = *((memory_file_reader*)ReaderInit);
    CheckBoundedInt32(0, FilePosition, Reader.MemorySize, return 0);

    int32x BytesRead = UInt8Count;
    if ((FilePosition + BytesRead) > Reader.MemorySize)
    {
        BytesRead = Reader.MemorySize - FilePosition;
    }

    Copy(BytesRead, &Reader.Memory[FilePosition], Buffer);

    return BytesRead;
}

static CALLBACK_FN(bool)
MemoryGetReaderSize(file_reader* ReaderInit, int32x* SizeVar)
{
    CheckPointerNotNull(ReaderInit, return 0);
    CheckPointerNotNull(SizeVar, return false);

    memory_file_reader &Reader = *((memory_file_reader*)ReaderInit);
    *SizeVar = Reader.MemorySize;

    return true;
}

file_reader *GRANNY
CreateMemoryFileReader(int32x MemorySize, void const* Memory)
{
    CheckCondition(MemorySize >= 0, return 0);
    CheckCondition(Memory != 0 || MemorySize == 0, return 0);

    memory_file_reader *Reader = Allocate(memory_file_reader, AllocationUnknown);
    if(Reader)
    {
        InitializeFileReader(MemoryCloseFileReader,
                             MemoryReadAtMost,
                             MemoryGetReaderSize,
                             Reader->Base);
        Reader->MemorySize = MemorySize;
        Reader->Memory = (uint8 *)Memory;
    }

    return((file_reader *)Reader);
}
