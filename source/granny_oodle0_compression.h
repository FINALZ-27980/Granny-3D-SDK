#if !defined(GRANNY_OODLE0_COMPRESSION_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_oodle0_compression.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

// CompressedBytes must be padded at the end by at least the number of
// bytes returned by GetOodle0DecompressBufferPaddingSize(void) or
// the compressor will read-fault off the end.
int32x GetOodle0DecompressBufferPaddingSize(void);
bool Oodle0Decompress(bool FileIsByteReversed,
                      int32x CompressedBytesSize,
                      void *CompressedBytes,
                      int32x Stop0, int32x Stop1, int32x Stop2,
                      void *DecompressedBytes);
bool Oodle0Decompress(bool FileIsByteReversed,
                      int32x CompressedBytesSize,
                      void *CompressedBytes,
                      int32x DecompressedSize,
                      void *DecompressedBytes);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_OODLE0_COMPRESSION_H
#endif
