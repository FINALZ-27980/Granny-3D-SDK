// ========================================================================
// $File: //jeffr/granny_29/rt/granny_lookat.cpp $
// $DateTime: 2012/07/20 17:10:16 $
// $Change: 38457 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_lookat.h"

#include "granny_assert.h"
#include "granny_limits.h"
#include "granny_local_pose.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
#include "granny_math.h"
#include "granny_world_pose.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#undef SubsystemCode
#define SubsystemCode IKLogMessage

USING_GRANNY_NAMESPACE;

static void
ComputeUpdateRotation(int32x const  BoneIndex,
                      real32 const* OrientAxis,
                      real32 const* TargetPosition,
                      world_pose&   WorldPose,
                      real32*       Axis,
                      real32*       Angle)
{
    Assert(Axis && Angle);
    Assert(BoneIndex >= 0 && BoneIndex < GetWorldPoseBoneCount(WorldPose));

    real32* WorldMatrix = GetWorldPose4x4(WorldPose, BoneIndex);

    triple Desired;
    {
        VectorSubtract3(Desired, TargetPosition, &WorldMatrix[12]);
        NormalizeOrZero3(Desired);
    }

    triple MatchAxis;
    {
        VectorEquals3(MatchAxis, OrientAxis);
        VectorTransform4x3(MatchAxis, 0, WorldMatrix);
        NormalizeOrZero3(MatchAxis);
    }

    VectorCrossProduct3(Axis, MatchAxis, Desired);
    *Angle = (real32)IntrinsicATan2(NormalizeOrZero3(Axis),
                                    InnerProduct3(MatchAxis, Desired));

    // Note: this step will not work on models that have scale factors, it depends
    // on: (Inverse = Transpose).
    TransposeVectorTransform4x3(Axis, 0.0f, WorldMatrix);
}


bool GRANNY
IKOrientTowards(int32x          BoneIndex,
                real32 const*   OrientAxis,
                real32 const*   TargetPosition,
                skeleton const& Skeleton,
                local_pose&     LocalPose,
                real32*         Offset4x4,
                world_pose&     WorldPose)
{
    CheckInt32Index(BoneIndex, Skeleton.BoneCount, return false);
    CheckPointerNotNull(OrientAxis, return false);
    CheckPointerNotNull(TargetPosition, return false);
    CheckCondition(GetLocalPoseBoneCount(LocalPose) == Skeleton.BoneCount, return false);
    CheckCondition(GetLocalPoseBoneCount(LocalPose) == GetWorldPoseBoneCount(WorldPose), return false);

    quad UpdateRotation;
    {
        triple Axis;
        real32 Angle;
        ComputeUpdateRotation(BoneIndex,
                              OrientAxis,
                              TargetPosition, WorldPose,
                              Axis, &Angle);
        ConstructQuaternion4(UpdateRotation, Axis, Angle);
    }

    // Apply the difference
    transform* Transform = GetLocalPoseTransform(LocalPose, BoneIndex);
    QuaternionMultiply4(Transform->Orientation, Transform->Orientation, UpdateRotation);
    Transform->Flags |= HasOrientation;
    UpdateWorldPoseChildren(Skeleton, BoneIndex, LocalPose, Offset4x4, WorldPose);

    return true;
}


struct neck_swing_desc
{
    // All of these are in object space
    triple OffsetPt;
    triple AimVec;
    triple LevelVec;

    // As are these
    triple TargetPt;
    triple GroundNormal;

    quad   UpdateRotation;

    int32x  LinksRemaining;
    real32  RemainingLength;

    int32x* BoneIndices;
    real32* BoneLengths;
};

// Handy.
#define MakeKnownPlane(Plane, VecOne, VecTwo)                                           \
    {                                                                                   \
        VectorCrossProduct3(Plane, VecOne, VecTwo);                                     \
        if (NormalizeOrZero3(Plane) == 0.0f)                                            \
        {                                                                               \
            InvalidCodePath("We should not be able to have a degenerate plane here.");  \
            return false;                                                               \
        }                                                                               \
    } typedef int __require_semicolon


static real32
AngleWithQuadrant(real32 const* UnitOne,
                  real32 const* UnitTwo,
                  real32 const* PlaneNormal)
{
    real32 CosAngle = InnerProductUnitClamped3(UnitOne, UnitTwo);
    real32 RawAngle = (real32)IntrinsicACos(CosAngle);

    triple Cross;
    VectorCrossProduct3(Cross, UnitOne, UnitTwo);
    NormalizeOrZero3(Cross);

    if (InnerProduct3(Cross, PlaneNormal) > 0)
        RawAngle = 2*Pi32 - RawAngle;

    return RawAngle;
}

static void
ComputeRotateToPlane(real32 const* Vector,
                     real32 const* Axis,
                     real32 const* PlaneNormal,
                     real32*       ToPlaneRotation)
{
    triple LineVector;
    VectorCrossProduct3(LineVector, Axis, PlaneNormal);

    if (NormalizeOrZero3(LineVector) == 0.0f)
    {
        // If this occurs, then OffsetHat and AimVec are colinear, which means that
        // AimVec is /already/ in the plane we want.  No need to rotate at this
        // point.
        VectorSet4(ToPlaneRotation, 0, 0, 0, 1);
        return;
    }

    real32 AlongAxis   = InnerProductUnitClamped3(Vector, Axis);
    real32 AgainstAxis = SquareRoot(1 - Square(AlongAxis));
    triple PointOnPlane;
    VectorScale3(PointOnPlane, AlongAxis, Axis);

    triple PointTo;
    VectorEquals3(PointTo, PointOnPlane);
    if (InnerProduct3(PointOnPlane, Vector) > 0)
        ScaleVectorAdd3(PointTo, AgainstAxis, LineVector);
    else
        ScaleVectorAdd3(PointTo, -AgainstAxis, LineVector);

    QuaternionBetweenVectors(ToPlaneRotation, Vector, PointTo);
}

// Temporary.  This is designed to level the ear vector without flipping it.
static void
ComputeCloseRotateToPlane(real32 const* Vector,
                          real32 const* Axis,
                          real32 const* PlaneNormal,
                          real32*       ToPlaneRotation)
{
    triple LineVector;
    VectorCrossProduct3(LineVector, Axis, PlaneNormal);

    if (NormalizeOrZero3(LineVector) == 0.0f)
    {
        // If this occurs, then OffsetHat and AimVec are colinear, which means that
        // AimVec is /already/ in the plane we want.  No need to rotate at this
        // point.
        VectorSet4(ToPlaneRotation, 0, 0, 0, 1);
        return;
    }

    real32 AlongAxis   = InnerProductUnitClamped3(Vector, Axis);
    real32 AgainstAxis = SquareRoot(1 - Square(AlongAxis));
    triple PointOnPlane;
    VectorScale3(PointOnPlane, AlongAxis, Axis);
    ScaleVectorAdd3(PointOnPlane, AgainstAxis, LineVector);

    if (InnerProduct3(PointOnPlane, Vector) < 0)
        VectorNegate3(PointOnPlane);

    QuaternionBetweenVectors(ToPlaneRotation, Vector, PointOnPlane);
}

static bool
ComputeSwingTo(neck_swing_desc /*const*/& Swing,
               real32* Axis, real32* Angle)
{
    Assert(Swing.LinksRemaining > 0);
    Assert(Swing.RemainingLength > 0.0f);
    Assert(Swing.BoneIndices);
    Assert(Swing.BoneLengths);
    Assert(Axis);
    Assert(Angle);
    Assert(2 * VectorLength3(Swing.OffsetPt) <= VectorLength3(Swing.TargetPt));
    Assert(AbsoluteValue(VectorLength3(Swing.AimVec) - 1.0f) < 1e-4f);  // we want this to be unit
    Assert(AbsoluteValue(VectorLength3(Swing.LevelVec) - 1.0f) < 1e-4f);   // we want this to be unit
    Assert(AbsoluteValue(InnerProduct3(Swing.AimVec, Swing.LevelVec)) < 1e-4f);

    // In rough outline, the procedure for computing the transform we'll swing by is as
    // follows.
    //
    // Offset:   ObSpace point where the eyes are
    // AimVec:   ObSpace vec
    // LevelVec: ObSpace vec
    // TargetPt: ObSpace location of the target
    // Plane:    ObSpace plane formed by (origin, offset, target)
    //
    // 1. Rotate around OffsetHat until LookHat lies in the plane formed by the origin,
    //    offset, and the target.
    //
    // 2. Rotate around Plane normal, until VHat points at TargetPt
    //
    // 3. Rotate around TargetHat until Dot(LevelVec,GroundNormal) is minimized.  Note that
    //    there are at least 2 solutions to this, choose the version where Dot(Ear, Ear')
    //    is positive.
    //
    // There are obviously a bunch of error cases to handle that we'll get to below.

    quad CompositeRotation = { 0, 0, 0, 1 };

    // Create our Hat (i.e, unit) vectors
    real32 OffsetLength;
    real32 TargetLength;
    triple OffsetHat;
    triple TargetHat;
    {
        VectorEquals3(OffsetHat, Swing.OffsetPt);
        OffsetLength = NormalizeOrZero3(OffsetHat);

        VectorEquals3(TargetHat, Swing.TargetPt);
        TargetLength = NormalizeOrZero3(TargetHat);
        Assert(TargetLength != 0.0f);
    }

    // We'll be modifying these as we go, so make local copies
    triple OffsetPt = { Swing.OffsetPt[0], Swing.OffsetPt[1], Swing.OffsetPt[2] };
    triple AimVec  = { Swing.AimVec[0], Swing.AimVec[1], Swing.AimVec[2] };

    // Check to see if the origin, offset, target plane is degenerate.  This can happen in
    // one of two ways.  TargetPt is never at the origin, but Offset may be.  In addition,
    // abs(dot(OffsetHat, TargetPt)) can indicate that Offset and Target are colinear.
    //
    // note, these aren't really empirically set epsilons...
    triple PlaneNormal;
    real32 const AbsDotOffTarg = AbsoluteValue(InnerProduct3(OffsetHat, TargetHat));
    if (OffsetLength < 1e-5f || AbsDotOffTarg > 0.9999f)
    {
        // Degenerate plane.  Use (Origin, Swing.AimVec, Target) as the plane.  This
        // /also/ yields a possible degeneracy, but it's one that we can deal with.  It
        // occurs when the AimVec is /already/ pointing at the target.  Or possibly
        // directly away from it.

        real32 const AbsDotLookTarg = AbsoluteValue(InnerProduct3(AimVec, TargetHat));
        if (AbsDotLookTarg > 0.9999f)
        {
            // Also degenerate!  Luckily, we know that the Ear vector is non-colinear with
            // the AimVector, so we have an axis that we can rotate around.
            MakeKnownPlane(PlaneNormal, Swing.LevelVec, AimVec);
        }
        else
        {
            // Sensible plane.  Construct the plane normal
            MakeKnownPlane(PlaneNormal, AimVec, TargetHat);
        }

        // Note that both cases, AimVec /already/ lies in the plane, by construction, so
        // there's nothing more to do. at this stage.
    }
    else
    {
        // Ok, in this case, we know that OffsetHat is non-colinear with TargetHat.
        MakeKnownPlane(PlaneNormal, TargetHat, OffsetHat);

        quad ToPlaneRotation;
        ComputeRotateToPlane(AimVec, OffsetHat, PlaneNormal, ToPlaneRotation);

        NormalQuaternionTransform3(AimVec, ToPlaneRotation);
        VectorEquals4(CompositeRotation, ToPlaneRotation);
    }

    // However we got here, the look vec should be lying in the plane
    // under consideration
    real32 PlaneResidual = InnerProduct3(PlaneNormal, AimVec);
    Assert(AbsoluteValue(PlaneResidual) < 1e-3f);

    // Onto step 2.  Since we know that the target point is outside the radius swept in
    // object space by OffsetPt, /and/ that AimVec now resides in the plane we
    // constructed in the last step, which includes both the OffsetPt and the TargetPt, we
    // can use some triangle identities to compute the angle we need to rotate by to point
    // AimVec at the target.
    //
    // The mental picture you should have for our target position is a triangle formed by
    // Offset, Offset + AimVec*t, TargetPos
    //
    // When those 3 elements are in their correct final position, we know 3 things about
    // the triangle,
    //  1. Length of the Offset edge
    //  2. Angle between Offset and AimVec
    //  3. Length of the TargetPt edge.
    //
    // we can then use the law of sines to retrive the angle between offset and targetpt
    // in the final configuration.  The difference between that angle and the current
    // angle is our rotation.  Yikes.
    //
    // todo: There is probably a faster way to do this, given the number of inv trig ops
    // todo: check if offsethat = lookvec
    {
        quad PlaneRotation;
        if (OffsetLength != 0.0f)
        {
            triple NegOffHat; VectorNegate3(NegOffHat, OffsetHat);
            real32 LookOffAngle = AngleWithQuadrant(NegOffHat, AimVec, PlaneNormal);

            real32 SourceAngle = LookOffAngle;
            if (LookOffAngle > Pi32)
                SourceAngle = 2*Pi32 - LookOffAngle;
            Assert(SourceAngle >= 0 && SourceAngle <= Pi32);


            real32 SinSource   = IntrinsicSin(SourceAngle);
            real32 OppAngle    = IntrinsicASin(OffsetLength * SinSource / TargetLength);
            real32 FinalAngle  = Pi32 - (SourceAngle + OppAngle);
            Assert(FinalAngle >= 0);

            real32 CurAngle = AngleWithQuadrant(OffsetHat, TargetHat, PlaneNormal);

            real32 DiffAngle;
            if (LookOffAngle > Pi32)
            {
                DiffAngle = CurAngle - FinalAngle;
            }
            else
            {
                DiffAngle = CurAngle + FinalAngle;
            }
            ConstructQuaternion4(PlaneRotation, PlaneNormal, -DiffAngle);
        }
        else
        {
            // Oh, awesome.  Did I mention this?  If the OffsetPt is /at/ the origin, then we
            // just rotate look vec until it points at the target.  Much easier.
            QuaternionBetweenVectors(PlaneRotation, AimVec, TargetHat);
        }

        // Move the local copies
        NormalQuaternionTransform3(AimVec,  PlaneRotation);
        NormalQuaternionTransform3(OffsetPt, PlaneRotation);

        // Composite the rotation
        QuaternionMultiply4(CompositeRotation, PlaneRotation, CompositeRotation);

#if DEBUG
        // Debug check.
        {
            triple NewOff;
            VectorSubtract3(NewOff, Swing.TargetPt, OffsetPt);
            real32 Length = NormalizeOrZero3(NewOff);
            Assert(Length != 0.0f);

            real32 Residual = InnerProductUnitClamped3(NewOff, AimVec);
            Assert(Residual > 0.99f);
        }
#endif // DEBUG
    }

    // Step 3.  Now we're going to bring the earvec level.  We want to minimize the
    // deflection w.r.t. the ground plane, while making sure we don't invert the head.
    // (I.e, the ear should still point in the same general direction, rather than
    // reversing left to right.)
    {
        // The new ear vec is going to be perpendicular to both the plane normal and
        // the target axis, which means we can just find it by construction.

        // First, let's get a modified copy with the current rotation
        triple LevelVec;
        VectorEquals3(LevelVec, Swing.LevelVec);
        NormalQuaternionTransform3(LevelVec, CompositeRotation);

        quad ToPlaneRotation = { 0, 0, 0, 1 };

        triple NewVec;
        VectorCrossProduct3(NewVec, Swing.GroundNormal, TargetHat);
        if (NormalizeOrZero3(NewVec) == 0.0f)
        {
            // Target hat and ground normal are the same, which means we have
            // no basis with which to level the ear vector.  Take no action
        }
        else
        {
            ComputeCloseRotateToPlane(LevelVec, TargetHat, Swing.GroundNormal, ToPlaneRotation);
        }

        NormalQuaternionTransform3(LevelVec, ToPlaneRotation);
        NormalQuaternionTransform3(OffsetPt, ToPlaneRotation);
        NormalQuaternionTransform3(AimVec,   ToPlaneRotation);

        // Composite the rotation
        QuaternionMultiply4(CompositeRotation, ToPlaneRotation, CompositeRotation);
    }

#if DEBUG
    // Debug check.
    {
        triple NewOff;
        VectorSubtract3(NewOff, Swing.TargetPt, OffsetPt);
        real32 Length = NormalizeOrZero3(NewOff);
        Assert(Length != 0.0f);

        real32 Residual = InnerProductUnitClamped3(NewOff, AimVec);
        Assert(Residual > 0.99f);
    }
#endif // DEBUG


    // Convert back to axis angle
    {
        *Angle = (real32)(IntrinsicACos(CompositeRotation[3]) * 2.0f);
        if (*Angle != 0)
        {
            VectorEquals3(Axis, CompositeRotation);
            NormalizeOrZero3(Axis);
        }
        else
        {
            VectorSet3(Axis, 0, 0, 1);
        }
    }

    return true;
}

static void
PopLink(neck_swing_desc& Swing, local_pose const& LocalPose)
{
    Assert(Swing.BoneIndices);
    Assert(Swing.BoneIndices[0] >= 0 && Swing.BoneIndices[0] < GetLocalPoseBoneCount(LocalPose));

    transform const* ChildTransform = GetLocalPoseTransform(LocalPose, Swing.BoneIndices[0]);
    Assert(ChildTransform);

    // These are global coordinates, so, we have:
    // T  = parent
    // c  = child
    // w  = point in world space
    // p  = point in child space
    // p' = point in parent space
    //
    // 1. Tcp = w
    // 2. Tp' = w
    // 3. Tp' = Tcp
    // 4. p'  = cp
    //
    // Note that for normals, I'm fudging a bit, but if you work the inv. transpose, it comes out
    // the same.
    TransformPointInPlace (Swing.TargetPt,     *ChildTransform);
    TransformVectorInPlace(Swing.GroundNormal, *ChildTransform);

    // The local coordinates get modified by the update rotation, if it exists.
    transform* Transform = GetLocalPoseTransform(LocalPose, Swing.BoneIndices[0]);
    QuaternionMultiply4(Transform->Orientation, Transform->Orientation, Swing.UpdateRotation);
    Transform->Flags |= HasOrientation;
    VectorSet4(Swing.UpdateRotation, 0, 0, 0, 1);

    // These are points/vectors in the local space of the bone.  To move them to parent space,
    // multiply by the inverse of the child transform.
    transform Inverse;
    BuildInverse(Inverse, *ChildTransform);
    TransformPointInPlace (Swing.OffsetPt, *ChildTransform);
    TransformVectorInPlace(Swing.AimVec,  *ChildTransform);
    TransformVectorInPlace(Swing.LevelVec,   *ChildTransform);

    Swing.RemainingLength -= Swing.BoneLengths[0];
    --Swing.LinksRemaining;
    ++Swing.BoneIndices;
    ++Swing.BoneLengths;
}

static bool
IKLookAtDriver(neck_swing_desc& Swing,
               skeleton const&  Skeleton,
               local_pose&      LocalPose,
               real32 const*    Offset4x4,
               world_pose&      WorldPose)
{
    if (Swing.LinksRemaining == 0)
        return true;

    Assert(Swing.LinksRemaining > 0);
    Assert(Swing.BoneIndices);
    Assert(Swing.BoneLengths);
    Assert(Swing.RemainingLength >= 0.0f);
    Assert(GetLocalPoseBoneCount(LocalPose) == Skeleton.BoneCount);
    Assert(GetLocalPoseBoneCount(LocalPose) == GetWorldPoseBoneCount(WorldPose));

    // Compute modification
    triple Axis;
    real32 Angle;
    if (!ComputeSwingTo(Swing, Axis, &Angle))
        return false;

    // Apply modification * AmountApplied
    real32 const AmountApplied = Swing.BoneLengths[0] / Swing.RemainingLength;
    if (AmountApplied < 1.0f && Angle > Pi32)
    {
        // Make sure we go the short way around if we're lerping...
        Angle = 2*Pi32 - Angle;
        Axis[0] *= -1;
        Axis[1] *= -1;
        Axis[2] *= -1;
    }

    Angle *= AmountApplied;


    // Joint limits?  Simple or quaternion?
#if 0
    if (AbsoluteValue(Angle) > Pi32/2.0f)
        Angle = Pi32/2.0f * Sign(Angle);
#endif

    // Store the difference transform in the structure (applied in PopLink, see comment above)
    ConstructQuaternion4(Swing.UpdateRotation, Axis, Angle);

    PopLink(Swing, LocalPose);
    return IKLookAtDriver(Swing, Skeleton, LocalPose, Offset4x4, WorldPose);
}

bool GRANNY
IKAimAt(int32x          HeadBoneIndex,
        int32x          LinkCountInit,
        int32x          InactiveLinkCountInit,
        real32 const*   OSOffset3,
        real32 const*   OSAimVec3,
        real32 const*   OSLevelVec3,
        real32 const*   WSGroundNormal,
        real32 const*   TargetPositionInit,
        skeleton const& Skeleton,
        local_pose&     LocalPose,
        real32*         Offset4x4,
        world_pose&     WorldPose)
{
    int32x InactiveLinkCount = InactiveLinkCountInit;
    int32x LinkCount         = LinkCountInit;

    // Sanity checks.  Make /really/ sure.
    CheckInt32Index(HeadBoneIndex, Skeleton.BoneCount, return false);
    CheckGreater0(LinkCount, return false);
    CheckGreaterEqual0(InactiveLinkCount, return false);
    CheckCondition(InactiveLinkCount < LinkCount, return false);
    CheckCondition(LinkCount < HeadBoneIndex, return false);
    CheckCondition(LinkCount <= MaximumIKLinkCount, return false);
    CheckPointerNotNull(OSOffset3,  return false);
    CheckPointerNotNull(OSAimVec3, return false);
    CheckPointerNotNull(OSLevelVec3,  return false);
    CheckPointerNotNull(WSGroundNormal, return false);
    CheckPointerNotNull(TargetPositionInit, return false);
    CheckCondition(GetLocalPoseBoneCount(LocalPose) == Skeleton.BoneCount, return false);
    CheckCondition(GetLocalPoseBoneCount(LocalPose) == GetWorldPoseBoneCount(WorldPose), return false);

    triple TargetPosition;
    VectorEquals3(TargetPosition, TargetPositionInit);

    // Clean bill of health!  Let's set this up for the internal driver.
    real32 ChainLength = 0.0f;
    int32x BoneIndices[MaximumIKLinkCount];
    real32 BoneLengths[MaximumIKLinkCount];
    {
        {for (int32x Idx = 0, Walk = HeadBoneIndex; Idx < LinkCount; ++Idx)
        {
            if (Walk == NoParentBone)
            {
                InvalidCodePath("Causality violation");
                return false;
            }

            BoneIndices[Idx] = Walk;
            BoneLengths[Idx] = VectorLength3(Skeleton.Bones[BoneIndices[Idx]].LocalTransform.Position);
            ChainLength += BoneLengths[Idx];

            Walk = Skeleton.Bones[Walk].ParentIndex;
        }}
    }

    // We'll use this in a couple of places...
    real32 const* BoneWorld4x4 = GetWorldPose4x4(WorldPose, HeadBoneIndex);
    Assert(BoneWorld4x4);

    // Compute the distance to the target position from the end effector.  We need to move
    // this out until it is at least 3*ChainLength away from the EE.
    float DistanceToTarget =
        SquareRoot(SquaredDistanceBetween3(&BoneWorld4x4[12], TargetPosition));
    if (DistanceToTarget < 1e-5f)
    {
        Log1(ErrorLogMessage, IKLogMessage,
             "Target is too close for stable computation: %f\n", DistanceToTarget);
        return false;
    }

    // Deal with zero length chains
    if (ChainLength == 0.0f)
    {
        // Just pretend that each link is equal length.  Since we'll be adjusting the
        // target position based on the total length, just use that as a base
        ChainLength = DistanceToTarget / 4;
        {for (int32x Idx = 0; Idx < LinkCount; ++Idx)
        {
            BoneLengths[Idx] = DistanceToTarget / 4.0f;
        }}
    }
    else if (3 * ChainLength > DistanceToTarget)
    {
        // Push the target out from the eye until it's at least this far away
        triple tvec;
        VectorSubtract3(tvec, TargetPosition, &BoneWorld4x4[12]);
        ScaleVectorAdd3(TargetPosition, (3 * ChainLength)/DistanceToTarget, tvec);
    }

    // Start filling the description out.
    neck_swing_desc Swing;
    {
        // Offset, look and ear vecs are in local space
        VectorEquals3(Swing.OffsetPt, OSOffset3);
        VectorEquals3(Swing.AimVec,   OSAimVec3);
        VectorEquals3(Swing.LevelVec, OSLevelVec3);

        // Obtain the inverse of the chain end's WS transform

        matrix_4x4 InvBoneWorld4x4;
        if (!MatrixInvert4x3(&InvBoneWorld4x4[0][0], BoneWorld4x4))
        {
            Log1(ErrorLogMessage, IKLogMessage,
                 "Unable to invert world matrix of the end bone (%d) in IKLookAt\n",
                 HeadBoneIndex);
            return false;
        }

        // Move the targets into object space
        VectorTransform4x3(Swing.TargetPt,     TargetPosition, 1.0f, &InvBoneWorld4x4[0][0]);
        VectorTransform4x3(Swing.GroundNormal, WSGroundNormal, 0.0f, &InvBoneWorld4x4[0][0]);

        Swing.LinksRemaining  = LinkCount;
        Swing.RemainingLength = ChainLength;
        Swing.BoneIndices     = BoneIndices;
        Swing.BoneLengths     = BoneLengths;
        VectorSet4(Swing.UpdateRotation, 0, 0, 0, 1);
    }

    // Handle inactive links (not the most efficient way to do this, but it keeps things
    // simple).
    while (InactiveLinkCount--)
    {
        Assert(InactiveLinkCount >= 0);
        PopLink(Swing, LocalPose);
    }

    // Start the driver.  We don't pass in the inactive links,
    if (IKLookAtDriver(Swing, Skeleton, LocalPose, Offset4x4, WorldPose))
    {
        // Fix up the world pose
        UpdateWorldPoseChildren(Skeleton, BoneIndices[LinkCount-1], LocalPose, Offset4x4, WorldPose);
        return true;
    }

    return false;
}


