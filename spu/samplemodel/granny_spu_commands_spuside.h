#if !defined(GRANNY_SPU_COMMANDS_SPUSIDE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/cell/spu/granny_spu_commands_spuside.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#include <stdint.h>

EXPTYPE struct radspu_command_queue_spuside;
EXPTYPE struct radspu_command;

// These three functions must be provided by the connecting elf image
extern "C"
{
bool RADSPUObtainCommand(radspu_command_queue_spuside const* Queue,
                         radspu_command* Command,
                         unsigned int DMATag);

void RADSPUWorkingOn(radspu_command_queue_spuside const* Queue, uint64_t FenceValue);
void RADSPUFinished(radspu_command_queue_spuside const* Queue, uint64_t FenceValue);
}

// Provided by libgranny_spu
EXPAPI int GrannySPUMain(radspu_command_queue_spuside* Queue, int DMATag);

#include "header_postfix.h"
#define GRANNY_SPU_COMMANDS_SPUSIDE_H
#endif
