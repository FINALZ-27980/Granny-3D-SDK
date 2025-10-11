#if !defined(X86_GRANNY_CPU_QUERIES_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/x86/x86_granny_cpu_queries.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

bool SSEIsAvailable(void);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define X86_GRANNY_CPU_QUERIES_H
#endif
