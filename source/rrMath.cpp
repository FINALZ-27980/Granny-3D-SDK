// rrMath.cpp - copyright 2009 RAD Game Tools
//

#include "rrMath.h"
#include <math.h>

//_mm_set_ss
//_mm_rsqrt_ss
//_mm_store_ss
//_mm_cvtss_f32
/*
RADDEFFUNC F32 rrRecipSqrt(F32 val)
{
    RR_ASSERT( val > 0.f );
    return 1.f / sqrt(val);
}
*/

//rsqrtss + Newton Raphson
RADDEFFUNC F32 rrRecipSqrt(F32 x)
{
    RR_ASSERT( x > 0.f );

    #ifdef __RADNT__

    F32 xhalf = 0.5f * x;

    __declspec( align(16) ) F32 t;

    //get reciprocal sqrt estimate
    //t = _mm_cvtss_f32( _mm_rsqrt_ss( _mm_set_ss( x ) ) );
    _mm_store_ss(&t, _mm_rsqrt_ss( _mm_set_ss( x ) ) );

    // one NR step :
    t = t * (1.5f - xhalf*t*t);

    // 1.0 - t*t*x  6.1706030285435531e-008 double

    return t;

    #else

    return 1.f / sqrtf(x);

    #endif
}

RADDEFFUNC F64_OR_32 rrlog2( const F64_OR_32 X )
{
    RR_ASSERT( X > 0.0 );

    // TODO : some platforms have a native log 2
    //  could also get log2 from exponent and then NR to improve it
    //  (but NR requires a fast 2.0^exp with exp floating)
    // easier to do taylor because taylor of log is easy

    // NOTE(casey): Changed this from comment-style switch to #if switch
    // so that stricter compilers don't complain.
#if 1

    F64_OR_32 exact =  log( X ) * (1.0 / RR_LN2);

    return exact;
#else

    ///-------------
    // approximate log2 by getting the exponent from the float
    //  and then using the mantissa to do a taylor series

    // CB : this works but is by no means fast so fuck it

    U32 X_as_int = rrF32AsInt((F32)X);
    int iLogFloor = (X_as_int >> 23) - 127;

    rrFloatAnd32 fi;
    fi.i = (X_as_int & ( (1<<23)-1 ) ) | (127<<23);
    F32 frac = fi.f;
    RR_ASSERT( frac >= 1.0 && frac < 2.0 );

    if ( frac > 4.0/3.0 )
    {
        // (frac/2) is closer to 2.0 than frac is
        //  push the iLog up and our correction will now be negative
        // the branch here sucks but this is necessary for accuracy
        //  when frac is near 2.0, t is near 1.0 and the Taylor is totally invalid
        frac /= 2.0;
        iLogFloor++;
    }

    // X = 2^iLogFloor * frac
    F64 t = frac - 1.0;

    F64 lnFrac = t - t*t*0.5 + (t*t*t)*( (1.0/3.0) - t*(1.0/4.0) + t*t*(1.0/5.0) - t*t*t*(1.0/6.0) );

    F64 approx = (F64)iLogFloor + lnFrac * (1.0 / RR_LN2);

    RR_DURING_ASSERT( F64 exact =  log( X ) * (1.0 / RR_LN2) );
    RR_ASSERT_NO_ASSUME( fabs(exact - approx) <= 0.0002 );

    return approx;

#endif
}

RADDEFFUNC F32 rrlog2f_approx( const F32 X )
{
    RR_ASSERT( X > 0.0f );

    rrFloatAnd32 fi;
    fi.f = X;

    // get the float as an int, subtract off exponent bias
    float vv = (float) (fi.i - (127<<23));

    vv *= (1.f/8388608);

    // vv is now like a fixed point with the exponent in the int
    //  and the mantissa in the fraction
    //
    // that is actually already an approximate log2, but very inaccurate

    // get the fractional part of vv :
    //float frac = vv - ftoi(vv);

    fi.i = (fi.i & 0x7FFFFF) | (127<<23);
    float frac = fi.f - 1.f;

    // use frac to apply a correction hump :
    const float C = 0.346573583f;

    F32 approx = vv + C * frac * (1.f - frac);

    RR_DURING_ASSERT( F64_OR_32 exact =  rrlog2( X ) );
    RR_ASSERT_NO_ASSUME( fabs(exact - approx) <= 0.01 );

    return approx;
}

RADDEFFUNC U32 rrIlog2floorf(F32 val)
{
    return ((rrF32AsInt(val)) >> 23) - 127;
}

RADDEFFUNC U32 rrIlog2ceilf(F32 val)
{
    return ((rrF32AsInt(val) + 0x7FFFFF) >> 23) - 127;
}

RADDEFFUNC U32 rrIlog2roundf(F32 val)
{
    return ((rrF32AsInt(val) + 0x257D86) >> 23) - 127;
}

RADDEFFUNC U32 rrIlog2ceil(U32 val)
{
	RR_ASSERT( val != 0 );
    return rrGetBitLevel_V( (val-1) );
}

RADDEFFUNC U32 rrIlog2floor(U32 val)
{
    // note : BitScanReverse is almost certainly faster
    return rrIlog2floorf( (F32)val );
}

RADDEFFUNC U32 rrIlog2round(U32 val)
{
    // note : BitScanReverse is almost certainly faster
    return rrIlog2roundf( (F32)val );
}

#if 0

// rrIlog2ceil is the same as getbitlevel except on powers of 2
//  ceil( log2( val ) ) , so rrIlog2ceil(256) = 8 , rrIlog2ceil(257) = 9
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4035) // no return value
RADDEFFUNC U32 rrIlog2ceil(U32 val)
{
    return rrIlog2ceilf( (F32)val );
    /*
    // TODO : BitScanReverse might be better

    // CB : you can use different adds in this trick to get a log2 that floors or rounds if you want that

    __asm
    {
        FILD val
        FSTP val
        mov eax,val
        add eax,0x7FFFFF // 1<<23 - 1
        shr eax,23
        sub eax,127
    }
    */
}
#pragma warning(pop)
#else

RADDEFFUNC U32 rrIlog2ceil(U32 val)
{
    U32 L;
    for(L=0; (1UL<<L) < val; L++) ;
    // (1<<L) >= val;
    return L;
}
#endif

#endif

// don't require CRT
// if you don't care, testing for both nan or infinity is faster
RADDEFFUNC rrbool rr_isnaninf_f(F32 val)
{
   union { F32 f; U32 u; } num;
   num.f = val;
   // nan or inf if exponent is 255
   return (num.u & 0x7f800000) == 0x7f800000;
}

RADDEFFUNC rrbool rr_isnaninf(F64 val)
{
   union { F64 f; U64 u; } num;
   num.f = val;
   // nan or inf if exponent is 2^11-1
   #if defined(_MSC_VER) && _MSC_VER < 1300
   return (num.u & 0x7ff0000000000000Ui64) == 0x7ff0000000000000Ui64;
   #else
   return (num.u & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL;
   #endif
}

RADDEFFUNC rrbool rr_isnan_f(F32 val)
{
   union { F32 f; U32 u; } num;
   num.f = val;
   // nan or inf if exponent is 255
   return (num.u & 0x7f800000) == 0x7f800000
      // nan if non-zero mantissa
      &&  (num.u & 0x007fffff);
}

RADDEFFUNC rrbool rr_isnan(F64 val)
{
   union { F64 f; U64 u; } num;
   num.f = val;
   // nan or inf if exponent is 2^11-1
   #if defined(_MSC_VER) && _MSC_VER < 1300
   return (num.u & 0x7ff0000000000000Ui64) == 0x7ff0000000000000Ui64
      // nan if non-zero mantissa
      &&  (num.u & 0x000fffffffffffffUi64);
   #else
   return (num.u & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL
      // nan if non-zero mantissa
      &&  (num.u & 0x000fffffffffffffULL);
   #endif
}

RADDEFFUNC U32 rrNextPow2(const U32 x)
{
	if ( x <= 0 ) return 1;
	
    U32 bl = rrGetBitLevel_V( (x-1) );
    
    U32 up = 1 << bl;
    
    RR_ASSERT( up >= x );
    RR_ASSERT( up < 2*x );
    
    return up;
}

RADDEFFUNC U32 rrPrevPow2(const U32 x)
{
	if ( x == 0 ) return 0;

    U32 bl = rrGetBitLevel_V( x );
    RR_ASSERT( bl != 0 );
    
    U32 down = 1 << (bl-1);
    
    RR_ASSERT( down <= x );
    RR_ASSERT( down*2 > x );
    
    return down;
}
