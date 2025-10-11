// ========================================================================
// $File: //jeffr/granny_29/rt/granny_transform.cpp $
// $DateTime: 2012/05/16 15:18:35 $
// $Change: 37432 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_transform.h"

#include "granny_limits.h"
#include "granny_math.h"

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

#if !PROCESSOR_CELL_SPU

void GRANNY
MakeIdentity(transform &Result)
{
    Result.Flags = 0;
    VectorZero3(Result.Position);
    VectorEquals4(Result.Orientation, GlobalWAxis);
    MatrixIdentity3x3(Result.ScaleShear);
}

void GRANNY
ZeroTransform(transform &Result)
{
    Result.Flags = 0;
    VectorZero3(Result.Position);
    VectorZero4(Result.Orientation);
    MatrixZero3x3((real32 *)Result.ScaleShear);
}

real32 GRANNY
GetTransformDeterminant(transform const &Transform)
{
    return(MatrixDeterminant3x3((real32 const *)Transform.ScaleShear));
}

void GRANNY
TransformVectorInPlace(real32 *Result, transform const &Transform)
{
    VectorTransform3(Result, (real32 *)Transform.ScaleShear);
    NormalQuaternionTransform3(Result, Transform.Orientation);
}

void GRANNY
TransformVectorInPlaceTransposed(real32 *Result, transform const &Transform)
{
    quad InverseOrientation = {-Transform.Orientation[0],
                               -Transform.Orientation[1],
                               -Transform.Orientation[2],
                               Transform.Orientation[3]};
    NormalQuaternionTransform3(Result, InverseOrientation);
    TransposeVectorTransform3(Result, (real32 const *)Transform.ScaleShear);
}

void GRANNY
TransformVector(real32 *Dest, transform const &Transform,
                real32 const *Source)
{
    VectorTransform3(Dest, (real32 const *)Transform.ScaleShear, Source);
    NormalQuaternionTransform3(Dest, Transform.Orientation);
}

void GRANNY
TransformPointInPlace(real32 *Result, transform const &Transform)
{
    VectorTransform3(Result, (real32 *)Transform.ScaleShear);
    NormalQuaternionTransform3(Result, Transform.Orientation);
    VectorAdd3(Result, Transform.Position);
}

void GRANNY
TransformPoint(real32 *Dest, transform const &Transform,
               real32 const *Source)
{
    VectorTransform3(Dest, (real32 const *)Transform.ScaleShear, Source);
    NormalQuaternionTransform3(Dest, Transform.Orientation);
    VectorAdd3(Dest, Transform.Position);
}

void GRANNY
PreMultiplyByInverse(transform &Transform, transform const &PreMult)
{
    // TODO: Optimize this once Multiply() works optimally
    transform Temp, Temp2;
    BuildInverse(Temp, PreMult);
    Multiply(Temp2, Temp, Transform);
    Transform = Temp2;
}

void GRANNY
PostMultiplyBy(transform &Transform, transform const &PostMult)
{
    // TODO: Optimize this once Multiply() works optimally
    transform Temp;
    Multiply(Temp, Transform, PostMult);
    Transform = Temp;
}

void GRANNY
LinearBlendTransform(transform &Result, transform const &A, real32 t, transform const &B)
{
    Result.Flags = A.Flags | B.Flags;

    if(Result.Flags & HasPosition)
    {
        ScaleVectorPlusScaleVector3(Result.Position,
                                    (1.0f - t), A.Position,
                                    t, B.Position);
    }
    else
    {
        VectorZero3(Result.Position);
    }

    if(Result.Flags & HasOrientation)
    {
        real32 IP = InnerProduct4(A.Orientation, B.Orientation);
        ScaleVectorPlusScaleVector4(Result.Orientation,
                                    (1.0f - t), A.Orientation,
                                    ((IP >= 0) ? t : -t), B.Orientation);
        Normalize4(Result.Orientation);
    }
    else
    {
        VectorEquals4(Result.Orientation, GlobalWAxis);
    }

    if(Result.Flags & HasScaleShear)
    {
        ScaleMatrixPlusScaleMatrix3x3((real32 *)Result.ScaleShear,
                                      (1.0f - t), (real32 const *)A.ScaleShear,
                                      t, (real32 const *)B.ScaleShear);
    }
    else
    {
        MatrixIdentity3x3(Result.ScaleShear);
    }
}

void GRANNY
LinearBlendTransformNeighborhooded(transform &Result, transform const &A, real32 t, transform const &B, transform const& Neighborhood)
{
    Result.Flags = A.Flags | B.Flags;

    if(Result.Flags & HasPosition)
    {
        ScaleVectorPlusScaleVector3(Result.Position,
                                    (1.0f - t), A.Position,
                                    t, B.Position);
    }
    else
    {
        VectorZero3(Result.Position);
    }

    if(Result.Flags & HasOrientation)
    {
        real32 IPA = InnerProduct4(A.Orientation, Neighborhood.Orientation);
        real32 IPB = InnerProduct4(B.Orientation, Neighborhood.Orientation);
        ScaleVectorPlusScaleVector4(Result.Orientation,
                                    ((IPA >= 0) ? (1.0f - t) : (t - 1.0f)), A.Orientation,
                                    ((IPB >= 0) ?          t :         -t), B.Orientation);
        Normalize4(Result.Orientation);
    }
    else
    {
        VectorEquals4(Result.Orientation, GlobalWAxis);
    }

    if(Result.Flags & HasScaleShear)
    {
        ScaleMatrixPlusScaleMatrix3x3((real32 *)Result.ScaleShear,
                                      (1.0f - t), (real32 const *)A.ScaleShear,
                                      t, (real32 const *)B.ScaleShear);
    }
    else
    {
        MatrixIdentity3x3(Result.ScaleShear);
    }
}


void GRANNY
SimilarityTransform(transform &Result, real32 const *Affine3,
                    real32 const *Linear3x3, real32 const *InverseLinear3x3)
{
    InPlaceSimilarityTransform(Affine3, Linear3x3, InverseLinear3x3,
                               Result.Position, Result.Orientation,
                               (real32 *)Result.ScaleShear);
    ResetTransformFlags(Result);
}

void GRANNY
BuildCompositeTransform(transform const &Transform,
                        int32 Stride, real32 *Composite3x3)
{
    triple Orientation[3];
    MatrixEqualsQuaternion3x3((real32 *)Orientation, Transform.Orientation);

    triple Upper3x3[3];
    MatrixMultiply3x3((real32 *)Upper3x3,
                      (real32 *)Orientation,
                      (real32 *)Transform.ScaleShear);

    Composite3x3[0] = Upper3x3[0][0];
    Composite3x3[1] = Upper3x3[1][0];
    Composite3x3[2] = Upper3x3[2][0];
    Composite3x3 += Stride;

    Composite3x3[0] = Upper3x3[0][1];
    Composite3x3[1] = Upper3x3[1][1];
    Composite3x3[2] = Upper3x3[2][1];
    Composite3x3 += Stride;

    Composite3x3[0] = Upper3x3[0][2];
    Composite3x3[1] = Upper3x3[1][2];
    Composite3x3[2] = Upper3x3[2][2];
    Composite3x3 += Stride;
}

void GRANNY
BuildCompositeTransform4x4(transform const &Transform, real32 *Composite4x4)
{
    triple Orientation[3];
    MatrixEqualsQuaternion3x3((real32 *)Orientation, Transform.Orientation);

    triple *Upper3x3;
    triple MultBuffer[3];
    if(Transform.Flags & HasScaleShear)
    {
        MatrixMultiply3x3((real32 *)MultBuffer,
                          (real32 *)Orientation,
                          (real32 *)Transform.ScaleShear);
        Upper3x3 = MultBuffer;
    }
    else
    {
        Upper3x3 = Orientation;
    }

    Composite4x4[0] = Upper3x3[0][0];
    Composite4x4[1] = Upper3x3[1][0];
    Composite4x4[2] = Upper3x3[2][0];
    Composite4x4[3] = 0.0f;
    Composite4x4 += 4;

    Composite4x4[0] = Upper3x3[0][1];
    Composite4x4[1] = Upper3x3[1][1];
    Composite4x4[2] = Upper3x3[2][1];
    Composite4x4[3] = 0.0f;
    Composite4x4 += 4;

    Composite4x4[0] = Upper3x3[0][2];
    Composite4x4[1] = Upper3x3[1][2];
    Composite4x4[2] = Upper3x3[2][2];
    Composite4x4[3] = 0.0f;
    Composite4x4 += 4;

    Composite4x4[0] = Transform.Position[0];
    Composite4x4[1] = Transform.Position[1];
    Composite4x4[2] = Transform.Position[2];
    Composite4x4[3] = 1.0f;
}

void GRANNY
BuildCompositeTransform4x3(transform const &Transform, real32 *Composite4x3)
{
    triple Orientation[3];
    MatrixEqualsQuaternion3x3((real32 *)Orientation, Transform.Orientation);

    triple *Upper3x3;
    triple MultBuffer[3];
    if(Transform.Flags & HasScaleShear)
    {
        MatrixMultiply3x3((real32 *)MultBuffer,
                          (real32 *)Orientation,
                          (real32 *)Transform.ScaleShear);
        Upper3x3 = MultBuffer;
    }
    else
    {
        Upper3x3 = Orientation;
    }

    Composite4x3[0] = Upper3x3[0][0];
    Composite4x3[1] = Upper3x3[1][0];
    Composite4x3[2] = Upper3x3[2][0];
    Composite4x3 += 3;

    Composite4x3[0] = Upper3x3[0][1];
    Composite4x3[1] = Upper3x3[1][1];
    Composite4x3[2] = Upper3x3[2][1];
    Composite4x3 += 3;

    Composite4x3[0] = Upper3x3[0][2];
    Composite4x3[1] = Upper3x3[1][2];
    Composite4x3[2] = Upper3x3[2][2];
    Composite4x3 += 3;

    Composite4x3[0] = Transform.Position[0];
    Composite4x3[1] = Transform.Position[1];
    Composite4x3[2] = Transform.Position[2];
}
#endif // !PROCESSOR_CELL_SPU


void GRANNY
SetTransform(transform &Result,
             real32 const *Position3,
             real32 const *Orientation4,
             real32 const *ScaleShear3x3)
{
    Result.Flags = HasPosition | HasOrientation | HasScaleShear;
    VectorEquals3(Result.Position, Position3);
    VectorEquals4(Result.Orientation, Orientation4);
    MatrixEquals3x3((real32 *)Result.ScaleShear, ScaleShear3x3);
}

void GRANNY
SetTransformWithIdentityCheck(transform &Result,
                              real32 const *Position3,
                              real32 const *Orientation4,
                              real32 const *ScaleShear3x3)
{
    // TODO: Check and set flags
    SetTransform(Result, Position3, Orientation4, ScaleShear3x3);
    ResetTransformFlags(Result);
}

void GRANNY
PreMultiplyBy(transform &Transform, transform const &PreMult)
{
    // TODO: Optimize this once Multiply() works optimally
    transform Temp;
    Multiply(Temp, PreMult, Transform);
    Transform = Temp;
}

void GRANNY
ResetTransformFlags(transform& Result)
{
    Result.Flags =
        (AreEqual(3, Result.Position, GlobalZeroVector, PositionIdentityThreshold) ? 0 : HasPosition) |
        (AreEqual(4, Result.Orientation, GlobalWAxis, OrientationIdentityThreshold) ? 0 : HasOrientation) |
        (AreEqual(9, (real32 const*)Result.ScaleShear, (real32 const *)GlobalIdentity3x3, ScaleShearIdentityThreshold) ? 0 : HasScaleShear);
}


void GRANNY
BuildInverse(transform &Result, transform const &Source)
{
    triple ResultPosition;
    quad ResultOrientation;
    matrix_3x3 ResultScaleShear;

    // Invert the orientation
    ResultOrientation[0] = -Source.Orientation[0];
    ResultOrientation[1] = -Source.Orientation[1];
    ResultOrientation[2] = -Source.Orientation[2];
    ResultOrientation[3] = Source.Orientation[3];

    triple InverseOrientationMatrix[3];
    MatrixEqualsQuaternion3x3((real32 *)InverseOrientationMatrix,
                              ResultOrientation);

    // Invert the scale/shear (very costly)
    MatrixInvert3x3((real32 *)ResultScaleShear, (real32 *)Source.ScaleShear);
    triple Temp[3];
    MatrixMultiply3x3((real32 *)Temp, (real32 *)ResultScaleShear,
                      (real32 *)InverseOrientationMatrix);
    TransposeMatrixMultiply3x3((real32 *)ResultScaleShear,
                               (real32 *)InverseOrientationMatrix,
                               (real32 *)Temp);

    // Invert the translation
    ResultPosition[0] = -Source.Position[0];
    ResultPosition[1] = -Source.Position[1];
    ResultPosition[2] = -Source.Position[2];
    VectorTransform3(ResultPosition, (real32 *)ResultScaleShear);
    VectorTransform3(ResultPosition, (real32 *)InverseOrientationMatrix);

    SetTransformWithIdentityCheck(Result, ResultPosition, ResultOrientation,
                                  (real32 const *)ResultScaleShear);
}

void GRANNY
Multiply(transform &Result, transform const &A, transform const &B)
{
    Assert(&Result != &A);
    Assert(&Result != &B);

    real32 OrientationA[3][3];
    MatrixEqualsQuaternion3x3((real32 *)OrientationA, A.Orientation);
    real32 OrientationB[3][3];
    MatrixEqualsQuaternion3x3((real32 *)OrientationB, B.Orientation);

    // This does the expanded matrix product of:
    // Transpose(B.Orientation) * A.ScaleShear * B.Orientation * B.ScaleShear;
    real32 Temp[3][3];
    MatrixMultiply3x3((real32 *)Result.ScaleShear, (real32 *)OrientationB,
                      (real32 *)B.ScaleShear);
    MatrixMultiply3x3((real32 *)Temp, (real32 *)A.ScaleShear,
                      (real32 *)Result.ScaleShear);
    TransposeMatrixMultiply3x3((real32 *)Result.ScaleShear, (real32 *)OrientationB,
                               (real32 *)Temp);

    // This does the expanded vector/matrix product of:
    // A.Position + A.Orientation*A.ScaleShear*B.Position
    VectorTransform3(Result.Position, (real32 const *)A.ScaleShear, B.Position);
    VectorTransform3(Result.Position, (real32 *)OrientationA);
    VectorAdd3(Result.Position, A.Position);

    QuaternionMultiply4(Result.Orientation, A.Orientation, B.Orientation);

    Result.Flags = A.Flags | B.Flags;
}

