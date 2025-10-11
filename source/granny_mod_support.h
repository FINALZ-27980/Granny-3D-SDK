#if !defined(GRANNY_MOD_SUPPORT_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_mod_support.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(ModGroup);

#if BUILDING_GRANNY_FOR_MODS

// This should be 4 random hex digits that will identify your game's files.
#if !defined(MOD_VERSION_TAG)
  #error "MOD_VERSION_TAG must be defined as 4 random hex digits"
  // #define MOD_VERSION_TAG 0x481f
#endif

// Three letters/numbers that are not GR2
#if !defined(MOD_FILE_EXTENSION)
  #error MOD_FILE_EXTENSION must be defined as a 3LE for your files: i.e, "MGF"
  // #define MOD_FILE_EXTENSION "MGF"
#endif

// This string will prevent the type info from matching granny::file_info
#if !defined(MOD_UNIQUE_PREFIX)
  #error "MOD_UNIQUE_PREFIX must be a short unique string for your application"
  // #define MOD_UNIQUE_PREFIX "UniqShortString"
#endif

#if !defined(MOD_NAME_BASE)
  #error "MOD_NAME_BASE must be defined"
  // #define MOD_NAME_BASE "YourGame"
#endif

#define MOD_VIEWER_NAME   MOD_NAME_BASE " Viewer"
#define MOD_EXPORTER_NAME MOD_NAME_BASE " Exporter"

CompileAssert((MOD_VERSION_TAG) > 0x0 && (MOD_VERSION_TAG) < 0x10000);

// So... this is a little awkward, obviously.  The external version of this must refer to
// GrannyCurrent... not the Current... version, hence the little dance here.
#define CurrentMODStandardTag ((GrannyCurrentGRNStandardTag) | ((MOD_VERSION_TAG) << 15)) EXPMACRO
#undef CurrentMODStandardTag
#define CurrentMODStandardTag ((CurrentGRNStandardTag) | ((MOD_VERSION_TAG) << 15))

#endif

struct file;
struct file_info;
struct data_type_definition;

EXPAPI GS_PARAM file_info* GetModFileInfo(file& File);

EXPCONST EXPGROUP(GetModFileInfo) extern data_type_definition ModFileInfoType[];

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MOD_SUPPORT_H
#endif /* GRANNY_MOD_SUPPORT_H */
