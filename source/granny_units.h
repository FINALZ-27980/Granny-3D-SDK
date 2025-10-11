#if !defined(GRANNY_UNITS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_units.h $
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

real32 MetersPerSecondFromMilesPerHour(real32 MilesPerHour);
real32 MetersFromFeet(real32 Feet);

real32 RadiansFromDegrees(real32 Degrees);
real32 DegreesFromRadians(real32 Radians);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_UNITS_H
#endif
