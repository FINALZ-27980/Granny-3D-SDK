// ========================================================================
// $File: //jeffr/granny_29/rt/granny_quaternion_scaleoffset.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_quaternion_scaleoffset.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

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
