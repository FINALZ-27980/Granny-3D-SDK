// ps3_basic.cpp : Simple Granny loading and render example
//
// This program demonstrates how to load and render a Granny model,
// using shaders to perform the matrix skinning.  The technique used
// here is to load the matrix palette into a shader constant.
//
// This example strives for clarity and simplicity, rather than speed
// or brevity.  We've marked several places in this example where you
// would likely want to do things differently in a production
// application.
//
#include <math.h>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "granny.h"
#include "granny_callbacks.h"

#include <PSGL/psgl.h>
#include <PSGL/psglu.h>


//-------------------------------------------------------------------------------------
// We simply hard-code our resource names for the purposes of this example
//-------------------------------------------------------------------------------------
char const *ModelFileName           = "/app_home/Media/rayback.gr2";
char const *PixelShaderFile         = "/app_home/Shaders/Fragment.bin";
char const *VertexShaderRigidFile   = "/app_home/Shaders/VertexRigid.bin";
char const *VertexShaderSkinnedFile = "/app_home/Shaders/VertexSkinned.bin";

//-------------------------------------------------------------------------------------
// Global PSGL variables
//-------------------------------------------------------------------------------------
CGprogram g_VertexProgram_Skinned; // loaded vertex program (skinned mesh)
CGprogram g_VertexProgram_Rigid;   // loaded vertex program (rigid mesh)
CGprogram g_FragmentProgram;       // loaded fragment program

// The size of the constant matrix array for the shader
#define MAX_CONSTANT_MATRICES 46

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


// Keep it simple.
GLuint ScreenWidth = 1280;
GLuint ScreenHeight = 720;
void GetScreenDimensions(int* Width, int* Height)
{
    *Width  = (int)ScreenWidth;
    *Height = (int)ScreenHeight;
}


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
    granny_world_pose *WorldPose;

    GLuint MatrixBufferName;
    
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

    // We need a temp buffer to build the composite, since we can't
    // build directly into the CG parameter space
    granny_matrix_3x4* SharedTempMatrices;

    // Simple directional light
    float DirFromLight[4];
    float LightColour[4];
    float AmbientColour[4];
};
Scene g_scene;


//-------------------------------------------------------------------------------------
// Loads a shader from a compiled file
//-------------------------------------------------------------------------------------
CGprogram loadProgramFromFile(CGprofile target, const char* filename)
{
    CGprogram id = cgCreateProgramFromFile(cgCreateContext(), CG_BINARY, filename, target, NULL, NULL);
    if(!id)
    {
        printf("Failed to load shader program >>%s<<\nExiting\n", filename);
        exit(0);
    }
    else
        return id;
}

//-------------------------------------------------------------------------------------
// Name: InitPSGL()
// Desc: Initializes PSGL state
//-------------------------------------------------------------------------------------
void reportFunc(GLenum reportEnum, GLuint reportClassMask,
                const char* String)
{
    printf("%s\n", String);
}


bool InitPSGL()
{
    psglInit(NULL);

    PSGLdevice* psgldev;
    PSGLcontext* psglcon;
    psgldev = psglCreateDeviceAuto(GL_ARGB_SCE, GL_DEPTH_COMPONENT24, GL_MULTISAMPLING_NONE_SCE);
    psglGetDeviceDimensions(psgldev, &ScreenWidth, &ScreenHeight);

    psglcon = psglCreateContext();
    psglMakeCurrent(psglcon, psgldev);
    psglSetReportFunction(reportFunc);

    // Create our shaders...
    g_VertexProgram_Skinned = loadProgramFromFile(cgGLGetLatestProfile(CG_GL_VERTEX), VertexShaderSkinnedFile);
    g_VertexProgram_Rigid   = loadProgramFromFile(cgGLGetLatestProfile(CG_GL_VERTEX), VertexShaderRigidFile);
    g_FragmentProgram       = loadProgramFromFile(cgGLGetLatestProfile(CG_GL_FRAGMENT), PixelShaderFile);

    return true;
}


//-------------------------------------------------------------------------------------
// Name: InitGranny()
// Desc: Install any callback and subsystem overrides in the Granny API
//-------------------------------------------------------------------------------------
bool InitGranny()
{
    // Since this is a sample, I want Granny to call us back any time
    // there's an error or warning, so I can pop up a dialog box and
    // display it.
    granny_log_callback Callback;
    Callback.Function = GrannyError;
    Callback.UserData = 0;
    GrannySetLogCallback(&Callback);

    // And set up Granny's memory manager.
    GrannySetAllocator(IAmGrannysAllocate, IAmGrannysDeallocate);

    return true;
}


//-------------------------------------------------------------------------------------
// CreateRGBATexture() converts a granny_texture into a PSGL Texture
//-------------------------------------------------------------------------------------
GLuint
CreateRGBATexture(granny_texture *GrannyTexture, int ImageIndex)
{
    assert(GrannyTexture->ImageCount > ImageIndex);
    granny_texture_image *GrannyImage = &GrannyTexture->Images[ImageIndex];
    
    GLuint textureName;
    glGenTextures(1, &textureName);
    glBindTexture(GL_TEXTURE_2D, textureName);

    // Ask PSGL to create me a BGRA or BGR texture so I can copy into
    // it.  I just pick BGRA8888 as a texture format for simplicity,
    // but if you wanted to be more thorough, you could use
    // GrannyTextureHasAlpha to figure out whether the texture has
    // alpha, or is compressed, etc. and then pick the best format.
    
    // Create a temporary buffer to copy into...
    granny_uint8* pBuffer = new granny_uint8[GrannyTexture->Width * GrannyTexture->Height * 4];

    // Since we don't particularly care about performance in this
    // example, we'll just use GLU to generate miplevels.
    {
        assert(GrannyImage->MIPLevelCount > 0);

        // GrannyConvertPixelFormat takes any arbitrarily formatted
        // source texture, including Bink-compressed textures, and
        // spits them out in the RGBA format of your choice.
        GrannyCopyTextureImage(GrannyTexture, ImageIndex, 0,
                               GrannyRGBA8888PixelFormat,
                               GrannyTexture->Width, GrannyTexture->Height,
                               GrannyImage->MIPLevels[0].Stride,
                               pBuffer);

        glTexImage2D(GL_TEXTURE_2D, 0,
                     GL_RGBA,
                     GrannyTexture->Width,
                     GrannyTexture->Height,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_INT_8_8_8_8,
                     pBuffer);
        glGenerateMipmapOES(GL_TEXTURE_2D);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Get rid of our temp buffer
    delete [] pBuffer;

    return textureName;
}

//-------------------------------------------------------------------------------------
// Transforms a file_info structure into a consistent coordinate system
//-------------------------------------------------------------------------------------
static void
TransformFile(granny_file_info *FileInfo)
{
    if ( FileInfo->ArtToolInfo == NULL )
    {
        // File doesn't have any art tool info.  Might never have been
        // exported, or might have been stripped by a preprocessor.
        // Note that the model that ships with this example has been
        // run through the Granny preprocessor, so it's already in the
        // correct coordinates, and has been stripped of it's
        // ArtToolInfo.
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

        glGenBuffers(1, &NewMesh->IndexBufferName);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, NewMesh->IndexBufferName);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, IndexBufferSize, NULL, GL_STATIC_DRAW);

        // ... and copy them in.  The GrannyCopyMeshIndices routine
        // can do arbitrary bytes-per-index conversion, so the 4 just says
        // "make sure it's 4-byte (32-bit) indices".  If it was stored
        // in the file as 32-bit indices, it's a block copy, but if it wasn't,
        // it does index-by-index conversion.
        GLvoid* bufPtr = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
        GrannyCopyMeshIndices(GrannyMesh, 4, bufPtr);
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);     
    }

    // And the vertex buffer
    {
        int VertexCount = GrannyGetMeshVertexCount(GrannyMesh);

        int VertexBufferSize = 0;
        granny_data_type_definition* VertexType = 0;
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
            VertexBufferSize = VertexCount * sizeof(granny_pnt332_vertex);
            VertexType = GrannyPNT332VertexType;
        }
        else
        {
            // Just as above, but we do need to preserve weights and
            // indices for skinned meshes.  We'll use the predefined
            // GrannyPWNT3432 layout.
            VertexBufferSize = VertexCount * sizeof(granny_pwnt3432_vertex);
            VertexType = GrannyPWNT3432VertexType;
        }

        glGenBuffers(1, &NewMesh->VertexBufferName);
        glBindBuffer(GL_ARRAY_BUFFER, NewMesh->VertexBufferName);
        glBufferData(GL_ARRAY_BUFFER, VertexBufferSize, NULL, GL_STATIC_DRAW);
            
        GLvoid* bufPtr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        GrannyCopyMeshVertices(GrannyMesh, VertexType, bufPtr);
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    return NewMesh;
}


//-------------------------------------------------------------------------------------                                           
// Name: InitScene()
// Desc: Creates the scene.  Load and process the .gr2 file we'll be displaying, and
//       initialize the Granny camera helper.
//-------------------------------------------------------------------------------------
bool InitScene()
{
    g_scene.FrameCount = 0;

    g_scene.File = GrannyReadEntireFile(ModelFileName);
    if (g_scene.File == NULL)
        return false;

    g_scene.FileInfo = GrannyGetFileInfo(g_scene.File);
    if (g_scene.FileInfo == NULL)
        return false;

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
            NewTexture->TextureName = CreateRGBATexture(GrannyTexture, 0);
        }
        else
        {
            // We're not going to handle cube maps or other whacky
            // varieties of texture maps in this sample app, but if
            // you wanted to, you'd do it here.
            NewTexture->TextureName = 0;
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
        g_scene.MaxBoneCount = std::max(g_scene.MaxBoneCount, BoneCount);

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
            int const MatrixBufferSize = BoneCount * (4 * 4 * sizeof(float));

            // Just one buffer here.  In Real Life, you probably want
            // to double or N-buffer.
            glGenBuffers(1, &NewModel->MatrixBufferName);
            glBindBuffer(GL_ARRAY_BUFFER, NewModel->MatrixBufferName);
            glBufferData(GL_ARRAY_BUFFER, MatrixBufferSize, NULL, GL_STREAM_DRAW);
        }
        else
        {
            NewModel->MatrixBufferName = 0;
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

    // Temp matrix buffer
    g_scene.SharedTempMatrices = new granny_matrix_3x4[g_scene.MaxBoneCount];

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

    return true;
}


//-------------------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the world for the next frame
//-------------------------------------------------------------------------------------
void Update(granny_real32 const CurrentTime,
            granny_real32 const DeltaTime)
{
    // Sweep the camera around the model every 10 seconds
    g_scene.Camera.ElevAzimRoll[1] = (float)fmod(2 * M_PI * CurrentTime / 10, 2 * M_PI);

    for (size_t Idx = 0; Idx < g_scene.Models.size(); Idx++)
    {
        model* Model = g_scene.Models[Idx];
        granny_skeleton *Skeleton = GrannyGetSourceSkeleton(Model->GrannyInstance);

        // Set the model clock
        GrannySetModelClock(Model->GrannyInstance, CurrentTime);

        // Use the accelerated sampler to build the world_pose.  A
        // null offset matrix indicates that we want the model placed
        // at the origin...
        GrannySampleModelAnimationsAccelerated(Model->GrannyInstance,
                                               Skeleton->BoneCount,
                                               NULL,
                                               g_scene.SharedLocalPose,
                                               Model->WorldPose);
    }
}


//-------------------------------------------------------------------------------------
// Name: Render()
// Desc: RenderModel() is the meat of the rendering in this app
//-------------------------------------------------------------------------------------
void RenderModel(model *Model)
{
    cgGLEnableProfile(cgGLGetLatestProfile(CG_GL_VERTEX));
    cgGLEnableProfile(cgGLGetLatestProfile(CG_GL_FRAGMENT));

    // Set the pixel shaders.
    // We use the same ones all the time, but of course you'd
    // normally set these differently for each model
    cgGLBindProgram(g_FragmentProgram);

    // Now I render all the meshes.
    {for (size_t MeshIndex = 0; MeshIndex < Model->Meshes.size(); ++MeshIndex)
    {
        mesh *Mesh = Model->Meshes[MeshIndex];

        // First I load the mesh's buffers
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Mesh->IndexBufferName);
        glBindBuffer(GL_ARRAY_BUFFER, Mesh->VertexBufferName);

        // We'll use this array to remap from Model vert indices to
        // Mesh vert indices.
        int const *ToBoneIndices = GrannyGetMeshBindingToBoneIndices(Mesh->GrannyBinding);

        int const NumMeshBones   = GrannyGetMeshBindingBoneCount(Mesh->GrannyBinding);
        // you'd want to handle this better in Real Life, but we'll
        // just place a restriction on the art for this sample.
        assert(NumMeshBones <= MAX_CONSTANT_MATRICES);  
 
        // Next I load the mesh's vertex buffer, or deform
        // into a temporary buffer and load that, depending on
        // whether the mesh is rigid or not.
        int VertexCount = GrannyGetMeshVertexCount(Mesh->GrannyMesh);
        CGprogram *LoadedProgram = NULL;
        if (GrannyMeshIsRigid(Mesh->GrannyMesh))
        {
            // It's a rigid mesh, so load the appropriate shader and
            // vertex decl.  Note that this is pretty slow, normally
            // you'd order your meshes to minimize the number of state
            // switches.
            cgGLBindProgram( g_VertexProgram_Rigid );
            LoadedProgram = &g_VertexProgram_Rigid;

            // Bind the vertex buffer.  Note that in a real
            // application, you wouldn't be looking these parameters
            // up every frame...
            {
                CGparameter inPos  = cgGetNamedParameter( g_VertexProgram_Rigid, "In.pos" );
                cgGLSetParameterPointer(inPos,  3, GL_FLOAT, sizeof(granny_pnt332_vertex), (GLvoid*)0);
                cgGLEnableClientState(inPos);
            
                CGparameter inNorm = cgGetNamedParameter( g_VertexProgram_Rigid, "In.norm" );
                cgGLSetParameterPointer(inNorm, 3, GL_FLOAT, sizeof(granny_pnt332_vertex), (GLvoid*)12);
                cgGLEnableClientState(inNorm);
            
                CGparameter inTex  = cgGetNamedParameter( g_VertexProgram_Rigid, "In.tex0" );
                cgGLSetParameterPointer(inTex,  2, GL_FLOAT, sizeof(granny_pnt332_vertex), (GLvoid*)24);
                cgGLEnableClientState(inTex);
            }

            // Now I look up the transform for this mesh, and load it.
            granny_matrix_4x4 __attribute__ ((aligned (16))) Transform;
			granny_skeleton *Skeleton = GrannyGetSourceSkeleton(Model->GrannyInstance);
            GrannyBuildIndexedCompositeBuffer(Skeleton, Model->WorldPose,
                                              ToBoneIndices, 1,
                                              &Transform);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glMultMatrixf((GLfloat*)&Transform);

            // The rigid case uses the proj * modelview to transform
            // the verts, so load that now.
            CGparameter mvp = cgGetNamedParameter( g_VertexProgram_Rigid, "modelViewProj" );
            cgGLSetStateMatrixParameter(mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX,
                                        CG_GL_MATRIX_IDENTITY);

            // And the obj2world for normals
            CGparameter o2w = cgGetNamedParameter( g_VertexProgram_Rigid, "objToWorld" );
            cgGLSetStateMatrixParameter(o2w, CG_GL_MODELVIEW_MATRIX,
                                        CG_GL_MATRIX_IDENTITY);
        }
        else
        {
            // It's a skinned mesh
            cgGLBindProgram( g_VertexProgram_Skinned );
            LoadedProgram = &g_VertexProgram_Skinned;

            // Bind the vertex buffer.  Note that in a real
            // application, you wouldn't be looking these parameters
            // up every frame...
            {
                CGparameter inPos  = cgGetNamedParameter( g_VertexProgram_Skinned, "In.Position" );
                cgGLSetParameterPointer(inPos,  3, GL_FLOAT, sizeof(granny_pwnt3432_vertex), (GLvoid*)0);
                cgGLEnableClientState(inPos);
            
                CGparameter inBW  = cgGetNamedParameter( g_VertexProgram_Skinned, "In.BoneWeights" );
                cgGLSetParameterPointer(inBW, 4, GL_UNSIGNED_BYTE, sizeof(granny_pwnt3432_vertex), (GLvoid*)12);
                cgGLEnableClientState(inBW);

                CGparameter inBI  = cgGetNamedParameter( g_VertexProgram_Skinned, "In.BoneIndices" );
                cgGLSetParameterPointer(inBI, 4, GL_UNSIGNED_BYTE, sizeof(granny_pwnt3432_vertex), (GLvoid*)16);
                cgGLEnableClientState(inBI);

                CGparameter inNorm = cgGetNamedParameter( g_VertexProgram_Skinned, "In.Normal" );
                cgGLSetParameterPointer(inNorm, 3, GL_FLOAT, sizeof(granny_pwnt3432_vertex), (GLvoid*)20);
                cgGLEnableClientState(inNorm);
            
                CGparameter inTex  = cgGetNamedParameter( g_VertexProgram_Skinned, "In.Tex0" );
                cgGLSetParameterPointer(inTex,  2, GL_FLOAT, sizeof(granny_pwnt3432_vertex), (GLvoid*)32);
                cgGLEnableClientState(inTex);
            }

            // Note that we don't want to push a bone matrix onto the
            // modelview stack, but push the matrix stack up anyways,
            // so we can unconditionally pop once we're done.
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();

            // Build the composites into the tempbuffer
            granny_skeleton *Skeleton = GrannyGetSourceSkeleton(Model->GrannyInstance);
            GrannyBuildIndexedCompositeBufferTransposed(Skeleton, Model->WorldPose,
                                                        ToBoneIndices, NumMeshBones,
                                                        g_scene.SharedTempMatrices);

            CGparameter bonemats = cgGetNamedParameter( g_VertexProgram_Skinned, "BoneMatrices" );
            cgGLSetMatrixParameterArrayfr(bonemats, 0, NumMeshBones,
                                          (GLfloat*)g_scene.SharedTempMatrices);

            // Setup the viewproj matrix
            CGparameter mvp = cgGetNamedParameter( g_VertexProgram_Skinned, "proj" );
            cgGLSetStateMatrixParameter(mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX,
                                        //cgGLSetStateMatrixParameter(mvp, CG_GL_PROJECTION_MATRIX,
                                        CG_GL_MATRIX_IDENTITY);
        }

        // Setup the common parameters
        assert(LoadedProgram != 0);
        {
            CGparameter dfl = cgGetNamedParameter( *LoadedProgram, "DirFromLight" );
            cgGLSetParameter3fv(dfl, &g_scene.DirFromLight[0]);

            CGparameter lc = cgGetNamedParameter( *LoadedProgram, "LightColour" );
            cgGLSetParameter4fv(lc, &g_scene.LightColour[0]);

            CGparameter al = cgGetNamedParameter( *LoadedProgram, "AmbientColour" );
            cgGLSetParameter4fv(al, &g_scene.AmbientColour[0]);
        }


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
                if (Texture)
                {
                    // Inefficient, we could cache this off...
                    CGparameter texParam = cgGetNamedParameter(g_FragmentProgram, "diffuse_texture");
                    cgGLSetTextureParameter(texParam, Texture->TextureName);
                }
            }

            glDrawRangeElements(GL_TRIANGLES,
                                0, VertexCount-1,
                                Group->TriCount*3,
                                GL_UNSIGNED_INT,
                                (GLvoid*)(3*Group->TriFirst * sizeof(GLuint)));
            ++Group;
        }

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }}

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    cgGLDisableProfile(CG_PROFILE_SCE_VP_TYPEC);
    cgGLDisableProfile(CG_PROFILE_SCE_FP_TYPEC);
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
    glClearColor(0, 0.15f, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT);
    glFrontFace(GL_CW);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf((GLfloat*)g_scene.Camera.Projection4x4);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf((GLfloat*)g_scene.Camera.View4x4);
    glDepthRangef(g_scene.Camera.NearClipPlane, g_scene.Camera.FarClipPlane);

    // Loop over the models, and render their meshes
    for (size_t Idx = 0; Idx < g_scene.Models.size(); Idx++)
    {
        RenderModel(g_scene.Models[Idx]);
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Present the backbuffer contents to the display
    psglSwap();
}


//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
int main()
{
    // Initialize PSGL
    if( !InitPSGL() )
        return -1;

    // Initialize the granny scene
    if( !InitGranny() || !InitScene() )
        return -1;

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

    return 0;
}
