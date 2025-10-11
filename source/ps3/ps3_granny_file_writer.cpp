// ========================================================================
// $File: //jeffr/granny_29/rt/ps3/ps3_granny_file_writer.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "ps3_granny_std.h"
#include "granny_file_writer.h"
#include "granny_memory.h"
#include "granny_assert.h"
#include "granny_crc.h"

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

// GRANNY_PORTING
// Note that we don't support writing by default on the PS3.
//  if you'd like to use the ANSI standard fopen, etc. simply
//  replace this file with ansi_granny_file_writer.cpp and
//  the associated utilities, and you're good to go.

file_writer* GRANNY
CreatePlatformFileWriterInternal(char const *FileNameToOpen, bool EraseExisting)
{
    Assert ( !"No Granny file writers included by default" );
    return 0;
}

open_file_writer_callback *GRANNY OpenFileWriterCallback = CreatePlatformFileWriterInternal;
