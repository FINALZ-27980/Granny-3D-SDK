// ========================================================================
// Granny loading and multithreaded sampling
//
// $Notice: $
//
// This sample modifies the basic Xenon sample such that the animation
// sampling tasks are spread evenly across the 6 CPU threads.  The
// documentation is sparse, but here are the basic details.  The section
// below marked "Example control variables" contains the variables that
// control how many model instances are created (Rows * Cols), whether or
// not the models are rendered or the timing of the sampling is all we care
// about (RAW_SPEED), and the number of worker threads used to sample the
// animations.
//
// The thread controls are a little opaque, so here are the details.
// numThreads controls the absolute number of worker threads used to sample
// the animations.  numShares is the total number of workload shares, which
// allows you to allocate different workloads to different threads to
// simulate external requirements (i.e, giving half of thread 5 to audio
// processing, or what have you.  numShares should be equal to the sum of
// ThreadShares from 0 to numThreads - 1.  ThreadCore controls which Core
// thread the sampling worker thread is assigned to.  For maximum
// performance, this array is designed to assign the worker threads to
// different cores as much as possible before stacking multiple sampling
// threads on the same core.  The default setup is 3 equal threads running
// on 3 separate cores.
//
// Note that the threading control mechanism we've chosen here is to start
// our worker threads in infinite loops, and release them with Events when
// there is work for them to do.  This is very simple to setup and control,
// but you may want to start the thread on a per-frame basis, or provide a
// command queue interface so you can shut down the workers, etc.  As
// always, we're striving for simplicity in demonstrating the concept here
// rather than efficiency or controllability.
//
// ========================================================================
#include "xenon_speedcheck.h"
#include <assert.h>

using namespace std;

// =============================================================================
// Example control variables
// =============================================================================

// Rows/Cols/Meters apart in x/z6
int   NumModelRows = 24;
int   NumModelCols = 15;
float ModelSpacing = 1.0f;

// Set this to one to to timing tests of just the Granny functions.  Set to 0
// to actually render the scene
#define RAW_SPEED 0

// Control how many worker threads are started.
// numShares should be equal to Sum{ThreadShares, [0, numThreads)}
const int numThreads = 3;
const int numShares  = 6;
int ThreadCore[6]   = { 0, 2, 4, 1, 3, 5 };
int ThreadShares[6] = { 2, 2, 2, 2, 2, 1 };


//-------------------------------------------------------------------------------------
// Global D3D variables
//-------------------------------------------------------------------------------------
D3DDevice*             g_pd3dDevice = NULL;
D3DVertexDeclaration*  g_pVertexDecl_Skinned = NULL;
D3DVertexShader*       g_pVertexShader_Skinned = NULL;
D3DPixelShader*        g_pPixelShader = NULL;
BOOL                   g_bWidescreen = TRUE;

// These must match the constant declarations in the vertex shaders...
#define MATRIX_CONSTANT_WORLD2VIEW 4
#define MATRIX_CONSTANT_VIEW2CLIP  8
#define VEC3_DIRFROMLIGHT          12
#define VEC4_LIGHTCOLOUR           13
#define VEC4_AMBIENTCOLOUR         14

Scene g_scene;


// =============================================================================
// Simple threading loops
// =============================================================================

HANDLE ThreadRelease[numThreads];
HANDLE ThreadFinished[numThreads];
HANDLE ThreadHandles[numThreads];
int ThreadLoadStart[6];
int ThreadLoadEnd[6];

DWORD __stdcall WorkerThread(LPVOID lpThreadParameter)
{
    int const ThreadIndex = (int)lpThreadParameter;

    granny_local_pose* SharedLocalPose = GrannyNewLocalPose(g_scene.MaxBoneCount);
    granny_world_pose* SharedWorldPose = GrannyNewWorldPoseNoComposite(g_scene.MaxBoneCount);
    granny_matrix_3x4* LocalMatrices = new granny_matrix_3x4[g_scene.MaxBoneCount];

    XSetThreadProcessor(GetCurrentThread(), ThreadCore[ThreadIndex]);
    for (;;)
    {
        if (WaitForSingleObject(ThreadRelease[ThreadIndex], INFINITE) == WAIT_OBJECT_0)
        {
            // Execute the current sampling load...
            for (int i = ThreadLoadStart[ThreadIndex]; i < ThreadLoadEnd[ThreadIndex]; ++i)
            {
                model* Model = g_scene.Models[i];
                granny_skeleton *Skeleton = GrannyGetSourceSkeleton(Model->GrannyInstance);

                // Sample the animation into the shared local pose...
                GrannySampleModelAnimationsAccelerated(Model->GrannyInstance,
                                                       Skeleton->BoneCount,
                                                       Model->Placement,
                                                       SharedLocalPose,
                                                       SharedWorldPose);

                // Now copy the bone matrices into the appropriate constantvb
                if (Model->MatrixBuffer[0] != NULL)
                {
                    assert(Model->MatrixBuffer[1] != NULL);
                    IDirect3DVertexBuffer9 *Buffer = Model->MatrixBuffer[g_scene.FrameCount % model::s_numBuffers];

#if RAW_SPEED
                    GrannyBuildCompositeBufferTransposed(Skeleton,
                                                         0, Skeleton->BoneCount,
                                                         SharedWorldPose,
                                                         LocalMatrices);
#else
                    granny_matrix_3x4 *Matrices;
                    Buffer->Lock(0, 0, (void**)&Matrices, 0);
                    GrannyBuildCompositeBufferTransposed(Skeleton,
                                                         0, Skeleton->BoneCount,
                                                         SharedWorldPose,
                                                         Matrices);
                    Buffer->Unlock();
#endif
                }
            }

            SetEvent(ThreadFinished[ThreadIndex]);
        }
    }

    // Never actually exits, but what the hey.
    return 0;
}

void StartWorkerThreads()
{
    for (int i = 0; i < numThreads; i++)
    {
        ThreadRelease[i]  = CreateEvent(NULL, FALSE, FALSE, NULL);
        ThreadFinished[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        ThreadHandles[i] = CreateThread(NULL, 0, WorkerThread, (LPVOID)i, 0, NULL);
    }
}


//-------------------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the world for the next frame
//-------------------------------------------------------------------------------------
void Update(granny_real32 const CurrentTime,
            granny_real32 const DeltaTime)
{
    for (size_t Idx = 0; Idx < g_scene.Models.size(); Idx++)
    {
        // Set the model clock
        model* Model = g_scene.Models[Idx];
        GrannySetModelClock(Model->GrannyInstance, CurrentTime + Model->TimeOffset);
    }

    // Apportion the work out to the threads...
    int NumModels = g_scene.Models.size();
    int ModelsPerShare = max(NumModels / numShares, 1);
    int Current = 0;
    for (int i = 0; i < numThreads; ++i)
    {
        ThreadLoadStart[i] = Current;
        ThreadLoadEnd[i] = Current + ThreadShares[i] * ModelsPerShare;
        ThreadLoadEnd[i] = min(ThreadLoadEnd[i], NumModels);
        Current = ThreadLoadEnd[i];
    }
    ThreadLoadEnd[numThreads - 1] = g_scene.Models.size();

    // Release the hounds!
    GrannySetAllowGlobalStateChanges(false);
    for (int i = numThreads - 1; i >= 0; --i)
    {
        SetEvent(ThreadRelease[i]);
    }
    WaitForMultipleObjects(numThreads, ThreadFinished, TRUE, INFINITE);
    GrannySetAllowGlobalStateChanges(true);
}


//-------------------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-------------------------------------------------------------------------------------
void Render()
{
#if !RAW_SPEED
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

    if (SUCCEEDED(g_pd3dDevice->BeginScene()))
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
#endif
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

    StartWorkerThreads();

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

        // Render the scene.  Insert a fence so we can lock and write to
        // the vertex buffers from other threads.
        Render();
        g_pd3dDevice->InsertFence();
        g_scene.FrameCount++;

        if ((g_scene.FrameCount % 50) == 0)
        {
            char buffer[512];
            sprintf(buffer, "%f ms/frame\n", DeltaTime*1000);
            OutputDebugString(buffer);
        }
    }
}
