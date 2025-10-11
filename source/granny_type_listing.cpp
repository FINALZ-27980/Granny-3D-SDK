// ========================================================================
// $File: //jeffr/granny_29/rt/granny_type_listing.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_type_listing.h"

#include "granny_animation.h"
#include "granny_art_tool_info.h"
#include "granny_curve.h"
#include "granny_exporter_info.h"
#include "granny_material.h"
#include "granny_mesh.h"
#include "granny_model.h"
#include "granny_periodic_loop.h"
#include "granny_pixel_layout.h"
#include "granny_skeleton.h"
#include "granny_string_table.h"
#include "granny_texture.h"
#include "granny_track_group.h"
#include "granny_tri_topology.h"
#include "granny_vertex_data.h"
#include "granny_memory.h"

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

// Must agree with entry in export/granny_export_ui.h
int32x const DefinedTypeIDStart = (1 << 16);

defined_type DefinedTypes[] =
{
    // NOTE: THESE CANNOT BE RENUMBERED!
    //  These are also used in the granny_export_ui code.
    //  Touch not, lest ye mess stuff up.

    {DefinedTypeIDStart +   1, "animation", AnimationType},
    {DefinedTypeIDStart +   2, "art_tool_info", ArtToolInfoType},
    {DefinedTypeIDStart +   3, "curve2", Curve2Type},
    {DefinedTypeIDStart +   4, "exporter_info", ExporterInfoType},
    {DefinedTypeIDStart +   5, "material_map", MaterialMapType},
    {DefinedTypeIDStart +   6, "material", MaterialType},
    {DefinedTypeIDStart +   7, "bone_binding", BoneBindingType},
    {DefinedTypeIDStart +   8, "material_binding", MaterialBindingType},
    {DefinedTypeIDStart +   9, "morph_target", MorphTargetType},
    {DefinedTypeIDStart +  10, "mesh", MeshType},
    {DefinedTypeIDStart +  11, "model_mesh_binding", ModelMeshBindingType},
    {DefinedTypeIDStart +  12, "model", ModelType},
    {DefinedTypeIDStart +  13, "periodic_loop", PeriodicLoopType},
    {DefinedTypeIDStart +  14, "pixel_layout", PixelLayoutType},

//     {DefinedTypeIDStart +  15, "light_info", LightInfoType},       GONE
//     {DefinedTypeIDStart +  16, "camera_info", CameraInfoType},     GONE

    {DefinedTypeIDStart +  17, "bone", BoneType},
    {DefinedTypeIDStart +  18, "skeleton", SkeletonType},
    {DefinedTypeIDStart +  19, "strings", StringType},
    {DefinedTypeIDStart +  20, "texture_mip_level", TextureMIPLevelType},
    {DefinedTypeIDStart +  21, "texture_image", TextureImageType},
    {DefinedTypeIDStart +  22, "texture", TextureType},
    {DefinedTypeIDStart +  23, "vector_track", VectorTrackType},
    {DefinedTypeIDStart +  24, "transform_track", TransformTrackType},
    {DefinedTypeIDStart +  25, "text_track_entry", TextTrackEntryType},
    {DefinedTypeIDStart +  26, "text_track", TextTrackType},
    {DefinedTypeIDStart +  27, "track_group", TrackGroupType},
    {DefinedTypeIDStart +  28, "tri_material_group", TriMaterialGroupType},
    {DefinedTypeIDStart +  29, "tri_annotation_set", TriAnnotationSetType},
    {DefinedTypeIDStart +  30, "tri_topology", TriTopologyType},
    {DefinedTypeIDStart +  31, "vertex_annotation_set", VertexAnnotationSetType},
    {DefinedTypeIDStart +  32, "vertex_data", VertexDataType},
};

END_GRANNY_NAMESPACE;

int32x GRANNY
GetDefinedTypeCount(void)
{
    return ArrayLength(DefinedTypes);
}
