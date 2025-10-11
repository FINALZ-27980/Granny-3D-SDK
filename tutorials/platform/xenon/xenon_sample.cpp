// ========================================================================
// Simple Granny loading and render example
//
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
//
// This program demonstrates how to load and render a Granny model,
// using shaders to perform the matrix skinning.  The technique used
// here is to take advantage of the 360's vfetch facility, which
// allows us to use matrix palettes of arbitrary size.  You can find a
// more detailed description of the the benefits of this particular
// technique (which is only one of many!) in the Xenon XDK example for
// GPU skinning.
//
// This example strives for clarity and simplicity, rather than speed
// or brevity.  We've marked several places in this example where you
// would likely want to do things differently in a production
// application.
// ========================================================================
#include <xtl.h>
#include <xboxmath.h>
#include <vector>
#include <assert.h>
#include "granny.h"
#include "granny_callbacks.h"

//-------------------------------------------------------------------------------------
// We simply hard-code our resource names for the purposes of this example
//-------------------------------------------------------------------------------------
char const *ModelFileName           = "d:\\Media\\rayback.gr2";
char const *PixelShaderFile         = "d:\\Media\\PixelShader.xpu";
char const *VertexShaderRigidFile   = "d:\\Media\\RigidVS.xvu";
char const *VertexShaderSkinnedFile = "d:\\Media\\SkinVSVertexFetch.xvu";


//-------------------------------------------------------------------------------------
// Global D3D variables
//-------------------------------------------------------------------------------------
D3DDevice*             g_pd3dDevice = NULL;

D3DVertexDeclaration*  g_pVertexDecl_Rigid = NULL;
D3DVertexDeclaration*  g_pVertexDecl_Skinned = NULL;

D3DVertexShader*       g_pVertexShader_Rigid = NULL;
D3DVertexShader*       g_pVertexShader_Skinned = NULL;
D3DPixelShader*        g_pPixelShader = NULL;

BOOL                   g_bWidescreen = TRUE;


// These must match the constant declarations in the vertex shaders...
#define MATRIX_CONSTANT_OBJ2WORLD  0
#define MATRIX_CONSTANT_WORLD2VIEW 4
#define MATRIX_CONSTANT_VIEW2CLIP  8
#define VEC3_DIRFROMLIGHT          12
#define VEC4_LIGHTCOLOUR           13
#define VEC4_AMBIENTCOLOUR         14

// ---- Utility functions (implmentation at the bottom of this file)
void TransposedMatrix4x4(granny_real32 *Matrix);
void SetTransposedMatrix4x4(LPDIRECT3DDEVICE9, int ShaderConstant, float *Matrix);
void GetScreenDimensions(int* Width, int* Height);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


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
    granny_world_pose *WorldPose;

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
    granny_local_pose *SharedLocalPose;

    // Simple directional light
    float DirFromLight[4];
    float LightColour[4];
    float AmbientColour[4];
};
Scene g_scene;


//-------------------------------------------------------------------------------------
// Loads a shader from an .?pu file
//-------------------------------------------------------------------------------------
#define LOAD_SHADER(file, create_fn, shader_var)                    \
    {                                                               \
        FILE* pFile = fopen(file, "rb");                            \
        if (!pFile) return E_FAIL;                                  \
        fseek(pFile, 0, SEEK_END);                                  \
        int const fileSize = ftell(pFile);                          \
        fseek(pFile, 0, SEEK_SET);                                  \
        void* Buff = malloc(fileSize);                              \
        if (!fread(Buff, 1, fileSize, pFile))                       \
        {                                                           \
            fclose(pFile); free(Buff);                              \
            return E_FAIL;                                          \
        }                                                           \
        g_pd3dDevice-> ## create_fn( (DWORD*)Buff, & shader_var );  \
        free(Buff);                                                 \
        fclose(pFile);                                              \
    } 0

//-------------------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D
//-------------------------------------------------------------------------------------
HRESULT InitD3D()
{
    // Create the D3D object.
    Direct3D* pD3D = Direct3DCreate9( D3D_SDK_VERSION );
    if( !pD3D )
    {
        return E_FAIL;
    }

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    XVIDEO_MODE VideoMode;
    XGetVideoMode( &VideoMode );
    g_bWidescreen = VideoMode.fIsWideScreen;
    d3dpp.BackBufferWidth        = min( VideoMode.dwDisplayWidth, 1280 );
    d3dpp.BackBufferHeight       = min( VideoMode.dwDisplayHeight, 720 );
    d3dpp.BackBufferFormat       = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferCount        = 1;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;

    // Create the Direct3D device.
    if( FAILED( pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL,
                                    D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                    &d3dpp, &g_pd3dDevice ) ) )
    {
        return E_FAIL;
    }

    // Create our shaders...
    LOAD_SHADER(PixelShaderFile,         CreatePixelShader,  g_pPixelShader);
    LOAD_SHADER(VertexShaderRigidFile,   CreateVertexShader, g_pVertexShader_Rigid);
    LOAD_SHADER(VertexShaderSkinnedFile, CreateVertexShader, g_pVertexShader_Skinned);

    // Create the rigid and skinned vertex declarations
    {
        D3DVERTEXELEMENT9 VertexElementsRigid[] = {
            { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
            { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        // Note the position[1-3] elements are pulled from vertex
        // stream 1, rather than 0.  We'll be binding stream 1 to our
        // matrix palette.  Essentially we're hijacking those vertex
        // semantics to represent a set of 4x3 matrices.
        const D3DVERTEXELEMENT9 VertexElementsSkinned[] = {
            { 0,  0, D3DDECLTYPE_FLOAT3,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,     0 },
            { 0, 12, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT,  0 },
            { 0, 16, D3DDECLTYPE_UBYTE4,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
            { 0, 20, D3DDECLTYPE_FLOAT3,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,       0 },
            { 0, 32, D3DDECLTYPE_FLOAT2,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     0 },
            { 1,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
            { 1, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 2 },
            { 1, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 3 },
            D3DDECL_END()
        };

        g_pd3dDevice->CreateVertexDeclaration( VertexElementsRigid,   &g_pVertexDecl_Rigid   );
        g_pd3dDevice->CreateVertexDeclaration( VertexElementsSkinned, &g_pVertexDecl_Skinned );
    }

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: InitGranny()
// Desc: Install any callback and subsystem overrides in the Granny API
//-------------------------------------------------------------------------------------
HRESULT InitGranny()
{
    // Since this is a sample, I want Granny to call us back any time
    // there's an error or warning, so I can pop up a dialog box and
    // display it.
    granny_log_callback Callback;
    Callback.Function = GrannyError;
    Callback.UserData = 0;
    GrannySetLogCallback(&Callback);

    // In more a more comprehensive implementation, you might replace
    // Granny's file IO, memory allocator or other customizable
    // components here.

    return S_OK;
}


//-------------------------------------------------------------------------------------
// CreateRGBATexture() converts a granny_texture into a D3D Texture
//-------------------------------------------------------------------------------------
IDirect3DTexture9*
CreateRGBATexture(granny_texture *GrannyTexture, int ImageIndex)
{
    granny_texture_image *GrannyImage = &GrannyTexture->Images[ImageIndex];

    // First I ask D3D to create me a BGRA or BGR texture so I can
    // copy into it.  I just pick BGRA8888 as a texture format for
    // simplicity, but if you wanted to be more thorough, you could
    // use GrannyTextureHasAlpha to figure out whether the texture has
    // alpha, or is compressed, etc. and then pick the best format.
    IDirect3DTexture9 *D3DTexture;
    g_pd3dDevice->CreateTexture(GrannyTexture->Width, GrannyTexture->Height,
                                GrannyImage->MIPLevelCount,
                                0, D3DFMT_LIN_A8R8G8B8,
                                D3DPOOL_MANAGED, &D3DTexture, NULL);
    assert(D3DTexture);

    // Now I loop over all the MIP levels and fill them.
    int Width  = GrannyTexture->Width;
    int Height = GrannyTexture->Height;
    {for(int MIPIndex = 0; MIPIndex < GrannyImage->MIPLevelCount; ++MIPIndex)
    {
        granny_texture_mip_level &GrannyMIPLevel =
            GrannyImage->MIPLevels[MIPIndex];

        D3DLOCKED_RECT LockedRect;
        D3DTexture->LockRect(MIPIndex, &LockedRect, 0, 0);

        // GrannyConvertPixelFormat takes any arbitrarily formatted
        // source texture, including Bink-compressed textures, and
        // spits them out in the RGBA format of your choice.  In this
        // case we pick either BGRA8888 (if the texture has alpha) or
        // BGR888, since that's how we created the D3D texture.
        GrannyCopyTextureImage(GrannyTexture, ImageIndex, MIPIndex,
                               //GrannyBGRA8888PixelFormat,
                               GrannyARGB8888PixelFormat,
                               Width, Height,
                               LockedRect.Pitch,
                               LockedRect.pBits);

        D3DTexture->UnlockRect(MIPIndex);

        // Each cycle, the MIP'ing makes the texture half as tall and wide,
        // stopping at 1 pixel
        Width  = max(Width  / 2, 1);
        Height = max(Height / 2, 1);
    }}

    return(D3DTexture);
}

//-------------------------------------------------------------------------------------
// Transforms a file_info structure into a consistent coordinate system
//-------------------------------------------------------------------------------------
static void
TransformFile(granny_file_info *FileInfo)
{
    if ( FileInfo->ArtToolInfo == NULL )
    {
        // File doesn't have any art tool info.
        // Might never have been exported, or might have been stripped by a preprocessor.
        return;
    }

    float Origin[]      = {0, 0, 0};
    float RightVector[] = {1, 0, 0};
    float UpVector[]    = {0, 1, 0};
    float BackVector[]  = {0, 0, 1};

    float Affine3[3];
    float Linear3x3[3][3];
    float InverseLinear3x3[3][3];
    GrannyComputeBasisConversion(FileInfo, 1.0f,
                                 Origin, RightVector, UpVector,
                                 BackVector, Affine3, (float *)Linear3x3,
                                 (float *)InverseLinear3x3);

    GrannyTransformFile(FileInfo, Affine3,
                        (float *)Linear3x3,
                        (float *)InverseLinear3x3,
                        1e-5f, 1e-5f,
                        GrannyRenormalizeNormals | GrannyReorderTriangleIndices);
}

static texture*
FindTextureForMaterial(granny_material *GrannyMaterial)
{
    if (GrannyMaterial == NULL)
        return NULL;

    // I ask Granny for the diffuse color texture of this material,
    // if there is one.  For a more complicated shader system,
    // you would probably ask for more maps (like bump maps) and
    // maybe query extra properties like shininess and so on.
    granny_texture *GrannyTexture =
        GrannyGetMaterialTextureByType(GrannyMaterial,
                                       GrannyDiffuseColorTexture);
    if(GrannyTexture)
    {
        // Now I look through all the textures in the file for a
        // match-by-name.
        for(size_t Tex = 0; Tex < g_scene.Textures.size(); ++Tex)
        {
            texture *Texture = g_scene.Textures[Tex];
            if(strcmp(Texture->Name, GrannyTexture->FromFileName) == 0)
                return Texture;
        }
    }

    return NULL;
}


static mesh*
CreateMesh(granny_model* Model, int const MeshIndex)
{
    assert(MeshIndex >= 0 && MeshIndex < Model->MeshBindingCount);

    granny_mesh *GrannyMesh = Model->MeshBindings[MeshIndex].Mesh;
    mesh* NewMesh = new mesh;

    // Note the mesh pointer and the binding
    NewMesh->GrannyMesh    = GrannyMesh;
    NewMesh->GrannyBinding = GrannyNewMeshBinding(GrannyMesh,
                                                  Model->Skeleton,
                                                  Model->Skeleton);

    // Create the texture references
    for (int i = 0; i < GrannyMesh->MaterialBindingCount; i++)
    {
        NewMesh->TextureReferences.push_back(FindTextureForMaterial(GrannyMesh->MaterialBindings[i].Material));
    }

    // Now create our index buffer
    {
        int const IndexCount = GrannyGetMeshIndexCount(GrannyMesh);
        int const IndexBufferSize = sizeof(granny_uint32) * IndexCount;
        g_pd3dDevice->CreateIndexBuffer(IndexBufferSize,
                                        D3DUSAGE_WRITEONLY,
                                        D3DFMT_INDEX32,
                                        D3DPOOL_MANAGED,
                                        &NewMesh->IndexBuffer, NULL);

        // ... and I copy them in.  The GrannyCopyMeshIndices routine
        // can do arbitrary bytes-per-index conversion, so the 4 just says
        // "make sure it's 4-byte (32-bit) indices".  If it was stored
        // in the file as 32-bit indices, it's a block copy, but if it wasn't,
        // it does index-by-index conversion.
        BYTE *Indices;
        NewMesh->IndexBuffer->Lock(0, IndexBufferSize, (void**)&Indices, 0);
        GrannyCopyMeshIndices(GrannyMesh, 4, Indices);
        NewMesh->IndexBuffer->Unlock();
    }

    // And the vertex buffer
    {
        int VertexCount = GrannyGetMeshVertexCount(GrannyMesh);
        if (GrannyMeshIsRigid(GrannyMesh))
        {
            // GrannyCopyMeshVertices can do arbitrary vertex format
            // conversion, so in this case I just request that, whatever
            // format the mesh is in, it gets converted to 3-float
            // position, 3-float normal, 2-float texture coordinate
            // format.  I use the pre-defined layout
            // GrannyPNT332VertexLayout for this, but you can also define
            // your own, so you're not limited to Granny's presets if you
            // want to use other more fanciful vertex formats (more
            // texture coordinates, tangent spaces, etc., can all be in
            // there).
            int const VertexBufferSize = VertexCount * sizeof(granny_pnt332_vertex);
            g_pd3dDevice->CreateVertexBuffer(VertexBufferSize,
                                             D3DUSAGE_WRITEONLY, 0, 0,
                                             &NewMesh->VertexBuffer, NULL);

            BYTE *Vertices;
            NewMesh->VertexBuffer->Lock(0, VertexBufferSize, (void**)&Vertices, 0);
            GrannyCopyMeshVertices(GrannyMesh, GrannyPNT332VertexType, Vertices);
            NewMesh->VertexBuffer->Unlock();
        }
        else
        {
            // Just as above, but we do need to preserve weights and
            // indices for skinned meshes.  We'll use the predefined
            // GrannyPWNT3432 layout.
            //
            // Note that we'll also be translating the mesh bone
            // indices into "model-absolute" indices, since we don't
            // want to reorder the world_pose buffer for each mesh.
            // This reordering is specific to the vfetch technique
            // we're using here.  If you were using a set of constant
            // registers to hold the matrix palette, you would /not/
            // reorder the mesh indices, but you'd use the Mesh to
            // Model bone index mapping to download the set of
            // matrices that the mesh expected.
            granny_pwnt3432_vertex *TempVerts = new granny_pwnt3432_vertex[VertexCount];
            GrannyCopyMeshVertices(GrannyMesh, GrannyPWNT3432VertexType, TempVerts);

            int const *FromBoneIndices =
                GrannyGetMeshBindingFromBoneIndices(NewMesh->GrannyBinding);
            for (int i = 0; i < VertexCount; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    TempVerts[i].BoneIndices[j] =
                        FromBoneIndices[TempVerts[i].BoneIndices[j]];
                }
            }

            int const VertexBufferSize = VertexCount * sizeof(granny_pwnt3432_vertex);
            g_pd3dDevice->CreateVertexBuffer(VertexBufferSize,
                                             D3DUSAGE_WRITEONLY, 0, 0,
                                             &NewMesh->VertexBuffer, NULL);

            BYTE *Vertices;
            NewMesh->VertexBuffer->Lock(0, VertexBufferSize, (void**)&Vertices, 0);
            memcpy(Vertices, TempVerts, VertexBufferSize);
            NewMesh->VertexBuffer->Unlock();

            delete [] TempVerts;
        }
    }

    return NewMesh;
}


//-------------------------------------------------------------------------------------
// Name: InitScene()
// Desc: Creates the scene.  Load and process the .gr2 file we'll be displaying, and
//       initialize the Granny camera helper.
//-------------------------------------------------------------------------------------
HRESULT InitScene()
{
    g_scene.FrameCount = 0;

    g_scene.File = GrannyReadEntireFile(ModelFileName);
    if (g_scene.File == NULL)
        return E_FAIL;

    g_scene.FileInfo = GrannyGetFileInfo(g_scene.File);
    if (g_scene.FileInfo == NULL)
        return E_FAIL;

    // Put the file into a consistent coordinate system.
    TransformFile(g_scene.FileInfo);

    // Create the textures for this file
    for (int Texture = 0; Texture < g_scene.FileInfo->TextureCount; ++Texture)
    {
        granny_texture *GrannyTexture = g_scene.FileInfo->Textures[Texture];
        texture* NewTexture = new texture;
        g_scene.Textures.push_back(NewTexture);

        // The name of the texture is just the file name that
        // the texture came from.  I'll use this later (in FindTexture())
        // to match texture references to the textures I create here.
        NewTexture->Name = GrannyTexture->FromFileName;

        // Granny can store various different types of textures, but in
        // this sample I'm only interested in the basic color texture map.
        // So I make sure it's one of those, and that it only has one
        // image in it.
        if((GrannyTexture->TextureType == GrannyColorMapTextureType) &&
           (GrannyTexture->ImageCount == 1))
        {
            // We're going to create only uncompressed RGBA textures
            // for this demo.  You can use Granny to create DXTc
            // textures, or any other format you'd like.  This keeps
            // things simple, though.
            NewTexture->TextureBuffer = CreateRGBATexture(GrannyTexture, 0);
        }
        else
        {
            // We're not going to handle cube maps or other whacky
            // varieties of texture maps in this sample app, but if
            // you wanted to, you'd do it here.
            NewTexture->TextureBuffer = NULL;
        }
    }

    // And then the models.  Note that we'll track the maximum bone
    //  count here, since we need it for the shared local pose...
    g_scene.MaxBoneCount = 0;
    for (int Model = 0; Model < g_scene.FileInfo->ModelCount; Model++)
    {
        granny_model *GrannyModel = g_scene.FileInfo->Models[Model];
        int const BoneCount = GrannyModel->Skeleton->BoneCount;

        model* NewModel = new model;
        g_scene.Models.push_back(NewModel);

        // Create the model instance and it's world pose buffer
        NewModel->GrannyInstance = GrannyInstantiateModel(GrannyModel);
        NewModel->WorldPose      = GrannyNewWorldPoseNoComposite(BoneCount);

        // Track the max bone count
        g_scene.MaxBoneCount = max(g_scene.MaxBoneCount, BoneCount);

        // Create the meshes for this model
        bool AnySkinned = false;
        for (int Mesh = 0; Mesh < GrannyModel->MeshBindingCount; Mesh++)
        {
            NewModel->Meshes.push_back(CreateMesh(GrannyModel, Mesh));

            // Is the mesh skinned?
            if (!GrannyMeshIsRigid(NewModel->Meshes.back()->GrannyMesh))
                AnySkinned = true;
        }

        if (AnySkinned)
        {
            int const MatrixBufferSize = BoneCount * sizeof(granny_matrix_3x4);

            for (int i = 0; i < model::s_numBuffers; i++)
            {
                if (FAILED(g_pd3dDevice->CreateVertexBuffer(MatrixBufferSize,
                                                            D3DUSAGE_SOFTWAREPROCESSING, 0, 0,
                                                            &NewModel->MatrixBuffer[i],
                                                            NULL)))
                    return E_FAIL;
            }
        }
        else
        {
            for (int i = 0; i < model::s_numBuffers; i++)
                NewModel->MatrixBuffer[i] = 0;
        }

        // ... and then I see if it has an associated animation here.
        // Animations in Granny are two-tiered.  Each animation
        // represents a specific animated clip, but inside each
        // animation is a number of track groups.  Each track group
        // corresponds to a particular model.  This structure allows
        // you to pull out, say, a particular dancing animation, and
        // apply the two different track groups in it to two different
        // characters and so on.
        if (g_scene.FileInfo->AnimationCount)
        {
            granny_animation *Animation = g_scene.FileInfo->Animations[0];

            // GrannyPlayControlledAnimation is the simplest way to
            // start an animation playing on a model instance.  There
            // are more complicated calls that allow for greater
            // control over how the animation is played, but the
            // purposes of this sample, this is all I need.
            granny_control *Control =
                GrannyPlayControlledAnimation(0.0f, Animation,
                                              NewModel->GrannyInstance);
            if(Control)
            {
                // I want to play this animation ad infinitum, so I
                // set the loop count to the magic value of 0, which
                // means forever.  Any other loop value would play the
                // animation for that many loops and then clamp to the
                // final frame.
                GrannySetControlLoopCount(Control, 0);

                // Since I don't plan to make any further adjustments
                // to this animation, I can just throw away the
                // control handle completely.  However, so I don't
                // leak memory, I have to tell Granny that, once the
                // model(s) that this control affects are freed (which
                // I will do during shutdown), free the control too.
                // Normally Granny won't ever free something you've
                // created unless you tell her too, so this call is
                // basically giving her permission.
                GrannyFreeControlOnceUnused(Control);
            }
        }
    }

    // When Granny updates models, she needs an intermediate buffer
    // that holds the local transform for each bone in a model.  But,
    // I really don't want to keep around a separate buffer for each
    // model because I don't care about the results outside of the
    // update loop.  So I allocate a single local pose buffer, and use
    // it for all the models.
    g_scene.SharedLocalPose = GrannyNewLocalPose(g_scene.MaxBoneCount);

    // Initialize the camera.  We've transformed the file into units
    // of 1 unit = 1 meter, so just position the camera about 5 meters
    // back and 1 up, which should be reasonable for the purposes of
    // this test.
    GrannyInitializeDefaultCamera(&g_scene.Camera);
    g_scene.Camera.ElevAzimRoll[0] = float(-M_PI / 6);
    g_scene.Camera.Offset[2] = 2.f;
    g_scene.Camera.FarClipPlane  = 200.0f;
    g_scene.Camera.NearClipPlane = 0.1f;

    // And we'll use a simple fixed directional light for this sample...
    g_scene.DirFromLight[0]  = 0.5f;
    g_scene.DirFromLight[1]  = 0.8660f;
    g_scene.DirFromLight[2]  = 0.0f;
    g_scene.LightColour[0]   = 1.0f;
    g_scene.LightColour[1]   = 1.0f;
    g_scene.LightColour[2]   = 1.0f;
    g_scene.LightColour[3]   = 0.8f;
    g_scene.AmbientColour[0] = 0.2f;
    g_scene.AmbientColour[1] = 0.2f;
    g_scene.AmbientColour[2] = 0.2f;
    g_scene.AmbientColour[3] = 0.2f;

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the world for the next frame
//-------------------------------------------------------------------------------------
void Update(granny_real32 const CurrentTime,
            granny_real32 const DeltaTime)
{
    // Sweep the camera around the model every 10 seconds
    g_scene.Camera.ElevAzimRoll[1] = fmodf(float(2 * M_PI * CurrentTime / 10),
                                           float(2 * M_PI));

    for (size_t Idx = 0; Idx < g_scene.Models.size(); Idx++)
    {
        model* Model = g_scene.Models[Idx];
        granny_skeleton *Skeleton = GrannyGetSourceSkeleton(Model->GrannyInstance);

        // Set the model clock
        GrannySetModelClock(Model->GrannyInstance, CurrentTime);

        // Sample the animation into the shared local pose...
        GrannySampleModelAnimationsAccelerated(Model->GrannyInstance,
                                               Skeleton->BoneCount, NULL,
                                               g_scene.SharedLocalPose,
                                               Model->WorldPose);

        // Now copy the bone matrices into the appropriate constantvb
        if (Model->MatrixBuffer[0] != NULL)
        {
            assert(Model->MatrixBuffer[1] != NULL);
            IDirect3DVertexBuffer9 *Buffer = Model->MatrixBuffer[g_scene.FrameCount % model::s_numBuffers];

            granny_matrix_3x4 *Matrices;
            Buffer->Lock(0, 0, (void**)&Matrices, 0);
            GrannyBuildCompositeBufferTransposed(Skeleton,
                                                 0, Skeleton->BoneCount,
                                                 Model->WorldPose,
                                                 Matrices);
            Buffer->Unlock();
        }
    }
}


//-------------------------------------------------------------------------------------
// Name: Render()
// Desc: RenderModel() is the meat of the rendering in this app
//-------------------------------------------------------------------------------------
void RenderModel(model *Model)
{
    // Set the pixel shaders.
    // We use the same ones all the time, but of course you'd
    // normally set these differently for each model
    g_pd3dDevice->SetPixelShader( g_pPixelShader );
    g_pd3dDevice->SetSamplerState ( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState ( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetSamplerState ( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    // Now I render all the meshes.
    {for (size_t MeshIndex = 0; MeshIndex < Model->Meshes.size(); ++MeshIndex)
    {
        mesh *Mesh = Model->Meshes[MeshIndex];

        // Normally, you might use the BindingToBoneIndices
        // function here to map from Mesh verts to model verts.  Since
        // we've already reordered the mesh indices into absolute
        // model indices, we can ignore this for the purposes of this
        // sample.  If you were loading skinning matrices into the
        // constant registers, this is where you'd do the mapping.
        //int const *ToBoneIndices =
        //    GrannyGetMeshBindingToBoneIndices(Mesh->GrannyBinding);

        // Next I load the mesh's vertex buffer, or deform
        // into a temporary buffer and load that, depending on
        // whether the mesh is rigid or not.
        int VertexCount = GrannyGetMeshVertexCount(Mesh->GrannyMesh);
        if (GrannyMeshIsRigid(Mesh->GrannyMesh))
        {
            // We don't handle rigid meshes in this application, since the rayback doesn't
            // have any rigid components.  For simplicity, this has been left out.
            continue;
        }
        else
        {
            // It's a skinned mesh
            g_pd3dDevice->SetVertexShader( g_pVertexShader_Skinned );
            g_pd3dDevice->SetVertexDeclaration( g_pVertexDecl_Skinned );

            // Mesh verts into stream 0
            g_pd3dDevice->SetStreamSource(0, Mesh->VertexBuffer, 0,
                                          sizeof(granny_pwnt3432_vertex));

            // Load the bone matrix palette vertex buffer into stream 1.
            D3DVertexBuffer* pCurrentConstantVB =
                Model->MatrixBuffer[ g_scene.FrameCount % model::s_numBuffers ];
            g_pd3dDevice->SetStreamSource(1, pCurrentConstantVB, 0, sizeof(granny_matrix_3x4));
        }

        // Load the mesh's index buffer.
        g_pd3dDevice->SetIndices(Mesh->IndexBuffer);

        // Now both the indices and vertices are loaded, so I can
        // render.  I grab the material groups and spin over them,
        // changing to the appropriate texture and rendering each
        // batch.  A more savvy rendering loop might have instead
        // built a sorted list of material groups to minimize texture
        // changes, etc., but this is the most basic way to render.
        int GroupCount = GrannyGetMeshTriangleGroupCount(Mesh->GrannyMesh);
        granny_tri_material_group *Group =
            GrannyGetMeshTriangleGroups(Mesh->GrannyMesh);
        while(GroupCount--)
        {
            if(Group->MaterialIndex < int(Mesh->TextureReferences.size()))
            {
                texture *Texture = Mesh->TextureReferences[Group->MaterialIndex];
                if(Texture)
                    g_pd3dDevice->SetTexture(0, Texture->TextureBuffer);
            }

            g_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
                                               0,
                                               0, VertexCount,
                                               3*Group->TriFirst,
                                               Group->TriCount);
            ++Group;
        }
    }}
}


//-------------------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-------------------------------------------------------------------------------------
void Render()
{
    int Width, Height;
    GetScreenDimensions(&Width, &Height);

    // Note that I set the camera aspect ratios every frame, because
    // the width and height of the window may have changed which
    // affects the aspect ratio correction.  However, if you're a full
    // screen game and you know when you're changing screen modes and
    // such, then you'd only have to call GrannySetCameraAspectRatios()
    // when you actually change modes.
    GrannySetCameraAspectRatios(&g_scene.Camera,
                                GrannyNTSCTelevisionPhysicalAspectRatio,
                                (float)Width, (float)Height,
                                (float)Width, (float)Height);
    GrannyBuildCameraMatrices(&g_scene.Camera);

    // Clear the backbuffer to a blue color
    g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         D3DCOLOR_XRGB(0,0,255), 1.0f, 0L );

    if(SUCCEEDED(g_pd3dDevice->BeginScene()))
    {
        // Setup the camera and lighting parameters, which don't change
        // over the course of the scene
        SetTransposedMatrix4x4(g_pd3dDevice, MATRIX_CONSTANT_WORLD2VIEW, (FLOAT*)g_scene.Camera.View4x4 );
        SetTransposedMatrix4x4(g_pd3dDevice, MATRIX_CONSTANT_VIEW2CLIP,  (FLOAT*)g_scene.Camera.Projection4x4 );
        g_pd3dDevice->SetVertexShaderConstantF( VEC3_DIRFROMLIGHT,  (FLOAT*)g_scene.DirFromLight, 1 );
        g_pd3dDevice->SetVertexShaderConstantF( VEC4_LIGHTCOLOUR,   (FLOAT*)g_scene.LightColour, 1 );
        g_pd3dDevice->SetVertexShaderConstantF( VEC4_AMBIENTCOLOUR, (FLOAT*)g_scene.AmbientColour, 1 );
        g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);

        // Loop over the models, and render their meshes
        for (size_t Idx = 0; Idx < g_scene.Models.size(); Idx++)
        {
            RenderModel(g_scene.Models[Idx]);
        }
        g_pd3dDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
}


//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
void __cdecl main()
{
    // Initialize Direct3D
    if( FAILED( InitD3D() ) )
        return;

    // Initialize the granny scene
    if( FAILED( InitScene() ) )
        return;

    granny_system_clock StartClock;
    GrannyGetSystemSeconds(&StartClock);

    granny_system_clock LastClock = StartClock;
    while (true)
    {
        // Extract the current time and the frame delta
        granny_real32 CurrentTime, DeltaTime;
        {
            granny_system_clock CurrClock;
            GrannyGetSystemSeconds(&CurrClock);

            // Ignore clock recentering issues for this example
            CurrentTime = GrannyGetSecondsElapsed(&StartClock, &CurrClock);
            DeltaTime   = GrannyGetSecondsElapsed(&LastClock, &CurrClock);
            LastClock = CurrClock;
        }

        // Update the world
        Update(CurrentTime, DeltaTime);

        // Render the scene
        Render();
        g_scene.FrameCount++;
    }
}


//-------------------------------------------------------------------------------------
// After this point are some small utility functions that aren't
// critical to understanding the way Granny works
//-------------------------------------------------------------------------------------
void TransposedMatrix4x4(granny_real32 *Matrix)
{
    granny_real32 TMatrix[16];
    static const int remaps[16] = {
        0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
    };

    for (int i = 0; i < 16; i++)
        TMatrix[i] = Matrix[remaps[i]];
    memcpy(Matrix, TMatrix, sizeof(TMatrix));
}

void SetTransposedMatrix4x4 ( LPDIRECT3DDEVICE9 D3DDevice, int ShaderConstant, float *Matrix )
{
    float TMatrix[16];
    memcpy(TMatrix, Matrix, sizeof(TMatrix));
    TransposedMatrix4x4(TMatrix);

    D3DDevice->SetVertexShaderConstantF( ShaderConstant, TMatrix, 4 );
}


void GetScreenDimensions(int* Width, int* Height)
{
    XVIDEO_MODE xvideomode;
    XGetVideoMode ( &xvideomode );
    *Width  = xvideomode.dwDisplayWidth;
    *Height = xvideomode.dwDisplayHeight;
}

