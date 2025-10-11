// ========================================================================
// $File: //jeffr/granny_29/rt/granny_file_compressor.cpp $
// $DateTime: 2012/09/24 14:04:06 $
// $Change: 39522 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_file_compressor.h"

#include "granny_aggr_alloc.h"
#include "granny_file_operations.h"
#include "granny_file_reader.h"
#include "granny_file_writer.h"
#include "granny_log.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_oodle0_compression.h"
#include "granny_oodle1_compression.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

struct no_compression_state
{
    int32x CompressedSize;
};

struct file_compressor
{
    compression_type Type;
    file_writer *Writer;
    bool32 WritingForReversedPlatform;

    union
    {
        no_compression_state NoCompressionState;
        oodle1_state *Oodle1State;
    };
};

END_GRANNY_NAMESPACE;

int32x GRANNY
GetCompressedBytesPaddingSize(compression_type Format)
{
    switch(Format)
    {
        case Oodle0Compression:
        case Oodle1Compression:
        {
            return(4);
        } //break;

        default:
        {
            return(0);
        } //break;
    }
}

bool GRANNY
DecompressData(compression_type Format,
               bool FileIsByteReversed,
               int32x CompressedBytesSize,
               void *CompressedBytes,
               int32x Stop0, int32x Stop1, int32x Stop2,
               void *DecompressedBytes)
{
    bool Result = false;

    switch(Format)
    {
        case NoCompression:
        {
            Copy(Stop2, CompressedBytes, DecompressedBytes);
            Result = true;
        } break;

        case Oodle0Compression:
        {
            Result = Oodle0Decompress(FileIsByteReversed,
                                      CompressedBytesSize, CompressedBytes,
                                      Stop0, Stop1, Stop2, DecompressedBytes);
        } break;

        case Oodle1Compression:
        {
            Result = Oodle1Decompress(FileIsByteReversed,
                                      CompressedBytesSize, CompressedBytes,
                                      Stop0, Stop1, Stop2, DecompressedBytes);
        } break;

        default:
        {
            Log1(ErrorLogMessage, CompressorLogMessage,
                 "Unrecognized compression type %d", Format);
        } break;
    }

    return Result;
}

bool GRANNY
DecompressDataChunk(compression_type Format,
                    bool FileIsByteReversed,
                    int32x CompressedBytesSize,
                    void *CompressedBytes,
                    int32x DecompressedBytesSize,
                    void *DecompressedBytes)
{
    bool Result = false;

    switch(Format)
    {
        case NoCompression:
        {
            Copy(DecompressedBytesSize, CompressedBytes, DecompressedBytes);
            Result = true;
        } break;

        case Oodle0Compression:
        {
            Result = Oodle0Decompress(FileIsByteReversed,
                                      CompressedBytesSize, CompressedBytes,
                                      DecompressedBytesSize, DecompressedBytes);
        } break;

        case Oodle1Compression:
        {
            Result = Oodle1Decompress(FileIsByteReversed,
                                      CompressedBytesSize, CompressedBytes,
                                      DecompressedBytesSize, DecompressedBytes);
        } break;

        default:
        {
            Log1(ErrorLogMessage, CompressorLogMessage,
                 "Unrecognized compression type %d", Format);
        } break;
    }

    return(Result);
}

file_compressor *GRANNY
BeginFileCompression(uint32x ExpandedDataSize,
                     int32x ContentsCount,
                     compression_type Type,
                     bool WritingForReversedPlatform,
                     file_writer *Writer)
{
    file_compressor *Compressor = 0;

    switch(Type)
    {
        case NoCompression:
        {
            aggr_allocator Allocator;
            InitializeAggrAlloc(Allocator);

            AggrAllocPtr(Allocator, Compressor);
            if(EndAggrAlloc(Allocator, AllocationTemporary))
            {
                Compressor->NoCompressionState.CompressedSize = 0;
            }
        } break;

        case Oodle0Compression:
        {
            Assert(!"Oodle0 compression no longer supported, only decompression");
        } break;

        case Oodle1Compression:
        {
            oodle1_state *Oodle1State;

            aggr_allocator Allocator;
            InitializeAggrAlloc(Allocator);

            AggrAllocPtr(Allocator, Compressor);
            AggrOodle1(Allocator, Oodle1State, ExpandedDataSize, ContentsCount);
            if(EndAggrAlloc(Allocator, AllocationTemporary))
            {
                Compressor->Oodle1State = Oodle1State;
                Oodle1Begin(*Compressor->Oodle1State, ContentsCount);
            }
        } break;

        default:
        {
            Log1(ErrorLogMessage, CompressorLogMessage,
                 "Unrecognized compression type %d", Type);
        } break;
    }

    if(Compressor)
    {
        Compressor->Type = Type;
        Compressor->Writer = Writer;
        Compressor->WritingForReversedPlatform = WritingForReversedPlatform;
    }

    return(Compressor);
}

bool GRANNY
CompressContentsOfReader(file_compressor &Compressor,
                         int32x FileSize,
                         file_reader &Reader)
{
    bool Result = false;

    switch(Compressor.Type)
    {
        case NoCompression:
        {
            uint32x CompressedSize = 0;
            Result = ConcatenateFileReader(Compressor.Writer, &Reader, &CompressedSize);
            Compressor.NoCompressionState.CompressedSize += CompressedSize;
        } break;

        case Oodle0Compression:
        {
            Assert(!"Oodle0 compression no longer supported, only decompression");
        } break;

        case Oodle1Compression:
        {
            int32x BufferPadding =
                GetOodle1CompressBufferPaddingSize();

            int32x FilePosition = 0;
            uint8* Buffer = (uint8*)AllocateSize(FileSize + BufferPadding, AllocationTemporary);
            if(Buffer && ReadExactly(&Reader, FilePosition, FileSize, Buffer))
            {
                // Ensure that the compression bytes are zeroed
                Zero(BufferPadding, Buffer + FileSize);

                FilePosition += FileSize;
                Oodle1Compress(*Compressor.Oodle1State,
                               FileSize, Buffer);
                Result = true;
            }
            Deallocate(Buffer);
        } break;

        default:
        {
            Log1(ErrorLogMessage, CompressorLogMessage,
                 "Unrecognized compression type %d", Compressor.Type);
        };
    }

    return(Result);
}


bool GRANNY
CompressContentsOfFile(file_compressor &Compressor,
                       int32x FileSize,
                       char const *FileName)
{
    bool Result = false;

    switch(Compressor.Type)
    {
        case NoCompression:
        {
            uint32x CompressedSize = 0;
            Result = ConcatenateFile(Compressor.Writer, FileName, &CompressedSize);
            Compressor.NoCompressionState.CompressedSize += CompressedSize;
        } break;

        case Oodle0Compression:
        {
            Assert(!"Oodle0 compression no longer supported, only decompression");
        } break;

        case Oodle1Compression:
        {
            file_reader *Reader = OpenFileReader(FileName);
            if(Reader)
            {
                Result = CompressContentsOfReader(Compressor, FileSize, *Reader);
                CloseFileReader(Reader);
            }
        } break;

        default:
        {
            Log1(ErrorLogMessage, CompressorLogMessage,
                 "Unrecognized compression type %d", Compressor.Type);
        };
    }

    return(Result);
}


bool GRANNY
CompressContentsOfMemory(file_compressor &Compressor,
                         int32x BufferSize, void const *Buffer)
{
    bool Result = false;

    switch(Compressor.Type)
    {
        case NoCompression:
        {
            Result = WriteBytes(Compressor.Writer, BufferSize, Buffer);
            Compressor.NoCompressionState.CompressedSize += BufferSize;
        } break;

        case Oodle0Compression:
        {
            Assert(!"Oodle0 compression no longer supported, only decompression");
        } break;

        case Oodle1Compression:
        {
            // We assume that the padding has been added here...
            Oodle1Compress(*Compressor.Oodle1State, BufferSize, Buffer);
            Result = true;
        } break;

        default:
        {
            Log1(ErrorLogMessage, CompressorLogMessage,
                 "Unrecognized compression type %d", Compressor.Type);
        };
    }

    return(Result);
}



bool GRANNY
EndFileCompression(file_compressor *Compressor,
                   uint32x &CompressedSize)
{
    bool Result = false;
    CompressedSize = 0;

    if(Compressor)
    {
        switch(Compressor->Type)
        {
            case NoCompression:
            {
                Result = true;
                CompressedSize =
                    Compressor->NoCompressionState.CompressedSize;
            } break;

            case Oodle0Compression:
            {
                Assert(!"Oodle0 compression no longer supported, only decompression");
            } break;

            case Oodle1Compression:
            {
                void *Buffer;
                CompressedSize = Oodle1End(*Compressor->Oodle1State, Buffer,
                                           Bool32(Compressor->WritingForReversedPlatform));
                Result = WriteBytes(Compressor->Writer, CompressedSize, Buffer);
            } break;

            default:
            {
                Log1(ErrorLogMessage, CompressorLogMessage,
                     "Unrecognized compression type %d", Compressor->Type);
            };
        }

    }

    Deallocate(Compressor);

    return(Result);
}
