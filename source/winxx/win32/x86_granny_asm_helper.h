#if !defined(X86_GRANNY_ASM_HELPER_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/x86/x86_granny_asm_helper.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if COMPILER_MSVC
  #define FORCEINLINE __forceinline
  #define ASMSIZE(x) (size x)
  #define STRUCTACCESSOFFSET(src_reg, offset, struct_type, member) [src_reg + offset]struct_type ## . ## member
  #define STRUCTACCESS(src_reg, struct_type, member) STRUCTACCESSOFFSET(src_reg, 0, struct_type, member)
  #define ASMALIGN(n)  ALIGN n
#elif COMPILER_GCC
  #define FORCEINLINE __attribute((always_inline))
  #define ASMSIZE(x) sizeof(x)
  #define STRUCTACCESS(src_reg, struct_type, member) [src_reg + offsetof(struct_type, member)]
  #define STRUCTACCESSOFFSET(src_reg, offset, struct_type, member) [src_reg + offsetof(struct_type, member) + offset]
  #define ASMALIGN(n)
#endif

#include "header_postfix.h"
#define X86_GRANNY_ASM_HELPER_H
#endif /* X86_GRANNY_ASM_HELPER_H */
