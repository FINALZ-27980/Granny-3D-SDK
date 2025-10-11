// ========================================================================
// $File: //jeffr/granny_29/rt/granny_bone_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_bone_operations.h"

#include "granny_math.h"
#include "granny_matrix_operations.h"
#include "granny_transform.h"

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

// Routines that only generate the world-pose, not the composite.
GENERIC_DISPATCH(BuildIdentityWorldPoseOnly)(real32 const* ParentMatrix,
                                             real32*       ResultWorldMatrix)
{
    MatrixEquals4x4(ResultWorldMatrix, ParentMatrix);
}

GENERIC_DISPATCH(BuildPositionWorldPoseOnly)(real32 const* Position,
                                             real32 const* ParentMatrix,
                                             real32*       ResultWorldMatrix)
{
    MatrixEquals4x4(ResultWorldMatrix, ParentMatrix);
    ResultWorldMatrix[12] = (Position[0]*ParentMatrix[0] +
                             Position[1]*ParentMatrix[4] +
                             Position[2]*ParentMatrix[8] +
                             ParentMatrix[12]);
    ResultWorldMatrix[13] = (Position[0]*ParentMatrix[1] +
                             Position[1]*ParentMatrix[5] +
                             Position[2]*ParentMatrix[9] +
                             ParentMatrix[13]);
    ResultWorldMatrix[14] = (Position[0]*ParentMatrix[2] +
                             Position[1]*ParentMatrix[6] +
                             Position[2]*ParentMatrix[10] +
                             ParentMatrix[14]);
}

GENERIC_DISPATCH(BuildPositionOrientationWorldPoseOnly)(real32 const* Position,
                                                        real32 const* Orientation,
                                                        real32 const* ParentMatrix,
                                                        real32*       ResultWorldMatrix)
{
    transform Transform;
    Transform.Flags = HasOrientation | HasPosition;
    VectorEquals3(Transform.Position, Position);
    VectorEquals4(Transform.Orientation, Orientation);
    BuildFullWorldPoseOnly_Generic(Transform, ParentMatrix, ResultWorldMatrix);
}

GENERIC_DISPATCH(BuildFullWorldPoseOnly)(transform const& Transform,
                                         real32 const*    ParentMatrix,
                                         real32*          ResultWorldMatrix)
{
    matrix_4x4 Temp;
    BuildCompositeTransform4x4(Transform, (real32 *)Temp);
    ColumnMatrixMultiply4x3Impl(ResultWorldMatrix, (real32 *)Temp, ParentMatrix);
}



// Generating the composite from the world.
GENERIC_DISPATCH(BuildSingleCompositeFromWorldPose)(real32 const* InverseWorld4x4,
                                                    real32 const* WorldMatrix,
                                                    real32*       ResultComposite )
{
    ColumnMatrixMultiply4x3Impl(ResultComposite, InverseWorld4x4, WorldMatrix);
}

GENERIC_DISPATCH(BuildSingleCompositeFromWorldPoseTranspose)(real32 const* InverseWorld4x4,
                                                             real32 const* WorldMatrix,
                                                             real32*       ResultComposite3x4)
{
    ColumnMatrixMultiply4x3TransposeImpl(ResultComposite3x4, InverseWorld4x4, WorldMatrix);
}
