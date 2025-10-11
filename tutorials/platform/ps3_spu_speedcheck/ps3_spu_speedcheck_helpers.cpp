// ========================================================================
// $File: //jeffr/granny_29/tutorial/platform/ps3_spu_speedcheck/ps3_spu_speedcheck_helpers.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "ps3_spu_speedcheck.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

//-------------------------------------------------------------------------------------
// Global PSGL variables
//-------------------------------------------------------------------------------------
char const *PixelShaderFile         = "/app_home/Shaders/Fragment.bin";
char const *VertexShaderRigidFile   = "/app_home/Shaders/VertexRigid.bin";
char const *VertexShaderSkinnedFile = "/app_home/Shaders/VertexSkinned.bin";
CGprogram g_VertexProgram_Skinned; // loaded vertex program (skinned mesh)
CGprogram g_VertexProgram_Rigid;   // loaded vertex program (rigid mesh)
CGprogram g_FragmentProgram;       // loaded fragment program

// The size of the constant matrix array for the shader
#define MAX_CONSTANT_MATRICES 46


// Keep it simple.
GLuint ScreenWidth = 1280;
GLuint ScreenHeight = 720;
void GetScreenDimensions(int* Width, int* Height)
{
    *Width  = (int)ScreenWidth;
    *Height = (int)ScreenHeight;
}



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
    PSGLinitOptions options;
    options.enable = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS;
    options.maxSPUs = 0;
    options.initializeSPUs = GL_FALSE;
    psglInit(&options);

    //psglInit(NULL);

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

texture*
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


mesh*
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

}

