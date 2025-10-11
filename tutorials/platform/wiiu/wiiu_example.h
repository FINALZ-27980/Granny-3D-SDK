// ========================================================================
// $File: //jeffr/granny_29/tutorial/platform/wiiu/wiiu_example.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(WIIU_EXAMPLE_H)

#include "granny.h"
#include <vector>

struct DemoTexture
{
    char* Name;
    // IDirect3DTexture9* TextureBuffer;

    DemoTexture();
    ~DemoTexture();
};


struct DemoMesh
{
    granny_mesh*         Mesh;
    granny_mesh_binding* MeshBinding;

    void* pVertexBuffer;
    u32   vertexSize;
    u32   vertexStride;

    u32*  pIndexBuffer;
    u32   indexCount;
    u32   indexSize;

    std::vector<DemoTexture*> MaterialBindings;

    DemoMesh(granny_mesh* ForMesh, granny_model* OnModel);
    ~DemoMesh();
};

struct DemoModel
{
    granny_model_instance* ModelInstance;
    std::vector<DemoMesh*> BoundMeshes;

    granny_real32 InitialMatrix[16];

    void Update(granny_real32 t);
    void Render();

    DemoModel(granny_model* ForModel);
    ~DemoModel();
};

struct DemoScene
{
    granny_file      *SceneFile;
    granny_file_info *SceneFileInfo;

    granny_camera DemoCamera;

    std::vector<DemoTexture*> m_Textures;
    std::vector<DemoModel*>   m_Models;

    // For the purposes of this sample, we'll be sampling the model
    // animation immediately before rendering, which allows us to share
    // the local pose across all models.
    granny_int32x      MaxBoneCount;
    granny_local_pose* SharedLocalPose;
    granny_world_pose* SharedWorldPose;

    // Simple directional light
    float DirFromLight[4];
    float LightColour[4];
    float AmbientColour[4];

    DemoScene(char const* SceneFilename);
    ~DemoScene();
};


#define WIIU_EXAMPLE_H
#endif /* WIIU_EXAMPLE_H */
