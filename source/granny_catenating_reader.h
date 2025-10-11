#if !defined(GRANNY_CATENATING_READER_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_catenating_reader.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

struct file_reader;
file_reader* CreateCatenatingReader(int32x        NumReaders,
                                    file_reader** Readers,
                                    int32x        Alignment);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_CATENATING_READER_H
#endif
