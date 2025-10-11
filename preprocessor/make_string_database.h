#if !defined(MAKE_STRING_DATABASE_H)
// ========================================================================
// $File: //jeffr/granny_29/preprocessor/make_string_database.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#include "preprocessor.h"

bool ExtractStrings(input_file*     InputFiles,
                    granny_int32x   NumInputFiles,
                    key_value_pair* KeyValues,
                    granny_int32x   NumKeyValues,
                    granny_memory_arena* TempArena);

bool RemapStrings(input_file& InputFile,
                  key_value_pair* KeyValues,
                  granny_int32x   NumKeyValues,
                  granny_memory_arena* TempArena);

#define MAKE_STRING_DATABASE_H
#endif /* MAKE_STRING_DATABASE_H */
