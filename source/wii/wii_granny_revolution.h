#if !defined(WII_GRANNY_REVOLUTION_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/wii/wii_granny_revolution.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#include <revolution.h>

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

#define WiiLogErrorAsWarning(FailedWiiFunction)                     \
    WiiLogLastError(false, __FILE__, __LINE__, #FailedWiiFunction);
#define WiiLogErrorAsError(FailedWiiFunction)                       \
    WiiLogLastError(true, __FILE__, __LINE__, #FailedWiiFunction);

// The following functions should not be called directly.  You
// should use the macros defined above.
void WiiLogLastError(bool IsError,
                     char const *SourceFile, int32x SourceLineNumber,
                     char const *FailedWiiFunction);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define WII_GRANNY_REVOLUTION_H
#endif
