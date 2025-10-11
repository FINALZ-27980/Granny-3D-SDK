#if !defined(GRANNY_MATH_INLINES_PPC_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_math_inlines_wii.h $
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

#if !PLATFORM_WII
#error "Only valid for Gekko processor"
#endif

BEGIN_GRANNY_NAMESPACE;

inline real32 FastInvSquareRootGreater0(real32 register Value)
{
    const real64x InfFloat = 1.7976931348623158e+308 * 2.0;  // DBL_MAX * 2 = INF

    register double est;
    register double temp0, temp1;

    register double half  = 0.5;
    register double three = 3.0;
    asm
    {
        frsqrte est, Value;

        fmul    temp0, est, est;             // est * est
        fmul    temp1, est, half;            // est/2
        fnmsub  temp0, temp0, Value, three;  // (3 - est*est*Value)
        fmul    est, temp0, temp1;           // (est/2)(3 - est*est*Value)

        fmul    temp0, est, est;             // est * est
        fmul    temp1, est, half;            // est/2
        fnmsub  temp0, temp0, Value, three;  // (3 - est*est*Value)
        fmul    est, temp0, temp1;           // (est/2)(3 - est*est*Value)
    }

    return real32(est);
}

inline real32 FastSquareRootGreater0(real32 Value)
{
    return real32(FastInvSquareRootGreater0(Value) * Value);
}

inline real32 FastDivGreater0(real32 register Numerator,
                              real32 register Den)
{
    // In some real-world tests, the estimate/refine seems to be more or less a wash.  The
    // gain here doesn't seem to be worth the difference in the way denorms, infs, etc are
    // potentially handled.
    return Numerator / Den;

    // // Tested exhaustively: all positive floats, including denorms out to 8e37.  No more
    // // than 1 iterations required for 1 ULP precision.  After around 8e37 you run into
    // // trouble because the result is required to be a denorm.

    // register double est;
    // register double temp0, temp1;
    // asm
    // {
    //     fres        est, Den;

    //     fadd      temp0, est, est;          // temp0 = e+e
    //     fmul      temp1, est, est;          // temp1 = e*e
    //     fnmsub    est,   Den, temp1, temp0; // est' = 2e - Den*e*e
    // }

    // return est * Numerator;
}

inline real32 LengthFactor4(real32* Vec)
{
    // TODO
    real32 LengthSq = VectorLengthSquared4(Vec);
    return FastInvSquareRootGreater0(LengthSq);

//     // Stupid compiler bug in 6132.  Debug versions of __fsel mishandle the NaN case,
//     // which happens if the length is 0.
// #if DEBUG
//     if (LengthSq > 0.0f)
//         return 1.0f / SquareRoot(LengthSq);
//     else
//         return 0.0f;
// #else
//     double est = __frsqrte( LengthSq );  // estimate
//     est = est * 0.5 * ( 3.0 - est * est * LengthSq ); //12
//     est = est * 0.5 * ( 3.0 - est * est * LengthSq ); //24
//     est = est * 0.5 * ( 3.0 - est * est * LengthSq ); //32
//     return (real32)__fsel(est, est, 0.0);
// #endif
}

inline real32 LengthFactor3(real32* Vec)
{
    // TODO
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

        Dest[0] *= ApproximateOneOverRoot;
        Dest[1] *= ApproximateOneOverRoot;
        Dest[2] *= ApproximateOneOverRoot;
        Dest[3] *= ApproximateOneOverRoot;
    }
}



END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MATH_INLINES_PPC_H
#endif
