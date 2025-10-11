#if !defined(GRANNY_BINK0_COMPRESSION_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_bink0_compression.h $
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

void BinkTCCheckSizes0(uint32 *width, uint32 *height);

void FromBinkTC0(int16 **output,
                 uint32 planes,
                 void const * bink_buf,
                 uint32 width,
                 uint32 height,
                 void *temp,
                 uint32 temp_size);
uint32 FromBinkTCTempMem0(void const *binkbuf);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_BINK0_COMPRESSION_H
#endif
