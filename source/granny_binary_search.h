#if !defined(GRANNY_BINARY_SEARCH_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_binary_search.h $
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

struct binary_searcher
{
    int32x Base;
    int32x Window;
};

void InitializeSearch(binary_searcher& Searcher, int32x Count);
bool SearchRemaining(binary_searcher& Searcher);

int32x GetSearchProbe(binary_searcher& Searcher);
void   UpdateLessThan(binary_searcher& Searcher, int32x Probe);
void   UpdateGreaterThan(binary_searcher& Searcher, int32x Probe);

END_GRANNY_NAMESPACE;


#include "header_postfix.h"
#define GRANNY_BINARY_SEARCH_H
#endif
