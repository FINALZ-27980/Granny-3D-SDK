#if !defined(GRANNY_STRING_TABLE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_string_table.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(StringTableGroup);

struct memory_arena;
EXPTYPE struct string_table;

EXPAPI GS_SAFE string_table* NewStringTable();
EXPAPI GS_SAFE string_table* NewArenaStringTable(memory_arena* Arena);
EXPAPI GS_PARAM void FreeStringTable(string_table *Table);

EXPAPI GS_PARAM char const *MapString(string_table &Table, char const *String);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_STRING_TABLE_H
#endif
