#if !defined(GRANNY_COMPILER_H)
// #include "header_preamble.h" nb: granny_compiler is included in header_preamble!
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_compiler.h $
// $DateTime: 2012/08/28 16:33:35 $
// $Change: 39055 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

// Clear all flags
#define COMPILER_GCC 0
#define COMPILER_METROWERKS 0
#define COMPILER_MSVC 0
#define COMPILER_RVCT 0
#define COMPILER_SNC 0
#define COMPILER_GHS 0

// By default
#define COMPILER_SUPPORTS_VAR_MACRO 1

// Microsoft VC++ (version 4.0 defines this as 1000)
#ifdef _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1

#if _MSC_VER < 1400
  #undef COMPILER_SUPPORTS_VAR_MACRO
  #define COMPILER_SUPPORTS_VAR_MACRO 0
#endif


#endif

// Gnu GCC (watch out for SNC and GHS, which define these...)
#if (defined(__GNUG__) || defined(__GNUC__))
  #if (!(defined(__SNC__) || defined(NN_COMPILER_RVCT)) && \
       !defined(__ghs__))
    #undef COMPILER_GCC
    #define COMPILER_GCC 1
  #endif
#endif

#if defined(NN_COMPILER_RVCT)
#undef  COMPILER_RVCT
#define COMPILER_RVCT 1
#endif

// Metrowerks (CodeWarrior)
#ifdef __MWERKS__
#undef COMPILER_METROWERKS
#define COMPILER_METROWERKS 1
#endif

#if defined(__SNC__)
#undef COMPILER_SNC
#define COMPILER_SNC 1
#endif

#if defined(__ghs__)
#undef COMPILER_GHS
#define COMPILER_GHS 1
#endif

#define IS_ALIGNED_N(A, n) ((((uintaddrx)A) % (n)) == 0)
#define IS_ALIGNED_16(A)   IS_ALIGNED_N((A), 16)

#if COMPILER_MSVC

#define ALIGN_N(type, n) __declspec(align(n)) type
#define ALIGN16(type) ALIGN_N(type, 16)
#define ALIGN16_STACK(type, varname, arraysize) ALIGN16(type) varname[arraysize]

// See note in dll_header about _XBOX define
#if defined(_XENON) || defined(_XBOX)
  #define NOALIAS __restrict
#else
  #define NOALIAS
#endif

#if !DEBUG
// MSVC 2010 doesn't consider sizeof(x) to be a use of x
#pragma warning(disable : 4189) // local variable is initialized but not referenced
#endif

#pragma warning( disable : 4100 ) // warning C4100: 'CopySize' : unreferenced formal parameter
#pragma warning( disable : 4103 ) // used #pragma pack to change alignment
#pragma warning( disable : 4121 ) // alignment of a member was sensitive to packing
#pragma warning( disable : 4127 ) // "conditional expression is constant", e.g. Assert(!"Scary")
#pragma warning( disable : 4200 ) // warning C4200: nonstandard extension used : zero-sized array in struct/union
#pragma warning( disable : 4201 ) // warning C4201: nonstandard extension used : nameless struct/union
#pragma warning( disable : 4328 ) // indirection alignment of formal parameter 5 (8) is greater than the actual argument alignment (4)
#pragma warning( disable : 4505 ) // warning C4505: 'AccumulateModelAnimations' : unreferenced local function has been removed
#pragma warning( disable : 4514 ) // "unreferenced inline function has been removed"
#pragma warning( disable : 4725 ) // warning C4725: instruction may be inaccurate on some Pentiums (it's the FDIV bug!)
#pragma warning( disable : 4740 ) // flow in or out of inline asm code suppresses global optimization

#elif COMPILER_RVCT

#define ALIGN_N(type, n)    __attribute__ ((aligned (n))) type
#define ALIGN16(type)       ALIGN_N(type, 16)

#define NOALIAS __restrict

// This is non-ideal, but it's required to do alignments over 8
#define ALIGN16_STACK(type, varname, arrsize)                           \
    uint8 varname ## _unalignedbuffer[(sizeof(type) * arrsize) + 16];   \
    type* varname = (type*)(((uintaddrx)(&((varname ## _unalignedbuffer)[0])) + 15) & ~0xf)

#else

#define ALIGN_N(type, n) __attribute__ ((aligned (n))) type
#define ALIGN16(type) ALIGN_N(type, 16)
#define NOALIAS __restrict

#define ALIGN16_STACK(type, varname, arrsize)                           \
    uint8 varname ## _unalignedbuffer[(sizeof(type) * arrsize) + 16];   \
    type* varname = (type*)(((uintaddrx)(&((varname ## _unalignedbuffer)[0])) + 15) & ~0xf)

#endif

#if SOURCE_ANALYSIS_WIN32
   #include <windows.h>
   #include <codeanalysis\sourceannotations.h>

   #define AnalysisCond(x) x
   // __analysis_assume is present already
#else
  #define AnalysisCond(x)

  #ifndef __analysis_assume
    #define __analysis_assume(x) (void)sizeof(x)
  #endif
#endif


#if !COMPILER_RVCT && !COMPILER_GHS
  #define UNUSED_VARIABLE(x) (void)(sizeof(x))
#else
  #define UNUSED_VARIABLE(x) x = x
#endif

#if COMPILER_SNC
#pragma diag_suppress=237
#endif

// Callback conventions
#if defined(_XENON) || defined(_XBOX) || defined(_WIN32)
  #define CALLBACK_FN(ret) ret __cdecl
#else
  #define CALLBACK_FN(ret) ret
#endif


#if !COMPILER_MSVC && !COMPILER_GCC && !COMPILER_SNC && !COMPILER_METROWERKS && !COMPILER_RVCT && !COMPILER_GHS
#error Unrecognized compiler (see compiler.h)
#endif

#if ((COMPILER_MSVC + COMPILER_GCC + COMPILER_SNC + COMPILER_METROWERKS + COMPILER_RVCT + COMPILER_GHS) != 1)
#error "Too many complier defines"
#endif

// #include "header_postfix.h" nb: granny_compiler is included in header_preamble!
#define GRANNY_COMPILER_H
#endif
