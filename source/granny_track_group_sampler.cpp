// ========================================================================
// $File: //jeffr/granny_29/rt/granny_track_group_sampler.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_track_group_sampler.h"

#include "granny_aggr_alloc.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"

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
#define SubsystemCode TrackGroupLogMessage

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

struct track_group_sampler
{
    bool32 BlockedAllocation;
    int32x TransformCurveCount;

    int32x MaxSampleCount;

    // Valid in all allocations
    real32** TrackPositionSamples;
    real32** TrackOrientationSamples;
    real32** TrackScaleShearSamples;

    // Only valid in blocked allocations
    int32x PositionSampleCount;
    real32 *PositionSamples;

    int32x OrientationSampleCount;
    real32 *OrientationSamples;

    int32x ScaleShearSampleCount;
    real32 *ScaleShearSamples;
};

END_GRANNY_NAMESPACE;

track_group_sampler *GRANNY
BeginSampledAnimation(int32x TransformCurveCount,
                      int32x SampleCount)
{
    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    track_group_sampler *Sampler;
    AggrAllocPtr(Allocator, Sampler);

    AggrAllocOffsetCountlessArrayPtr(Allocator, Sampler, TransformCurveCount, TrackPositionSamples);
    AggrAllocOffsetCountlessArrayPtr(Allocator, Sampler, TransformCurveCount, TrackOrientationSamples);
    AggrAllocOffsetCountlessArrayPtr(Allocator, Sampler, TransformCurveCount, TrackScaleShearSamples);

    AggrAllocOffsetArrayPtr(Allocator, Sampler, SampleCount * TransformCurveCount * 3,
                            PositionSampleCount, PositionSamples);
    AggrAllocOffsetArrayPtr(Allocator, Sampler, SampleCount * TransformCurveCount * 4,
                            OrientationSampleCount, OrientationSamples);
    AggrAllocOffsetArrayPtr(Allocator, Sampler, SampleCount * TransformCurveCount * 9,
                            ScaleShearSampleCount, ScaleShearSamples);

    if(EndAggrAlloc(Allocator, AllocationBuilder))
    {
        Sampler->BlockedAllocation = true;
        Sampler->TransformCurveCount = TransformCurveCount;
        Sampler->MaxSampleCount = SampleCount;

        {for(int32x Track = 0; Track < Sampler->TransformCurveCount; ++Track)
        {
            Sampler->TrackPositionSamples[Track] =
                (Sampler->PositionSamples + Track * (Sampler->MaxSampleCount * 3));
            Sampler->TrackOrientationSamples[Track] =
                (Sampler->OrientationSamples + Track * (Sampler->MaxSampleCount * 4));
            Sampler->TrackScaleShearSamples[Track] =
                (Sampler->ScaleShearSamples + Track * (Sampler->MaxSampleCount * 9));
        }}
    }

    return(Sampler);
}

track_group_sampler* GRANNY
BeginSampledAnimationNonBlocked(int32x TransformCurveCount, int32x SampleCount)
{
    track_group_sampler* Sampler = Allocate(track_group_sampler, AllocationBuilder);
    if (Sampler)
    {
        ZeroStructure(*Sampler);
        Sampler->BlockedAllocation = false;
        Sampler->TransformCurveCount = TransformCurveCount;
        Sampler->MaxSampleCount = SampleCount;

        Sampler->TrackPositionSamples    = AllocateArray(TransformCurveCount, real32*, AllocationUnknown);
        Sampler->TrackOrientationSamples = AllocateArray(TransformCurveCount, real32*, AllocationUnknown);
        Sampler->TrackScaleShearSamples  = AllocateArray(TransformCurveCount, real32*, AllocationUnknown);
        if (!Sampler->TrackPositionSamples ||
            !Sampler->TrackOrientationSamples ||
            !Sampler->TrackScaleShearSamples)
        {
            // Oh crap.  Bail.
            Deallocate(Sampler->TrackPositionSamples);
            Deallocate(Sampler->TrackOrientationSamples);
            Deallocate(Sampler->TrackScaleShearSamples);
            Deallocate(Sampler);
            Sampler = NULL;
        }
        else
        {
            ZeroArray(TransformCurveCount, Sampler->TrackPositionSamples);
            ZeroArray(TransformCurveCount, Sampler->TrackOrientationSamples);
            ZeroArray(TransformCurveCount, Sampler->TrackScaleShearSamples);

            // At this point, if we fail, we can bail using the EndSampledAnimation
            // function, so we have a slightly easier time of it.
            {for(int32x Track = 0; Track < TransformCurveCount; ++Track)
            {
                Sampler->TrackPositionSamples[Track]    = AllocateArray(SampleCount * 3, real32, AllocationUnknown);
                Sampler->TrackOrientationSamples[Track] = AllocateArray(SampleCount * 4, real32, AllocationUnknown);
                Sampler->TrackScaleShearSamples[Track]  = AllocateArray(SampleCount * 9, real32, AllocationUnknown);

                if (Sampler->TrackPositionSamples[Track] &&
                    Sampler->TrackOrientationSamples[Track] &&
                    Sampler->TrackScaleShearSamples[Track])
                {
                    ZeroArray(SampleCount * 3, Sampler->TrackPositionSamples[Track]);
                    ZeroArray(SampleCount * 4, Sampler->TrackOrientationSamples[Track]);
                    ZeroArray(SampleCount * 9, Sampler->TrackScaleShearSamples[Track]);
                }
                else
                {
                    EndSampledAnimation(Sampler);
                    Sampler = NULL;
                    break;
                }
            }}
        }
    }

    return Sampler;
}

void GRANNY
EndSampledAnimation(track_group_sampler *Sampler)
{
    if (Sampler)
    {
        if (Sampler->BlockedAllocation)
        {
            Deallocate(Sampler);
        }
        else
        {
            // Have to tear this down piece by piece.
            Assert(Sampler->PositionSamples == NULL);
            Assert(Sampler->OrientationSamples == NULL);
            Assert(Sampler->ScaleShearSamples == NULL);

            {for(int32x Track = 0; Track < Sampler->TransformCurveCount; ++Track)
            {
                Deallocate(Sampler->TrackPositionSamples[Track]);
                Deallocate(Sampler->TrackOrientationSamples[Track]);
                Deallocate(Sampler->TrackScaleShearSamples[Track]);
            }}
            Deallocate(Sampler->TrackPositionSamples);
            Deallocate(Sampler->TrackOrientationSamples);
            Deallocate(Sampler->TrackScaleShearSamples);
            Deallocate(Sampler);
        }
    }
}

real32* GRANNY
GetPositionSamples(track_group_sampler &Sampler, int32x TrackIndex)
{
    CheckInt32Index(TrackIndex, Sampler.TransformCurveCount, return NULL);
    CheckPointerNotNull(Sampler.TrackPositionSamples[TrackIndex], return NULL);

    return Sampler.TrackPositionSamples[TrackIndex];
}

real32* GRANNY
GetOrientationSamples(track_group_sampler &Sampler, int32x TrackIndex)
{
    CheckInt32Index(TrackIndex, Sampler.TransformCurveCount, return NULL);
    CheckPointerNotNull(Sampler.TrackOrientationSamples[TrackIndex], return NULL);

    return Sampler.TrackOrientationSamples[TrackIndex];
}

real32* GRANNY
GetScaleShearSamples(track_group_sampler &Sampler, int32x TrackIndex)
{
    CheckInt32Index(TrackIndex, Sampler.TransformCurveCount, return NULL);
    CheckPointerNotNull(Sampler.TrackScaleShearSamples[TrackIndex], return NULL);

    return Sampler.TrackScaleShearSamples[TrackIndex];
}

void GRANNY
GetTransformSample(track_group_sampler &Sampler,
                   int32x TrackIndex,
                   int32x SampleIndex,
                   real32* Position3,
                   real32* Orientation4,
                   real32* ScaleShear3x3)
{
    Assert(TrackIndex < Sampler.TransformCurveCount);
    Assert(SampleIndex < Sampler.MaxSampleCount);

    real32 const* PositionSamples    = GetPositionSamples(Sampler, TrackIndex);
    real32 const* OrientationSamples = GetOrientationSamples(Sampler, TrackIndex);
    real32 const* ScaleShearSamples  = GetScaleShearSamples(Sampler, TrackIndex);

    Copy32(3, &PositionSamples[SampleIndex * 3],    Position3);
    Copy32(4, &OrientationSamples[SampleIndex * 4], Orientation4);
    Copy32(9, &ScaleShearSamples[SampleIndex * 9],  ScaleShear3x3);
}

void GRANNY
SetTransformSample(track_group_sampler &Sampler,
                   int32x TrackIndex,
                   int32x SampleIndex,
                   real32 const *Position3,
                   real32 const *Orientation4,
                   real32 const *ScaleShear3x3)
{
    Assert(TrackIndex < Sampler.TransformCurveCount);
    Assert(SampleIndex < Sampler.MaxSampleCount);

    real32* PositionSamples    = GetPositionSamples(Sampler, TrackIndex);
    real32* OrientationSamples = GetOrientationSamples(Sampler, TrackIndex);
    real32* ScaleShearSamples  = GetScaleShearSamples(Sampler, TrackIndex);

    Copy32(3, Position3,     &PositionSamples[SampleIndex * 3]);
    Copy32(4, Orientation4,  &OrientationSamples[SampleIndex * 4]);
    Copy32(9, ScaleShear3x3, &ScaleShearSamples[SampleIndex * 9]);
}

// void GRANNY
// PushSampledFrame(track_group_sampler &Sampler)
// {
//     Assert(Sampler.SampleCount < Sampler.MaxSampleCount);
//     ++Sampler.SampleCount;
// }
