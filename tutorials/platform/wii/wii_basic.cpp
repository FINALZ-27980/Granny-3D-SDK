// wii_basic.cpp : Simple Granny loading and render example
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
#include "granny.h"
#include "wii_basic.h"

#include <vector>
#include <algorithm>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

//#define ENABLE_PROFILE

#ifdef ENABLE_PROFILE
#include <revolution/wiiprofiler.h>
#endif

// We'll use the RVL_SDK demo library to handle initialization of the graphics, etc.
#include <demo.h>


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//-------------------------------------------------------------------------------------
// We simply hard-code our resource names for the purposes of this example
//-------------------------------------------------------------------------------------
char const *ModelFileName           = "rayback.gr2";

//-------------------------------------------------------------------------------------
// Scene description
//-------------------------------------------------------------------------------------
struct texture
{
    char const *Name;

    void* Buffer;

    GXTexObj TexObj;
};

struct mesh
{
    granny_mesh         *GrannyMesh;
    granny_mesh_binding *GrannyBinding;
    std::vector<texture*> TextureReferences;

    granny_mesh_deformer* GrannyDeformer;
};

struct model
{
    granny_model_instance *GrannyInstance;
    granny_world_pose *WorldPose;

    std::vector<mesh*> Meshes;
};

struct Scene
{
    granny_camera Camera;

    granny_file      *File;
    granny_file_info *FileInfo;

    std::vector<texture*> Textures;
    std::vector<model*>   Models;

    // For the purposes of this sample, we'll be sampling the model
    // animation immediately before rendering, which allows us to
    // share the local pose across all models.
    int                MaxBoneCount;
    granny_local_pose *SharedLocalPose;

    int MaxVertexCount;
    granny_pnt332_vertex* TransformedBuffer;
};
Scene g_scene;


//-------------------------------------------------------------------------------------
// Name: InitGranny()
// Desc: Install any callback and subsystem overrides in the Granny API
//-------------------------------------------------------------------------------------
static void ErrorMessage(char const * const FormatString, ...)
{
    va_list ArgList;
    va_start(ArgList, FormatString);
    vprintf(FormatString, ArgList);
    va_end(ArgList);
}

// GrannyError() gets called whenever Granny encounters a problem or
// wants to warn us about a potential problem.  The Error string
// is an explanation of the problem, but the Type and Origin
// identifiers also allow a programmatic analysis of what
// happened.
GRANNY_CALLBACK(void) GrannyError(granny_log_message_type Type,
                                  granny_log_message_origin Origin,
                                  char const* /*File*/, granny_int32x /*Line*/,
                                  char const *Message,
                                  void* /*UserData*/)
{
    char const* TypeString   = GrannyGetLogMessageTypeString(Type);
    char const* OriginString = GrannyGetLogMessageOriginString(Origin);

    /* We're just going to dump to STDOUT in this demo.  You can route the messages to the
       filesystem, or a network interface if you need to. */
    printf("Granny says: %s (%s)\n"
           "  %s\n", TypeString, OriginString, Message);
}

OSHeapHandle TheHeap;
bool InitGranny()
{
    // Since this is a sample, I want Granny to call us back any time
    // there's an error or warning, so I can pop up a dialog box and
    // display it.
    granny_log_callback Callback;
    Callback.Function = GrannyError;
    Callback.UserData = 0;
    GrannySetLogCallback(&Callback);

    return true;
}


//-------------------------------------------------------------------------------------
// CreateRGBATexture() converts a granny_texture into a PSGL Texture
//-------------------------------------------------------------------------------------
bool
CreateRGBATexture(granny_texture *GrannyTexture,
                  int ImageIndex,
                  texture* Dest)
{
    assert(GrannyTexture->ImageCount > ImageIndex);
    granny_texture_image *GrannyImage = &GrannyTexture->Images[ImageIndex];
    assert(GrannyImage->MIPLevelCount > 0);

    granny_int32x Width  = GrannyTexture->Width;
    granny_int32x Height = GrannyTexture->Height;

    // To keep things simple, I'm going to always create a 32-bit texture with no mipmaps.
    u32 TexObjSize = GXGetTexBufferSize((u16)Width, (u16)Height,
                                        GX_TF_RGBA8, GX_TRUE, 1);

    granny_uint8* TempBuffer = new granny_uint8[Width * Height * 4];
    // Default everything (in particular, the alpha) to full on.
    memset(TempBuffer, 0xff, size_t(Width * Height * 4));

    // GrannyConvertPixelFormat takes any arbitrarily formatted
    // source texture, including Bink-compressed textures, and
    // spits them out in the RGBA format of your choice.
    if (!GrannyCopyTextureImage(GrannyTexture, ImageIndex, 0,
                                GrannyARGB8888PixelFormat,
                                Width, Height, Width*4,
                                TempBuffer))
    {
        delete [] TempBuffer;
        return false;
    }

    Dest->Buffer = OSAlloc(TexObjSize);
    if (Dest->Buffer)
    {
        // Granny provides a function to swizzle a linear buffer into the swizzle pattern
        // expected by the hardware, since this is conspicuously lacking in the Wii SDK.
        GrannyARGB8888SwizzleNGC((granny_uint32)Width,
                                 (granny_uint32)Height,
                                 (granny_uint32)Width * 4,
                                 TempBuffer, Dest->Buffer);

        GXInitTexObj(&Dest->TexObj, Dest->Buffer,
                     (u16)Width, (u16)Height,
                     GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
    }

    delete [] TempBuffer;
    return Dest->Buffer != NULL;
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

    // Create the deformer for the model.  Note that in ALL cases, we're using a
    // granny_deformer, to keep the example simple.  For rigid meshes, it would probably
    // be better to use the hardware to transform the verts.
    int VertexCount = GrannyGetMeshVertexCount(GrannyMesh);
    g_scene.MaxVertexCount = std::max(g_scene.MaxVertexCount, VertexCount);
    {
        // Create a Granny deformer for this mesh.  I ask for deformations for the
        // position and normal in this case, since that's all I've got, but I could also
        // ask for deformations of the tangent space if I was doing bump mapping.
        NewMesh->GrannyDeformer =
            GrannyNewMeshDeformer(GrannyGetMeshVertexType(GrannyMesh),
                                  GrannyPNT332VertexType,
                                  GrannyDeformPositionNormal,
                                  GrannyDontAllowUncopiedTail);
        if(!NewMesh->GrannyDeformer)
        {
            ErrorMessage("Granny didn't find a matching deformer for "
                         "the vertex format used by mesh \"%s\".  This "
                         "mesh won't be rendered properly.",
                         GrannyMesh->Name);
            delete NewMesh;
            return NULL;
        }
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
        if((GrannyTexture->TextureType == GrannyColorMapTextureType) &&
           (GrannyTexture->ImageCount == 1))
        {
            texture* NewTexture = new texture;

            // The name of the texture is just the file name that
            // the texture came from.  I'll use this later (in FindTexture())
            // to match texture references to the textures I create here.
            NewTexture->Name = GrannyTexture->FromFileName;

            if (CreateRGBATexture(GrannyTexture, 0, NewTexture))
                g_scene.Textures.push_back(NewTexture);
            else
                delete NewTexture;
        }
        else
        {
            // We're not going to handle cube maps or other whacky
            // varieties of texture maps in this sample app, but if
            // you wanted to, you'd do it here.
        }
    }

    // And then the models.  Note that we'll track the maximum bone count here and the max
    //  vert count (in CreateMesh), since we need it for the shared buffer objects...
    g_scene.MaxBoneCount = 0;
    g_scene.MaxVertexCount = 0;
    for (int Model = 0; Model < g_scene.FileInfo->ModelCount; Model++)
    {
        granny_model *GrannyModel = g_scene.FileInfo->Models[Model];
        int const BoneCount = GrannyModel->Skeleton->BoneCount;

        model* NewModel = new model;
        g_scene.Models.push_back(NewModel);

        // Create the model instance and it's world pose buffer
        NewModel->GrannyInstance = GrannyInstantiateModel(GrannyModel);
        NewModel->WorldPose      = GrannyNewWorldPose(BoneCount);

        // Track the max bone count
        g_scene.MaxBoneCount = std::max(g_scene.MaxBoneCount, BoneCount);

        // Create the meshes for this model
        for (int Mesh = 0; Mesh < GrannyModel->MeshBindingCount; Mesh++)
        {
            mesh* NewMesh = CreateMesh(GrannyModel, Mesh);
            if (NewMesh)
                NewModel->Meshes.push_back(NewMesh);
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

    // Similarly, create a buffer large enough to hold the transformed verts for the
    // largest mesh.
    g_scene.TransformedBuffer = new granny_pnt332_vertex[g_scene.MaxVertexCount];


    // Initialize the camera.  We've transformed the file into units
    // of 1 unit = 1 meter, so just position the camera about 5 meters
    // back and 1 up, which should be reasonable for the purposes of
    // this test.
    GrannyInitializeDefaultCamera(&g_scene.Camera);
    g_scene.Camera.ElevAzimRoll[0] = float(-M_PI / 6);
    g_scene.Camera.Offset[2] = 2.f;
    g_scene.Camera.FarClipPlane  = 200.0f;
    g_scene.Camera.NearClipPlane = 0.1f;

    // IMPORTANT!  For the Wii, you have to change the output z range.
    g_scene.Camera.OutputZRange = GrannyCameraOutputZNegativeOneToZero;

    return true;
}


//-------------------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the world for the next frame
//-------------------------------------------------------------------------------------
void Update(granny_real32 const CurrentTime,
            granny_real32 const /*DeltaTime*/)
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
// Name: RenderModel()
// Desc: RenderModel() is the meat of the rendering in this app
//-------------------------------------------------------------------------------------
void RenderModel(model *Model)
{
    // Since I'm going to need it constantly, I dereference the composite
    // transform buffer for the model's current world-space pose.  This
    // buffer holds the transforms that move vertices from the position
    // in which they were modeled to their current position in world space.
    granny_matrix_4x4 *CompositeBuffer =
        GrannyGetWorldPoseComposite4x4Array(Model->WorldPose);

    // Now I render all the meshes.
    {for(size_t MeshIndex = 0;
         MeshIndex < Model->Meshes.size();
         ++MeshIndex)
    {
        mesh& Mesh = *Model->Meshes[MeshIndex];

        // Then, I dereference the index table that maps mesh bone indices
        // to bone indices in the model.
        granny_int32x const *ToBoneIndices =
            GrannyGetMeshBindingToBoneIndices(Mesh.GrannyBinding);

        // Next I load the mesh's vertex buffer, or deform
        // into a temporary buffer and load that, depending on
        // whether the mesh is rigid or not.
        int VertexCount = GrannyGetMeshVertexCount(Mesh.GrannyMesh);
        if(Mesh.GrannyDeformer)
        {
            // Now I tell Granny to deform the vertices of the mesh
            // with the current world pose of the model, and dump
            // the results into the vertex buffer.
            GrannyDeformVertices(Mesh.GrannyDeformer,
                                 ToBoneIndices, (float *)CompositeBuffer,
                                 VertexCount,
                                 GrannyGetMeshVertices(Mesh.GrannyMesh),
                                 g_scene.TransformedBuffer);
        }
        else
        {
            // If we got here, we had a bad mesh (the CreateMesh call
            // should've popped up an error message announcing this
            // earlier), so we skip it.
            continue;
        }

        // Now both the indices and vertices are loaded, so I can
        // render.  I grab the material groups and spin over them,
        // changing to the appropriate texture and rendering each batch.
        // A more savvy rendering loop might have instead built a
        // sorted list of material groups to minimize texture changes,
        // etc., but this is the most basic way to render.
        int GroupCount = GrannyGetMeshTriangleGroupCount(Mesh.GrannyMesh);
        granny_tri_material_group *Group =
            GrannyGetMeshTriangleGroups(Mesh.GrannyMesh);
        while(GroupCount--)
        {
            if(Group->MaterialIndex < int(Mesh.TextureReferences.size()))
            {
                texture *Texture =
                    Mesh.TextureReferences[(size_t)Group->MaterialIndex];
                if(Texture)
                {
                    GXLoadTexObj(&Texture->TexObj, GX_TEXMAP0);

                    int IndexCount = 3*Group->TriCount;
                    GXBegin(GX_TRIANGLES, GX_VTXFMT0, (u16)IndexCount);
                    unsigned int *Index =
                        &((unsigned int *)GrannyGetMeshIndices(Mesh.GrannyMesh))
                        [3*Group->TriFirst];
                    while(IndexCount--)
                    {
                        granny_pnt332_vertex &Vertex = g_scene.TransformedBuffer[*Index];
                        GXPosition3f32(Vertex.Position[0],
                                       Vertex.Position[1],
                                       Vertex.Position[2]);
                        GXNormal3f32(Vertex.Normal[0],
                                     Vertex.Normal[1],
                                     Vertex.Normal[2]);
                        GXTexCoord2f32(Vertex.UV[0], Vertex.UV[1]);
                        ++Index;
                    }
                    GXEnd();
                }
            }

            ++Group;
        }
    }}
}


//-------------------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-------------------------------------------------------------------------------------
static void
MakeGameCubeMatrix(float *From, float *To)
{
    To[0]  = From[0]; To[1]  = From[4]; To[2]  = From[8];  To[3]  = From[12];
    To[4]  = From[1]; To[5]  = From[5]; To[6]  = From[9];  To[7]  = From[13];
    To[8]  = From[2]; To[9]  = From[6]; To[10] = From[10]; To[11] = From[14];
    To[12] = From[3]; To[13] = From[7]; To[14] = From[11]; To[15] = From[15];
}

void Render()
{
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS,  GX_DIRECT);
    GXSetVtxDesc(GX_VA_NRM,  GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,  GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM,  GX_NRM_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST,  GX_F32, 0);

    //  Set the Texture Environment (Tev) Mode for stage 0
    GXSetNumTexGens(1);     // # of Tex gen
    GXSetNumChans(1);       // # of color channels
    GXSetNumTevStages(1);   // # of Tev Stage

    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);




    GXRenderModeObj* Mode = DEMOGetRenderModeObj();
    int Width  = Mode->viWidth;
    int Height = Mode->viHeight;

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
    {
        f32 Matrix[4][4];
        MakeGameCubeMatrix((float *)g_scene.Camera.Projection4x4, (float *)Matrix);
        GXSetProjection(Matrix, GX_PERSPECTIVE);

        MakeGameCubeMatrix((float *)g_scene.Camera.View4x4, (float *)Matrix);
        GXLoadPosMtxImm(Matrix, GX_PNMTX0);

        MakeGameCubeMatrix((float *)g_scene.Camera.InverseView4x4, (float *)Matrix);
        GXLoadNrmMtxImm(Matrix, GX_PNMTX0);
        GXSetCurrentMtx(GX_PNMTX0);
        GXSetCullMode(GX_CULL_FRONT);
    }

    // Loop over the models, and render their meshes
    for (size_t Idx = 0; Idx < g_scene.Models.size(); Idx++)
    {
        RenderModel(g_scene.Models[Idx]);
    }
}

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
int main()
{
    // Sets up the framebuffer, and initializes the heap
    DEMOInit(NULL);

#ifdef ENABLE_PROFILE
    {
        //Set up MEM2 buffer for profiler
        u32 bufferSize = (32*1024*1024);    //32MB
        void * buffer = OSAllocFromMEM2ArenaLo(bufferSize, 32);
        WIIPROFILER_Init(buffer, bufferSize, TRUE);
    }
#endif

    // Initialize the granny scene
    if( !InitGranny() || !InitScene() )
        return -1;

    granny_system_clock StartClock;
    GrannyGetSystemSeconds(&StartClock);

    GXColor color = { 0, 38, 255, 255};
    GXSetCopyClear(color, GX_MAX_Z24);

    granny_system_clock LastClock = StartClock;
    while (true)
    {
#ifdef ENABLE_PROFILE
        WIIPROFILER_MarkFrameBegin();    //Top of main loop
#endif

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
        DEMOBeforeRender();
        Render();
        DEMODoneRender();
    }

    return 0;
}
