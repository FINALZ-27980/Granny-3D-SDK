#if !defined(GRANNY_FILE_COMPRESSOR_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_file_compressor.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(FileCompressionGroup);

struct file_writer;
struct file_reader;

EXPTYPE enum compression_type
{
    NoCompression,
    Oodle0Compression,
    Oodle1Compression,
    OnePastLastCompressionType,

    compression_type_forceint = 0x7fffffff
};

EXPTYPE struct file_compressor;

EXPAPI GS_SAFE int32x GetCompressedBytesPaddingSize(compression_type Format);
EXPAPI GS_PARAM bool DecompressData(compression_type Format,
                                    bool FileIsByteReversed,
                                    int32x CompressedBytesSize,
                                    void *CompressedBytes,
                                    int32x Stop0, int32x Stop1, int32x Stop2,
                                    void *DecompressedBytes);
EXPAPI GS_PARAM bool DecompressDataChunk(compression_type Format,
                                         bool FileIsByteReversed,
                                         int32x CompressedBytesSize,
                                         void *CompressedBytes,
                                         int32x DecompressedBytesSize,
                                         void *DecompressedBytes);

EXPAPI GS_PARAM file_compressor *BeginFileCompression(uint32x ExpandedDataSize,
                                                      int32x ContentsCount,
                                                      compression_type Type,
                                                      bool WritingForReversedPlatform,
                                                      file_writer *Writer);

EXPAPI GS_MODIFY bool CompressContentsOfFile(file_compressor &Compressor,
                                             int32x FileSize,
                                             char const *FileName);
EXPAPI GS_PARAM bool CompressContentsOfReader(file_compressor &Compressor,
                                              int32x FileSize,
                                              file_reader &Reader);
EXPAPI GS_PARAM bool CompressContentsOfMemory(file_compressor &Compressor,
                                              int32x BufferSize,
                                              void const *Buffer);

EXPAPI GS_PARAM bool EndFileCompression(file_compressor *Compressor,
                                        uint32x &CompressedSize);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_FILE_COMPRESSOR_H
#endif
