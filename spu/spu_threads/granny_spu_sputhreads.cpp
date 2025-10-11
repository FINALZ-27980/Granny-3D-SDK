// ========================================================================
// $File: //jeffr/granny_29/spu/spu_threads/granny_spu_sputhreads.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny2_spu_samplemodel.h"

#include <cell/sync/queue.h>
#include <cell/atomic.h>
#include <cell/dma.h>
#include <sys/spu_thread.h>


struct radspu_command_queue_spuside
{
    uint64_t SyncQueueEA;
    uint64_t FenceEA;
};

// Used for the atomic stores.
static uint64_t AtomicBuf[128/sizeof(uint64_t)] __attribute__((aligned(128)));

extern "C" bool
RADSPUObtainCommand(radspu_command_queue_spuside* RADQueue,
                    radspu_command* Command,
                    unsigned int DMATag)
{
    int RetVal;
    do
    {
		RetVal = cellSyncQueueTryPop(RADQueue->SyncQueueEA, Command, DMATag);
        if (RetVal == CELL_SYNC_ERROR_BUSY)
        {
            // Yield
            sys_spu_thread_group_yield();
        }
    } while (RetVal != CELL_OK);

    // Need to wait for the DMA to complete...
    cellDmaWaitTagStatusAll(1 << DMATag);

    return RetVal == CELL_OK;
}

extern "C" void
RADSPUWorkingOn(radspu_command_queue_spuside const* Queue, uint64_t FenceValue)
{
	cellAtomicStore64(AtomicBuf, Queue->FenceEA + sizeof(uint64_t), FenceValue);
}

extern "C" void
RADSPUFinished(radspu_command_queue_spuside const* Queue, uint64_t FenceValue)
{
	cellAtomicStore64(AtomicBuf, Queue->FenceEA, FenceValue);
}


int main(uint64_t QueueEA, uint64_t FenceEA, uint32_t SPUPort)
{
	radspu_command_queue_spuside RADQueue;
	RADQueue.SyncQueueEA = QueueEA;
	RADQueue.FenceEA     = FenceEA;

    return GrannySPUMain(&RADQueue, 0);
}
