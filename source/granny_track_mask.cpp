// ========================================================================
// $File: //jeffr/granny_29/rt/granny_track_mask.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_track_mask.h"

#include "granny_aggr_alloc.h"
#include "granny_data_type_conversion.h"
#include "granny_member_iterator.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_model.h"
#include "granny_parameter_checking.h"
#include "granny_skeleton.h"
#include "granny_string.h"
#include "granny_track_group.h"

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
#define SubsystemCode TrackMaskLogMessage

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

data_type_definition TrackMaskType[] =
{
    { Real32Member, "DefaultWeight" },
    { ReferenceToArrayMember, "BoneWeights", Real32Type },
    { EndMember }
};


data_type_definition UnboundWeightType[] =
{
    { StringMember, "Name" },
    { Real32Member, "Weight" },
    { EndMember }
};

data_type_definition UnboundTrackMaskType[] =
{
    { Real32Member, "DefaultWeight" },
    { ReferenceToArrayMember, "Weights", UnboundWeightType },
    { EndMember }
};



track_mask IdentityTrackMask = { 1.0f, 0, NULL };
track_mask NullTrackMask     = { 0.0f, 0, NULL };
CompileAssert(SizeOf(track_mask) == (SizeOf(real32) +
                                     SizeOf(int32x) +
                                     SizeOf(real32*)));

END_GRANNY_NAMESPACE;

static void
AggrTrackMask(aggr_allocator& Allocator,
              track_mask*& TrackMask,
              int32x BoneCount)
{
    Assert(BoneCount > 0);

    AggrAllocPtr(Allocator, TrackMask);
    AggrAllocOffsetArrayPtr(Allocator, TrackMask, BoneCount, BoneWeightCount, BoneWeights);
}

track_mask* GRANNY
NewTrackMask(real32 DefaultWeight, int32x BoneCount)
{
    CheckCondition(BoneCount > 0, return 0);

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    track_mask* TrackMask = 0;
    AggrTrackMask(Allocator, TrackMask, BoneCount);
    if(EndAggrAlloc(Allocator, AllocationInstance))
    {
        TrackMask->DefaultWeight = DefaultWeight;

        {for(int32x BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
        {
            TrackMask->BoneWeights[BoneIndex] = TrackMask->DefaultWeight;
        }}
    }

    return TrackMask;
}

track_mask* GRANNY
NewTrackMaskInPlace(real32 DefaultWeight,
                    int32x BoneCount,
                    void*  Memory)
{
    CheckPointerNotNull(Memory, return 0);
    CheckCondition(BoneCount > 0, return 0);

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    track_mask* TrackMask = 0;
    AggrTrackMask(Allocator, TrackMask, BoneCount);

    if (EndAggrPlacement(Allocator, Memory))
    {
        TrackMask->DefaultWeight = DefaultWeight;

        {for(int32x BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
        {
            TrackMask->BoneWeights[BoneIndex] = TrackMask->DefaultWeight;
        }}
    }

    return TrackMask;
}


int32x GRANNY
GetTrackMaskSize(int32x BoneCount)
{
    CheckCondition(BoneCount > 0, return 0);

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    track_mask* TrackMask;
    AggrTrackMask(Allocator, TrackMask, BoneCount);

    return (int32x)EndAggrSize(Allocator);
}

real32 GRANNY
GetTrackMaskBoneWeight(track_mask const &Mask, int32x BoneIndex)
{
    return((BoneIndex < Mask.BoneWeightCount) ?
           Mask.BoneWeights[BoneIndex] :
           Mask.DefaultWeight);
}

void GRANNY
SetTrackMaskBoneWeight(track_mask &Mask, int32x BoneIndex, real32 Weight)
{
    if(BoneIndex < Mask.BoneWeightCount)
    {
        Mask.BoneWeights[BoneIndex] = Weight;
    }
    else
    {
        Log2(ErrorLogMessage, TrackMaskLogMessage,
             "BoneIndex %d is out-of-bounds "
             "(only %d bones in this track mask)",
             BoneIndex, Mask.BoneWeightCount);
    }
}

void GRANNY
FreeTrackMask(track_mask *Mask)
{
    Deallocate(Mask);
}

bool GRANNY
CopyTrackMask(track_mask& Dest, track_mask const &Source)
{
    if (Dest.BoneWeightCount != Source.BoneWeightCount)
        return false;

    Dest.DefaultWeight = Source.DefaultWeight;
    CopyArray(Source.BoneWeightCount, Source.BoneWeights, Dest.BoneWeights);

    return true;
}

track_mask *GRANNY
CloneTrackMask(track_mask const &Mask)
{
    track_mask *MaskCopy = NewTrackMask(Mask.DefaultWeight, Mask.BoneWeightCount);
    if(MaskCopy)
    {
        Copy32(Mask.BoneWeightCount, Mask.BoneWeights, MaskCopy->BoneWeights);
    }
    else
    {
        Log0(ErrorLogMessage, TrackMaskLogMessage,
             "Unable to create new track mask for copying");
    }

    return (MaskCopy);
}

void GRANNY
InvertTrackMask(track_mask &Mask)
{
    {for(int32x BoneIndex = 0;
         BoneIndex < Mask.BoneWeightCount;
         ++BoneIndex)
    {
        real32 &Weight = Mask.BoneWeights[BoneIndex];
        Weight = 1.0f - Weight;
    }}
}

void GRANNY
SetSkeletonTrackMaskFromTrackGroup(track_mask &Mask,
                                   skeleton const &Skeleton,
                                   track_group const &TrackGroup,
                                   real32 IdentityValue,
                                   real32 ConstantValue,
                                   real32 AnimatedValue)
{
    {for(int32x TrackIndex = 0;
         TrackIndex < TrackGroup.TransformTrackCount;
         ++TrackIndex)
    {
        transform_track &Track = TrackGroup.TransformTracks[TrackIndex];
        int32x BoneIndex;
        if(FindBoneByName(&Skeleton, Track.Name, BoneIndex))
        {
            if(TransformTrackIsAnimated(Track))
            {
                SetTrackMaskBoneWeight(Mask, BoneIndex, AnimatedValue);
            }
            else if(TransformTrackIsIdentity(Track))
            {
                SetTrackMaskBoneWeight(Mask, BoneIndex, IdentityValue);
            }
            else
            {
                SetTrackMaskBoneWeight(Mask, BoneIndex, ConstantValue);
            }
        }
    }}
}

void GRANNY
SetSkeletonTrackMaskChainUpwards(track_mask &Mask,
                                 skeleton const &Skeleton,
                                 int32x ChainLeafBoneIndex,
                                 real32 Weight)
{
    while(ChainLeafBoneIndex != NoParentBone)
    {
        SetTrackMaskBoneWeight(Mask, ChainLeafBoneIndex, Weight);
        ChainLeafBoneIndex = Skeleton.Bones[ChainLeafBoneIndex].ParentIndex;
    }
}

void GRANNY
SetSkeletonTrackMaskChainDownwards(track_mask &Mask,
                                   skeleton const &Skeleton,
                                   int32x ChainRootBoneIndex,
                                   real32 Weight)
{
    real32 KeyWeight = -12345.0f;

    SetTrackMaskBoneWeight(Mask, ChainRootBoneIndex, KeyWeight);
    {for(int32x Index = ChainRootBoneIndex + 1;
         Index < Skeleton.BoneCount;
         ++Index)
    {
        int32x ParentIndex = Skeleton.Bones[Index].ParentIndex;
        if((ParentIndex != NoParentBone) &&
           (GetTrackMaskBoneWeight(Mask, ParentIndex) == KeyWeight))
        {
            SetTrackMaskBoneWeight(Mask, Index, KeyWeight);
        }
    }}

    SetTrackMaskBoneWeight(Mask, ChainRootBoneIndex, Weight);
    {for(int32x Index = ChainRootBoneIndex + 1;
         Index < Skeleton.BoneCount;
         ++Index)
    {
        if(GetTrackMaskBoneWeight(Mask, Index) == KeyWeight)
        {
            SetTrackMaskBoneWeight(Mask, Index, Weight);
        }
    }}
}




struct granny_bone_track_mask_data
{
    real32 BoneWeight;
};

static bool
RecursiveSearchVariantForWeight(variant const& Variant,
                                char const* MaskName,
                                real32* Weight)
{
    CheckPointerNotNull(MaskName, return false);
    CheckPointerNotNull(Weight, return false);

    if (!Variant.Type || !Variant.Object)
        return false;

    const data_type_definition GrannyBoneTrackMaskDataType[] = {
        {Real32Member, MaskName},
        {EndMember}
    };

    variant Result;
    FindMatchingMember ( Variant.Type, Variant.Object, MaskName, &Result );
    if ( Result.Object != NULL )
    {
        // Found it
        granny_bone_track_mask_data TheData;
        ConvertSingleObject ( Variant.Type, Variant.Object, GrannyBoneTrackMaskDataType, &TheData, 0 );
        *Weight = TheData.BoneWeight;

        return true;
    }
    else
    {
        // Search sub-members for references or variants that we can recurse into
        member_iterator Iter;
        IterateOverMembers(Variant.Type, Variant.Object, Iter);
        {for(;
             MemberIteratorIsValid(Iter);
             AdvanceMemberIterator(Iter))
        {
            switch(Iter.Type)
            {
                case ReferenceMember:
                case VariantReferenceMember:
                {
                    variant SubObject = { *Iter.PointerType, *Iter.Pointer };
                    if (RecursiveSearchVariantForWeight(SubObject, MaskName, Weight))
                        return true;
                } break;

                default: break;
            }
        }}

        return false;
    }
}

extract_track_mask_result GRANNY
ExtractTrackMask(track_mask *TrackMask,
                 int32x BoneCount,
                 skeleton const &Skeleton,
                 char const* MaskName,
                 real32 DefaultWeight,
                 bool UseParentForDefault)
{
    // Can't ask for more bones than the skeleton has.
    CheckBoundedInt32 ( 0, BoneCount, Skeleton.BoneCount,
                        return ExtractTrackMaskResult_NoDataPresent );
    // Can't ask for more bones than the track_mask has.
    CheckBoundedInt32 ( 0, BoneCount, TrackMask->BoneWeightCount,
                        return ExtractTrackMaskResult_NoDataPresent );

    TrackMask->DefaultWeight = DefaultWeight;

    bool FoundNoData = true;
    bool FoundAllData = true;
    {for ( int32x BoneNum = 0; BoneNum < BoneCount; BoneNum++ )
    {
        // Look for extended data called MaskName in each bone.
        bone &Bone = Skeleton.Bones[BoneNum];

        real32 Weight;
        if (RecursiveSearchVariantForWeight(Bone.ExtendedData, MaskName, &Weight))
        {
            FoundNoData = false;
            TrackMask->BoneWeights[BoneNum] = Weight;
        }
        else
        {
            // Not found.
            FoundAllData = false;
            if ( UseParentForDefault )
            {
                // Use the parent's weight (which in turn will have used its parent's
                // weight, etc).
                int32x ParentIndex = Skeleton.Bones[BoneNum].ParentIndex;
                Assert ( ParentIndex < BoneNum );
                if ( ParentIndex == -1 )
                {
                    // I am a root bone.
                    TrackMask->BoneWeights[BoneNum] = DefaultWeight;
                }
                else
                {
                    TrackMask->BoneWeights[BoneNum] = TrackMask->BoneWeights[ParentIndex];
                }
            }
            else
            {
                TrackMask->BoneWeights[BoneNum] = DefaultWeight;
            }
        }
    }}

    if ( FoundNoData )
    {
        return ExtractTrackMaskResult_NoDataPresent;
    }
    else if ( FoundAllData )
    {
        return ExtractTrackMaskResult_AllDataPresent;
    }
    else
    {
        return ExtractTrackMaskResult_PartialDataPresent;
    }
}


// EXPAPI GS_SAFE int32x GetUnboundTrackMaskSize(int32x BoneCount);
// EXPAPI GS_SAFE unbound_track_mask *NewUnboundTrackMask(real32 DefaultWeight, int32x BoneCount);
// EXPAPI GS_SAFE unbound_track_mask *NewUnboundTrackMaskInPlace(real32 DefaultWeight, int32x BoneCount, void* Memory);

static void
AggrUnboundTrackMask(aggr_allocator& Allocator,
                     unbound_track_mask*& TrackMask,
                     int32x BoneCount)
{
    Assert(BoneCount > 0);
    AggrAllocPtr(Allocator, TrackMask);
    AggrAllocOffsetArrayPtr(Allocator, TrackMask, BoneCount, WeightCount, Weights);
}

unbound_track_mask* GRANNY
NewUnboundTrackMask(real32 DefaultWeight, int32x BoneCount)
{
    CheckCondition(BoneCount > 0, return 0);

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    unbound_track_mask* TrackMask = 0;
    AggrUnboundTrackMask(Allocator, TrackMask, BoneCount);
    if(EndAggrAlloc(Allocator, AllocationBuilder))
    {
        TrackMask->DefaultWeight = DefaultWeight;
        {for(int32x BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
        {
            TrackMask->Weights[BoneIndex].Name   = 0;
            TrackMask->Weights[BoneIndex].Weight = TrackMask->DefaultWeight;
        }}
    }

    return TrackMask;
}

unbound_track_mask* GRANNY
NewUnboundTrackMaskInPlace(real32 DefaultWeight,
                           int32x BoneCount,
                           void*  Memory)
{
    CheckPointerNotNull(Memory, return 0);
    CheckCondition(BoneCount > 0, return 0);

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    unbound_track_mask* TrackMask = 0;
    AggrUnboundTrackMask(Allocator, TrackMask, BoneCount);

    if (EndAggrPlacement(Allocator, Memory))
    {
        TrackMask->DefaultWeight = DefaultWeight;

        {for(int32x BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
        {
            TrackMask->Weights[BoneIndex].Name   = 0;
            TrackMask->Weights[BoneIndex].Weight = TrackMask->DefaultWeight;
        }}
    }

    return TrackMask;
}

void GRANNY
FreeUnboundTrackMask(unbound_track_mask *Mask)
{
    // Do not delete the names...
    if (Mask)
    {
        Deallocate(Mask);
    }
}


int32x GRANNY
GetUnboundTrackMaskSize(int32x BoneCount)
{
    CheckCondition(BoneCount > 0, return 0);

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    unbound_track_mask* TrackMask;
    AggrUnboundTrackMask(Allocator, TrackMask, BoneCount);

    return (int32x)EndAggrSize(Allocator);
}




int32x GRANNY
FindMaskIndexForName(unbound_track_mask& UnboundMask,
                     char const* Name)
{
    CheckPointerNotNull(Name, return -1);
    CheckCondition(UnboundMask.Weights || UnboundMask.WeightCount == 0, return -1);

    {for(int32x MaskIdx = 0; MaskIdx < UnboundMask.WeightCount; ++MaskIdx)
    {
        if (StringsAreEqualOrCallback(UnboundMask.Weights[MaskIdx].Name, Name))
        {
            return MaskIdx;
        }
    }}

    return -1;
}


void GRANNY
BindTrackmaskToModel(unbound_track_mask& UnboundMask,
                     model& Model,
                     track_mask& Mask)
{
    CheckPointerNotNull(Model.Skeleton, return);
    CheckCondition(Model.Skeleton->BoneCount == Mask.BoneWeightCount, return);

    Mask.DefaultWeight = UnboundMask.DefaultWeight;
    {for(int32x Bone = 0; Bone < Model.Skeleton->BoneCount; ++Bone)
    {
        char const* BoneName = Model.Skeleton->Bones[Bone].Name;
        int32x IndexForName = FindMaskIndexForName(UnboundMask, BoneName);

        if (IndexForName != -1)
            Mask.BoneWeights[Bone] = UnboundMask.Weights[IndexForName].Weight;
        else
            Mask.BoneWeights[Bone] = UnboundMask.DefaultWeight;
    }}
}

void GRANNY
BindTrackmaskToTrackGroup(unbound_track_mask& UnboundMask,
                          track_group& TrackGroup,
                          track_mask& Mask)
{
    CheckCondition(TrackGroup.TransformTracks || TrackGroup.TransformTrackCount == 0, return);
    CheckCondition(TrackGroup.TransformTrackCount == Mask.BoneWeightCount, return);

    Mask.DefaultWeight = UnboundMask.DefaultWeight;
    {for(int32x Track = 0; Track < TrackGroup.TransformTrackCount; ++Track)
    {
        char const* TrackName = TrackGroup.TransformTracks[Track].Name;
        int32x IndexForName = FindMaskIndexForName(UnboundMask, TrackName);

        if (IndexForName != -1)
            Mask.BoneWeights[Track] = UnboundMask.Weights[IndexForName].Weight;
        else
            Mask.BoneWeights[Track] = UnboundMask.DefaultWeight;
    }}
}
