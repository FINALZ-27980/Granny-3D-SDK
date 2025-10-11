#if !defined(GRANNY_FILE_INFO_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_file_info.h $
// $DateTime: 2012/08/03 14:39:43 $
// $Change: 38655 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_DATA_TYPE_DEFINITION_H)
#include "granny_data_type_definition.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(FileInfoGroup);

struct animation;
struct art_tool_info;
struct exporter_info;
struct file;
struct material;
struct memory_arena;
struct mesh;
struct model;
struct skeleton;
struct texture;
struct track_group;
struct tri_topology;
struct vertex_data;

EXPTYPE struct file_info
{
    art_tool_info *ArtToolInfo;
    exporter_info *ExporterInfo;

    char const *FromFileName;

    // Material data
    int32 TextureCount;
    texture **Textures;

    int32 MaterialCount;
    material **Materials;

    // Model data
    int32 SkeletonCount;
    skeleton **Skeletons;

    int32 VertexDataCount;
    vertex_data **VertexDatas;

    int32 TriTopologyCount;
    tri_topology **TriTopologies;

    int32 MeshCount;
    mesh **Meshes;

    int32 ModelCount;
    model **Models;

    // Animation data
    int32 TrackGroupCount;
    track_group **TrackGroups;

    int32 AnimationCount;
    animation **Animations;

    // Extended data
    variant ExtendedData;
};
EXPCONST EXPGROUP(file_info) extern data_type_definition FileInfoType[];

EXPAPI GS_PARAM file_info *GetFileInfo(file &File);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_FILE_INFO_H
#endif
