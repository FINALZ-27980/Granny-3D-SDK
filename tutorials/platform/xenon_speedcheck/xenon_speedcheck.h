#if !defined(XENON_SPEEDCHECK_H)
// ========================================================================
// See .cpp file comment for details...
//
// (C) Copyright 1999-2007 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include <xtl.h>
#include <xboxmath.h>
#include <vector>
#include "granny.h"

//-------------------------------------------------------------------------------------
// Scene description
//-------------------------------------------------------------------------------------
struct texture
{
    char const        *Name;
    IDirect3DTexture9 *TextureBuffer;
};

struct mesh
{
    granny_mesh         *GrannyMesh;
    granny_mesh_binding *GrannyBinding;
    std::vector<texture*> TextureReferences;

    IDirect3DIndexBuffer9  *IndexBuffer;
    IDirect3DVertexBuffer9 *VertexBuffer;
};

struct model
{
    granny_model_instance *GrannyInstance;

    // Placement matrix for SampleModelAnimAccel
    granny_real32 Placement[16];
    granny_real32 TimeOffset;

    // Double buffer our matrix palette.  This costs memory, but
    // prevents stalls.  This is something to /carefully/ evaluate in
    // the context of your own application.
    static const int s_numBuffers = 2;
    IDirect3DVertexBuffer9 *MatrixBuffer[s_numBuffers];

    std::vector<mesh*> Meshes;
};

struct Scene
{
    granny_camera Camera;
    int           FrameCount;

    granny_file      *File;
    granny_file_info *FileInfo;

    std::vector<texture*> Textures;
    std::vector<model*>   Models;

    // For the purposes of this sample, we'll be sampling the model
    // animation immediately before rendering, which allows us to
    // share the local pose across all models.
    int                MaxBoneCount;

    // Simple directional light
    float DirFromLight[4];
    float LightColour[4];
    float AmbientColour[4];
};

extern Scene g_scene;

// Functions moved to _helper file...
HRESULT InitD3D();
HRESULT InitGranny();
HRESULT InitScene();
void RenderModel(model *Model);

void SetTransposedMatrix4x4(LPDIRECT3DDEVICE9, int ShaderConstant, float *Matrix);
void GetScreenDimensions(int* Width, int* Height);

#define XENON_SPEEDCHECK_H
#endif
