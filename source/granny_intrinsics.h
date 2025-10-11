#if !defined(GRANNY_INTRINSICS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_intrinsics.h $
// $DateTime: 2012/04/03 12:10:06 $
// $Change: 36920 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GRANNY_CONSTANTS_H)
#include "granny_constants.h"
#endif

#if !defined(GRANNY_CONVERSIONS_H)
#include "granny_conversions.h"
#endif

#if PLATFORM_XENON
#include <ppcintrinsics.h>
#endif

#if PLATFORM_PS2 || PLATFORM_PSP

// doubles!?!?!? Crazy man.
extern "C"
{
    extern GRANNY real32 radsqrt(GRANNY real32);
    extern GRANNY real32 radsin(GRANNY real32);
    extern GRANNY real32 radcos(GRANNY real32);
    extern GRANNY real32 radfabs(GRANNY real32);
    extern GRANNY real32 radatan(GRANNY real32);
    extern GRANNY real32 radatan2(GRANNY real32, GRANNY real32);

    // PS2 has these as intrinsics, psp and ps3 need them declared as
    // of now
#if PLATFORM_PSP
    extern double fabs(double);
    extern double acos(double);
    extern int    abs(int);
#endif

    void* memcpy(void *dest, const void *src, __typeof__(sizeof(void*)) count);
    void* memset(void *dest, int c, __typeof__(sizeof(void*)) count);
}


BEGIN_GRANNY_NAMESPACE;

inline void
IntrinsicMemcpy(void* Dest, const void *Src, uintaddrx Count)
{
    memcpy(Dest, Src, Count);
}

inline void
IntrinsicMemset(void *Dest, int C, uintaddrx Count)
{
    memset(Dest, C, Count);
}

inline real32
IntrinsicAbsoluteValue(real32 const Value)
{
    return(radfabs(Value));
}

inline real32
IntrinsicSquareRoot(real32 const Value)
{
    return(radsqrt(Value));
}

inline real32
IntrinsicModulus(real32 const Dividend, real32 const Divisor)
{
    return(Dividend - Divisor*((real32)(TruncateReal32ToInt32(Dividend / Divisor))));
}

inline real32
IntrinsicSin(real32 const AngleInRadians)
{
    return(radsin(AngleInRadians));
}

inline real32
IntrinsicCos(real32 const AngleInRadians)
{
    return(radcos(AngleInRadians));
}

inline void
IntrinsicSinCos(real32 const AngleInRadians, real32* SinVal, real32* CosVal)
{
    *SinVal = IntrinsicSin(AngleInRadians);
    *CosVal = IntrinsicCos(AngleInRadians);
}

inline real32
IntrinsicTan(real32 const AngleInRadians)
{
  Assert(0);
  return 0.0f;
}

inline real32
IntrinsicATan2(real32 const SinAngle, real32 const CosAngle)
{
    return(radatan2(SinAngle, CosAngle));
}

inline real32
IntrinsicASin(real32 const SinValue)
{
    if (SinValue == 0)
        return 0;

    return radatan2(SinValue, radsqrt((1 - SinValue)*(1 + SinValue)));
}

inline real32
IntrinsicACos(real32 const CosValue)
{
    if (CosValue == 0)
        return Pi32 * 0.5f;

    return radatan2(radsqrt((1 - CosValue)*(1 + CosValue)), CosValue);
}


inline real32
IntrinsicATan(real32 const AngleInRadians)
{
    return(radatan(AngleInRadians));
}

END_GRANNY_NAMESPACE;

#elif (PLATFORM_PS3 && PROCESSOR_CELL) || (PLATFORM_PSP2)

extern "C"
{
    extern double fmod(double, double);
    extern double fabs(double);
    extern float  sinf(float);
    extern float  cosf(float);
    extern void   sincosf(float x, float *sin, float *cos);
    extern float  tanf(float);
    extern float  asinf(float);
    extern float  acosf(float);
    extern float  atanf(float);
    extern float  atan2f(float,float);
    extern float  sqrtf(float);
    extern int    abs(int);

    void* memcpy(void *dest, const void *src, __typeof__(sizeof(void*)) count);
    void* memset(void *dest, int c, __typeof__(sizeof(void*)) count);
}


BEGIN_GRANNY_NAMESPACE;

inline void
IntrinsicMemcpy(void* Dest, const void *Src, uintaddrx Count)
{
    memcpy(Dest, Src, Count);
}

inline void
IntrinsicMemset(void *Dest, int C, uintaddrx Count)
{
    memset(Dest, C, Count);
}

inline real32
IntrinsicAbsoluteValue(real32 const Value)
{
    return fabs(Value);
}

inline real32
IntrinsicSquareRoot(real32 const Value)
{
    return sqrtf(Value);
}

inline real32
IntrinsicModulus(real32 const Dividend, real32 const Divisor)
{
    return fmod(Dividend, Divisor);
}

inline real32
IntrinsicSin(real32 const AngleInRadians)
{
    return sinf(AngleInRadians);
}

inline real32
IntrinsicCos(real32 const AngleInRadians)
{
    return cosf(AngleInRadians);
}

inline void
IntrinsicSinCos(real32 const AngleInRadians, real32* SinVal, real32* CosVal)
{
    sincosf(AngleInRadians, SinVal, CosVal);
}

inline real32
IntrinsicTan(real32 const AngleInRadians)
{
    return tanf(AngleInRadians);
}

inline real32
IntrinsicATan2(real32 const SinAngle, real32 const CosAngle)
{
    return atan2f(SinAngle, CosAngle);
}

inline real32
IntrinsicASin(real32 const SinValue)
{
    return asinf(SinValue);
}

inline real32
IntrinsicACos(real32 const CosValue)
{
    return acosf(CosValue);
}


inline real32
IntrinsicATan(real32 const AngleInRadians)
{
    return atanf(AngleInRadians);
}

END_GRANNY_NAMESPACE;

#else

#if COMPILER_MSVC
extern "C"
{
    void* __cdecl memcpy(void *dest, const void *src, size_t count);
    void* __cdecl memset(void *dest, int c, size_t count);

    int __cdecl abs(int);
    double __cdecl log(double);
    double __cdecl fabs(double);
    double __cdecl sqrt(double);
    double __cdecl fmod(double, double);
    double __cdecl sin(double);
    double __cdecl cos(double);
    double __cdecl tan(double);
    double __cdecl asin(double);
    double __cdecl acos(double);
    double __cdecl atan(double);
    double __cdecl atan2(double, double);

#if !PLATFORM_XENON
#pragma intrinsic(abs, log, fabs, sqrt, fmod, sin, cos, tan, asin, acos, atan, atan2, memset, memcpy)
#else
#pragma intrinsic(abs, fabs, sqrt, memset, memcpy)
#endif
};

#elif COMPILER_RVCT || COMPILER_GHS || PLATFORM_NACL

#include <math.h>
#include <string.h>

#elif COMPILER_GCC
extern "C"
{
    void* memcpy(void *dest, const void *src, __typeof__(sizeof(void*)));
    void* memset(void *dest, int c,  __typeof__(sizeof(void*)));

#if !PLATFORM_GAMECUBE && !PLATFORM_LINUX && !PLATFORM_PS2 && !PLATFORM_ANDROID
    // We can't compile abs this way on the 'cube, cause the GCC variant
    // there defines it internally
    extern int abs(int);
#endif
    extern double fabs(double);
    extern double log(double);
    extern double fmod(double, double);
    extern double tan(double);
    extern double asin(double);
    extern double acos(double);
    extern double atan(double);
    extern double atan2(double, double);
    extern double sqrt(double);
    extern double sin(double);
    extern double cos(double);
    extern double fabs(double);
};


#elif COMPILER_METROWERKS

extern "C"
{
    void* memcpy(void *dest, const void *src, __typeof__(sizeof(void*)) count);
    void* memset(void *dest, int c, __typeof__(sizeof(void*)) count);

    extern int abs(int);
    extern double sqrt(double);
    extern double sin(double);
    extern double cos(double);
    extern double fabs(double);
    extern double log(double);
    extern double fmod(double, double);
    extern double tan(double);
    extern double asin(double);
    extern double acos(double);
    extern double atan(double);
    extern double atan2(double, double);
}

#else
#error "Unhandled platform/compiler combination"
#endif

BEGIN_GRANNY_NAMESPACE;

inline void
IntrinsicMemcpy(void* Dest, const void *Src, uintaddrx Count)
{
    memcpy(Dest, Src, Count);
}

inline void
IntrinsicMemset(void *Dest, int C, uintaddrx Count)
{
    memset(Dest, C, Count);
}

inline double
IntrinsicAbsoluteValue(double const Value)
{
    return(fabs(Value));
}

inline double
IntrinsicSquareRoot(double const Value)
{
    return sqrt(Value);
}

inline real32
IntrinsicModulus(real32 const Dividend, real32 const Divisor)
{
#if PROCESSOR_X86
    real32 Result;
    __asm
    {
        fld [Divisor]
        fld [Dividend]
    looper:
        fprem
        fwait
        fnstsw ax
        sahf
        jp looper
        fstp [Result]
        fstp st(0)
    }

    return(Result);
#else
    return(Dividend - Divisor*((real32)(TruncateReal32ToInt32(Dividend / Divisor))));
#endif
}

inline real32
IntrinsicSin(double const AngleInRadians)
{
    return real32(sin(AngleInRadians));
}

inline real32
IntrinsicCos(double const AngleInRadians)
{
    return real32(cos(AngleInRadians));
}


inline void
IntrinsicSinCos(double const AngleInRadians, real32* SinVal, real32* CosVal)
{
#if PROCESSOR_X86
    __asm
    {
        fld [AngleInRadians]
        fsincos
        mov eax, [SinVal]
        mov edx, [CosVal]
        fstp DWORD PTR [edx]
        fstp DWORD PTR [eax]
    }
#else
    *SinVal = (real32)IntrinsicSin(AngleInRadians);
    *CosVal = (real32)IntrinsicCos(AngleInRadians);
#endif
}


inline double
IntrinsicTan(double const AngleInRadians)
{
    return(tan(AngleInRadians));
}

inline double
IntrinsicATan2(double const SinAngle, double const CosAngle)
{
    return(atan2(SinAngle, CosAngle));
}

inline real32
IntrinsicASin(real32 const SinValue)
{
    if (SinValue == 0)
        return 0;

    return (real32)atan2(SinValue, sqrt((1 - SinValue)*(1 + SinValue)));
}

inline real32
IntrinsicACos(real32 const CosValue)
{
    if (CosValue == 0)
        return Pi32 * 0.5f;

    return (real32)atan2(sqrt((1 - CosValue)*(1 + CosValue)), CosValue);
}

inline double
IntrinsicATan(double const AngleInRadians)
{
    return(atan(AngleInRadians));
}

END_GRANNY_NAMESPACE;

#endif //#else //#if PLATFORM_PS2

#include "header_postfix.h"
#define GRANNY_INTRINSICS_H
#endif
