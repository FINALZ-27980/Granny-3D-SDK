// ========================================================================
// $File: //jeffr/granny_29/rt/granny_catenating_reader.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_catenating_reader.h"

#include "granny_aggr_alloc.h"
#include "granny_assert.h"
#include "granny_file_reader.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"

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

struct catenating_reader
{
    file_reader Base;
    uint32      MagicNumber;

    uint32x  TotalSize;
    int32    NumReaders;
    file_reader** Readers;

    // stuff
    uint32x* ReaderSizes;
    uint32x* ReaderStarts;

    int32  AlignmentByteCount;
    uint8* AlignmentBytes;
};

END_GRANNY_NAMESPACE;

static uint32 const CatenatingReaderMagic = 0x2BADC73A;

static catenating_reader*
ObtainCatenatingReader(file_reader* ReaderInit)
{
    CheckPointerNotNull(ReaderInit, return 0);

    catenating_reader* Reader = (catenating_reader*)ReaderInit;
    CheckCondition(Reader->MagicNumber == CatenatingReaderMagic, return 0);

    return Reader;
}

static CALLBACK_FN(void)
CatenatingCloseFileReader(file_reader* ReaderInit)
{
    catenating_reader* Reader = ObtainCatenatingReader(ReaderInit);
    CheckPointerNotNull(Reader, return);

    Deallocate(Reader);
}

static CALLBACK_FN(int32x)
CatenatingReadAtMost(file_reader* ReaderInit,
                     int32x FilePosition,
                     int32x UInt8Count,
                     void *Buffer)
{
    CheckCondition(FilePosition >= 0, return -1);
    CheckCondition(UInt8Count >= 0, return -1);
    CheckPointerNotNull(Buffer, return -1);

    uint8* BufferAsBytes = (uint8*)Buffer;

    catenating_reader* Reader = ObtainCatenatingReader(ReaderInit);
    CheckPointerNotNull(Reader, return 0);
    if (FilePosition >= (int32x)Reader->TotalSize)
        return 0;

    int32x StartStream = 0;
    while (StartStream < Reader->NumReaders &&
           FilePosition >= (int32x)(Reader->ReaderStarts[StartStream] + Reader->ReaderSizes[StartStream]))
    {
        ++StartStream;
    }

    uint32x CurrentPos = FilePosition;
    uint32x EndPos     = FilePosition + UInt8Count;
    uint32x BufferPos  = 0;
    while (CurrentPos < EndPos)
    {
        if (StartStream < Reader->NumReaders)
        {
            if (CurrentPos < Reader->ReaderStarts[StartStream])
            {
                // Align
                uint32x const AlignEndPos =
                    (EndPos <= Reader->ReaderStarts[StartStream] ?
                     EndPos : Reader->ReaderStarts[StartStream]);

                uint32x const NumAlignBytes = AlignEndPos - CurrentPos;

                Copy(NumAlignBytes,
                     Reader->AlignmentBytes,
                     BufferAsBytes + BufferPos);

                BufferPos += NumAlignBytes;
                CurrentPos = AlignEndPos;
            }

            if (EndPos > Reader->ReaderStarts[StartStream])
            {
                uint32x const StreamEnd = (Reader->ReaderStarts[StartStream] +
                                           Reader->ReaderSizes[StartStream]);
                uint32x const ReadEndPos =
                    (EndPos <= StreamEnd ? EndPos : StreamEnd);
                uint32x const ReadBytes = ReadEndPos - CurrentPos;

                if (ReadAtMost(Reader->Readers[StartStream],
                               CurrentPos - Reader->ReaderStarts[StartStream],
                               ReadBytes,
                               BufferAsBytes + BufferPos) != (int32x)ReadBytes)
                {
                    Log0(ErrorLogMessage, FileReadingLogMessage, "Unable to read expected number of bytes from a sub-Reader");
                    return false;
                }

                BufferPos += ReadBytes;
                CurrentPos = ReadEndPos;
            }

            StartStream++;
        }
        else
        {
            // Handle the end.
            int32x const LastStreamPos = (Reader->ReaderStarts[Reader->NumReaders-1] +
                                          Reader->ReaderSizes[Reader->NumReaders-1]);
            Assert(int32x(Reader->TotalSize - LastStreamPos) < Reader->AlignmentByteCount);

            uint32x const RealEndPos = (EndPos <= Reader->TotalSize) ? EndPos : Reader->TotalSize;
            Assert(RealEndPos >= CurrentPos);
            uint32x const NumAlignBytes = RealEndPos - CurrentPos;

            Copy(NumAlignBytes,
                 Reader->AlignmentBytes,
                 BufferAsBytes + BufferPos);

            BufferPos += NumAlignBytes;
            CurrentPos = RealEndPos;
            break;
        }
    }
    Assert(BufferPos == (CurrentPos - FilePosition));

    return BufferPos;
}

static CALLBACK_FN(bool)
CatenatingGetReaderSize(file_reader* ReaderInit,
                        int32x*      SizeVar)
{
    CheckPointerNotNull(SizeVar, return false);

    catenating_reader* Reader = ObtainCatenatingReader(ReaderInit);
    CheckPointerNotNull(Reader, return 0);
    *SizeVar = Reader->TotalSize;

    return true;
}


static void
AggrCatenatingReader(aggr_allocator&     Allocator,
                     catenating_reader*& Reader,
                     int32x              NumReaders,
                     int32x              Alignment)
{
    AggrAllocPtr(Allocator, Reader);

    AggrAllocOffsetArrayPtr(Allocator, Reader, NumReaders, NumReaders, Readers);
    AggrAllocOffsetCountlessArrayPtr(Allocator, Reader, NumReaders, ReaderSizes);
    AggrAllocOffsetCountlessArrayPtr(Allocator, Reader, NumReaders, ReaderStarts);

    AggrAllocOffsetArrayPtr(Allocator, Reader, Alignment, AlignmentByteCount, AlignmentBytes);
}


file_reader* GRANNY
CreateCatenatingReader(int32x        NumReaders,
                       file_reader** Readers,
                       int32x        Alignment)
{
    CheckCondition(NumReaders > 0, return 0);
    CheckPointerNotNull(Readers, return 0);
    {for(int32x Idx = 0; Idx < NumReaders; ++Idx)
    {
        CheckPointerNotNull(Readers[Idx], return 0);
    }}

    CheckCondition(Alignment >= 0, return 0);
    if (Alignment == 0)
        Alignment = 1;

    catenating_reader* Reader = 0;
    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    AggrCatenatingReader(Allocator, Reader, NumReaders, Alignment);
    if (EndAggrAlloc(Allocator, AllocationUnknown))
    {
        uint32x CurrentStart = 0;
        {for(int32x ReaderIdx = 0; ReaderIdx < NumReaders; ++ReaderIdx)
        {
            Reader->Readers[ReaderIdx] = Readers[ReaderIdx];

            int32x ReaderSize;
            if (!GetReaderSize(Readers[ReaderIdx], &ReaderSize))
            {
                Log0(ErrorLogMessage, FileReadingLogMessage,
                     "Unable to obtain reader size in CatenatingReader");
                Deallocate(Reader);
                return 0;
            }

            Reader->ReaderSizes[ReaderIdx]  = ReaderSize;
            Reader->ReaderStarts[ReaderIdx] = CurrentStart;
            CurrentStart = (uint32x)AlignN(CurrentStart + ReaderSize, Alignment);
        }}
        Reader->TotalSize = CurrentStart;

        ZeroArray(Reader->AlignmentByteCount, Reader->AlignmentBytes);
        InitializeFileReader(CatenatingCloseFileReader,
                             CatenatingReadAtMost,
                             CatenatingGetReaderSize,
                             Reader->Base);
        Reader->MagicNumber = CatenatingReaderMagic;
    }

    return (file_reader*)Reader;
}

