// ========================================================================
// $File: //jeffr/granny_29/rt/ansi/ansi_granny_memory.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "ansi_granny_std.h"
#include "granny_memory.h"

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

void *GRANNY
PlatformAllocate(uintaddrx Size)
{
    return(malloc(Size));
}

void GRANNY
PlatformDeallocate(void *Memory)
{
    free(Memory);
}
