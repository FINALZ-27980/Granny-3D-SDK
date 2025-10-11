#if !defined(GRANNY_CONSTANTS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_constants.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(MathGroup);

// Constants
extern ALIGN16(real32) const Pi32;

extern ALIGN16(real32) const GlobalZeroVector[4];
extern ALIGN16(real32) const GlobalXAxis[4];
extern ALIGN16(real32) const GlobalYAxis[4];
extern ALIGN16(real32) const GlobalZAxis[4];
extern ALIGN16(real32) const GlobalWAxis[4];
extern ALIGN16(real32) const GlobalNegativeXAxis[4];
extern ALIGN16(real32) const GlobalNegativeYAxis[4];
extern ALIGN16(real32) const GlobalNegativeZAxis[4];
extern ALIGN16(real32) const GlobalNegativeWAxis[4];
extern ALIGN16(real32) const GlobalIdentity3x3[3][3];
extern ALIGN16(real32) const GlobalIdentity4x4[4][4];

extern ALIGN16(real32) const GlobalIdentity6[6];
extern ALIGN16(real32) const GlobalZero16[16];

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_CONSTANTS_H
#endif
