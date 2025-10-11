#if !defined(GRANNY_EXPORTER_INFO_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_exporter_info.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_DATA_TYPE_DEFINITION_H)
#include "granny_data_type_definition.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(ExporterInfoGroup);

EXPTYPE struct exporter_info
{
    char *ExporterName;

    int32 ExporterMajorRevision;
    int32 ExporterMinorRevision;
    int32 ExporterCustomization;
    int32 ExporterBuildNumber;

    variant ExtendedData;
};
EXPCONST EXPGROUP(exporter_info) extern data_type_definition ExporterInfoType[];

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_EXPORTER_INFO_H
#endif
