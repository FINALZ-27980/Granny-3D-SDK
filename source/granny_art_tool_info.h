#if !defined(GRANNY_ART_TOOL_INFO_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_art_tool_info.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_DATA_TYPE_DEFINITION_H)
#include "granny_data_type_definition.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(ArtToolInfoGroup);

EXPTYPE struct art_tool_info
{
    char const *FromArtToolName;
    int32 ArtToolMajorRevision;
    int32 ArtToolMinorRevision;
    int32 ArtToolPointerSize;

    real32 UnitsPerMeter;
    triple Origin;
    triple RightVector;
    triple UpVector;
    triple BackVector;

    variant ExtendedData;
};
EXPCONST EXPGROUP(art_tool_info) extern data_type_definition ArtToolInfoType[];

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_ART_TOOL_INFO_H
#endif
