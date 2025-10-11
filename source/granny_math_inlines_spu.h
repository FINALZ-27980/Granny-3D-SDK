#if !defined(GRANNY_MATH_INLINES_SPU_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_math_inlines_spu.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_CONVERSIONS_H)
#include "granny_conversions.h"
#endif

#if !defined(GRANNY_INTRINSICS_H)
#include "granny_intrinsics.h"
#endif

#if !PROCESSOR_CELL_SPU
#error "Only valid for SPU"
#endif

#include <vectormath/cpp/vectormath_aos.h>
#include <spu_intrinsics_gcc.h>

BEGIN_GRANNY_NAMESPACE;

static inline vector float VecInvSquareRootFast(vector float Value)
{
    vector float Half   = spu_splats(0.5f);
    vector float Three  = spu_splats(3.0f);

    vector float Vec = spu_rsqrte(Value);
    Vec = Vec * Half * ( Three - Vec * Vec * Value ); //12
    Vec = Vec * Half * ( Three - Vec * Vec * Value ); //24

    return Vec;
}

inline vector float VecFastSquareRootGreater0(vector float Value)
{
    return Value * VecInvSquareRootFast(Value);
}


static inline vector float InvSquareRootFast(real32 Value)
{
    return VecInvSquareRootFast(spu_splats(Value));
}


inline real32 FastInvSquareRootGreater0(real32 Value)
{
    return InvSquareRootFast(Value)[0];
}

inline real32 FastSquareRootGreater0(real32 Value)
{
    return VecFastSquareRootGreater0(spu_splats(Value))[0];
}

inline real32 FastDivGreater0(real32 Numerator, real32 Den)
{
    // Tested exhaustively: all positive floats, including
    // denorms.  No more than 2 iterations required for 1 ULP
    // precision.
    vector float DenVec = spu_splats(Den);
    vector float One    = spu_splats(1.0f);

    vector float est = spu_re(DenVec);
    est = (One - est * DenVec) * est + est;
    est = (One - est * DenVec) * est + est;

    return est[0] * Numerator;
}

inline void Normalize4(real32* Dest)
{
    vector float v = { Dest[0], Dest[1], Dest[2], Dest[3] };
    Vectormath::Aos::Vector4 dotv(Dest[0], Dest[1], Dest[2], Dest[3]);
    vector float scale = InvSquareRootFast(Vectormath::Aos::dot(dotv, dotv));

    vector float normVec = v * scale;
    Dest[0] = normVec[0];
    Dest[1] = normVec[1];
    Dest[2] = normVec[2];
    Dest[3] = normVec[3];
}

inline void NormalizeWithTest4(real32 *Dest)
{
    Normalize4(Dest);
}


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MATH_INLINES_SPU_H
#endif
