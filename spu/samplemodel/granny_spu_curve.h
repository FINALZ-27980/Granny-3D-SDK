#if !defined(GRANNY_SPU_CURVE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_spu_curve.h $
// $DateTime: 2012/09/07 12:25:44 $
// $Change: 39201 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(SPUGroup);

struct sample_context;

EXPTYPE enum spu_curve_types
{
    SPUCurveTypeReal32 = 0,
    SPUCurveTypeK16    = 1,
    SPUCurveTypeK8     = 2,
    SPUCurveType4nK16  = 3,
    SPUCurveType4nK8   = 4,

    spu_curve_types_forceint = 0x7fffffff
};

EXPTYPE enum spu_replication_type
{
    SPUReplicationRaw        = 0,
    SPUReplicationNormOri    = 1,
    SPUReplicationDiagonalSS = 2,

    spu_replication_type_forceint = 0x7fffffff
};

EXPTYPE enum spu_constants
{
    MaxSPUBones = 160,
    MaxSPUTransformTrackSize = (32 << 10),

    spu_constants_forceint = 0x7fffffff
};

// Used for all spu curves
EXPTYPE struct spu_curve_header_basic
{
    uint16 TotalSize;        // 2 bytes (Must be a multiple of 16 bytes)
    uint16 CurveType;        // 4 bytes (One of the spu_curve_types above)
    uint8  Degree;           // 5 bytes
    uint8  Dimension;        // 6 bytes
    uint16 ReplicationType;  // 8 bytes
    uint16 KnotCount;        // 10 bytes
    uint16 KnotStart;        // 12 bytes
    uint16 ControlStart;     // 14 bytes
    uint16 ControlStride;    // 16 bytes
};

EXPTYPE struct spu_curve_header_quantized
{
    spu_curve_header_basic BasicHeader;
    real32                 OneOverKnotScale;
    // ScaleOffset entries follow directly after this structure
};


EXPTYPE struct spu_curve_header_quantized4n
{
    spu_curve_header_basic BasicHeader;
    int32                  ScaleOffsetTableEntries;
    real32                 OneOverKnotScale;
};


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_SPU_CURVE_H
#endif
