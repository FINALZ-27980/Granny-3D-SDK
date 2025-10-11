// ========================================================================
// $File: //jeffr/granny_29/rt/granny_exporter_info.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_exporter_info.h"

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

data_type_definition ExporterInfoType[] =
{
    {StringMember, "ExporterName"},

    {Int32Member, "ExporterMajorRevision"},
    {Int32Member, "ExporterMinorRevision"},
    {Int32Member, "ExporterCustomization"},
    {Int32Member, "ExporterBuildNumber"},

    {VariantReferenceMember, "ExtendedData" },

    {EndMember},
};

END_GRANNY_NAMESPACE;
