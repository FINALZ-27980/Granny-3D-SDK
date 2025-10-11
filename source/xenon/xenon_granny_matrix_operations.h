#if !defined(XENON_GRANNY_MATRIX_OPERATIONS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/xenon/xenon_granny_matrix_operations.h $
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

// "Aligned" here means 16-byte alignment. The 101 parameters refer to the arguments, in
// order.  First function is commented for clarity.

void XenonAligned101ColumnMatrix4x3Multiply(real32 * NOALIAS Result,  // aligned
                                            real32 const* NOALIAS A,  // unaligned
                                            real32 const* NOALIAS B); // aligned

void XenonAligned110ColumnMatrix4x3Multiply(real32 * NOALIAS Result,
                                            real32 const* NOALIAS A,
                                            real32 const* NOALIAS B);


void XenonAlignedColumnMatrix4x3Multiply(real32 * NOALIAS Result,
                                         real32 const* NOALIAS A,
                                         real32 const* NOALIAS B);
void XenonUnalignedColumnMatrix4x3Multiply(real32 * NOALIAS Result,
                                           real32 const* NOALIAS A,
                                           real32 const* NOALIAS B);


void XenonAligned101ColumnMatrix4x3MultiplyTranspose(real32 * NOALIAS Result,
                                                     real32 const* NOALIAS A,
                                                     real32 const* NOALIAS B);
void XenonUnalignedColumnMatrix4x3MultiplyTranspose(real32 * NOALIAS Result,
                                                    real32 const* NOALIAS A,
                                                    real32 const* NOALIAS B);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define XENON_GRANNY_MATRIX_OPERATIONS_H
#endif /* XENON_GRANNY_MATRIX_OPERATIONS_H */

