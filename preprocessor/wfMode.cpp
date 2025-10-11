#include "preprocessor.h"
#include <stdio.h>
#include <string.h>

#include "wf_utilities.h"

bool CombineObjectElement(granny_file_info* BaseObject,
                          granny_file_info** Secondaries,
                          granny_int32x SecondaryCount,
                          char const* ElementName,
                          granny_memory_arena* TempArena,
						  std::vector< const char* >* elementsToAddNames = NULL,
						  const char* nameSuffixAdd = NULL,
						  const char* searchUserString = NULL,
						  bool checkForDuplicates = false);

bool WayForwardMode(input_file& InputFile, key_value_pair* KeyValues, granny_int32x NumKeyValues, granny_memory_arena* TempArena )
{
	RequireKey("output", "must specify an output file with \"-output <filename>\"\n");
	RequireKey("r", "Must specify a resource.xml file with \"-r resources.xml\"\n");

	granny_file_info* Info = ExtractFileInfo(InputFile);
    if (Info == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input file\n");
        return false;
    }

	const char* debugOutput = FindFirstValueForKey( KeyValues, NumKeyValues, "d" );

	bool extraDebugOutput = debugOutput && strcmp( debugOutput, "true" ) == 0;

	const char* platform = FindFirstValueForKey( KeyValues, NumKeyValues, "p" );
	g_littleEndian = platform && !strcmp( platform, "PC32" );

	g_wiiTextures = platform && !strcmp( platform, "WII" );

	// types.xml for animation property enums
	const char* typesFile = FindFirstValueForKey( KeyValues, NumKeyValues, "t" );
	if( typesFile )
	{
		QFile typesXMLFile( typesFile );
		typesXMLFile.open( QIODevice::ReadOnly );
	
		//	set XML data
		if( !g_typesXml.setContent( &typesXMLFile ) )
		{
			typesXMLFile.close();
			return false;
		}

		//	close input files
		typesXMLFile.close();
	}

	// resources.xml for animations to combine into us
	const char* resourcesFile = FindFirstValueForKey( KeyValues, NumKeyValues, "r" );
	if( resourcesFile )
	{
		QFile resourcesXMLFile( resourcesFile );
		resourcesXMLFile.open( QIODevice::ReadOnly );
	
		//	set XML data
		if( !g_resourcesXml.setContent( &resourcesXMLFile ) )
		{
			resourcesXMLFile.close();
			return false;
		}

		//	close input files
		resourcesXMLFile.close();
	}

	const char* r90CCW = FindFirstValueForKey( KeyValues, NumKeyValues, "rotate" );
	if( r90CCW )
	{
		g_rotate90CCW = strstr(r90CCW, "true") != NULL;
	}

	const char* textureMode = FindFirstValueForKey( KeyValues, NumKeyValues, "texture" );
	if( textureMode )
	{
		g_s3tcTextures = strstr( textureMode, "s3tc" ) != NULL;
	}

	const char* animFolder = FindFirstValueForKey( KeyValues, NumKeyValues, "animFolder" );

	// Get base file name and create absolute filepath to do anim search
	char animBuffer[256] = { 0 };
	char resourcePath[256] = { 0 };

	strcpy( animBuffer, InputFile.Filename );
	char* curChar = strstr( animBuffer, ".gr2" );
	if( curChar == NULL )
		curChar = strstr( animBuffer, ".GR2" );

	if( curChar == NULL )
		return false;

	*curChar = '\0';	// Remove extension

	// Find last *slash*
	while( *curChar != '\\' && *curChar != '/' )
		--curChar;

	bool isInAnimFolder = strstr( animBuffer, "_anim" ) != NULL;

	++curChar;

	char filename[64] = { 0 };
	strcpy( filename, curChar );
	strncpy( resourcePath, InputFile.Filename, curChar - animBuffer );

	// check for platform override, have to add back up one directory if in a override subdirectory
	// this is assuming we never have override anim directories at presetnt
	bool backUpForOverride = false;
	char *platformPathSubString = strstr( animBuffer, "\\!" );
	if ( platformPathSubString == NULL )
	{
		platformPathSubString = strstr( animBuffer, "/!" );
	}

	if ( platformPathSubString )
	{
		platformPathSubString += 2;
		int offset = 0;
		bool matchingPlatform = true;
		// Make this case insensitive because we should never trust the artists not to name it 'wiI'
		while ( platformPathSubString[ offset ] != '\\' && platformPathSubString[ offset] != '/' && platformPathSubString[ offset ] != '\0' )
		{
			if ( ( toupper( platformPathSubString[ offset ] ) ) != ( toupper( platform[ offset ] ) ) )
			{
				matchingPlatform = false;
				break;
			}
			++offset;
		}
		backUpForOverride = matchingPlatform;
	}

	// We were given specific animation folder use that
	if( animFolder && animFolder[0] )
	{
		short animOffset = 0;
		if( backUpForOverride )
		{
			curChar = platformPathSubString - 1;
		}

		if( animFolder[0] == '.' && animFolder[1] == '.' )
		{
			curChar -= 2;
			while( *curChar != '\\' && *curChar != '/' )
				--curChar;
			animOffset = 2;
		}

		strcpy( curChar, &animFolder[ animOffset ] );
	}
	else
	{
		if ( backUpForOverride )
		{
			platformPathSubString -= 1;
			curChar = platformPathSubString;
			// Removed this, assume gr2tex file is in same folder now
			//strncpy( resourcePath, InputFile.Filename, ( curChar - animBuffer ) - 1 );
			//resourcePath[ ( curChar - animBuffer ) ] = '\0';
		}

		*curChar++ = '!';
		strcpy( curChar, filename );

		strcat( animBuffer, "_anims"/*/*.gr2*/ );
	}

	std::vector< std::string > animationNameList;
	std::vector< float > animationTimeList;

	const int renameCmdIndex = FindCommandEntry( "RenameElement" );

	std::vector< std::vector< const char* > > meshPropNames;

	// Base rig animation change
	if( Info->AnimationCount > 0 )
	{
		animationNameList.push_back( std::string( "" ) );

		char animName[64] = { 0 };
		strcpy( animName, filename );
		strcat( animName, ".gr2" );
		animationNameList.back() = animName;

		std::vector< const char* > meshNames;

		FindRenameAnimPropMeshes( Info, TempArena, &meshNames, animName, "NodeType = 8\r\nAnimationXML = " );
		RenameAnimPropModels( Info, TempArena, &meshNames, animName );

		animationTimeList.push_back( FindStartTime( Info ) );

		meshPropNames.push_back( meshNames );
	}

	const int transCmdIndex = FindCommandEntry( "TransformFile" );
	const int boundsCmdIndex = FindCommandEntry( "TranslationBounds" );
	const int meshBoundCmdIndex = FindCommandEntry( "ModelMeshBounds" );

	// Transform orientation
	if( transCmdIndex >= 0 )
	{
		command_entry& cmdEntry = GlobalCommandList()[ transCmdIndex ];

		char unitsPerMeterString[64] = { 0 };
		sprintf( unitsPerMeterString, "%f", Info->ArtToolInfo->UnitsPerMeter );

		if( g_rotate90CCW )
		{
			key_value_pair transformKeyValuePairs[] = { { "output", FindFirstValueForKey( KeyValues, NumKeyValues, "output" ) }, { "up", "Y" }, { "right", "Z" }, { "back", "X" }, { "unitsPerMeter", unitsPerMeterString } };

			Info = cmdEntry.BatchableFn( NULL, NULL, Info, &transformKeyValuePairs[0], 5, TempArena );
		}
		else
		{
			key_value_pair transformKeyValuePairs[] = { { "output", FindFirstValueForKey( KeyValues, NumKeyValues, "output" ) }, { "up", "Y" }, { "right", "X" }, { "back", "NegZ" }, { "unitsPerMeter", unitsPerMeterString } };

			Info = cmdEntry.BatchableFn( NULL, NULL, Info, &transformKeyValuePairs[0], 5, TempArena );
		}
	}

	// Calculate bounding of the rig
	if( boundsCmdIndex >= 0 && Info->AnimationCount == 1 && Info->ModelCount > 0 )
	{
		command_entry& cmdEntry = GlobalCommandList()[ boundsCmdIndex ];

		key_value_pair boundKeyValuePairs[] = { { "ignore", "NodeType = 8\r\nAnimationXML = " }, { "boundType", "all" } };
		
		Info = cmdEntry.BatchableFn( NULL, NULL, Info, &boundKeyValuePairs[0], 2, TempArena );
	}

	if( meshBoundCmdIndex >= 0 && Info->ModelCount > 0 )
	{
		command_entry& cmdEntry = GlobalCommandList()[ meshBoundCmdIndex ];
		
		key_value_pair boundKeyValuePairs[] = { { "ignore", "wfProperties" } };
		Info = cmdEntry.BatchableFn( NULL, NULL, Info, &boundKeyValuePairs[0], 1, TempArena );
	}

	bool stripTextureData = false;

	QDir resourceDir( resourcePath );
	QStringList fileList = resourceDir.entryList( QStringList( "*.gr2tex" ) );
	// Check for gr2Tex file, which means combine textures into one file (done externally), so we have to strip texture data from us
	if( !fileList.empty() )
	{
		QFile gr2TexFile( QString( resourcePath ) + fileList.at( 0 ) );
		if( gr2TexFile.open( QIODevice::ReadOnly ) )
		{
			QDomDocument xml;
			//	set XML data
			if( !xml.setContent( &gr2TexFile ) )
			{
				gr2TexFile.close();
				//return false;
			}

			//	close input files
			gr2TexFile.close();

			stripTextureData = true;
		}
	}

#if 0 // needs to be fixed to allow pass through of vertex colors - DM 
	CalcVertexFormat( Info, TempArena );
#endif

	QDomNodeList folders = g_resourcesXml.elementsByTagName( "folder" );

	for( int i = 0; i < folders.count(); ++i )
	{
		QDomElement folderElement = folders.at( i ).toElement();
		QString folderName = folderElement.attribute( "source" );

		if( folderName.compare( QString( animBuffer ), Qt::CaseInsensitive ) == 0 )
		{
			QDomNodeList subAnims = folderElement.elementsByTagName( "gr2" );
			for( int j = 0; j < subAnims.count(); ++j )
			{
				QDomElement subElement = subAnims.at( j ).toElement();
				QByteArray subFullPath = subElement.attribute( "resource" ).toAscii();
				QByteArray qsubFilename = QString( subElement.attribute( "name" ) + ".gr2" ).toAscii();
				char subFilename[256] = { 0 };
				strcpy( &subFilename[0], qsubFilename.data() );

				if( extraDebugOutput )
				{
					printf( "\tCombining Animation %s\n", (const char*)subFullPath.data() );
				}

				granny_file* grannyFile = GrannyReadEntireFile( (const char*)subFullPath.data() );

				if( grannyFile != NULL )
				{
					granny_file_info* otherFileInfo = GrannyGetFileInfo( grannyFile );

					if( transCmdIndex >= 0 )
					{
						command_entry& cmdEntry = GlobalCommandList()[ transCmdIndex ];

						char unitsPerMeterString[64] = { 0 };
						sprintf( unitsPerMeterString, "%f", otherFileInfo->ArtToolInfo->UnitsPerMeter );

						if ( g_rotate90CCW )
						{
							key_value_pair transformKeyValuePairs[] = { { "output", FindFirstValueForKey( KeyValues, NumKeyValues, "output" ) }, { "up", "Y" }, { "right", "Z" }, { "back", "X" }, { "unitsPerMeter", unitsPerMeterString } };

							otherFileInfo = cmdEntry.BatchableFn( NULL, NULL, otherFileInfo, &transformKeyValuePairs[0], 5, TempArena );
						}
						else
						{
							key_value_pair transformKeyValuePairs[] = { { "output", FindFirstValueForKey( KeyValues, NumKeyValues, "output" ) }, { "up", "Y" }, { "right", "X" }, { "back", "NegZ" }, { "unitsPerMeter", unitsPerMeterString } };

							otherFileInfo = cmdEntry.BatchableFn( NULL, NULL, otherFileInfo, &transformKeyValuePairs[0], 5, TempArena );
						}
					}

					// Make sure after transform so bounding is with respect to new orientation
					if( boundsCmdIndex >= 0 && otherFileInfo->AnimationCount == 1 && otherFileInfo->ModelCount > 0 )
					{
						command_entry& cmdEntry = GlobalCommandList()[ boundsCmdIndex ];
		
						key_value_pair boundKeyValuePairs[] = { { "ignore", "NodeType = 8\r\nAnimationXML = " }, { "boundType", "all" } };
		
						otherFileInfo = cmdEntry.BatchableFn( NULL, NULL, otherFileInfo, &boundKeyValuePairs[0], 2, TempArena );
					}

					animationNameList.push_back( std::string( "" ) );
					animationTimeList.push_back( FindStartTime( otherFileInfo ) );

					// Change the animation name to match the gr2 file
					if( otherFileInfo->AnimationCount > 0 )
					{
						animationNameList.back() = subFilename;
					}

					std::vector< const char* > meshNames;

					// Copy over animations/trackgroups into our rig
					if( otherFileInfo != NULL )
					{
						CombineObjectElement( Info, &otherFileInfo, 1, "Animations", TempArena );
						CombineObjectElement( Info, &otherFileInfo, 1, "TrackGroup", TempArena );
						CombineObjectElement( Info, &otherFileInfo, 1, "Meshes", TempArena, &meshNames, subFilename, "NodeType = 8\r\nAnimationXML = " );
						CombineObjectElement( Info, &otherFileInfo, 1, "Models", TempArena, &meshNames, subFilename );
					}

					meshPropNames.push_back( meshNames );

					//GrannyFreeFile( grannyFile );
				}
			}
			break;
		}
	}

	if( animationNameList.size() )
	{
		if( extraDebugOutput )
		{
			printf( "\tRenaming Animation TrackGroups\n" );
		}

		key_value_pair* animKeyValues = new key_value_pair[ animationNameList.size() * 2 ];

		std::string* valueStrings = new std::string[ animationNameList.size() ];

		// Rename animation names to match original granny file name
		for( unsigned i = 0; i < animationNameList.size(); ++i )
		{
			char valueString[64] = { 0 };
			sprintf( valueString, "Animations[%d]", i );
			valueStrings[i] = valueString;

			animKeyValues[ i * 2 ].Key = "rename";
			animKeyValues[ i * 2 ].Value = valueStrings[i].c_str();

			animKeyValues[ ( i * 2 ) + 1 ].Key = "newname";
			animKeyValues[ ( i * 2 ) + 1 ].Value = animationNameList[i].c_str();
		}

		command_entry& cmdEntry = GlobalCommandList()[ renameCmdIndex ];

		for( unsigned i = 0; i < animationNameList.size(); ++i )
		{
			cmdEntry.BatchableFn( NULL, NULL, Info, &animKeyValues[ i * 2 ], 2, TempArena );
			RenameAnimTrackGroups( Info->Animations[i], TempArena, &meshPropNames[i], animKeyValues[ ( i * 2 ) + 1 ].Value );

			int dataTypeCount = 0;
			unsigned curDataSize = 0;
			
			if( Info->Animations[i]->ExtendedData.Type )
			{
				curDataSize = GrannyGetTotalObjectSize( Info->Animations[i]->ExtendedData.Type );
				dataTypeCount = ( GrannyGetTotalTypeSize( Info->Animations[i]->ExtendedData.Type ) / sizeof( granny_data_type_definition ) ) - 1;
			}

			granny_data_type_definition* userPropType = ( granny_data_type_definition* )GrannyMemoryArenaPush( TempArena, sizeof( granny_data_type_definition ) * ( dataTypeCount + 2 ) );
			memset( userPropType, 0, sizeof( granny_data_type_definition ) * ( dataTypeCount + 2 ) );

			userPropType[0].Type = GrannyReal32Member;
			userPropType[0].Name = "wfStartTime";
			userPropType[0].ArrayWidth = 1;

			if( dataTypeCount > 0 )
			{
				memcpy( &userPropType[1], Info->Animations[i]->ExtendedData.Type, sizeof( granny_data_type_definition ) * dataTypeCount );
			}

			userPropType[ dataTypeCount + 1 ].Type = GrannyEndMember;

			float* startTime = (float*)GrannyMemoryArenaPush( TempArena, sizeof( float ) + curDataSize );

			*startTime = animationTimeList[i];

			if( curDataSize > 0 )
			{
				memcpy( startTime + 1, Info->Animations[i]->ExtendedData.Object, curDataSize );
			}

			Info->Animations[i]->ExtendedData.Type = &userPropType[0];
			Info->Animations[i]->ExtendedData.Object = startTime;
		}

		delete[] animKeyValues;
		delete[] valueStrings;
	}

	if( isInAnimFolder )
	{
		const int removeCmdIndex = FindCommandEntry( "RemoveElements" );
		command_entry& cmdEntry = GlobalCommandList()[ removeCmdIndex ];

		key_value_pair removeKeyValuePairs[] = { { "remove", "Textures" }, { "remove", "Materials" } };
		Info = cmdEntry.BatchableFn( NULL, NULL, Info, &removeKeyValuePairs[0], 2, TempArena );

		RemoveNonAnimProps( Info );
	}

	ChangeTextureOrderAlphabeticalAndLowercase( Info );

	if ( g_wiiTextures )
	{
		CreateLightmapAndBlendedMaterials( Info, TempArena );

		// Strip out specular/emissive/bump materials
		WiiMaterialStrip( Info, TempArena );

		if( !stripTextureData )
		{
			WiiTextures( Info, TempArena );
		}

		WiiVertexStrip( Info, TempArena );
	}
	else
	{
		if( !stripTextureData )
		{
			if( g_s3tcTextures )
			{
				S3TCTextures( Info, TempArena );
			}	

			ForceProperS3TCTextureFormat( Info, TempArena );
		}

		CreateLightmapAndBlendedMaterials( Info, TempArena );
	}

	// Vertex caching
	const int vertCacheOptIndex = FindCommandEntry( "VertexCacheOptimize" );
	if( vertCacheOptIndex >= 0 )
	{
		command_entry& cmdEntry = GlobalCommandList()[ vertCacheOptIndex ];

		Info = cmdEntry.BatchableFn( NULL, NULL, Info, NULL, 0, TempArena );
	}

	if( !g_wiiTextures )
	{
		// Compress vert data to half floats and u8s
		const int compressVertsIndex = FindCommandEntry( "CompressVertSample" );
		if( compressVertsIndex >= 0 )
		{
			command_entry& cmdEntry = GlobalCommandList()[ compressVertsIndex ];

			key_value_pair compressKeyValuePairs[] = { { "tex", "1" }, { "norm", "1" }, { "color", "1" }, { "reverseOrder", strcmp( platform, "XBOX360" ) == 0 ? "true" : "false" } };
			Info = cmdEntry.BatchableFn( NULL, NULL, Info, &compressKeyValuePairs[0], 4, TempArena );
		}
	}
	else
	{
		// Compress vert data to fixed point and u8s
		const int compressVertsIndex = FindCommandEntry( "CompressVertSampleWii" );
		if( compressVertsIndex >= 0 )
		{
			command_entry& cmdEntry = GlobalCommandList()[ compressVertsIndex ];

			key_value_pair compressKeyValuePairs[] = { { "tex", "1" }, { "norm", "1" }, { "color", "1" }, { "reverseOrder", strcmp( platform, "XBOX360" ) == 0 ? "true" : "false" } };
			Info = cmdEntry.BatchableFn( NULL, NULL, Info, &compressKeyValuePairs[0], 4, TempArena );
		}
	}

	if( stripTextureData )
	{
		RemoveTextureData( Info );
	}

	 // Write out the resulting file...
    bool success = WriteInfoPreserve(KeyValues, NumKeyValues, Info, InputFile.GrannyFile, true, true, true, TempArena);

	if( success )
	{
		const char* endian = ( g_littleEndian ) ? "little" : "big";

		const int cnvCmdIndex = FindCommandEntry( "PlatformConvert" );

		// Convert for the given platform
		if( cnvCmdIndex >= 0 )
		{
			if( extraDebugOutput )
			{
				printf( "\tPlatform Convert\n" );
			}
			command_entry& cmdEntry = GlobalCommandList()[ cnvCmdIndex ];

			key_value_pair convertKeyValuePairs[] = { { "output", FindFirstValueForKey( KeyValues, NumKeyValues, "output" ) }, { "pointer", "32" }, { "endian", endian } };

			cmdEntry.SingleFn( InputFile, &convertKeyValuePairs[0], 3, TempArena );
		}

		// Do SPU animation conversion here
		if( false && platform && !strcmp( platform, "PS3" ) && Info->AnimationCount )
		{
			if( extraDebugOutput )
			{
				printf( "\tPS3 SPU Animation Creation\n" );
			}
			const int spuCmdIndex = FindCommandEntry( "MakeSPUAnimation" );
			if( spuCmdIndex >= 0 )
			{
				command_entry& cmdEntry = GlobalCommandList()[ spuCmdIndex ];

				char spuFilename[256] = { 0 };

				strcpy( spuFilename, FindFirstValueForKey( KeyValues, NumKeyValues, "output" ) );

				strcpy( strstr( spuFilename, ".gr2" ), ".gr2spu" );

				key_value_pair SPUKeyValuePairs[] = { { "output", &spuFilename[0] } };

				success = cmdEntry.SingleFn( InputFile, &SPUKeyValuePairs[0], 1, TempArena );

			}
		}
	}

	return success;
}

// Creates a gr2 for runtime that only includes texture data in it based on the folder it's inside
bool WayForwardTextureMode( key_value_pair* KeyValues, granny_int32x NumKeyValues, granny_memory_arena* TempArena )
{
	RequireKey("input", "must specify an input file with \"-input <filename>\"\n");
	RequireKey("output", "must specify an output file with \"-output <filename>\"\n");

	const char* platform = FindFirstValueForKey( KeyValues, NumKeyValues, "p" );
	const char* inputFile = FindFirstValueForKey( KeyValues, NumKeyValues, "input" );
	const char* outputFile = FindFirstValueForKey( KeyValues, NumKeyValues, "output" );
	const char* endian = FindFirstValueForKey( KeyValues, NumKeyValues, "e" );

	g_wiiTextures = platform && !strcmp( platform, "WII" );


	// Find the sourcePath, probably could just use QDir to do this for me.
	char sourcePath[256] = { 0 };
	strcpy( &sourcePath[0], inputFile );
	char* curChar = &sourcePath[ strlen( sourcePath ) - 1 ];
	// Find last *slash*
	while( *curChar != '\\' && *curChar != '/' )
		--curChar;
	*( curChar + 1 ) = '\0';

	// Removed this assumes gr2tex is in same folder as the gr2s
	// Check for platform folder
	/*if( QDir( sourcePath ).exists( "!" + QString( platform ) ) )
	{
		strcat( sourcePath, "!" );
		strcat( sourcePath, platform );
		strcat( sourcePath, "/" );
	}*/

	// Get all gr2s in our folder
	QStringList fileList = QDir( sourcePath ).entryList( QStringList( "*.gr2" ) );
	granny_file_info* baseGr2FileInfo = NULL;
	granny_file* baseGr2File = NULL;

	if( fileList.size() )
	{
		// Use first gr2 as our base one
		const QString& filename = fileList.at( 0 );
		QString fullPath = sourcePath + filename;
		QByteArray fullPathByte = fullPath.toAscii();
		baseGr2File = GrannyReadEntireFile( (const char*)fullPathByte.data() );
		if( !baseGr2File )
		{
			return true;
		}
		
		baseGr2FileInfo = GrannyGetFileInfo( baseGr2File );

		// Make sure texture names are lowercase
		ChangeTextureOrderAlphabeticalAndLowercase( baseGr2FileInfo );

		std::vector< const char* > changedTextures;
		if( !g_wiiTextures )
		{
			ForceProperS3TCTextureFormat( baseGr2FileInfo, TempArena, &changedTextures );
		}

		// Remove non-textures
		const int removeCmdIndex = FindCommandEntry( "RemoveElements" );
		command_entry& cmdEntry = GlobalCommandList()[ removeCmdIndex ];

		key_value_pair removeKeyValuePairs[] = 
		{
			{ "remove", "ArtToolInfo" },
			{ "remove", "ExporterInfo" },
			{ "remove", "FromFileName" },
			{ "remove", "Materials" },
			{ "remove", "Skeletons" },
			{ "remove", "VertexDatas" },
			{ "remove", "TriTopologies" },
			{ "remove", "Meshes" },
			{ "remove", "Models" },
			{ "remove", "TrackGroups" },
			{ "remove", "Animations" },
			{ "remove", "ExtendedData" }
		};

		baseGr2FileInfo = cmdEntry.BatchableFn( NULL, NULL, baseGr2FileInfo, &removeKeyValuePairs[0], sizeof( removeKeyValuePairs ) / sizeof( removeKeyValuePairs[0] ), TempArena );

		// Open the rest of the gr2s
		std::vector< granny_file_info* > otherFileInfos( fileList.size() - 1, NULL );
		for( int i = 1; i < fileList.size(); ++i )
		{
			const QString& filename = fileList.at( i );
			QString fullPath = sourcePath + filename;
			QByteArray fullPathByte = fullPath.toAscii();
			granny_file* grannyFile = GrannyReadEntireFile( (const char*)fullPathByte.data() );
			if( !grannyFile )
			{
				continue;
			}

			otherFileInfos[i-1] = GrannyGetFileInfo( grannyFile );
			ChangeTextureOrderAlphabeticalAndLowercase( otherFileInfos[i-1] );
			
			if( !g_wiiTextures )
			{
				ForceProperS3TCTextureFormat( otherFileInfos[i-1], TempArena, &changedTextures );
			}
		}

		CombineObjectElement( baseGr2FileInfo, &otherFileInfos[0], otherFileInfos.size(), "Textures", TempArena, NULL, NULL, NULL, true );
		ChangeTextureOrderAlphabeticalAndLowercase( baseGr2FileInfo );

		if( g_wiiTextures )
		{
			WiiTextures( baseGr2FileInfo, TempArena );
		}

		bool success = WriteInfoPreserve( KeyValues, NumKeyValues, baseGr2FileInfo, NULL, true, true, true, TempArena );

		if( success )
		{
			const int cnvCmdIndex = FindCommandEntry( "PlatformConvert" );

			// Convert for the given platform
			if( cnvCmdIndex >= 0 )
			{
				command_entry& cmdEntry = GlobalCommandList()[ cnvCmdIndex ];

				key_value_pair convertKeyValuePairs[] = { { "output", FindFirstValueForKey( KeyValues, NumKeyValues, "output" ) }, { "pointer", "32" }, { "endian", endian } };

				input_file InputFile = { baseGr2File, NULL };
				cmdEntry.SingleFn( InputFile, &convertKeyValuePairs[0], 3, TempArena );
			}
		}

		return success;
	}

	return true;
}

static char const* HelpString =
    "Auto combines animation files in folder named !(GrannyFile)_anims\n"
	"Grabs any Anim Properties and creates extended data for it\n"
	"Converts to proper platform's endianness\n"
	"Rotates so face correct direction for in game use\n"
	"Calculate bounding of each animation\n"
	"Stores proper start time of each animation\n"
	"For PS3 creates an SPU file\n"
	"Much, much, much more!";

static CommandRegistrar RegAutoCombineAnims(eSingleFile_NoMultiRun, WayForwardMode, "WayForwardMode", "Does all the stuff for a WayForward granny file.", HelpString);

static char const* HelpStringTex =
    "Creates a gr2 for runtime that only includes texture data in it based on the folder it's inside";

static CommandRegistrar RegCombineTextures( WayForwardTextureMode, "WayForwardTextureMode", "Does stuff for a WayForward granny texture file.", HelpStringTex );