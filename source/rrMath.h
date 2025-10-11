#ifndef __RADRR_MATHH__
#define __RADRR_MATHH__

#include "rrBase.h"

#ifdef __RADNT__
#include <xmmintrin.h>
#endif

#ifdef __RADXENON__
#include <ppcintrinsics.h>
#endif

#ifdef __RADSPU__
#include <spu_intrinsics.h>
#endif

/**

CB : math helpers

todo : there's good stuff in radmath.h , move some of that over

for little single float & int math routines

not the place to stick a matrix library or whatever complex juju


probably some things should get moves to rrMath.inl to clean up this header

**/

#define RR_LN2      (0.6931471805599453)
#define RR_PI       (3.141592653589793)
#define RR_LN2f     (0.6931471805599453f)
#define RR_PIf      (3.141592653589793f)

#define RR_F32_EPSILON     (1.192092896e-07F)        /* smallest such that 1.0+FLT_EPSILON != 1.0 */
#define RR_F32_MAX         (3.402823466e+38F)        /* max value */

#define RR_S32_MAX          (2147483647)
#define RR_U32_MAX          (~(U32)0)

RADDEFSTART

RADDEFFUNC F64_OR_32 rrlog2( F64_OR_32 X );
RADDEFFUNC F32 rrlog2f_approx( const F32 X );

RADDEFFUNC U32 rrIlog2floor(U32 val);
RADDEFFUNC U32 rrIlog2ceil (U32 val);
RADDEFFUNC U32 rrIlog2round(U32 val);

RADDEFFUNC U32 rrIlog2floorf(F32 val);
RADDEFFUNC U32 rrIlog2ceilf(F32 val);
RADDEFFUNC U32 rrIlog2roundf(F32 val);

RADDEFFUNC rrbool rr_isnan(F64 val);
RADDEFFUNC rrbool rr_isnan_f(F32 val);
RADDEFFUNC rrbool rr_isnaninf(F64 val);
RADDEFFUNC rrbool rr_isnaninf_f(F32 val);

RADDEFEND

static RADINLINE rrbool rrIsPow2(const U32 x)
{
    return ! (x & ~(-(S32)x));
}

// rrNextPow2 : if x already is Pow2, returns x ; else returns the next higher power of 2 ; rrNextPow2 returns 1 for x <= 0
RADDEFFUNC U32 rrNextPow2(const U32 x);

// rrPrevPow2 : if x already is Pow2, returns x ; else returns the next lower power of 2 ; rrPrevPow2 returns 0 for x <= 0
RADDEFFUNC U32 rrPrevPow2(const U32 x);

/*
static RADINLINE S32 rrNextPow2(const S32 x)
{
    int y = 1; // compile as C
    rrassert( x >= 0 );
    // @@ could probably be faster, but not used anywhere important at the moment
    while ( y < x )
    {
        y += y;
    }
    return y;
}
*/

// Aligns v up, alignment must be a power of two.
static RADINLINE U32 rrAlignUp32(const U32 v, const S32 alignment)
{
    rrassert(rrIsPow2(alignment));
    //rrassert( v >= 0 );
    return (v+(alignment-1)) & ~(alignment-1);
}

// Aligns v down, alignment must be a power of two.
static RADINLINE U32 rrAlignDown32(const U32 v, const S32 alignment)
{
    rrassert(rrIsPow2(alignment));
    //rrassert( v >= 0 );
    return v & ~(alignment-1);
}

// Aligns v up, alignment must be a power of two.
static RADINLINE U64 rrAlignUp64(const U64 v, const S32 alignment)
{
    rrassert(rrIsPow2(alignment));
    //rrassert( v >= 0 );
    return (v+(alignment-1)) & ~(alignment-1);
}

// Aligns v down, alignment must be a power of two.
static RADINLINE U64 rrAlignDown64(const U64 v, const S32 alignment)
{
    rrassert(rrIsPow2(alignment));
    //rrassert( v >= 0 );
    return v & ~(alignment-1);
}

#ifdef __cplusplus
// 64 bit overloads for the normal names
// Aligns v up, alignment must be a power of two.
static RADINLINE S64 rrAlignUp(const S64 v, const S32 alignment)
{
    return rrAlignUp64(v,alignment);
}

// Aligns v down, alignment must be a power of two.
static RADINLINE S64 rrAlignDown(const S64 v, const S32 alignment)
{
    return rrAlignDown64(v,alignment);
}

static RADINLINE S32 rrAlignUp(const S32 v, const S32 alignment)
{
    return rrAlignUp32((U32)v,alignment);
}

// Aligns v down, alignment must be a power of two.
static RADINLINE S32 rrAlignDown(const S32 v, const S32 alignment)
{
    return rrAlignDown32((U32)v,alignment);
}

static RADINLINE U32 rrAlignUp(const U32 v, const S32 alignment)
{
    return rrAlignUp32((U32)v,alignment);
}

// Aligns v down, alignment must be a power of two.
static RADINLINE U32 rrAlignDown(const U32 v, const S32 alignment)
{
    return rrAlignDown32((U32)v,alignment);
}

static RADINLINE U64 rrAlignUp(const U64 v, const S32 alignment)
{
    return rrAlignUp64(v,alignment);
}

// Aligns v down, alignment must be a power of two.
static RADINLINE U64 rrAlignDown(const U64 v, const S32 alignment)
{
    return rrAlignDown64(v,alignment);
}

static RADINLINE UINTa rrAlignUpA(const UINTa v, const S32 alignment)
{
    return RR_STRING_JOIN(rrAlignUp,RAD_PTRBITS)((RR_STRING_JOIN(U,RAD_PTRBITS))v,alignment);
}

// Aligns v down, alignment must be a power of two.
static RADINLINE UINTa rrAlignDownA(const UINTa v, const S32 alignment)
{
    return RR_STRING_JOIN(rrAlignDown,RAD_PTRBITS)((RR_STRING_JOIN(U,RAD_PTRBITS))v,alignment);
}

template <typename T>
static RADINLINE T * rrAlignUpPointer(T * v, const S32 alignment)
{
    return (T *) rrAlignUpA((UINTa)v,alignment);
}

template <typename T>
static RADINLINE T * rrAlignDownPointer(T * v, const S32 alignment)
{
    return (T *) rrAlignDownA((UINTa)v,alignment);
}

template <typename T>
static RADINLINE bool rrIsAligned(T v, const S32 alignment)
{
    return rrAlignDown(v,alignment) == v;
}

template <>
RADINLINE bool rrIsAligned<UINTa>(UINTa v, const S32 alignment)
{
    return rrAlignDownA(v,alignment) == v;
}

template <typename T>
static RADINLINE bool rrIsAlignedPointer(T * v, const S32 alignment)
{
    return rrIsAligned((UINTa)v,alignment);
}

#endif

//=============================================

RADDEFFUNC F32 rrRecipSqrt(F32 val);


// return a*b 
static RADINLINE U32 rrMul32High(U32 a,U32 b)
{
    // NOTE(casey): Changed this from comment-style switch to #if switch
    // so that stricter compilers don't complain.
#if 1
    // CB : this appears to be magic in MSVC that does the right hing :
    
    return (U32)( ((U64) a * b) >>32);
    
    //return UInt32x32To64(a,b)>>32;
    
#else
    
    // asm ruins outer optimization - don't use
    
    __asm
    {
        mov eax, a
        mul b
        mov eax,edx
    }
#endif
}

//=============================================

// unions for treating floats as their bits :

union rrFloatAnd32
{
    U32 i;
    F32 f;
};

union rrDoubleAnd64
{
    U64 i;
    F64 f;
};

static RADFORCEINLINE U32 rrF32AsInt(F32 val)
{
    union rrFloatAnd32 ff;
    ff.f = val;
    return ff.i;
}

static RADFORCEINLINE U64 rrF64AsInt(F64 val)
{
    union rrDoubleAnd64 ff;
    ff.f = val;
    return ff.i;
}

static RADFORCEINLINE F32 rrF32FromInt(U32 val)
{
    union rrFloatAnd32 ff;
    ff.i = val;
    return ff.f;
}

static RADFORCEINLINE F64 rrF64FromInt(U64 val)
{
    union rrDoubleAnd64 ff;
    ff.i = val;
    return ff.f;
}

//=============================================

/*

ftoi : truncates ; eg. fractions -> 0

*/
static RADINLINE S32 rr_ftoi_trunc(const F32 f)
{
    #ifdef __RADNT__

    // plain old C cast is actually fast with /QIfist
    //  the only problem with that is if D3D or anyone changes the FPU settings
    #ifdef _DEBUG
    
    return (int)f;
    
    #else
    
    // @@ URG - Edit & Continue fucks this up somehow, it blows up something in the SSE state
    
    // SSE single scalar cvtt is not as fast but is reliable :
    return _mm_cvtt_ss2si( _mm_set_ss( f ) );
    
    #endif // _DEBUG
    
    #else
    
    return (S32) f;
    
    #endif
}

static RADINLINE S32 rr_ftoi_round(const F32 val)
{
    return ( val >= 0.f ) ? rr_ftoi_trunc( val + 0.5f ) : rr_ftoi_trunc( val - 0.5f );
}

static RADINLINE S32 rr_ftoi_round_positive(const F32 val)
{
    // only correct for val >= 0 ; doesn't assert though, lets you pass negatives, they are just wrong
    return rr_ftoi_trunc( val + 0.5f );
}

// @@ alias ?
#define rr_ftoi         rr_ftoi_trunc
#define rr_froundint    rr_ftoi_round
#define rr_froundint_positive    rr_ftoi_round_positive

// convert double/float to integer with fastest path, don't care about rounding
#define rr_dtoi_fastest(x)  ((S32) (x))
#define rr_ftoi_fastest(x)  ((S32) (x))

//=======================================================

// F32 min/max and select
#if defined(__RADXENON__) || defined(__RADWII__)
   #define RR_FSEL_F32(x,a,b) ((F32) __fsel((x), (a), (b)))
   #define RR_MIN_F32(a,b) RR_FSEL_F32((b)-(a), (a), (b))
   #define RR_MAX_F32(a,b) RR_FSEL_F32((b)-(a), (b), (a))
#elif defined(__RADPS3__)
   #define RR_FSEL_F32(x,a,b) __fsels((x), (a), (b))
   #define RR_MIN_F32(a,b) RR_FSEL_F32((b)-(a), (a), (b))
   #define RR_MAX_F32(a,b) RR_FSEL_F32((b)-(a), (b), (a))
#elif defined(__RADWIIU__)
   #define RR_FSEL_F32(x,a,b) ((F32) __FSEL((x), (a), (b)))
   #define RR_MIN_F32(a,b) RR_FSEL_F32((b)-(a), (a), (b))
   #define RR_MAX_F32(a,b) RR_FSEL_F32((b)-(a), (b), (a))
#else
   #define RR_FSEL_F32(x,a,b) (((x) >= 0) ? (a) : (b))
   #define RR_MIN_F32(a,b) RR_MIN(a,b)
   #define RR_MAX_F32(a,b) RR_MAX(a,b)
#endif

//=======================================================
// CB : getbitlevel used to be in VarBits , but doesn't really belong there;
//  doesn't really belong here either though

// rrGetBitLevel :
//    getbitlevel(n) is the number of bits that n uses for its on bits
//    eg. n < (1<<getbitlevel(n)) , n >= (1<<(getbitlevel(n)-1))
//    getbitlevel(n)-1 is the bit position of the leftmost 1 bit in 'n'
// NOTE : getbitlevel(0) = 0
//  getbitlevel(n) = ilog2ceil except on powers of two (on powers of two, GetBitLevel is one larger)
//
// that is :
//	 (1<<getbitlevel(n))-1 
//  is a mask which covers all the 1 bits of n
//---------------------------------------


// WARNING : this getbitlevel only works on *constant* U16 values ! (up to 65535)
//   it will fail to compile with strange errors if you use a variable
#define rrGetBitLevel_C(level)        \
(                                      \
  (((level)<    1)?0:                  \
  (((level)<    2)?1:                  \
  (((level)<    4)?2:                  \
  (((level)<    8)?3:                  \
  (((level)<   16)?4:                  \
  (((level)<   32)?5:                  \
  (((level)<   64)?6:                  \
  (((level)<  128)?7:                  \
  (((level)<  256)?8:                  \
  (((level)<  512)?9:                  \
  (((level)< 1024)?10:                 \
  (((level)< 2048)?11:                 \
  (((level)< 4096)?12:                 \
  (((level)< 8192)?13:                 \
  (((level)<16384)?14:                 \
  (((level)<32768)?15:                 \
  (((level)<65536)?16:sizeof(char[65535-level]) \
  )))))))))))))))))                    \
)


#if defined(__RADXENON__)

   #define rrGetBitLevel_V(n) (U32) (32 - _CountLeadingZeros(n))

   #define rrGetBitLevel64_V(n) (64 - _CountLeadingZeros64(n))

#elif defined(__RADPPC__)

  // Watch out for SNC, it defines __GNUC__, too.
  #if defined(__GNUC__) && !(defined(__SNC__) || defined(__ghs__))

    #define count_leading_zeros(count, x) \
      __asm__ ("{cntlz|cntlzw} %0,%1"     \
         : "=r" (count)                   \
          : "r" (x))

    static RADINLINE U32 rrGetBitLevel_V( register U32 n )
    {
      count_leading_zeros( n, n );
      return( 32 - n );
    }

    #ifdef __RAD64REGS__

    #define count_leading_zeros64(count, x) \
      __asm__ ("cntlzd %0,%1"     \
         : "=r" (count)                   \
          : "r" (x))

    static RADINLINE U32 rrGetBitLevel64_V( register U64 n )
    {
      count_leading_zeros64( n, n );
      return( 64 - n );
    }

    #endif

  #else

    #define rrGetBitLevel_V(n)  (32 - __cntlzw(n))

    #ifdef __RAD64REGS__
    #define rrGetBitLevel64_V(n) (64 - __cntlzd(n))
    #endif

  #endif

#elif defined(__RADSPU__)

  static RADINLINE U32 rrGetBitLevel_V( register U32 n )
  {
    vector unsigned int v = spu_promote(n,0);
    vector unsigned int ret = __builtin_spu_cntlz( v );  
    return( 32 - ret[0] );
  }
#elif defined(__RADPS2__)

    #define count_leading_zeros(count, x) \
      __asm__ ("plzcw %0,%1"              \
         : "=r" (count)                   \
         : "r" (x))

    static RADINLINE U32 rrGetBitLevel_V( register U32 n )
    {
      count_leading_zeros( n, n );
      return( 31 - (n & 0xFF) );
    }

#elif (_MSC_FULL_VER >= 13012035 )

    RADDEFSTART
    unsigned char _BitScanReverse(unsigned long* Index, unsigned long Mask);
    #pragma intrinsic(_BitScanReverse)
    RADDEFEND

    static RADFORCEINLINE U32 rrGetBitLevel_V( register U32 val )
    {
        unsigned long b = 0;
        _BitScanReverse( &b, val );
        return (val) ? ((U32)b + 1) : 0;
    }

    #ifdef __RAD64REGS__

    RADDEFSTART
    unsigned char _BitScanReverse64(unsigned long* Index, unsigned __int64 Mask);
    #pragma intrinsic(_BitScanReverse64)
    RADDEFEND

    static RADFORCEINLINE U32 rrGetBitLevel64_V( register U64 val )
    {
        unsigned long b = 0;
        _BitScanReverse64( &b, val );
        return (val) ? ((U32)b + 1) : 0;
    }
    
    #endif
    
#else

    static RADINLINE U32 rrGetBitLevel_V( register U32 val )
    {
      static char vs[16]={0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4};
      int bits=0;

      if ( val & 0xffff0000 )
      {
        bits = 16;
        val >>= 16;
      }

      if ( val & 0xff00 )
      {
        bits += 8;
        val >>= 8;
      }

      if ( val & 0xf0 )
      {
        bits += 4;
        val >>= 4;
      }

      bits += vs[ val ];

      return bits;
    }

#ifdef __RAD64REGS__
    static RADINLINE U32 rrGetBitLevel64_V( register U64 val )
    {
        static char vs[16]={0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4};
        int bits=0;

        if ( val & 0xffffffff00000000 )
        {
            bits = 32;
            val >>= 32;
        }

        if ( val & 0xffff0000 )
        {
            bits += 16;
            val >>= 16;
        }

        if ( val & 0xff00 )
        {
            bits += 8;
            val >>= 8;
        }

        if ( val & 0xf0 )
        {
            bits += 4;
            val >>= 4;
        }

        bits += vs[ val ];

        return bits;
    }
#endif // __RAD64REGS__


#endif

//=======================================================

// default CountLeadingZeros is to undo the 32 - from GetBitLevel :

static RADFORCEINLINE U32 rrCountLeadingZeros32(U32 val)
{
    return 32 - rrGetBitLevel_V(val);
}

#ifdef __RAD64REGS__
static RADFORCEINLINE U32 rrCountLeadingZeros64(U64 val)
{
    return 64 - rrGetBitLevel64_V(val);
}
#endif


//=======================================================

#endif //__RADRR_MATHH__
