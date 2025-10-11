#if !defined(GRANNY_CURVE_FAST_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_curve_fast.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

// Set to 0 to use the old paths.
#define USE_FAST_CURVE_SAMPLERS 1

BEGIN_GRANNY_NAMESPACE;

struct curve2;

void CurveExtractKnotValuesD4nK8uC7u(curve2 const &Curve,
                                     int32x KnotIndex,
                                     int32x KnotCount,
                                     real32* NOALIAS KnotResults,
                                     real32* NOALIAS ControlResults,
                                     real32 const* NOALIAS IdentityVector);

void CurveExtractKnotValuesD4nK16uC15u(curve2 const &Curve,
                                       int32x KnotIndex,
                                       int32x KnotCount,
                                       real32* NOALIAS KnotResults,
                                       real32* NOALIAS ControlResults,
                                       real32 const* NOALIAS IdentityVector );

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_CURVE_FAST_H
#endif
