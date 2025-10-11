// ========================================================================
// $File: //jeffr/granny_29/rt/granny_file_writer.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_file_writer.h"

#include "granny_assert.h"
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

#define SubsystemCode FileWritingLogMessage

USING_GRANNY_NAMESPACE;

bool GRANNY InitializeFileWriter(delete_file_writer_callback*   DeleteFileWriterCallback,
                                 seek_file_writer_callback*     SeekWriterCallback,
                                 write_file_writer_callback*    WriteCallback,
                                 begincrc_file_writer_callback* BeginCRCCallback,
                                 endcrc_file_writer_callback*   EndCRCCallback,
                                 file_writer& Writer)
{
    CheckPointerNotNull(DeleteFileWriterCallback, return false);
    CheckPointerNotNull(SeekWriterCallback,       return false);
    CheckPointerNotNull(WriteCallback,            return false);
    CheckPointerNotNull(BeginCRCCallback,         return false);
    CheckPointerNotNull(EndCRCCallback,           return false);

    Writer.DeleteFileWriterCallback = DeleteFileWriterCallback;
    Writer.SeekWriterCallback       = SeekWriterCallback;
    Writer.WriteCallback            = WriteCallback;
    Writer.BeginCRCCallback         = BeginCRCCallback;
    Writer.EndCRCCallback           = EndCRCCallback;

    Writer.CRCing = false;
    Writer.CRC    = 0;

    return true;
}

open_file_writer_callback *GRANNY
GetDefaultFileWriterOpenCallback()
{
    return(OpenFileWriterCallback);
}

void GRANNY
SetDefaultFileWriterOpenCallback(open_file_writer_callback *OpenFileWriterCallbackInit)
{
    OpenFileWriterCallback = OpenFileWriterCallbackInit;
}


int32x GRANNY
PredictWriterAlignment(int32x Position, int32x Alignment)
{
    // Bad.
    CheckCondition((Alignment & (Alignment-1)) == 0, return Position);

    // Determine where we are, and where we'd like to be
    int32x const NewPosition = (Position + (Alignment-1)) & ~(Alignment-1);

    // Return the difference
    return(NewPosition - Position);
}

int32x GRANNY
AlignWriterTo(file_writer* Writer, uint32x Alignment)
{
    return AlignWriterToWith(Writer, Alignment, 0);
}

int32x GRANNY
AlignWriterToWith(file_writer* Writer, uint32x Alignment, uint8 PadByte)
{
    CheckPointerNotNull(Writer, return 0);
    CheckCondition((Alignment & (Alignment - 1)) == 0, return 0);

    // Determine how far we'll have to go to get to where we want to be
    uint32 Position = GetWriterPosition(Writer);
    uint32 Aligned  = (Position + (Alignment-1)) & ~(Alignment-1);
    Assert(Aligned >= Position);
    Assert((Aligned - Position) < Alignment);

    int32x const Correction = Aligned - Position;

    while (Position != Aligned)
    {
        WriteBytes(Writer, 1, &PadByte);
        ++Position;
    }

    return Correction;
}

int32x GRANNY
SeekWriterFromStartStub(file_writer* Writer,
                        int32x OffsetInUInt8s)
{
    CheckPointerNotNull(Writer, return 0);
    return (*Writer->SeekWriterCallback)(Writer, OffsetInUInt8s, SeekStart);
}

int32x GRANNY
SeekWriterFromEndStub(file_writer* Writer,
                      int32x OffsetInUInt8s)
{
    CheckPointerNotNull(Writer, return 0);
    return (*Writer->SeekWriterCallback)(Writer, OffsetInUInt8s, SeekEnd);
}

int32x GRANNY
SeekWriterFromCurrentPositionStub(file_writer* Writer,
                                  int32x OffsetInUInt8s)
{
    CheckPointerNotNull(Writer, return 0);
    return (*Writer->SeekWriterCallback)(Writer, OffsetInUInt8s, SeekCurrent);
}

file_writer* GRANNY
CreatePlatformFileWriter(char const *FileNameToOpen, bool EraseExisting)
{
    return CreatePlatformFileWriterInternal(FileNameToOpen, EraseExisting);
}
