#if !defined(GRANNY_QUATERNION_MODE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_quaternion_mode.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(LocalPoseGroup);

// NB: These are stored in an int8 in the bound_transform_track, so don't add any values
// outside that range...
EXPTYPE enum quaternion_mode
{
    BlendQuaternionDirectly            = 0,
    BlendQuaternionInverted            = 1,
    BlendQuaternionNeighborhooded      = 2,
    BlendQuaternionAccumNeighborhooded = 3,

    quaternion_mode_forceint = 0x7fffffff
};

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_QUATERNION_MODE_H
#endif /* GRANNY_QUATERNION_MODE_H */
