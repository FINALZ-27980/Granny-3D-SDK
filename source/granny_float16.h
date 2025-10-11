#if !defined(GRANNY_FLOAT16_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_float16.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// Conversions to and from the OpenEXR float16 or "half" format.  This is
// IEEE compliant floating point format with 1 sign bit, 5 exponent bits,
// and 10 bits of mantissa.
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(MathGroup);

EXPAPI GS_SAFE real16 Real32ToReal16(real32 Value);
EXPAPI GS_SAFE void   Real16ToReal32(real16 Value, real32* Output);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_FLOAT16_H
#endif

