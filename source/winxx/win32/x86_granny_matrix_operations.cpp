// ========================================================================
// $File: //jeffr/granny_29/rt/x86/x86_granny_matrix_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_matrix_operations.h"
#include "x86_granny_matrix_operations_ops.inl"

#include "granny_assert.h"
#include "granny_memory.h"
#include "granny_parameter_checking.h"
#include "x86_granny_cpu_queries.h"

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

// Set to 0 to fall back to the older C pathways...
#define USE_ACCELERATED_MATRIX_OPS 1


static void
ColumnMatrixMultiply4x3Unaligned_SSE(real32*       ABMatrix,
                                     real32 const* AMatrix,
                                     real32 const* BMatrix)
{
    __asm {
        mov     A,  [ AMatrix ];
        mov     B,  [ BMatrix ];
        mov     AB, [ ABMatrix ];
    }
    UnalignedSetup();

    RowMultiply4x3();
    StoreRowUnaligned();
    AdvancePointers4x4();

    RowMultiply4x3();
    StoreRowUnaligned();
    AdvancePointers4x4();

    RowMultiply4x3();
    StoreRowUnaligned();
    AdvancePointers4x4();

    RowMultiply4x3_With1();
    StoreRowUnaligned();
}

static void
ColumnMatrixMultiply4x3Aligned_SSE(real32*       ABMatrix,
                                   real32 const* AMatrix,
                                   real32 const* BMatrix)
{
    Assert(IS_ALIGNED_16(ABMatrix));
    Assert(IS_ALIGNED_16(BMatrix));

    __asm {
        mov     A,  [ AMatrix ];
        mov     B,  [ BMatrix ];
        mov     AB, [ ABMatrix ];
    }
    AlignedSetup();

    RowMultiply4x3();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x3();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x3();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x3_With1();
    StoreRowAligned();
}

static void
ColumnMatrixMultiply4x3TransposeUnaligned_SSE(real32*       ABMatrix,
                                              real32 const* AMatrix,
                                              real32 const* BMatrix)
{
    ALIGN16(real32) Temp[16];
    real32* TempPtr = &Temp[0];

    __asm {
        mov     A,  [ AMatrix ];
        mov     B,  [ BMatrix ];
        mov     AB, [ TempPtr ];
    }
    UnalignedSetup();

    RowMultiply4x4();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowAligned();

    // Transpose: todo sse?
    ABMatrix[ 0] = Temp[0 +  0];
    ABMatrix[ 1] = Temp[0 +  4];
    ABMatrix[ 2] = Temp[0 +  8];
    ABMatrix[ 3] = Temp[0 + 12];
    ABMatrix[ 4] = Temp[1 +  0];
    ABMatrix[ 5] = Temp[1 +  4];
    ABMatrix[ 6] = Temp[1 +  8];
    ABMatrix[ 7] = Temp[1 + 12];
    ABMatrix[ 8] = Temp[2 +  0];
    ABMatrix[ 9] = Temp[2 +  4];
    ABMatrix[10] = Temp[2 +  8];
    ABMatrix[11] = Temp[2 + 12];
}

static void
ColumnMatrixMultiply4x3TransposeAligned_SSE(real32*       ABMatrix,
                                            real32 const* AMatrix,
                                            real32 const* BMatrix)
{
    ALIGN16(real32) Temp[16];
    real32* TempPtr = &Temp[0];

    __asm {
        mov     A,  [ AMatrix ];
        mov     B,  [ BMatrix ];
        mov     AB, [ TempPtr ];
    }
    AlignedSetup();

    RowMultiply4x4();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowAligned();

    // Transpose: todo sse?
    ABMatrix[ 0] = Temp[0 +  0];
    ABMatrix[ 1] = Temp[0 +  4];
    ABMatrix[ 2] = Temp[0 +  8];
    ABMatrix[ 3] = Temp[0 + 12];
    ABMatrix[ 4] = Temp[1 +  0];
    ABMatrix[ 5] = Temp[1 +  4];
    ABMatrix[ 6] = Temp[1 +  8];
    ABMatrix[ 7] = Temp[1 + 12];
    ABMatrix[ 8] = Temp[2 +  0];
    ABMatrix[ 9] = Temp[2 +  4];
    ABMatrix[10] = Temp[2 +  8];
    ABMatrix[11] = Temp[2 + 12];
}

static void
ColumnMatrixMultiply4x4Unaligned_SSE(real32*       ABMatrix,
                                     real32 const* AMatrix,
                                     real32 const* BMatrix)
{
    __asm {
        mov     A,  [ AMatrix ];
        mov     B,  [ BMatrix ];
        mov     AB, [ ABMatrix ];
    }
    UnalignedSetup();

    RowMultiply4x4();
    StoreRowUnaligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowUnaligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowUnaligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowUnaligned();
}

static void
ColumnMatrixMultiply4x4Aligned_SSE(real32*       ABMatrix,
                                   real32 const* AMatrix,
                                   real32 const* BMatrix)
{
    Assert(IS_ALIGNED_16(ABMatrix));
    Assert(IS_ALIGNED_16(BMatrix));

    __asm {
        mov     A,  [ AMatrix ];
        mov     B,  [ BMatrix ];
        mov     AB, [ ABMatrix ];
    }
    AlignedSetup();

    RowMultiply4x4();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowAligned();
    AdvancePointers4x4();

    RowMultiply4x4();
    StoreRowAligned();
}


// -----------------------------------------------------------------------------
// Alignment check and forwarding
// -----------------------------------------------------------------------------
void ColumnMatrixMultiply4x3Impl_SSE(real32*       ABMatrix,
                                     real32 const* AMatrix,
                                     real32 const* BMatrix)
{
    // AMatrix is accessed as scalars, so it's not required to be aligned
    if (IS_ALIGNED_16(ABMatrix) && IS_ALIGNED_16(BMatrix))
        ColumnMatrixMultiply4x3Aligned_SSE(ABMatrix, AMatrix, BMatrix);
    else
        ColumnMatrixMultiply4x3Unaligned_SSE(ABMatrix, AMatrix, BMatrix);
}

void ColumnMatrixMultiply4x3TransposeImpl_SSE(real32*       ABMatrix,
                                              real32 const* AMatrix,
                                              real32 const* BMatrix)
{
    // Because of the transpose, we don't actually care about the alignment of the
    // ABMatrix...
    //
    // AMatrix is accessed as scalars, so it's not required to be aligned
    if (IS_ALIGNED_16(BMatrix))
        ColumnMatrixMultiply4x3TransposeAligned_SSE(ABMatrix, AMatrix, BMatrix);
    else
        ColumnMatrixMultiply4x3TransposeUnaligned_SSE(ABMatrix, AMatrix, BMatrix);
}

void ColumnMatrixMultiply4x4Impl_SSE(real32*       ABMatrix,
                                     real32 const* AMatrix,
                                     real32 const* BMatrix)
{
    // AMatrix is accessed as scalars, so it's not required to be aligned
    if (IS_ALIGNED_16(ABMatrix) && IS_ALIGNED_16(BMatrix))
        ColumnMatrixMultiply4x4Aligned_SSE(ABMatrix, AMatrix, BMatrix);
    else
        ColumnMatrixMultiply4x4Unaligned_SSE(ABMatrix, AMatrix, BMatrix);
}


static void
SetPointers(void)
{
    ColumnMatrixMultiply4x3Impl          = ColumnMatrixMultiply4x3Impl_Generic;
    ColumnMatrixMultiply4x3TransposeImpl = ColumnMatrixMultiply4x3TransposeImpl_Generic;
    ColumnMatrixMultiply4x4Impl          = ColumnMatrixMultiply4x4Impl_Generic;

#if USE_ACCELERATED_MATRIX_OPS
    if (SSEIsAvailable())
    {
        ColumnMatrixMultiply4x3Impl          = ColumnMatrixMultiply4x3Impl_SSE;
        ColumnMatrixMultiply4x3TransposeImpl = ColumnMatrixMultiply4x3TransposeImpl_SSE;
        ColumnMatrixMultiply4x4Impl          = ColumnMatrixMultiply4x4Impl_SSE;
    }
#endif

    // Call the unit test (very fast, do at all times)
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
