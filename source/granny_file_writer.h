#if !defined(GRANNY_FILE_WRITER_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_file_writer.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

/* ========================================================================
   Explicit Dependencies
   ======================================================================== */
#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(FileWriterGroup);

struct file_writer;

// Assign these fixed values so they can be used in exported macros
EXPTYPE enum file_writer_seek_type
{
    SeekStart   = 0,
    SeekEnd     = 1,
    SeekCurrent = 2,

    file_writer_seek_type_forceint = 0x7fffffff
};

// On creation, you must specify a file to write and a flag to specify
// whether or not to check for overwriting an existing file.  If the
// file cannot be opened for any reason, a warning is written to the
// error log and 0 is returned.
EXPAPI typedef CALLBACK_FN(file_writer *) open_file_writer_callback(char const *FileName,
                                                                   bool EraseExisting);
EXPAPI typedef CALLBACK_FN(void) delete_file_writer_callback(file_writer *Writer);
EXPAPI typedef CALLBACK_FN(int32x) seek_file_writer_callback(file_writer* Writer,
                                                             int32x OffsetInUInt8s,
                                                             int32x SeekType);  // should be file_writer_seek_type, int for expapi
EXPAPI typedef CALLBACK_FN(bool) write_file_writer_callback(file_writer* Writer,
                                               int32x UInt8Count,
                                               void const *WritePointer);
EXPAPI typedef CALLBACK_FN(void) begincrc_file_writer_callback(file_writer* Writer);
EXPAPI typedef CALLBACK_FN(uint32) endcrc_file_writer_callback(file_writer* Writer);



EXPTYPE_EPHEMERAL struct file_writer
{
    delete_file_writer_callback*   DeleteFileWriterCallback;
    seek_file_writer_callback*     SeekWriterCallback;
    write_file_writer_callback*    WriteCallback;
    begincrc_file_writer_callback* BeginCRCCallback;
    endcrc_file_writer_callback*   EndCRCCallback;

    bool32   CRCing;
    uint32 CRC;
};


#define NewFileWriter(FileName, EraseExisting) (CreatePlatformFileWriter(FileName, EraseExisting))
#define CloseFileWriter(Writer) { if (Writer) {(*(Writer)->DeleteFileWriterCallback)(Writer);} } typedef int __granny_require_semicolon EXPMACRO

// Note that we're using the fixed values from above, rather than the symbols.  CRAPI
// cannot handle replacing SeekStart -> GrannySeekStart in the exported header.
#define GetWriterPosition(Writer) (*((Writer)->SeekWriterCallback))(Writer, 0, 2) EXPMACRO

#define SeekWriterFromStart(Writer, OffsetInUInt8s)           (*((Writer)->SeekWriterCallback))(Writer, OffsetInUInt8s, 0) EXPMACRO
#define SeekWriterFromEnd(Writer, OffsetInUInt8s)             (*((Writer)->SeekWriterCallback))(Writer, OffsetInUInt8s, 1) EXPMACRO
#define SeekWriterFromCurrentPosition(Writer, OffsetInUInt8s) (*((Writer)->SeekWriterCallback))(Writer, OffsetInUInt8s, 2) EXPMACRO

// Generic writing of bytes.  If a write fails, false is returned
// and a warning is written to the error log.
#define WriteBytes(Writer, UInt8Count, WritePointer) (*((Writer)->WriteCallback))(Writer, UInt8Count, WritePointer) EXPMACRO

#define BeginWriterCRC(Writer) (*((Writer)->BeginCRCCallback))(Writer) EXPMACRO
#define EndWriterCRC(Writer)   (*((Writer)->EndCRCCallback))(Writer) EXPMACRO
#define WriterIsCRCing(Writer) (((Writer)->CRCing)) EXPMACRO

// AlignWriterTo() will move the writing pointer by as many bytes as
// necessary to ensure optimal alignment for whatever filesystem
// is being written (this is usually 32-bit alignment, but may
// be 64-bit on some systems).  It returns the number of bytes it
// moved forward.  Align is CRC aware and will write 0's instead of
// seeking if CRCing is active, to ensure that the CRC is valid.
EXPAPI GS_PARAM int32x AlignWriterTo(file_writer* Writer, uint32x Alignment);

// Still the same thing, but accepts an specified alignment and the pad character.  (Must be pow2)
EXPAPI GS_PARAM int32x AlignWriterToWith(file_writer* Writer, uint32x Alignment, uint8 PadByte);

// PredictWriterAlignment() must return what Align() would return
// if the current position was as given.  Alignment must be pow2
EXPAPI GS_SAFE int32x PredictWriterAlignment(int32x Position, int32x Alignment);


EXPAPI GS_PARAM bool InitializeFileWriter(delete_file_writer_callback*   DeleteFileWriterCallback,
                                          seek_file_writer_callback*     SeekWriterCallback,
                                          write_file_writer_callback*    WriteCallback,
                                          begincrc_file_writer_callback* BeginCRCCallback,
                                          endcrc_file_writer_callback*   EndCRCCallback,
                                          file_writer&                   Writer);

CALLBACK_FN(file_writer*) CreatePlatformFileWriterInternal(char const *FileNameToOpen, bool EraseExisting);
EXPAPI GS_PARAM file_writer *CreatePlatformFileWriter(char const *FileNameToOpen, bool EraseExisting);



// TODO: Access to some sort of list of open file handles?

extern open_file_writer_callback *OpenFileWriterCallback;

EXPAPI GS_READ open_file_writer_callback *GetDefaultFileWriterOpenCallback(void);
EXPAPI GS_MODIFY void SetDefaultFileWriterOpenCallback(
    open_file_writer_callback *OpenFileWriterCallback);



EXPAPI GS_PARAM int32x SeekWriterFromStartStub(file_writer* Writer,
                                               int32x OffsetInUInt8s);
EXPAPI GS_PARAM int32x SeekWriterFromEndStub(file_writer* Writer,
                                             int32x OffsetInUInt8s);
EXPAPI GS_PARAM int32x SeekWriterFromCurrentPositionStub(file_writer* Writer,
                                                         int32x OffsetInUInt8s);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_FILE_WRITER_H
#endif
