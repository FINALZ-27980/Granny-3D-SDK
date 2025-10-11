// ========================================================================
// $File: //jeffr/granny_29/spu/spurs/granny_spu_spurs.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny2_spu_samplemodel.h"

#include <cell/spurs.h>
#include <cell/dma.h>
#include <cell/atomic.h>
#include <spu_printf.h>

struct radspu_command_queue_spuside
{
    uint64_t SpursQueueEA;
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
        RetVal = cellSpursQueueTryPopBegin(RADQueue->SpursQueueEA, Command, DMATag);

        // Yield, but only if we get a "queue empty" signal
        if (RetVal == CELL_SPURS_TASK_ERROR_AGAIN)
        {
            RetVal = cellSpursQueuePopBegin(RADQueue->SpursQueueEA, Command, DMATag);
        }
    } while (RetVal == CELL_SPURS_TASK_ERROR_AGAIN || RetVal == CELL_SPURS_TASK_ERROR_BUSY);

    if (RetVal == CELL_OK)
        RetVal = cellSpursQueuePopEnd(RADQueue->SpursQueueEA, DMATag);

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


// #include <LibSN_SPU.h>

int
cellSpursTaskMain(qword argTask, uint64_t argTaskset)
{
    //snPause();

    radspu_command_queue_spuside RADQueue;
    RADQueue.SpursQueueEA = spu_extract((vec_ullong2)argTask,0);
    RADQueue.FenceEA      = spu_extract((vec_ullong2)argTask,1);

    return GrannySPUMain(&RADQueue, 0);
}



#define DEFAULT_GET_DMA_TAG 0
#define DEFAULT_PUT_DMA_TAG 1

// These functions are needed for shared RAD code
extern "C" int
RAD_memcpy_from_PPU( void * spu_address, void const * ppu_address, unsigned int bytes, int tag )
{
  cellDmaGet( spu_address, (uint64_t)(unsigned int)ppu_address, bytes, (tag < 0 ) ? DEFAULT_GET_DMA_TAG : tag, 0, 0 );

  if ( tag < 0 )
    cellDmaWaitTagStatusAll( 1 << DEFAULT_GET_DMA_TAG );

  return 1;
}


extern "C" int
RAD_memcpy_to_PPU( void * ppu_address, void const * spu_address, unsigned int bytes, int tag )
{
  cellDmaPut( spu_address, (uint64_t)(unsigned int)ppu_address, bytes, (tag < 0 ) ? DEFAULT_PUT_DMA_TAG : tag, 0, 0 );

  if ( tag < 0 )
    cellDmaWaitTagStatusAll( 1 << DEFAULT_PUT_DMA_TAG );

  return 1;
}


extern "C" int
RAD_wait_on_memcpy( int tag )
{
  cellDmaWaitTagStatusAll( 1 << tag );
  return 1;
}

