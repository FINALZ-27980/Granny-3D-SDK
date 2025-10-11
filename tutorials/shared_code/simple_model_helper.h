// ========================================================================
// $File: //jeffr/granny_29/tutorial/shared_code/simple_model_helper.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(SIMPLE_MODEL_HELPER_H)

#include "granny.h"
#include <d3d9.h>
#include <d3dx9.h>
#include <vector>

struct SimpleTexture
{
    char* Name;
    IDirect3DTexture9* TextureBuffer;

    SimpleTexture();
    ~SimpleTexture();
};

struct SimpleMesh
{
    granny_mesh*         Mesh;
    granny_mesh_binding* MeshBinding;
    std::vector<SimpleTexture*> MaterialBindings;

    IDirect3DIndexBuffer9*  IndexBuffer;
    IDirect3DVertexBuffer9* VertexBuffer;

    SimpleMesh();
    ~SimpleMesh();
};

struct SimpleModel
{
    granny_model_instance* ModelInstance;
    std::vector<SimpleMesh*>      BoundMeshes;

    granny_real32 InitialMatrix[16];

    SimpleModel();
    ~SimpleModel();
};

bool InitSimpleModelHelper(IDirect3DDevice9* pD3DDevice);
void CleanupSimpleModelHelper();

void SimpleModelSetup(granny_camera const& DemoCamera,
                      granny_real32 const* DirFromLight,
                      granny_real32 const* LightColour,
                      granny_real32 const* AmbientColour);

void RenderSimpleModel(SimpleModel* Model,
                       granny_world_pose* WorldPose,
                       IDirect3DDevice9* pD3DDevice);


bool CreateSimpleTextures(IDirect3DDevice9*       pD3DDevice,
                          granny_file_info*       Info,
                          std::vector<SimpleTexture*>& Textures);

bool CreateSimpleModels(IDirect3DDevice9*       pD3DDevice,
                        granny_file_info*       Info,
                        std::vector<SimpleTexture*>& Textures,
                        std::vector<SimpleModel*>&   Models,
                        bool CreateAnimations = true);


#define SIMPLE_MODEL_HELPER_H
#endif /* SIMPLE_MODEL_HELPER_H */
