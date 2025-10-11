// ========================================================================
// $File: //jeffr/granny_29/preprocessor/samples/model_mesh_bounds.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "../wf_utilities.h"

using namespace std;

inline void UpdateMin(granny_real32* Base, granny_real32 const* New)
{
    Base[0] = min(Base[0], New[0]);
    Base[1] = min(Base[1], New[1]);
    Base[2] = min(Base[2], New[2]);
}

inline void UpdateMax(granny_real32* Base, granny_real32 const* New)
{
    Base[0] = max(Base[0], New[0]);
    Base[1] = max(Base[1], New[1]);
    Base[2] = max(Base[2], New[2]);
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
}


granny_file_info*
ModelMeshBounds(char const*          OriginalFilename,
                char const*          OutputFilename,
                granny_file_info*    Info,
                key_value_pair*      KeyValues,
                granny_int32x        NumKeyValues,
                granny_memory_arena* TempArena)
{
    if (Info->ModelCount == 0)
    {
        ErrOut("this version of the command requires at least one model to work with\n");
        return false;
    }

	const char* ignoreText = FindFirstValueForKey( KeyValues, NumKeyValues, "ignore" );

    // We'll use this in the variant builder...
    granny_string_table* StringTable = GrannyNewArenaStringTable(TempArena);

    for (int ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        granny_model* Model = Info->Models[ModelIdx];
        if (Model == 0)
            continue;

        if (Model->MeshBindingCount == 0)
        {
			if( !strcmp( Model->Name, "!!collisionprop" ) && !strcmp( Model->Name, "!!leveldata" ) )
			{
				WarnOut("skipping model: '%s'.  No meshes bound.\n", Model->Name);
			}
            continue;
        }

        bool BoundsValid = false;

        {for (int MeshIdx = 0; MeshIdx < Model->MeshBindingCount; ++MeshIdx)
        {
            granny_mesh* Mesh = Model->MeshBindings[MeshIdx].Mesh;
            if (!Mesh)
                continue;

            int const VertCount = GrannyGetMeshVertexCount(Mesh);
            if (VertCount == 0)
                continue;

			if( wfStringBeginsWithI( Mesh->Name, "Bone" ) || wfStringBeginsWithI( Mesh->Name, "Bip" ) )
			{
				continue;
			}

			if( ignoreText && HasMadeUserProperties( &Mesh->ExtendedData, ignoreText ) )
			{
				continue;
			}

            BoundsValid = true;

            vector<granny_p3_vertex> Verts(VertCount);
            GrannyCopyMeshVertices(Mesh, GrannyP3VertexType, &Verts[0]);

			granny_real32 MeshMin[3] = { 1e38f, 1e38f, 1e38f };
			granny_real32 MeshMax[3] = { -1e38f, -1e38f, -1e38f };
            for (int VertIdx = 0; VertIdx < VertCount; ++VertIdx)
            {
                UpdateMin(MeshMin, Verts[VertIdx].Position);
                UpdateMax(MeshMax, Verts[VertIdx].Position);
            }

			granny_variant_builder* VariantBuilder = GrannyBeginVariant(StringTable);

            GrannyAddScalarArrayMember(VariantBuilder, "MinPosition", 3, MeshMin);
            GrannyAddScalarArrayMember(VariantBuilder, "MaxPosition", 3, MeshMax);

            granny_int32x TypeSize = GrannyGetResultingVariantTypeSize(VariantBuilder);
            granny_int32x ObjSize  = GrannyGetResultingVariantObjectSize(VariantBuilder);

            GrannyEndVariantInPlace(VariantBuilder,
                                    PushArray(TempArena, TypeSize, granny_uint8),
                                    &Mesh->ExtendedData.Type,
                                    PushArray(TempArena, ObjSize, granny_uint8),
                                    &Mesh->ExtendedData.Object);
        }}
    }

    // Since we're using an Arena-based string table, we can release it at this point, the
    // strings are allocated into TempArena, and will remain valid.
    GrannyFreeStringTable(StringTable);

    return Info;
}

static CommandRegistrar RegModelMeshBounds(ModelMeshBounds,
                                           "ModelMeshBounds",
                                           "Attach bounding volume information for the model's meshes");
