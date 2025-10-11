// ========================================================================
// $File: //jeffr/granny_29/rt/wii/wii_granny_file_writer.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_assert.h"
#include "granny_file_writer.h"
#include "wii_granny_revolution.h"

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


file_writer *GRANNY
CreatePlatformFileWriterInternal(char const *FileNameToOpen, bool EraseExisting)
{
    InvalidCodePath("No write support on Wii as of yet");

    return NULL;
}

open_file_writer_callback *GRANNY OpenFileWriterCallback = CreatePlatformFileWriterInternal;
