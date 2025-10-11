// ========================================================================
// $File: //jeffr/granny_29/rt/wii/wii_granny_bone_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_bone_operations.h"

#include "granny_math.h"
#include "granny_matrix_operations.h"
#include "granny_memory.h"
#include "granny_skeleton.h"
#include "granny_transform.h"
#include "granny_system_clock.h"

#include <revolution.h>
#include <revolution/mtx/mtx44ext.h>

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define WII_VERSION(Name) static void Name##_Wii
#define USE_ACCELERATED_BONE_OPS 1

USING_GRANNY_NAMESPACE;

#define ALIASED_NAME(Name) Name##Aliased
#define DECLARE_UNALIASED(Name, type) type* NOALIAS Name = Name ## Aliased

WII_VERSION(BuildPositionWorldPoseOnly)(real32 const* ALIASED_NAME(Position),
                                        real32 const* ALIASED_NAME(ParentMatrix),
                                        real32*       ALIASED_NAME(ResultWorldMatrix))
{
    DECLARE_UNALIASED(Position, real32 const);
    DECLARE_UNALIASED(ParentMatrix, real32 const);
    DECLARE_UNALIASED(ResultWorldMatrix, real32);

    Mtx TransformMat;
    MTXTrans(TransformMat, Position[0], Position[1], Position[2]);

    Mtx ParentMat;
    {
        ParentMat[0][0] = ParentMatrix[0 + 0];
        ParentMat[0][1] = ParentMatrix[0 + 4];
        ParentMat[0][2] = ParentMatrix[0 + 8];
        ParentMat[0][3] = ParentMatrix[0 + 12];
        ParentMat[1][0] = ParentMatrix[1 + 0];
        ParentMat[1][1] = ParentMatrix[1 + 4];
        ParentMat[1][2] = ParentMatrix[1 + 8];
        ParentMat[1][3] = ParentMatrix[1 + 12];
        ParentMat[2][0] = ParentMatrix[2 + 0];
        ParentMat[2][1] = ParentMatrix[2 + 4];
        ParentMat[2][2] = ParentMatrix[2 + 8];
        ParentMat[2][3] = ParentMatrix[2 + 12];
    }

    MTXConcat(ParentMat, TransformMat, TransformMat);

    // Untranspose
    {
        ResultWorldMatrix[0 + 0]  = TransformMat[0][0];
        ResultWorldMatrix[0 + 4]  = TransformMat[0][1];
        ResultWorldMatrix[0 + 8]  = TransformMat[0][2];
        ResultWorldMatrix[0 + 12] = TransformMat[0][3];
        ResultWorldMatrix[1 + 0]  = TransformMat[1][0];
        ResultWorldMatrix[1 + 4]  = TransformMat[1][1];
        ResultWorldMatrix[1 + 8]  = TransformMat[1][2];
        ResultWorldMatrix[1 + 12] = TransformMat[1][3];
        ResultWorldMatrix[2 + 0]  = TransformMat[2][0];
        ResultWorldMatrix[2 + 4]  = TransformMat[2][1];
        ResultWorldMatrix[2 + 8]  = TransformMat[2][2];
        ResultWorldMatrix[2 + 12] = TransformMat[2][3];
        ResultWorldMatrix[3 + 0]  = 0;
        ResultWorldMatrix[3 + 4]  = 0;
        ResultWorldMatrix[3 + 8]  = 0;
        ResultWorldMatrix[3 + 12] = 1;
    }
}

WII_VERSION(BuildPositionOrientationWorldPoseOnly)(real32 const* ALIASED_NAME(Position),
                                                   real32 const* ALIASED_NAME(Orientation),
                                                   real32 const* ALIASED_NAME(ParentMatrix),
                                                   real32*       ALIASED_NAME(ResultWorldMatrix))
{
    DECLARE_UNALIASED(Position, real32 const);
    DECLARE_UNALIASED(Orientation, real32 const);
    DECLARE_UNALIASED(ParentMatrix, real32 const);
    DECLARE_UNALIASED(ResultWorldMatrix, real32);

    Mtx TransformMat;
    {
        MTXQuat(TransformMat, (QuaternionPtr)Orientation);
        TransformMat[0][3] = Position[0];
        TransformMat[1][3] = Position[1];
        TransformMat[2][3] = Position[2];
    }

    Mtx ParentMat;
    {
        ParentMat[0][0] = ParentMatrix[0 + 0];
        ParentMat[0][1] = ParentMatrix[0 + 4];
        ParentMat[0][2] = ParentMatrix[0 + 8];
        ParentMat[0][3] = ParentMatrix[0 + 12];
        ParentMat[1][0] = ParentMatrix[1 + 0];
        ParentMat[1][1] = ParentMatrix[1 + 4];
        ParentMat[1][2] = ParentMatrix[1 + 8];
        ParentMat[1][3] = ParentMatrix[1 + 12];
        ParentMat[2][0] = ParentMatrix[2 + 0];
        ParentMat[2][1] = ParentMatrix[2 + 4];
        ParentMat[2][2] = ParentMatrix[2 + 8];
        ParentMat[2][3] = ParentMatrix[2 + 12];
    }

    MTXConcat(ParentMat, TransformMat, TransformMat);

    // Untranspose
    {
        ResultWorldMatrix[0 + 0]  = TransformMat[0][0];
        ResultWorldMatrix[0 + 4]  = TransformMat[0][1];
        ResultWorldMatrix[0 + 8]  = TransformMat[0][2];
        ResultWorldMatrix[0 + 12] = TransformMat[0][3];
        ResultWorldMatrix[1 + 0]  = TransformMat[1][0];
        ResultWorldMatrix[1 + 4]  = TransformMat[1][1];
        ResultWorldMatrix[1 + 8]  = TransformMat[1][2];
        ResultWorldMatrix[1 + 12] = TransformMat[1][3];
        ResultWorldMatrix[2 + 0]  = TransformMat[2][0];
        ResultWorldMatrix[2 + 4]  = TransformMat[2][1];
        ResultWorldMatrix[2 + 8]  = TransformMat[2][2];
        ResultWorldMatrix[2 + 12] = TransformMat[2][3];
        ResultWorldMatrix[3 + 0]  = 0;
        ResultWorldMatrix[3 + 4]  = 0;
        ResultWorldMatrix[3 + 8]  = 0;
        ResultWorldMatrix[3 + 12] = 1;
    }
}

WII_VERSION(BuildFullWorldPoseOnly)(transform const& Transform,
                                    real32 const*    ALIASED_NAME(ParentMatrix),
                                    real32*          ALIASED_NAME(ResultWorldMatrix))
{
    DECLARE_UNALIASED(ParentMatrix, real32 const);
    DECLARE_UNALIASED(ResultWorldMatrix, real32);

    Mtx TransformMat;
    {
        MTXQuat(TransformMat, (QuaternionPtr)Transform.Orientation);

        if(Transform.Flags & HasScaleShear)
        {
            // Need to transpose for Wii math
            Mtx ScaleShear;
            ScaleShear[0][0] = Transform.ScaleShear[0][0];
            ScaleShear[0][1] = Transform.ScaleShear[1][0];
            ScaleShear[0][2] = Transform.ScaleShear[2][0];
            ScaleShear[0][3] = 0.0f;
            ScaleShear[1][0] = Transform.ScaleShear[0][1];
            ScaleShear[1][1] = Transform.ScaleShear[1][1];
            ScaleShear[1][2] = Transform.ScaleShear[2][1];
            ScaleShear[1][3] = 0.0f;
            ScaleShear[2][0] = Transform.ScaleShear[0][2];
            ScaleShear[2][1] = Transform.ScaleShear[1][2];
            ScaleShear[2][2] = Transform.ScaleShear[2][2];
            ScaleShear[2][3] = 0.0f;
            MTXConcat(TransformMat, ScaleShear, TransformMat);
        }

        TransformMat[0][3] = Transform.Position[0];
        TransformMat[1][3] = Transform.Position[1];
        TransformMat[2][3] = Transform.Position[2];
    }

    Mtx ParentMat;
    {
        ParentMat[0][0] = ParentMatrix[0 + 0];
        ParentMat[0][1] = ParentMatrix[0 + 4];
        ParentMat[0][2] = ParentMatrix[0 + 8];
        ParentMat[0][3] = ParentMatrix[0 + 12];
        ParentMat[1][0] = ParentMatrix[1 + 0];
        ParentMat[1][1] = ParentMatrix[1 + 4];
        ParentMat[1][2] = ParentMatrix[1 + 8];
        ParentMat[1][3] = ParentMatrix[1 + 12];
        ParentMat[2][0] = ParentMatrix[2 + 0];
        ParentMat[2][1] = ParentMatrix[2 + 4];
        ParentMat[2][2] = ParentMatrix[2 + 8];
        ParentMat[2][3] = ParentMatrix[2 + 12];
    }

    MTXConcat(ParentMat, TransformMat, TransformMat);

    // Untranspose
    {
        ResultWorldMatrix[0 + 0]  = TransformMat[0][0];
        ResultWorldMatrix[0 + 4]  = TransformMat[0][1];
        ResultWorldMatrix[0 + 8]  = TransformMat[0][2];
        ResultWorldMatrix[0 + 12] = TransformMat[0][3];
        ResultWorldMatrix[1 + 0]  = TransformMat[1][0];
        ResultWorldMatrix[1 + 4]  = TransformMat[1][1];
        ResultWorldMatrix[1 + 8]  = TransformMat[1][2];
        ResultWorldMatrix[1 + 12] = TransformMat[1][3];
        ResultWorldMatrix[2 + 0]  = TransformMat[2][0];
        ResultWorldMatrix[2 + 4]  = TransformMat[2][1];
        ResultWorldMatrix[2 + 8]  = TransformMat[2][2];
        ResultWorldMatrix[2 + 12] = TransformMat[2][3];
        ResultWorldMatrix[3 + 0]  = 0;
        ResultWorldMatrix[3 + 4]  = 0;
        ResultWorldMatrix[3 + 8]  = 0;
        ResultWorldMatrix[3 + 12] = 1;
    }
}


WII_VERSION(BuildSingleCompositeFromWorldPose)(real32 const* ALIASED_NAME(InverseWorld4x4),
                                               real32 const* ALIASED_NAME(WorldMatrix),
                                               real32*       ALIASED_NAME(ResultComposite))
{
    DECLARE_UNALIASED(InverseWorld4x4, real32 const);
    DECLARE_UNALIASED(WorldMatrix, real32 const);
    DECLARE_UNALIASED(ResultComposite, real32);

    ColumnMatrixMultiply4x3Impl(ResultComposite, InverseWorld4x4, WorldMatrix);
}

WII_VERSION(BuildSingleCompositeFromWorldPoseTranspose)(real32 const *InverseWorld4x4,
                                                        real32 const *WorldMatrix,
                                                        real32 *ResultComposite3x4)
{
    // TODO: not yet a common path on the Wii
    ColumnMatrixMultiply4x3TransposeImpl(ResultComposite3x4, InverseWorld4x4, WorldMatrix);
}


static void
SetPointers(void)
{
    BuildIdentityWorldPoseOnly = BuildIdentityWorldPoseOnly_Generic;

#if USE_ACCELERATED_BONE_OPS
    BuildPositionWorldPoseOnly                 = BuildPositionWorldPoseOnly_Wii;
    BuildPositionOrientationWorldPoseOnly      = BuildPositionOrientationWorldPoseOnly_Wii;
    BuildFullWorldPoseOnly                     = BuildFullWorldPoseOnly_Wii;
    BuildSingleCompositeFromWorldPose          = BuildSingleCompositeFromWorldPose_Wii;
    BuildSingleCompositeFromWorldPoseTranspose = BuildSingleCompositeFromWorldPoseTranspose_Wii;
#else
    BuildPositionWorldPoseOnly                 = BuildPositionWorldPoseOnly_Generic;
    BuildPositionOrientationWorldPoseOnly      = BuildPositionOrientationWorldPoseOnly_Generic;
    BuildFullWorldPoseOnly                     = BuildFullWorldPoseOnly_Generic;
    BuildSingleCompositeFromWorldPose          = BuildSingleCompositeFromWorldPose_Generic;
    BuildSingleCompositeFromWorldPoseTranspose = BuildSingleCompositeFromWorldPoseTranspose_Generic;
#endif

    TestFastMatrixOps();
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
