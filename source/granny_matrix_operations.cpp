// ========================================================================
// $File: //jeffr/granny_29/rt/granny_matrix_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_matrix_operations.h"

#include "granny_assert.h"
#include "granny_floats.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode MathLogMessage
USING_GRANNY_NAMESPACE;

void GRANNY
ColumnMatrixMultiply4x3(real32*       IntoMatrix4x4,
                        real32 const* Matrix4x4,
                        real32 const* ByMatrix4x4)
{
    ColumnMatrixMultiply4x3Impl(IntoMatrix4x4, Matrix4x4, ByMatrix4x4);
}

void GRANNY
ColumnMatrixMultiply4x3Transpose(real32*       IntoMatrix3x4,
                                 real32 const* Matrix4x4,
                                 real32 const* ByMatrix4x4)
{
    ColumnMatrixMultiply4x3TransposeImpl(IntoMatrix3x4, Matrix4x4, ByMatrix4x4);
}

void GRANNY
ColumnMatrixMultiply4x4(real32*       IntoMatrix4x4,
                        real32 const* Matrix4x4,
                        real32 const* ByMatrix4x4)
{
    ColumnMatrixMultiply4x4Impl(IntoMatrix4x4, Matrix4x4, ByMatrix4x4);
}



GENERIC_DISPATCH(ColumnMatrixMultiply4x3Impl)(real32 *IntoMatrix4x4,
                                              real32 const *Matrix4x4,
                                              real32 const *ByMatrix4x4)
{
    // 6*4 + 3 = 27 adds
    // 9*4 = 36 multiplies
    // 4*4 = 16 stores
    Assert(!StructuresOverlap(*IntoMatrix4x4, *Matrix4x4));
    Assert(!StructuresOverlap(*IntoMatrix4x4, *ByMatrix4x4));

    quad* NOALIAS Result = (quad *)IntoMatrix4x4;
    quad const *A = (quad const *)Matrix4x4;
    quad const *B = (quad const *)ByMatrix4x4;

    // Matrix-multiply the linear component (upper 3x3)
    Result[0][0] = A[0][0]*B[0][0] + A[0][1]*B[1][0] + A[0][2]*B[2][0];
    Result[0][1] = A[0][0]*B[0][1] + A[0][1]*B[1][1] + A[0][2]*B[2][1];
    Result[0][2] = A[0][0]*B[0][2] + A[0][1]*B[1][2] + A[0][2]*B[2][2];

    Result[1][0] = A[1][0]*B[0][0] + A[1][1]*B[1][0] + A[1][2]*B[2][0];
    Result[1][1] = A[1][0]*B[0][1] + A[1][1]*B[1][1] + A[1][2]*B[2][1];
    Result[1][2] = A[1][0]*B[0][2] + A[1][1]*B[1][2] + A[1][2]*B[2][2];

    Result[2][0] = A[2][0]*B[0][0] + A[2][1]*B[1][0] + A[2][2]*B[2][0];
    Result[2][1] = A[2][0]*B[0][1] + A[2][1]*B[1][1] + A[2][2]*B[2][1];
    Result[2][2] = A[2][0]*B[0][2] + A[2][1]*B[1][2] + A[2][2]*B[2][2];

    // Special-case multiply the affine component (position)
    Result[3][0] = (A[3][0]*B[0][0] + A[3][1]*B[1][0] + A[3][2]*B[2][0] +
                    B[3][0]);
    Result[3][1] = (A[3][0]*B[0][1] + A[3][1]*B[1][1] + A[3][2]*B[2][1] +
                    B[3][1]);
    Result[3][2] = (A[3][0]*B[0][2] + A[3][1]*B[1][2] + A[3][2]*B[2][2] +
                    B[3][2]);

    // Ignore the full 4x4-ness of the matrix, since we only care about
    // the 4x3 subsection.
    Result[0][3] = Result[1][3] = Result[2][3] = 0.0f;
    Result[3][3] = 1.0f;
}

GENERIC_DISPATCH(ColumnMatrixMultiply4x3TransposeImpl)(real32 *IntoMatrix3x4,
                                                       real32 const *Matrix4x4,
                                                       real32 const *ByMatrix4x4)
{
    // 6*4 + 3 = 27 adds
    // 9*4 = 36 multiplies
    // 4*3 = 12 stores
    Assert(!StructuresOverlap(*IntoMatrix3x4, *Matrix4x4));
    Assert(!StructuresOverlap(*IntoMatrix3x4, *ByMatrix4x4));

    quad* NOALIAS Result = (quad *)IntoMatrix3x4;
    quad const *A = (quad const *)Matrix4x4;
    quad const *B = (quad const *)ByMatrix4x4;

    // Matrix-multiply the linear component (upper 3x3)
    Result[0][0] = A[0][0]*B[0][0] + A[0][1]*B[1][0] + A[0][2]*B[2][0];
    Result[0][1] = A[1][0]*B[0][0] + A[1][1]*B[1][0] + A[1][2]*B[2][0];
    Result[0][2] = A[2][0]*B[0][0] + A[2][1]*B[1][0] + A[2][2]*B[2][0];
    // Special-case multiply the affine component (position)
    Result[0][3] = (A[3][0]*B[0][0] + A[3][1]*B[1][0] + A[3][2]*B[2][0] +
                    B[3][0]);

    Result[1][0] = A[0][0]*B[0][1] + A[0][1]*B[1][1] + A[0][2]*B[2][1];
    Result[1][1] = A[1][0]*B[0][1] + A[1][1]*B[1][1] + A[1][2]*B[2][1];
    Result[1][2] = A[2][0]*B[0][1] + A[2][1]*B[1][1] + A[2][2]*B[2][1];
    Result[1][3] = (A[3][0]*B[0][1] + A[3][1]*B[1][1] + A[3][2]*B[2][1] +
                    B[3][1]);

    Result[2][0] = A[0][0]*B[0][2] + A[0][1]*B[1][2] + A[0][2]*B[2][2];
    Result[2][1] = A[1][0]*B[0][2] + A[1][1]*B[1][2] + A[1][2]*B[2][2];
    Result[2][2] = A[2][0]*B[0][2] + A[2][1]*B[1][2] + A[2][2]*B[2][2];
    Result[2][3] = (A[3][0]*B[0][2] + A[3][1]*B[1][2] + A[3][2]*B[2][2] +
                    B[3][2]);
}

GENERIC_DISPATCH(ColumnMatrixMultiply4x4Impl)(real32 *IntoMatrix4x4,
                                              real32 const *Matrix4x4,
                                              real32 const *ByMatrix4x4)
{
    Assert(!StructuresOverlap(*IntoMatrix4x4, *Matrix4x4));
    Assert(!StructuresOverlap(*IntoMatrix4x4, *ByMatrix4x4));

    quad* NOALIAS Result = (quad *)IntoMatrix4x4;
    quad const *A = (quad const *)Matrix4x4;
    quad const *B = (quad const *)ByMatrix4x4;

    Result[0][0] = A[0][0]*B[0][0] + A[0][1]*B[1][0] + A[0][2]*B[2][0] + A[0][3]*B[3][0];
    Result[0][1] = A[0][0]*B[0][1] + A[0][1]*B[1][1] + A[0][2]*B[2][1] + A[0][3]*B[3][1];
    Result[0][2] = A[0][0]*B[0][2] + A[0][1]*B[1][2] + A[0][2]*B[2][2] + A[0][3]*B[3][2];
    Result[0][3] = A[0][0]*B[0][3] + A[0][1]*B[1][3] + A[0][2]*B[2][3] + A[0][3]*B[3][3];

    Result[1][0] = A[1][0]*B[0][0] + A[1][1]*B[1][0] + A[1][2]*B[2][0] + A[1][3]*B[3][0];
    Result[1][1] = A[1][0]*B[0][1] + A[1][1]*B[1][1] + A[1][2]*B[2][1] + A[1][3]*B[3][1];
    Result[1][2] = A[1][0]*B[0][2] + A[1][1]*B[1][2] + A[1][2]*B[2][2] + A[1][3]*B[3][2];
    Result[1][3] = A[1][0]*B[0][3] + A[1][1]*B[1][3] + A[1][2]*B[2][3] + A[1][3]*B[3][3];

    Result[2][0] = A[2][0]*B[0][0] + A[2][1]*B[1][0] + A[2][2]*B[2][0] + A[2][3]*B[3][0];
    Result[2][1] = A[2][0]*B[0][1] + A[2][1]*B[1][1] + A[2][2]*B[2][1] + A[2][3]*B[3][1];
    Result[2][2] = A[2][0]*B[0][2] + A[2][1]*B[1][2] + A[2][2]*B[2][2] + A[2][3]*B[3][2];
    Result[2][3] = A[2][0]*B[0][3] + A[2][1]*B[1][3] + A[2][2]*B[2][3] + A[2][3]*B[3][3];

    Result[3][0] = A[3][0]*B[0][0] + A[3][1]*B[1][0] + A[3][2]*B[2][0] + A[3][3]*B[3][0];
    Result[3][1] = A[3][0]*B[0][1] + A[3][1]*B[1][1] + A[3][2]*B[2][1] + A[3][3]*B[3][1];
    Result[3][2] = A[3][0]*B[0][2] + A[3][1]*B[1][2] + A[3][2]*B[2][2] + A[3][3]*B[3][2];
    Result[3][3] = A[3][0]*B[0][3] + A[3][1]*B[1][3] + A[3][2]*B[2][3] + A[3][3]*B[3][3];
}


// Run a quick unit test against the _Generic implementations with the currently installed
// matrix ops.  This is left on for release, though the asserts won't fire.  6 matrix muls
// shouldn't even blip.
void GRANNY
TestFastMatrixOps()
{
    // Note that all of the publically exposed matrix ops must run correctly on 4-byte
    // aligned input, so make sure that the sources are not 16-byte aligned by accident.
    ALIGN16_STACK(real32, GenericResult4x3, 17);
    ALIGN16_STACK(real32, GenericResult3x4, 13);
    ALIGN16_STACK(real32, GenericResult4x4, 17);
    ALIGN16_STACK(real32, FastResult4x3, 17);
    ALIGN16_STACK(real32, FastResult3x4, 13);
    ALIGN16_STACK(real32, FastResult4x4, 17);

    SetUInt8(SizeOf(GenericResult4x3), 0xff, GenericResult4x3);
    SetUInt8(SizeOf(GenericResult3x4), 0xff, GenericResult3x4);
    SetUInt8(SizeOf(GenericResult4x4), 0xff, GenericResult4x4);
    SetUInt8(SizeOf(FastResult4x3), 0xff, FastResult4x3);
    SetUInt8(SizeOf(FastResult3x4), 0xff, FastResult3x4);
    SetUInt8(SizeOf(FastResult4x4), 0xff, FastResult4x4);

    ALIGN16_STACK(real32, Source_4x3_A, 17);
    ALIGN16_STACK(real32, Source_4x3_B, 17);
    {
        Source_4x3_A[0] = GetReal32QuietNaN();
        Source_4x3_B[0] = GetReal32QuietNaN();
        {for(int32x i = 0; i < 16; ++i)
        {
            Source_4x3_A[1 + i] = real32(i);
            Source_4x3_B[1 + i] = real32(i + 16);
        }}

        // Enforce constraints on 4x3 sources (note that this would normally be +3, but
        // we're going to shift over when addressing the matrices...)
        Source_4x3_A[ 0 + 4] = 0;
        Source_4x3_A[ 4 + 4] = 0;
        Source_4x3_A[ 8 + 4] = 0;
        Source_4x3_A[12 + 4] = 1;
        Source_4x3_B[ 0 + 4] = 0;
        Source_4x3_B[ 4 + 4] = 0;
        Source_4x3_B[ 8 + 4] = 0;
        Source_4x3_B[12 + 4] = 1;
    }

    // Check with arguments unaligned
#define UNALIGN(Var) &Var[1]

    // Do the generic muls
    ColumnMatrixMultiply4x3Impl_Generic(UNALIGN(GenericResult4x3),
                                        UNALIGN(Source_4x3_A),
                                        UNALIGN(Source_4x3_B));
    ColumnMatrixMultiply4x3TransposeImpl_Generic(UNALIGN(GenericResult3x4),
                                                 UNALIGN(Source_4x3_A),
                                                 UNALIGN(Source_4x3_B));
    ColumnMatrixMultiply4x4Impl_Generic(UNALIGN(GenericResult4x4),
                                        UNALIGN(Source_4x3_A),
                                        UNALIGN(Source_4x3_B));

    // And the currently install fast muls
    ColumnMatrixMultiply4x3Impl(UNALIGN(FastResult4x3),
                                UNALIGN(Source_4x3_A),
                                UNALIGN(Source_4x3_B));
    ColumnMatrixMultiply4x3TransposeImpl(UNALIGN(FastResult3x4),
                                         UNALIGN(Source_4x3_A),
                                         UNALIGN(Source_4x3_B));
    ColumnMatrixMultiply4x4Impl(UNALIGN(FastResult4x4),
                                UNALIGN(Source_4x3_A),
                                UNALIGN(Source_4x3_B));

    // And assert that they match...
    Assert(Compare(SizeOf(GenericResult4x3), GenericResult4x3, FastResult4x3));
    Assert(Compare(SizeOf(GenericResult3x4), GenericResult3x4, FastResult3x4));
    Assert(Compare(SizeOf(GenericResult4x4), GenericResult4x4, FastResult4x4));
#undef UNALIGN

    // ...and with the arguments aligned
    Source_4x3_A[0] = 0.0f;
    Source_4x3_B[0] = 0.0f;

    SetUInt8(SizeOf(GenericResult4x3), 0xff, GenericResult4x3);
    SetUInt8(SizeOf(GenericResult3x4), 0xff, GenericResult3x4);
    SetUInt8(SizeOf(GenericResult4x4), 0xff, GenericResult4x4);
    SetUInt8(SizeOf(FastResult4x3), 0xff, FastResult4x3);
    SetUInt8(SizeOf(FastResult3x4), 0xff, FastResult3x4);
    SetUInt8(SizeOf(FastResult4x4), 0xff, FastResult4x4);

    // Do the generic muls
    ColumnMatrixMultiply4x3Impl_Generic         (GenericResult4x3, Source_4x3_A, Source_4x3_B);
    ColumnMatrixMultiply4x3TransposeImpl_Generic(GenericResult3x4, Source_4x3_A, Source_4x3_B);
    ColumnMatrixMultiply4x4Impl_Generic         (GenericResult4x4, Source_4x3_A, Source_4x3_B);

    // And the currently install fast muls
    ColumnMatrixMultiply4x3Impl_Generic         (FastResult4x3, Source_4x3_A, Source_4x3_B);
    ColumnMatrixMultiply4x3TransposeImpl_Generic(FastResult3x4, Source_4x3_A, Source_4x3_B);
    ColumnMatrixMultiply4x4Impl_Generic         (FastResult4x4, Source_4x3_A, Source_4x3_B);

    // And assert that they match...
    Assert(Compare(SizeOf(GenericResult4x3), GenericResult4x3, FastResult4x3));
    Assert(Compare(SizeOf(GenericResult3x4), GenericResult3x4, FastResult3x4));
    Assert(Compare(SizeOf(GenericResult4x4), GenericResult4x4, FastResult4x4));

}
