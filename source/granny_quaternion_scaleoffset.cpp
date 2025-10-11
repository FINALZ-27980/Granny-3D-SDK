// ========================================================================
// $File: //jeffr/granny_29/rt/granny_quaternion_scaleoffset.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_quaternion_scaleoffset.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

#define OneOverSqrt2 (0.707106781f)

// Which components of this table are and are not included was found very much by trial & error!
// Don't change it, or old stuff will break horribly :-)
ALIGN16(real32) const QuaternionCurveScaleOffsetTable[16 * 2] =
{
    OneOverSqrt2 * 2.0f,  -OneOverSqrt2,
    OneOverSqrt2 * 1.0f,  -OneOverSqrt2 * 0.5f,
    OneOverSqrt2 * 0.5f,  -OneOverSqrt2 * 0.75f,
    OneOverSqrt2 * 0.5f,  -OneOverSqrt2 * 0.25f,

    OneOverSqrt2 * 0.5f,   OneOverSqrt2 * 0.25f,
    OneOverSqrt2 * 0.25f, -OneOverSqrt2 * 0.250f,
    OneOverSqrt2 * 0.25f, -OneOverSqrt2 * 0.125f,
    OneOverSqrt2 * 0.25f,  OneOverSqrt2 * 0.000f,

    // And the same table, negated.
    -OneOverSqrt2 * 2.0f,   OneOverSqrt2,
    -OneOverSqrt2 * 1.0f,   OneOverSqrt2 * 0.5f,
    -OneOverSqrt2 * 0.5f,   OneOverSqrt2 * 0.75f,
    -OneOverSqrt2 * 0.5f,   OneOverSqrt2 * 0.25f,
    -OneOverSqrt2 * 0.5f,  -OneOverSqrt2 * 0.25f,
    -OneOverSqrt2 * 0.25f,  OneOverSqrt2 * 0.250f,
    -OneOverSqrt2 * 0.25f,  OneOverSqrt2 * 0.125f,
    -OneOverSqrt2 * 0.25f, -OneOverSqrt2 * 0.000f
};

END_GRANNY_NAMESPACE;
