// ========================================================================
// $File: //jeffr/granny_29/rt/granny_spu_animation.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_spu_animation.h"

#include "granny_spu_track_group.h"
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

USING_GRANNY_NAMESPACE;

data_type_definition GRANNY SPUAnimationType[] =
{
    {StringMember, "Name"},

    {Real32Member, "Duration"},
    {Real32Member, "TimeStep"},
    {Real32Member, "Oversampling"},

    {ArrayOfReferencesMember, "TrackGroups", SPUTrackGroupType},

    {EndMember},
};

bool GRANNY
FindSPUTrackGroupForModel(spu_animation const& Animation,
                          char const* ModelName,
                          int32x& TrackGroupIndex)
{
    {for(TrackGroupIndex = 0;
         TrackGroupIndex < Animation.TrackGroupCount;
         ++TrackGroupIndex)
    {
        if (StringsAreEqualLowercaseOrCallback(
                Animation.TrackGroups[TrackGroupIndex]->Name,
                ModelName))
        {
            return true;
        }
    }}

    return false;
}
