// ========================================================================
// $File: //jeffr/granny_29/rt/xenon/xenon_granny_assert.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#if DEBUG
#include "granny_assert.h"
#include "granny_limits.h"
#include "granny_types.h"
#include "xenon_granny_xtl.h"
#include "rrCore.h"

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

char AssertionBuffer[MaximumMessageBufferSize];
bool GRANNY
DisplayAssertion(char const * const Expression,
                 char const * const File,
                 int32x const LineNumber,
                 char const * const Function,
                 bool* IgnoreAssertion,
                 void* UserData)
{
    if (IgnoreAssertion == 0 || !*IgnoreAssertion)
    {
        wsprintf(AssertionBuffer, "%s:%d \"%s\"", File, LineNumber, Expression);
        OutputDebugStringA(AssertionBuffer);
        return true;
    }

    return false;
}
#endif
