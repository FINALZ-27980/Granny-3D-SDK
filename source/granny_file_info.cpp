// ========================================================================
// $File: //jeffr/granny_29/rt/granny_file_info.cpp $
// $DateTime: 2012/08/03 14:39:43 $
// $Change: 38655 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_file_info.h"

#include "granny_animation.h"
#include "granny_art_tool_info.h"
#include "granny_data_type_conversion.h"
#include "granny_exporter_info.h"
#include "granny_file.h"
#include "granny_file_format.h"
#include "granny_log.h"
#include "granny_material.h"
#include "granny_mesh.h"
#include "granny_model.h"
#include "granny_telemetry.h"
#include "granny_texture.h"
#include "granny_track_group.h"
#include "granny_tri_topology.h"
#include "granny_vertex_data.h"

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

data_type_definition FileInfoType[] =
{
    // Note that the TouchVersion command looks for this member, make sure to update that
    // preprocessor command if this moves.
    {ReferenceMember, "ArtToolInfo", ArtToolInfoType},

    {ReferenceMember, "ExporterInfo", ExporterInfoType},
    {StringMember, "FromFileName"},

    {ArrayOfReferencesMember, "Textures", TextureType},
    {ArrayOfReferencesMember, "Materials", MaterialType},

    {ArrayOfReferencesMember, "Skeletons", SkeletonType},
    {ArrayOfReferencesMember, "VertexDatas", VertexDataType},
    {ArrayOfReferencesMember, "TriTopologies", TriTopologyType},
    {ArrayOfReferencesMember, "Meshes", MeshType},
    {ArrayOfReferencesMember, "Models", ModelType},

    {ArrayOfReferencesMember, "TrackGroups", TrackGroupType},
    {ArrayOfReferencesMember, "Animations", AnimationType},

    {VariantReferenceMember, "ExtendedData"},

    {EndMember},
};

END_GRANNY_NAMESPACE;


file_info *GRANNY
GetFileInfo(file &File)
{
    GRANNY_AUTO_ZONE_FN();

    variant Root;
    GetDataTreeFromFile(File, &Root);

    uint32 TypeTag = File.Header->TypeTag;
    if(TypeTag == CurrentGRNStandardTag)
    {
        return((file_info *)Root.Object);
    }
    else
    {
        GRANNY_AUTO_ZONE(FileInfo_Version);

        if(!File.ConversionBuffer)
        {
            Log2(WarningLogMessage, FileReadingLogMessage,
                 "File has run-time type tag of 0x%x, which doesn't match this "
                 "version of Granny (0x%x).  Automatic conversion will "
                 "be attempted.", TypeTag, CurrentGRNStandardTag);

            File.ConversionBuffer =
                ConvertTree(Root.Type, Root.Object, FileInfoType, 0);
        }

        return((file_info *)File.ConversionBuffer);
    }
}

