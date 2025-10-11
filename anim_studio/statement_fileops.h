// ========================================================================
// $File: //jeffr/granny_29/statement/statement_fileops.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(STATEMENT_FILEOPS_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

bool
QueryForReadableFile(int BufferSize, char *Buffer,
                     bool AllowMultiSelect,
                     char const *Title,
                     char const *FileTypeDescription,
                     char const *FileTypeExtension);

bool
QueryForWritableFile(int BufferSize, char *Buffer,
                     char const *Title,
                     char const *FileTypeDescription,
                     char const *FileTypeExtension);

END_GRANNY_NAMESPACE;

#define STATEMENT_FILEOPS_H
#endif /* STATEMENT_FILEOPS_H */
