// ========================================================================
// $File: //jeffr/granny_29/rt/xenon/xenon_granny_bone_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_bone_operations.h"
#include "xenon_granny_xtl.h"

#include "xenon_granny_matrix_operations.h"
#include "granny_math.h"
#include "granny_memory.h"
#include "granny_skeleton.h"
#include "granny_transform.h"


#include <xboxmath.h>

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

#define XENON_VERSION(Name) static void Name##_Xenon
#define DECLARE_UNALIASED(Name, type) type* NOALIAS Name = Name ## Aliased


XENON_VERSION(BuildPositionOrientationWorldPoseOnly)(real32 const* PositionAliased,
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
    XMMATRIX PosOrient;
    {
        XMVECTOR Quaternion;
        XMVECTOR PosVector;
        XMVECTOR Q0, Q1;
        XMVECTOR V0, V1, V2;
        XMVECTOR R1, R2;
        XMVECTOR ZO;
        XMVECTOR Constant1110;

        XMDUMMY_INITIALIZE_VECTOR(ZO);

        // Unaligned quat load
        Quaternion = __lvlx(Orientation, 0);
        V0         = __lvrx(Orientation, 16);
        Quaternion = __vor(Quaternion, V0);

        // Unaligned pos load
        PosVector  = __lvlx(Position, 0);
        V1         = __lvrx(Position, 16);
        PosVector  = __vor(PosVector, V1);

        // Straight from xmmatrix.inl until the note below...
        Q0 = __vaddfp(Quaternion, Quaternion);
        Q1 = __vmulfp(Quaternion, Q0);

        ZO = __vupkd3d(ZO, VPACK_NORMSHORT2);
        Constant1110 = __vpermwi(ZO, 0xFE);

        V0 = __vpermwi(Q1, 0x40);
        V1 = __vpermwi(Q1, 0xA4);

        PosOrient.r[0] = __vsubfp(Constant1110, V0);
        PosOrient.r[0] = __vsubfp(PosOrient.r[0], V1);

        V0 = __vpermwi(Quaternion, 0x7);
        V1 = __vpermwi(Q0, 0x9B);
        V0 = __vmulfp(V0, V1);

        V1 = __vspltw(Quaternion, 3);
        V2 = __vpermwi(Q0, 0x63);
        V1 = __vmulfp(V1, V2);

        R1 = __vaddfp(V0, V1);
        R2 = __vsubfp(V0, V1);

        PosOrient.r[0] = __vrlimi(PosOrient.r[0], ZO, 1, 3);
        PosOrient.r[1] = __vpermwi(PosOrient.r[0], 0x7);

        V0 = __vpermwi(R1, 0x42);
        PosOrient.r[2] = __vsldoi(R2, R1, 2 << 2);
        V0 = __vrlimi(V0, R2, 0x6, 3);
        PosOrient.r[2] = __vpermwi(PosOrient.r[2], 0x88);

        // We need to account for the position, so this is slightly different...
        PosOrient.r[3] = __vpermwi(ZO, 0xAB);
        PosOrient.r[3] = __vmaddfp(PosVector, Constant1110, PosOrient.r[3]);

        PosOrient.r[2] = __vrlimi(PosOrient.r[2], PosOrient.r[0], 0x3, 0);
        PosOrient.r[1] = __vmrglw(V0, PosOrient.r[1]);
        PosOrient.r[0] = __vrlimi(PosOrient.r[0], V0, 0x6, 3);
    }

    // ResultWorldMatrix = 4x3 Mult: (T * ParentMatrix)
    XenonAlignedColumnMatrix4x3Multiply(ResultWorldMatrix,
                                        (real32 const*)&PosOrient.m[0][0],
                                        ParentMatrix);
}


XENON_VERSION(BuildPositionWorldPoseOnly)(real32 const* PositionAliased,
                                          real32 const* ParentMatrixAliased,
                                          real32*       ResultWorldMatrixAliased)
{
    DECLARE_UNALIASED(Position, real32 const);
    DECLARE_UNALIASED(ParentMatrix, real32 const);
    DECLARE_UNALIASED(ResultWorldMatrix, real32);

    // Position:          not aligned
    // ParentMatrix:      align 16
    // InverseWorld4x4:   not aligned
    // Result:            align 16
    // ResultWorldMatrix: align 16
    Assert(IS_ALIGNED_16(ParentMatrix));
    Assert(IS_ALIGNED_16(ResultWorldMatrix));

    {
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


XENON_VERSION(BuildIdentityWorldPoseOnly)(real32 const* ParentMatrixAliased,
                                          real32*       ResultWorldMatrixAliased)
{
    DECLARE_UNALIASED(ParentMatrix, real32 const);
    DECLARE_UNALIASED(ResultWorldMatrix, real32);

    // ParentMatrix:      not aligned
    // InverseWorld4x4:   not aligned
    // Result:            align 16
    // ResultWorldMatrix: align 16
    Assert(IS_ALIGNED_16(ParentMatrix));
    Assert(IS_ALIGNED_16(ResultWorldMatrix));

    {for(int Idx = 0; Idx < 16; ++Idx)
    {
        ResultWorldMatrix[Idx] = ParentMatrix[Idx];
    }}
}


XENON_VERSION(BuildFullWorldPoseOnly)(transform const& Transform,
                                      real32 const*    ParentMatrixAliased,
                                      real32*          ResultWorldMatrixAliased)
{
    DECLARE_UNALIASED(ParentMatrix, real32 const);
    DECLARE_UNALIASED(ResultWorldMatrix, real32);

    // ParentMatrix:      not aligned
    // Result:            align 16
    // ResultWorldMatrix: align 16
    Assert(IS_ALIGNED_16(ParentMatrix));
    Assert(IS_ALIGNED_16(ResultWorldMatrix));

    ALIGN16(matrix_4x4) Temp;
    Assert(IS_ALIGNED_16(&Temp));

    BuildCompositeTransform4x4(Transform, (real32 *)Temp);

    XenonAligned110ColumnMatrix4x3Multiply(ResultWorldMatrix, (real32 *)Temp, ParentMatrix);
}


XENON_VERSION(BuildSingleCompositeFromWorldPose)(real32 const* InverseWorld4x4Aliased,
                                                 real32 const* WorldMatrixAliased,
                                                 real32*       ResultCompositeAliased)
{
    DECLARE_UNALIASED(InverseWorld4x4, real32 const);
    DECLARE_UNALIASED(WorldMatrix, real32 const);
    DECLARE_UNALIASED(ResultComposite, real32);

    Assert(IS_ALIGNED_16(WorldMatrixAliased));
    Assert(IS_ALIGNED_16(ResultCompositeAliased));

    XenonAligned101ColumnMatrix4x3Multiply(ResultComposite, InverseWorld4x4, WorldMatrix);
}

XENON_VERSION(BuildSingleCompositeFromWorldPoseTranspose)(real32 const* InverseWorld4x4Aliased,
                                                          real32 const* WorldMatrixAliased,
                                                          real32*       ResultCompositeAliased)
{
    DECLARE_UNALIASED(InverseWorld4x4, real32 const);
    DECLARE_UNALIASED(WorldMatrix, real32 const);
    DECLARE_UNALIASED(ResultComposite, real32);

    Assert(IS_ALIGNED_16(WorldMatrixAliased));
    Assert(IS_ALIGNED_16(ResultCompositeAliased));

    XenonAligned101ColumnMatrix4x3MultiplyTranspose(ResultComposite, InverseWorld4x4, WorldMatrix);
}


static void
SetPointers(void)
{
#ifdef USE_ACCELERATED_BONE_OPS

    BuildIdentityWorldPoseOnly = BuildIdentityWorldPoseOnly_Xenon;
    BuildPositionWorldPoseOnly = BuildPositionWorldPoseOnly_Xenon;
    BuildPositionOrientationWorldPoseOnly = BuildPositionOrientationWorldPoseOnly_Xenon;
    BuildFullWorldPoseOnly = BuildFullWorldPoseOnly_Xenon;

    BuildSingleCompositeFromWorldPose = BuildSingleCompositeFromWorldPose_Xenon;
    BuildSingleCompositeFromWorldPoseTranspose = BuildSingleCompositeFromWorldPoseTranspose_Xenon;

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
    BuildFullWorldPoseOnly(Transform, ParentMatrix,
                           ResultWorldMatrix);
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


#define STUB_POINTER(Name)                                                  \
    OPTIMIZED_DISPATCH(Name) = (OPTIMIZED_DISPATCH_TYPE(Name) *)Name##_Stub

BEGIN_GRANNY_NAMESPACE;

STUB_POINTER(BuildIdentityWorldPoseOnly);
STUB_POINTER(BuildPositionWorldPoseOnly);
STUB_POINTER(BuildPositionOrientationWorldPoseOnly);
STUB_POINTER(BuildFullWorldPoseOnly);

STUB_POINTER(BuildSingleCompositeFromWorldPose);
STUB_POINTER(BuildSingleCompositeFromWorldPoseTranspose);

END_GRANNY_NAMESPACE;
