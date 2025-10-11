#if !defined(GRANNY_CPP_SETTINGS_H)
// "header_preamble.h" NO PREFIX FOR THIS FILE
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_cpp_settings.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

// This should always be the last header included in any Granny source
// file.  The intention is to set any packing/warning etc. settings
// that are required for the environment

#if !defined(GRANNY_COMPILER_H)
#include "granny_compiler.h"
#endif

#if !defined(GRANNY_ASSERT_H)
#include "granny_assert.h"
#endif

// Pack structures to 4-bytes...
#if (defined(COMPILER_MSVC) && COMPILER_MSVC) || defined(__x86_64__)

#pragma pack(4)

#endif

// "header_postfix.h"  NO POSTFIX FOR THIS FILE
#define GRANNY_CPP_SETTINGS_H
#endif
