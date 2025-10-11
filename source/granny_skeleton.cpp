// ========================================================================
// $File: //jeffr/granny_29/rt/granny_skeleton.cpp $
// $DateTime: 2012/03/14 11:05:37 $
// $Change: 36762 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_skeleton.h"

#include "granny_aggr_alloc.h"
#include "granny_controlled_animation.h"
#include "granny_limits.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"
#include "granny_string.h"

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
#define SubsystemCode SkeletonLogMessage

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

data_type_definition BoneType[] =
{
    {StringMember, "Name"},

    {Int32Member, "ParentIndex"},
    {TransformMember, "Transform"},
    {Real32Member, "InverseWorldTransform", 0, 16},
    {Real32Member, "LODError"},

    {VariantReferenceMember, "ExtendedData"},

    {EndMember},
};

data_type_definition SkeletonType[] =
{
    {StringMember, "Name"},

    {ReferenceToArrayMember, "Bones", BoneType},

    {Int32Member, "LODType"},
    {VariantReferenceMember, "ExtendedData"},

    {EndMember},
};

END_GRANNY_NAMESPACE;

inline transform &
GetTransform(int32x TransformStride, transform const *Transforms,
             int32x Index)
{
    return(*(transform *)(((uint8 *)Transforms) +
                          (TransformStride*Index)));
}

inline int32x
GetParent(int32x ParentStride, int32 const *Parents, int32x Index)
{
    return(*(int32 *)(((uint8 *)Parents) + (ParentStride*Index)));
}


void GRANNY
AggrSkeleton(aggr_allocator& Allocator,
             int32x          NumBones,
             skeleton*&      Skeleton)
{
    AggrAllocPtr(Allocator, Skeleton);
    AggrAllocOffsetArrayPtr(Allocator, Skeleton, NumBones, BoneCount, Bones);
}


skeleton* GRANNY
NewSkeleton(int32x BoneCount)
{
    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    skeleton* Skeleton = 0;
    AggrSkeleton(Allocator, BoneCount, Skeleton);
    if(EndAggrAlloc(Allocator, AllocationBuilder))
    {
        Skeleton->Name = 0;
        Skeleton->LODType = NoSkeletonLOD;
        ZeroArray(Skeleton->BoneCount, Skeleton->Bones);
        Skeleton->ExtendedData.Type = 0;
        Skeleton->ExtendedData.Object = 0;
    }

    return Skeleton;
}


void GRANNY
BuildSkeletonRelativeTransform(int32x SourceTransformStride,
                               transform const *SourceTransforms,
                               int32x SourceParentStride,
                               int32 const *SourceParents,
                               int32x BoneIndex,
                               transform &Result)
{
    int32x ParentIndex = GetParent(SourceParentStride, SourceParents, BoneIndex);

    // Do we have parents?
    if(ParentIndex == NoParentBone)
    {
        // We have no parents, so copy the result directly
        Copy(SizeOf(Result),
             &GetTransform(SourceTransformStride, SourceTransforms, BoneIndex),
             &Result);
    }
    else
    {
        // We have at least one parent, so fill the result with the first
        // concatenation, then loop from there.
        Multiply(Result,
                 GetTransform(SourceTransformStride,
                              SourceTransforms,
                              ParentIndex),
                 GetTransform(SourceTransformStride,
                              SourceTransforms,
                              BoneIndex));
        BoneIndex = ParentIndex;

        while((ParentIndex = GetParent(SourceParentStride, SourceParents, BoneIndex)) != NoParentBone)
        {
            PreMultiplyBy(Result,
                          GetTransform(SourceTransformStride,
                                       SourceTransforms, ParentIndex));
            BoneIndex = ParentIndex;
        }
    }
}

void GRANNY
BuildSkeletonRelativeTransforms(int32x SourceTransformStride,
                                transform const *SourceTransforms,
                                int32x SourceParentStride,
                                int32 const *SourceParents,
                                int32x Count,
                                int32x ResultStride,
                                transform *Results)
{
    int32 const *Parent = SourceParents;
    transform *Dest = Results;

    if(SourceTransforms == Dest)
    {
        // This is the in-buffer transform path, which is generally
        // more efficient since we don't have to do any copying.

        while(Count--)
        {
            if(*Parent != NoParentBone)
            {
                // We have parents, so multiply by our parents _result_
                // transform (thereby saving us the work of multiplying
                // out our entire chain).
                PreMultiplyBy(*Dest, *((transform *)((uint8 *)Results + (*Parent * ResultStride))));
            }

            Parent = (int32 const *)((uint8 const *)Parent + SourceParentStride);
            Dest = (transform *)((uint8 *)Dest + ResultStride);
        }
    }
    else
    {
        // This is the buffer-to-buffer transform path
        transform const *Source = SourceTransforms;

        while(Count--)
        {
            if(*Parent == NoParentBone)
            {
                // We have no parents, so copy the result directly
                Copy(SizeOf(*Dest), Source, Dest);
            }
            else
            {
                // We have parents, so multiply by our parents _result_
                // transform (thereby saving us the work of multiplying
                // out our entire chain).
                Multiply(*Dest, GetTransform(ResultStride, Results, *Parent),
                         *Source);
            }

            Source = (transform const *)((uint8 *)Source + SourceTransformStride);
            Parent = (int32 const *)((uint8 const *)Parent + SourceParentStride);
            Dest = (transform *)((uint8 *)Dest + ResultStride);
        }
    }
}

bool GRANNY
FindBoneByName(skeleton const *Skeleton, char const *BoneName, int32x &BoneIndex)
{
    if(Skeleton)
    {
        {for(BoneIndex = 0;
             BoneIndex < Skeleton->BoneCount;
             ++BoneIndex)
        {
            if (StringsAreEqualOrCallback(BoneName, Skeleton->Bones[BoneIndex].Name))
                return true;
        }}
    }

    return(false);
}

bool GRANNY
FindBoneByNameLowercase(skeleton const *Skeleton, char const *BoneName, int32x &BoneIndex)
{
    if(Skeleton)
    {
        {for(BoneIndex = 0;
             BoneIndex < Skeleton->BoneCount;
             ++BoneIndex)
        {
            if (StringsAreEqualLowercaseOrCallback(BoneName,
                                                   Skeleton->Bones[BoneIndex].Name))
            {
                return(true);
            }
        }}
    }

    return(false);
}


bool GRANNY
FindBoneByRule(skeleton const* Skeleton,
               char const *ProcessedBoneName,
               char const *BonePattern,
               int32x &BoneIndex)
{
    char BoneNameBuffer[MaximumBoneNameLength + 1];
    if (Skeleton)
    {
        {for(BoneIndex = 0;
             BoneIndex < Skeleton->BoneCount;
             ++BoneIndex)
        {
            BoneNameBuffer[0] = '\0';
            WildCardMatch(Skeleton->Bones[BoneIndex].Name, BonePattern, BoneNameBuffer);
            if (StringsAreEqual(ProcessedBoneName, BoneNameBuffer))
            {
                return true;
            }
        }}
    }

    BoneIndex = -1;
    return false;
}


int32x GRANNY
GetBoneCountForLOD(skeleton const* Skeleton,
                   real32          AllowedError)
{
    CheckPointerNotNull(Skeleton, return 0);

    if (AllowedError == 0.0f)
        return Skeleton->BoneCount;

    real32 ErrorEnd;
    real32 ErrorScaler;
    FindAllowedErrorNumbers(AllowedError, &ErrorEnd, &ErrorScaler);

    if (Skeleton)
    {
        if (Skeleton->LODType == NoSkeletonLOD)
        {
            return Skeleton->BoneCount;
        }

        // Otherwise, we do a search.
        // TODO: replace with binary
        int Count = 0;
        for (; Count < Skeleton->BoneCount; Count++)
        {
            // < to make sure that 0 = 0 doesn't get skipped...
            if (Skeleton->Bones[Count].LODError < ErrorEnd)
                break;
        }

        return Count;
    }
    else
    {
        return 0;
    }
}

