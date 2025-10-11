// ========================================================================
// $File: //jeffr/granny_29/rt/x86/x86_granny_bone_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_bone_operations.h"

#include "granny_transform.h"
#include "granny_math.h"
#include "granny_matrix_operations.h"
#include "x86_granny_cpu_queries.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

USING_GRANNY_NAMESPACE;

#define USE_ACCELERATED_BONE_OPS 1


#define SSE_VERSION(function) static void function##_SSE

SSE_VERSION(BuildPositionOrientationWorldPoseOnly)(real32 const* Position,
                                                   real32 const* Orientation,
                                                   real32 const* ParentMatrix,
                                                   real32*       ResultWorldMatrix)
{
    real32 XsYs;

    static real32 onevalue = 1.0f;

    __asm
    {
#define QM0 xmm0
#define QM1 xmm1

#define TEMP0 xmm2
#define TEMP1 xmm3
#define TEMP2 xmm4
#define TEMP3 xmm6
#define TEMP4 xmm7

#define POS esi
#define ORI eax

#define WSM edi
#define PM  ecx

        mov      ORI, [ Orientation ];
        mov      PM,  [ ParentMatrix ];
        mov      WSM, [ ResultWorldMatrix ];
        movups   QM0, [ ORI ];

        prefetchnta [ PM ];
        prefetcht0  [ WSM ];

        // Turn quaternion into 3x3 matrix
        movaps   TEMP3, QM0;
        movaps   TEMP0, QM0;
        movaps   TEMP1, QM0;
        movaps   TEMP2, QM0;

        shufps   QM0, QM0, 144;
        shufps   TEMP3, TEMP3, 72;
        mulps    QM0, TEMP3;

        shufps   TEMP0, TEMP0, 60;
        mulps    TEMP0, TEMP1;

        mulps    TEMP2, TEMP2;

        addps    QM0, QM0;
        addps    TEMP0, TEMP0;
        addps    TEMP2, TEMP2;

        movaps   QM1, QM0;
        addps    QM0, TEMP0;
        subps    QM1, TEMP0;

        movaps   TEMP1, TEMP2;
        movss    TEMP0, TEMP2;
        movss    TEMP3, TEMP2;
        movss    TEMP4, dword ptr [ onevalue ];
        shufps   TEMP2, TEMP2, 2;
        shufps   TEMP1, TEMP2, 1;

        movss    QM1, TEMP4;
        movss    QM0, TEMP4;

        addss    TEMP3, TEMP2; // X+Z
        addss    TEMP2, TEMP1; // Z+Y
        addss    TEMP0, TEMP1; // X+Y

        subss    QM0, TEMP3;
        subss    QM1, TEMP2;
        subss    TEMP4, TEMP0;

        movss    [XsYs], TEMP4;

        // ok, at this point:
        // QM0 0 = XS+ZS, QM0 1 = XZ+WY, QM0 2 = XY+WZ, QM0 3 = YZ+WX
        // QM1 0 = YS+ZS, QM1 1 = XZ-WY, QM1 2 = XY-WZ, QM1 3 = YZ-WX
        // stack var XsYs = XS+YS

        // now start the A*B matrix multiply
        mov      POS, [ Position ];

        movss    TEMP0, QM1;
        movaps   TEMP1, QM0;
        movaps   TEMP2, QM1;

        shufps   TEMP0, TEMP0, 0;
        shufps   TEMP1, TEMP1, 170;
        shufps   TEMP2, TEMP2, 85;

        mulps    TEMP0, [ PM ];
        mulps    TEMP1, [ PM + 16 ];
        addps    TEMP0, TEMP1;
        mulps    TEMP2, [ PM + 32 ];
        addps    TEMP0, TEMP2;
        movaps   [ WSM ], TEMP0;


        movaps   TEMP0, QM1;
        movss    TEMP1, QM0;
        movaps   TEMP2, QM0;

        shufps   TEMP0, TEMP0, 170;
        shufps   TEMP1, TEMP1, 0;
        shufps   TEMP2, TEMP2, 255;

        mulps    TEMP0, [ PM ];
        mulps    TEMP1, [ PM + 16 ];
        addps    TEMP0, TEMP1;
        mulps    TEMP2, [ PM + 32 ];
        addps    TEMP0, TEMP2;
        movaps   [ WSM + 16 ], TEMP0;

        movss    TEMP2, [ XsYs ];

        shufps   QM0, QM0, 85;
        shufps   QM1, QM1, 255;
        shufps   TEMP2, TEMP2, 0;

        mulps    QM0, [ PM ];
        mulps    QM1, [ PM + 16 ];
        addps    QM0, QM1;
        mulps    TEMP2, [ PM + 32 ];
        addps    QM0, TEMP2;
        movaps   [ WSM + 32 ], QM0;


        movss    TEMP0, dword ptr [ POS ];
        movss    TEMP1, dword ptr [ POS + 4 ];
        movss    TEMP2, dword ptr [ POS + 8 ];

        shufps   TEMP0, TEMP0, 0;
        shufps   TEMP1, TEMP1, 0;
        shufps   TEMP2, TEMP2, 0;

        mulps    TEMP0, [ PM ];
        mulps    TEMP1, [ PM + 16 ];
        addps    TEMP0, TEMP1;
        mulps    TEMP2, [ PM + 32 ];
        addps    TEMP0, [ PM + 48 ];
        addps    TEMP0, TEMP2;
        movaps   [ WSM + 48 ], TEMP0;
#undef QM0
#undef QM1

#undef TEMP0
#undef TEMP1
#undef TEMP2
#undef TEMP3
#undef TEMP4

#undef POS
#undef ORI

#undef WSM
#undef PM
    }
}

SSE_VERSION(BuildFullWorldPoseOnly)(transform const& Transform,
                                    real32 const*    ParentMatrix,
                                    real32*          ResultWorldMatrix)
{
    ALIGN16(matrix_4x4) Temp;
    BuildCompositeTransform4x4(Transform, (real32 *)Temp);
    ColumnMatrixMultiply4x3Impl(ResultWorldMatrix, (real32 *)Temp, ParentMatrix);
}


static void
SetPointers(void)
{
    BuildIdentityWorldPoseOnly                 = BuildIdentityWorldPoseOnly_Generic;
    BuildPositionWorldPoseOnly                 = BuildPositionWorldPoseOnly_Generic;
    BuildPositionOrientationWorldPoseOnly      = BuildPositionOrientationWorldPoseOnly_Generic;
    BuildFullWorldPoseOnly                     = BuildFullWorldPoseOnly_Generic;
    BuildSingleCompositeFromWorldPose          = BuildSingleCompositeFromWorldPose_Generic;
    BuildSingleCompositeFromWorldPoseTranspose = BuildSingleCompositeFromWorldPoseTranspose_Generic;

#if USE_ACCELERATED_BONE_OPS
    if (SSEIsAvailable())
    {
        BuildPositionOrientationWorldPoseOnly      = BuildPositionOrientationWorldPoseOnly_SSE;
        BuildFullWorldPoseOnly                     = BuildFullWorldPoseOnly_SSE;
        return;
    }
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


#define STUB_POINTER(Name) OPTIMIZED_DISPATCH(Name) = (OPTIMIZED_DISPATCH_TYPE(Name) *)Name##_Stub

BEGIN_GRANNY_NAMESPACE;

STUB_POINTER(BuildIdentityWorldPoseOnly);
STUB_POINTER(BuildPositionWorldPoseOnly);
STUB_POINTER(BuildPositionOrientationWorldPoseOnly);
STUB_POINTER(BuildFullWorldPoseOnly);

STUB_POINTER(BuildSingleCompositeFromWorldPose);
STUB_POINTER(BuildSingleCompositeFromWorldPoseTranspose);

END_GRANNY_NAMESPACE;
