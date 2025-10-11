// ========================================================================
// $File: //jeffr/granny_29/rt/granny_spu_track_group.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_spu_track_group.h"

#include "granny_math.h"

#if PLATFORM_PS3
#include "granny_spu_curve_sample_ppu.h"
#endif

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

#undef SubsystemCode
#define SubsystemCode TrackGroupLogMessage

BEGIN_GRANNY_NAMESPACE;

data_type_definition SPUTransformTrackType[] =
{
    {UInt32Member,    "FromNameIndex"},
    {UInt32Member,    "Flags"},

    {Real32Member,    "AnimLODValue"},
    {TransformMember, "Transform" },

    {UInt32Member,    "PosCurveOffset"},
    {UInt32Member,    "OriCurveOffset"},
    {UInt32Member,    "SSCurveOffset"},

    {UInt32Member,    "BytesRequiredToSample"},

    {EndMember},
};

data_type_definition SPUTrackGroupType[] =
{
    { StringMember, "Name" },
    { Int32Member,  "Flags" },

    { ReferenceToArrayMember, "TrackNames",      StringType },
    { ReferenceToArrayMember, "TransformTracks", SPUTransformTrackType },
    { ReferenceToArrayMember, "CurveBytes",      UInt8Type },

    { EndMember },
};

END_GRANNY_NAMESPACE;

void GRANNY
RecomputeSPUTrackRequiredBytes(spu_track_group& Group)
{
    {for (int32x Idx = 0; Idx < Group.TransformTrackCount; ++Idx)
    {
        spu_transform_track& Track = Group.TransformTracks[Idx];

        Track.BytesRequiredToSample = uint32(-1);
        if (Track.PosCurveOffset != uint32(-1))
            Track.BytesRequiredToSample = Track.PosCurveOffset;
        else if (Track.OriCurveOffset != uint32(-1))
            Track.BytesRequiredToSample = Track.OriCurveOffset;
        else if (Track.SSCurveOffset != uint32(-1))
            Track.BytesRequiredToSample = Track.SSCurveOffset;
    }}

    uint32 Current = Group.CurveByteCount;
    {for (int32x Idx = Group.TransformTrackCount - 1; Idx >= 0; --Idx)
    {
        uint32 ThisSetting = Group.TransformTracks[Idx].BytesRequiredToSample;
        if (ThisSetting == uint32(-1))
        {
            Group.TransformTracks[Idx].BytesRequiredToSample = 0;
        }
        else
        {
            Assert(ThisSetting < Current);

            Group.TransformTracks[Idx].BytesRequiredToSample = (Current - ThisSetting);
            Current = ThisSetting;
        }
    }}
    Assert(Current == 0);
}

void GRANNY
SPUSampleTrackPOLocal(sample_context const&      Context,
                      spu_track_group const&     SourceGroup,
                      spu_transform_track const& SourceTrack,
                      real32*                    ResultPosition,
                      real32*                    ResultOrientation)
{
#if PLATFORM_PS3
    if (SourceTrack.PosCurveOffset != SPUTransformTrackNoCurve)
    {
        EvaluateSPUCurve(Context,
                         SourceGroup.CurveBytes + SourceTrack.PosCurveOffset,
                         3,
                         ResultPosition);
    }
    else
    {
        VectorEquals3(ResultPosition, SourceTrack.LODTransform.Position);
    }

    if (SourceTrack.OriCurveOffset != SPUTransformTrackNoCurve)
    {
        EvaluateSPUCurve(Context,
                         SourceGroup.CurveBytes + SourceTrack.OriCurveOffset,
                         4,
                         ResultOrientation);
    }
    else
    {
        VectorEquals4(ResultOrientation, SourceTrack.LODTransform.Orientation);
    }
#else
    InvalidCodePath("SPUSampleTrackPOLocal not supported on this platform");
#endif
}

