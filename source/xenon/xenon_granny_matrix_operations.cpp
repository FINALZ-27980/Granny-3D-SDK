// ========================================================================
// $File: //jeffr/granny_29/rt/xenon/xenon_granny_matrix_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_matrix_operations.h"
#include "xenon_granny_matrix_operations.h"
#include "xenon_granny_xtl.h"

#include "granny_assert.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"

#include <xboxmath.h>

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode Undefined_LogMessage
USING_GRANNY_NAMESPACE;

// Set to 0 to use the generic C matrix operations
#define USE_ACCELERATED_MATRIX_OPS 1


// =====================================================================
// Factored out code chunks to be combined for different alignments

#define DECLARE_VARS                                        \
    XMVECTOR AR0, AR1, AR2, AR3;                            \
    XMVECTOR BR0, BR1, BR2, BR3;                            \
    XMVECTOR Temp0, Temp1, Temp2;                           \
    XMVECTOR ResultCol0, ResultCol1, ResultCol2, ResultCol3

#define UNALIGNED_LOAD(VecPrefix, Matrix)                                 \
    VecPrefix##0     = __vor(__lvlx(Matrix, 0  + 0), __lvrx(Matrix, 0  + 16));  \
    VecPrefix##1     = __vor(__lvlx(Matrix, 16 + 0), __lvrx(Matrix, 16 + 16));  \
    VecPrefix##2     = __vor(__lvlx(Matrix, 32 + 0), __lvrx(Matrix, 32 + 16));  \
    VecPrefix##3     = __vor(__lvlx(Matrix, 48 + 0), __lvrx(Matrix, 48 + 16))

#define ALIGNED_LOAD(VecPrefix, Matrix)         \
    VecPrefix##0 = __lvx(Matrix, 0);            \
    VecPrefix##1 = __lvx(Matrix, 16);           \
    VecPrefix##2 = __lvx(Matrix, 32);           \
    VecPrefix##3 = __lvx(Matrix, 48)

#define MATRIX_MUL                                  \
    Temp0 = __vspltw(AR0, 0);                       \
    Temp1 = __vspltw(AR0, 1);                       \
    Temp2 = __vspltw(AR0, 2);                       \
    ResultCol0 = __vmulfp(Temp0,  BR0);             \
    ResultCol0 = __vmaddfp(Temp1, BR1, ResultCol0); \
    ResultCol0 = __vmaddfp(Temp2, BR2, ResultCol0); \
    /* -- Col1 */                                   \
    Temp0 = __vspltw(AR1, 0);                       \
    Temp1 = __vspltw(AR1, 1);                       \
    Temp2 = __vspltw(AR1, 2);                       \
    ResultCol1 = __vmulfp(Temp0, BR0);              \
    ResultCol1 = __vmaddfp(Temp1, BR1, ResultCol1); \
    ResultCol1 = __vmaddfp(Temp2, BR2, ResultCol1); \
    /* -- Col2 */                                   \
    Temp0 = __vspltw(AR2, 0);                       \
    Temp1 = __vspltw(AR2, 1);                       \
    Temp2 = __vspltw(AR2, 2);                       \
    ResultCol2 = __vmulfp(Temp0, BR0);              \
    ResultCol2 = __vmaddfp(Temp1, BR1, ResultCol2); \
    ResultCol2 = __vmaddfp(Temp2, BR2, ResultCol2); \
    /* -- Col3 */                                   \
    Temp0 = __vspltw(AR3, 0);                       \
    Temp1 = __vspltw(AR3, 1);                       \
    Temp2 = __vspltw(AR3, 2);                       \
    ResultCol3 = __vmulfp(Temp0, BR0);              \
    ResultCol3 = __vmaddfp(Temp1, BR1, ResultCol3); \
    ResultCol3 = __vmaddfp(Temp2, BR2, ResultCol3); \
    /* note the extra add here... */                \
    ResultCol3 = __vaddfp(ResultCol3, BR3)

#define ALIGNED_STORE(Result, VecPrefix)        \
    __stvx(VecPrefix##0, Result, 0);            \
    __stvx(VecPrefix##1, Result, 16);           \
    __stvx(VecPrefix##2, Result, 32);           \
    __stvx(VecPrefix##3, Result, 48)

#define UNALIGNED_STORE(Result, VecPrefix)      \
    __stvlx(VecPrefix##0, Result, 0 + 0);       \
    __stvrx(VecPrefix##0, Result, 0 + 16);      \
    __stvlx(VecPrefix##1, Result, 16 + 0);      \
    __stvrx(VecPrefix##1, Result, 16 + 16);     \
    __stvlx(VecPrefix##2, Result, 32 + 0);      \
    __stvrx(VecPrefix##2, Result, 32 + 16);     \
    __stvlx(VecPrefix##3, Result, 48 + 0);      \
    __stvrx(VecPrefix##3, Result, 48 + 16)



// Partially aligned operands...
void GRANNY
XenonAligned101ColumnMatrix4x3Multiply(real32 * NOALIAS Result, // aligned
                                       real32 const* NOALIAS A, // unaligned
                                       real32 const* NOALIAS B) // aligned
{
    Assert(IS_ALIGNED_16(B));
    Assert(IS_ALIGNED_16(Result));

    DECLARE_VARS;

    // Load the a/b matrices
    {
        UNALIGNED_LOAD(AR, A);
        ALIGNED_LOAD(BR, B);
    }

    MATRIX_MUL;
    ALIGNED_STORE(Result, ResultCol);
}


// Partially aligned operands...
void GRANNY
XenonAligned110ColumnMatrix4x3Multiply(real32 * NOALIAS Result, // aligned
                                       real32 const* NOALIAS A, // aligned
                                       real32 const* NOALIAS B) // unaligned
{
    Assert(IS_ALIGNED_16(B));
    Assert(IS_ALIGNED_16(Result));

    DECLARE_VARS;

    // Load the a/b matrices
    {
        ALIGNED_LOAD(AR, A);
        UNALIGNED_LOAD(BR, B);
    }

    MATRIX_MUL;
    ALIGNED_STORE(Result, ResultCol);
}


void GRANNY
XenonAlignedColumnMatrix4x3Multiply(real32 * NOALIAS Result,
                                    real32 const* NOALIAS A,
                                    real32 const* NOALIAS B)
{
    Assert(IS_ALIGNED_16(A));
    Assert(IS_ALIGNED_16(B));
    Assert(IS_ALIGNED_16(Result));

    DECLARE_VARS;

    // Load the a/b matrices
    {
        ALIGNED_LOAD(AR, A);
        ALIGNED_LOAD(BR, B);
    }

    MATRIX_MUL;
    ALIGNED_STORE(Result, ResultCol);
}

void GRANNY
XenonUnalignedColumnMatrix4x3Multiply(real32 * NOALIAS Result,
                                      real32 const* NOALIAS A,
                                      real32 const* NOALIAS B)
{
    DECLARE_VARS;

    // Load the a/b matrices
    {
        UNALIGNED_LOAD(AR, A);
        UNALIGNED_LOAD(BR, B);
    }

    MATRIX_MUL;
    UNALIGNED_STORE(Result, ResultCol);
}


// Partially aligned operands, transposed...
void GRANNY
XenonAligned101ColumnMatrix4x3MultiplyTranspose(real32 * NOALIAS Result, // aligned
                                                real32 const* NOALIAS A, // unaligned
                                                real32 const* NOALIAS B) // aligned
{
    Assert(IS_ALIGNED_16(B));
    Assert(IS_ALIGNED_16(Result));
    Assert(!StructuresOverlap(*Result, *A));
    Assert(!StructuresOverlap(*Result, *B));

    XMVECTOR AR0, AR1, AR2, AR3;
    XMVECTOR BR0, BR1, BR2, BR3;
    XMVECTOR Temp0, Temp1, Temp2, Temp3;
    XMVECTOR ResultRow0, ResultRow1, ResultRow2;

    ALIGN16(matrix_4x4) TransposeA = {
        A[0],  A[4],  A[8],  A[12],
        A[1],  A[5],  A[9],  A[13],
        A[2],  A[6],  A[10], A[14],
        A[3],  A[7],  A[11], A[15],
    };

    XMDUMMY_INITIALIZE_VECTOR(Temp0);
    Temp0 = __vupkd3d(Temp0, VPACK_NORMSHORT2);
    XMVECTOR Constant0001 = __vpermwi(Temp0, 0xAB);

    // Load the a/b matrices
    {
        ALIGNED_LOAD(AR, TransposeA);
        ALIGNED_LOAD(BR, B);
    }

    Temp0 = __vspltw(BR0, 0);
    Temp1 = __vspltw(BR1, 0);
    Temp2 = __vspltw(BR2, 0);
    Temp3 = __vspltw(BR3, 0);
    ResultRow0 = __vmulfp(Temp0,  AR0);
    ResultRow0 = __vmaddfp(Temp1, AR1, ResultRow0);
    ResultRow0 = __vmaddfp(Temp2, AR2, ResultRow0);
    ResultRow0 = __vmaddfp(Constant0001, Temp3, ResultRow0);

    Temp0 = __vspltw(BR0, 1);
    Temp1 = __vspltw(BR1, 1);
    Temp2 = __vspltw(BR2, 1);
    Temp3 = __vspltw(BR3, 1);
    ResultRow1 = __vmulfp(Temp0,  AR0);
    ResultRow1 = __vmaddfp(Temp1, AR1, ResultRow1);
    ResultRow1 = __vmaddfp(Temp2, AR2, ResultRow1);
    ResultRow1 = __vmaddfp(Constant0001, Temp3, ResultRow1);

    Temp0 = __vspltw(BR0, 2);
    Temp1 = __vspltw(BR1, 2);
    Temp2 = __vspltw(BR2, 2);
    Temp3 = __vspltw(BR3, 2);
    ResultRow2 = __vmulfp(Temp0,  AR0);
    ResultRow2 = __vmaddfp(Temp1, AR1, ResultRow2);
    ResultRow2 = __vmaddfp(Temp2, AR2, ResultRow2);
    ResultRow2 = __vmaddfp(Constant0001, Temp3, ResultRow2);

    __stvx(ResultRow0, Result, 0);
    __stvx(ResultRow1, Result, 16);
    __stvx(ResultRow2, Result, 32);
}

void GRANNY
XenonUnalignedColumnMatrix4x3MultiplyTranspose(real32 * NOALIAS Result,
                                               real32 const* NOALIAS A,
                                               real32 const* NOALIAS B)
{
    Assert(!StructuresOverlap(*Result, *A));
    Assert(!StructuresOverlap(*Result, *B));

    XMVECTOR AR0, AR1, AR2, AR3;
    XMVECTOR BR0, BR1, BR2, BR3;
    XMVECTOR Temp0, Temp1, Temp2, Temp3;
    XMVECTOR ResultRow0, ResultRow1, ResultRow2;

    ALIGN16(matrix_4x4) TransposeA = {
        A[0],  A[4],  A[8],  A[12],
        A[1],  A[5],  A[9],  A[13],
        A[2],  A[6],  A[10], A[14],
        A[3],  A[7],  A[11], A[15],
    };

    XMDUMMY_INITIALIZE_VECTOR(Temp0);
    Temp0 = __vupkd3d(Temp0, VPACK_NORMSHORT2);
    XMVECTOR Constant0001 = __vpermwi(Temp0, 0xAB);

    // Load the a/b matrices
    {
        ALIGNED_LOAD(AR, TransposeA);
        UNALIGNED_LOAD(BR, B);
    }

    Temp0 = __vspltw(BR0, 0);
    Temp1 = __vspltw(BR1, 0);
    Temp2 = __vspltw(BR2, 0);
    Temp3 = __vspltw(BR3, 0);
    ResultRow0 = __vmulfp(Temp0,  AR0);
    ResultRow0 = __vmaddfp(Temp1, AR1, ResultRow0);
    ResultRow0 = __vmaddfp(Temp2, AR2, ResultRow0);
    ResultRow0 = __vmaddfp(Constant0001, Temp3, ResultRow0);

    Temp0 = __vspltw(BR0, 1);
    Temp1 = __vspltw(BR1, 1);
    Temp2 = __vspltw(BR2, 1);
    Temp3 = __vspltw(BR3, 1);
    ResultRow1 = __vmulfp(Temp0,  AR0);
    ResultRow1 = __vmaddfp(Temp1, AR1, ResultRow1);
    ResultRow1 = __vmaddfp(Temp2, AR2, ResultRow1);
    ResultRow1 = __vmaddfp(Constant0001, Temp3, ResultRow1);

    Temp0 = __vspltw(BR0, 2);
    Temp1 = __vspltw(BR1, 2);
    Temp2 = __vspltw(BR2, 2);
    Temp3 = __vspltw(BR3, 2);
    ResultRow2 = __vmulfp(Temp0,  AR0);
    ResultRow2 = __vmaddfp(Temp1, AR1, ResultRow2);
    ResultRow2 = __vmaddfp(Temp2, AR2, ResultRow2);
    ResultRow2 = __vmaddfp(Constant0001, Temp3, ResultRow2);

    __stvlx(ResultRow0, Result, 0  + 0);
    __stvrx(ResultRow0, Result, 0  + 16);
    __stvlx(ResultRow1, Result, 16 + 0);
    __stvrx(ResultRow1, Result, 16 + 16);
    __stvlx(ResultRow2, Result, 32 + 0);
    __stvrx(ResultRow2, Result, 32 + 16);
}


static void
SetPointers(void)
{
#if USE_ACCELERATED_MATRIX_OPS
    ColumnMatrixMultiply4x3Impl          = XenonUnalignedColumnMatrix4x3Multiply;
    ColumnMatrixMultiply4x3TransposeImpl = XenonUnalignedColumnMatrix4x3MultiplyTranspose;
    ColumnMatrixMultiply4x4Impl          = ColumnMatrixMultiply4x4Impl_Generic;
#else
    ColumnMatrixMultiply4x3Impl          = ColumnMatrixMultiply4x3Impl_Generic;
    ColumnMatrixMultiply4x3TransposeImpl = ColumnMatrixMultiply4x3TransposeImpl_Generic;
    ColumnMatrixMultiply4x4Impl          = ColumnMatrixMultiply4x4Impl_Generic;
#endif

    TestFastMatrixOps();
}

static void
ColumnMatrixMultiply4x3Impl_Stub(real32 *IntoMatrix4x4,
                                 real32 const *Matrix4x4,
                                 real32 const *ByMatrix4x4)
{
    SetPointers();
    ColumnMatrixMultiply4x3Impl(IntoMatrix4x4, Matrix4x4, ByMatrix4x4);
}

static void
ColumnMatrixMultiply4x3TransposeImpl_Stub(real32 *IntoMatrix3x4,
                                          real32 const *Matrix4x4,
                                          real32 const *ByMatrix4x4)
{
    SetPointers();
    ColumnMatrixMultiply4x3TransposeImpl(IntoMatrix3x4, Matrix4x4, ByMatrix4x4);
}

static void
ColumnMatrixMultiply4x4Impl_Stub(real32 *IntoMatrix4x4,
                                 real32 const *Matrix4x4,
                                 real32 const *ByMatrix4x4)
{
    SetPointers();
    ColumnMatrixMultiply4x4Impl(IntoMatrix4x4, Matrix4x4, ByMatrix4x4);
}



BEGIN_GRANNY_NAMESPACE;

#define STUB_POINTER(Name) OPTIMIZED_DISPATCH(Name) = (OPTIMIZED_DISPATCH_TYPE(Name) *)Name##_Stub

STUB_POINTER(ColumnMatrixMultiply4x3Impl);
STUB_POINTER(ColumnMatrixMultiply4x3TransposeImpl);
STUB_POINTER(ColumnMatrixMultiply4x4Impl);

END_GRANNY_NAMESPACE;
