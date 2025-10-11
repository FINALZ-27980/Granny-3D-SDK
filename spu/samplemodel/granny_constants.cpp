// ========================================================================
// $File: //jeffr/granny_29/rt/granny_constants.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_constants.h"

#include "granny_assert.h"
#include "granny_parameter_checking.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode Undefined_LogMessage
USING_GRANNY_NAMESPACE;

ALIGN16(real32) const GRANNY Pi32 = 3.14159265358979323846f;

ALIGN16(real32) const GRANNY GlobalZeroVector[4] = {0, 0, 0, 0};
ALIGN16(real32) const GRANNY GlobalXAxis[4] = {1, 0, 0, 0};
ALIGN16(real32) const GRANNY GlobalYAxis[4] = {0, 1, 0, 0};
ALIGN16(real32) const GRANNY GlobalZAxis[4] = {0, 0, 1, 0};
ALIGN16(real32) const GRANNY GlobalWAxis[4] = {0, 0, 0, 1};
ALIGN16(real32) const GRANNY GlobalNegativeXAxis[4] = {-1, 0, 0, 0};
ALIGN16(real32) const GRANNY GlobalNegativeYAxis[4] = {0, -1, 0, 0};
ALIGN16(real32) const GRANNY GlobalNegativeZAxis[4] = {0, 0, -1, 0};
ALIGN16(real32) const GRANNY GlobalNegativeWAxis[4] = {0, 0, 0, -1};
ALIGN16(real32) const GRANNY GlobalIdentity3x3[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
ALIGN16(real32) const GRANNY GlobalIdentity4x4[4][4] = {{1, 0, 0, 0},
                                                 {0, 1, 0, 0},
                                                 {0, 0, 1, 0},
                                                 {0, 0, 0, 1}};
ALIGN16(real32) const GRANNY GlobalIdentity6[6] = {1, 0, 0, 1, 0, 1};
ALIGN16(real32) const GRANNY GlobalZero16[16] = {0};
