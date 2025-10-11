// ========================================================================
// $File: //jeffr/granny_29/rt/winxx/winxx_granny_assert.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

// Covers full file
#if DEBUG

#include "winxx_granny_windows.h"
#include "granny_assert.h"
#include "granny_limits.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

USING_GRANNY_NAMESPACE;

static char AssertionBuffer[MaximumMessageBufferSize];
bool GRANNY
DisplayAssertion(char const * const Expression,
                 char const * const File,
                 int32x const LineNumber,
                 char const * const Function,
                 bool* IgnoredPtr,
                 void* /*UserData*/)
{
    bool& Ignored = *IgnoredPtr;
    if (Ignored)
        return false;

    wsprintfA(AssertionBuffer,
              "Expression: %s\r\n"
              "File: %s\r\n"
              "Line: %d\r\n"
              "Function: %s\r\n"
              "\r\n"
              "(Abort = debug, Retry = ignore once, Ignore = ignore forever)\r\n",
              Expression, File, LineNumber, Function);

    DWORD ReturnCode = WinXXPopMessageBox(AssertionBuffer, "Run-time Error", eAbortRetryIgnore);
    switch (ReturnCode)
    {
        default:
        case IDABORT:
        {
            // We break into the debugger
            return true;
        } break;

        case IDIGNORE:
        {
            // We don't ever assert here again
            Ignored = true;
            return false;
        } break;

        case IDRETRY:
        {
            // We let things go
            return false;
        } break;
    }
}

#endif // DEBUG
