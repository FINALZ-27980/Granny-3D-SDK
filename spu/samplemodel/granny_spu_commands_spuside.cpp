// ========================================================================
// $File: //jeffr/granny_29/rt/cell/spu/granny_spu_commands_spuside.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_spu_command.h"
#include "granny_spu_commands_spuside.h"
#include "granny_spu_sample_model_animations.h"
#include "granny_memory.h"

#include <cell/dma.h>
#include <cell/spurs.h>

#include "granny_cpp_settings.h"
// ***VERSION_CHECK***

USING_GRANNY_NAMESPACE;

ALIGN_N(radspu_command, 128) NextCommand;
ALIGN_N(uint64x, 128)        FenceLine[128 / sizeof(uint64x)];

typedef bool command_dispatcher(int         CommandType,
                                void const* CommandBuffer);

static int
QueuePump(radspu_command_queue_spuside* Queue,
          command_dispatcher Dispatcher,
          int DMATag)
{
    // Write the fence.  Note that the fence number is the first uint64x in a 128
    // block of memory, cacheline aligned.

    int RetVal = 0;
    while (true)
    {
        if (RADSPUObtainCommand(Queue, &NextCommand, DMATag))
        {
            RADSPUWorkingOn(Queue, NextCommand.FenceValue);
            //cellAtomicStore64(FenceLine+sizeof(uint64_t), Queue.FenceNumberEA, NextCommand.FenceValue);

            if (NextCommand.CommandType != SPUCommand_Shutdown &&
                NextCommand.CommandType != SPUCommand_FenceNOP)
            {
                if (!(*Dispatcher)(NextCommand.CommandType, NextCommand.CommandBuffer))
                {
                    // TODO: Error in the dispatcher, log?  return failure?
                }
            }

            // Write the fence.  Note that the fence number is the first uint64x in a 128
            // block of memory, cacheline aligned.
            RADSPUFinished(Queue, NextCommand.FenceValue);
            // {
            //     cellAtomicStore64(FenceLine, Queue.FenceNumberEA, NextCommand.FenceValue);
            // }

            if(NextCommand.CommandType == SPUCommand_Shutdown)
            {
                break;
            }
        }
        else
        {
            // The blocking obtaincommand only returns false when an error is encountered.
            RetVal = -1;
            break;
        }
    }

    return RetVal;
}


int
GrannySPUMain(radspu_command_queue_spuside* Queue, int DMATag)
{
    return QueuePump(Queue, DispatchSampleModelCommand, DMATag);
}
