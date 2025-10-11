#if !defined(GRANNY_CRC_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_crc.h $
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

EXPAPI GS_SAFE void BeginCRC32(uint32 &CRC);
EXPAPI GS_SAFE void AddToCRC32(uint32 &CRC, uint64x Count, void const *UInt8s);
EXPAPI GS_SAFE void EndCRC32(uint32 &CRC);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_CRC_H
#endif
