// ========================================================================
// $File: //jeffr/granny_29/rt/granny_animation.cpp $
// $DateTime: 2012/10/22 16:12:50 $
// $Change: 39903 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_animation.h"

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

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

data_type_definition AnimationType[] =
{
    {StringMember, "Name"},

    {Real32Member, "Duration"},
    {Real32Member, "TimeStep"},
    {Real32Member, "Oversampling"},

    {ArrayOfReferencesMember, "TrackGroups", TrackGroupType},

    {Int32Member, "DefaultLoopCount"},
    {Int32Member, "Flags"},

    {VariantReferenceMember, "ExtendedData"},

    {EndMember},
};

END_GRANNY_NAMESPACE;

bool GRANNY
FindTrackGroupForModel(animation const &Animation,
                       char const *ModelName,
                       int32x &TrackGroupIndex)
{
    for (TrackGroupIndex = 0; TrackGroupIndex < Animation.TrackGroupCount; ++TrackGroupIndex)
    {
        // Ignore morph tracks in this loop, but do that check first, since it's faster
        // than a strcmp
        if ((Animation.TrackGroups[TrackGroupIndex]->Flags & TrackGroupIsMorphs) == 0 &&
            StringsAreEqualLowercaseOrCallback(Animation.TrackGroups[TrackGroupIndex]->Name,
                                               ModelName))
        {
            return true;
        }
    }

    // Just to be sure
    TrackGroupIndex = -1;
    return false;
}

bool GRANNY
FindMorphTrackGroupForMesh(animation const &Animation,
                           char const* MeshName,
                           int32x &TrackGroupIndex)
{
    for (TrackGroupIndex = 0; TrackGroupIndex < Animation.TrackGroupCount; ++TrackGroupIndex)
    {
        // Ignore morph tracks in this loop, but do that check first, since it's faster
        // than a strcmp
        if ((Animation.TrackGroups[TrackGroupIndex]->Flags & TrackGroupIsMorphs) != 0 &&
            StringsAreEqualLowercaseOrCallback(Animation.TrackGroups[TrackGroupIndex]->Name,
                                               MeshName))
        {
            return true;
        }
    }

    // Just to be sure
    TrackGroupIndex = -1;
    return false;
}
