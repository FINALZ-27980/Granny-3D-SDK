#if !defined(GRANNY_VERSION_H)
// #include "header_preamble.h" keep this simple for the rc compiler
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_version.h $
// $DateTime: 2012/11/02 15:18:08 $
// $Change: 40197 $
// $Revision: #43 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#include "granny_exp_interface.h"

EXPGROUP(VersionGroup)

#define ProductVersion "2.9.12.0"  EXPMACRO
#define ProductMajorVersion    2  EXPMACRO
#define ProductMinorVersion    9  EXPMACRO
#define ProductBuildNumber     12  EXPMACRO
#define ProductCustomization   0  EXPMACRO

#define ProductReleaseName release EXPMACRO
#define ProductReleaseString "release"

#define ProductCompanyName "RAD Game Tools, Inc." EXPMACRO
#define ProductName "Granny" EXPMACRO
#define ProductCopyrightYears "1999-2011"
#define ProductCopyright "1999-2011 by RAD Game Tools, Inc., All Rights Reserved."  EXPMACRO
#define ProductTrademarks ProductName " is a registered trademark of " ProductCompanyName EXPMACRO
#define ProductComments "Who's your Granny?" EXPMACRO

#define ProductSupportAddress "granny3@radgametools.com" EXPMACRO
#define ProductSupportPage "www.radgametools.com/granny.html" EXPMACRO

#define CallbackMacro "GRANNY_CALLBACK"

// #include "header_postfix.h" keep this simple for the rc compiler
#define GRANNY_VERSION_H
#endif
