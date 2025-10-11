// Note missing header guards, we want this to include every time...

// ========================================================================
// $File: //jeffr/granny_29/rt/header_preamble.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_COMPILER_H)
#include "granny_compiler.h"
#endif

// Structure packing explicitly set to 4
#if COMPILER_MSVC || defined(__x86_64__)

#pragma pack(push)
#pragma pack(4)

#endif
