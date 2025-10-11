#pragma once
#include "granny.h"
#include <QtXml>

extern bool g_littleEndian;
extern bool g_rotate90CCW;
extern bool g_s3tcTextures;
extern bool g_wiiTextures;
extern QDomDocument g_typesXml;
extern QDomDocument g_resourcesXml;

// Handy little utilities for dealing with ArrayOfRef members
#define AOR_ExtractCountAndPtr(Count, Pointer, Memory)          \
    __pragma(warning(push))                                     \
    __pragma(warning(disable:4127))                             \
    do {                                                    \
        Count   = *((granny_int32*)(Memory));                   \
        Pointer = *((void**)((granny_uint8*)Memory + 4));       \
    } while (false)                                             \
    __pragma(warning(pop))

bool HasUserProperties( granny_variant* extendedData, const char* userSearchString, char*& propertyString );
bool HasMadeUserProperties( granny_variant* extendedData, const char* propName );
bool CreateUserProperties( granny_variant* extendedData, const char* userSearchString, granny_memory_arena* TempArena );

void RenameAnimTrackGroups( granny_animation* animation, granny_memory_arena* TempArena, std::vector< const char* >* meshPropNames, const char* postfixChange );
void FindRenameAnimPropMeshes( granny_file_info* file_info, granny_memory_arena* TempArena, std::vector< const char* >* elementsToAddNames,
								const char* nameSuffixAdd, const char* searchUserString );
void RenameAnimPropModels( granny_file_info* file_info, granny_memory_arena* TempArena, 
							std::vector< const char* >* elementsToAddNames,	const char* nameSuffixAdd );
float FindStartTime( granny_file_info* file_info );

void RemoveNonAnimProps( granny_file_info* file_info );

void S3TCTextures( granny_file_info* file_info, granny_memory_arena* TempArena );
void WiiTextures( granny_file_info* file_info, granny_memory_arena* TempArena );
void WiiVertexStrip( granny_file_info* file_info, granny_memory_arena* TempArena );
void WiiMaterialStrip( granny_file_info* file_info, granny_memory_arena* TempArena );
void ForceProperS3TCTextureFormat( granny_file_info* file_info, granny_memory_arena* TempArena, std::vector< const char* >* texturesChanged = NULL );
void ChangeTextureOrderAlphabeticalAndLowercase( granny_file_info* file_info );
void RemoveTextureData( granny_file_info* file_info );

bool HasNormalSpecEmissiveMap( granny_mesh* mesh );
void CalcVertexFormat( granny_file_info* file_info, granny_memory_arena* TempArena );
void CreateLightmapAndBlendedMaterials( granny_file_info* file_info, granny_memory_arena* TempArena );