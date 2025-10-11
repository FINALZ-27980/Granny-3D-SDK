#if !defined(GRANNY_FILE_OPERATIONS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_file_operations.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

// This prevents windows.h from stealing the name
#undef DeleteFile

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(FileBuilderGroup);

struct file_writer;
struct file_reader;

EXPAPI GS_SAFE char const *GetTemporaryDirectory(void);
void DeleteFile(char const *FileName);
bool AppendStringToFile(char const* FileName, char const* String);
bool ConcatenateFile(file_writer* Writer, char const *FileName, uint32x* BytesCopied);
bool ConcatenateFileReader(file_writer* Writer,
                           file_reader* Reader,
                           uint32x* BytesCopied);
int32x GetFileSize(char const *FileName);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_FILE_OPERATIONS_H
#endif
