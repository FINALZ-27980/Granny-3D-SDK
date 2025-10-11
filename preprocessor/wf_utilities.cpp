#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"
#include "wf_utilities.h"
#include "preprocessor.h"

#include <stdio.h>
#include <map>
#include <vector>
#include <string>

bool g_littleEndian = true;
bool g_rotate90CCW = false;
bool g_s3tcTextures = false;
bool g_wiiTextures = false;
QDomDocument g_typesXml;
QDomDocument g_resourcesXml;

using namespace std;

namespace
{
	enum { kPropType_Bool, kPropType_Int, kPropType_Float, kPropType_Vec, kPropType_Quat, kPropType_String, kPropType_Enum };
	struct BaseProperty
	{
		unsigned char type;
		QString name;
		std::vector< float > times;

		virtual unsigned GetValuesSize(void) { return 0; }
		virtual void* GetValues(void) { return NULL; }
		virtual unsigned GetCount(void) { return 0; }
	};

	template< typename T >
	struct Property : public BaseProperty
	{
		std::vector< T > values;

		virtual unsigned GetValuesSize(void) { return unsigned( sizeof( T ) * values.size() ); }
		virtual void* GetValues(void) { return &values[0]; }
		virtual unsigned GetCount(void) { return unsigned( values.size() ); }
	};

	struct Vec
	{
		float x,y,z;
	};

	struct Quat
	{
		float w,x,y,z;
	};

	unsigned& SwapEndian( unsigned& data )
	{
		if( !g_littleEndian )
		{
			data = ( (data & 0xFF000000) >> 24 ) | ( (data & 0x00FF0000) >> 8 ) | ( (data & 0x0000FF00) << 8 ) | ( (data & 0x000000FF) << 24 );
		}

		return data;
	}

	float& SwapEndian( float& data )
	{
		return reinterpret_cast< float& >( SwapEndian( reinterpret_cast< unsigned& >( data ) ) );
	}

	bool wfStringEqualsI( const char* a, const char* b )
	{
		for(;;)
		{
			if( tolower(*a) != tolower(*b) ) return false;
			if( *a == NULL && *b == NULL ) return true;
			if( *a == NULL || *b == NULL ) return false;
			++a;
			++b;
		}

		return false;
	}

	inline
	bool wfStringBeginsWithI( const char* string, const char* beginsWith )
	{
		for(;;)
		{
			if( *beginsWith == NULL )		return true;
			if( tolower(*string) != tolower(*beginsWith) ) return false;
			++string;
			++beginsWith;
		}

		return false;
	}

	inline
	bool wfStringContainsI( const char* string, const char* contains )
	{
		// this algo is naive: it would try even if matching is impossible, when strlen(string)<strlen(contains)
		for(;;)
		{
			if( *string == NULL )	return false;
			if( wfStringBeginsWithI( string, contains ) ) return true;
			++string;
		}

		return false;
	}

	inline void wfStringToLower( char* string )
	{
		for(;;)
		{
			if( *string == NULL )	return;

			*string = tolower( *string );
			++string;
		}
	}
}

bool HasUserProperties( granny_variant* extendedData, const char* userSearchString, char*& propertyString )
{
	granny_variant result;
	if( !GrannyFindMatchingMember( extendedData->Type, extendedData->Object, "UserDefinedProperties", &result ) )
		return false;

	granny_data_type_definition userPropType[] =
	{
		{ GrannyStringMember, "UserDefinedProperties" },
		{ GrannyEndMember }
	};

	propertyString = NULL;
	GrannyConvertSingleObject( result.Type, result.Object, userPropType, &propertyString, NULL );

	if( strstr( propertyString, userSearchString ) == NULL )
		return false;

	return true;
}

bool HasMadeUserProperties( granny_variant* extendedData, const char* propName )
{
	granny_variant result;
	if( !GrannyFindMatchingMember( extendedData->Type, extendedData->Object, propName, &result ) )
		return false;

	return true;
}

void ParseKeys( QDomNodeList& keyElements, QString& type, BaseProperty* newProperty, QStringList& enumNameList )
{
	for( int j = 0; j < keyElements.size(); ++j )
	{
		QDomElement keyElement = keyElements.at( j ).toElement();
		QString value = keyElement.attribute( "value" );
		QString time = keyElement.attribute( "time" );

		float fTime = time.toFloat();

		// Change to use wtConfig.xml type properties
		if( type.compare( "bool" ) == 0 )
		{
			Property<char>* ourProperty = ( Property<char>* )newProperty;
			bool bValue = value.compare( "true" ) == 0;

			if( fTime == 0.0f )
			{
				ourProperty->values[0] = bValue;
			}
			else
			{
				ourProperty->values.push_back( bValue );
				ourProperty->times.push_back( SwapEndian( fTime ) );
			}
		}
		else if( type.compare( "int" ) == 0 )
		{
			Property<int>* ourProperty = ( Property<int>* )newProperty;
			int iValue = value.toInt();

			if( fTime == 0.0f )
			{
				ourProperty->values[0] = SwapEndian( reinterpret_cast< unsigned& >( iValue ) );
			}
			else
			{
				ourProperty->values.push_back( SwapEndian( reinterpret_cast< unsigned& >( iValue ) ) );
				ourProperty->times.push_back( SwapEndian( fTime ) );
			}
		}
		else if( type.compare( "float" ) == 0 )
		{
			Property<float>* ourProperty = ( Property<float>* )newProperty;
			float fValue = value.toFloat();

			if( fTime == 0.0f )
			{
				ourProperty->values[0] = SwapEndian( fValue );
			}
			else
			{
				ourProperty->values.push_back( SwapEndian( fValue ) );
				ourProperty->times.push_back( SwapEndian( fTime ) );
			}
		}
		else if( type.compare( "wfVec" ) == 0 )
		{
			Property<Vec>* ourProperty = ( Property<Vec>* )newProperty;
			QStringList valueList = value.split( ",", QString::SkipEmptyParts );
			
			float fValueX = valueList[0].toFloat();
			float fValueY = valueList[1].toFloat();
			float fValueZ = valueList[2].toFloat();

			Vec vect;
			vect.x = SwapEndian( fValueX );
			vect.y = SwapEndian( fValueY );
			vect.z = SwapEndian( fValueZ );

			if( fTime == 0.0f )
			{
				ourProperty->values[0] = vect;
			}
			else
			{
				ourProperty->values.push_back( vect );
				ourProperty->times.push_back( SwapEndian( fTime ) );
			}
		}
		else if( type.compare( "wfQuat" ) == 0 )
		{
			Property<Quat>* ourProperty = ( Property<Quat>* )newProperty;
			QStringList valueList = value.split( ",", QString::SkipEmptyParts );
			
			float fValueW = valueList[0].toFloat();
			float fValueX = valueList[1].toFloat();
			float fValueY = valueList[2].toFloat();
			float fValueZ = valueList[3].toFloat();

			Quat quat;
			quat.x = SwapEndian( fValueX );
			quat.y = SwapEndian( fValueY );
			quat.z = SwapEndian( fValueZ );
			quat.w = SwapEndian( fValueW );

			if( fTime == 0.0f )
			{
				ourProperty->values[0] = quat;
			}
			else
			{
				ourProperty->values.push_back( quat );
				ourProperty->times.push_back( SwapEndian( fTime ) );
			}
		}
		else if( type.compare( "string" ) == 0 || type.compare( "wfFileName" ) == 0 || type.compare( "wfLuaFileName" ) == 0 )
		{
			Property<char>* ourProperty = ( Property<char>* )newProperty;
			
			for( int l = 0; l < value.size(); ++l )
			{
				ourProperty->values.push_back( value[l].toAscii() );
			}

			ourProperty->values.push_back( '\0' );
			ourProperty->times.push_back( SwapEndian( fTime ) );
		}
		else if( type.compare( "enum" ) == 0 )
		{
			Property<int>* ourProperty = ( Property<int>* )newProperty;
			int iValue = enumNameList.indexOf( value );

			if( fTime == 0.0f )
			{
				ourProperty->values[0] = SwapEndian( reinterpret_cast< unsigned& >( iValue ) );
			}
			else
			{
				ourProperty->values.push_back( SwapEndian( reinterpret_cast< unsigned& >( iValue ) ) );
				ourProperty->times.push_back( SwapEndian( fTime ) );
			}
		}
	}
}

bool CreateUserProperties( granny_variant* extendedData, const char* userSearchString, granny_memory_arena* TempArena )
{
	char* propertyString = NULL;
	if( !HasUserProperties( extendedData, userSearchString, propertyString ) )
		return false;

	QDomDocument xmlProperties;
	if( !xmlProperties.setContent( QString( propertyString + strlen( userSearchString ) ) ) )
	{
		return false;
	}

	std::vector< BaseProperty* > properties;

	QDomNodeList propertyElements = xmlProperties.elementsByTagName( "property" );

	for( int i = 0; i < propertyElements.size(); ++i )
	{
		BaseProperty* newProperty = NULL;

		QDomElement propElement = propertyElements.at( i ).toElement();
		QString type = propElement.attribute( "type" );
		QString subType = propElement.attribute( "subtype" );
		QString name = propElement.attribute( "name" );

		QDomNodeList keyElements = propElement.elementsByTagName( "key" );

		QStringList enumNameList;

		float defaultTime = 0.0f;
		bool ignoreDefaultTime = false;

		if( type.compare( "bool" ) == 0 )
		{
			Property<char>* ourProperty = new Property<char>();
			newProperty = ourProperty;

			const bool defaultValue = false;

			ourProperty->type = kPropType_Bool;
			ourProperty->values.push_back( defaultValue );
		}
		else if( type.compare( "int" ) == 0 )
		{
			Property<int>* ourProperty = new Property<int>();
			newProperty = ourProperty;

			int defaultValue = 0;

			ourProperty->type = kPropType_Int;
			ourProperty->values.push_back( SwapEndian( reinterpret_cast< unsigned& >( defaultValue ) ) );
		}
		else if( type.compare( "float" ) == 0 )
		{
			Property<float>* ourProperty = new Property<float>();
			newProperty = ourProperty;

			float defaultValue = 0;

			ourProperty->type = kPropType_Float;
			ourProperty->values.push_back( SwapEndian( defaultValue ) );
		}
		else if( type.compare( "wfVec" ) == 0 )
		{
			Property<Vec>* ourProperty = new Property<Vec>();
			newProperty = ourProperty;

			Vec vect = { 0.0f, 0.0f, 0.0f };

			ourProperty->type = kPropType_Vec;
			ourProperty->values.push_back( vect );
		}
		else if( type.compare( "wfQuat" ) == 0 )
		{
			Property<Quat>* ourProperty = new Property<Quat>();
			newProperty = ourProperty;

			Quat quat = { 0.0f, 0.0f, 0.0f, 0.0f };
			ourProperty->type = kPropType_Quat;
			ourProperty->values.push_back( quat );
		}
		else if( type.compare( "string" ) == 0 || type.compare( "wfFileName" ) == 0 || type.compare( "wfLuaFileName" ) == 0 )
		{
			Property<char>* ourProperty = new Property<char>();
			newProperty = ourProperty;

			ignoreDefaultTime = true;

			ourProperty->type = kPropType_String;
		}
		else if( type.compare( "enum" ) == 0 )
		{
			Property<int>* ourProperty = new Property<int>();
			newProperty = ourProperty;

			int defaultValue = 0;

			ourProperty->type = kPropType_Enum;
			ourProperty->values.push_back( SwapEndian( reinterpret_cast< unsigned& >( defaultValue ) ) );

			QDomElement& enumElements = g_typesXml.firstChildElement( "types" ).firstChildElement( "enums" );

			if( enumElements.isNull() )
				return false;

			// Get the enum list
			for( QDomElement enumElementIter = enumElements.firstChildElement( "enum" );
				!enumElementIter.isNull();
				enumElementIter = enumElementIter.nextSiblingElement( "enum" ) )
			{
				QString enumName = enumElementIter.attribute( "name" );

				if( enumName == subType )
				{
					enumNameList = enumElementIter.attribute( "value" ).split( "|", QString::SkipEmptyParts );
					break;
				}
			}
		}

		if( !ignoreDefaultTime )
		{
			newProperty->times.push_back( SwapEndian( defaultTime ) );
		}

		ParseKeys( keyElements, type, newProperty, enumNameList );
	
		if( newProperty )
		{
			newProperty->name = name;
			properties.push_back( newProperty );
		}
	}

	if( properties.size() )
	{
		unsigned memSize = sizeof( unsigned );
		for( std::vector< BaseProperty* >::iterator iter = properties.begin(); iter != properties.end(); ++iter )
		{
			unsigned tempSize = ( ( sizeof( unsigned char ) + (unsigned)(*iter)->name.size() + 1 + sizeof( unsigned char ) ) + 0x03 ) & ~0x03;
			memSize += tempSize;
			memSize += sizeof( float ) * (unsigned)(*iter)->times.size();
			memSize += sizeof( unsigned );
			memSize += ( (*iter)->GetValuesSize() + 0x03 ) & ~0x03;
		}

		char* newUserProperties = (char*)GrannyMemoryArenaPush( TempArena, memSize );
		char* baseUserProperties = newUserProperties;

		*( unsigned* )( newUserProperties ) = (unsigned)properties.size();
		SwapEndian( *( unsigned* )( newUserProperties ) );
		newUserProperties += sizeof( unsigned );

		for( std::vector< BaseProperty* >::iterator iter = properties.begin(); iter != properties.end(); ++iter )
		{
			char* currPropLocation = newUserProperties;
			*( unsigned char* )( newUserProperties ) = (*iter)->type;
			newUserProperties += sizeof( unsigned char );

			QByteArray asciiString = (*iter)->name.toAscii();
			strcpy( newUserProperties, asciiString.data() );
			newUserProperties += (*iter)->name.size() + 1;

			*( unsigned char* )( newUserProperties ) = (unsigned char)( (*iter)->times.size() );
			newUserProperties += sizeof( unsigned char );

			newUserProperties += ( ( ( newUserProperties - currPropLocation ) + 0x03 ) & ~0x03 ) - ( newUserProperties - currPropLocation );

			const unsigned timeSize = sizeof( float ) * (unsigned)(*iter)->times.size();
			memcpy( newUserProperties, &(*iter)->times[0], timeSize );
			newUserProperties += timeSize;

			const unsigned valueSize = (*iter)->GetValuesSize();
			*( unsigned* )( newUserProperties ) = valueSize;
			SwapEndian( *( unsigned* )( newUserProperties ) );
			newUserProperties += sizeof( unsigned );

			memcpy( newUserProperties, (*iter)->GetValues(), valueSize );
			newUserProperties += ( valueSize + 0x03 ) & ~0x03;
		}

		granny_data_type_definition* userPropType = ( granny_data_type_definition* )GrannyMemoryArenaPush( TempArena, sizeof( granny_data_type_definition ) * 2 );
		memset( userPropType, 0, sizeof( granny_data_type_definition ) * 2 );

		userPropType[0].Type = GrannyInt8Member;
		userPropType[0].Name = "wfProperties";
		userPropType[0].ArrayWidth = memSize;

		userPropType[1].Type = GrannyEndMember;

		// Nope, not right
		extendedData->Type = &userPropType[0];
		extendedData->Object = baseUserProperties;
	}

	while( properties.size() )
	{
		delete properties.back();
		properties.pop_back();
	}

	return true;
}

void RenameAnimTrackGroups( granny_animation* animation, granny_memory_arena* TempArena, std::vector< const char* >* meshPropNames, const char* postfixChange )
{
	for( granny_int32 i = 0; i < animation->TrackGroupCount; ++i )
	{
		granny_track_group* trackGroup = animation->TrackGroups[i];

		bool foundMeshName = false;
		if( meshPropNames )
		{
			for( std::vector< const char* >::iterator iter = meshPropNames->begin(); iter != meshPropNames->end(); ++iter )
			{
				if( strcmp( *iter, trackGroup->Name ) == 0 )
				{
					foundMeshName = true;
					break;
				}
			}
		}

		if( postfixChange && foundMeshName )
		{
			char nameChange[256] = { 0 };
			strcpy( nameChange, trackGroup->Name );
			strcat( nameChange, postfixChange );

			trackGroup->Name = GrannyMemoryArenaPushString( TempArena, &nameChange[0] );
		}
	}
}

void FindRenameAnimPropMeshes( granny_file_info* file_info, granny_memory_arena* TempArena, 
								std::vector< const char* >* elementsToAddNames,
								const char* nameSuffixAdd,
								const char* searchUserString )
{
	granny_int32x Offset = 0;

    granny_data_type_definition* CurrentType = GrannyFileInfoType;
    while( CurrentType->Type != GrannyEndMember )
    {
        if( _stricmp(CurrentType->Name, "Meshes") == 0 )
        {
            switch (CurrentType->Type)
            {
                case GrannyArrayOfReferencesMember:
                {
					granny_int32 Count;
					void* ArrayLocation;
					AOR_ExtractCountAndPtr( Count, ArrayLocation, ((granny_uint8*)file_info) + Offset );
					for( granny_int32x Idx  = 0; Idx < Count; ++Idx )
					{
						//bool haveObject = false;
						const char* newName = *reinterpret_cast< const char** >( ( (void**) ArrayLocation )[Idx] );
						const char* origName = newName;
						
						bool specialObject = false;
						if( searchUserString )
						{
							// For now assume this is a mesh
							granny_mesh* mesh = reinterpret_cast< granny_mesh* >( ( (void**) ArrayLocation )[Idx] );
							specialObject = CreateUserProperties( &mesh->ExtendedData, searchUserString, TempArena );

							if( elementsToAddNames && specialObject )
							{
								elementsToAddNames->push_back( origName );

								if( nameSuffixAdd )
								{
									char nameChange[256] = { 0 };
									strcpy( nameChange, newName );
									strcat( nameChange, nameSuffixAdd );

									newName = *reinterpret_cast< const char** >( ( (void**) ArrayLocation )[Idx] ) = GrannyMemoryArenaPushString( TempArena, &nameChange[0] );
								}
							}
						}			
					}
                }
				break;
            }

			break;
        }

        // Advance in the object, and the type array
        Offset += GrannyGetMemberTypeSize(CurrentType);
        ++CurrentType;
    }
}

void RenameAnimPropModels( granny_file_info* file_info, granny_memory_arena* TempArena, 
							std::vector< const char* >* elementsToAddNames,
							const char* nameSuffixAdd )
{
	granny_int32x Offset = 0;

    granny_data_type_definition* CurrentType = GrannyFileInfoType;
    while( CurrentType->Type != GrannyEndMember )
    {
        if( _stricmp(CurrentType->Name, "Models") == 0 )
        {
            switch (CurrentType->Type)
            {
                case GrannyArrayOfReferencesMember:
                {
					granny_int32 Count;
					void* ArrayLocation;
					AOR_ExtractCountAndPtr( Count, ArrayLocation, ((granny_uint8*)file_info) + Offset );
					for( granny_int32x Idx  = 0; Idx < Count; ++Idx )
					{
						//bool haveObject = false;
						const char* newName = *reinterpret_cast< const char** >( ( (void**) ArrayLocation )[Idx] );
						const char* origName = newName;

						if( elementsToAddNames )
						{
							bool foundName = false;
							for( std::vector< const char* >::iterator iter = elementsToAddNames->begin(); iter != elementsToAddNames->end(); ++iter )
							{
								if( strcmp( *iter, origName ) == 0 )
								{
									foundName = true;
									break;
								}
							}

							if( foundName && nameSuffixAdd )
							{
								char nameChange[256] = { 0 };
								strcpy( nameChange, newName );
								strcat( nameChange, nameSuffixAdd );

								newName = *reinterpret_cast< const char** >( ( (void**) ArrayLocation )[Idx] ) = GrannyMemoryArenaPushString( TempArena, &nameChange[0] );
							}
						}
					}
				}
			}
			break;
        }

        // Advance in the object, and the type array
        Offset += GrannyGetMemberTypeSize(CurrentType);
        ++CurrentType;
    }
}

float FindStartTime( granny_file_info* file_info )
{
	for( granny_int32 i = 0; i < file_info->ModelCount; ++i )
	{
		granny_model* model = file_info->Models[i];
		if( _stricmp( model->Name, "!!leveldata" ) == 0 )
		{
			granny_variant* extendedData = &model->Skeleton->Bones->ExtendedData;
			granny_variant result;
			if( !GrannyFindMatchingMember( extendedData->Type, extendedData->Object, "UserDefinedProperties", &result ) )
				return 0.0f;

			granny_data_type_definition userPropType[] =
			{
				{ GrannyStringMember, "UserDefinedProperties" },
				{ GrannyEndMember }
			};

			char* propertyString = NULL;
			GrannyConvertSingleObject( result.Type, result.Object, userPropType, &propertyString, NULL );

			propertyString = strstr( propertyString, "StartTime = " );
			if( propertyString == NULL )
				return 0.0f;

			propertyString += strlen( "StartTime = " );

			return (float)atof( propertyString );
		}
	}

	return 0.0f;
}

void RemoveNonAnimProps( granny_file_info* file_info )
{
	std::vector< granny_model* > models;
	std::vector< granny_mesh* > meshes;
	std::vector< granny_vertex_data* > vertexDatas;
	std::vector< granny_tri_topology* > topDatas;
	for( granny_int32 i = 0; i < file_info->ModelCount; ++i )
	{
		granny_model* model = file_info->Models[i];

		bool validModel = false;
		for( granny_int32 j = 0; j < model->MeshBindingCount; ++j )
		{
			granny_mesh* mesh = model->MeshBindings[j].Mesh;
			granny_variant variantResult;
			if( mesh->ExtendedData.Object && mesh->ExtendedData.Type
				&& GrannyFindMatchingMember( mesh->ExtendedData.Type, mesh->ExtendedData.Object, "wfProperties", &variantResult ) )
			{
				validModel = true;
				models.push_back( model );
				break;
			}
		}

		if( validModel )
		{
			for( granny_int32 j = 0; j < model->MeshBindingCount; ++j )
			{
				granny_mesh* mesh = model->MeshBindings[j].Mesh;
				meshes.push_back( mesh );

				vertexDatas.push_back( mesh->PrimaryVertexData );
				topDatas.push_back( mesh->PrimaryTopology );
			}
		}
	}

	file_info->ModelCount = models.size();
	for( int i = 0; i < file_info->ModelCount; ++i )
	{
		file_info->Models[i] = models[i];
	}

	file_info->MeshCount = meshes.size();
	for( int i = 0; i < file_info->MeshCount; ++i )
	{
		file_info->Meshes[i] = meshes[i];
	}

	file_info->VertexDataCount = vertexDatas.size();
	for( int i = 0; i < file_info->VertexDataCount; ++i )
	{
		file_info->VertexDatas[i] = vertexDatas[i];
	}

	file_info->TriTopologyCount = topDatas.size();
	for( int i = 0; i < file_info->TriTopologyCount; ++i )
	{
		file_info->TriTopologies[i] = topDatas[i];
	}
}

void S3TCTextures( granny_file_info* file_info, granny_memory_arena* TempArena )
{
	for( granny_int32 i = 0; i < file_info->TextureCount; ++i )
	{
		granny_texture* texture = file_info->Textures[i];

		if( texture->Encoding != GrannyS3TCTextureEncoding )
		{
			granny_s3tc_texture_format s3tcFormat = GrannyS3TCBGRA8888InterpolatedAlpha;
			const granny_int32x oldImageSize = GrannyGetRawImageSize( GrannyRGBA8888PixelFormat, texture->Width * 4, texture->Width, texture->Height );

			const granny_pixel_layout* s3tcLayout = GrannyGetS3TCPixelLayout( s3tcFormat );
			const granny_int32x stride = texture->Width * s3tcLayout->BytesPerPixel;

			void* oldImageMem = GrannyMemoryArenaPush( TempArena, oldImageSize );
			GrannyCopyTextureImage( texture, 0, 0, GrannyRGBA8888PixelFormat, texture->Width, texture->Height, texture->Width * 4, oldImageMem );

			char textureName[256] = { 0 };

			strcpy( &textureName[0], texture->FromFileName );

			granny_texture_builder* textureBuilder = GrannyBeginS3TCTexture( texture->Width, texture->Height, s3tcFormat );
			GrannyEncodeImage( textureBuilder, texture->Width, texture->Height, stride, texture->ImageCount, oldImageMem );
			file_info->Textures[i] = GrannyEndTexture( textureBuilder );
			file_info->Textures[i]->FromFileName = GrannyMemoryArenaPushString( TempArena, &textureName[0] );

			// Make sure materials are pointing at new texture
			for(int MatIdx = 0; MatIdx < file_info->MaterialCount; ++MatIdx)
			{
				granny_material* Material = file_info->Materials[MatIdx];
				if (!Material || !Material->Texture)
					continue;

				if (Material->Texture == texture)
				{
					Material->Texture = file_info->Textures[i];
				}
			}
		}
	}
}

static void Dxt5ToDxt1Block( unsigned int* pSrc, unsigned int* pDst )
{
	unsigned short* srcColors = ( unsigned short* )( pSrc + 2 );
	if( srcColors[0] > srcColors[1] )
	{
		pSrc += 2;
		*pDst = *pSrc; ++pDst; ++pSrc;
		*pDst = *pSrc; ++pDst; ++pSrc;
	}
	else
	{
		unsigned short* dstColors = ( unsigned short* )pDst;
		dstColors[0] = srcColors[1];
		dstColors[1] = srcColors[0];
		unsigned int* srcPixels = pSrc + 3;
		unsigned int* dstPixels = pDst + 1;
		*dstPixels = ~*srcPixels;
	}
}

static int Color565_DistSq( const unsigned int c0, const unsigned int c1 )
{
	const int r0 = (c0&0xF800 ) >> 11;
	const int r1 = (c1&0xF800 ) >> 11;
	const int g0 = (c0&0x7E0  ) >> 5;
	const int g1 = (c1&0x7E0  ) >> 5;
	const int b0 = (c0&0x1F   ) >> 0;
	const int b1 = (c1&0x1F   ) >> 0;
	
	const int r = r0 - r1;
	const int g = g0 - g1; // storing green in higher precision gives it more weight in color distance calculations -- a good thing
	const int b = b0 - b1;

	return r*r + g*g + b*b;
}

static unsigned short Color565_Blend( const unsigned int c0, const unsigned int c1, const unsigned int t )	// t == [0,12]
{
	unsigned int r0 = (c0&0xF800 ) >> 11;
	unsigned int r1 = (c1&0xF800 ) >> 11;
	unsigned int g0 = (c0&0x7E0  ) >> 5;
	unsigned int g1 = (c1&0x7E0  ) >> 5;
	unsigned int b0 = (c0&0x1F   ) >> 0;
	unsigned int b1 = (c1&0x1F   ) >> 0;

	const unsigned int t0 = t;
	const unsigned int t1 = 12 - t0;
	
	const unsigned int r = (r0*t0)/12 + (r1*t1)/12;
	const unsigned int g = (r0*t0)/12 + (r1*t1)/12;
	const unsigned int b = (r0*t0)/12 + (r1*t1)/12;
	
	//wtAssert( (r&~0x1F) == 0 );
	//wtAssert( (g&~0x3F) == 0 );
	//wtAssert( (b&~0x1F) == 0 );

	return (r<<11) | (g<<5) | (b<<0);
}

static unsigned int Dxt5Block_NumPartiallyTransparentPixels( unsigned int* pSrc )
{
	unsigned char* srcAlpha = ( unsigned char* )pSrc;
	unsigned int alphaTable[8] = { ( unsigned int )srcAlpha[0], ( unsigned int )srcAlpha[1] };
	if( alphaTable[0] > alphaTable[1] )
	{
		alphaTable[2] = ( unsigned int(alphaTable[0])*6 + unsigned int(alphaTable[1])*1 ) / 7;
		alphaTable[3] = ( unsigned int(alphaTable[0])*5 + unsigned int(alphaTable[1])*2 ) / 7;
		alphaTable[4] = ( unsigned int(alphaTable[0])*4 + unsigned int(alphaTable[1])*3 ) / 7;
		alphaTable[5] = ( unsigned int(alphaTable[0])*3 + unsigned int(alphaTable[1])*4 ) / 7;
		alphaTable[6] = ( unsigned int(alphaTable[0])*2 + unsigned int(alphaTable[1])*5 ) / 7;
		alphaTable[7] = ( unsigned int(alphaTable[0])*1 + unsigned int(alphaTable[1])*6 ) / 7;
	}
	else
	{
		alphaTable[2] = ( unsigned int(alphaTable[0])*4 + unsigned int(alphaTable[1])*1 ) / 5;
		alphaTable[3] = ( unsigned int(alphaTable[0])*3 + unsigned int(alphaTable[1])*2 ) / 5;
		alphaTable[4] = ( unsigned int(alphaTable[0])*2 + unsigned int(alphaTable[1])*3 ) / 5;
		alphaTable[5] = ( unsigned int(alphaTable[0])*1 + unsigned int(alphaTable[1])*4 ) / 5;
		alphaTable[6] = 0;
		alphaTable[7] = 255;
	}

	unsigned int numPartiallyTransparent = 0;
	unsigned long long* srcAlphaBlock = ( unsigned long long* )pSrc;
	unsigned int alphaShift = 16; // alpha block points to the alpha endpoint values as well, skip over those
	unsigned long long alphaMask = 0x7 << alphaShift;
	for( unsigned int i = 0; i != 16; ++i, alphaShift += 3, alphaMask <<= 3 )
	{
		const unsigned int alpha = alphaTable[ (*srcAlphaBlock&alphaMask)>>alphaShift ];
		if( alpha < 255 && alpha > 0 )
		{
			++numPartiallyTransparent;
		}
	}
	return numPartiallyTransparent;
}

static float Dxt5TransparentPercent( unsigned int* pSrc, unsigned numBlocks )
{
	float transparentCount = 0.0f;
	for( unsigned i = 0; i < numBlocks; ++i )
	{
		transparentCount += (float)Dxt5Block_NumPartiallyTransparentPixels( pSrc );
		pSrc += 4;
	}

	return transparentCount / ( 16.0f * numBlocks );
}

static void Dxt5Block_ToDxt1Block_Transparency( unsigned int* pSrc, unsigned int* pDst )
{
	unsigned char* srcAlpha = ( unsigned char* )pSrc;
	unsigned int alphaTable[8] = { ( unsigned int )srcAlpha[0], ( unsigned int )srcAlpha[1] };
	if( alphaTable[0] > alphaTable[1] )
	{
		alphaTable[2] = ( unsigned int(alphaTable[0])*6 + unsigned int(alphaTable[1])*1 ) / 7;
		alphaTable[3] = ( unsigned int(alphaTable[0])*5 + unsigned int(alphaTable[1])*2 ) / 7;
		alphaTable[4] = ( unsigned int(alphaTable[0])*4 + unsigned int(alphaTable[1])*3 ) / 7;
		alphaTable[5] = ( unsigned int(alphaTable[0])*3 + unsigned int(alphaTable[1])*4 ) / 7;
		alphaTable[6] = ( unsigned int(alphaTable[0])*2 + unsigned int(alphaTable[1])*5 ) / 7;
		alphaTable[7] = ( unsigned int(alphaTable[0])*1 + unsigned int(alphaTable[1])*6 ) / 7;
	}
	else
	{
		alphaTable[2] = ( unsigned int(alphaTable[0])*4 + unsigned int(alphaTable[1])*1 ) / 5;
		alphaTable[3] = ( unsigned int(alphaTable[0])*3 + unsigned int(alphaTable[1])*2 ) / 5;
		alphaTable[4] = ( unsigned int(alphaTable[0])*2 + unsigned int(alphaTable[1])*3 ) / 5;
		alphaTable[5] = ( unsigned int(alphaTable[0])*1 + unsigned int(alphaTable[1])*4 ) / 5;
		alphaTable[6] = 0;
		alphaTable[7] = 255;
	}

	unsigned long long* srcAlphaBlock = ( unsigned long long* )pSrc;
	bool hasAlpha = false;
	bool hasPartialAlpha = false;
	bool pixelIsTransparent[16];
	unsigned int alphaShift = 16; // alpha block points to the alpha endpoint values as well, skip over those
	unsigned long long alphaMask = 0x7 << alphaShift;
	for( unsigned int i = 0; i != 16; ++i, alphaShift += 3, alphaMask <<= 3 )
	{
		const unsigned int alpha = alphaTable[ (*srcAlphaBlock&alphaMask)>>alphaShift ];
		pixelIsTransparent[ i ] = false;
		if( alpha < 255 )
		{
			hasAlpha = true;
			if( alpha > 0 )
			{
				hasPartialAlpha = true;
			}
			pixelIsTransparent[ i ] = alpha <= 127;
		}
	}

	unsigned short* srcColors = ( unsigned short* )( pSrc + 2 );
	unsigned short* dstColors = ( unsigned short* )pDst;
	unsigned int* srcPixels = pSrc + 3;
	unsigned int* dstPixels = pDst + 1;
	
	if( hasAlpha )
	{
		*dstPixels = 0;
		unsigned int colorShift = 0;
		unsigned int colorMask = 0x3;
		unsigned int swapped[4] = { 0x1, 0x0, 0x2, 0x2 };
		if( srcColors[0] > srcColors[1] )
		{
			dstColors[0] = srcColors[1];
			dstColors[1] = srcColors[0];
		}
		else
		{
			swapped[0] = 0; swapped[1] = 1;
			dstColors[0] = srcColors[0];
			dstColors[1] = srcColors[1];
		}
		unsigned int dstPal[3] =
		{
			( unsigned int )dstColors[0],
			( unsigned int )dstColors[1],
			Color565_Blend( dstColors[0], dstColors[1], 6 )
		};
		unsigned int srcPal[4] =
		{
			( unsigned int )srcColors[0],
			( unsigned int )srcColors[1],
			Color565_Blend( srcColors[0], srcColors[1], 9 ),
			Color565_Blend( srcColors[0], srcColors[1], 3 )
		};
		for( unsigned int i = 0; i != 16; ++i, colorShift += 2, colorMask <<= 2 )
		{
			if( pixelIsTransparent[ i ] )
			{
				*dstPixels |= 0x3 << colorShift ;
			}
			else
			{
				const unsigned int srcPalIdx = (*srcPixels&colorMask)>>colorShift;
				unsigned int palIdx = swapped[ srcPalIdx ];
				if( srcPalIdx == 0x2 || srcPalIdx == 0x3 )
				{
					int minDist = INT_MAX;
					for( unsigned int j = 0; j != 3; ++j )
					{
						const int dist = Color565_DistSq( srcPal[ srcPalIdx ], dstPal[ j ] );
						if( dist < minDist )
						{
							minDist = dist;
							palIdx = j;
						}
					}
					//wfAssert( minDist < INT_MAX );
				}
				*dstPixels |= palIdx << colorShift;
			}
		}
	}
	else
	{
		if( srcColors[0] > srcColors[1] )
		{
			*( (unsigned long long*)dstColors ) = *( (unsigned long long*)srcColors );
		}
		else
		{
			dstColors[0] = srcColors[1];
			dstColors[1] = srcColors[0];
			*dstPixels = 0;
			unsigned int colorShift = 0;
			unsigned int colorMask = 0x3;
			const unsigned int swapped[4] = { 0x1, 0x0, 0x3, 0x2 };
			for( unsigned int i = 0; i != 16; ++i, colorShift += 2, colorMask <<= 2 )
			{
				*dstPixels |= swapped[ (*srcPixels&colorMask)>>colorShift ] << colorShift;
			}
		}
	}
}

void ForceProperS3TCTextureFormat( granny_file_info* file_info, granny_memory_arena* TempArena, std::vector< const char* >* texturesChanged )
{
	for( granny_int32 i = 0; i < file_info->MaterialCount; ++i )
	{
		granny_material* material = file_info->Materials[i];
		for( granny_int32 j = 0; j < material->MapCount; ++j )
		{
			const char* usage = material->Maps[j].Usage;
			granny_texture* texture = material->Maps[j].Material->Texture;
			if( !texture && material->Maps[j].Material->MapCount )
			{
				texture = material->Maps[j].Material->Maps[0].Material->Texture;
			}

			const bool normalOrSpecMap = wfStringEqualsI( usage, "Normal map" )
									|| wfStringEqualsI( usage, "Bump" )
									|| wfStringEqualsI( usage, "Specular Color" )
									|| wfStringEqualsI( usage, "Specular Level" );

			const bool diffuseMap = wfStringEqualsI( usage, "Diffuse color" )
									|| wfStringEqualsI( usage, "color" );

			// Check list of texture names that have changed and see if this one has already been changed
			if( texturesChanged && texture )
			{
				bool ignoreTexture = false;
				for( std::vector< const char* >::iterator iter = texturesChanged->begin(); iter != texturesChanged->end(); ++iter )
				{
					if( strcmp( *iter, texture->FromFileName ) == 0 )
					{
						ignoreTexture = true;
						break;
					}
				}

				if( ignoreTexture )
				{
					continue;
				}
				else
				{
					texturesChanged->push_back( texture->FromFileName );
				}
			}

			if( texture 
				&& ( normalOrSpecMap
					|| ( diffuseMap && texture->Encoding == GrannyS3TCTextureEncoding && texture->SubFormat != GrannyS3TCBGRA5551 && texture->ImageCount
						&& Dxt5TransparentPercent( (unsigned int*)texture->Images[0].MIPLevels[0].PixelBytes, ( texture->Images[0].MIPLevels[0].PixelByteCount / 2 ) / 8 ) < 0.05f )
					)
				)
			{
				granny_int32 fileTextureIndex = -1;
				for( granny_int32 k = 0; k < file_info->TextureCount; ++k )
				{
					if( file_info->Textures[k] == texture )
					{
						fileTextureIndex = k;
						break;
					}
				}

				if( fileTextureIndex >= 0 && ( texture->Encoding == GrannyS3TCTextureEncoding && texture->SubFormat != GrannyS3TCBGRA5551 ) )
				{
					const granny_pixel_layout* s3tcLayout = GrannyGetS3TCPixelLayout( GrannyS3TCBGRA5551 );

					granny_texture* newTexture = (granny_texture*)GrannyMemoryArenaPush( TempArena, sizeof( granny_texture ) );
					newTexture->Encoding = GrannyS3TCTextureEncoding;
					newTexture->FromFileName = GrannyMemoryArenaPushString( TempArena, texture->FromFileName );
					newTexture->Height = texture->Height;
					newTexture->Width = texture->Width;
					newTexture->ExtendedData.Object = newTexture->ExtendedData.Type = NULL;
					newTexture->ImageCount = 1;
					newTexture->Layout = *s3tcLayout;
					newTexture->SubFormat = GrannyS3TCBGRA5551;
					newTexture->TextureType = GrannyColorMapTextureType;

					newTexture->Images = (granny_texture_image*)GrannyMemoryArenaPush( TempArena, sizeof( granny_texture_image ) );
					newTexture->Images[0].MIPLevelCount = texture->Images[0].MIPLevelCount;
					newTexture->Images[0].MIPLevels = (granny_texture_mip_level*)GrannyMemoryArenaPush( TempArena, sizeof( granny_texture_mip_level ) * newTexture->Images[0].MIPLevelCount );
					for( granny_int32 mipLevel = 0; mipLevel < newTexture->Images[0].MIPLevelCount; ++mipLevel )
					{
						newTexture->Images[0].MIPLevels[ mipLevel ].Stride = texture->Images[0].MIPLevels[ mipLevel ].Stride / 2;
						newTexture->Images[0].MIPLevels[ mipLevel ].PixelByteCount = texture->Images[0].MIPLevels[ mipLevel ].PixelByteCount / 2;
						newTexture->Images[0].MIPLevels[ mipLevel ].PixelBytes = GrannyMemoryArenaPush( TempArena, newTexture->Images[0].MIPLevels[ mipLevel ].PixelByteCount );

						const int dxt1Size = texture->Images[0].MIPLevels[ mipLevel ].PixelByteCount / 2;
						const int numDxtBlocks = dxt1Size / 8;
						unsigned int* dxt5Src = (unsigned int*)texture->Images[0].MIPLevels[ mipLevel ].PixelBytes;
						unsigned int* dxt1Dst = (unsigned int*)newTexture->Images[0].MIPLevels[ mipLevel ].PixelBytes;

						for( int i = 0; i < numDxtBlocks; ++i )
						{
							if( normalOrSpecMap )
							{
								Dxt5ToDxt1Block( dxt5Src, dxt1Dst );
							}
							else
							{
								Dxt5Block_ToDxt1Block_Transparency( dxt5Src, dxt1Dst );
							}
							dxt5Src += 4;
							dxt1Dst += 2;
						}
					}

					file_info->Textures[ fileTextureIndex ] = newTexture;

					// Make sure materials are pointing at new texture
					for( int MatIdx = 0; MatIdx < file_info->MaterialCount; ++MatIdx )
					{
						granny_material* Material = file_info->Materials[MatIdx];
						if( !Material || !Material->Texture )
							continue;

						if( Material->Texture == texture )
						{
							Material->Texture = file_info->Textures[ fileTextureIndex ];
						}
					}
				}
			}
		}
	}
}

// These wii routines are duplicated in wtTextureUtils.cpp

static void FixWiiDXT1Word( unsigned short* data )
{
	unsigned short tmp;
	tmp = *data;

	// reverse tuple order within bytes
	*data = ( (tmp & 0x3 )   << 6 ) |
			( (tmp & 0xC )   << 2 ) |
			( (tmp & 0x30)   >> 2 ) |
			( (tmp & 0xC0)   >> 6 ) |

            ( (tmp & 0x300 ) << 6 ) |
			( (tmp & 0xC00 ) << 2 ) |
			( (tmp & 0x3000) >> 2 ) |
			( (tmp & 0xC000) >> 6 ) ;
}

static void SwizzleWiiDXT1Tile( unsigned char* srcPtrRaw, unsigned int width, unsigned int height, unsigned int tileX, unsigned int tileY, unsigned short* dstPtr)
{
	unsigned int  x, y;
	unsigned short  tmp;
	unsigned short* srcPtr;
	unsigned int  srcTileOffset;
	unsigned int  subTileRows, subRowShorts;    // number of s3 4x4 tiles
	unsigned int  srcPadWidth, srcPadHeight;
	unsigned short* buffPtr;

	// set the padded size of the s3 source image out to a 4-texel boundary
	srcPadWidth  = ( (width  + 3) >> 2 );
	srcPadHeight = ( (height + 3) >> 2 );

	// number of bytes in a single row of 4x4 texel source tiles
	srcTileOffset = srcPadWidth * 8;

	// number of 4x4 (source) tile rows to copy ( will be 1 or 2 )
	subTileRows = 2;
	if( (srcPadHeight - tileY) < 2 )
		subTileRows = 1;

	// number of 4x4 tile cols to copy translated into number of short values
	// ( will be 4 or 8 )
	subRowShorts = 8;
	if( (srcPadWidth - tileX) < 2 )
		subRowShorts = 4;

	for( y=0; y < subTileRows; y++ )
	{
		srcPtr  = (unsigned short*)( (unsigned char*)(srcPtrRaw) + ((tileY + y) * srcTileOffset) + (tileX*8) ); 
		buffPtr = ( dstPtr + (y * 8) );        // 16 bytes per subRow = 8 shorts

		// process one or both 4x4 row tiles at once- 4 short each
		for( x=0; x < subRowShorts; x++ )
		{			
			switch( x )
			{

			// color table entries - switch bytes within a 16-bit word only
			case 0:	
			case 1:
			case 4:
			case 5:
				{
				tmp = *srcPtr++;
				unsigned short swap = ( (tmp >> 8) | (tmp << 8) );
				*buffPtr++ = swap;
				}
				break;
			
			// 2-bit color tuples;
			// reverse tuple order within bytes of a word
			case 2:
			case 3:
			case 6:
			case 7:
				tmp = *srcPtr++;
				FixWiiDXT1Word( &tmp );
				*buffPtr++ = tmp;
				break;

			} // end switch
		} // end for( subRowShorts )			
	} // end for( subTileRows )
}

void SwizzleWiiDXT1( unsigned char* srcPtr, unsigned int width, unsigned int height, unsigned short* dstPtr)
{
	unsigned int tileRow, tileCol;
	unsigned int srcTileRows, srcTileCols;

	// each source tile is 4x4 texels, 8B
	srcTileRows   = ((height + 3) >> 2);
	srcTileCols   = ((width  + 3) >> 2);

	// each dst tile is 2x2 source tiles, so move by 2 each iteration
	for(tileRow = 0; tileRow < srcTileRows; tileRow += 2 )
	{
		for(tileCol = 0; tileCol < srcTileCols; tileCol += 2 )
		{
			SwizzleWiiDXT1Tile( srcPtr, width, height, tileCol, tileRow, dstPtr );
			dstPtr += 16; // 32B per dst tile, short ptr
		}
	}
}

void PreconvertSwizzleTextureWiiRGB5A3(unsigned int *pSrcData, unsigned int *pDstData, const unsigned int uWidth, const unsigned int uHeight)
{
	// Pre-swizzle the source data, wii uses 32 byte tiles, the pixel dimentions of a tile depends on the format
	unsigned int tileSizeX = 4;
	unsigned int tileSizeY = 4;						
	{
		unsigned int *swizzlewrite = pDstData;

		for (unsigned int ytile = 0; ytile < (uHeight / tileSizeY); ytile++)
		{
			unsigned int ytileOffset = ytile * tileSizeY * uWidth;
			for (unsigned int xtile = 0; xtile < (uWidth / tileSizeX); xtile++)
			{
				unsigned int xtileOffset = xtile * tileSizeY + ytileOffset;
				for (unsigned int ypix = 0; ypix < tileSizeY; ypix++)
				{
					unsigned int ypixOffset = ypix * uWidth + xtileOffset;
					for (unsigned int xpix = 0; xpix < tileSizeX; xpix++)
					{
						*swizzlewrite = pSrcData[ypixOffset + xpix];
						++swizzlewrite;
					}
				}
			}
		}
	}
}


static void DownconvertWiiToRGB5A3(unsigned int *pSrc, unsigned short *pDst, const unsigned int uWidth, const unsigned int uHeight)
{
	const unsigned int pixCount = uWidth * uHeight;
	for (unsigned int pixIdx = 0; pixIdx != pixCount; ++pixIdx)
	{
		unsigned int b = ( ( pSrc[ pixIdx ] >> 16 ) & 0xFF );
		unsigned int g = ( ( pSrc[ pixIdx ] >> 8 ) & 0xFF );
		unsigned int r = ( pSrc[ pixIdx ] & 0xFF );
		unsigned int a = ( pSrc[ pixIdx ] >> 24 );

		if (a == 255)
		{
			// opaque, 5 bit colors
			r >>= 3;
			g >>= 3;
			b >>= 3;
			unsigned short dstPix = ( 0x8000 | ( r << 10 ) | ( g << 5 ) | b );
			unsigned short swapped = ( (dstPix >> 8) | (dstPix << 8) );
			*pDst++ = swapped;
		}
		else
		{
			// transparent, 4 bit colors, 3 bit alpha
			r >>= 4;
			g >>= 4;
			b >>= 4;
			a >>= 5;
			unsigned short dstPix = ( ( a << 12 ) | ( r << 8 ) | ( g << 4 ) | b );
			unsigned short swapped = ( (dstPix >> 8) | (dstPix << 8) );
			*pDst++ = swapped;
		}
	}
}

// NCT_FIX clean up, replace with better scaling algorithm
static void HackDownscale8888( const int srcWidth, const int srcHeight, const int dstWidth, const int dstHeight, const unsigned int* pSrc, const unsigned int* pDst )
{
	const int intBump = ( srcHeight / dstHeight ) * srcWidth;
	const int fracBump = srcHeight % dstHeight;
	const int pixelInt = srcWidth / dstWidth;
	const int pixelFrac = srcWidth & dstWidth;
	unsigned int *src = const_cast< unsigned int* >( pSrc );
	unsigned int *dst = const_cast< unsigned int* >( pDst );
	int lineCount = dstHeight;
	int accum = 0;
	
	while ( lineCount-- > 0 )
	{
		int pixelCount = dstWidth;
		int pixelAccum = 0;
		unsigned int *lineSrc = src;
		unsigned int *lineDst = dst;
		
		while ( pixelCount-- > 0 )
		{
			*lineDst++ = *lineSrc;
			lineSrc += pixelInt;
			pixelAccum += pixelFrac;
			if ( pixelAccum >= dstWidth )
			{
				pixelAccum -= dstWidth;
				lineSrc++;
			}
		}
		
		dst += dstWidth;
		src += intBump;
		accum += fracBump;
		if ( accum >= dstHeight )
		{
			accum -= dstHeight;
			src += srcWidth;
		}
	}
}

static granny_data_type_definition* WiiRemoveTangentsFromType(granny_data_type_definition* Type, granny_memory_arena* Arena)
{
    int const MemberCount = GrannyGetTotalTypeSize(Type) / sizeof(granny_data_type_definition);

    granny_data_type_definition* NewType = PushArray(Arena, MemberCount, granny_data_type_definition);
    granny_int32x NewTypeSize = 0;
    {for (int Idx = 0; Idx < MemberCount; ++Idx)
    {
        if (Type[Idx].Name && (_strnicmp(Type[Idx].Name,
                                         GrannyVertexTangentName,
                                         strlen(GrannyVertexTangentName)) == 0 ||
                               _strnicmp(Type[Idx].Name,
                                         GrannyVertexBinormalName,
                                         strlen(GrannyVertexBinormalName)) == 0 ||
                               _strnicmp(Type[Idx].Name,
                                         GrannyVertexTangentBinormalCrossName,
                                         strlen(GrannyVertexTangentBinormalCrossName)) == 0))
        {
            // don't copy that one.
        }
        else
        {
            NewType[NewTypeSize++] = Type[Idx];
        }
    }}
    pp_assert(NewType[NewTypeSize-1].Type == GrannyEndMember);

    return NewType;
}

struct SCompVertex
{
    int VertSize;
    SCompVertex(int size) : VertSize(size) { }
    SCompVertex();

    bool operator()(granny_uint8 const* One,
                    granny_uint8 const* Two) const
    {
        return memcmp(One, Two, VertSize) < 0;
    }
};

static granny_vertex_data* WiiStripTangentsFromBuffer(granny_vertex_data*  VertexData,
                        vector<int>&         RemappedIndex,
                        vector<int>&         SourceIndices,
                        granny_memory_arena* Arena)
{
    granny_data_type_definition* NewType = WiiRemoveTangentsFromType(VertexData->VertexType, Arena);

    granny_int32x NewVertexSize  = GrannyGetTotalObjectSize(NewType);
    vector<granny_uint8> VertCopy(VertexData->VertexCount * NewVertexSize);

    GrannyConvertVertexLayouts(VertexData->VertexCount,
                               VertexData->VertexType,
                               VertexData->Vertices,
                               NewType, &VertCopy[0]);

    typedef map< granny_uint8*, int, SCompVertex > vertex_map;
    SCompVertex VC(NewVertexSize);
    vertex_map VertMap( VC );

    {for (int Idx = 0; Idx < VertexData->VertexCount; ++Idx)
    {
        vertex_map::iterator Itr = VertMap.find(&VertCopy[Idx * NewVertexSize]);
        if (Itr == VertMap.end())
            VertMap[&VertCopy[Idx * NewVertexSize]] = Idx;
    }}

    granny_int32x NewVertexCount = 0;
    granny_uint8* StrippedVerts = PushArray(Arena, (VertexData->VertexCount * NewVertexSize), granny_uint8);

    SourceIndices.clear();
    SourceIndices.resize(VertexData->VertexCount, -1);
    {for (vertex_map::iterator Itr = VertMap.begin(); Itr != VertMap.end(); ++Itr)
    {
        granny_uint8* Dest = StrippedVerts + (NewVertexSize * NewVertexCount);

        SourceIndices[Itr->second] = NewVertexCount++;
        memcpy(Dest, Itr->first, NewVertexSize);
    }}

    RemappedIndex.resize(VertexData->VertexCount);
    {for (int Idx = 0; Idx < VertexData->VertexCount; ++Idx)
    {
        RemappedIndex[Idx] = SourceIndices[VertMap[&VertCopy[Idx * NewVertexSize]]];
        pp_assert(RemappedIndex[Idx] != -1);
    }}

    granny_vertex_data* VertData = PushObject(Arena, granny_vertex_data);
    VertData->VertexType = NewType;
    VertData->VertexCount = NewVertexCount;
    VertData->Vertices = StrippedVerts;
    VertData->VertexComponentNameCount = 0;
    VertData->VertexComponentNames = 0;
    VertData->VertexAnnotationSetCount = 0;
    VertData->VertexAnnotationSets = 0;

    return VertData;
}


static granny_vertex_data* WiiRemoveMorphTangents(granny_vertex_data*  VertexData,
                    vector<int>&         UsedVerts,
                    granny_memory_arena* Arena)
{
    granny_data_type_definition* NewType = WiiRemoveTangentsFromType(VertexData->VertexType, Arena);
    granny_int32x const NewSize = GrannyGetTotalObjectSize(NewType);
    pp_assert(NewSize <= GrannyGetTotalTypeSize(VertexData->VertexType));

    granny_int32x NewVertexSize  = GrannyGetTotalObjectSize(NewType);
    vector<granny_uint8> VertCopy(VertexData->VertexCount * NewVertexSize);
    GrannyConvertVertexLayouts(VertexData->VertexCount,
                               VertexData->VertexType,
                               VertexData->Vertices,
                               NewType, &VertCopy[0]);

    granny_int32x NewVertexCount = 0;
    {for (int VertIdx = 0; VertIdx < VertexData->VertexCount; ++VertIdx)
    {
        if (UsedVerts[VertIdx] == -1)
            continue;

        granny_uint8*       Dest   = VertexData->Vertices + (NewSize * UsedVerts[VertIdx]);
        granny_uint8 const* Source = &VertCopy[NewSize * VertIdx];
        memcpy(Dest, Source, NewSize);
        ++NewVertexCount;
    }}

    granny_vertex_data* VertData = PushObject(Arena, granny_vertex_data);
    VertData->VertexType  = NewType;
    VertData->VertexCount = NewVertexCount;
    VertData->Vertices    = VertexData->Vertices;
    VertData->VertexComponentNameCount = 0;
    VertData->VertexComponentNames = 0;
    VertData->VertexAnnotationSetCount = 0;
    VertData->VertexAnnotationSets = 0;

    return VertData;
}

static granny_mesh* WiiStripTangentsForMesh(granny_mesh* OriginalMesh,
                   granny_memory_arena* Arena)
{
    // Normalize the verts first.  What we want is a mesh with no tangents or binormal
    // components, with morph targets that are mapped in the same way.  (Note that
    // stripping the tangents means that we have to re-unify split verts, so we have to be
    // careful to replicate the mapping from the base mesh across all the morphs.)
    {
        vector<int> Remapped;
        vector<int> Used;
        OriginalMesh->PrimaryVertexData = WiiStripTangentsFromBuffer(OriginalMesh->PrimaryVertexData, Remapped, Used, Arena);

        {for (int MorphIdx = 0; MorphIdx < OriginalMesh->MorphTargetCount; ++MorphIdx)
        {
            // We have to be a bit careful to remove exactly the same verts
            OriginalMesh->MorphTargets[MorphIdx].VertexData =
                WiiRemoveMorphTangents(OriginalMesh->MorphTargets[MorphIdx].VertexData, Used, Arena);
        }}

        // TODO: 16-bit fix
        for (int i = 0; i < OriginalMesh->PrimaryTopology->IndexCount; ++i)
            OriginalMesh->PrimaryTopology->Indices[i] = Remapped[OriginalMesh->PrimaryTopology->Indices[i]];
    }
	return OriginalMesh;
}

void WiiVertexStrip( granny_file_info* file_info, granny_memory_arena* TempArena )
{
	// Code c&p and modified from generate_tangents.cpp, we want to strip the binormal/tangents out for wii
	// since I don't think we'll have the headroom to make use of them and that's 12 bytes per vert!
    {for (int MeshIdx = 0; MeshIdx < file_info->MeshCount; ++MeshIdx)
    {
        granny_mesh* Mesh = file_info->Meshes[MeshIdx];
        if (!Mesh)
            continue;
		if (!Mesh->PrimaryVertexData)
			continue;
		if (Mesh->PrimaryVertexData->VertexCount == 0)
			continue;

        file_info->Meshes[MeshIdx] = WiiStripTangentsForMesh(Mesh, TempArena);
    }}
}

// Make sure even if a material seems to be something we want to strip, that
// we don't strip it if it's linked to a diffuse or glow texture name, I'm looking
// at you skybox! Probably mistagged materials, but I suspect getting art fixed is
// more work than this...
// Oh, and try not to strip lightmaps...

static bool checkForDontStripOverrideName( const char* baseName )
{
	bool overrideStrip = false;

	char tempString[1024];
	strcpy(tempString, baseName);
	size_t stringLength = strlen(tempString);
	for (unsigned int i = 0; i < stringLength; i++)
	{
		tempString[i] = tolower(tempString[i]);
	}

	if (strstr(tempString, "_diffuse"))
	{
		overrideStrip = true;
	}
	else if (strstr(tempString, "_glow"))
	{
		overrideStrip = true;
	}
	else if (strstr(tempString, "_lm"))
	{
		overrideStrip = true;
	}

	return overrideStrip;
}

static bool strippableName( const char* baseName )
{
	char tempString[1024];
	strcpy(tempString, baseName);
	size_t stringLength = strlen(tempString);
	for (unsigned int i = 0; i < stringLength; i++)
	{
		tempString[i] = tolower(tempString[i]);
	}

	bool strip = false;
	if (strstr(tempString, "_normal"))
	{
		strip = true;
	}
	else if (strstr(tempString, "_spec"))
	{
		strip = true;
	}
	else if (strstr(tempString, "_emiss"))
	{
		strip = true;
	}
	else if (strstr(tempString, "_bump."))
	{
		strip = true;
	}
	else if (strstr(tempString, "_bumpmap"))
	{
		strip = true;
	}

	return strip;
}

void WiiMaterialStrip( granny_file_info* file_info, granny_memory_arena* TempArena )
{
	std::list<granny_texture *> stripTextureList;
	std::list<granny_material *> stripMaterialList;
	std::list<granny_material_map *> stripMaterialMapList;

	for(int MatIdx = 0; MatIdx < file_info->MaterialCount; ++MatIdx)
	{
		granny_material* Material = file_info->Materials[MatIdx];
		if (!Material)
			continue;

		if ( Material->Texture )
		{
			bool stripIt = false;
			if ( Material->Texture->FromFileName )
			{
				stripIt = strippableName( Material->Texture->FromFileName );
				if ( stripIt )
				{
					stripTextureList.push_front( Material->Texture );
					stripMaterialList.push_front( Material );
				}
			}
		}

		for( int mapIdx = 0; mapIdx < Material->MapCount; ++mapIdx )
		{
			granny_material_map& map = Material->Maps[ mapIdx ];

			bool stripIt = false;
			if( map.Material->Texture )
			{
				if ( map.Material->Texture->FromFileName )
				{
					stripIt = strippableName( map.Material->Texture->FromFileName );
					if ( stripIt )
					{
						stripTextureList.push_front( map.Material->Texture );
						stripMaterialList.push_front( map.Material );
//						stripMaterialMapList.push_front( &Material->Maps[ mapIdx ] );
					}
				}
			}
		}
	}

	for( int i = 0; i < file_info->MeshCount; ++i )
	{
		granny_mesh* mesh = file_info->Meshes[i];

		for( int matIdx = 0; matIdx < mesh->MaterialBindingCount; ++matIdx )
		{
			granny_material* Material = mesh->MaterialBindings[ matIdx ].Material;
			if (!Material)
				continue;
//	for(int MatIdx = 0; MatIdx < file_info->MaterialCount; ++MatIdx)
//	{
//			granny_material* Material = file_info->Materials[MatIdx];
//			if (!Material || !Material->Texture)
//				continue;

			int strippedMapCount = 0;
			for( int mapIdx = 0; mapIdx < Material->MapCount; ++mapIdx )
			{
				granny_material_map& map = Material->Maps[ mapIdx ];

				if( map.Material->Texture )
				{
					bool overrideStripCheck = checkForDontStripOverrideName( map.Material->Texture->FromFileName );
					if ( !overrideStripCheck )
					{
						if ( !(map.Material->Name && wfStringEqualsI( map.Material->Name, "lightmap" )) )
						{
							if( wfStringEqualsI( map.Usage, "Normal map" ) || 
								wfStringEqualsI( map.Usage, "Bump" ) || 
								wfStringEqualsI( map.Usage, "Specular Color" ) || 
								wfStringEqualsI( map.Usage, "Specular Level" ) || 
								wfStringEqualsI( map.Usage, "Self-Illumination" ) )
							{
//								stripMaterialMapList.push_front( &Material->Maps[ mapIdx ] );
								stripMaterialList.push_front( map.Material );
								stripTextureList.push_front( map.Material->Texture );
								++strippedMapCount;
							}
						}
					}
				}
				else if( map.Material->MapCount && map.Material->Maps[ 0 ].Material->Texture )
				{
					bool overrideStripCheck = checkForDontStripOverrideName( map.Material->Maps[ 0 ].Material->Texture->FromFileName );
					if ( !overrideStripCheck )
					{
						if ( !(map.Material->Maps[ 0 ].Material->Name && wfStringEqualsI( map.Material->Maps[ 0 ].Material->Name, "lightmap" )) )
						{
							if( wfStringEqualsI( map.Usage, "Normal map" ) || wfStringEqualsI( map.Usage, "Bump" ) )
							{
//								stripMaterialMapList.push_front( &Material->Maps[ mapIdx ] );
								stripMaterialList.push_front( map.Material );
								stripMaterialList.push_front( map.Material->Maps[ 0 ].Material );
								stripTextureList.push_front( map.Material->Maps[ 0 ].Material->Texture );
								++strippedMapCount;
							}
						}
					}
				}
			}
			if ( strippedMapCount == Material->MapCount )
			{
				stripMaterialList.push_front( Material );
			}
		}
	}

	// Strip materials out of maps
	for(int MatIdx = 0; MatIdx < file_info->MaterialCount; ++MatIdx)
	{
		granny_material* Material = file_info->Materials[MatIdx];
		if (!Material)
			continue;

		int newMapEntryCount = 0;

		for( int mapIdx = 0; mapIdx < Material->MapCount; ++mapIdx )
		{
			granny_material_map& map = Material->Maps[ mapIdx ];
			
			if( map.Material )
			{
				bool inMaterialList = false;
				for (std::list<granny_material *>::iterator matIter = stripMaterialList.begin(); matIter != stripMaterialList.end(); matIter++)
				{
					if ( ( *matIter ) == map.Material )
					{
						inMaterialList = true;
						break;
					}
				}
				if (!inMaterialList)
				{
					++newMapEntryCount;
				}
			}
		}

		if ( newMapEntryCount )
		{
			granny_material_map* newMaps = (granny_material_map*)GrannyMemoryArenaPush( TempArena, sizeof( granny_material_map ) * newMapEntryCount );
			unsigned int insertMapEntry = 0;

			for( int mapIdx = 0; mapIdx < Material->MapCount; ++mapIdx )
			{
				granny_material_map& map = Material->Maps[ mapIdx ];

				if( map.Material )
				{
					bool inMaterialList = false;
					for (std::list<granny_material *>::iterator matIter = stripMaterialList.begin(); matIter != stripMaterialList.end(); matIter++)
					{
						if ( ( *matIter ) == map.Material )
						{
							inMaterialList = true;
							break;
						}
					}
					if (!inMaterialList)
					{
						memcpy( &newMaps[insertMapEntry++], &map, sizeof(granny_material_map) );
					}
				}
			}
			Material->MapCount = newMapEntryCount;
			Material->Maps = newMaps;
		}
		else
		{
			stripMaterialMapList.push_front( Material->Maps );
			Material->MapCount = 0;
			Material->Maps = NULL;
			if (Material->Texture == NULL)
			{
				stripMaterialList.push_front( Material );
			}
		}
	}

	// Strip the orphaned maps
	for( int i = 0; i < file_info->MeshCount; ++i )
	{
		granny_mesh* mesh = file_info->Meshes[i];

		for( int matIdx = 0; matIdx < mesh->MaterialBindingCount; ++matIdx )
		{
			granny_material* Material = mesh->MaterialBindings[ matIdx ].Material;
			if (!Material)
				continue;
//	for(int MatIdx = 0; MatIdx < file_info->MaterialCount; ++MatIdx)
//	{
//			granny_material* Material = file_info->Materials[MatIdx];
//			if (!Material || !Material->Texture)
//				continue;

			int newMapCount = 0;
			for( int mapIdx = 0; mapIdx < Material->MapCount; ++mapIdx )
			{
				granny_material_map* map = &Material->Maps[ mapIdx ];

				bool inMapList = false;
				for (std::list<granny_material_map *>::iterator mapIter = stripMaterialMapList.begin(); mapIter != stripMaterialMapList.end(); mapIter++)
				{
					if ( ( *mapIter ) == map )
					{
						inMapList = true;
						break;
					}
				}
				if (!inMapList)
				{
					++newMapCount;
				}
			}

			if (newMapCount)
			{
				granny_material_map* newMaps = (granny_material_map*)GrannyMemoryArenaPush( TempArena, sizeof( granny_material_map ) * newMapCount );
				unsigned int insertMap = 0;

				for( int mapIdx = 0; mapIdx < Material->MapCount; ++mapIdx )
				{
					granny_material_map* map = &Material->Maps[ mapIdx ];

					bool inMapList = false;
					for (std::list<granny_material_map *>::iterator mapIter = stripMaterialMapList.begin(); mapIter != stripMaterialMapList.end(); mapIter++)
					{
						if ( ( *mapIter ) == map )
						{
							inMapList = true;
							break;
						}
					}
					if (!inMapList)
					{
						memcpy( &newMaps[insertMap++], map, sizeof(granny_material_map) );
					}
				}
				Material->MapCount = newMapCount;
				Material->Maps = newMaps;
			}
			else
			{
				Material->MapCount = 0;
				Material->Maps = NULL;
			}
		}
	}

	// Strip the orphaned materials from mesh material mappings
	for( int i = 0; i < file_info->MeshCount; ++i )
	{
		granny_mesh* mesh = file_info->Meshes[i];

		int newBindingCount = 0;

		for( int matIdx = 0; matIdx < mesh->MaterialBindingCount; ++matIdx )
		{
			granny_material* material = mesh->MaterialBindings[ matIdx ].Material;
			bool inMaterialList = false;

			for (std::list<granny_material *>::iterator matIter = stripMaterialList.begin(); matIter != stripMaterialList.end(); matIter++)
			{
				if ( ( *matIter ) == material )
				{
					inMaterialList = true;
					break;
				}
			}
			if (!inMaterialList)
			{
				++newBindingCount;
			}
		}

		if (newBindingCount)
		{
			granny_material_binding* newBindings = (granny_material_binding*)GrannyMemoryArenaPush( TempArena, sizeof( granny_material_binding ) * newBindingCount );
			unsigned int insertBinding = 0;

			for( int matIdx = 0; matIdx < mesh->MaterialBindingCount; ++matIdx )
			{
				granny_material* material = mesh->MaterialBindings[ matIdx ].Material;
				bool inMaterialList = false;

				for (std::list<granny_material *>::iterator matIter = stripMaterialList.begin(); matIter != stripMaterialList.end(); matIter++)
				{
					if ( ( *matIter ) == material )
					{
						inMaterialList = true;
						break;
					}
				}
				if (!inMaterialList)
				{
					memcpy( &newBindings[insertBinding++], &mesh->MaterialBindings[ matIdx ], sizeof(granny_material_binding) );
				}
			}

			mesh->MaterialBindingCount = newBindingCount;
			mesh->MaterialBindings = newBindings;
		}
		else
		{
			mesh->MaterialBindingCount = 0;
			mesh->MaterialBindings = NULL;
		}
	}

	// Strip the orphaned materials from the material list
	int newMaterialCount = 0;
	for(int MatIdx = 0; MatIdx < file_info->MaterialCount; ++MatIdx)
	{
		granny_material* Material = file_info->Materials[MatIdx];
		if (!Material || !Material->Texture)
			continue;

		bool inMaterialList = false;

		for (std::list<granny_material *>::iterator matIter = stripMaterialList.begin(); matIter != stripMaterialList.end(); matIter++)
		{
			if ( ( *matIter ) == Material )
			{
				inMaterialList = true;
				break;
			}
		}
		if (!inMaterialList)
		{
			++newMaterialCount;
		}
	}

	if ( newMaterialCount )
	{
		granny_material** newMaterials = (granny_material**)GrannyMemoryArenaPush( TempArena, sizeof( granny_material* ) * newMaterialCount );
		unsigned int insertMaterial = 0;

		for(int MatIdx = 0; MatIdx < file_info->MaterialCount; ++MatIdx)
		{
			granny_material* Material = file_info->Materials[MatIdx];
			if (!Material || !Material->Texture)
				continue;

			bool inMaterialList = false;

			for (std::list<granny_material *>::iterator matIter = stripMaterialList.begin(); matIter != stripMaterialList.end(); matIter++)
			{
				if ( ( *matIter ) == Material )
				{
					inMaterialList = true;
					break;
				}
			}
			if (!inMaterialList)
			{
				newMaterials[ insertMaterial++ ] = Material;
			}
		}

		file_info->MaterialCount = newMaterialCount;
		file_info->Materials = newMaterials;
	}
	else
	{
		file_info->MaterialCount = 0;
		file_info->Materials = NULL;
	}

	// Strip the normal/bump/spec/emissive textures
	int newTextureCount = 0;
	for( granny_int32 i = 0; i < file_info->TextureCount; ++i )
	{
		granny_texture* texture = file_info->Textures[i];
		bool inPurgeList = false;
		for (std::list<granny_texture *>::iterator texIter = stripTextureList.begin(); texIter != stripTextureList.end(); texIter++)
		{
			if ( ( *texIter ) == texture )
			{
				inPurgeList = true;
				break;
			}
		}
		if (!inPurgeList)
		{
			++newTextureCount;
		}
	}

	if ( newTextureCount )
	{
		granny_texture** newTextures = (granny_texture**)GrannyMemoryArenaPush( TempArena, sizeof( granny_texture* ) * newTextureCount );
		unsigned int insertTexture = 0;
		for( granny_int32 i = 0; i < file_info->TextureCount; ++i )
		{
			granny_texture* texture = file_info->Textures[i];
			bool inPurgeList = false;
			for (std::list<granny_texture *>::iterator texIter = stripTextureList.begin(); texIter != stripTextureList.end(); texIter++)
			{
				if ( ( *texIter ) == texture )
				{
					inPurgeList = true;
					break;
				}
			}
			if (!inPurgeList)
			{
				newTextures[ insertTexture++ ] = texture;
			}
		}

		file_info->TextureCount = newTextureCount;
		file_info->Textures = newTextures;
	}
	else
	{
		file_info->TextureCount = 0;
		file_info->Textures = NULL;
	}
}

enum WiiTexFormat
{
	WIITEXFMT_RGB5A3 = 0,
	WIITEXFMT_DXT1_565,
	WIITEXFMT_DXT1_5551
};

static WiiTexFormat DetermineAlpha( granny_texture* texture, void* imageMem )
{
	WiiTexFormat returnFormat = WIITEXFMT_DXT1_565;

	bool hasAlpha = GrannyTextureHasAlpha(texture);
	if ( hasAlpha )
	{
		bool has00 = false;
		bool hasFF = false;
		bool hasOther = false;
		// Check to see if we *really* have alpha, or if it's all just 0/255, 1-bit is fine for DXT1

		const unsigned int pixCount = texture->Width * texture->Height;
		unsigned int* pSrc = reinterpret_cast< unsigned int* >( imageMem );
		for (unsigned int pixIdx = 0; pixIdx != pixCount; ++pixIdx)
		{
			unsigned int a = ( pSrc[ pixIdx ] >> 24 );
			if ( a == 255 )
			{
				hasFF = true;
			}
			else if ( a == 0 )
			{
				has00 = true;
			}
			else
			{
				hasOther = true;
				break;
			}
		}

		if ( hasOther )
		{
			// real alpha channel
			returnFormat = WIITEXFMT_RGB5A3;
		}
		else if ( has00 && hasFF )
		{
			// 1 bit alpha
			returnFormat = WIITEXFMT_RGB5A3; //WIITEXFMT_DXT1_5551;
		}
	}

	return returnFormat;
}

void WiiTextures( granny_file_info* file_info, granny_memory_arena* TempArena )
{
	for( granny_int32 i = 0; i < file_info->TextureCount; ++i )
	{
		bool lightmapTexture = false;
		granny_texture* texture = file_info->Textures[i];
		if (texture->FromFileName)
		{
			char lowerName[1024];
			strcpy(lowerName, texture->FromFileName);
			int lowerOffset = 0;
			while (lowerName[lowerOffset])
			{
				lowerName[lowerOffset] = tolower(lowerName[lowerOffset]);
				++lowerOffset;
			}
			if ( strstr(lowerName, "_lm") || strstr(lowerName, "_lightmap") )
			{
				lightmapTexture = true;
			}
		}

		const granny_int32x oldImageSize = GrannyGetRawImageSize( GrannyRGBA8888PixelFormat, texture->Width * 4, texture->Width, texture->Height );
		void* oldImageMem = GrannyMemoryArenaPush( TempArena, oldImageSize );
		GrannyCopyTextureImage( texture, 0, 0, GrannyRGBA8888PixelFormat, texture->Width, texture->Height, texture->Width * 4, oldImageMem );

		// Wii rules:
		// If it has alpha, encode to the 16-bit 5A3 format, if not, DXT1 (CMPR)
		WiiTexFormat texFmt = DetermineAlpha( texture, oldImageMem );
		// force lightmap to DXT1
		if (lightmapTexture)
		{
			texFmt = WIITEXFMT_DXT1_565;
		}

		// Texture sizes on an enforced diet for Wii!
		int newWidth = texture->Width;
		int newHeight = texture->Height;
		int maxDimension = 1024; //( texFmt == WIITEXFMT_RGB5A3 ? 128 : 256 );
		bool scaleTexture = false;
		if (newWidth > maxDimension)
		{
			newWidth = maxDimension;
			scaleTexture = true;
		}
		if (newHeight > maxDimension)
		{
			newHeight = maxDimension;
			scaleTexture = true;
		}
/*
		if (!lightmapTexture)
		{
			if (newWidth > 64)
			{
				newWidth /= 2;
				scaleTexture = true;
			}
			if (newHeight > 64)
			{
				newHeight /= 2;
				scaleTexture = true;
			}
		}
*/
		const granny_int32x scaleImageSize = GrannyGetRawImageSize( GrannyRGBA8888PixelFormat, newWidth * 4, newWidth, newHeight );
		void* scaleImageMem = GrannyMemoryArenaPush( TempArena, scaleImageSize );

		if (scaleTexture)
		{
			HackDownscale8888(texture->Width, texture->Height, newWidth, newHeight, (unsigned int*)oldImageMem, (unsigned int*)scaleImageMem);
		}
		else
		{
			memcpy(scaleImageMem, oldImageMem, scaleImageSize);
		}

		granny_texture* newTexture;
		if ( texFmt == WIITEXFMT_RGB5A3 )
		{
			char textureName[256] = { 0 };
			strcpy( &textureName[0], texture->FromFileName );

			const granny_int32x destStride = newWidth * GrannyRGBA4444PixelFormat->BytesPerPixel;
			const granny_int32x destSize = destStride * newHeight;
			granny_texture_builder* textureBuilder = GrannyBeginRawTexture( newWidth, newHeight, GrannyRGBA4444PixelFormat, destStride );
			GrannyEncodeImage( textureBuilder, newWidth, newHeight, newWidth * 4, 1, scaleImageMem );
			newTexture = GrannyEndTexture( textureBuilder );

			// Assuming only 1 image/mip here
			granny_texture_image &newImage = newTexture->Images[0];
            granny_texture_mip_level &newMIPLevel = newImage.MIPLevels[0];
            void *finalPixels = newMIPLevel.PixelBytes;

			void* swizzledPixels = GrannyMemoryArenaPush( TempArena, destSize * 2 );
			PreconvertSwizzleTextureWiiRGB5A3( (unsigned int *)scaleImageMem, (unsigned int *)swizzledPixels, newWidth, newHeight );
			DownconvertWiiToRGB5A3( (unsigned int*)swizzledPixels, (unsigned short*)finalPixels, newWidth, newHeight );
			//void* swizzledPixels = GrannyMemoryArenaPush( TempArena, destSize );
			//GrannyAll16SwizzleWii( newWidth, newHeight, destStride, newUnswizzledPixels, swizzledPixels );
			//memcpy( newUnswizzledPixels, swizzledPixels, destSize );

			file_info->Textures[i] = newTexture;
			file_info->Textures[i]->FromFileName = GrannyMemoryArenaPushString( TempArena, &textureName[0] );
		}
		else
		{
			char textureName[256] = { 0 };
			strcpy( &textureName[0], texture->FromFileName );

			granny_s3tc_texture_format s3tcFormat = ( texFmt == WIITEXFMT_DXT1_565 ? GrannyS3TCBGR565 : GrannyS3TCBGRA5551 );
			granny_int32x compressedSize = GrannyGetS3TCImageSize(s3tcFormat, newWidth, newHeight);
			const granny_pixel_layout* s3tcLayout = GrannyGetS3TCPixelLayout( s3tcFormat );
			const granny_int32x stride = newWidth / 2; //* s3tcLayout->BytesPerPixel;
			void* swizzledPixels = GrannyMemoryArenaPush( TempArena, compressedSize );
			void* unswizzledPixels = GrannyMemoryArenaPush( TempArena, compressedSize );

//			granny_texture_builder* textureBuilder = GrannyBeginS3TCTexture( newWidth, newHeight, s3tcFormat );
//			GrannyEncodeImage( textureBuilder, newWidth, newHeight, stride, 1, scaleImageMem );
//			newTexture = GrannyEndTexture( textureBuilder );

//			granny_texture_builder* textureBuilder = GrannyBeginS3TCTexture( newWidth, newHeight, s3tcFormat );
//			GrannyEncodeImage( textureBuilder, newWidth, newHeight, stride, 1, scaleImageMem );
//			newTexture = GrannyEndTexture( textureBuilder );
			// Assuming only 1 image/mip here
//			granny_texture_image &newImage = newTexture->Images[0];
//            granny_texture_mip_level &newMIPLevel = newImage.MIPLevels[0];
//            void *newUnswizzledPixels = newMIPLevel.PixelBytes;
//			SwizzleWiiDXT1( (unsigned char *)newUnswizzledPixels, newWidth, newHeight, (unsigned short *)swizzledPixels);
//			GrannyS3TCSwizzleWii( newWidth, newHeight, newUnswizzledPixels, swizzledPixels );
//			memcpy( newUnswizzledPixels, swizzledPixels, compressedSize );

			newTexture = (granny_texture*)GrannyMemoryArenaPush( TempArena, sizeof( granny_texture ) );
//			newTexture->FromFileName = GrannyMemoryArenaPushString( TempArena, texture->FromFileName );
			newTexture->TextureType = GrannyColorMapTextureType;
			newTexture->Width = newWidth;
			newTexture->Height = newHeight;
			newTexture->Encoding = GrannyS3TCTextureEncoding;
			newTexture->SubFormat = s3tcFormat;
			newTexture->Layout = *s3tcLayout;
			newTexture->ImageCount = 1;
			newTexture->ExtendedData.Object = newTexture->ExtendedData.Type = NULL;

			newTexture->Images = (granny_texture_image*)GrannyMemoryArenaPush( TempArena, sizeof( granny_texture_image ) );
			newTexture->Images[0].MIPLevelCount = 1;
			newTexture->Images[0].MIPLevels = (granny_texture_mip_level*)GrannyMemoryArenaPush( TempArena, sizeof( granny_texture_mip_level ) * newTexture->Images[0].MIPLevelCount );
			newTexture->Images[0].MIPLevels[0].PixelBytes = swizzledPixels;
			newTexture->Images[0].MIPLevels[0].PixelByteCount = compressedSize;
			newTexture->Images[0].MIPLevels[0].Stride = stride;

			int dxtTileWidth = newWidth / 4;
			int dxtTileHeight = newHeight / 4;
			unsigned char* pWritePtr = (unsigned char*)unswizzledPixels;
			unsigned int inputBlock[16];
			unsigned int sourceStride = newWidth;

			for (int tileY = 0; tileY < dxtTileHeight; tileY++)
			{
				for (int tileX = 0; tileX < dxtTileWidth; tileX++)
				{
					unsigned int* pSource = (unsigned int*)scaleImageMem;
					pSource += (tileY * sourceStride * 4) + (tileX * 4);
					inputBlock[0] = pSource[0];
					inputBlock[1] = pSource[1];
					inputBlock[2] = pSource[2];
					inputBlock[3] = pSource[3];
					pSource += sourceStride;
					inputBlock[4] = pSource[0];
					inputBlock[5] = pSource[1];
					inputBlock[6] = pSource[2];
					inputBlock[7] = pSource[3];
					pSource += sourceStride;
					inputBlock[8] = pSource[0];
					inputBlock[9] = pSource[1];
					inputBlock[10] = pSource[2];
					inputBlock[11] = pSource[3];
					pSource += sourceStride;
					inputBlock[12] = pSource[0];
					inputBlock[13] = pSource[1];
					inputBlock[14] = pSource[2];
					inputBlock[15] = pSource[3];

					stb_compress_dxt_block(pWritePtr, (const unsigned char *)inputBlock, 0, STB_DXT_HIGHQUAL);
					pWritePtr += 8;
				}
			}

			SwizzleWiiDXT1( (unsigned char *)unswizzledPixels, newWidth, newHeight, (unsigned short *)swizzledPixels);

			file_info->Textures[i] = newTexture;
			file_info->Textures[i]->FromFileName = GrannyMemoryArenaPushString( TempArena, &textureName[0] );
		}

		// Make sure materials are pointing at new texture
		for(int MatIdx = 0; MatIdx < file_info->MaterialCount; ++MatIdx)
		{
			granny_material* Material = file_info->Materials[MatIdx];
			if (!Material || !Material->Texture)
				continue;

			if (Material->Texture == texture)
			{
				Material->Texture = newTexture;
			}
		}
	}
}

void ChangeTextureOrderAlphabeticalAndLowercase( granny_file_info* file_info )
{
	// Swap orders first
	for( granny_int32 i = 0; i < file_info->TextureCount-1; ++i )
	{
		granny_texture* textureA = file_info->Textures[i];

		for( granny_int32 j = i + 1; j < file_info->TextureCount; ++j )
		{
			granny_texture* textureB = file_info->Textures[j];

			int nameDiff = strcmp( textureA->FromFileName, textureB->FromFileName );

			if( nameDiff > 0 )
			{
				file_info->Textures[i] = textureB;
				file_info->Textures[j] = textureA;

				textureA = file_info->Textures[i];
				textureB = file_info->Textures[j];
			}
		}
	}

	// Lowercase texture names
	for( granny_int32 i = 0; i < file_info->TextureCount; ++i )
	{
		wfStringToLower( (char*)file_info->Textures[i]->FromFileName );
	}
}

// Removes the actual texture data, not references to it and such
void RemoveTextureData( granny_file_info* file_info )
{
	for( granny_int32 i = 0; i < file_info->TextureCount; ++i )
	{
		granny_texture* texture = file_info->Textures[i];
		//granny_texture_image* texImages = texture->Images;

		texture->Images = NULL;
	}
}

bool HasNormalSpecEmissiveMap( granny_mesh* mesh )
{
	for( int matIdx = 0; matIdx < mesh->MaterialBindingCount; ++matIdx )
	{
		granny_material* material = mesh->MaterialBindings[ matIdx ].Material;
		for( int mapIdx = 0; mapIdx < material->MapCount; ++mapIdx )
		{
			granny_material_map& map = material->Maps[ mapIdx ];

			if( map.Material->Texture )
			{
				if( wfStringEqualsI( map.Usage, "Normal map" ) || wfStringEqualsI( map.Usage, "Bump" )
					|| wfStringEqualsI( map.Usage, "Specular Color" ) || wfStringEqualsI( map.Usage, "Specular Level" ) )
				{
					return true;
				}
			}
			else if( map.Material->MapCount && map.Material->Maps[ 0 ].Material->Texture )
			{
				if( wfStringEqualsI( map.Usage, "Normal map" ) || wfStringEqualsI( map.Usage, "Bump" ) )
				{
					return true;
				}	
			}
		}
	}

	return false;
}

void CalcVertexFormat( granny_file_info* file_info, granny_memory_arena* TempArena )
{
	static const int kPNGBT33332ComponentCount = 5;
	static const int kPNT332ComponentCount = 3;
	static const int kPWNGBT343332ComponentCount = 7;
	static const int kPWNT3432ComponentCount = 5;

	static const char* kPNT332ComponentNames[ kPNT332ComponentCount ] = 
	{
		GrannyVertexPositionName,
		GrannyVertexNormalName,
		GrannyVertexTextureCoordinatesName "0"
	};

	static const char* kPNGBT33332ComponentNames[ kPNGBT33332ComponentCount ] = 
	{
		GrannyVertexPositionName,
		GrannyVertexNormalName,
		GrannyVertexTangentName,
		GrannyVertexBinormalName,
		GrannyVertexTextureCoordinatesName "0"
	};

	static const char* kPWNT3432ComponentNames[ kPWNT3432ComponentCount ] =
	{
		GrannyVertexPositionName,
		GrannyVertexBoneWeightsName,
		GrannyVertexBoneIndicesName,
		GrannyVertexNormalName,
		GrannyVertexTextureCoordinatesName "0"
	};

	static const char* kPWNGBT343332ComponentNames[ kPWNGBT343332ComponentCount ] = 
	{
		GrannyVertexPositionName,
		GrannyVertexBoneWeightsName,
		GrannyVertexBoneIndicesName,
		GrannyVertexNormalName,
		GrannyVertexTangentName,
		GrannyVertexBinormalName,
		GrannyVertexTextureCoordinatesName "0"
	};

	for( int i = 0; i < file_info->MeshCount; ++i )
	{
		const granny_int32x VertexCount = GrannyGetMeshVertexCount( file_info->Meshes[i] );

		if( VertexCount == 0 )
			continue;

		granny_data_type_definition* vertexType = NULL;
		void* NewVertices = NULL;
		int vertexComponentCount = 0;
		const char** vertexComponentNames = NULL;

		if( HasNormalSpecEmissiveMap( file_info->Meshes[i] ) )
		{
			if( GrannyMeshIsRigid( file_info->Meshes[i] ) )
			{
				NewVertices = PushArray( TempArena, VertexCount, granny_pngbt33332_vertex );
				vertexType = GrannyPNGBT33332VertexType;
				vertexComponentCount = kPNGBT33332ComponentCount;
				vertexComponentNames = kPNGBT33332ComponentNames;
			}
			else
			{
				NewVertices = PushArray( TempArena, VertexCount, granny_pwngbt343332_vertex );
				vertexType = GrannyPWNGBT343332VertexType;
				vertexComponentCount = kPWNGBT343332ComponentCount;
				vertexComponentNames = kPWNGBT343332ComponentNames;
			}
            
		}
		else
		{
			if( GrannyMeshIsRigid( file_info->Meshes[i] ) )
			{
				NewVertices = PushArray( TempArena, VertexCount, granny_pnt332_vertex );
				vertexType = GrannyPNT332VertexType;
				vertexComponentCount = kPNT332ComponentCount;
				vertexComponentNames = kPNT332ComponentNames;
			}
			else
			{
				NewVertices = PushArray( TempArena, VertexCount, granny_pwnt3432_vertex );
				vertexType = GrannyPWNT3432VertexType;
				vertexComponentCount = kPWNT3432ComponentCount;
				vertexComponentNames = kPWNT3432ComponentNames;
			}
		}

		GrannyConvertVertexLayouts( VertexCount, GrannyGetMeshVertexType( file_info->Meshes[i] ),
									GrannyGetMeshVertices( file_info->Meshes[i] ), vertexType, NewVertices );

        GrannyOneNormalizeWeights( VertexCount, vertexType, NewVertices );

        file_info->Meshes[i]->PrimaryVertexData->VertexType					= vertexType;
        file_info->Meshes[i]->PrimaryVertexData->Vertices					= (granny_uint8*)NewVertices;
        file_info->Meshes[i]->PrimaryVertexData->VertexComponentNameCount	= vertexComponentCount;
        file_info->Meshes[i]->PrimaryVertexData->VertexComponentNames		= vertexComponentNames;
	}
}

void CreateCompositeMaterialExtendedData( granny_memory_arena* TempArena, granny_material* material )
{
	// Setup extended data to mark it as a composite
	granny_data_type_definition* userPropType = ( granny_data_type_definition* )GrannyMemoryArenaPush( TempArena, sizeof( granny_data_type_definition ) * 2 );
	memset( userPropType, 0, sizeof( granny_data_type_definition ) * 2 );

	userPropType[0].Type = GrannyStringMember;
	userPropType[0].Name = "typeName";
	userPropType[0].ArrayWidth = 0;

	userPropType[1].Type = GrannyEndMember;

	struct DataType
	{
		char* typeName;
	};

	DataType data = { GrannyMemoryArenaPushString( TempArena, "Composite" ) };

	void* dataMem = GrannyMemoryArenaPush( TempArena, sizeof( DataType ) );
	memcpy( dataMem, &data, sizeof( DataType ) );

	material->ExtendedData.Type = &userPropType[0];
	material->ExtendedData.Object = dataMem;
}

void CreateLightmapAndBlendedMaterials( granny_file_info* file_info, granny_memory_arena* TempArena )
{
	granny_mesh* meshToBindTo = NULL;

	for( granny_int32 i = 0; i < file_info->MeshCount; ++i )
	{
		// Ignore "Mesh" bones/bips
		if( wfStringBeginsWithI( file_info->Meshes[i]->Name, "Bone" ) 
			|| wfStringBeginsWithI( file_info->Meshes[i]->Name, "Bip" ) )
		{
			continue;
		}

		// Ignore animation properties
		char* propertyString = NULL;
		if( HasUserProperties( &file_info->Meshes[i]->ExtendedData, "NodeType = 8\r\nAnimationXML = ", propertyString ) )
		{
			continue;
		}

		// Found a normal mesh use this and assume this is what they want
		meshToBindTo = file_info->Meshes[i];
		break;
	}

	if( meshToBindTo == NULL )
	{
		return;
	}

	granny_material* lightMapMat = NULL;
	granny_material* blendMats[2] = { NULL, NULL };
	for( granny_int32 i = 0; i < file_info->MaterialCount; ++i )
	{
		if( wfStringEqualsI( file_info->Materials[i]->Name, "Lightmap" ) )
		{
			lightMapMat = file_info->Materials[i];
		}
		else if( wfStringBeginsWithI( file_info->Materials[i]->Name, "Blend_" ) )
		{
			if( wfStringContainsI( file_info->Materials[i]->Name + 6, "01_Red" ) )
			{
				blendMats[0] = file_info->Materials[i];
			}
			else if( wfStringContainsI( file_info->Materials[i]->Name + 6, "02_Green" ) )
			{
				blendMats[1] = file_info->Materials[i];
			}
		}
	}

	if( blendMats[0] && blendMats[1] )
	{
		// Create new material
		granny_material* newCompositeBlendMat = (granny_material*)GrannyMemoryArenaPush( TempArena, sizeof( granny_material ) );
		newCompositeBlendMat->MapCount = 2;
		newCompositeBlendMat->Name = "BlendMap";
		newCompositeBlendMat->Texture = NULL;

		// Create material maps
		granny_material_map* matMap = (granny_material_map*)GrannyMemoryArenaPush( TempArena, sizeof( granny_material_map ) * 2 );
		newCompositeBlendMat->Maps = matMap;

		matMap[0].Usage = GrannyMemoryArenaPushString( TempArena, "Mat. 1" );
		matMap[0].Material = blendMats[0];

		matMap[1].Usage = GrannyMemoryArenaPushString( TempArena, "Mat. 2" );
		matMap[1].Material = blendMats[1];

		CreateCompositeMaterialExtendedData( TempArena, newCompositeBlendMat );

		// Add to global material list
		++file_info->MaterialCount;
		granny_material** materials = PushArray( TempArena, file_info->MaterialCount, granny_material* );

        for( granny_int32x i = 0; i < file_info->MaterialCount - 1; ++i )
        {
            materials[i] = file_info->Materials[i];
        }

		materials[ file_info->MaterialCount - 1 ] = newCompositeBlendMat;
		file_info->Materials = materials;

		// Find and replace first material with composite material (assuming red is applied to mesh)
		for( granny_int32x i = 0; i < meshToBindTo->MaterialBindingCount; ++i )
        {
			if( meshToBindTo->MaterialBindings[i].Material == blendMats[0] || meshToBindTo->MaterialBindings[i].Material == blendMats[1] )
			{
				meshToBindTo->MaterialBindings[i].Material = newCompositeBlendMat;
				break;
			}
        }
	}
	
	if( lightMapMat )
	{
		// Create new material
		granny_material* newCompositeLightMat = (granny_material*)GrannyMemoryArenaPush( TempArena, sizeof( granny_material ) );
		newCompositeLightMat->MapCount = 1;
		newCompositeLightMat->Name = "LightMap";
		newCompositeLightMat->Texture = NULL;

		// Create material map
		granny_material_map* matMap = (granny_material_map*)GrannyMemoryArenaPush( TempArena, sizeof( granny_material_map ) );
		newCompositeLightMat->Maps = matMap;

		matMap->Usage = GrannyMemoryArenaPushString( TempArena, "Mat. 3" );
		matMap->Material = lightMapMat;

		CreateCompositeMaterialExtendedData( TempArena, newCompositeLightMat );

		// Add to global material list
		++file_info->MaterialCount;
		granny_material** materials = PushArray( TempArena, file_info->MaterialCount, granny_material* );

        for( granny_int32x i = 0; i < file_info->MaterialCount - 1; ++i )
        {
            materials[i] = file_info->Materials[i];
        }

		materials[ file_info->MaterialCount - 1 ] = newCompositeLightMat;

		file_info->Materials = materials;

		// Add material to mesh's list
		++meshToBindTo->MaterialBindingCount;
		granny_material_binding* matBindings = PushArray( TempArena, file_info->MaterialCount, granny_material_binding );

        for( granny_int32x i = 0; i < meshToBindTo->MaterialBindingCount - 1; ++i )
        {
            matBindings[i] = meshToBindTo->MaterialBindings[i];
        }

		matBindings[ meshToBindTo->MaterialBindingCount - 1 ].Material = newCompositeLightMat;

		meshToBindTo->MaterialBindings = matBindings;
	}
}