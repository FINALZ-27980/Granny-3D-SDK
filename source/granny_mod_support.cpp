// ========================================================================
// $File: //jeffr/granny_29/rt/granny_mod_support.cpp $
// $DateTime: 2012/07/25 15:47:26 $
// $Change: 38520 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_mod_support.h"

#include "granny_animation.h"
#include "granny_art_tool_info.h"
#include "granny_data_type_conversion.h"
#include "granny_data_type_definition.h"
#include "granny_exporter_info.h"
#include "granny_file.h"
#include "granny_file_format.h"
#include "granny_file_info.h"
#include "granny_log.h"
#include "granny_material.h"
#include "granny_mesh.h"
#include "granny_model.h"
#include "granny_parameter_checking.h"
#include "granny_texture.h"
#include "granny_track_group.h"
#include "granny_tri_topology.h"
#include "granny_vertex_data.h"


// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode FileReadingLogMessage
USING_GRANNY_NAMESPACE;

#if BUILDING_GRANNY_FOR_MODS

data_type_definition GRANNY ModFileInfoType[] = {
    {ReferenceMember,         (MOD_UNIQUE_PREFIX "ArtToolInfo"),   ArtToolInfoType},
    {ReferenceMember,         (MOD_UNIQUE_PREFIX "ExportInfo"),    ExporterInfoType},
    {StringMember,            (MOD_UNIQUE_PREFIX "FromFileName") },
    {ArrayOfReferencesMember, (MOD_UNIQUE_PREFIX "Textures"),      TextureType},
    {ArrayOfReferencesMember, (MOD_UNIQUE_PREFIX "Materials"),     MaterialType},
    {ArrayOfReferencesMember, (MOD_UNIQUE_PREFIX "Skeletons"),     SkeletonType},
    {ArrayOfReferencesMember, (MOD_UNIQUE_PREFIX "VertexDatas"),   VertexDataType},
    {ArrayOfReferencesMember, (MOD_UNIQUE_PREFIX "TriTopologies"), TriTopologyType},
    {ArrayOfReferencesMember, (MOD_UNIQUE_PREFIX "Meshes"),        MeshType},
    {ArrayOfReferencesMember, (MOD_UNIQUE_PREFIX "Models"),        ModelType},
    {ArrayOfReferencesMember, (MOD_UNIQUE_PREFIX "TrackGroups"),   TrackGroupType},
    {ArrayOfReferencesMember, (MOD_UNIQUE_PREFIX "Animations"),    AnimationType},
    {VariantReferenceMember,  (MOD_UNIQUE_PREFIX "ExtendedData") },

    {EndMember},
};

// These compile asserts guard against the file_info type drifting from what is specified above
CompileAssert(SizeOf(file_info) == (14 * SizeOf(void*) +
                                    9  * SizeOf(int32)));
CompileAssert(OffsetFromType(file_info, ArtToolInfo) == 0);
CompileAssert(OffsetFromType(file_info, ExporterInfo) == OffsetFromType(file_info, ArtToolInfo) + SizeOf(void*));
CompileAssert(OffsetFromType(file_info, FromFileName) == OffsetFromType(file_info, ExporterInfo) + SizeOf(void*));
CompileAssert(OffsetFromType(file_info, TextureCount) == OffsetFromType(file_info, FromFileName) + SizeOf(void*));
CompileAssert(OffsetFromType(file_info, Textures) == OffsetFromType(file_info, TextureCount) + SizeOf(int32));
CompileAssert(OffsetFromType(file_info, MaterialCount) == OffsetFromType(file_info, Textures) + SizeOf(void*));
CompileAssert(OffsetFromType(file_info, Materials) == OffsetFromType(file_info, MaterialCount) + SizeOf(int32));
CompileAssert(OffsetFromType(file_info, SkeletonCount) == OffsetFromType(file_info, Materials) + SizeOf(void*));
CompileAssert(OffsetFromType(file_info, Skeletons) == OffsetFromType(file_info, SkeletonCount) + SizeOf(int32));
CompileAssert(OffsetFromType(file_info, VertexDataCount) == OffsetFromType(file_info, Skeletons) + SizeOf(void*));
CompileAssert(OffsetFromType(file_info, VertexDatas) == OffsetFromType(file_info, VertexDataCount) + SizeOf(int32));
CompileAssert(OffsetFromType(file_info, TriTopologyCount) == OffsetFromType(file_info, VertexDatas) + SizeOf(void*));
CompileAssert(OffsetFromType(file_info, TriTopologies) == OffsetFromType(file_info, TriTopologyCount) + SizeOf(int32));
CompileAssert(OffsetFromType(file_info, MeshCount) == OffsetFromType(file_info, TriTopologies) + SizeOf(void*));
CompileAssert(OffsetFromType(file_info, Meshes) == OffsetFromType(file_info, MeshCount) + SizeOf(int32));
CompileAssert(OffsetFromType(file_info, ModelCount) == OffsetFromType(file_info, Meshes) + SizeOf(void*));
CompileAssert(OffsetFromType(file_info, Models) == OffsetFromType(file_info, ModelCount) + SizeOf(int32));
CompileAssert(OffsetFromType(file_info, TrackGroupCount) == OffsetFromType(file_info, Models) + SizeOf(void*));
CompileAssert(OffsetFromType(file_info, TrackGroups) == OffsetFromType(file_info, TrackGroupCount) + SizeOf(int32));
CompileAssert(OffsetFromType(file_info, AnimationCount) == OffsetFromType(file_info, TrackGroups) + SizeOf(void*));
CompileAssert(OffsetFromType(file_info, Animations) == OffsetFromType(file_info, AnimationCount) + SizeOf(int32));
CompileAssert(OffsetFromType(file_info, ExtendedData) == OffsetFromType(file_info, Animations) + SizeOf(void*));

file_info* GRANNY
GetModFileInfo(file& File)
{
    variant Root;
    GetDataTreeFromFile(File, &Root);

    uint32 TypeTag = File.Header->TypeTag;
    if (TypeTag == CurrentMODStandardTag)
    {
        return ((file_info *)Root.Object);
    }
    else
    {
        if (!File.ConversionBuffer)
        {
            Log2(WarningLogMessage, FileReadingLogMessage,
                 "MOD File has run-time type tag of 0x%x, which doesn't match this "
                 "version of Granny (0x%x).  Automatic conversion will "
                 "be attempted.", TypeTag, CurrentMODStandardTag);

            File.ConversionBuffer =
                ConvertTree(Root.Type, Root.Object, ModFileInfoType, 0);
        }

        return ((file_info *)File.ConversionBuffer);
    }
}

#else

data_type_definition GRANNY ModFileInfoType[] = {
    { EndMember },
};


file_info* GRANNY
GetModFileInfo(file& File)
{
    Log0(ErrorLogMessage, FileReadingLogMessage,
         "This version of granny not compiled for MOD support, invalid call to GetModFileInfo\n");
    return 0;
}

#endif // BUILDING_GRANNY_FOR_MODS

