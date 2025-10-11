#if !defined(GRANNY_LIMITS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_limits.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(LimitsGroup);

// These are hard limits
#define MaximumSystemFileNameSize (1 << 9) EXPMACRO
#define MaximumTempFileNameSize (1 << 9) EXPMACRO
#define MaximumMessageBufferSize (1 << 10) EXPMACRO
#define MaximumLogFileNameSize (1 << 9) EXPMACRO
#define MaximumNumberToStringBuffer (1 << 8) EXPMACRO
#define MaximumIKLinkCount (1 << 5) EXPMACRO
#define MaximumMIPLevelCount (1 << 5) EXPMACRO
#define MaximumVertexNameLength (1 << 6) EXPMACRO
#define MaximumUserData (1 << 2) EXPMACRO
#define MaximumBoneNameLength (1 << 9) EXPMACRO

#define MaximumChannelCount (1 << 8) EXPMACRO
#define MaximumChannelWidth (1 << 2) EXPMACRO

#define MaximumBSplineDimension 16 EXPMACRO

#define MaximumBSplineKnotPower 16 EXPMACRO

// Fixed allocators use a slightly larger alignment
#define DefaultFixedAllocatorBlockAlignment 16


// These are default counts for block allocators
#define BlockFileFixupCount (1 << 8) EXPMACRO
#define ExpectedUsablePageSize (4000) EXPMACRO
#define MinimumAllocationsPerFixedBlock (1 << 6) EXPMACRO

// These are default buffer sizes
#define FileCopyBufferSize (1 << 16) EXPMACRO
#define CRCCheckBufferSize (1 << 16) EXPMACRO

#define DefaultFileDiskAlignment 4 EXPMACRO

// These are floating point control constants
#define TrackWeightEpsilon 0.001f EXPMACRO

#define PositionIdentityThreshold 0.001f EXPMACRO
#define OrientationIdentityThreshold 0.0001f EXPMACRO
#define ScaleShearIdentityThreshold 0.001f EXPMACRO

#define BlendEffectivelyZero 0.001f EXPMACRO
#define BlendEffectivelyOne  0.999f EXPMACRO

// Remember some people can run their game logic at speeds of 1000Hz.
#define TimeEffectivelyZero 0.00001f EXPMACRO

#define MaximumSectionCount 32 EXPMACRO

// Some default values.

#define DefaultLocalPoseFillThreshold 0.2f EXPMACRO


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_LIMITS_H
#endif
