// ========================================================================
// $File: //jeffr/granny_29/rt/granny_art_tool_info.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_art_tool_info.h"

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

BEGIN_GRANNY_NAMESPACE;

data_type_definition ArtToolInfoType[] =
{
    { StringMember, "FromArtToolName"      },
    { Int32Member,  "ArtToolMajorRevision" },
    { Int32Member,  "ArtToolMinorRevision" },
    { Int32Member,  "ArtToolPointerSize"   },

    { Real32Member, "UnitsPerMeter"     },
    { Real32Member, "Origin",      0, 3 },
    { Real32Member, "RightVector", 0, 3 },
    { Real32Member, "UpVector",    0, 3 },
    { Real32Member, "BackVector",  0, 3 },

    { VariantReferenceMember, "ExtendedData" },

    { EndMember },
};

END_GRANNY_NAMESPACE;
