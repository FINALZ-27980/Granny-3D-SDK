#if !defined(GRANNY_QUATERNION_SCALEOFFSET_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_quaternion_scaleoffset.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

#define QUATERNION_SCALE_ENTRY(Index, Entry) (QuaternionCurveScaleOffsetTable[(Index)*2 + (Entry)])
extern ALIGN16(real32) const QuaternionCurveScaleOffsetTable[16*2];

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_QUATERNION_SCALEOFFSET_H
#endif
