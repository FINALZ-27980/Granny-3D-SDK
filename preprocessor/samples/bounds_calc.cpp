// ========================================================================
// $File: //jeffr/granny_29/preprocessor/lod_calc.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <stdio.h>
#include <assert.h>
#include <vector>
#include <string>
#include <string.h>
#include <stdlib.h>

using namespace std;

bool BoundsCalc(input_file*     InputFiles,
                granny_int32x   NumInputFiles,
                key_value_pair* KeyValues,
                granny_int32x   NumKeyValues,
                granny_memory_arena* TempArena)
{
    if (NumInputFiles != 2)
    {
        ErrOut("must supply model and animation (if they are the same, specify twice)\n");
        return false;
    }

    granny_file_info* ModelInfo = ExtractFileInfo(InputFiles[0]);
    if (ModelInfo == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input model file\n");
        return false;
    }

    granny_file_info* AnimInfo = ExtractFileInfo(InputFiles[1]);
    if (AnimInfo == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input animation file\n");
        return false;
    }

    if (ModelInfo->MeshCount == 0)
    {
        ErrOut("ModelFile contains no mesh information with which to make the LOD calcs\n");
        return false;
    }

    if (AnimInfo->AnimationCount == 0)
    {
        ErrOut("AnimFile contains no animations\n");
        return false;
    }

    // Compute mesh bindings for the models...
    granny_int32x NumBindings = 0;
    {for(granny_int32x Model = 0; Model < ModelInfo->ModelCount; ++Model)
    {
        NumBindings += ModelInfo->Models[Model]->MeshBindingCount;
    }}

    // We need to hang onto the BindingModels so we don't get confused
    // later.  The mesh binding only holds the skeleton, so we can't
    // backtrack from there.  Annoying.
    granny_mesh_binding** Bindings = PushArray(TempArena, NumBindings, granny_mesh_binding*);
    granny_model** BindingModels   = PushArray(TempArena, NumBindings, granny_model*);

    NumBindings = 0;
    {for(granny_int32x Model = 0; Model < ModelInfo->ModelCount; ++Model)
    {
        {for(granny_int32 Binding = 0; Binding < ModelInfo->Models[Model]->MeshBindingCount; ++Binding)
        {
            if (ModelInfo->Models[Model]->MeshBindings[Binding].Mesh)
            {
                granny_mesh*  ThisMesh  = ModelInfo->Models[Model]->MeshBindings[Binding].Mesh;
                granny_model* ThisModel = ModelInfo->Models[Model];

                BindingModels[NumBindings] = ThisModel;
                Bindings[NumBindings] =
                    GrannyNewMeshBinding(ThisMesh, ThisModel->Skeleton, ThisModel->Skeleton);
                ++NumBindings;
            }
        }}
    }}

    // This allocates more model_instances than necessary, but it's simpler to follow
    for (int BindIdx = 0; BindIdx < NumBindings; ++BindIdx)
    {
        granny_int32x BoneCount         = BindingModels[BindIdx]->Skeleton->BoneCount;
        granny_local_pose*     ScrLocal = GrannyNewLocalPose(BoneCount);
        granny_world_pose*     ScrWorld = GrannyNewWorldPose(BoneCount);
        granny_model_instance* Instance = GrannyInstantiateModel(BindingModels[BindIdx]);

        // Known input and output vertex format
        granny_mesh* Mesh = GrannyGetMeshBindingSourceMesh(Bindings[BindIdx]);
        granny_int32x VertexCount = GrannyGetMeshVertexCount(Mesh);

        vector<granny_pwn343_vertex> Vertices(VertexCount);
        vector<granny_pn33_vertex>   DeformedVertices(VertexCount);
        GrannyCopyMeshVertices(Mesh, GrannyPWN343VertexType, &Vertices[0]);

        granny_mesh_deformer* Deformer = GrannyNewMeshDeformer(GrannyPWN343VertexType,
                                                               GrannyPN33VertexType,
                                                               GrannyDeformPositionNormal,
                                                               GrannyDontAllowUncopiedTail);
        assert(Deformer); // always valid

        granny_real32 MinPoint[3] = {  1e38f,   1e38f,   1e38f };
        granny_real32 MaxPoint[3] = { -1e38f,  -1e38f,  -1e38f };

        for(granny_int32x Anim = 0; Anim < AnimInfo->AnimationCount; ++Anim)
        {
            granny_animation* Animation = AnimInfo->Animations[Anim];

            granny_control* Control = GrannyPlayControlledAnimation(0, Animation, Instance);
            if (Control)
            {
                granny_real32 t = 0;
                while (t < Animation->Duration)
                {
                    GrannySetModelClock(Instance, t);
                    GrannySampleModelAnimationsAccelerated(Instance, BoneCount, 0, ScrLocal, ScrWorld);

                    // Deform mesh
                    GrannyDeformVertices(Deformer, 
                                         GrannyGetMeshBindingFromBoneIndices(Bindings[BindIdx]),
                                         (granny_real32*)GrannyGetWorldPoseComposite4x4Array(ScrWorld),
                                         VertexCount,
                                         &Vertices[0],
                                         &DeformedVertices[0]);

                    // Scan for the bound
                    for (int v = 0; v < VertexCount; ++v)
                    {
                        for (int d = 0; d < 3; ++d)
                        {
                            MinPoint[d] = min(MinPoint[d], DeformedVertices[v].Position[d]);
                            MaxPoint[d] = max(MaxPoint[d], DeformedVertices[v].Position[d]);
                        }
                    }

                    t += Animation->TimeStep;
                }

                GrannyFreeControl(Control);
            }
        }

        printf("Bounds for mesh %s:\n"
               "  %f %f %f\n"
               "  %f %f %f\n",
               Mesh->Name,
               MinPoint[0], MinPoint[1], MinPoint[2],
               MaxPoint[0], MaxPoint[1], MaxPoint[2]);

        GrannyFreeModelInstance(Instance);
        GrannyFreeLocalPose(ScrLocal);
        GrannyFreeWorldPose(ScrWorld);
    }

    // Free the bindings
    {for(granny_int32x Binding = 0; Binding < NumBindings; ++Binding)
    {
        GrannyFreeMeshBinding(Bindings[Binding]);
    }}

    return true;
}

static char const* HelpString =
    (" BoundsCalc displays mesh bounding information for a given model/mesh\n"
     " animation pairing.  Call with the model/mesh file first, followed\n"
     " by the animation source. These may be the same file.\n"
     "\n"
     "    preprocessor BoundsCalc model.gr2 anim.gr2\n");

static CommandRegistrar RegBoundsCalc(BoundsCalc, "BoundsCalc",
                                      "Demonstrates Computing mesh bounds for an animation",
                                      HelpString);
