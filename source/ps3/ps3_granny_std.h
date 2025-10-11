#if !defined(PS3_GRANNY_STD_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/ps3/ps3_granny_std.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <errno.h>

BEGIN_GRANNY_NAMESPACE;

// PS3LogErrorAsWarning and PS3LogErrorAsError will check the last
// error code.  If it is set, it will log the reported string as
// a warning or an error (respectively) in the error log.
#define PS3LogErrorAsWarning(FailedPS3Function) \
        PS3LogLastError(false, __FILE__, __LINE__, #FailedPS3Function);
#define PS3LogErrorAsError(FailedPS3Function) \
        PS3LogLastError(true, __FILE__, __LINE__, #FailedPS3Function);

int32x PS3Seek(FILE *PS3FileHandle, int32x Offset, int32x MoveMethod);

// The following functions should not be called directly.  You
// should use the macros defined above.
void PS3LogLastError(bool IsError,
                      char const *SourceFile, int32x SourceLineNumber,
                      char const *FailedPS3Function);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define PS3_GRANNY_STD_H
#endif
