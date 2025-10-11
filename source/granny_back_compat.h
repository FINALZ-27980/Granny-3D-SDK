#if !defined(GRANNY_BACK_COMPAT_H)
#include "header_preamble.h"
// ========================================================================
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

struct file_info;
struct data_type_definition;
struct memory_arena;

EXPAPI GS_PARAM data_type_definition* GetFileInfoType();
EXPAPI GS_PARAM bool ClearOldStructures(file_info* Info, memory_arena* Arena);

END_GRANNY_NAMESPACE;


#include "header_postfix.h"
#define GRANNY_BACK_COMPAT_H
#endif /* GRANNY_BACK_COMPAT_H */
