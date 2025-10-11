// ========================================================================
// $File: //jeffr/granny_29/rt/granny_file_reader.cpp $
// $DateTime: 2012/10/16 14:34:17 $
// $Change: 39810 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_file_reader.h"
#include "granny_parameter_checking.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode FileReadingLogMessage

USING_GRANNY_NAMESPACE;

bool GRANNY
InitializeFileReader(close_file_reader_callback *CloseFileReaderCallback,
                     read_at_most_callback *ReadAtMostCallback,
                     get_reader_size_callback* GetReaderSizeCallback,
                     file_reader &Reader)
{
    CheckPointerNotNull(CloseFileReaderCallback,   return false);
    CheckPointerNotNull(ReadAtMostCallback,    return false);
    CheckPointerNotNull(GetReaderSizeCallback, return false);

    Reader.CloseFileReaderCallback = CloseFileReaderCallback;
    Reader.ReadAtMostCallback = ReadAtMostCallback;
    Reader.GetReaderSizeCallback = GetReaderSizeCallback;

    return true;
}

open_file_reader_callback *GRANNY
GetDefaultFileReaderOpenCallback(void)
{
    return(OpenFileReaderCallback);
}

void GRANNY
SetDefaultFileReaderOpenCallback(
    open_file_reader_callback *OpenFileReaderCallbackInit)
{
    OpenFileReaderCallback = OpenFileReaderCallbackInit;
}

file_reader* GRANNY
CreatePlatformFileReader(char const *FileNameToOpen)
{
    return CreatePlatformFileReaderInternal(FileNameToOpen);
}


void GRANNY
SetOSSpecificFileParameters(void*)
{
    
}
