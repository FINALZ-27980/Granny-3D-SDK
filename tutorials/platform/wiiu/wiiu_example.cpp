#include <cafe/demo.h>
#include <cafe/gx2.h>
#include <assert.h>

#include "wiiu_example.h"
#include "granny.h"

////////////////////////////////////////////////////
//
// Assets data, types and interface for demos
//
////////////////////////////////////////////////////

// A .gsh file is a GX2 specific file that contains vertex and pixel
// shader data.
static const char * const GSHList[] =
{
    "pixo_run.gsh"
};

// Struct for attribute buffers
typedef struct granny_pwn343_vertex gpu_vertex;
granny_data_type_definition const* GPUVertexType = GrannyPWN343VertexType;

DemoScene* g_Scene = 0;

// Shader data
DEMOGfxShader skinnedShader = {0};


// The initialization function for the rendering portions of this sample.
// It is responsible for allocating the three types of shaders and buffers
// as well as ensuring that data is flushed from the CPU to GPU memory
// for Simple Shader
static void InitShader(DEMOGfxShader *pShader, const char *fileName)
{
    void * pGshBuf;
    u32 len;

    // Load shader binary to memory allocated by DEMOFSSimpleRead. This
    // memory must be freed with a call to DEMOFree after shaders are loaded.
    pGshBuf = DEMOFSSimpleRead(fileName, &len);
    ASSERT(NULL != pGshBuf && "Unable to load the shader file");

    // Load shaders from memory.
    DEMOGfxLoadShaders(pShader, 0, pGshBuf);

    // Free memory allocated by DEMOFSSimpleRead when loading the shader file.
    DEMOFree(pGshBuf);

    // Init attribute to shader
    DEMOGfxInitShaderAttribute(pShader, "MyPosition", 0,
                               offsetof(gpu_vertex, Position),
                               GX2_ATTRIB_FORMAT_32_32_32_FLOAT);
    DEMOGfxInitShaderAttribute(pShader, "BoneWeights", 0,
                               offsetof(gpu_vertex, BoneWeights),
                               GX2_ATTRIB_FORMAT_8_8_8_8_UNORM);
    DEMOGfxInitShaderAttribute(pShader, "BoneIndices", 0,
                               offsetof(gpu_vertex, BoneIndices),
                               GX2_ATTRIB_FORMAT_8_8_8_8_UINT);
    DEMOGfxInitShaderAttribute(pShader, "MyNormal", 0,
                               offsetof(gpu_vertex, Normal),
                               GX2_ATTRIB_FORMAT_32_32_32_FLOAT);

    // Get uniform location
    DEMOGfxGetVertexShaderUniformLocation(pShader, "u_viewMtx");  // 0
    DEMOGfxGetVertexShaderUniformLocation(pShader, "u_projMtx");  // 1

    DEMOGfxGetVertexShaderUniformLocation(pShader, "DirFromLight");      // 2
    DEMOGfxGetVertexShaderUniformLocation(pShader, "LightColour");        // 3
    DEMOGfxGetVertexShaderUniformLocation(pShader, "AmbientLightColour"); // 4

    DEMOGfxGetVertexShaderUniformLocation(pShader, "BoneMatrices[0]");  // 5

    // Initialize fetch Shader
    DEMOGfxInitFetchShader(pShader);

    // This call with set all shaders.
    GX2SetShaders(&pShader->fetchShader,
                  pShader->pVertexShader,
                  pShader->pPixelShader);
}

GRANNY_CALLBACK(void)
LogCallback(granny_log_message_type Type,
            granny_log_message_origin Origin,
            char const* File, granny_int32x Line,
            char const * Message,
            void* /*UserData*/)
{
    assert(Message);

    /* Granny provides helper functions to get printable strings for the message type and
       origin. */
    char const* TypeString   = GrannyGetLogMessageTypeString(Type);
    char const* OriginString = GrannyGetLogMessageOriginString(Origin);

    /* We're just going to dump to STDOUT in this demo.  You can route the messages to the
       filesystem, or a network interface if you need to. */
    printf("Granny sez: %s (%s)\n"
        "  %s(%d): %s\n", TypeString, OriginString, File, Line, Message);
}


int main(int argc, char **argv)
{
    /* As always, we check to make sure that the version of Granny matches the library
       linked. */
    if (!GrannyVersionsMatch)
    {
        OSFatal("Granny version mismatch");
        return -1;
    }

    granny_log_callback NewCallback;
    NewCallback.Function = LogCallback;
    NewCallback.UserData = NULL;
    GrannySetLogCallback(&NewCallback);

    /* We'll leverage the Nintendo demo structures to initialize the graphics system. */

    // Initialize the DEMO library, DEMO test system, and create the
    // primary display.
    DEMOInit();
    DEMOTestInit(argc, argv);

    // This function will create the appropriate size color and depth
    // buffers, setup the scan buffer, and initialize the viewport and
    // scissor rectangles to be the entire screen.
    DEMOGfxInit(argc, argv);

    // After the DEMO library is initialize the sample's initialization
    // function can be called.

    /* Now we can load Granny files as normal. We'll use the pixo_run scene. */
    g_Scene = new DemoScene("/vol/content/pixo_run.gr2");
    
    // Initialize the shader
    InitShader(&skinnedShader, GSHList[0]);

    granny_system_clock StartTime;
    GrannyGetSystemSeconds(&StartTime);
    while (DEMOIsRunning())
    {
        granny_system_clock CurrTime;
        GrannyGetSystemSeconds(&CurrTime);
        granny_real32 t = GrannyGetSecondsElapsed(&StartTime, &CurrTime);

        // Spin the camera
        g_Scene->DemoCamera.ElevAzimRoll[1] = -(t * 0.5f);
        GrannyBuildCameraMatrices(&g_Scene->DemoCamera);
        
        DEMOGfxBeforeRender();
        GX2ClearColor(&DEMOColorBuffer, 0.2f, 0.2f, 0.2f, 1.0f);
        GX2ClearDepthStencil(&DEMODepthBuffer, GX2_CLEAR_BOTH);


        // We could update at the same time as rendering, since this is single-threaded,
        // but it's solid practice to separate it anyways.
        {for (size_t Idx = 0; Idx < g_Scene->m_Models.size(); ++Idx)
        {
            g_Scene->m_Models[Idx]->Update(t);
        }}

        // ... And get into the rendering ...

        // Restore demo context state that was saved
        GX2SetContextState(DEMOContextState);

        {for (size_t Idx = 0; Idx < g_Scene->m_Models.size(); ++Idx)
        {
            g_Scene->m_Models[Idx]->Render();
        }}

        DEMOGfxDoneRender();
    }

    delete g_Scene;

    // Free shaders
    DEMOGfxFreeShaders(&skinnedShader);

    DEMOGfxShutdown();
    DEMOShutdown();
    return DEMOTestResult();
}


DemoMesh::DemoMesh(granny_mesh* ForMesh, granny_model* OnModel)
  : Mesh(ForMesh)
{
    MeshBinding = GrannyNewMeshBinding(ForMesh, OnModel->Skeleton, OnModel->Skeleton);

    // The nintendo attribute buffers
    {
        granny_int32x VertexSize = sizeof(gpu_vertex);
        granny_int32x VertexCount = GrannyGetMeshVertexCount(ForMesh);

        granny_int32x IndexCount = GrannyGetMeshIndexCount(ForMesh);

        // Allocate buffers for attributes
        vertexSize    = VertexSize * VertexCount;
        pVertexBuffer = DEMOGfxAllocMEM2(vertexSize, GX2_VERTEX_BUFFER_ALIGNMENT);
        vertexStride  = VertexSize;

        indexCount   = IndexCount;
        indexSize    = sizeof(u32);
        pIndexBuffer = (u32*)DEMOGfxAllocMEM2(indexSize * indexCount, GX2_INDEX_BUFFER_ALIGNMENT);

        GrannyCopyMeshVertices(ForMesh, GPUVertexType, pVertexBuffer);
        GrannyCopyMeshIndices(ForMesh, indexSize, pIndexBuffer);

        // Invalidate attribute buffers
        GX2Invalidate(GX2_INVALIDATE_CPU_ATTRIB_BUFFER, pVertexBuffer, vertexSize);
        GX2Invalidate(GX2_INVALIDATE_CPU_ATTRIB_BUFFER, pIndexBuffer,  indexSize*indexCount);
    }
}

DemoMesh::~DemoMesh()
{
    DEMOGfxFreeMEM2(pVertexBuffer); pVertexBuffer = 0;
    DEMOGfxFreeMEM2(pIndexBuffer);  pIndexBuffer  = 0;

    GrannyFreeMeshBinding(MeshBinding);
    MeshBinding = 0;
    Mesh = 0;
}

DemoModel::DemoModel(granny_model* ForModel)
{
    ModelInstance = GrannyInstantiateModel(ForModel);
    GrannyBuildCompositeTransform4x4(&ForModel->InitialPlacement, InitialMatrix);
}

DemoModel::~DemoModel()
{
    GrannyFreeModelInstance(ModelInstance);
    ModelInstance = 0;
}

void DemoModel::Update(granny_real32 t)
{
    GrannySetModelClock(ModelInstance, t);
}

void DemoModel::Render()
{
    granny_model*    Model    = GrannyGetSourceModel(ModelInstance);
    granny_skeleton* Skeleton = Model->Skeleton;

    // Sample the animation.
    GrannySampleModelAnimationsAccelerated(ModelInstance,
                                           Skeleton->BoneCount,
                                           InitialMatrix,
                                           g_Scene->SharedLocalPose,
                                           g_Scene->SharedWorldPose);

    // Set Vertex uniform Regs
    u32 uniformVSCount = 0;
    GX2SetVertexUniformReg(skinnedShader.uniformsVS.location[uniformVSCount++], 4*4, g_Scene->DemoCamera.View4x4);
    GX2SetVertexUniformReg(skinnedShader.uniformsVS.location[uniformVSCount++], 4*4, g_Scene->DemoCamera.Projection4x4);

    // Light color, etc.
    GX2SetVertexUniformReg(skinnedShader.uniformsVS.location[uniformVSCount++], 4, g_Scene->DirFromLight);
    GX2SetVertexUniformReg(skinnedShader.uniformsVS.location[uniformVSCount++], 4, g_Scene->LightColour);
    GX2SetVertexUniformReg(skinnedShader.uniformsVS.location[uniformVSCount++], 4, g_Scene->AmbientColour);
    
    // Set the matrices.  Note that normally we would not do this once per mesh.
    {for (int Idx = 0; Idx < BoundMeshes.size(); ++Idx)
    {
        DemoMesh* Mesh = BoundMeshes[Idx];

        int NumMeshBones = GrannyGetMeshBindingBoneCount(Mesh->MeshBinding);
        assert(NumMeshBones <= 56); // our shader only supports this many...
        granny_real32* CompositeBuffer = GrannyAllocateCompositeBuffer(NumMeshBones);
        
        // Puts the matrices in mesh order, and in 3x4 opengl constant order as well
        GrannyBuildIndexedCompositeBuffer(
            GrannyGetMeshBindingToSkeleton(Mesh->MeshBinding),
            g_Scene->SharedWorldPose,
            GrannyGetMeshBindingToBoneIndices(Mesh->MeshBinding),
            GrannyGetMeshBindingBoneCount(Mesh->MeshBinding),
            (granny_matrix_4x4*)CompositeBuffer);

        // Send the matrices to the shader.  Note that we don't increment the uniformVSCount here...
        GX2SetVertexUniformReg(skinnedShader.uniformsVS.location[uniformVSCount],
                               4 * 4 * NumMeshBones,
                               CompositeBuffer);
        
        GrannyFreeCompositeBuffer(CompositeBuffer);

        // Set Attrib buffer
        GX2SetAttribBuffer(0, Mesh->vertexSize, Mesh->vertexStride, Mesh->pVertexBuffer);

        // This command will actually result in a draw command being issued to
        // the GPU.
        GX2DrawIndexed(GX2_PRIMITIVE_TRIANGLES,
                       Mesh->indexCount,
                       GX2_INDEX_FORMAT_U32,
                       Mesh->pIndexBuffer);
    }}
}


DemoScene::DemoScene(char const* SceneFilename)
{
    SceneFile     = GrannyReadEntireFile(SceneFilename);
    SceneFileInfo = SceneFile ? GrannyGetFileInfo(SceneFile) : 0;
    if (!SceneFileInfo)
    {
        // Not good, but we won't handle the error in the demo...
        OSFatal("Unable to get a granny_file_info for pixo_run.gr2\n");
        return;
    }

    // Get the camera into a reasonable spot...
    {
        GrannyInitializeDefaultCamera(&DemoCamera);
        DemoCamera.NearClipPlane = 10;
        DemoCamera.FarClipPlane  = 400;
        DemoCamera.FOVY = 0.2f * 3.14f;
        DemoCamera.Offset[1] = 35;
        DemoCamera.Offset[2] = 150;
    }

    MaxBoneCount = 0;
    
    // Initialize the models...
    {for (int ModelIdx= 0; ModelIdx < SceneFileInfo->ModelCount; ++ModelIdx)
    {
        granny_model* Model = SceneFileInfo->Models[ModelIdx];
        if (!Model)
            continue;

        // Track the max bone count for our poses
        if (Model->Skeleton->BoneCount > MaxBoneCount)
            MaxBoneCount = Model->Skeleton->BoneCount;

        DemoModel* NewModel = new DemoModel(Model);
        {for (int MeshIdx = 0; MeshIdx < Model->MeshBindingCount; ++MeshIdx)
        {
            granny_mesh* Mesh = Model->MeshBindings[MeshIdx].Mesh;
            if (!Mesh)
                continue;

            DemoMesh* NewMesh = new DemoMesh(Mesh, Model);
            NewModel->BoundMeshes.push_back(NewMesh);
        }}

        // Create animation control.  We'll need to grub through the animations in the
        // source file to find one that matches.  We'll just use the first in an infinite
        // loop...
        {for (int AnimIdx = 0; AnimIdx < SceneFileInfo->AnimationCount; ++AnimIdx)
        {
            granny_animation* Animation = SceneFileInfo->Animations[AnimIdx];
            if (!Animation) // wildly erroneous, but possible
                continue;
            
            granny_control* Control = GrannyPlayControlledAnimation(0, Animation, NewModel->ModelInstance);
            if (Control)
            {
                GrannySetControlLoopCount(Control, 0);  // Forever!
                break;
            }
        }}

        m_Models.push_back(NewModel);
    }}

    SharedLocalPose = GrannyNewLocalPose(MaxBoneCount);
    SharedWorldPose = GrannyNewWorldPoseNoComposite(MaxBoneCount);

    // Default lighting for the shader...
    DirFromLight[0]  = -0.8660f;
    DirFromLight[1]  = 0.5f;
    DirFromLight[2]  = 0;
    LightColour[0]   = 0.8f;
    LightColour[1]   = 0.8f;
    LightColour[2]   = 0.8f;
    LightColour[3]   = 0.8f;
    AmbientColour[0] = 0.2f;
    AmbientColour[1] = 0.2f;
    AmbientColour[2] = 0.2f;
    AmbientColour[3] = 0.2f;

    // Set the camera aspect ratio
    {
        granny_real32 Aspect = 1.0f;
        switch (GX2GetSystemTVAspectRatio())
        {
            case GX2_ASPECT_RATIO_4_BY_3:  Aspect = 4.0f / 3.0f; break;
            case GX2_ASPECT_RATIO_16_BY_9: Aspect = 16.0f / 9.0f; break;
            default:
                assert(false);
                break;
        }

        int Width, Height;
        switch (GX2GetSystemTVScanMode())
        {
            case GX2_TV_SCAN_MODE_576I:

            case GX2_TV_SCAN_MODE_480I:
            case GX2_TV_SCAN_MODE_480P:
                Width = 640;
                Height = 480;
                break;

            case GX2_TV_SCAN_MODE_720P:
                Width  = 1280;
                Height = 720;

            case GX2_TV_SCAN_MODE_1080I:
            case GX2_TV_SCAN_MODE_1080P:
                Width  = 1920;
                Height = 1080;
                break;

            case GX2_TV_SCAN_MODE_NONE:
            case GX2_TV_SCAN_MODE_RESERVED:
                assert(false);
                Width = Height = 1;
                break;
        }
        
        GrannySetCameraAspectRatios(&DemoCamera, Aspect,
                                    Width, Height,
                                    Width, Height);
    }
}


DemoScene::~DemoScene()
{
    GrannyFreeWorldPose(SharedWorldPose); SharedWorldPose = 0;
    GrannyFreeLocalPose(SharedLocalPose); SharedLocalPose = 0;

    {for (size_t Idx = 0; Idx < m_Models.size(); ++Idx)
    {
        delete m_Models[Idx];
        m_Models[Idx] = 0;
    }}

    SceneFileInfo = 0;
    GrannyFreeFile(SceneFile);
    SceneFile = 0;
}
