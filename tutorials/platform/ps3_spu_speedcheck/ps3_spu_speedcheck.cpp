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
#include "ps3_spu_speedcheck.h"
#include "granny_callbacks.h"

#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <sys/spu_image.h>
#include <sys/spu_initialize.h>
#include <sys/spu_thread.h>
#include <sys/spu_thread_group.h>
#include <sys/spu_utility.h>
#include <vector>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <dirent.h>
#include <cell/gcm.h>
#include <sys/timer.h>
#include <cell/audio.h>
#include <cell/pad.h>
#include <sysutil/sysutil_sysparam.h>
#include <sys/sys_time.h>
#include <cell/cell_fs.h>
#include <cell/spurs.h>
#include <cell/sysmodule.h>
#include <sys/spu_initialize.h>
#include <fcntl.h>
#include <unistd.h>


Scene g_scene;
using namespace std;

// Change with left/right, up/down on the dpad
// Rows/Cols/Meters apart in x/z6
int   NumModelRows = 10;
int   NumModelCols = 10;
float ModelSpacing = 1.0f;

// Toggle on and off with the Blue Cross button.
// Set to true to turn off rendering and run as fast as possible...
bool RawSpeed = false;


//-------------------------------------------------------------------------------------
// We simply hard-code our resource names for the purposes of this example
//-------------------------------------------------------------------------------------
char const *ModelFileName = "/app_home/Media/pixo_run.gr2";
char const *AnimFileName  = "/app_home/Media/proc.gr2";
char const *SPUSelfName    = "/app_home/SampleModel/PS3_Debug/SampleModel.spu.self";
char const *SPURawSelfName = "/app_home/SampleModel/PS3_Debug/SampleModelRaw.spu.elf";

int const MaxUsedSPUs = 6;

granny_radspu_command_queue* g_CommandQueue = 0;
CellSpurs2* SpursInstance = 0;

extern const CellSpursTaskBinInfo _binary_task_SampleModelSPURS_elf_taskbininfo;

bool StartProcessors(int NumTasks)
{
    // Create the spurs instance
    {
        SpursInstance = (CellSpurs2*)memalign(128,sizeof(CellSpurs2));

        int num_spu = NumTasks;
        int spu_thread_group_priority = 100;
        int spurs_handler_thread_priority = 2;
        bool release_spu_when_idle = false;

        CellSpursAttribute attributeSpurs;
        cellSpursAttributeInitialize(
            &attributeSpurs,
            num_spu,
            spu_thread_group_priority,
            spurs_handler_thread_priority,
            release_spu_when_idle);

        cellSpursInitializeWithAttribute2(
            SpursInstance,
            &attributeSpurs);
    }


    g_CommandQueue = GrannyInitCommandQueueSPURS(32, NumTasks, &_binary_task_SampleModelSPURS_elf_taskbininfo, SpursInstance);

    return true;
}

void ShutdownProcessors()
{
    if (g_CommandQueue)
    {
        GrannyShutdownCommandQueueSPU(g_CommandQueue);
        g_CommandQueue = 0;
    }

    if (SpursInstance)
    {
        int finalCode = cellSpursFinalize(SpursInstance);
        if (finalCode != CELL_OK)
        {
            // well, that's bad.  All of the tasks should be gone.
            // log error
        }

        free(SpursInstance);
    }
}


static void
UpdateModels()
{
    if (NumModelCols <= 0) NumModelCols = 1;
    if (NumModelRows <= 0) NumModelRows = 1;

    for (size_t i = 0; i < g_scene.Models.size(); ++i)
        delete g_scene.Models[i];
    g_scene.Models.clear();

    int const TotalModels = NumModelCols * NumModelRows;
    for (int x = 0; x < NumModelCols; ++x)
    {
        float XPos = (x - (NumModelCols/2.0f)) * ModelSpacing;
        for (int z = 0; z < NumModelRows; ++z)
        {
            float ZPos = -z * ModelSpacing;

            int ModelIndex = x * NumModelRows + z;
            float Offset = (ModelIndex * g_scene.SPUFileInfo->SPUAnimations[0]->Duration) / TotalModels;

            for (int Model = 0; Model < g_scene.FileInfo->ModelCount; Model++)
            {
                granny_model *GrannyModel = g_scene.FileInfo->Models[Model];
                int const BoneCount = GrannyModel->Skeleton->BoneCount;

                model* NewModel = new model;
                g_scene.Models.push_back(NewModel);

                // Create the model instance and it's world pose buffer
                NewModel->GrannyInstance = GrannyInstantiateModel(GrannyModel);
                NewModel->LocalPose      = GrannyNewLocalPose(BoneCount);
                NewModel->WorldPose      = GrannyNewWorldPose(BoneCount);

                NewModel->TimeOffset = Offset;
                NewModel->Placement = (granny_real32*)malloc(sizeof(granny_real32) * 16);
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


                // Track the max bone count
                g_scene.MaxBoneCount = std::max(g_scene.MaxBoneCount, BoneCount);

                // Create the meshes for this model
                for (int Mesh = 0; Mesh < GrannyModel->MeshBindingCount; Mesh++)
                {
                    NewModel->Meshes.push_back(g_scene.ModelMeshes[Model][Mesh]);
                }

                //if (AnySkinned)
                {
                    int const MatrixBufferSize = BoneCount * (4 * 4 * sizeof(float));

                    // Just one buffer here.  In Real Life, you probably want
                    // to double or N-buffer.
                    glGenBuffers(1, &NewModel->MatrixBufferName);
                    glBindBuffer(GL_ARRAY_BUFFER, NewModel->MatrixBufferName);
                    glBufferData(GL_ARRAY_BUFFER, MatrixBufferSize, NULL, GL_STREAM_DRAW);
                }

                if (g_scene.SPUFileInfo->SPUAnimationCount)
                {
                    granny_spu_animation *Animation = g_scene.SPUFileInfo->SPUAnimations[0];
                    granny_control* Control =
                        GrannyPlayControlledSPUAnimation(0.0f, Animation,
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
    g_scene.SPUFile = GrannyReadEntireFile(AnimFileName);
    if (g_scene.File == NULL || g_scene.SPUFile == NULL)
    {
        return false;
    }

    g_scene.FileInfo = GrannyGetFileInfo(g_scene.File);
    g_scene.SPUFileInfo = GrannyGetSPUAnimationInfo(g_scene.SPUFile);
    if (g_scene.FileInfo == NULL || g_scene.SPUFileInfo == NULL)
    {
        return false;
    }

    // Create the textures for this file
    for (int Texture = 0; Texture < g_scene.FileInfo->TextureCount; ++Texture)
    {
        granny_texture *GrannyTexture = g_scene.FileInfo->Textures[Texture];
        texture* NewTexture = new texture;
        g_scene.Textures.push_back(NewTexture);

        NewTexture->Name = GrannyTexture->FromFileName;

        if (GrannyTexture->TextureType == GrannyColorMapTextureType &&
            GrannyTexture->ImageCount == 1)
        {
            NewTexture->TextureName = CreateRGBATexture(GrannyTexture, 0);
        }
        else
        {
            NewTexture->TextureName = 0;
        }
    }

    g_scene.ModelMeshes.resize(g_scene.FileInfo->ModelCount);
    for (int Model = 0; Model < g_scene.FileInfo->ModelCount; Model++)
    {
        granny_model *GrannyModel = g_scene.FileInfo->Models[Model];

        vector<mesh*>& Meshes = g_scene.ModelMeshes[Model];
        for (int Mesh = 0; Mesh < GrannyModel->MeshBindingCount; Mesh++)
        {
            // Make sure to ignore the NULLs returned for rigid meshes in this demo...
            mesh* NewMesh = CreateMesh(GrannyModel, Mesh);
            if (NewMesh)
                Meshes.push_back(NewMesh);
        }
    }

    g_scene.MaxBoneCount = 0;
    UpdateModels();

    // Temp matrix buffer
    g_scene.SharedTempMatrices = new granny_matrix_3x4[g_scene.MaxBoneCount];

    // Initialize the camera.  We've transformed the file into units
    // of 1 unit = 1 meter, so just position the camera about 5 meters
    // back and 1 up, which should be reasonable for the purposes of
    // this test.
    GrannyInitializeDefaultCamera(&g_scene.Camera);
    g_scene.Camera.ElevAzimRoll[0] = -0.5;
    g_scene.Camera.Offset[1] = 0.5f;
    g_scene.Camera.Offset[2] = 3.5f;
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


void Update(granny_real32 const CurrentTime,
            granny_real32 const DeltaTime)
{
    for (size_t Idx = 0; Idx < g_scene.Models.size(); Idx++)
    {
        // Set the model clock
        model* Model = g_scene.Models[Idx];
        GrannySetModelClock(Model->GrannyInstance, CurrentTime + Model->TimeOffset);
    }

    // Setup the sample submissions
    granny_uint64x FinalFence = GrannyGetLastInsertedFenceSPU(g_CommandQueue);

    granny_radspu_command_sma_accel SMAACommand;
    SMAACommand.LocalPoseFillThreshold = GrannyDefaultLocalPoseFillThreshold;
    SMAACommand.AllowedError = 0.0f;

    for (size_t Idx = 0; Idx < g_scene.Models.size(); Idx++)
    {
        model* Model = g_scene.Models[Idx];
        granny_skeleton *Skeleton = GrannyGetSourceSkeleton(Model->GrannyInstance);
        SMAACommand.InstanceEA = (granny_uint64x)Model->GrannyInstance;
        SMAACommand.BoneCount  = Skeleton->BoneCount;
        SMAACommand.OffsetEA   = (granny_uint64x)Model->Placement;
        SMAACommand.WorldPoseEA = (granny_uint64x)GrannyGetWorldPose4x4Array(Model->WorldPose);
        SMAACommand.CompositeEA = (granny_uint64x)GrannyGetWorldPoseComposite4x4Array(Model->WorldPose);

        FinalFence =
            GrannyInsertCommandSPU(g_CommandQueue,
                                     GrannySPUCommand_SampleModelAnimAccelerated,
                                     &SMAACommand, sizeof(SMAACommand));
    }

    // Wait for the commands to clear the queues
    while (GrannyGetCurrentClearedFenceSPU(g_CommandQueue) != FinalFence)
        sys_timer_usleep(0);
}


//############################################################################
//##                                                                        ##
//## Update_input - read the gamepad and act on it:                         ##
//##                                                                        ##
//##    Left joystick -   Move the video around on screen.                  ##
//##    Right joystick -  Scale the movie up or down.                       ##
//##    X button -        Skip to the next Bink movie.                      ##
//##    Circle button -   Play the movie as fast as possible .              ##
//##    Square button -   Pause the current Bink movie.                     ##
//##    Triangle button - Loop the current movie (otherwise go to next).    ##
//##    Start button -    Show help.                                        ##
//##                                                                        ##
//## This function returns -1 if the current movie should be skipped.       ##
//##                                                                        ##
//############################################################################
#define BUTTON_SELECT         0x0100
#define BUTTON_START          0x0800
#define BUTTON_PAD_U          0x1000
#define BUTTON_PAD_R          0x2000
#define BUTTON_PAD_D          0x4000
#define BUTTON_PAD_L          0x8000
#define BUTTON_L2             0x0001
#define BUTTON_R2             0x0002
#define BUTTON_L1             0x0004
#define BUTTON_R1             0x0008
#define BUTTON_GREEN_TRIANGLE 0x0010
#define BUTTON_RED_CIRCLE     0x0020
#define BUTTON_BLUE_CROSS     0x0040
#define BUTTON_PINK_SQUARE    0x0080

granny_uint32 buttons_pressed;
granny_uint32 buttons_held;
static granny_int32 update_input( void )
{
    granny_int32 changed = 0;
    static granny_uint32 last_check = 0;

    // Only check every 100 ms

    granny_uint32 time = sys_time_get_system_time();

    if ( ( time - last_check ) > 100000 )
    {
        granny_uint32 buttons;

        last_check = time;

        //
        // Read the current state
        //

        CellPadData pad_data;
        granny_int32 pad_state = cellPadGetData( 0, &pad_data );

        if ( ( pad_state == CELL_PAD_OK ) && ( pad_data.len ) )
        {
            buttons = ( (granny_uint32) pad_data.button[ 2 ] << 8 ) | pad_data.button[ 3 ];
        }
        else
        {
            return( 0 );
        }

        //
        // Figure out which buttons have pushed since last time
        //

        buttons_pressed = buttons & ( buttons ^ buttons_held );
        changed = ( buttons != buttons_held );
        buttons_held = buttons;

        bool UpdateModelThisFrame = false;
        if (buttons_pressed & BUTTON_START)
        {
            NumModelRows = 10;
            NumModelCols = 10;
            RawSpeed = false;
            UpdateModelThisFrame = true;
        }


        if (buttons_pressed & BUTTON_PAD_U)
        {
            ++NumModelRows;
            UpdateModelThisFrame = true;
        }
        if (buttons_pressed & BUTTON_PAD_D)
        {
            --NumModelRows;
            UpdateModelThisFrame = true;
        }
        if (buttons_pressed & BUTTON_PAD_R)
        {
            ++NumModelCols;
            UpdateModelThisFrame = true;
        }
        if (buttons_pressed & BUTTON_PAD_L)
        {
            --NumModelCols;
            UpdateModelThisFrame = true;
        }
        if (UpdateModelThisFrame)
        {
            UpdateModels();
        }

        if (buttons_pressed & BUTTON_BLUE_CROSS)
        {
            RawSpeed = !RawSpeed;
        }

    }

    return changed;
}


//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
#include <libsn.h>
#include <spu_printf.h>

int main()
{
    snInit();

    #define SPU_PRINTF_PRIORITY 999
    spu_printf_initialize(SPU_PRINTF_PRIORITY, NULL);


#if USE_RAW_SPUS
    sys_spu_initialize(6, 5);
#else
    sys_spu_initialize(6, 0);
#endif

    // Initialize PSGL
    if( !InitPSGL() )
        return -1;

    // Initialize the granny scene
    if( !InitGrannyCallbacks() || !InitScene() )
        return -1;

    // Start the threaded processors
    if (!StartProcessors(MaxUsedSPUs))
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
        {
            // Clear the backbuffer to a blue color
            glClearColor(0, 0.15f, 1, 1);
            glClear(GL_COLOR_BUFFER_BIT |
                    GL_DEPTH_BUFFER_BIT |
                    GL_STENCIL_BUFFER_BIT);
        }
        if (!RawSpeed)
        {
            Render();
        }
        else if ((g_scene.FrameCount % 50) == 0)
        {
            printf("%d Models, secs/frame: %f (%d/sec)\n", (NumModelRows*NumModelCols), DeltaTime, int((NumModelRows*NumModelCols)/DeltaTime));
        }

        {
            // Present the backbuffer contents to the display
            psglSwap();
        }


        g_scene.FrameCount++;
        update_input();
    }

    return 0;
}
