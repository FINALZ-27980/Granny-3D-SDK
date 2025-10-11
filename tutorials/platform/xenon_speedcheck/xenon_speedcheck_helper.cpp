// ========================================================================
// $File: //jeffr/granny_29/tutorial/platform/xenon_speedcheck/xenon_speedcheck_helper.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "xenon_speedcheck.h"
#include <assert.h>

using namespace std;

//-------------------------------------------------------------------------------------
// We simply hard-code our resource names for the purposes of this example
//-------------------------------------------------------------------------------------
char const *ModelFileName           = "d:\\Media\\test.gr2";
char const *PixelShaderFile         = "d:\\Media\\PixelShader.xpu";
char const *VertexShaderSkinnedFile = "d:\\Media\\SkinVSVertexFetch.xvu";


// Some variables used in both files...
extern int   NumModelRows;
extern int   NumModelCols;
extern float ModelSpacing;

extern D3DDevice* g_pd3dDevice;
extern D3DVertexDeclaration* g_pVertexDecl_Skinned;
extern D3DVertexShader* g_pVertexShader_Skinned;
extern D3DPixelShader* g_pPixelShader;
extern BOOL g_bWidescreen;


// Generic sprintf wrapper around OutputDebugString
static void
ErrorMessage(char const * const FormatString, ...)
{
    char OutBuffer[2048];

    va_list ArgList;
    va_start(ArgList, FormatString);
    vsprintf(OutBuffer, FormatString, ArgList);
    va_end(ArgList);

    OutputDebugStringA(OutBuffer);
}


// GrannyError() gets called whenever Granny encounters a problem or
// wants to warn us about a potential problem.  The Error string
// is an explanation of the problem, but the Type and Origin
// identifiers also allow a programmatic analysis of what
// happened.
void
GrannyError(granny_log_message_type Type,
            granny_log_message_origin Origin,
            char const*  File,
            granny_int32x Line,
            char const *Error, void *UserData)
{
    ErrorMessage("GRANNY: \"%s\"\n", Error);
}

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
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;

    // Create the Direct3D device.
    if( FAILED( pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL,
                                    D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                    &d3dpp, &g_pd3dDevice ) ) )
    {
        return E_FAIL;
    }

    // Create our shaders...
    LOAD_SHADER(PixelShaderFile,         CreatePixelShader,  g_pPixelShader);
    LOAD_SHADER(VertexShaderSkinnedFile, CreateVertexShader, g_pVertexShader_Skinned);

    // Create the rigid and skinned vertex declarations
    {
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
    if (GrannyMeshIsRigid(GrannyMesh))
        return NULL;

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
        assert(!GrannyMeshIsRigid(GrannyMesh));

        int const VertexCount = GrannyGetMeshVertexCount(GrannyMesh);

        granny_pwnt3432_vertex* TempVerts = new granny_pwnt3432_vertex[VertexCount];
        GrannyCopyMeshVertices(GrannyMesh, GrannyPWNT3432VertexType, TempVerts);

        // Because we have one bone buffer per skeleton in this demo, rather than per
        // mesh, we want the vertex indices to be in skeletal space, rather than mesh
        // space.  Apply that mapping before download.
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

    vector< vector<mesh*> > ModelMeshes;
    ModelMeshes.resize(g_scene.FileInfo->ModelCount);

    for (int Model = 0; Model < g_scene.FileInfo->ModelCount; Model++)
    {
        granny_model *GrannyModel = g_scene.FileInfo->Models[Model];

        vector<mesh*>& Meshes = ModelMeshes[Model];
        for (int Mesh = 0; Mesh < GrannyModel->MeshBindingCount; Mesh++)
        {
            // Make sure to ignore the NULLs returned for rigid meshes in this demo...
            mesh* NewMesh = CreateMesh(GrannyModel, Mesh);
            if (NewMesh)
                Meshes.push_back(NewMesh);
        }
    }

    // And then the models.  Note that we'll track the maximum bone
    //  count here, since we need it for the shared local pose...
    g_scene.MaxBoneCount = 0;
    int const TotalModels = NumModelCols * NumModelRows;

    for (int x = 0; x < NumModelCols; ++x)
    {
        float XPos = (x - (NumModelCols/2.0f)) * ModelSpacing;
        for (int z = 0; z < NumModelRows; ++z)
        {
            float ZPos = -z * ModelSpacing;

            int ModelIndex = x * NumModelRows + z;
            float Offset = (ModelIndex * g_scene.FileInfo->Animations[0]->Duration) / TotalModels;

            for (int Model = 0; Model < g_scene.FileInfo->ModelCount; Model++)
            {
                granny_model *GrannyModel = g_scene.FileInfo->Models[Model];
                int const BoneCount = GrannyModel->Skeleton->BoneCount;

                model* NewModel = new model;
                NewModel->TimeOffset = Offset;
                g_scene.Models.push_back(NewModel);

                NewModel->Placement[0] = 1.0f;
                NewModel->Placement[1] = 0.0f;
                NewModel->Placement[2] = 0.0f;
                NewModel->Placement[3] = 0.0f;
                NewModel->Placement[4] = 0.0f;
                NewModel->Placement[5] = 1.0f;
                NewModel->Placement[6] = 0.0f;
                NewModel->Placement[7] = 0.0f;
                NewModel->Placement[8] = 0.0f;
                NewModel->Placement[9] = 0.0f;
                NewModel->Placement[10] = 1.0f;
                NewModel->Placement[11] = 0.0f;
                NewModel->Placement[12] = XPos;
                NewModel->Placement[13] = 0.0f;
                NewModel->Placement[14] = ZPos;
                NewModel->Placement[15] = 1.0f;

                NewModel->GrannyInstance = GrannyInstantiateModel(GrannyModel);
                g_scene.MaxBoneCount = max(g_scene.MaxBoneCount, BoneCount);

                // Create the meshes for this model
                for (int Mesh = 0; Mesh < GrannyModel->MeshBindingCount; Mesh++)
                {
                    NewModel->Meshes.push_back(ModelMeshes[Model][Mesh]);
                }

                int const MatrixBufferSize = BoneCount * sizeof(granny_matrix_3x4);
                for (int i = 0; i < model::s_numBuffers; i++)
                {
                    if (FAILED(g_pd3dDevice->CreateVertexBuffer(MatrixBufferSize,
                                                                D3DUSAGE_SOFTWAREPROCESSING, 0, 0,
                                                                &NewModel->MatrixBuffer[i],
                                                                NULL)))
                    {
                        return E_FAIL;
                    }
                }

                if (g_scene.FileInfo->AnimationCount)
                {
                    granny_animation *Animation = g_scene.FileInfo->Animations[0];
                    granny_control *Control =
                        GrannyPlayControlledAnimation(0.0f, Animation,
                                                      NewModel->GrannyInstance);
                    if(Control)
                    {
                        GrannySetControlLoopCount(Control, 0);
                        GrannyFreeControlOnceUnused(Control);
                    }
                }
            }
        }
    }


    // Initialize the camera.  We've transformed the file into units
    // of 1 unit = 1 meter, so just position the camera about 5 meters
    // back and 1 up, which should be reasonable for the purposes of
    // this test.
    GrannyInitializeDefaultCamera(&g_scene.Camera);
    g_scene.Camera.ElevAzimRoll[0] = -0.5;
    g_scene.Camera.Offset[1] = 0.f;
    g_scene.Camera.Offset[2] = 2.f;
    g_scene.Camera.FarClipPlane  = 200.0f;
    g_scene.Camera.NearClipPlane = 0.1;

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

        // load the mesh's vertex buffer
        if (GrannyMeshIsRigid(Mesh->GrannyMesh))
        {
            // Ignore rigid meshes for this test...
            continue;
        }
        else
        {
            // It's a skinned mesh, go ahead...
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

        int const VertexCount = GrannyGetMeshVertexCount(Mesh->GrannyMesh);

        // load the mesh's index buffer.
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

            g_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0,
                                               0, VertexCount,
                                               3*Group->TriFirst,
                                               Group->TriCount);
            ++Group;
        }
    }}
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

