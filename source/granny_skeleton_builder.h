#if !defined(GRANNY_SKELETON_BUILDER_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_skeleton_builder.h $
// $DateTime: 2012/06/01 10:29:47 $
// $Change: 37652 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(SkeletonBuilderGroup);

struct skeleton;
EXPTYPE struct skeleton_builder;

// Skeleton builder
EXPAPI GS_SAFE skeleton_builder *BeginSkeleton(int32x BoneCount);
EXPAPI GS_PARAM skeleton *EndSkeleton(skeleton_builder *Builder,
                                      int32x *RemappingTable);

// User-allocated store
EXPAPI GS_SAFE int32x GetResultingSkeletonSize(skeleton_builder const &Builder);
EXPAPI GS_PARAM skeleton *EndSkeletonInPlace(skeleton_builder *Builder,
                                             void *InMemory,
                                             int32x *RemappingTable);

// Transforms
EXPAPI GS_PARAM bool AddBone(skeleton_builder& Builder,
                             char const*       BoneName,
                             real32 const*     LocalPosition3,
                             real32 const*     LocalOrientation4,
                             real32 const*     LocalScaleShear3x3,
                             real32 const*     WorldPosition3,
                             real32 const*     WorldOrientation4,
                             real32 const*     WorldScaleShear3x3);
                
EXPAPI GS_PARAM void AddBoneWithInverse(skeleton_builder& Builder,
                                        char const*       BoneName,
                                        real32 const*     LocalPosition3,
                                        real32 const*     LocalOrientation4,
                                        real32 const*     LocalScaleShear3x3,
                                        matrix_4x4 const* InverseWorld4x4);

// Transform parenting
EXPAPI GS_PARAM void SetBoneParent(skeleton_builder &Builder,
                                   int32x BoneIndex,
                                   int32x ParentIndex);

EXPAPI GS_PARAM void SetBoneLODError(skeleton_builder &Builder,
                                     int32x BoneIndex,
                                     real32 LODError);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_SKELETON_BUILDER_H
#endif
