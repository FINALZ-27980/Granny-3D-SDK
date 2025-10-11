#if !defined(GRANNY_MATRIX_OPERATIONS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_matrix_operations.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GRANNY_OPTIMIZED_DISPATCH_H)
#include "granny_optimized_dispatch.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(MathGroup);

EXPAPI GS_PARAM void ColumnMatrixMultiply4x3(real32 *IntoMatrix4x4,
                                             real32 const *Matrix4x4,
                                             real32 const *ByMatrix4x4);
EXPAPI GS_PARAM void ColumnMatrixMultiply4x3Transpose(real32 *IntoMatrix3x4,
                                                      real32 const *Matrix4x4,
                                                      real32 const *ByMatrix4x4);
EXPAPI GS_PARAM void ColumnMatrixMultiply4x4(real32 *IntoMatrix4x4,
                                             real32 const *Matrix4x4,
                                             real32 const *ByMatrix4x4);


typedef void OPTIMIZED_DISPATCH_TYPE(ColumnMatrixMultiply4x3Impl)(real32 *IntoMatrix4x4,
                                                                  real32 const *Matrix4x4,
                                                                  real32 const *ByMatrix4x4);
typedef void OPTIMIZED_DISPATCH_TYPE(ColumnMatrixMultiply4x3TransposeImpl)(real32 *IntoMatrix3x4,
                                                                           real32 const *Matrix4x4,
                                                                           real32 const *ByMatrix4x4);
typedef void OPTIMIZED_DISPATCH_TYPE(ColumnMatrixMultiply4x4Impl)(real32 *IntoMatrix4x4,
                                                                  real32 const *Matrix4x4,
                                                                  real32 const *ByMatrix4x4);

extern OPTIMIZED_DISPATCH(ColumnMatrixMultiply4x3Impl);
extern OPTIMIZED_DISPATCH(ColumnMatrixMultiply4x3TransposeImpl);
extern OPTIMIZED_DISPATCH(ColumnMatrixMultiply4x4Impl);

DECL_GENERIC_DISPATCH(ColumnMatrixMultiply4x3Impl)(real32 *IntoMatrix4x4,
                                                   real32 const *Matrix4x4,
                                                   real32 const *ByMatrix4x4);
DECL_GENERIC_DISPATCH(ColumnMatrixMultiply4x3TransposeImpl)(real32 *IntoMatrix3x4,
                                                            real32 const *Matrix4x4,
                                                            real32 const *ByMatrix4x4);
DECL_GENERIC_DISPATCH(ColumnMatrixMultiply4x4Impl)(real32 *IntoMatrix4x4,
                                                   real32 const *Matrix4x4,
                                                   real32 const *ByMatrix4x4);


void TestFastMatrixOps();

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MATRIX_OPERATIONS_H
#endif

