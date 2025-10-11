#if !defined(GRANNY_VERSION_CHECKING_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_version_checking.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GRANNY_VERSION_H)
#include "granny_version.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(VersionGroup);

EXPAPI GS_SAFE char const *GetVersionString(void);
EXPAPI GS_SAFE void GetVersion(int32x &MajorVersion, int32x &MinorVersion,
                               int32x &BuildNumber,  int32x &Customization);
EXPAPI GS_SAFE bool VersionsMatch_(int32x MajorVersion, int32x MinorVersion,
                                   int32x BuildNumber,  int32x Customization);

#define VersionsMatch GrannyVersionsMatch_(GrannyProductMajorVersion, GrannyProductMinorVersion, GrannyProductBuildNumber, GrannyProductCustomization) EXPMACRO

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_VERSION_CHECKING_H
#endif
