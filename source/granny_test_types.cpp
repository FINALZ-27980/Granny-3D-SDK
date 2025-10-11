// ========================================================================
// $File: //jeffr/granny_29/rt/granny_test_types.cpp $
// $DateTime: 2012/04/03 14:17:31 $
// $Change: 36922 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_test_types.h"

#include "granny_assert.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define NUM_BITS(t) (sizeof(t) * 8)

// Check the size of our types to make sure there what we expect...

BEGIN_GRANNY_NAMESPACE;

// See the comment in dll_header.h near the #define bool for C code
#if (defined(_GAMECUBE) || defined(_PSX2) || (defined(_MACOSX) && defined(__ppc__)))
   CompileAssert(NUM_BITS(bool) == 32);
#else
   CompileAssert(NUM_BITS(bool) == 8);
#endif

#if PROCESSOR_32BIT_POINTER
  CompileAssert(sizeof(void*) == 4);
#elif PROCESSOR_64BIT_POINTER
  CompileAssert(sizeof(void*) == 8);
#else
  #error "Pointer size not set!"
#endif
CompileAssert(sizeof(intaddrx) == sizeof(void*));

// exact
CompileAssert(NUM_BITS(uint64) == 64);
CompileAssert(NUM_BITS(uint32) == 32);
CompileAssert(NUM_BITS(uint16) == 16);
CompileAssert(NUM_BITS(uint8)  == 8);
CompileAssert(NUM_BITS(int64)  == 64);
CompileAssert(NUM_BITS(int32)  == 32);
CompileAssert(NUM_BITS(int16)  == 16);
CompileAssert(NUM_BITS(int8)   == 8);
CompileAssert(NUM_BITS(real32) == 32);

// "at least"
CompileAssert(NUM_BITS(uint64x) >= 64);
CompileAssert(NUM_BITS(uint32x) >= 32);
CompileAssert(NUM_BITS(uint16x) >= 16);
CompileAssert(NUM_BITS(uint8x)  >= 8);
CompileAssert(NUM_BITS(int64x)  >= 64);
CompileAssert(NUM_BITS(int32x)  >= 32);
CompileAssert(NUM_BITS(int16x)  >= 16);
CompileAssert(NUM_BITS(int8x)   >= 8);
CompileAssert(NUM_BITS(real64x) >= 64);


END_GRANNY_NAMESPACE;

