#if !defined(GRANNY_PLATFORM_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_platform.h $
// $DateTime: 2012/04/27 12:43:03 $
// $Change: 37156 $
// $Revision: #7 $
//
// $Notice: $
// ========================================================================

// Clear all windowing system flags
#define PLATFORM_WIN32 0
#define PLATFORM_WIN64 0
#define PLATFORM_WINXX 0
#define PLATFORM_MACOSX 0
#define PLATFORM_XENON 0
#define PLATFORM_SEKRIT 0
#define PLATFORM_PS2 0
#define PLATFORM_PSP 0
#define PLATFORM_PS3 0
#define PLATFORM_PS3_SPU 0
#define PLATFORM_GAMECUBE 0
#define PLATFORM_WII 0
#define PLATFORM_WIIU 0
#define PLATFORM_N3DS 0
#define PLATFORM_LINUX 0
#define PLATFORM_IPHONE 0
#define PLATFORM_GENERIC_ARM9 0
#define PLATFORM_PSP2 0
#define PLATFORM_ANDROID 0
#define PLATFORM_NACL 0

#define PLATFORM_PATH_SEPARATOR "/"
#define PLATFORM_HAS_MEMORY_INTRINSICS 1
#define PLATFORM_SUPPORTS_MULTITHREADING 0

// See if we're Windows
#ifdef _WIN32
// Cunningly, both _WIN64 and _WIN32 are defined for X64 stuff.
#ifdef _WIN64

#undef PLATFORM_WIN64
#define PLATFORM_WIN64 1
#undef PLATFORM_WINXX
#define PLATFORM_WINXX 1

#else //#ifdef _WIN64

#undef PLATFORM_WIN32
#define PLATFORM_WIN32 1
#undef PLATFORM_WINXX
#define PLATFORM_WINXX 1

#endif //#else //#ifdef _WIN64

#endif // _WIN32


// See if we're Macintosh
// TODO: We should really distinguish between Mac OS X and OS < X here.
#if defined(_MACOSX) || defined(macintosh)
#undef PLATFORM_MACOSX
#define PLATFORM_MACOSX 1
#endif


// Or android
#if defined(ANDROID)
  // Test here because android also defines linux
  #undef PLATFORM_ANDROID
  #define PLATFORM_ANDROID 1
#elif defined(_LINUX) || defined(linux) || defined(__linux__)
  // See if we're generic Linux
  #undef PLATFORM_LINUX
  #define PLATFORM_LINUX 1
#endif

// NaCl?
#if defined(__native_client__)
#undef PLATFORM_NACL
#define PLATFORM_NACL 1
#endif


// See if we're on Xenon
#if defined(_XENON) || defined(_XBOX)
// TODO: Why does Microsoft's compiler insist on defining _WIN32 on Xenon??
#undef PLATFORM_WIN32
#define PLATFORM_WIN32 0
#undef PLATFORM_WINXX
#define PLATFORM_WINXX 0
#undef PLATFORM_XENON
#define PLATFORM_XENON 1
#undef PLATFORM_PATH_SEPARATOR
#define PLATFORM_PATH_SEPARATOR "\\"
#endif

#if defined(_SEKRIT)

#undef PLATFORM_WIN64
#define PLATFORM_WIN64 0
#undef PLATFORM_WINXX
#define PLATFORM_WINXX 0
#undef PLATFORM_SEKRIT
#define PLATFORM_SEKRIT 1

#endif


// See if we're PS2
#ifdef _PSX2
#undef PLATFORM_PS2
#define PLATFORM_PS2 1
#endif


// See if we're PSP
#ifdef __psp__
#undef PLATFORM_PSP
#define PLATFORM_PSP 1
#endif


#if defined(__psp2__) || defined(SN_TARGET_PSP2)
#undef PLATFORM_PSP2
#define PLATFORM_PSP2 1
#endif

#if defined(GRANNY_IPHONE)
#undef PLATFORM_IPHONE
#define PLATFORM_IPHONE 1
#endif

#if GENERIC_ARM9
#undef PLATFORM_GENERIC_ARM9
#define PLATFORM_GENERIC_ARM9 1
#endif



// See if we're PS3
#ifdef __CELLOS_LV2__
#undef PLATFORM_PS3
#define PLATFORM_PS3 1

    // Double check that pointers are 32-bit
    #if !defined(__LP32__)
    #error "Granny supports only the 32bit PS3 ABI"
    #endif

#if defined(__SPU__)
// Don't use the memory intrinsics on the SPU for now...
#undef PLATFORM_HAS_MEMORY_INTRINSICS
#define PLATFORM_HAS_MEMORY_INTRINSICS 0

// But do mark this so we can detect it.
#undef PLATFORM_PS3_SPU
#define PLATFORM_PS3_SPU 1

#endif

#endif


// See if we're GAMECUBE
#ifdef _GAMECUBE
#undef PLATFORM_GAMECUBE
#define PLATFORM_GAMECUBE 1
#endif

// Or wii...
#if defined(REVOLUTION)
#undef PLATFORM_WII
#define PLATFORM_WII 1
#endif

// Or wiiu...
#if defined(CAFE)
#undef PLATFORM_WIIU
#define PLATFORM_WIIU 1
#endif

// Or 3ds
#if defined(NN_PLATFORM_CTR)
#undef PLATFORM_N3DS
#define PLATFORM_N3DS 1
#endif


#if (!PLATFORM_WIN32 && !PLATFORM_WIN64 && !PLATFORM_MACOSX &&                                      \
     !PLATFORM_XENON && !PLATFORM_SEKRIT && !PLATFORM_PS2 && !PLATFORM_PSP &&                      \
     !PLATFORM_PS3 && !PLATFORM_GAMECUBE && !PLATFORM_WII && !PLATFORM_WIIU && !PLATFORM_N3DS &&    \
     !PLATFORM_LINUX && !PLATFORM_IPHONE && !PLATFORM_GENERIC_ARM9 && !PLATFORM_PSP2 &&             \
     !PLATFORM_ANDROID && !PLATFORM_NACL)
#error Unrecognized platform (see granny_platform.h)
#endif

#if ((PLATFORM_WIN32 + PLATFORM_WIN64 + PLATFORM_MACOSX +                               \
      PLATFORM_XENON + PLATFORM_SEKRIT + PLATFORM_PS2 + PLATFORM_PSP + PLATFORM_PS3 +  \
      PLATFORM_GAMECUBE + PLATFORM_WII + PLATFORM_WIIU + PLATFORM_N3DS +                \
      PLATFORM_LINUX + PLATFORM_IPHONE + PLATFORM_GENERIC_ARM9 +                        \
      PLATFORM_PSP2 + PLATFORM_ANDROID + PLATFORM_NACL) != 1)
#error Multiple platforms defined! (see granny_platform.h)
#endif

// PLATFORM_WINXX means either WIN32 or WIN64
#if PLATFORM_WIN32 || PLATFORM_WIN64
#if !PLATFORM_WINXX
#error PLATFORM_WINXX not defined correctly (see granny_platform.h)
#endif
#else
#if PLATFORM_WINXX
#error PLATFORM_WINXX not defined correctly (see granny_platform.h)
#endif
#endif

// Set the multithreading flag...
#if (PLATFORM_WIN32 || PLATFORM_WIN64 || PLATFORM_WINXX || PLATFORM_XENON || PLATFORM_SEKRIT || PLATFORM_PS3)
  #undef PLATFORM_SUPPORTS_MULTITHREADING
  #undef GRANNY_THREADED
  #define GRANNY_THREADED 1
  #define PLATFORM_SUPPORTS_MULTITHREADING 1
#else
  #undef GRANNY_THREADED
  #undef PLATFORM_SUPPORTS_MULTITHREADING
  #define PLATFORM_SUPPORTS_MULTITHREADING 0
#endif

#include "header_postfix.h"
#define GRANNY_PLATFORM_H
#endif
