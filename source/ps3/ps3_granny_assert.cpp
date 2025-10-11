// ========================================================================
// $File: //jeffr/granny_29/rt/ps3/ps3_granny_assert.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if DEBUG

#include "ps3_granny_std.h"
#include "granny_assert.h"
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
        // GRANNY_PORTING
        // You should replace this with the appropriate code for your
        // dev environment to drop to the debugger, or halt with a
        // message for testers.  As a default, hard crash!
        RR_BREAK();
    }

    return true;
}

#endif // DEBUG
