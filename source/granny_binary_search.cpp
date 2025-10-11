// ========================================================================
// $File: //jeffr/granny_29/rt/granny_binary_search.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_binary_search.h"

#include "granny_assert.h"
#include "granny_parameter_checking.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode Undefined_LogMessage
USING_GRANNY_NAMESPACE;


void GRANNY
InitializeSearch(binary_searcher& Searcher, int32x Count)
{
    if (Count <= 0)
    {
        // Zero search
        Searcher.Base   = 0;
        Searcher.Window = 0;
    }
    else
    {
        Searcher.Base   = 0;
        Searcher.Window = Count;
    }
}

bool GRANNY
SearchRemaining(binary_searcher& Searcher)
{
    return (Searcher.Window > 0);
}

int32x GRANNY
GetSearchProbe(binary_searcher& Searcher)
{
    int32x Halfway = Searcher.Base + (Searcher.Window / 2);

    Assert(Halfway >= Searcher.Base &&
           Halfway < (Searcher.Base + Searcher.Window));

    return Halfway;
}

void GRANNY
UpdateLessThan(binary_searcher& Searcher, int32x Probe)
{
    Assert(SearchRemaining(Searcher));
    Assert(Probe >= Searcher.Base &&
           Probe < Searcher.Base + Searcher.Window);

    Searcher.Window = Probe - Searcher.Base;
}

void GRANNY
UpdateGreaterThan(binary_searcher& Searcher, int32x Probe)
{
    Assert(SearchRemaining(Searcher));
    Assert(Probe >= Searcher.Base &&
           Probe < Searcher.Base + Searcher.Window);

    Searcher.Window -= (Probe - Searcher.Base + 1);
    Searcher.Base    = Probe + 1;
}
