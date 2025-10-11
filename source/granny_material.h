#if !defined(GRANNY_MATERIAL_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_material.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GRANNY_DATA_TYPE_DEFINITION_H)
#include "granny_data_type_definition.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(MaterialGroup);

struct texture;
struct material;

EXPTYPE struct material_map
{
    char const *Usage;
    material *Material;
};
EXPCONST EXPGROUP(material_map) extern data_type_definition MaterialMapType[];

EXPTYPE struct material
{
    char const *Name;

    int32 MapCount;
    material_map *Maps;

    texture *Texture;

    variant ExtendedData;
};
EXPCONST EXPGROUP(material) extern data_type_definition MaterialType[];

EXPTYPE enum material_texture_type
{
    UnknownTextureType,
    AmbientColorTexture,
    DiffuseColorTexture,
    SpecularColorTexture,
    SelfIlluminationTexture,
    OpacityTexture,
    BumpHeightTexture,
    ReflectionTexture,
    RefractionTexture,
    DisplacementTexture,

    OnePastLastMaterialTextureType,

    material_texture_type_forceint = 0x7fffffff
};
EXPAPI GS_READ texture *GetMaterialTextureByType(material const *Material,
                                                 material_texture_type Type);
EXPAPI GS_READ material *GetTexturedMaterialByChannelName(material const *Material,
                                                          char const *ChannelName);
EXPAPI GS_READ texture *GetMaterialTextureByChannelName(material const *Material,
                                                        char const *ChannelName);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MATERIAL_H
#endif
