// ========================================================================
// $File: //jeffr/granny_29/rt/cell/spurs/granny_spu_commands_spurs.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_spu_command.h"
#include "granny_spu_commands_ppuside.h"

#include "granny_aggr_alloc.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"

#include <cell/sync2/queue.h>
#include <cell/atomic.h>
#include <cell/dma.h>
#include <cell/spurs.h>

#include "spuimageutils.h"

#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#undef SubsystemCode
#define SubsystemCode PS3SubsystemLogMessage

USING_GRANNY_NAMESPACE;

#define MAX_GRANNY_SPURS_TASKS 6

char const* g_GrannyTaskSetName = "GrannyTaskSet";
static char const* g_GrannyTaskNames[MAX_GRANNY_SPURS_TASKS] = {
    "GrannyTask0", "GrannyTask1", "GrannyTask2", "GrannyTask3", "GrannyTask4", "GrannyTask5"
};

struct FenceMarker
{
    uint64x ClearedFenceNumber;
    uint64x WorkingOnFenceNumber;
};

struct GRANNY radspu_command_queue
{
    CellSpursTasksetAttribute2 TasksetAttr;
    CellSpursTaskset2          Taskset;
    CellSpursTaskId            Tasks[MAX_GRANNY_SPURS_TASKS];

    uint64x NumTasks;

    // Allocate to ensure 128 byte alignment
    CellSpursQueue* SyncQueue;

    int32x CommandSize;
    int32x QueueDepth;
    uint8*  CommandBuffer;  // size = CommandSize * QueueDepth, align to 128...

    uint8*       ContextStores[MAX_GRANNY_SPURS_TASKS];
    FenceMarker* SPUFences;

    uint64x LastFenceInserted;
};


radspu_command_queue* GRANNY
InitCommandQueueSPURS(int32x QueueDepth, int32x NumTasks, void const* SpursTaskBinInfo, void* SpursInstance)
{
    CheckCondition(QueueDepth > 0, return 0);
    CheckBoundedInt32(0, NumTasks, MAX_GRANNY_SPURS_TASKS, return 0);
    CheckPointerNotNull(SpursTaskBinInfo, return 0);
    CheckPointerNotNull(SpursInstance, return 0);

    // Cast the args to the correct types, now that we're buried in ps3 land.
    CellSpursTaskBinInfo* TaskInfo = (CellSpursTaskBinInfo*)SpursTaskBinInfo;
    CellSpurs*            Spurs    = (CellSpurs*)SpursInstance;

    CellSpursTaskBinInfo* UseTaskInfo = TaskInfo;
    CellSpursTaskBinInfo  TightlyBound = *TaskInfo;
    if (get_spu_writable_ls_pattern(TightlyBound.lsPattern.u64,
                                    (void const*)(intaddrx)TaskInfo->eaElf) != 0)
    {
        // Hey, we've got the section info we need to tightly bound the context save size.
        // Excellent.
        UseTaskInfo = &TightlyBound;

        // Give ourselves 2k of persistent stack space
        or_ls_pattern(TightlyBound.lsPattern.u64, 254*1024, 256*1024 );
        TightlyBound.sizeContext =  get_ls_pattern_size(TightlyBound.lsPattern.u64);
    }
    else
    {
        // TODO: warning about missing section header info
    }

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);
    SetAggrAlignment(Allocator, 128);

    radspu_command_queue* Queue = NULL;
    {
        AggrAllocPtr(Allocator, Queue);
        AggrAllocOffsetPtr(Allocator, Queue, SyncQueue);
        AggrAllocOffsetSizePtr(Allocator, Queue, (sizeof(radspu_command) * QueueDepth), CommandBuffer);
        AggrAllocOffsetCountlessArrayPtr(Allocator, Queue, NumTasks, SPUFences);
        {for (int32x Idx = 0; Idx < NumTasks; ++Idx)
        {
            AggrAllocOffsetCountlessArrayPtr(Allocator, Queue, UseTaskInfo->sizeContext, ContextStores[Idx]);
        }}
    }

    if (EndAggrAlloc(Allocator, AllocationLongTerm))
    {
        // Incremented below...
        Queue->NumTasks = 0;
        Queue->LastFenceInserted = 0;

        ZeroStructure(Queue->TasksetAttr);
        ZeroStructure(Queue->Taskset);
        ZeroArray(MAX_GRANNY_SPURS_TASKS, Queue->Tasks);
        ZeroArray(NumTasks, Queue->SPUFences);

        // Null these pointers that aren't acutually used.
        {for (int32x Idx = NumTasks; Idx < MAX_GRANNY_SPURS_TASKS; ++Idx)
        {
            Queue->ContextStores[Idx] = 0;
        }}

        cellSpursTasksetAttribute2Initialize(&Queue->TasksetAttr);
        Queue->TasksetAttr.name = g_GrannyTaskSetName;

        if (cellSpursCreateTaskset2(Spurs, &Queue->Taskset, &Queue->TasksetAttr) != CELL_OK)
        {
            Log0(ErrorLogMessage, PS3SubsystemLogMessage, "Unable to create Taskset");
            Deallocate(Queue);
            return 0;
        }

        // Intialize the sync queue
        Queue->CommandSize = sizeof(radspu_command);
        Queue->QueueDepth  = QueueDepth;
        if (cellSpursQueueInitialize(&Queue->Taskset, Queue->SyncQueue,
                                     Queue->CommandBuffer, Queue->CommandSize, QueueDepth,
                                     CELL_SPURS_QUEUE_PPU2SPU) != CELL_OK)
        {
            // Shut it down.
            Log0(ErrorLogMessage, PS3SubsystemLogMessage, "Unable to create command queue");
            cellSpursDestroyTaskset2(&Queue->Taskset);
            Deallocate(Queue);
            return 0;
        }

        if (cellSpursQueueAttachLv2EventQueue(Queue->SyncQueue) != CELL_OK)
        {
            cellSpursDestroyTaskset2(&Queue->Taskset);
            Deallocate(Queue);
            return 0;
        }

        // Create the tasks...
        CellSpursTaskArgument Args;
        Args.u64[0] = (uint64_t)Queue->SyncQueue;

        {for (int32x Idx = 0; Idx < NumTasks; ++Idx)
        {
            Args.u64[1] = (uint64_t)&Queue->SPUFences[Idx];

            if (cellSpursCreateTask2WithBinInfo(&Queue->Taskset,
                                                &Queue->Tasks[Idx],
                                                UseTaskInfo, &Args,
                                                Queue->ContextStores[Idx],
                                                g_GrannyTaskNames[Idx], 0) != CELL_OK)
            {
                if (Idx != 0)
                {
                    // Shut it down.  This is a real mess at this point, since we've already
                    // started up some of these tasks.  *Try* to send them shutdown commands,
                    // but we may be screwed here anyways.
                    Log1(ErrorLogMessage, PS3SubsystemLogMessage, "Unable to create task %d, this is catastrophically bad.", Idx);

                    // Since we're at the end of the initialization, we can use the real shutdown code now.
                    ShutdownCommandQueueSPU(Queue);
                    Queue = 0;
                }

                return 0;
            }

            // Valid task!
            ++Queue->NumTasks;
        }}
    }

    return Queue;
}

radspu_command_queue* GRANNY
InitCommandQueueSPUThreads(int32x QueueDepth, int32x NumThreads, void const* ThreadImage)
{
    Log0(ErrorLogMessage, PS3SubsystemLogMessage,
         "InitCommandQueueSPUThreads called for SPURS-enabled library, unsupported!");
    return 0;
}


void GRANNY
ShutdownCommandQueueSPU(radspu_command_queue* Queue)
{
    if (!Queue)
        return;
    Assert(Queue->NumTasks > 0 && Queue->NumTasks <= MAX_GRANNY_SPURS_TASKS);

    // Insert Queue->NumTasks shutdown commands, and wait for the fence to clear.
    uint64x WaitFence;
    {for (int32x Idx = 0; Idx < Queue->NumTasks; ++Idx)
    {
        WaitFence = InsertCommandSPU(Queue, SPUCommand_Shutdown, 0, 0);
    }}

    // TODO: Put a timeout on the wait?
    while (GetCurrentClearedFenceSPU(Queue) < WaitFence)
    {
        // Yield?
    }

    // Join all the tasks
    {for (int32x Idx = 0; Idx < Queue->NumTasks; ++Idx)
    {
        int exitCode;
        if (cellSpursJoinTask2(&Queue->Taskset, Queue->Tasks[Idx], &exitCode) != CELL_OK)
        {
            Log1(ErrorLogMessage, PS3SubsystemLogMessage, "Unable to join task %d", Idx);
        }
    }}

    // Detach the event queue
    cellSpursQueueDetachLv2EventQueue(Queue->SyncQueue);
    Queue->SyncQueue = 0;
    Queue->NumTasks  = 0;

    // Destroy the taskset
    if (cellSpursDestroyTaskset2(&Queue->Taskset) != CELL_OK)
    {
        Log0(ErrorLogMessage, PS3SubsystemLogMessage, "Unable to destroy Taskset");
    }

    // Kill it!  Burn it with fire!
    ZeroStructure(*Queue);

    Deallocate(Queue);
}


uint64x GRANNY
GetLastInsertedFenceSPU(radspu_command_queue* Queue)
{
    CheckPointerNotNull(Queue, return 0);

    return Queue->LastFenceInserted;
}


uint64x GRANNY
GetCurrentClearedFenceSPU(radspu_command_queue* Queue)
{
    CheckPointerNotNull(Queue, return 0);

    // Update the observation: TODO: should lock, or mark this function as GS_PARAM

    // Make a copy of the fences.  Note that we are abusing our knowledge that the lock
    // line will tell us if anything in that cache line has changed
    FenceMarker SPUFences[8];
    Assert(IS_ALIGNED_N(Queue->SPUFences, 128));
    Assert(sizeof(SPUFences) == 128);

    uint64_t Old;
    do
    {
        Old = cellAtomicLockLine64(Queue->SPUFences);
        memcpy(SPUFences, Queue->SPUFences, sizeof(SPUFences));
    } while (cellAtomicStoreConditional64(Queue->SPUFences, Old));

    bool AnyNonWorking = false;
    uint64_t MaxNonWorking = 0;
    uint64_t MinWorking = uint64_t(-1);
    {for (int32x Idx = 0; Idx < Queue->NumTasks; ++Idx)
    {
        Assert(SPUFences[Idx].ClearedFenceNumber <= SPUFences[Idx].WorkingOnFenceNumber);

        if (SPUFences[Idx].ClearedFenceNumber < SPUFences[Idx].WorkingOnFenceNumber)
        {
            if (SPUFences[Idx].ClearedFenceNumber < MinWorking)
                MinWorking = SPUFences[Idx].ClearedFenceNumber;
        }
        else
        {
            if (SPUFences[Idx].ClearedFenceNumber != 0 &&
                SPUFences[Idx].ClearedFenceNumber > MaxNonWorking)
            {
                AnyNonWorking = true;
                MaxNonWorking = SPUFences[Idx].ClearedFenceNumber;
            }
        }
    }}

    if (AnyNonWorking)
    {
        return (MinWorking < MaxNonWorking) ? MinWorking : MaxNonWorking;
    }
    else
    {
        if (MinWorking == uint64_t(-1))
            return 0;

        return MinWorking;
    }
}


uint64x GRANNY
InsertFenceSPU(radspu_command_queue* Queue)
{
    return InsertCommandSPU(Queue, SPUCommand_FenceNOP, 0, 0);
}


uint64x GRANNY
TryInsertCommandSPU(radspu_command_queue* Queue, int32x CommandType,
                    void const* CommandBuffer, int32x CommandSize)
{
    ALIGN16(radspu_command) Command;
    Assert(CommandSize >= 0 && CommandSize <= SizeOf(Command.CommandBuffer));

    // TODO: should try to lock for LastFenceInserted, ret if fail

    Command.CommandType = CommandType;
    Command.FenceValue  = ++(Queue->LastFenceInserted);
    if (CommandSize)
    {
        Assert(CommandBuffer);
        Copy(CommandSize, CommandBuffer, Command.CommandBuffer);
    }

    int const PushRetVal = cellSpursQueueTryPush(Queue->SyncQueue, &Command);
    if (PushRetVal == CELL_OK)
    {
        return Command.FenceValue;
    }
    else if (PushRetVal == CELL_SYNC_ERROR_BUSY)
    {
        // Not an error, just a lack of space in the queue.
        return uint64x(-1);
    }
    else
    {
        Log1(ErrorLogMessage, PS3SubsystemLogMessage,
             "cellSyncQueueTryPush failed with error: %d", PushRetVal);
        return uint64x(-1);
    }
}


uint64x GRANNY
InsertCommandSPU(radspu_command_queue* Queue, int32x CommandType,
                 void const* CommandBuffer, int32x CommandSize)
{
    ALIGN16(radspu_command) Command;
    Assert(CommandSize >= 0 && CommandSize <= SizeOf(Command.CommandBuffer));

    // TODO: should lock for LastFenceInserted

    Command.CommandType = CommandType;
    Command.FenceValue  = ++Queue->LastFenceInserted;
    if (CommandSize)
    {
        Assert(CommandBuffer);
        Copy(CommandSize, CommandBuffer, Command.CommandBuffer);
    }

    int PushRetVal = cellSpursQueuePush(Queue->SyncQueue, &Command);
    if (PushRetVal == CELL_OK)
    {
        return Command.FenceValue;
    }
    else
    {
        Log1(ErrorLogMessage, PS3SubsystemLogMessage,
             "cellSyncQueuePush failed with error: %d", PushRetVal);
        return uint64x(-1);
    }
}

