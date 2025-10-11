// ========================================================================
// $File: //jeffr/granny_29/rt/wii/wii_granny_matrix_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_matrix_operations.h"

#include "granny_assert.h"
#include "granny_parameter_checking.h"
#include <revolution/mtx/mtx44ext.h>


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

#define USE_ACCELERATED_MATRIX_OPS 1

// Reordered and spread out the write registers to make the scheduler's work a bit
// easier...
static void
ColumnMatrixMultiply4x3Impl_Wii(real32 register*       ab,
                                const real32 register* a,
                                const real32 register* b)
{
    asm
    {
        psq_l       fp0,    0(a),   0,      0;      //   a00, a01
        psq_l       fp1,    8(a),   0,      0;      //   a02, a03

        psq_l       fp2,    0(b),   0,      0;      //   b00, b01
        psq_l       fp3,    8(b),   0,      0;      //   b02, b03
        psq_l       fp4,    16(b),  0,      0;      //   b10, b11
        psq_l       fp5,    24(b),  0,      0;      //   b12, b13
        psq_l       fp6,    32(b),  0,      0;      //   b20, b21
        psq_l       fp7,    40(b),  0,      0;      //   b22, b23

#define DSTCOL_01 fp8
#define DSTCOL_23 fp9

        ps_muls0    DSTCOL_01,   fp2,    fp0;                  //   a00*b00, a00*b01
        ps_madds1   DSTCOL_01,   fp4,    fp0,    DSTCOL_01;   // + a01*b10, a01*b11
        ps_madds0   DSTCOL_01,   fp6,    fp1,    DSTCOL_01;   // + a02*b20, a02*b21
        ps_muls0    DSTCOL_23,   fp3,    fp0;                  //   a00*b02, a00*b03
        ps_madds1   DSTCOL_23,   fp5,    fp0,    DSTCOL_23;   // + a01*b12, a01*b13
        ps_madds0   DSTCOL_23,   fp7,    fp1,    DSTCOL_23;   // + a02*b22, a02*b23

        psq_l       fp0,         16(a),  0,      0;      //   a10, a11
        psq_st      DSTCOL_01,   0(ab),  0,      0;      //   ab00, ab01
        psq_l       fp1,         24(a),  0,      0;      //   a12, a13
        psq_st      DSTCOL_23,   8(ab),  0,      0;      //   ab02, ab03

#undef DSTCOL_01
#undef DSTCOL_23
#define DSTCOL_01 fp10
#define DSTCOL_23 fp11

        ps_muls0    DSTCOL_01,   fp2,    fp0;                  //   a10*b00, a10*b01
        ps_madds1   DSTCOL_01,   fp4,    fp0,    DSTCOL_01;   // + a11*b10, a11*b11
        ps_madds0   DSTCOL_01,   fp6,    fp1,    DSTCOL_01;   // + a12*b20, a12*b21
        ps_muls0    DSTCOL_23,   fp3,    fp0;                  //   a10*b02, a10*b03
        ps_madds1   DSTCOL_23,   fp5,    fp0,    DSTCOL_23;   // + a11*b12, a11*b13
        ps_madds0   DSTCOL_23,   fp7,    fp1,    DSTCOL_23;   // + a12*b22, a12*b23

        psq_l       fp0,         32(a),  0,      0;      //   a20, a21
        psq_st      DSTCOL_01,   16(ab), 0,      0;      //   ab10, ab11
        psq_l       fp1,         40(a),  0,      0;      //   a22, a23
        psq_st      DSTCOL_23,   24(ab), 0,      0;      //   ab12, ab13

#undef DSTCOL_01
#undef DSTCOL_23
#define DSTCOL_01 fp12
#define DSTCOL_23 fp13

        ps_muls0    DSTCOL_01,   fp2,    fp0;                  //   a20*b00, a20*b01
        ps_madds1   DSTCOL_01,   fp4,    fp0,    DSTCOL_01;   // + a21*b10, a21*b11
        ps_madds0   DSTCOL_01,   fp6,    fp1,    DSTCOL_01;   // + a22*b20, a22*b21
        ps_muls0    DSTCOL_23,   fp3,    fp0;                  //   a20*b02, a20*b03
        ps_madds1   DSTCOL_23,   fp5,    fp0,    DSTCOL_23;   // + a21*b12, a21*b13
        ps_madds0   DSTCOL_23,   fp7,    fp1,    DSTCOL_23;   // + a22*b22, a22*b23

        // Note singular load here.
        psq_l       fp8,          48(b),  0,      0;      //   b30, b31
        psq_l       fp9,          56(b),  0,      0;      //   b32, b33

        psq_l       fp0,         48(a),  0,      0;      //   a30, a31
        psq_st      DSTCOL_01,   32(ab), 0,      0;      //   ab20, ab21
        psq_l       fp1,         56(a),  0,      0;      //   a32, a33
        psq_st      DSTCOL_23,   40(ab), 0,      0;      //   ab22, ab23

#undef DSTCOL_01
#undef DSTCOL_23
#define DSTCOL_01 fp10
#define DSTCOL_23 fp11

        ps_madds0   DSTCOL_01,   fp2,    fp0,    fp8;          //   a30*b00 + b30, a30*b01 + b31
        ps_madds1   DSTCOL_01,   fp4,    fp0,    DSTCOL_01;   // + a31*b10,       a31*b11
        ps_madds0   DSTCOL_01,   fp6,    fp1,    DSTCOL_01;   // + a32*b20,       a32*b21
        ps_madds0   DSTCOL_23,   fp3,    fp0,    fp9;          //   a30*b02 + b32, a30*b03 + b33
        ps_madds1   DSTCOL_23,   fp5,    fp0,    DSTCOL_23;   // + a31*b12,       a31*b13
        ps_madds0   DSTCOL_23,   fp7,    fp1,    DSTCOL_23;   // + a32*b22,       a32*b23

        psq_st      DSTCOL_01,   48(ab), 0,      0;      //   ab30, ab31
        psq_st      DSTCOL_23,   56(ab), 0,      0;      //   ab32, ab33
    }
#undef DSTCOL_01
#undef DSTCOL_23
}

static void
ColumnMatrixMultiply4x4Impl_Wii(real32*       IntoMatrix4x4,
                                real32 const* Matrix4x4,
                                real32 const* ByMatrix4x4)
{
    PSMTX44Concat((Mtx44Ptr)Matrix4x4, (Mtx44Ptr)ByMatrix4x4, (Mtx44Ptr)IntoMatrix4x4);
}


static void
SetPointers(void)
{
#if USE_ACCELERATED_MATRIX_OPS
    ColumnMatrixMultiply4x3Impl          = ColumnMatrixMultiply4x3Impl_Wii;
    ColumnMatrixMultiply4x3TransposeImpl = ColumnMatrixMultiply4x3TransposeImpl_Generic;
    ColumnMatrixMultiply4x4Impl          = ColumnMatrixMultiply4x4Impl_Wii;
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
