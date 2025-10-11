// ========================================================================
// $File: //jeffr/granny_29/statement/ui_character_render.cpp $
// $DateTime: 2012/10/26 17:31:31 $
// $Change: 40090 $
// $Revision: #11 $
//
// $Notice: $
// ========================================================================
#include "ui_character_render.h"
#include "statement.h"

#include "statement_editstate.h"
#include "statement_undostack.h"
#include "statement_xinput.h"

#include "luautils.h"
#include "ui_area.h"
#include "ui_controls.h"
#include "ui_core.h"
#include "ui_drawing.h"

#include "granny_art_tool_info.h"
#include "granny_camera.h"
#include "granny_control.h"
#include "granny_controlled_animation.h"
#include "granny_data_type_conversion.h"
#include "granny_data_type_definition.h"
#include "granny_file_info.h"
#include "granny_local_pose.h"
#include "granny_math.h"
#include "granny_memory_ops.h"
#include "granny_mesh.h"
#include "granny_mesh_binding.h"
#include "granny_mesh_deformer.h"
#include "granny_model.h"
#include "granny_model_instance.h"
#include "granny_pose_cache.h"
#include "granny_skeleton.h"
#include "granny_track_mask.h"
#include "granny_tri_topology.h"
#include "granny_vertex_data.h"
#include "granny_world_pose.h"

#include "gstate_aimat_ik.h"
#include "gstate_character_instance.h"
#include "gstate_mask_invert.h"
#include "gstate_mask_source.h"
#include "gstate_mask_union.h"
#include "gstate_node_visitor.h"
#include "gstate_state_machine.h"
#include "gstate_twobone_ik.h"

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include <windows.h>
#include <gl/gl.h>
#include <math.h>
#include <vector>

USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;
using namespace std;

camera SceneCamera;
camera PreviewCamera;


BEGIN_GRANNY_NAMESPACE;
namespace edit
{
    extern state_machine*             CurrentRootMachine;
    extern container*                 CurrentContainer;
    extern model*                     Model;
    extern gstate_character_instance* CharacterInstance;
    extern vector<mesh_binding*>      BoundMeshes;

    bool  FileYIsUp      = false;
    float BaseCameraOffset = 10;
    float EstimatedNear    = 1;
    float EstimatedFar     = 1000;
    int   CameraZoomClick  = 10;

    ECameraUp CameraUpSetting = eUseFileSetting;

    bool  CameraTracks = true;
    matrix_4x4 CharacterBaseXForm = {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 }
    };
    matrix_4x4 CharacterXForm = {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 }
    };

    inline float ZoomDistance()
    {
        return BaseCameraOffset * pow(10, CameraZoomClick / 20.0f);
    }
}
END_GRANNY_NAMESPACE;
using namespace GRANNY edit;

struct custom_pndt3332_vertex
{
    real32 Position[3];
    real32 Normal[3];
    real32 UV[2];
    real32 Diffuse[3];
};
data_type_definition CustomPNDT3332VertexType[] = {
    { Real32Member, VertexPositionName,     0, 3 },
    { Real32Member, VertexNormalName,       0, 3 },
    { Real32Member, VertexDiffuseColorName, 0, 3 },
    { Real32Member, VertexTextureCoordinatesName "0", 0, 2 },
    { EndMember }
};

void GRANNY
ResetCharacterXForm()
{
    memset(CharacterXForm, 0, sizeof(CharacterXForm));

    if (Model)
    {
        BuildCompositeTransform4x4(Model->InitialPlacement, (real32*)CharacterXForm);
    }
    else
    {
        CharacterXForm[0][0] = 1;
        CharacterXForm[1][1] = 1;
        CharacterXForm[2][2] = 1;
        CharacterXForm[3][3] = 1;
    }

    memcpy(CharacterBaseXForm, CharacterXForm, sizeof(CharacterXForm));
}


static void
InitializeCamera()
{
    InitializeDefaultCamera(SceneCamera);
    SceneCamera.FOVY = 3.14159f / 3;
    SceneCamera.Orientation[0] = 0;
    SceneCamera.Orientation[1] = 0;
    SceneCamera.Orientation[2] = 0;
    SceneCamera.Orientation[3] = 1;
    SceneCamera.ElevAzimRoll[0]   = 3.14159f * (-7 % 200)/200;
    SceneCamera.ElevAzimRoll[1]   = (2 * 3.14159f) * (-14 % 200)/200;
    SceneCamera.NearClipPlane  = EstimatedNear;
    SceneCamera.FarClipPlane   = EstimatedFar;

    FileYIsUp = false;

    // Copy into the preview cam
    PreviewCamera = SceneCamera;

    // Reset the motion extracted position
    ResetCharacterXForm();
}


void GRANNY
UpdateCameraForFile(file_info* FileInfo)
{
    if (FileInfo == 0)
        return;

    InitializeCamera();

    if (FileInfo != 0 && FileInfo->ArtToolInfo != 0)
    {
        real32 const *UpVector = FileInfo->ArtToolInfo->UpVector;
        FileYIsUp = UpVector[1] > UpVector[2];
    }

    // Copy into the preview cam
    PreviewCamera = SceneCamera;
}


void GRANNY
ZoomToBoundingBox()
{
    if (!Model)
        return;

    world_pose* RestPose = NewWorldPose(Model->Skeleton->BoneCount);
    BuildRestWorldPose(*Model->Skeleton,
                       0, Model->Skeleton->BoneCount,
                       (real32*)CharacterXForm,
                       *RestPose);

    // First, let's *extract* the bounding box.
    bool   BoundsValid = false;
    real32 BoundsMin[3];
    real32 BoundsMax[3];

    for (size_t i = 0; i < BoundMeshes.size(); ++i)
    {
        mesh_binding* Binding = BoundMeshes[i];
        int32 const* Indices = GetMeshBindingToBoneIndices(*Binding);

        mesh* Mesh = GetMeshBindingSourceMesh(*Binding);
        int32x const NumVerts = GetMeshVertexCount(*Mesh);
        if (NumVerts == 0)
            continue;

        vector<custom_pndt3332_vertex> FinalVerts(NumVerts);
        mesh_deformer* Deformer = NewMeshDeformer(GetMeshVertexType(*Mesh),
                                                  CustomPNDT3332VertexType,
                                                  DeformPositionNormal,
                                                  AllowUncopiedTail);
        if (!Deformer)
            continue;

        DeformVertices(*Deformer, Indices,
                       (real32*)GetWorldPoseComposite4x4Array(*RestPose),
                       NumVerts,
                       GetMeshVertices(*Mesh),
                       &FinalVerts[0]);
        FreeMeshDeformer(Deformer);

        if (BoundsValid == false)
        {
            BoundsMin[0] = FinalVerts[0].Position[0];
            BoundsMin[1] = FinalVerts[0].Position[1];
            BoundsMin[2] = FinalVerts[0].Position[2];

            BoundsMax[0] = FinalVerts[0].Position[0];
            BoundsMax[1] = FinalVerts[0].Position[1];
            BoundsMax[2] = FinalVerts[0].Position[2];
            BoundsValid = true;
        }

        {for (size_t Idx = 1; Idx < FinalVerts.size(); ++Idx)
        {
            for (int k = 0; k < 3; ++k)
            {
                BoundsMin[k] = min(BoundsMin[k], FinalVerts[Idx].Position[k]);
                BoundsMax[k] = max(BoundsMax[k], FinalVerts[Idx].Position[k]);
            }
        }}
    }

    if (BoundsValid)
    {
        // Todo: holy hackballs!
        if (SceneCamera.Orientation[0] != 0.0f)
        {
            SceneCamera.Position[2] = BoundsMin[2] + (BoundsMax[2] - BoundsMin[2]) * 0.5f;
        }
        else
        {
            SceneCamera.Position[1] = BoundsMin[1] + (BoundsMax[1] - BoundsMin[1]) * 0.5f;
        }

        // Start with the minimum offset we plan to allow
        SceneCamera.Offset[2] = 0.001f;

        // Refresh the camera location and such
        BuildCameraMatrices(SceneCamera);

        // Make sure we can see the entire ABB
        EnsureCameraSeesPoint(SceneCamera, BoundsMin);
        EnsureCameraSeesPoint(SceneCamera, BoundsMax);

        CameraZoomClick = 0;
        BaseCameraOffset  = SceneCamera.Offset[2];
        EstimatedNear     = BaseCameraOffset / 20.0f;
        EstimatedFar      = BaseCameraOffset * 40.0f;

        // Copy into the preview cam
        PreviewCamera = SceneCamera;
    }

    FreeWorldPose(RestPose);
}

void GRANNY
GetCharacterXForm(real32* XForm)
{
    memcpy(XForm, CharacterXForm, sizeof(CharacterXForm));
}

void GRANNY
SetCharacterXForm(real32 const* XForm)
{
    memcpy(CharacterXForm, XForm, sizeof(CharacterXForm));
}


void GRANNY
GetSceneCamera(camera* Camera, bool* Tracks)
{
    *Camera = SceneCamera;
    *Tracks = CameraTracks;
}

void GRANNY
SetSceneCamera(camera const& Camera, bool Tracks)
{
    SceneCamera  = Camera;
    CameraTracks = Tracks;
}


static bool GetCameraYUp()
{
    if (CameraUpSetting != eUseFileSetting)
    {
        return (CameraUpSetting == eUseYUp);
    }

    return FileYIsUp;
}

static void
UpdateCameraForScreen(LuaRect& ScreenRect, camera& Camera)
{
    SetCameraAspectRatios(Camera,
                          ScreenRect.w / real32(ScreenRect.h),
                          (real32)ScreenRect.w, (real32)ScreenRect.h,
                          (real32)ScreenRect.w, (real32)ScreenRect.h);

    Camera.Offset[2]     = ZoomDistance();
    Camera.NearClipPlane = EstimatedNear;
    Camera.FarClipPlane  = EstimatedFar;

    // Use y or z as the up vector...
    bool UseYUp = GetCameraYUp();
    if (UseYUp)
        VectorIdentityQuaternion(Camera.Orientation);
    else
        VectorSet4(Camera.Orientation, Sin(0.25f*Pi32), 0.0f, 0.0f, Cos(0.25f*Pi32));

    BuildCameraMatrices(Camera);
}


static void
DrawCoordinateFrame(camera& Camera, int32x w, int32x h, real32 X, real32 Y)
{
    return;
    float Screen[3] = { X, Y, 0 };
    float World[3];
    WindowSpaceToWorldSpace(Camera, (real32)w, (real32)h, Screen, World);

    glBegin(GL_LINES);
    {
        glColor3f(1, 0, 0); glVertex3fv(World); glVertex3f(World[0] + 1.f, World[1], World[2]);
        glColor3f(0, 1, 0); glVertex3fv(World); glVertex3f(World[0], World[1] + 1.f, World[2]);
        glColor3f(0, 0, 1); glVertex3fv(World); glVertex3f(World[0], World[1], World[2] + 1.f);
    }
    glEnd();
}


static void
SetupLighting()
{
    glEnable(GL_NORMALIZE);
    glDisable(GL_LIGHT0);
    glDisable(GL_LIGHT1);
    glDisable(GL_LIGHT2);
    glDisable(GL_LIGHT3);
    glDisable(GL_LIGHT4);

    glEnable(GL_LIGHTING);

    static bool AmbientLight     = true;
    static bool DirectionalLight = false;
    static bool PrimaryLight     = false;
    static bool BackLight        = false;
    static bool FillLight        = false;

    GLfloat LightOrientation[4] = { 0, 0, 0, 1 };

    if(AmbientLight)
    {
        GLfloat const LightAmbient[4] = {0.75f, 0.75f, 0.75f, 1};
        glLightfv(GL_LIGHT0, GL_AMBIENT, LightAmbient);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
    }

    if(DirectionalLight)
    {
        GLfloat LightPosition[4] = {1.0f, 1.0f, 1.0f, 0.0f};
        NormalQuaternionTransform3(LightPosition, LightOrientation);

        GLfloat const LightDiffuse[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        GLfloat const LightSpecular[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);
        glLightfv(GL_LIGHT1, GL_SPECULAR, LightSpecular);
        glLightfv(GL_LIGHT1, GL_POSITION, LightPosition);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT1);
    }

    if(PrimaryLight)
    {
        GLfloat LightPosition[4] = {-1.0f, 1.0f, 1.0f, 0.0f};
        NormalQuaternionTransform3(LightPosition, LightOrientation);

        GLfloat const LightDiffuse[4] = {1.0f, 0.8f, 0.5f, 1.0f};
        GLfloat const LightSpecular[4] = {1.0f, 0.8f, 0.5f, 1.0f};
        glLightfv(GL_LIGHT2, GL_DIFFUSE, LightDiffuse);
        glLightfv(GL_LIGHT2, GL_SPECULAR, LightSpecular);
        glLightfv(GL_LIGHT2, GL_POSITION, LightPosition);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT2);
    }

    if(FillLight)
    {
        GLfloat LightPosition[4] = {1.0f, -1.0f, 1.0f, 0.0f};
        NormalQuaternionTransform3(LightPosition, LightOrientation);

        GLfloat const LightDiffuse[4] = {0.4f, 0.7f, 1.0f, 1.0f};
        GLfloat const LightSpecular[4] = {0.4f, 0.7f, 1.0f, 1.0f};
        glLightfv(GL_LIGHT3, GL_DIFFUSE, LightDiffuse);
        glLightfv(GL_LIGHT3, GL_SPECULAR, LightSpecular);
        glLightfv(GL_LIGHT3, GL_POSITION, LightPosition);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT3);
    }

    if(BackLight)
    {
        GLfloat LightPosition[4] = {1.0f, 0.0f, -1.0f, 0.0f};
        NormalQuaternionTransform3(LightPosition, LightOrientation);

        GLfloat const LightDiffuse[4] = {0.5f, 0.5f, 0.5f, 1.0f};
        GLfloat const LightSpecular[4] = {0.5f, 0.5f, 0.5f, 1.0f};
        glLightfv(GL_LIGHT4, GL_DIFFUSE, LightDiffuse);
        glLightfv(GL_LIGHT4, GL_SPECULAR, LightSpecular);
        glLightfv(GL_LIGHT4, GL_POSITION, LightPosition);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT4);
    }
}


static void
AlterVertsForMorph(mesh& Mesh, data_type_definition* VertexType, uint8* FinalVerts)
{
    Assert(CurrentContainer);
    Assert(Mesh.MorphTargetCount != 0);

    // We can only do real32 morphs for the moment.
    {
        data_type_definition* Walk = VertexType;
        while (Walk->Type != EndMember)
        {
            if (Walk->Type != Real32Member)
                return;

            ++Walk;
        }
    }

    int32x VertexCount      = GetMeshVertexCount(Mesh);
    int32x FinalVertexSize  = GetTotalObjectSize(VertexType);
    Assert((FinalVertexSize%4) == 0);
    int32x ElementCount     = FinalVertexSize / sizeof(real32);

    vector<uint8> MorphVerts(VertexCount*FinalVertexSize);
    for (int Idx = 0; Idx < CurrentContainer->GetNumOutputs(); ++Idx)
    {
        if (CurrentContainer->IsOutputExternal(Idx) == false)
            continue;

        // Ignore all but morphs...
        if (CurrentContainer->GetOutputType(Idx) != MorphEdge)
            continue;

        // Name must match for now..
        if (_stricmp(CurrentContainer->GetOutputName(Idx), Mesh.Name) != 0)
            continue;

        int32x NumChannels = CurrentContainer->GetNumMorphChannels(Idx);
        if (NumChannels == -1)
            continue;

        if (NumChannels != Mesh.MorphTargetCount)
            continue;

        vector<real32> Channels(NumChannels, 0);
        if (!CurrentContainer->SampleMorphOutput(Idx, CharacterInstance->CurrentTime,
                                                 &Channels[0], NumChannels))
        {
            continue;
        }

        for (int MorphIdx = 0; MorphIdx < NumChannels; ++MorphIdx)
        {
            CopyMeshMorphVertices(Mesh, MorphIdx, VertexType, &MorphVerts[0]);

            // Hack-o
            real32*       TargetVerts = (real32*)FinalVerts;
            real32 const* SourceVerts = (real32 const*)&MorphVerts[0];
            for (int VertElemIdx = 0; VertElemIdx < VertexCount * ElementCount; ++VertElemIdx)
            {
                TargetVerts[VertElemIdx] += Channels[MorphIdx] * SourceVerts[VertElemIdx];
            }
        }
    }
}


static void
RenderBoundMesh(mesh_binding* Binding, world_pose* Pose, bool IgnoreLighting)
{
    extern real32 CurrT;

    Assert(Binding);
    Assert(Pose);

    mesh& Mesh = *GetMeshBindingSourceMesh(*Binding);
    int32x const NumVerts = GetMeshVertexCount(Mesh);
    vector<custom_pndt3332_vertex> FinalVerts(NumVerts);

    int32 const* Indices = GetMeshBindingToBoneIndices(*Binding);

    if (MeshIsRigid(Mesh))
    {
        CopyMeshVertices(Mesh, CustomPNDT3332VertexType, &FinalVerts[0]);

        if (Mesh.MorphTargetCount != 0)
        {
            AlterVertsForMorph(Mesh, CustomPNDT3332VertexType, (uint8*)&FinalVerts[0]);
        }

        real32* CompositeMatrix = GetWorldPoseComposite4x4(*Pose, Indices[0]);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glMultMatrixf(CompositeMatrix);
    }
    else
    {
        mesh_deformer* Deformer = NewMeshDeformer(GetMeshVertexType(Mesh),
                                                  CustomPNDT3332VertexType,
                                                  DeformPositionNormal,
                                                  AllowUncopiedTail);
        if (!Deformer)
            return;

        data_type_definition* VertexType = GetMeshVertexType(Mesh);
        int32x VertexSize = GetTotalObjectSize(VertexType);
        int32x VertexCount = GetMeshVertexCount(Mesh);

        vector<uint8> Predeformation(VertexSize*VertexCount);
        CopyMeshVertices(Mesh, VertexType, &Predeformation[0]);

        // Morph the predeformation 
        if (Mesh.MorphTargetCount != 0)
        {
            AlterVertsForMorph(Mesh, VertexType, &Predeformation[0]);
        }

        
        DeformVertices(*Deformer, Indices,
                       (real32*)GetWorldPoseComposite4x4Array(*Pose),
                       NumVerts,
                       &Predeformation[0],
                       &FinalVerts[0]);

        FreeMeshDeformer(Deformer);

        // Copy over the texture coordinates.  This isn't super awesome, but it works
        variant Tex;
        if (FindMatchingMember(GetMeshVertexType(Mesh), GetMeshVertices(Mesh),
                               VertexTextureCoordinatesName "0", &Tex))
        {
            int32x SourceVertSize = GetTotalObjectSize(GetMeshVertexType(Mesh));
            uint8 const* BasePtr = (uint8 const*)Tex.Object;

            {for (int32x Idx = 0; Idx < NumVerts; ++Idx)
            {
                memcpy(FinalVerts[Idx].UV, BasePtr, sizeof(real32)*2);
                BasePtr += SourceVertSize;
            }}
        }
    }

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    if (!IgnoreLighting)
    {
        glDisable(GL_BLEND);
    }

    // Alter the colors of the mesh if we have an active trackmask output edge.  This also
    // controls whether or not we turn on the mesh lighting...
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, sizeof(custom_pndt3332_vertex), &FinalVerts[0].Position[0]);
        glNormalPointer(GL_FLOAT, sizeof(custom_pndt3332_vertex), &FinalVerts[0].Normal[0]);
        glTexCoordPointer(2, GL_FLOAT, sizeof(custom_pndt3332_vertex), &FinalVerts[0].UV[0]);

        // light normally
        if (!IgnoreLighting)
        {
            SetupLighting();
        }
    }

    uint8 const* MeshIndices   = (uint8 const*)GetMeshIndices(Mesh);
    int32 const  BytesPerIndex = GetMeshBytesPerIndex(Mesh);

    {for (int Idx = 0; Idx < Mesh.PrimaryTopology->GroupCount; ++Idx)
    {
        tri_material_group const& Group = Mesh.PrimaryTopology->Groups[Idx];

        // Todo: texture
        // if (Group.MaterialIndex < Mesh.MaterialBindingCount)
        // {
        //     int Tex = GetDiffuseFor(State, Mesh.MaterialBindings[Group.MaterialIndex].Material);
        //     glEnable(GL_TEXTURE_2D);
        //     glBindTexture(GL_TEXTURE_2D, Tex);
        // }
        // else
        {
            glDisable(GL_TEXTURE_2D);
        }

        glDrawElements(GL_TRIANGLES, Group.TriCount * 3,
                       (BytesPerIndex == 2) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                       MeshIndices + ((Group.TriFirst * 3) * BytesPerIndex));
        // glDisable(GL_TEXTURE_2D);
    }}

    glPopClientAttrib();
    glPopAttrib();

    if (MeshIsRigid(Mesh))
    {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
}

// only handle up to 4...
struct SimpleWeights
{
    uint8 BoneWeights[4];
    uint8 BoneIndices[4];
};
data_type_definition SimpleWeightsType[] =
{
    {NormalUInt8Member, VertexBoneWeightsName, 0, 4},
    {UInt8Member, VertexBoneIndicesName, 0, 4},
    {EndMember},
};

static void
RenderMeshMask(mesh_binding* Binding, world_pose* Pose, track_mask* Mask)
{
    vector<real32> Colors;

    mesh& Mesh = *GetMeshBindingSourceMesh(*Binding);
    int32x const NumVerts = GetMeshVertexCount(Mesh);

    int32x const* BoneIndices = GetMeshBindingToBoneIndices(*Binding);

    Colors.resize(NumVerts * 3, 0);
    if (MeshIsRigid(Mesh))
    {
        // Is the bone this mesh is attached to lit?  Use that as the constant color...

    }
    else
    {
        // Otherwise, we've got work to do!
        vector<SimpleWeights> Weights(NumVerts);
        CopyMeshVertices(Mesh, SimpleWeightsType, &Weights[0]);


        // In mesh order...
        vector<real32> MaskWeights(Mesh.BoneBindingCount);
        for (int Idx = 0; Idx < Mesh.BoneBindingCount; ++Idx)
        {
            MaskWeights[Idx] = GetTrackMaskBoneWeight(*Mask, BoneIndices[Idx]);
        }

        for (int Idx = 0; Idx < NumVerts; ++Idx)
        {
            float v = 0;
            float w = 0;
            for (int WIdx = 0; WIdx < 4; ++WIdx)
            {
                v += MaskWeights[Weights[Idx].BoneIndices[WIdx]] * Weights[Idx].BoneWeights[WIdx];
                w += Weights[Idx].BoneWeights[WIdx];
            }
            v /= w;

            Colors[Idx * 3 + 0] = v*0.75f + 0.25f;
            Colors[Idx * 3 + 1] = v*0.75f + 0.25f;
            Colors[Idx * 3 + 2] = v*0.75f + 0.25f;
        }
    }

    glColorPointer(3, GL_FLOAT, sizeof(real32)*3, &Colors[0]);
    glEnableClientState(GL_COLOR_ARRAY);

    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_ZERO);

    RenderBoundMesh(Binding, Pose, true);

    glDepthFunc(GL_LESS);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDisable(GL_BLEND);
    glDisableClientState(GL_COLOR_ARRAY);
}

extern float g_GlobalClock;
extern float g_GlobalDelta;

real32 GRANNY
GetGridSpacing()
{
    // Those aren't magic numbers.  They're voodoo numbers.
    // @@ todo casey hotvals
    return BaseCameraOffset * 0.075f * 4.f;
}


static void
DrawGroundPlane(real32 const* Placement)
{
    StTMZone(__FUNCTION__);

    real32 Val   = GetGridSpacing();
    real32 Val10 = Val * 10.0f;

    real32 ModPosition[3] = {
        Placement[12] + fmod(-Placement[12], Val),
        Placement[13] + fmod(-Placement[13], Val),
        Placement[14] + fmod(-Placement[14], Val),
    };
    real32 Mod10Position[3] = {
        Placement[12] + fmod(-Placement[12], Val10),
        Placement[13] + fmod(-Placement[13], Val10),
        Placement[14] + fmod(-Placement[14], Val10),
    };
    if (GetCameraYUp())
    {
        real32 BaseDistToGround = CharacterBaseXForm[3][1];

        ModPosition[1]   = Placement[13] - BaseDistToGround;
        Mod10Position[1] = Placement[13] - BaseDistToGround;
    }
    else
    {
        real32 BaseDistToGround = CharacterBaseXForm[3][2];

        ModPosition[2]   = Placement[14] - BaseDistToGround;
        Mod10Position[2] = Placement[14] - BaseDistToGround;
    }

    glDisable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    glDisable(GL_TEXTURE_2D);

    int32x const NumSmallGrids = 30;
    int32x const NumLargeGrids = NumSmallGrids / 10;

    glEnable(GL_BLEND);
    glLineWidth(1);

    float MinPos[3];
    float MaxPos[3];
    if (GetCameraYUp())
    {
        MinPos[0] = ModPosition[0] + -NumSmallGrids * Val;
        MaxPos[0] = ModPosition[0] +  NumSmallGrids * Val;

        MinPos[1] = ModPosition[1];
        MaxPos[1] = ModPosition[1];

        MinPos[2] = ModPosition[2] + -NumSmallGrids * Val;
        MaxPos[2] = ModPosition[2] +  NumSmallGrids * Val;
    }
    else
    {
        MinPos[0] = ModPosition[0] + -NumSmallGrids * Val;
        MaxPos[0] = ModPosition[0] +  NumSmallGrids * Val;

        MinPos[1] = ModPosition[1] + -NumSmallGrids * Val;
        MaxPos[1] = ModPosition[1] +  NumSmallGrids * Val;

        MinPos[2] = ModPosition[2];
        MaxPos[2] = ModPosition[2];
    }

    glColor4f(1, 1, 1, 0.55f);
    glBegin(GL_LINES);
    for (int i = -NumLargeGrids; i < NumLargeGrids; ++i)
    {
        if (GetCameraYUp())
        {
            float X = Mod10Position[0] + i * Val10;
            if (X >= MinPos[0] && X <= MaxPos[0])
            {
                glVertex3f(Mod10Position[0] + i * Val10, Mod10Position[1], MinPos[2]);
                glVertex3f(Mod10Position[0] + i * Val10, Mod10Position[1], MaxPos[2]);
            }

            float Z = Mod10Position[2] + i * Val10;
            if (Z >= MinPos[2] && Z <= MaxPos[2])
            {
                glVertex3f(MinPos[0], Mod10Position[1], Mod10Position[2] + i * Val10);
                glVertex3f(MaxPos[0], Mod10Position[1], Mod10Position[2] + i * Val10);
            }
        }
        else
        {
            float X = Mod10Position[0] + i * Val10;
            if (X >= MinPos[0] && X <= MaxPos[0])
            {
                glVertex3f(Mod10Position[0] + i * Val10, MinPos[1], Mod10Position[2]);
                glVertex3f(Mod10Position[0] + i * Val10, MaxPos[1], Mod10Position[2]);
            }

            float Y = Mod10Position[1] + i * Val10;
            if (Y >= MinPos[1] && Y <= MaxPos[1])
            {
                glVertex3f(MinPos[0], Mod10Position[1] + i * Val10, Mod10Position[2]);
                glVertex3f(MaxPos[0], Mod10Position[1] + i * Val10, Mod10Position[2]);
            }
        }
    }
    glEnd();

    glColor4f(0.5, 0.5, 0.5, 0.5f);
    glBegin(GL_LINES);
    for (int i = -NumSmallGrids; i < NumSmallGrids; ++i)
    {
        if (GetCameraYUp())
        {
            glVertex3f(ModPosition[0] + i * Val, ModPosition[1], MinPos[2]);
            glVertex3f(ModPosition[0] + i * Val, ModPosition[1], MaxPos[2]);
            glVertex3f(MinPos[0],                ModPosition[1], ModPosition[2] + i * Val);
            glVertex3f(MaxPos[0],                ModPosition[1], ModPosition[2] + i * Val);
        }
        else
        {
            glVertex3f(ModPosition[0] + i * Val, ModPosition[1] + -NumSmallGrids * Val, ModPosition[2]);
            glVertex3f(ModPosition[0] + i * Val, ModPosition[1] +  NumSmallGrids * Val, ModPosition[2]);
            glVertex3f(MinPos[0], ModPosition[1] + i * Val, MinPos[2]);
            glVertex3f(MaxPos[0], ModPosition[1] + i * Val, MaxPos[2]);
        }
    }
    glEnd();
}

class activenode_visitor : public node_visitor
{
    lua_State*  LuaState;
    world_pose* CurrentPose;
    char const* ParentID;

public:
    virtual void VisitNode(aimat_ik* AimAt);
    virtual void VisitNode(twobone_ik* TwoBone);

    virtual void VisitNode(mask_source* Mask);
    virtual void VisitNode(mask_union* Mask);
    virtual void VisitNode(mask_invert* Mask);

    activenode_visitor(lua_State*  L,
                       char const* ParID,
                       world_pose* Pose)
      : LuaState(L), ParentID(ParID), CurrentPose(Pose)
    {
        // nada
    }
};

static void
MaskedCharOverlay(node* MaskNode,
                  int32x OnOutput,
                  world_pose* CurrentPose)
{
    gstate_character_instance* Instance = MaskNode->GetBoundCharacter();
    if (!Instance)
        return;

    model* Model = (model*)GetSourceModelForCharacter(Instance);
    if (!Model)
        return;

    track_mask* Mask = NewTrackMask(0, Model->Skeleton->BoneCount);
    if (MaskNode->SampleMaskOutput(0, 0, (granny_track_mask*)Mask))
    {
        for (size_t i = 0; i < BoundMeshes.size(); ++i)
            RenderMeshMask(BoundMeshes[i], CurrentPose, Mask);
    }

    FreeTrackMask(Mask);
}

void
activenode_visitor::VisitNode(mask_source* Mask)
{
    MaskedCharOverlay(Mask, 0, CurrentPose);
}

void
activenode_visitor::VisitNode(mask_union* Mask)
{
    MaskedCharOverlay(Mask, 0, CurrentPose);
}

void
activenode_visitor::VisitNode(mask_invert* Mask)
{
    MaskedCharOverlay(Mask, 0, CurrentPose);
}

void
activenode_visitor::VisitNode(aimat_ik* AimAt)
{
    if (AimAt->IsActive() == false)
        return;

    if (!Model)
        return;

    skeleton* Skeleton = Model->Skeleton;
    Assert(Skeleton);

    granny_real32 Position[3];
    AimAt->GetDefaultAimPosition(Position);

    real32 LineLength = 0.25f * GetGridSpacing();

    ControlIDGen IDGen("aimat_handle");
    if (DraggableLocation(Position, IDGen, LineLength))
    {
        PushUndoPos("Changing aim point");
        AimAt->SetDefaultAimPosition(Position);
    }

    // And now pull the pieces of the skeleton so we can draw the axes and chains...
    Assert(AimAt->GetHeadName());
    Assert(AimAt->GetFirstActiveName());
    Assert(AimAt->GetLastActiveName());

    int32x HeadIndex     = AimAt->GetBoundHeadIndex();
    int32x LinkCount     = AimAt->GetBoundLinkCount();
    int32x InactiveLinks = AimAt->GetBoundInactiveLinkCount();
    Assert(HeadIndex > 0);
    Assert(LinkCount > 0);
    Assert(InactiveLinks >= 0 && InactiveLinks < LinkCount);

    real32 const* AimAxis = AimAt->GetAimAxisFloat();
    real32 const* EarAxis = AimAt->GetEarAxisFloat();
    real32 const* GNormal = AimAt->GetGroundNormalFloat();

    // Draw the aim axis
    glDisable(GL_LINE_SMOOTH);
    glLineWidth(3);
    {
        real32 const* BaseMat = GetWorldPose4x4(*CurrentPose, 0);
        real32 const* PoseMat = GetWorldPose4x4(*CurrentPose, HeadIndex);

        real32 LocalAimAxis[3];
        VectorTransform4x3(LocalAimAxis, AimAxis, 0, PoseMat);

        real32 LocalEarAxis[3];
        VectorTransform4x3(LocalEarAxis, EarAxis, 0, PoseMat);

        glDisable(GL_DEPTH_TEST);
        glBegin(GL_LINES);
        {
            // Aim
            glColor3f(1, 1, 0);
            glVertex3fv(PoseMat + 12);
            glVertex3f(PoseMat[12] + LineLength * LocalAimAxis[0],
                       PoseMat[13] + LineLength * LocalAimAxis[1],
                       PoseMat[14] + LineLength * LocalAimAxis[2]);

            // Ear
            glColor3f(0, 1, 1);
            glVertex3fv(PoseMat + 12);
            glVertex3f(PoseMat[12] + LineLength * LocalEarAxis[0],
                       PoseMat[13] + LineLength * LocalEarAxis[1],
                       PoseMat[14] + LineLength * LocalEarAxis[2]);

            glColor3f(1, 0, 1);
            glVertex3fv(BaseMat + 12);
            glVertex3f(BaseMat[12] + LineLength * GNormal[0],
                       BaseMat[13] + LineLength * GNormal[1],
                       BaseMat[14] + LineLength * GNormal[2]);
        }
        glEnd();
    }

    // And the skeletal chain
    glLineWidth(1);
    {
        granny_int32x Walk = HeadIndex;

        glDisable(GL_DEPTH_TEST);
        glBegin(GL_LINE_STRIP);
        {
            glColor3f(1, 0, 0);
            glVertex3fv(GetWorldPose4x4(*CurrentPose, Walk) + 12);

            for (int i = 0; i < LinkCount; ++i)
            {
                Walk = Skeleton->Bones[Walk].ParentIndex;
                Assert(Walk != NoParentBone);

                if (i < InactiveLinks)
                    glColor3f(1, 0, 0);
                else
                    glColor3f(0, 0, 1);

                glVertex3fv(GetWorldPose4x4(*CurrentPose, Walk) + 12);
            }
        }
        glEnd();
    }

    // And the skeletal chain
    glLineWidth(2);
    {
        granny_int32x Walk = HeadIndex;

        glBegin(GL_LINES);
        {
            for (int i = 0; i < LinkCount; ++i)
            {
                Walk = Skeleton->Bones[Walk].ParentIndex;
                Assert(Walk != NoParentBone);

                granny_real32 const* PoseMat = GetWorldPose4x4(*CurrentPose, Walk);
                {
                    // Aim
                    glColor3f(1, 0, 0);
                    glVertex3fv(PoseMat + 12);
                    glVertex3f(PoseMat[12] + LineLength * 0.5f * PoseMat[0 + 0],
                               PoseMat[13] + LineLength * 0.5f * PoseMat[4 + 0],
                               PoseMat[14] + LineLength * 0.5f * PoseMat[8 + 0]);

                    // Ear
                    glColor3f(0, 1, 0);
                    glVertex3fv(PoseMat + 12);
                    glVertex3f(PoseMat[12] + LineLength * 0.5f * PoseMat[0 + 1],
                               PoseMat[13] + LineLength * 0.5f * PoseMat[4 + 1],
                               PoseMat[14] + LineLength * 0.5f * PoseMat[8 + 1]);

                    glColor3f(0, 0, 1);
                    glVertex3fv(PoseMat + 12);
                    glVertex3f(PoseMat[12] + LineLength * 0.5f * PoseMat[0 + 2],
                               PoseMat[13] + LineLength * 0.5f * PoseMat[4 + 2],
                               PoseMat[14] + LineLength * 0.5f * PoseMat[8 + 2]);
                }
            }
        }
        glEnd();
    }
}

void
activenode_visitor::VisitNode(twobone_ik* TwoBone)
{
    if (TwoBone->IsActive() == false)
        return;

    if (!Model)
        return;

    skeleton* Skeleton = Model->Skeleton;
    Assert(Skeleton);

    granny_real32 Position[3];
    TwoBone->GetDefaultFootPosition(Position);

    real32 Length = GetGridSpacing() * 0.15f;

    ControlIDGen IDGen("twobone_handle");
    if (DraggableLocation(Position, IDGen, Length))
    {
        PushUndoPos("Changing foot point");
        TwoBone->SetDefaultFootPosition(Position);
    }

    // And now pull the pieces of the skeleton so we can draw the axes and chains...
    int32x FootIndex = TwoBone->GetBoundFootIndex();
    int32x KneeIndex = TwoBone->GetBoundKneeIndex();
    int32x HipIndex  = TwoBone->GetBoundHipIndex();

    // And the skeletal chain
    {
        glDisable(GL_LINE_SMOOTH);
        glLineWidth(3);
        glDisable(GL_DEPTH_TEST);
        glBegin(GL_LINE_STRIP);
        {
            glColor3f(1, 0, 0);

            glVertex3fv(GetWorldPose4x4(*CurrentPose, FootIndex) + 12);
            glVertex3fv(GetWorldPose4x4(*CurrentPose, KneeIndex) + 12);
            glVertex3fv(GetWorldPose4x4(*CurrentPose, HipIndex)  + 12);
        }
        glEnd();


        // Draw the axis for the knee so the correct plane can be selected...
        glLineWidth(6);
        {
            real32 const* KneePlane = TwoBone->GetKneePlaneFloat();
            real32 const* KneeMat   = GetWorldPose4x4(*CurrentPose, KneeIndex);

            // real32 LocalKneeAxis[3];
            // VectorTransform4x3(LocalKneeAxis, KneePlane, 0, KneeMat);

            glBegin(GL_LINES);
            glColor3f(0, 1, 0);
            glVertex3fv(GetWorldPose4x4(*CurrentPose, KneeIndex) + 12);
            // glVertex3f(KneeMat[12] + Length * LocalKneeAxis[0],
            //            KneeMat[13] + Length * LocalKneeAxis[1],
            //            KneeMat[14] + Length * LocalKneeAxis[2]);
            glVertex3f(KneeMat[12] + Length * KneePlane[0],
                       KneeMat[13] + Length * KneePlane[1],
                       KneeMat[14] + Length * KneePlane[2]);
            glEnd();
        }

    }
}

static int
ActiveNodeInterface(lua_State* L,
                    world_pose* CurrentPose)
{
    char const* ParentID = lua_tostring(L, -1);

    StTMZone("ActiveNodeInterface");

    std::vector<tokenized*> Selection;
    if (!edit::GetSelection(Selection) || Selection.size() != 1)
        return 0;

    node* ActiveNode = GSTATE_DYNCAST(Selection[0], node);
    if (ActiveNode == 0)
        return 0;

    LuaRect RenderRect = UIAreaGet();
    UpdateCameraForScreen(RenderRect, SceneCamera);

    glPushAttrib(GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT | GL_VIEWPORT_BIT);
    glDepthMask(GL_TRUE);

    // Setup the screen viewport.  Note that this completely takes over the GUI setup, so
    // we need to restore that at the end...
    {
        int screenW, screenH;
        UIGetSize(&screenW, &screenH);
        glViewport(RenderRect.x, (screenH - RenderRect.y) - RenderRect.h, RenderRect.w, RenderRect.h);
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf((real32 *)SceneCamera.Projection4x4);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf((real32 *)SceneCamera.View4x4);

    activenode_visitor Visitor(L, ParentID, CurrentPose);
    ActiveNode->AcceptNodeVisitor(&Visitor);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glPopAttrib();

    glDepthMask(GL_FALSE);
    return 0;
}

static int
l_CharacterRender(lua_State* L)
{
    StTMZone("Character Render");

    LuaRect RenderRect = UIAreaGet();
    // Todo: clipping?
    {
        LuaPoint UL(RenderRect.x, RenderRect.y);
        AreaToScreen(UL);
        RenderRect.x = UL.x;
        RenderRect.y = UL.y;
    }

    glPushAttrib(GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT | GL_VIEWPORT_BIT);
    glDepthMask(GL_TRUE);

    // Setup the screen viewport.  Note that this completely takes over the GUI setup, so
    // we need to restore that at the end...
    {
        int screenW, screenH;
        UIGetSize(&screenW, &screenH);
        glViewport(RenderRect.x, (screenH - RenderRect.y) - RenderRect.h, RenderRect.w, RenderRect.h);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (Model)
    {
        skeleton* Skeleton = Model->Skeleton;
        pose_cache* Cache = NewPoseCache();
        local_pose* IntoPose = 0;

        if (CharacterInstance)
        {
            if (g_GlobalDelta != 0)
            {
                GStateAdvanceTime(CharacterInstance, g_GlobalDelta);
                ProcessXInput(g_GlobalClock, g_GlobalDelta);
            }
            else
            {
                CharacterInstance->DeltaT = 0.0f;
            }

            Assert(CurrentContainer);
            int ExternalOutput = CurrentContainer->GetNthExternalOutput(0);

            IntoPose =
                (local_pose*)CurrentContainer->SamplePoseOutput(ExternalOutput,
                                                                CharacterInstance->CurrentTime,
                                                                0.0f,
                                                                (granny_pose_cache*)Cache);
            real32 OldPosition[3] = {
                CharacterXForm[3][0],
                CharacterXForm[3][1],
                CharacterXForm[3][2]
            };

            real32 Translation[3] = { 0, 0, 0 };
            real32 Rotation[3] = { 0, 0, 0 };
            CurrentContainer->GetRootMotionVectors(ExternalOutput,
                                                   CharacterInstance->CurrentTime,
                                                   CharacterInstance->DeltaT,
                                                   Translation, Rotation, false);
            GrannyApplyRootMotionVectorsToMatrix((granny_real32*)CharacterXForm,
                                                 Translation, Rotation,
                                                 (granny_real32*)CharacterXForm);

            // Move the camera, this should be equiv to Translation[]
            if (CameraTracks)
            {
                SceneCamera.Position[0] += (CharacterXForm[3][0] - OldPosition[0]);
                SceneCamera.Position[1] += (CharacterXForm[3][1] - OldPosition[1]);
                SceneCamera.Position[2] += (CharacterXForm[3][2] - OldPosition[2]);
            }
        }

        UpdateCameraForScreen(RenderRect, SceneCamera);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadMatrixf((real32 *)SceneCamera.Projection4x4);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadMatrixf((real32 *)SceneCamera.View4x4);

        bool const RenderAsSkeleton  = false; // todo: GetGlobalBoolean(L, "RenderAsSkeleton");
        // bool const LockRootTransform = true;  // todo: GetGlobalBoolean(L, "RootTransformsFrozen");

        // Build a rest pose if there was no output
        if (IntoPose == 0)
        {
            IntoPose = GetNewLocalPose(*Cache, Skeleton->BoneCount);
            BuildRestLocalPose(*Skeleton, 0, Skeleton->BoneCount, *IntoPose);
        }
        // umm, Assert?

        world_pose* WorldPose = 0;
        if (IntoPose)
        {
            Assert(Model);

            WorldPose = NewWorldPose(Skeleton->BoneCount);
            BuildWorldPose(*Skeleton,
                           0, Skeleton->BoneCount,
                           *IntoPose,
                           (real32*)CharacterXForm,
                           *WorldPose);

            if (RenderAsSkeleton)
            {
                matrix_4x4* WPArray = GetWorldPose4x4Array(*WorldPose);
                glColor3f(1, 1, 1);
                glBegin(GL_LINES);
                for (int i = 0; i < Skeleton->BoneCount; i++)
                {
                    if (Skeleton->Bones[i].ParentIndex != NoParentBone)
                    {
                        glVertex3fv(&((real32*)WPArray[i])[12]);
                        glVertex3fv(&((real32*)WPArray[Skeleton->Bones[i].ParentIndex])[12]);
                    }
                }
                glEnd();
            }
            else
            {
                {for (uint32x Mesh = 0; Mesh < BoundMeshes.size(); ++Mesh)
                {
                    RenderBoundMesh(BoundMeshes[Mesh], WorldPose, false);
                }}
            }
        }

        // Put the character on the ground...
        DrawGroundPlane((real32*)CharacterXForm);

        // Time to render the active node interface
        if (WorldPose)
        {
            ActiveNodeInterface(L, WorldPose);

            // Lose the pose
            FreeWorldPose(WorldPose);
        }
        FreePoseCache(Cache);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }

    glPopAttrib();
    glDepthMask(GL_FALSE);
    //UIAreaActivate();

    return 0;
}

static int
l_CharacterPreview(lua_State* L)
{
    StTMZone("Character Preview");

    int SourceIndex = lua_tointeger(L, -2) - 1;
    int AnimIndex   = lua_tointeger(L, -1) - 1;

    LuaRect RenderRect = UIAreaGet();
    // Todo: clipping?
    {
        LuaPoint UL(RenderRect.x, RenderRect.y);
        AreaToScreen(UL);
        RenderRect.x = UL.x;
        RenderRect.y = UL.y;
    }

    glPushAttrib(GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT | GL_VIEWPORT_BIT);
    glDepthMask(GL_TRUE);

    // Setup the screen viewport.  Note that this completely takes over the GUI setup, so
    // we need to restore that at the end...
    {
        int screenW, screenH;
        UIGetSize(&screenW, &screenH);
        glViewport(RenderRect.x, (screenH - RenderRect.y) - RenderRect.h, RenderRect.w, RenderRect.h);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (Model)
    {
        model_instance* TempInstance = InstantiateModel(*Model);
        skeleton* Skeleton = Model->Skeleton;
        local_pose* IntoPose = NewLocalPose(Skeleton->BoneCount);

        animation* Animation = edit::GetAnimationForCurrSet(SourceIndex, AnimIndex);
        control* TempControl = 0;
        if (Animation)
        {
            TempControl = PlayControlledAnimation(0, *Animation, *TempInstance);
            if (TempControl)
                SetControlLoopCount(*TempControl, 0);

            SetModelClock(*TempInstance, g_GlobalClock);
        }

        UpdateCameraForScreen(RenderRect, PreviewCamera);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadMatrixf((real32 *)PreviewCamera.Projection4x4);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadMatrixf((real32 *)PreviewCamera.View4x4);
        {
            Assert(Model);

            real32 Placement[16];
            BuildCompositeTransform4x4(Model->InitialPlacement, (real32*)Placement);

            world_pose* WorldPose = NewWorldPose(Skeleton->BoneCount);
            SampleModelAnimationsAccelerated(*TempInstance, Skeleton->BoneCount,
                                             Placement,
                                             *IntoPose,
                                             *WorldPose);
            {for (uint32x Mesh = 0; Mesh < BoundMeshes.size(); ++Mesh)
            {
                RenderBoundMesh(BoundMeshes[Mesh], WorldPose, false);
            }}

            FreeWorldPose(WorldPose);
            DrawGroundPlane(Placement);
        }
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        if (TempControl)
            FreeControl(TempControl);
        FreeLocalPose(IntoPose);
        FreeModelInstance(TempInstance);
    }

    glPopAttrib();
    glDepthMask(GL_FALSE);
    //UIAreaActivate();

    return 0;
}

static int
l_ZoomCamera(lua_State* L)
{
    if (!!lua_toboolean(L, -1))
        --CameraZoomClick;
    else
        ++CameraZoomClick;

    return 0;
}

static int
l_CameraEAR(lua_State* L)
{
    SceneCamera.ElevAzimRoll[0] += (real32)lua_tonumber(L, -1);
    SceneCamera.ElevAzimRoll[1] += (real32)lua_tonumber(L, -2);
    SceneCamera.ElevAzimRoll[2] += (real32)lua_tonumber(L, -3);

    Copy(SizeOf(SceneCamera.ElevAzimRoll),
         SceneCamera.ElevAzimRoll,
         PreviewCamera.ElevAzimRoll);

    return 0;
}

static int
l_CameraPan(lua_State* L)
{
    MoveCameraRelative(SceneCamera,
                       (real32)lua_tonumber(L, -2) * ZoomDistance(),
                       (real32)lua_tonumber(L, -1) * ZoomDistance(),
                       0.0f);
    MoveCameraRelative(PreviewCamera,
                       (real32)lua_tonumber(L, -2) * ZoomDistance(),
                       (real32)lua_tonumber(L, -1) * ZoomDistance(),
                       0.0f);
    return 0;
}

#define IMPL_LUA_FUNCTION(FnName) static int FnName (lua_State* L)

IMPL_LUA_FUNCTION(Edit_GetCameraTracks)
{
    lua_pushboolean(L, CameraTracks);
    return 1;
}

IMPL_LUA_FUNCTION(Edit_SetCameraTracks)
{
    CameraTracks = !!lua_toboolean(L, -1);

    return 0;
}

IMPL_LUA_FUNCTION(Edit_SnapCameraToCharacter)
{
    SceneCamera.Position[0] = CharacterXForm[3][0];
    SceneCamera.Position[1] = CharacterXForm[3][1];
    SceneCamera.Position[2] = CharacterXForm[3][2];
    ZoomToBoundingBox();
    return 0;
}

IMPL_LUA_FUNCTION(Edit_ResetCharacterPosition)
{
    if (CameraTracks)
    {
        SceneCamera.Position[0] -= CharacterXForm[3][0];
        SceneCamera.Position[1] -= CharacterXForm[3][1];
        SceneCamera.Position[2] -= CharacterXForm[3][2];
    }

    ResetCharacterXForm();
    return 0;
}


ECameraUp GRANNY
GetCameraUpSetting()
{
    return CameraUpSetting;
}

void GRANNY
SetCameraUpSetting(ECameraUp Setting)
{
    CameraUpSetting = Setting;
}

IMPL_LUA_FUNCTION(Edit_GetUpVectorSetting)
{
    lua_pushinteger(L, GetCameraUpSetting());
    return 1;
}

IMPL_LUA_FUNCTION(Edit_SetUpVectorSetting)
{
    int UpVector = lua_tointeger(L, -1);
    if (UpVector < 1) UpVector = 1;
    if (UpVector > 3) UpVector = 3;

    PushUndoPos("change camera up");
    CameraUpSetting = (ECameraUp)UpVector;

    return 0;
}


bool GRANNY
UICharacterDrawing_Register(lua_State* State)
{
    InitializeCamera();

    lua_register(State, "CharacterRender",     l_CharacterRender);
    lua_register(State, "ZoomCamera",          l_ZoomCamera);
    lua_register(State, "CameraEAR",           l_CameraEAR);
    lua_register(State, "CameraPan",           l_CameraPan);

    lua_register(State, "GetCameraTracks",        Edit_GetCameraTracks);
    lua_register(State, "SetCameraTracks",        Edit_SetCameraTracks);
    lua_register(State, "SnapCameraToCharacter",  Edit_SnapCameraToCharacter);
    lua_register(State, "ResetCharacterPosition", Edit_ResetCharacterPosition);

    lua_register(State, "CharacterPreview",     l_CharacterPreview);

    lua_register(State, "Edit_GetUpVectorSetting", Edit_GetUpVectorSetting);
    lua_register(State, "Edit_SetUpVectorSetting", Edit_SetUpVectorSetting);

    return true;
}

void GRANNY
UICharacterDrawing_Shutdown()
{
    //
}

