#if !defined(GRANNY_SPU_COMMANDS_SPURS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_spu_commands_ppuside.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(SPUGroup);

EXPTYPE struct radspu_command_queue;

EXPAPI radspu_command_queue* InitCommandQueueSPURS(int32x QueueDepth, int32x NumTasks,
                                                   void const* SpursTaskBinInfo,
                                                   void*       SpursInstance);
EXPAPI radspu_command_queue* InitCommandQueueSPUThreads(int32x QueueDepth,
                                                        int32x NumTasks,
                                                        void const* ThreadImage);

EXPAPI void                  ShutdownCommandQueueSPU(radspu_command_queue* Queue);

EXPAPI uint64x GetLastInsertedFenceSPU(radspu_command_queue* Queue);
EXPAPI uint64x GetCurrentClearedFenceSPU(radspu_command_queue* Queue);
EXPAPI uint64x InsertFenceSPU(radspu_command_queue* Queue);

// return of 0xff...fff indicates failure to successfully submit.  TryInsert is
// non-blocking, Insert is blocking.
EXPAPI uint64x TryInsertCommandSPU(radspu_command_queue* Queue, int32x CommandType,
                                   void const* CommandBuffer, int32x CommandSize);
EXPAPI uint64x InsertCommandSPU(radspu_command_queue* Queue, int32x CommandType,
                                void const* CommandBuffer, int32x CommandSize);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_SPU_COMMANDS_SPURS_H
#endif /* GRANNY_SPU_COMMANDS_SPURS_H */
