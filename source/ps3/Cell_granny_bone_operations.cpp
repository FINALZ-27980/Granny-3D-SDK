// ========================================================================
// $File: //jeffr/granny_29/rt/cell/cell_granny_bone_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_bone_operations.h"

#include "granny_transform.h"
#include "granny_math.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_skeleton.h"

#include <altivec.h>

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define USE_ACCELERATED_BONE_OPS

USING_GRANNY_NAMESPACE;

#define CELL_VERSION(Name) static void Name##_Cell
#define DECLARE_UNALIASED(Name, type) type* NOALIAS Name = Name ## Aliased


// todo: find the correct sequence of precaches.

// =====================================================================
// Factored out code chunks to be combined for different alignments

#define DECLARE_VARS                                            \
    vector float AR0, AR1, AR2, AR3;                            \
    vector float BR0, BR1, BR2, BR3;                            \
    vector float ResultCol0, ResultCol1, ResultCol2, ResultCol3

#define UNALIGNED_LOAD(VecPrefix, Matrix)                               \
    VecPrefix##0  = (vector float)vec_perm(vec_lvx(0,  Matrix),         \
                                           vec_lvx(15, Matrix),         \
                                           vec_lvsl(0, Matrix));        \
    VecPrefix##1  = (vector float)vec_perm(vec_lvx(16,  Matrix),        \
                                           vec_lvx(16 + 15, Matrix),    \
                                           vec_lvsl(16, Matrix));       \
    VecPrefix##2  = (vector float)vec_perm(vec_lvx(32,  Matrix),        \
                                           vec_lvx(32 + 15, Matrix),    \
                                           vec_lvsl(32, Matrix));       \
    VecPrefix##3  = (vector float)vec_perm(vec_lvx(48,  Matrix),        \
                                           vec_lvx(48 + 15, Matrix),    \
                                           vec_lvsl(48, Matrix))

#define ALIGNED_LOAD(VecPrefix, Matrix)         \
    VecPrefix##0 = vec_lvx(0,  Matrix);         \
    VecPrefix##1 = vec_lvx(16, Matrix);         \
    VecPrefix##2 = vec_lvx(32, Matrix);         \
    VecPrefix##3 = vec_lvx(48, Matrix)

#define MATRIX_MUL                                                          \
    ResultCol0 = vec_madd(vec_vspltw(AR0, 0), BR0, (vector float)(0.0f));   \
    ResultCol0 = vec_madd(vec_vspltw(AR0, 1), BR1, ResultCol0);             \
    ResultCol0 = vec_madd(vec_vspltw(AR0, 2), BR2, ResultCol0);             \
    ResultCol1 = vec_madd(vec_vspltw(AR1, 0), BR0, (vector float)(0.0f));   \
    ResultCol1 = vec_madd(vec_vspltw(AR1, 1), BR1, ResultCol1);             \
    ResultCol1 = vec_madd(vec_vspltw(AR1, 2), BR2, ResultCol1);             \
    ResultCol2 = vec_madd(vec_vspltw(AR2, 0), BR0, (vector float)(0.0f));   \
    ResultCol2 = vec_madd(vec_vspltw(AR2, 1), BR1, ResultCol2);             \
    ResultCol2 = vec_madd(vec_vspltw(AR2, 2), BR2, ResultCol2);             \
    ResultCol3 = vec_madd(vec_vspltw(AR3, 0), BR0, (vector float)(0.0f));   \
    ResultCol3 = vec_madd(vec_vspltw(AR3, 1), BR1, ResultCol3);             \
    ResultCol3 = vec_madd(vec_vspltw(AR3, 2), BR2, ResultCol3);             \
    /* note the extra add here... */                                        \
    ResultCol3 = vec_add(ResultCol3, BR3)

#define ALIGNED_STORE(Result, VecPrefix)        \
    vec_stvx(VecPrefix##0, 0,  Result);         \
    vec_stvx(VecPrefix##1, 16, Result);         \
    vec_stvx(VecPrefix##2, 32, Result);         \
    vec_stvx(VecPrefix##3, 48, Result)


// Partially aligned operands...
void CellAligned101ColumnMatrix4x3Multiply(float * NOALIAS Result,   // aligned
                                           float const* NOALIAS A, // unaligned
                                           float const* NOALIAS B) // aligned
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
void CellAligned110ColumnMatrix4x3Multiply(real32 * NOALIAS Result,  // aligned
                                           real32 const* NOALIAS A, // aligned
                                           real32 const* NOALIAS B) // unaligned
{
    Assert(IS_ALIGNED_16(B));
    Assert(IS_ALIGNED_16(Result));

    DECLARE_VARS;

    // Load the a/b matrice
    {
        ALIGNED_LOAD(AR, A);
        UNALIGNED_LOAD(BR, B);
    }

    MATRIX_MUL;
    ALIGNED_STORE(Result, ResultCol);
}


void CellAlignedColumnMatrix4x3Multiply(real32 * NOALIAS Result,
                                        real32 const* NOALIAS A,
                                        real32 const* NOALIAS B)
{
    Assert(IS_ALIGNED_16(A));
    Assert(IS_ALIGNED_16(B));
    Assert(IS_ALIGNED_16(Result));

    Assert(IS_ALIGNED_16(B));
    Assert(IS_ALIGNED_16(Result));

    DECLARE_VARS;

    // Load the a/b matrice
    {
        ALIGNED_LOAD(AR, A);
        ALIGNED_LOAD(BR, B);
    }

    MATRIX_MUL;
    ALIGNED_STORE(Result, ResultCol);
}

// Partially aligned operands, transposed...
void CellAligned101ColumnMatrix4x3MultiplyTranspose(real32 * NOALIAS Result, // aligned
                                                    real32 const* NOALIAS A, // unaligned
                                                    real32 const* NOALIAS B) // aligned
{
    Assert(IS_ALIGNED_16(B));
    Assert(IS_ALIGNED_16(Result));
    Assert(!StructuresOverlap(*Result, *A));
    Assert(!StructuresOverlap(*Result, *B));

    vector float AR0, AR1, AR2, AR3;
    vector float BR0, BR1, BR2, BR3;
    vector float ResultRow0, ResultRow1, ResultRow2;
    UNUSED_VARIABLE(AR3);

    ALIGN16(real32) TransposeA[] = {
        A[0],  A[4],  A[8],  A[12],
        A[1],  A[5],  A[9],  A[13],
        A[2],  A[6],  A[10], A[14],
        A[3],  A[7],  A[11], A[15],
    };

    static const vector float Constant0001 = {0, 0, 0, 1};

    // Load the a/b matrices
    {
        ALIGNED_LOAD(AR, TransposeA);
        ALIGNED_LOAD(BR, B);
    }

    ResultRow0 = vec_madd(vec_vspltw(BR0, 0), AR0, (vector float)(0.0f));
    ResultRow0 = vec_madd(vec_vspltw(BR1, 0), AR1, ResultRow0);
    ResultRow0 = vec_madd(vec_vspltw(BR2, 0), AR2, ResultRow0);
    ResultRow0 = vec_madd(Constant0001, vec_vspltw(BR3, 0), ResultRow0);

    ResultRow1 = vec_madd(vec_vspltw(BR0, 1), AR0, (vector float)(0.0f));
    ResultRow1 = vec_madd(vec_vspltw(BR1, 1), AR1, ResultRow1);
    ResultRow1 = vec_madd(vec_vspltw(BR2, 1), AR2, ResultRow1);
    ResultRow1 = vec_madd(Constant0001, vec_vspltw(BR3, 1), ResultRow1);

    ResultRow2 = vec_madd(vec_vspltw(BR0, 2),  AR0, (vector float)(0.0f));
    ResultRow2 = vec_madd(vec_vspltw(BR1, 2), AR1, ResultRow2);
    ResultRow2 = vec_madd(vec_vspltw(BR2, 2), AR2, ResultRow2);
    ResultRow2 = vec_madd(Constant0001, vec_vspltw(BR3, 2), ResultRow2);

    vec_stvx(ResultRow0, 0,  Result);
    vec_stvx(ResultRow1, 16, Result);
    vec_stvx(ResultRow2, 32, Result);
}


CELL_VERSION(BuildPositionOrientationWorldPoseOnly)(real32 const* PositionAliased,
                                                    real32 const* OrientationAliased,
                                                    real32 const* ParentMatrixAliased,
                                                    real32*       ResultWorldMatrixAliased)
{
    DECLARE_UNALIASED(Position, real32 const);
    DECLARE_UNALIASED(Orientation, real32 const);
    DECLARE_UNALIASED(ParentMatrix, real32 const);
    DECLARE_UNALIASED(ResultWorldMatrix, real32);

    // Position:          not aligned
    // Orientation:       not aligned
    // ParentMatrix:      align 16
    // ResultWorldMatrix: align 16
    Assert(IS_ALIGNED_16(ParentMatrix));
    Assert(IS_ALIGNED_16(ResultWorldMatrix));

    // Convert Position and orientation to a Matrix (T) with
    //  R R R 0
    //  R R R 0
    //  R R R 0
    //  P P P 1
    // Where R is the 3x3 from the quaternion and P is the position
    //
    // ResultWorldMatrix = 4x3 Mult: (T * ParentMatrix)

    // Nicked from xmmatrix.inl, with the addition of the unaligned
    // quat and pos loads.  We could probably just use the XM function
    // for quat->mat, we're really only nicking the 1110 constant and
    // inserting one extra maddfp.
    vector float PosOrient[4];
    {
        vector float Q0, Q1;
        vector float V0, V1, V2, V3, V4, V5;
        vector float R0, R1, R2;
        static const vector float Constant1110 = {1.0f, 1.0f, 1.0f, 0.0f};

        #define XM_PERMUTE_0X       0x00010203
        #define XM_PERMUTE_0Y       0x04050607
        #define XM_PERMUTE_0Z       0x08090A0B
        #define XM_PERMUTE_0W       0x0C0D0E0F
        #define XM_PERMUTE_1X       0x10111213
        #define XM_PERMUTE_1Y       0x14151617
        #define XM_PERMUTE_1Z       0x18191A1B
        #define XM_PERMUTE_1W       0x1C1D1E1F
        static const vector int SwizzleXXYW     = {XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_0W};
        static const vector int SwizzleZYZW     = {XM_PERMUTE_0Z, XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_0W};
        static const vector int SwizzleYZXW     = {XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_0X, XM_PERMUTE_0W};
        static const vector int Permute0Y0X0X1W = {XM_PERMUTE_0Y, XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_1W};
        static const vector int Permute0Z0Z0Y1W = {XM_PERMUTE_0Z, XM_PERMUTE_0Z, XM_PERMUTE_0Y, XM_PERMUTE_1W};
        static const vector int Permute0Y1X1Y0Z = {XM_PERMUTE_0Y, XM_PERMUTE_1X, XM_PERMUTE_1Y, XM_PERMUTE_0Z};
        static const vector int Permute0X1Z0X1Z = {XM_PERMUTE_0X, XM_PERMUTE_1Z, XM_PERMUTE_0X, XM_PERMUTE_1Z};
        static const vector int Permute0X1X1Y0W = {XM_PERMUTE_0X, XM_PERMUTE_1X, XM_PERMUTE_1Y, XM_PERMUTE_0W};
        static const vector int Permute1Z0Y1W0W = {XM_PERMUTE_1Z, XM_PERMUTE_0Y, XM_PERMUTE_1W, XM_PERMUTE_0W};
        static const vector int Permute1X1Y0Z0W = {XM_PERMUTE_1X, XM_PERMUTE_1Y, XM_PERMUTE_0Z, XM_PERMUTE_0W};
        static const vector float g_XMNegativeZero = (vector float)((vector int){0x80000000, 0x80000000, 0x80000000, 0x80000000});

        // Unaligned quat load
        vector float Quaternion;
        Quaternion = (vector float)vec_lvlx(0, Orientation);
        V0         = (vector float)vec_lvrx(16, Orientation);
        Quaternion = (vector float)vec_or(Quaternion, V0);

        // Unaligned pos load
        vector float PosVector;
        PosVector  = (vector float)vec_lvlx(0, Position);
        V1         = (vector float)vec_lvrx(16, Position);
        PosVector  = (vector float)vec_or(PosVector, V1);

        Q0 = vec_add(Quaternion, Quaternion);

        V2 = vec_perm(Quaternion, Quaternion, (vector unsigned char)SwizzleXXYW);
        V4 = vec_vspltw(Quaternion, 3);

        Q1 = vec_madd(Quaternion, Q0, g_XMNegativeZero);

        V3 = vec_perm(Q0, Q0, (vector unsigned char)SwizzleZYZW);
        V5 = vec_perm(Q0, Q0, (vector unsigned char)SwizzleYZXW);

        V0 = vec_perm(Q1, Constant1110, (vector unsigned char)Permute0Y0X0X1W);
        V1 = vec_perm(Q1, Constant1110, (vector unsigned char)Permute0Z0Z0Y1W);

        V2 = vec_madd(V2, V3, (vector float)g_XMNegativeZero);
        V4 = vec_madd(V4, V5, (vector float)g_XMNegativeZero);

        R0 = vec_sub(Constant1110, V0);

        R1 = vec_add(V2, V4);
        R2 = vec_sub(V2, V4);

        R0 = vec_sub(R0, V1);

        V0 = vec_perm(R1, R2, (vector unsigned char)Permute0Y1X1Y0Z);
        V1 = vec_perm(R1, R2, (vector unsigned char)Permute0X1Z0X1Z);

        PosOrient[0] = vec_perm(R0, V0, (vector unsigned char)Permute0X1X1Y0W);
        PosOrient[1] = vec_perm(R0, V0, (vector unsigned char)Permute1Z0Y1W0W);
        PosOrient[2] = vec_perm(R0, V1, (vector unsigned char)Permute1X1Y0Z0W);
        PosOrient[3] = (vector float){0.0, 0.0, 0.0, 1.0};
        PosOrient[3] = vec_madd(PosVector, Constant1110, PosOrient[3]);
    }

    // ResultWorldMatrix = 4x3 Mult: (T * ParentMatrix)
    CellAlignedColumnMatrix4x3Multiply(ResultWorldMatrix,
                                       (real32 const*)&PosOrient[0],
                                       ParentMatrix);
}


CELL_VERSION(BuildPositionWorldPoseOnly)(real32 const* PositionAliased,
                                         real32 const* ParentMatrixAliased,
                                         real32*       ResultWorldMatrixAliased)
{
    DECLARE_UNALIASED(Position, real32 const);
    DECLARE_UNALIASED(ParentMatrix, real32 const);
    DECLARE_UNALIASED(ResultWorldMatrix, real32);

    // Position:          not aligned
    // ParentMatrix:      align 16
    // ResultWorldMatrix: align 16
    Assert(IS_ALIGNED_16(ParentMatrix));
    Assert(IS_ALIGNED_16(ResultWorldMatrix));

    {
        // Demonstrate to the compiler that the pointers aren't aliased...
        {for(int Idx = 0; Idx < 12; ++Idx)
        {
            ResultWorldMatrix[Idx] = ParentMatrix[Idx];
        }}

        ResultWorldMatrix[12] = (Position[0] * ParentMatrix[0] +
                                 Position[1] * ParentMatrix[4] +
                                 Position[2] * ParentMatrix[8] +
                                 ParentMatrix[12]);
        ResultWorldMatrix[13] = (Position[0] * ParentMatrix[1] +
                                 Position[1] * ParentMatrix[5] +
                                 Position[2] * ParentMatrix[9] +
                                 ParentMatrix[13]);
        ResultWorldMatrix[14] = (Position[0] * ParentMatrix[2] +
                                 Position[1] * ParentMatrix[6] +
                                 Position[2] * ParentMatrix[10] +
                                 ParentMatrix[14]);
        ResultWorldMatrix[15] = ParentMatrix[15];
    }
}


CELL_VERSION(BuildIdentityWorldPoseOnly)(real32 const* ParentMatrixAliased,
                                         real32*       ResultWorldMatrixAliased)
{
    DECLARE_UNALIASED(ParentMatrix, real32 const);
    DECLARE_UNALIASED(ResultWorldMatrix, real32);

    // ParentMatrix:      not aligned
    // ResultWorldMatrix: align 16
    Assert(IS_ALIGNED_16(ParentMatrix));
    Assert(IS_ALIGNED_16(ResultWorldMatrix));

    {for(int Idx = 0; Idx < 16; ++Idx)
    {
        ResultWorldMatrix[Idx] = ParentMatrix[Idx];
    }}
}


CELL_VERSION(BuildFullWorldPoseOnly)(transform const &Transform,
                                     real32 const *ParentMatrix,
                                     real32 * ResultWorldMatrix)
{
    // ParentMatrix:      not aligned
    // ResultWorldMatrix: align 16
    Assert(IS_ALIGNED_16(ParentMatrix));
    Assert(IS_ALIGNED_16(ResultWorldMatrix));

    ALIGN16(matrix_4x4) Temp;
    Assert(IS_ALIGNED_16(&Temp));

    BuildCompositeTransform4x4(Transform, (real32 *)Temp);
    CellAligned110ColumnMatrix4x3Multiply(ResultWorldMatrix, (real32 *)Temp, ParentMatrix);
}


CELL_VERSION(BuildSingleCompositeFromWorldPose)(real32 const* InverseWorld4x4Aliased,
                                                real32 const* WorldMatrixAliased,
                                                real32*       ResultCompositeAliased)
{
    DECLARE_UNALIASED(InverseWorld4x4, real32 const);
    DECLARE_UNALIASED(WorldMatrix, real32 const);
    DECLARE_UNALIASED(ResultComposite, real32);

    Assert(IS_ALIGNED_16(WorldMatrixAliased));
    Assert(IS_ALIGNED_16(ResultCompositeAliased));

    CellAligned101ColumnMatrix4x3Multiply(ResultComposite, InverseWorld4x4, WorldMatrix);
}

CELL_VERSION(BuildSingleCompositeFromWorldPoseTranspose)(real32 const* InverseWorld4x4Aliased,
                                                         real32 const* WorldMatrixAliased,
                                                         real32*       ResultCompositeAliased)
{
    DECLARE_UNALIASED(InverseWorld4x4, real32 const);
    DECLARE_UNALIASED(WorldMatrix, real32 const);
    DECLARE_UNALIASED(ResultComposite, real32);

    Assert(IS_ALIGNED_16(WorldMatrixAliased));
    Assert(IS_ALIGNED_16(ResultCompositeAliased));

    CellAligned101ColumnMatrix4x3MultiplyTranspose(ResultComposite, InverseWorld4x4, WorldMatrix);
}

static void
SetPointers(void)
{
#ifdef USE_ACCELERATED_BONE_OPS

    BuildIdentityWorldPoseOnly = BuildIdentityWorldPoseOnly_Cell;
    BuildPositionWorldPoseOnly = BuildPositionWorldPoseOnly_Cell;
    BuildPositionOrientationWorldPoseOnly = BuildPositionOrientationWorldPoseOnly_Cell;
    BuildFullWorldPoseOnly = BuildFullWorldPoseOnly_Cell;

    BuildSingleCompositeFromWorldPose = BuildSingleCompositeFromWorldPose_Cell;
    BuildSingleCompositeFromWorldPoseTranspose = BuildSingleCompositeFromWorldPoseTranspose_Cell;

#else

    BuildIdentityWorldPoseOnly = BuildIdentityWorldPoseOnly_Generic;
    BuildPositionWorldPoseOnly = BuildPositionWorldPoseOnly_Generic;
    BuildPositionOrientationWorldPoseOnly = BuildPositionOrientationWorldPoseOnly_Generic;
    BuildFullWorldPoseOnly = BuildFullWorldPoseOnly_Generic;

    BuildSingleCompositeFromWorldPose = BuildSingleCompositeFromWorldPose_Generic;
    BuildSingleCompositeFromWorldPoseTranspose = BuildSingleCompositeFromWorldPoseTranspose_Generic;

#endif

}

static void
BuildIdentityWorldPoseOnly_Stub(real32 const *ParentMatrix,
                                real32 *ResultWorldMatrix)
{
    SetPointers();
    BuildIdentityWorldPoseOnly(ParentMatrix,
                               ResultWorldMatrix);
}

static void
BuildPositionWorldPoseOnly_Stub(real32 const *Position,
                                real32 const *ParentMatrix,
                                real32 *ResultWorldMatrix)
{
    SetPointers();
    BuildPositionWorldPoseOnly(Position, ParentMatrix,
                               ResultWorldMatrix);
}

static void
BuildPositionOrientationWorldPoseOnly_Stub(real32 const *Position,
                                             real32 const *Orientation,
                                             real32 const *ParentMatrix,
                                             real32 * ResultWorldMatrix)
{
    SetPointers();
    BuildPositionOrientationWorldPoseOnly(Position,
                                            Orientation,
                                            ParentMatrix,
                                            ResultWorldMatrix);
}

static void
BuildFullWorldPoseOnly_Stub(transform const &Transform,
                              real32 const *ParentMatrix,
                              real32 * ResultWorldMatrix)
{
    SetPointers();
    BuildFullWorldPoseOnly(Transform, ParentMatrix, ResultWorldMatrix);
}

static void
BuildSingleCompositeFromWorldPose_Stub(real32 const *InverseWorld4x4,
                                       real32 const *WorldMatrix,
                                       real32 *ResultComposite)
{
    SetPointers();
    BuildSingleCompositeFromWorldPose(InverseWorld4x4,
                                      WorldMatrix,
                                      ResultComposite);
}

static void
BuildSingleCompositeFromWorldPoseTranspose_Stub(real32 const *InverseWorld4x4,
                                                real32 const *WorldMatrix,
                                                real32 *ResultComposite)
{
    SetPointers();
    BuildSingleCompositeFromWorldPoseTranspose(InverseWorld4x4,
                                               WorldMatrix,
                                               ResultComposite);
}

#define STUB_POINTER(Name)                                              \
OPTIMIZED_DISPATCH(Name) = (OPTIMIZED_DISPATCH_TYPE(Name) *)Name##_Stub

BEGIN_GRANNY_NAMESPACE;

STUB_POINTER(BuildIdentityWorldPoseOnly);
STUB_POINTER(BuildPositionWorldPoseOnly);
STUB_POINTER(BuildPositionOrientationWorldPoseOnly);
STUB_POINTER(BuildFullWorldPoseOnly);

STUB_POINTER(BuildSingleCompositeFromWorldPose);
STUB_POINTER(BuildSingleCompositeFromWorldPoseTranspose);

END_GRANNY_NAMESPACE;

