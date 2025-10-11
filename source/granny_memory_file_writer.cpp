// ========================================================================
// $File: //jeffr/granny_29/rt/granny_memory_file_writer.cpp $
// $DateTime: 2012/04/24 15:33:15 $
// $Change: 37118 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_memory_file_writer.h"

#include "granny_crc.h"
#include "granny_file_writer.h"
#include "granny_log.h"
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

#define SubsystemCode FileWritingLogMessage

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

struct memory_file_writer
{
    file_writer Base;

    // For now, implemented as a flat growable array, may be
    //  better to implement as a list of blocks, given that
    //  we don't really want to use the stl style 2x growth
    //  method
    bool32 StolenBuffer;
    uint8* Buffer;
    int32x Capacity;
    int32x BlockSize;

    int32x CurrentPosition;
    int32x CurrentSize;
};
#if !PLATFORM_ANDROID
  CompileAssert(OffsetFromType(memory_file_writer, Base) == 0);
#else
  // For some reason, android doesn't like these at file scope in pepper 18
  static void TestFn()
  {
      CompileAssert(OffsetFromType(memory_file_writer, Base) == 0);
  }
#endif

END_GRANNY_NAMESPACE;

static bool
GrowWriterToSize(memory_file_writer& Writer,
                 int32x NewSize)
{
    Assert(NewSize > 0);
    Assert(Writer.BlockSize > 0);
    Assert(NewSize > Writer.CurrentSize);
    Assert(NewSize > Writer.Capacity);

    // We allow non-pow2 block sizes, so no (& ~(BS - 1)) trick
    // allowed.
    int32x const RoundedCapacity = ((NewSize + Writer.BlockSize - 1) / Writer.BlockSize) * Writer.BlockSize;
    Assert(RoundedCapacity > Writer.Capacity);
    Assert(RoundedCapacity >= NewSize);

    bool Result = false;
    uint8* NewBuffer = (uint8*)AllocateSize(RoundedCapacity, AllocationUnknown);
    if (NewBuffer)
    {
        Copy(Writer.CurrentSize, Writer.Buffer, NewBuffer);

        Deallocate(Writer.Buffer);
        Writer.Buffer = NewBuffer;
        Writer.CurrentSize = NewSize;
        Writer.Capacity = RoundedCapacity;
        Result = true;
    }

    return Result;
}

static CALLBACK_FN(void)
MemoryDeleteWriter(file_writer *Writer)
{
    memory_file_writer *MemoryWriter = (memory_file_writer*)Writer;
    if(Writer)
    {
        Assert(!MemoryWriter->Base.CRCing);

        // Perfectly ok for this to be null in certain circumstances, i.e, if
        //  someone has taken possession of the buffer, or no bytes were
        //  written
        if (MemoryWriter->Buffer != NULL)
        {
            Deallocate(MemoryWriter->Buffer);
            MemoryWriter->Buffer = NULL;
        }

        Deallocate(Writer);
    }
}

static CALLBACK_FN(int32x)
MemorySeekWriter(file_writer* Writer,
                 int32x OffsetInUInt8s,
                 int32x SeekType)
{
    CheckPointerNotNull(Writer, return 0);
    memory_file_writer &MemoryWriter = *((memory_file_writer*)Writer);

    Assert(!MemoryWriter.Base.CRCing || (SeekType == SeekCurrent && OffsetInUInt8s == 0));
    Assert(!MemoryWriter.StolenBuffer);

    int32x TargetPosition;
    switch (SeekType)
    {
        case SeekStart:
            TargetPosition = OffsetInUInt8s;
            break;
        case SeekEnd:
            TargetPosition = MemoryWriter.CurrentSize - OffsetInUInt8s;
            break;
        case SeekCurrent:
            TargetPosition = MemoryWriter.CurrentPosition + OffsetInUInt8s;
            break;

        default:
            InvalidCodePath("Invalid seek type");
            return 0;
    }

    if (TargetPosition < 0)
    {
        Log1(ErrorLogMessage, FileWritingLogMessage,
             "Attempting to seek before beginning of a memory stream [r: %d]",
             TargetPosition);
        return 0;
    }
    else if (TargetPosition > MemoryWriter.CurrentSize)
    {
        // Increase capacity to handle the new position...
        if (!GrowWriterToSize(MemoryWriter, TargetPosition))
        {
            Log2(ErrorLogMessage, FileWritingLogMessage,
                 "Unable to grow memory stream to requested size [r: %d curr: %d]",
                 TargetPosition, MemoryWriter.Capacity);
            return 0;
        }
    }

    MemoryWriter.CurrentPosition = TargetPosition;
    return TargetPosition;
}

static CALLBACK_FN(bool)
MemoryWrite(file_writer* Writer, int32x UInt8Count, void const *WritePointer)
{
    CheckPointerNotNull(Writer, return 0);
    memory_file_writer &MemoryWriter = *((memory_file_writer*)Writer);

    Assert(!MemoryWriter.StolenBuffer);
    if(MemoryWriter.Base.CRCing)
    {
        // We're CRCing, so we need to update our current CRC value
        AddToCRC32(MemoryWriter.Base.CRC, UInt8Count, WritePointer);
    }

    const int32x ImpliedSize = MemoryWriter.CurrentPosition + UInt8Count;
    if (ImpliedSize > MemoryWriter.Capacity)
    {
        if (!GrowWriterToSize(MemoryWriter, ImpliedSize))
        {
            Log2(ErrorLogMessage, FileWritingLogMessage,
                 "Unable to grow memory stream to requested size [r: %d curr: %d]",
                 ImpliedSize, MemoryWriter.Capacity);
            return false;
        }
        Assert(MemoryWriter.Capacity    >= ImpliedSize);
        Assert(MemoryWriter.CurrentSize >= ImpliedSize);
    }
    else if (ImpliedSize > MemoryWriter.CurrentSize)
    {
        Assert(MemoryWriter.Capacity >= ImpliedSize);
        MemoryWriter.CurrentSize = ImpliedSize;
    }

    Copy(UInt8Count, WritePointer,
         MemoryWriter.Buffer + MemoryWriter.CurrentPosition);
    MemoryWriter.CurrentPosition += UInt8Count;
    Assert(MemoryWriter.CurrentPosition <= MemoryWriter.Capacity);
    Assert(MemoryWriter.CurrentPosition <= MemoryWriter.CurrentSize);

    return true;
}

static CALLBACK_FN(void)
MemoryBeginWriterCRC(file_writer* Writer)
{
    CheckPointerNotNull(Writer, return);
    memory_file_writer &MemoryWriter = *((memory_file_writer*)Writer);

    Assert(!MemoryWriter.StolenBuffer);
    Assert(!MemoryWriter.Base.CRCing);

    MemoryWriter.Base.CRCing = true;
    BeginCRC32(MemoryWriter.Base.CRC);
}

static CALLBACK_FN(uint32)
MemoryEndWriterCRC(file_writer* Writer)
{
    CheckPointerNotNull(Writer, return 0);
    memory_file_writer &MemoryWriter = *((memory_file_writer*)Writer);

    Assert(!MemoryWriter.StolenBuffer);
    Assert(MemoryWriter.Base.CRCing);

    MemoryWriter.Base.CRCing = false;
    EndCRC32(MemoryWriter.Base.CRC);

    return(MemoryWriter.Base.CRC);
}

file_writer* GRANNY
CreateMemoryFileWriter(int32x BlockSize)
{
    CheckCondition(BlockSize > 0, return 0);

    memory_file_writer *NewWriter = Allocate(memory_file_writer, AllocationUnknown);
    if(NewWriter)
    {
        InitializeFileWriter(MemoryDeleteWriter,
                             MemorySeekWriter,
                             MemoryWrite,
                             MemoryBeginWriterCRC,
                             MemoryEndWriterCRC,
                             NewWriter->Base);
        NewWriter->StolenBuffer    = false;
        NewWriter->Buffer          = NULL;
        NewWriter->Capacity        = 0;
        NewWriter->BlockSize       = BlockSize;
        NewWriter->CurrentPosition = 0;
        NewWriter->CurrentSize     = 0;

    }
    else
    {
        Log0(ErrorLogMessage, FileWritingLogMessage, "Unable to allocate a memory writer...");
    }

    return (file_writer*)NewWriter;
}

bool GRANNY
StealMemoryWriterBuffer(file_writer* Writer,
                        uint8**      BufferPtr,
                        int32x*      BufferSize)
{
    CheckPointerNotNull(Writer, return 0);
    memory_file_writer &MemoryWriter = *((memory_file_writer*)Writer);
    Assert(!MemoryWriter.StolenBuffer);

    CheckPointerNotNull(BufferPtr, return false);
    CheckPointerNotNull(BufferSize, return false);

    if (MemoryWriter.StolenBuffer)
    {
        *BufferPtr = NULL;
        *BufferSize = 0;
        return false;
    }

    *BufferPtr  = MemoryWriter.Buffer;
    *BufferSize = MemoryWriter.CurrentSize;

    MemoryWriter.Buffer = NULL;
    MemoryWriter.Capacity = 0;
    MemoryWriter.CurrentSize = 0;
    MemoryWriter.CurrentPosition = 0;
    MemoryWriter.StolenBuffer = true;

    return true;
}


void GRANNY
FreeMemoryWriterBuffer(uint8* Buffer)
{
    if (Buffer)
    {
        Deallocate(Buffer);
    }
}

