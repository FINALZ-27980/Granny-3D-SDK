// ========================================================================
// $File: //jeffr/granny_29/rt/granny_version_checking.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_version_checking.h"
#include "granny_log.h"

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

char const *GRANNY
GetVersionString(void)
{
    return ProductVersion;
}

void GRANNY
GetVersion(int32x &MajorVersion,
           int32x &MinorVersion,
           int32x &BuildNumber,
           int32x &Customization)
{
    MajorVersion  = ProductMajorVersion;
    MinorVersion  = ProductMinorVersion;
    BuildNumber   = ProductBuildNumber;
    Customization = ProductCustomization;
}

bool GRANNY
VersionsMatch_(int32x MajorVersion, int32x MinorVersion,
               int32x BuildNumber, int32x Customization)
{
    if ((MajorVersion  == ProductMajorVersion) &&
        (MinorVersion  == ProductMinorVersion) &&
        (BuildNumber   == ProductBuildNumber)  &&
        (Customization == ProductCustomization))
    {
        return true;
    }
    else
    {
        Log8(WarningLogMessage, VersionLogMessage,
             "Version of DLL (%d.%d.%d.%d) does not match "
             "version of .h (%d.%d.%d.%d)",
             ProductMajorVersion, ProductMinorVersion,
             ProductBuildNumber,  ProductCustomization, 
             MajorVersion, MinorVersion,
             BuildNumber,  Customization);

        return false;
    }
}
