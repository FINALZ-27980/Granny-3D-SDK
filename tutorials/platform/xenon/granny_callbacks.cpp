// ========================================================================
// A few utility functions that adapt Granny to the 360
//
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include <xtl.h>
#include <stdio.h>
#include "granny_callbacks.h"

// Generic sprintf wrapper around OutputDebugString
static void
ErrorMessage(char const * const FormatString, ...)
{
    char OutBuffer[2048];

    va_list ArgList;
    va_start(ArgList, FormatString);
    vsprintf(OutBuffer, FormatString, ArgList);
    va_end(ArgList);

    OutputDebugStringA(OutBuffer);
}


// GrannyError() gets called whenever Granny encounters a problem or
// wants to warn us about a potential problem.  The Error string
// is an explanation of the problem, but the Type and Origin
// identifiers also allow a programmatic analysis of what
// happened.
GRANNY_CALLBACK(void)
GrannyError(granny_log_message_type Type,
            granny_log_message_origin Origin,
            char const* File, granny_int32x Line,
            char const *Error, void *UserData)
{
    ErrorMessage("GRANNY: \"%s\"\n", Error);
}
