// ========================================================================
// $File: //jeffr/granny_29/rt/granny_units.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_units.h"

#include "granny_assert.h"
#include "granny_constants.h"
#include "granny_parameter_checking.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode Undefined_LogMessage
USING_GRANNY_NAMESPACE;

real32 GRANNY
MetersPerSecondFromMilesPerHour(real32 MilesPerHour)
{
    return(MilesPerHour * 0.447041f);
}

real32 GRANNY
MetersFromFeet(real32 Feet)
{
    return(Feet * 0.3048f);
}

real32 GRANNY
RadiansFromDegrees(real32 Degrees)
{
    return(Degrees * Pi32 / 180.0f);
}

real32 GRANNY
DegreesFromRadians(real32 Radians)
{
    return(Radians * 180.0f / Pi32);
}


