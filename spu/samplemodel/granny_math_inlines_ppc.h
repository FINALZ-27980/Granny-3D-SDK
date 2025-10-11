#if !defined(GRANNY_MATH_INLINES_PPC_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_math_inlines_ppc.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_CONVERSIONS_H)
#include "granny_conversions.h"
#endif

#if !defined(GRANNY_INTRINSICS_H)
#include "granny_intrinsics.h"
#endif

#if !PLATFORM_XENON && !(PLATFORM_PS3 && PROCESSOR_CELL)
#error "Only valid for PowerPC processors (not including SPU)"
#endif

#if PLATFORM_PS3
  #include <ppu_intrinsics.h>
#endif

BEGIN_GRANNY_NAMESPACE;

inline real32 FastInvSquareRootGreater0(real32 Value)
{
    double est = __frsqrte( Value );  // estimate
    est = est * 0.5 * ( 3.0 - est * est * Value ); //12
    est = est * 0.5 * ( 3.0 - est * est * Value ); //24
    est = est * 0.5 * ( 3.0 - est * est * Value ); //32

    return real32(est);
}

inline real32 FastSquareRootGreater0(real32 Value)
{
    return real32(FastInvSquareRootGreater0(Value) * Value);
}

inline real32 FastDivGreater0(real32 Numerator, real32 Den)
{
    const real64x InfFloat = 1.7976931348623158e+308 * 2.0;  // DBL_MAX * 2 = INF

    // Tested exhaustively: all positive floats, including
    // denorms.  No more than 2 iterations required for 1 ULP
    // precision.
#if COMPILER_GCC || COMPILER_MSVC
    double est = __fres( Den );  // estimate
#elif COMPILER_SNC
    double est = __builtin_fre(Den);
#else
#error "Unsupported compiler"
#endif
    est = (1 - est * Den) * est + est;
    est = (1 - est * Den) * est + est;

    // This handles denorms in the Den input.  Because of the
    // iteration, they come out as QNAN rather than Inf.  This
    // __fsel fixes that up.  We're accounting for a bug in the
    // debug compiler for 6132 here, __fsel mishandles NaNs in
    // non-optimized builds.
    //
    // NOTE!  Because of this fixup, we return INCORRECT values
    // for Denominators of Inf or NaN.  These divides result in
    // an Inf return, rather than 0 or NaN, respectively.
#if DEBUG
    return (est == est) ? real32(est * Numerator) : real32(InfFloat);
#else
    return real32(__fsel(est, est, InfFloat) * Numerator);
#endif
}

inline real32 LengthFactor4(real32* Vec)
{
    real32 LengthSq = VectorLengthSquared4(Vec);

    // Stupid compiler bug in 6132.  Debug versions of __fsel mishandle the NaN case,
    // which happens if the length is 0.
#if DEBUG
    if (LengthSq > 0.0f)
        return 1.0f / SquareRoot(LengthSq);
    else
        return 0.0f;
#else
    double est = __frsqrte( LengthSq );  // estimate
    est = est * 0.5 * ( 3.0 - est * est * LengthSq ); //12
    est = est * 0.5 * ( 3.0 - est * est * LengthSq ); //24
    est = est * 0.5 * ( 3.0 - est * est * LengthSq ); //32
    return (real32)__fsel(est, est, 0.0);
#endif
}

inline real32 LengthFactor3(real32* Vec)
{
    real32 LengthSq = VectorLengthSquared3(Vec);

    // Stupid compiler bug in 6132.  Debug versions of __fsel mishandle the NaN case,
    // which happens if the length is 0.
#if DEBUG
    if (LengthSq > 0.0f)
        return 1.0f / SquareRoot(LengthSq);
    else
        return 0.0f;
#else
    double est = __frsqrte( LengthSq );  // estimate
    est = est * 0.5 * ( 3.0 - est * est * LengthSq ); //12
    est = est * 0.5 * ( 3.0 - est * est * LengthSq ); //24
    est = est * 0.5 * ( 3.0 - est * est * LengthSq ); //32

    return (real32)__fsel(est, est, 0.0);
#endif
}

inline void Normalize4(real32* Dest)
{
    VectorScale4(Dest, (real32)LengthFactor4(Dest));
}

inline void Normalize3(real32 *Dest)
{
    VectorScale3(Dest, (real32)LengthFactor3(Dest));
}


inline void NormalizeWithTest4(real32 *Dest)
{
    real32 const Sum = (Dest[0] * Dest[0] +
                        Dest[1] * Dest[1] +
                        Dest[2] * Dest[2] +
                        Dest[3] * Dest[3]);

    if((Sum > 1.1f) || (Sum < 0.9f))
    {
        Normalize4(Dest);
    }
    else
    {
        real32 const Term = (3.0f - Sum);

        real32 const ApproximateOneOverRoot =
            0.0625f * (12.0f - Sum*Square(Term)) * Term;

        VectorScale4(Dest, ApproximateOneOverRoot);
    }
}



END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MATH_INLINES_PPC_H
#endif
