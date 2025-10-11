#if !defined(GRANNY_TRACK_MASK_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_track_mask.h $
// $DateTime: 2012/09/07 12:25:44 $
// $Change: 39201 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(TrackMaskGroup);

struct track_group;
struct model;
struct skeleton;
struct data_type_definition;

EXPTYPE struct track_mask;
struct track_mask
{
    real32 DefaultWeight;

    int32 BoneWeightCount;
    real32 *BoneWeights;
};
EXPCONST EXPGROUP(track_mask) extern data_type_definition TrackMaskType[];

EXPTYPE EXPGROUP(unbound_track_mask) struct unbound_weight
{
    char*  Name;
    real32 Weight;
};

EXPTYPE struct unbound_track_mask
{
    real32 DefaultWeight;

    int32 WeightCount;
    unbound_weight* Weights;
};
EXPCONST EXPGROUP(unbound_track_mask) extern data_type_definition UnboundTrackMaskType[];


EXPTYPE enum extract_track_mask_result
{
    ExtractTrackMaskResult_AllDataPresent,
    ExtractTrackMaskResult_PartialDataPresent,
    ExtractTrackMaskResult_NoDataPresent,

    extract_track_mask_forceint = 0x7fffffff
};



EXPAPI GS_SAFE int32x GetTrackMaskSize(int32x BoneCount);
EXPAPI GS_SAFE track_mask *NewTrackMask(real32 DefaultWeight, int32x BoneCount);
EXPAPI GS_SAFE track_mask *NewTrackMaskInPlace(real32 DefaultWeight, int32x BoneCount, void* Memory);
EXPAPI GS_PARAM extract_track_mask_result ExtractTrackMask(track_mask *TrackMask,
                                                           int32x BoneCount,
                                                           skeleton const &Skeleton,
                                                           char const* MaskName,
                                                           real32 DefaultWeight,
                                                           bool UseParentForDefault);
EXPAPI GS_READ real32 GetTrackMaskBoneWeight(track_mask const &Mask, int32x BoneIndex);
EXPAPI GS_PARAM void SetTrackMaskBoneWeight(track_mask &Mask, int32x BoneIndex,
                                            real32 Weight);
EXPAPI GS_PARAM void FreeTrackMask(track_mask *Mask);

EXPAPI GS_PARAM bool CopyTrackMask(track_mask& Dest, track_mask const &Source);
EXPAPI GS_PARAM track_mask *CloneTrackMask(track_mask const &Mask);
EXPAPI GS_PARAM void InvertTrackMask(track_mask &Mask);

EXPAPI GS_PARAM void SetSkeletonTrackMaskFromTrackGroup(track_mask &Mask,
                                                        skeleton const &Skeleton,
                                                        track_group const &TrackGroup,
                                                        real32 IdentityValue,
                                                        real32 ConstantValue,
                                                        real32 AnimatedValue);

EXPAPI GS_PARAM void SetSkeletonTrackMaskChainUpwards(track_mask &Mask,
                                                      skeleton const &Skeleton,
                                                      int32x ChainLeafBoneIndex,
                                                      real32 Weight);
EXPAPI GS_PARAM void SetSkeletonTrackMaskChainDownwards(track_mask &Mask,
                                                        skeleton const &Skeleton,
                                                        int32x ChainRootBoneIndex,
                                                        real32 Weight);

EXPAPI GS_SAFE int32x GetUnboundTrackMaskSize(int32x BoneCount);
EXPAPI GS_SAFE unbound_track_mask *NewUnboundTrackMask(real32 DefaultWeight, int32x BoneCount);
EXPAPI GS_SAFE unbound_track_mask *NewUnboundTrackMaskInPlace(real32 DefaultWeight, int32x BoneCount, void* Memory);
EXPAPI GS_PARAM void FreeUnboundTrackMask(unbound_track_mask *Mask);

EXPAPI GS_READ int32x FindMaskIndexForName(unbound_track_mask& UnboundMask,
                                           char const* Name);

EXPAPI GS_PARAM void BindTrackmaskToModel(unbound_track_mask& UnboundMask,
                                          model& Model,
                                          track_mask& Mask);
EXPAPI GS_PARAM void BindTrackmaskToTrackGroup(unbound_track_mask& UnboundMask,
                                               track_group& Model,
                                               track_mask& Mask);

EXPCONST extern track_mask IdentityTrackMask;
EXPCONST extern track_mask NullTrackMask;

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_TRACK_MASK_H
#endif
