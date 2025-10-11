// ========================================================================
// $File: //jeffr/granny_29/rt/granny_ik.cpp $
// $DateTime: 2012/04/11 15:54:26 $
// $Change: 37011 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_ik.h"

#include "granny_bone_operations.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_model_instance.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
#include "granny_world_pose.h"

// This should always be the last header included
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

// Slow but sure
static void
ComputeUpdateRotation(real32 const* VectorToActual,
                      real32 const* VectorToDesired,
                      real32* RotationIncrement)
{
    triple Desired;
    {
        VectorEquals3(Desired, VectorToDesired);
        NormalizeOrZero3(Desired);
    }

    triple MatchAxis;
    {
        VectorEquals3(MatchAxis, VectorToActual);
        NormalizeOrZero3(MatchAxis);
    }

    triple Axis;
    VectorCrossProduct3(Axis, MatchAxis, Desired);

    real32 Angle = (real32)IntrinsicATan2(NormalizeOrZero3(Axis),
                                          InnerProduct3(MatchAxis, Desired));

    ConstructQuaternion4(RotationIncrement, Axis, Angle);
}


void GRANNY
IKUpdate(int32x LinkCountInit, int32x EEBoneIndex,
         real32 const *DesiredPosition3,
         int32x IterationCount,
         skeleton const &Skeleton,
         real32 const* Offset4x4,
         local_pose &LocalPose,
         world_pose &WorldPose)
{
    Assert ( LinkCountInit <= MaximumIKLinkCount );
    int32x Links[MaximumIKLinkCount] = { 0 };
    int32x LinkCount = 0;
    {
        int32x BoneIndex = EEBoneIndex;
        {for(int32x LinkIndex = 0;
             LinkIndex <= LinkCountInit;
             ++LinkIndex)
        {
            Links[LinkIndex] = BoneIndex;
            BoneIndex = Skeleton.Bones[BoneIndex].ParentIndex;
            ++LinkCount;
            if(BoneIndex == NoParentBone)
            {
                break;
            }
        }}
    }

    real32 StartPosition[3];
    VectorEquals3(StartPosition, &GetWorldPose4x4(WorldPose, Links[0])[12]);


    {for(int32x CCDIteration = 0;
         CCDIteration < IterationCount;
         ++CCDIteration)
    {
        real32 IterationPosition[3];
        VectorSubtract3(IterationPosition, DesiredPosition3, StartPosition);
        VectorScale3(IterationPosition, real32(CCDIteration+1)/real32(IterationCount));
        VectorAdd3(IterationPosition, StartPosition);

        {for(int32x LinkIndex = 1;
             LinkIndex < LinkCount;
             ++LinkIndex)
        {
            real32 CurrentPosition[3];
            VectorEquals3(CurrentPosition, &GetWorldPose4x4(WorldPose, Links[0])[12]);

            int32x BoneIndex = Links[LinkIndex];
            transform *Relative = GetLocalPoseTransform(LocalPose, BoneIndex);
            real32 const *Absolute = GetWorldPose4x4(WorldPose, BoneIndex);

            triple VectorToDesired;
            VectorSubtract3(VectorToDesired, IterationPosition, &Absolute[12]);
            TransposeVectorTransform4x3(VectorToDesired, 0.0f, Absolute);

            triple VectorToActual;
            VectorSubtract3(VectorToActual, CurrentPosition, &Absolute[12]);
            TransposeVectorTransform4x3(VectorToActual, 0.0f, Absolute);

            quad LocalRotationIncrement;
            ComputeUpdateRotation(VectorToActual, VectorToDesired, LocalRotationIncrement);

            Relative->Flags |= HasOrientation;
            QuaternionMultiply4(Relative->Orientation,
                                Relative->Orientation,
                                LocalRotationIncrement);
            UpdateWorldPoseChildren(Skeleton, BoneIndex, LocalPose, Offset4x4, WorldPose);
        }}
    }}
}



bool GRANNY
IKUpdate2BoneDetailed(int32x EEBoneIndex,
                      int32x KneeBoneIndex,
                      int32x HipBoneIndex,
                      real32 const *DesiredPosition3,
                      real32 const *RestrictedMovementPlaneNormal3,
                      skeleton const &Skeleton,
                      real32 const* Offset4x4Initial,
                      local_pose &LocalPose,
                      world_pose &WorldPose,
                      real32 HyperExtensionStart,
                      real32 HyperExtensionScale)
{
    CheckInt32Index(EEBoneIndex,   Skeleton.BoneCount, return false);
    CheckInt32Index(KneeBoneIndex, Skeleton.BoneCount, return false);
    CheckInt32Index(HipBoneIndex,  Skeleton.BoneCount, return false);
    CheckCondition(EEBoneIndex   > KneeBoneIndex, return false);
    CheckCondition(KneeBoneIndex > HipBoneIndex,  return false);

    bool Success = true;
#if DEBUG
    // Make sure that we can get from EE to Knee by ParentIndex
    {
        int32x WalkIndex = Skeleton.Bones[EEBoneIndex].ParentIndex;
        while (WalkIndex != NoParentBone)
        {
            if (WalkIndex == KneeBoneIndex)
                break;
            WalkIndex = Skeleton.Bones[WalkIndex].ParentIndex;
        }
        if (WalkIndex != KneeBoneIndex)
            return false;
    }

    // Make sure that we can get from Knee to Hip by ParentIndex
    {
        int32x WalkIndex = Skeleton.Bones[KneeBoneIndex].ParentIndex;
        while (WalkIndex != NoParentBone)
        {
            if (WalkIndex == HipBoneIndex)
                break;
            WalkIndex = Skeleton.Bones[WalkIndex].ParentIndex;
        }
        if (WalkIndex != HipBoneIndex)
            return false;
    }
#endif // DEBUG


    // "WS" = WorldSpace - these are NOT in local space!
    // This doesn't move.
    real32 *L1WS = &(GetWorldPose4x4(WorldPose, HipBoneIndex)[12]);
    // These two will, but I need to know distances first.
    real32 *L2WS = &(GetWorldPose4x4(WorldPose, KneeBoneIndex)[12]);
    real32 *EEWS = &(GetWorldPose4x4(WorldPose, EEBoneIndex)[12]);

    // Vector from L1 to desired EE
    triple EEL1WS;
    VectorSubtract3 ( EEL1WS, DesiredPosition3, L1WS );
    real32 EEL1LengthSq = VectorLengthSquared3 ( EEL1WS );

    // "Length" of the two bones to rotate.
    // (note that this needs to be taken from the WS stuff because
    // we need to cope as best we can with scales and shears
    // (though I suspect shears and non-uniform scales will not be dealt with very elegantly).
    real32 L1L2LengthSq = SquaredDistanceBetween3 ( L1WS, L2WS );
    real32 L2EELengthSq = SquaredDistanceBetween3 ( L2WS, EEWS );

    // So what we need to find is the new L2 origin. L1 is fixed, and
    // the new EE is defined by the caller.

    // First find the point P along the line L1->EE that is nearest to the new L2
    // We know that:
    // P = L1 + lambda*(EE-L1)
    // and because angles L2-P-L1 and EE-P-L1 are right angles:
    // |P-L2|^2 + |P-L1|^2 = |L1-L2|^2
    // |P-L2|^2 + |P-EE|^2 = |EE-L2|^2
    // So we can solve for all those and find that:
    // lambda = 0.5 + ( ( |L1-L2|^2 + |EE-L2|^2 ) / ( 2 * |EE-L1|^2 ) )
    real32 Lambda = 0.0f;
#define SomeSmallTolerance 1e-8f
    if ( AbsoluteValue ( EEL1LengthSq ) < SomeSmallTolerance )
    {
        // They're basically coincident - you're doomed. A lambda of zero
        // is as good a value as any.
        Success = false;
    }
    else
    {
        Lambda = ( ( L1L2LengthSq - L2EELengthSq ) / EEL1LengthSq ) * 0.5f + 0.5f;
    }
    // Note that lambda can happily be <0.0f or >1.0f (though not _too_ far - we'll catch this case later)

    // So we know where P is: P = L1 + lambda*(EE-L1)
    triple PWS;
    ScaleVectorAdd3 ( PWS, L1WS, Lambda, EEL1WS );

    // And now we need to know the vector P->L2.
    // We know it's perpendicular to both EEL1 and RestrictedMovementPlaneNormal3,
    // and we know its length is sqrt(|L1-L2|^2 - |P-L1|^2)
    // (and |P-L1| == |EE-L1|*lambda)
    // So we just take the cross product of those two vectors, and normalise.
    // Although there's two possible vectors at right-angles to the two,
    // one the negative of the other, one of the purposes of RestrictedMovementPlaneNormal3
    // is to indicate which direction is the preferred one, so we always take the cross
    // product the same way.
    triple PL2WS;
    VectorCrossProduct3 ( PL2WS, EEL1WS, RestrictedMovementPlaneNormal3 );
    real32 TempLengthSq = VectorLengthSquared3 ( PL2WS );
    real32 DesiredLengthSq = L1L2LengthSq - ( Lambda * Lambda * EEL1LengthSq );

    // As DesiredLengthSq approaches zero, the joint is straightening.
    // This can happen very fast for very little end-effector movement,
    // and leads to "knee pop". This is a fairly good attempt at softening the
    // final motion, and stops the knee or elbow smacking into its stops quite as violently.
    // Good numbers are:
    // HyperExtensionStart = 0.1f;
    // HyperExtensionScale = 0.5f;
    if ( DesiredLengthSq < HyperExtensionStart * L1L2LengthSq )
    {
        // We're almost trying to hyper-extend, so make the transition into locking the
        // joint a lot gentler to avoid "knee pop".
        float Delta = DesiredLengthSq - HyperExtensionStart * L1L2LengthSq;
        Delta *= HyperExtensionScale;
        DesiredLengthSq = Delta + HyperExtensionStart * L1L2LengthSq;
        if ( DesiredLengthSq < 0.0f )
        {
            DesiredLengthSq = 0.0f;
        }
    }

    // Scale PL2WS and add to PWS to get the new L2.
    triple NewL2WS;
    ScaleVectorAdd3 ( NewL2WS, PWS, SquareRoot ( DesiredLengthSq / TempLengthSq ), PL2WS );


    // So, those are all in world-space. Now we need to rotate the
    // bone hierarchy to make those values happen.
    // This code is uncannily similar to the general case above,
    // since it basically does the same thing, except we don't need
    // to iterate now.

    // First, move the L1 bone so that L2's origin is in the correct place.
    {
        int32x BoneIndex = HipBoneIndex;
        const real32 *DesiredPosition = NewL2WS;
        const real32 *CurrentPosition = L2WS;


        transform *Relative = GetLocalPoseTransform(LocalPose, BoneIndex);
        real32 const *Absolute = GetWorldPose4x4(WorldPose, BoneIndex);

        triple VectorToDesired;
        VectorSubtract3(VectorToDesired, DesiredPosition, &Absolute[12]);
        TransposeVectorTransform4x3(VectorToDesired, 0.0f, Absolute);

        triple VectorToActual;
        VectorSubtract3(VectorToActual, CurrentPosition, &Absolute[12]);
        TransposeVectorTransform4x3(VectorToActual, 0.0f, Absolute);

        VectorAdd3(VectorToActual, VectorToDesired);
        VectorScale3(VectorToActual, 0.5f);

        quad LocalRotationIncrement;
        VectorCrossProduct3(LocalRotationIncrement,
                            VectorToActual, VectorToDesired);

        LocalRotationIncrement[3] = InnerProduct3(
            VectorToActual, VectorToDesired);

        Normalize4(LocalRotationIncrement);

        Relative->Flags |= HasOrientation;
        QuaternionMultiply4(Relative->Orientation,
                            Relative->Orientation,
                            LocalRotationIncrement);
    }

    // Re-do the world-space matrices for L1 and L2.
    // We could just call BuildWorldPose, but we just want these two bones.
    int32x L1ParentBoneIndex = Skeleton.Bones[HipBoneIndex].ParentIndex;

    ALIGN16_STACK(real32, Offset4x4, 16);
    real32 *ParentWorld;
    if ( L1ParentBoneIndex == NoParentBone )
    {
        MatrixEquals4x4(Offset4x4, Offset4x4Initial);
        ParentWorld = Offset4x4;
    }
    else
    {
        Assert ( L1ParentBoneIndex >= 0 );
        Assert ( L1ParentBoneIndex < WorldPose.BoneCount );
        ParentWorld = GetWorldPose4x4(WorldPose, L1ParentBoneIndex);
    }

        BuildFullWorldPoseOnly(*GetLocalPoseTransform ( LocalPose, HipBoneIndex ),
                               ParentWorld,
                               GetWorldPose4x4 ( WorldPose, HipBoneIndex ) );

        BuildFullWorldPoseOnly(*GetLocalPoseTransform ( LocalPose, KneeBoneIndex ),
                               GetWorldPose4x4 ( WorldPose, HipBoneIndex ),
                               GetWorldPose4x4 ( WorldPose, KneeBoneIndex ) );

        BuildFullWorldPoseOnly(*GetLocalPoseTransform ( LocalPose, EEBoneIndex ),
                               GetWorldPose4x4 ( WorldPose, KneeBoneIndex ),
                               GetWorldPose4x4 ( WorldPose, EEBoneIndex ) );
    if (GetWorldPoseComposite4x4Array(WorldPose))
    {
        BuildSingleCompositeFromWorldPose((real32*)Skeleton.Bones[HipBoneIndex].InverseWorld4x4,
                                          GetWorldPose4x4 ( WorldPose, HipBoneIndex ),
                                          GetWorldPoseComposite4x4 ( WorldPose, HipBoneIndex ));
        BuildSingleCompositeFromWorldPose((real32*)Skeleton.Bones[KneeBoneIndex].InverseWorld4x4,
                                          GetWorldPose4x4 ( WorldPose, KneeBoneIndex ),
                                          GetWorldPoseComposite4x4 ( WorldPose, KneeBoneIndex ));
        BuildSingleCompositeFromWorldPose((real32*)Skeleton.Bones[EEBoneIndex].InverseWorld4x4,
                                          GetWorldPose4x4 ( WorldPose, EEBoneIndex ),
                                          GetWorldPoseComposite4x4 ( WorldPose, EEBoneIndex ));
    }

#if 0
#if DEBUG
    if ( CheckStuff )
    {
        real32 OrigWorldL1[16];
        real32 OrigWorldL2[16];
        real32 OrigWorldEE[16];
        MatrixEquals4x4 ( OrigWorldL1, GetWorldPose4x4 ( WorldPose, HipBoneIndex ) );
        MatrixEquals4x4 ( OrigWorldL2, GetWorldPose4x4 ( WorldPose, KneeBoneIndex ) );
        MatrixEquals4x4 ( OrigWorldEE, GetWorldPose4x4 ( WorldPose, EEBoneIndex ) );

        real32 OrigCompositeL1[16];
        real32 OrigCompositeL2[16];
        real32 OrigCompositeEE[16];
        MatrixEquals4x4 ( OrigCompositeL1, GetWorldPoseComposite4x4 ( WorldPose, HipBoneIndex ) );
        MatrixEquals4x4 ( OrigCompositeL2, GetWorldPoseComposite4x4 ( WorldPose, KneeBoneIndex ) );
        MatrixEquals4x4 ( OrigCompositeEE, GetWorldPoseComposite4x4 ( WorldPose, EEBoneIndex ) );

        BuildWorldPose(
            Skeleton, 0, Skeleton.BoneCount, LocalPose,
            ModelRootTransform,
            WorldPose);

        real32 *NewWorldL1 = GetWorldPose4x4 ( WorldPose, HipBoneIndex );
        real32 *NewWorldL2 = GetWorldPose4x4 ( WorldPose, KneeBoneIndex );
        real32 *NewWorldEE = GetWorldPose4x4 ( WorldPose, KneeBoneIndex );
        real32 *NewCompositeL1 = GetWorldPoseComposite4x4 ( WorldPose, HipBoneIndex );
        real32 *NewCompositeL2 = GetWorldPoseComposite4x4 ( WorldPose, KneeBoneIndex );
        real32 *NewCompositeEE = GetWorldPoseComposite4x4 ( WorldPose, KneeBoneIndex );
        for ( int i = 0; i < 16; i++ )
        {
            Assert ( AbsoluteValue ( OrigWorldL1[i] - NewWorldL1[i] ) < 0.001f );
            Assert ( AbsoluteValue ( OrigWorldL2[i] - NewWorldL2[i] ) < 0.001f );
            Assert ( AbsoluteValue ( OrigWorldEE[i] - NewWorldEE[i] ) < 0.001f );
            Assert ( AbsoluteValue ( OrigCompositeL1[i] - NewCompositeL1[i] ) < 0.001f );
            Assert ( AbsoluteValue ( OrigCompositeL2[i] - NewCompositeL2[i] ) < 0.001f );
            Assert ( AbsoluteValue ( OrigCompositeEE[i] - NewCompositeEE[i] ) < 0.001f );
        }
    }
#endif
#endif

    // Now, move the L2 bone so that EE's origin is in the correct place.
    {
        int32x BoneIndex = KneeBoneIndex;
        const real32 *DesiredPosition = DesiredPosition3;
        const real32 *CurrentPosition = EEWS;

        transform *Relative = GetLocalPoseTransform(LocalPose, BoneIndex);
        real32 const *Absolute = GetWorldPose4x4(WorldPose, BoneIndex);

        triple VectorToDesired;
        VectorSubtract3(VectorToDesired, DesiredPosition, &Absolute[12]);
        TransposeVectorTransform4x3(VectorToDesired, 0.0f, Absolute);

        triple VectorToActual;
        VectorSubtract3(VectorToActual, CurrentPosition, &Absolute[12]);
        TransposeVectorTransform4x3(VectorToActual, 0.0f, Absolute);

        VectorAdd3(VectorToActual, VectorToDesired);
        VectorScale3(VectorToActual, 0.5f);

        quad LocalRotationIncrement;
        VectorCrossProduct3(LocalRotationIncrement,
                            VectorToActual, VectorToDesired);

        LocalRotationIncrement[3] = InnerProduct3(
            VectorToActual, VectorToDesired);

        Normalize4(LocalRotationIncrement);

        Relative->Flags |= HasOrientation;
        QuaternionMultiply4(Relative->Orientation,
                            Relative->Orientation,
                            LocalRotationIncrement);
    }

    // Update from HipBoneIndex
    UpdateWorldPoseChildren(Skeleton, HipBoneIndex,
                            LocalPose,
                            Offset4x4,
                            WorldPose);

    return Success;
}




bool GRANNY
IKUpdate2Bone(int32x EEBoneIndex,
              real32 const *DesiredPosition3,
              real32 const *RestrictedMovementPlaneNormal3,
              skeleton const &Skeleton,
              real32 const* Offset4x4Initial,
              local_pose &LocalPose,
              world_pose &WorldPose,
              real32 HyperExtensionStart,
              real32 HyperExtensionScale)
{
    // The bone hierarchy goes L1->L2->EE
    // L = "link", EE = end effector.
    int32x L2BoneIndex = Skeleton.Bones[EEBoneIndex].ParentIndex;
    CheckInt32Index(L2BoneIndex, Skeleton.BoneCount, return false);
    int32x L1BoneIndex = Skeleton.Bones[L2BoneIndex].ParentIndex;
    CheckInt32Index(L1BoneIndex, Skeleton.BoneCount, return false);

    return IKUpdate2BoneDetailed(EEBoneIndex, L2BoneIndex, L1BoneIndex,
                                 DesiredPosition3,
                                 RestrictedMovementPlaneNormal3,
                                 Skeleton,
                                 Offset4x4Initial,
                                 LocalPose,
                                 WorldPose,
                                 HyperExtensionStart,
                                 HyperExtensionScale);
}






