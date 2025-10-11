#if !defined(GRANNY_OPTIMIZED_DISPATCH_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_optimized_dispatch.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#define OPTIMIZED_DISPATCH_TYPE(function) function##_type
#define OPTIMIZED_DISPATCH(function) function##_type *function

#define DECL_GENERIC_DISPATCH(function) void function##_Generic
#define GENERIC_DISPATCH(function) void GRANNY function##_Generic

#include "header_postfix.h"
#define GRANNY_OPTIMIZED_DISPATCH_H
#endif
