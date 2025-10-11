#if !defined(PS3_SPU_SPEEDCHECK_H)
// ========================================================================
// $File: //jeffr/granny_29/tutorial/platform/ps3_spu_speedcheck/ps3_spu_speedcheck.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#include "granny.h"
#include <math.h>
#include <vector>
#include <PSGL/psgl.h>
#include <PSGL/psglu.h>

//-------------------------------------------------------------------------------------
// Scene description
//-------------------------------------------------------------------------------------
struct texture
{
    char const *Name;
    GLuint      TextureName;
};

struct mesh
{
    granny_mesh         *GrannyMesh;
    granny_mesh_binding *GrannyBinding;
    std::vector<texture*> TextureReferences;

    GLuint IndexBufferName;
    GLuint VertexBufferName;
};

struct model
{
    granny_model_instance *GrannyInstance;
    granny_local_pose *LocalPose;
    granny_world_pose *WorldPose;

    GLuint MatrixBufferName;

    float  TimeOffset;
    float* Placement;

    std::vector<mesh*> Meshes;
};

struct Scene
{
    granny_camera Camera;
    int           FrameCount;

    granny_file      *File;
    granny_file_info *FileInfo;

    granny_file      *SPUFile;
    granny_spu_animation_info *SPUFileInfo;

    std::vector<texture*> Textures;
    std::vector<model*>   Models;
    std::vector< std::vector<mesh*> > ModelMeshes;

    // For the purposes of this sample, we'll be sampling the model
    // animation immediately before rendering, which allows us to
    // share the local pose across all models.
    int                MaxBoneCount;
    granny_local_pose *SharedLocalPose;
    granny_local_pose *SharedLocalPose2;

    // We need a temp buffer to build the composite, since we can't
    // build directly into the CG parameter space
    granny_matrix_3x4* SharedTempMatrices;

    // Simple directional light
    float DirFromLight[4];
    float LightColour[4];
    float AmbientColour[4];
};

extern Scene g_scene;

mesh* CreateMesh(granny_model* Model, int const MeshIndex);
GLuint CreateRGBATexture(granny_texture *GrannyTexture, int ImageIndex);
bool InitPSGL();
void Render();

#define PS3_SPU_SPEEDCHECK_H
#endif
