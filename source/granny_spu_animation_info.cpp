// ========================================================================
// $File: //jeffr/granny_29/rt/granny_spu_animation_info.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_spu_animation_info.h"

#include "granny_data_type_conversion.h"
#include "granny_file.h"
#include "granny_file_format.h"
#include "granny_log.h"
#include "granny_spu_animation.h"
#include "granny_spu_track_group.h"

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

data_type_definition SPUAnimationInfoType[] =
{
    {ArrayOfReferencesMember, "SPUAnimations", SPUAnimationType},
    {VariantReferenceMember, "ExtendedData"},

    {EndMember},
};

END_GRANNY_NAMESPACE;

spu_animation_info* GRANNY
GetSPUAnimationInfo(file &File)
{
    variant Root;
    GetDataTreeFromFile(File, &Root);

    uint32 TypeTag = File.Header->TypeTag;
    if(TypeTag == CurrentGRNStandardTag)
    {
        return((spu_animation_info *)Root.Object);
    }
    else
    {
        if (!File.ConversionBuffer)
        {
            Log2(WarningLogMessage, FileReadingLogMessage,
                 "File has run-time type tag of 0x%x, which doesn't match this "
                 "version of Granny (0x%x).  Automatic conversion will "
                 "be attempted.", TypeTag, CurrentGRNStandardTag);

            File.ConversionBuffer =
                ConvertTree(Root.Type, Root.Object, SPUAnimationInfoType, 0);
        }

        // If we're converting, go back and touch the NumBytesRequired member
        spu_animation_info* Info = (spu_animation_info *)File.ConversionBuffer;
        if (Info)
        {
            {for (int32x Idx = 0; Idx < Info->SPUAnimationCount; ++Idx)
            {
                spu_animation* Animation = Info->SPUAnimations[Idx];
                if (!Animation)
                    continue;

                {for (int32x TGIdx = 0; TGIdx < Animation->TrackGroupCount; ++TGIdx)
                {
                    spu_track_group* TrackGroup = Animation->TrackGroups[TGIdx];
                    if (!TrackGroup)
                        continue;

                    RecomputeSPUTrackRequiredBytes(*TrackGroup);
                }}
            }}
        }

        return Info;
    }
}

