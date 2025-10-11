// ========================================================================
// $File: //jeffr/granny_29/rt/granny_spu_commands_ppuside.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_spu_commands_ppuside.h"

#include "granny_assert.h"
#include "granny_parameter_checking.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode Undefined_LogMessage
USING_GRANNY_NAMESPACE;

#if !defined(GRANNY_SPU_INTERFACE_IMPLEMENTED) || !GRANNY_SPU_INTERFACE_IMPLEMENTED

radspu_command_queue* GRANNY
InitCommandQueueSPURS(int32x QueueDepth, int32x NumTasks, void const* SpursTaskBinInfo, void* SpursInstance)
{
    Log0(ErrorLogMessage, PS3SubsystemLogMessage, "Not supported in this version of Granny!");
    return 0;
}

radspu_command_queue* GRANNY
InitCommandQueueSPUThreads(int32x QueueDepth, int32x NumThreads, void const* ThreadImage)
{
    Log0(ErrorLogMessage, PS3SubsystemLogMessage, "Not supported in this version of Granny!");
    return 0;
}


void GRANNY
ShutdownCommandQueueSPU(radspu_command_queue* Queue)
{
    Log0(ErrorLogMessage, PS3SubsystemLogMessage, "Not supported in this version of Granny!");
}


uint64x GRANNY
GetLastInsertedFenceSPU(radspu_command_queue* Queue)
{
    Log0(ErrorLogMessage, PS3SubsystemLogMessage, "Not supported in this version of Granny!");
    return 0;
}


uint64x GRANNY
GetCurrentClearedFenceSPU(radspu_command_queue* Queue)
{
    Log0(ErrorLogMessage, PS3SubsystemLogMessage, "Not supported in this version of Granny!");
    return 0;
}


uint64x GRANNY
InsertFenceSPU(radspu_command_queue* Queue)
{
    Log0(ErrorLogMessage, PS3SubsystemLogMessage, "Not supported in this version of Granny!");
    return 0;
}


uint64x GRANNY
TryInsertCommandSPU(radspu_command_queue* Queue, int32x CommandType,
                    void const* CommandBuffer, int32x CommandSize)
{
    Log0(ErrorLogMessage, PS3SubsystemLogMessage, "Not supported in this version of Granny!");
    return 0;
}

uint64x GRANNY
InsertCommandSPU(radspu_command_queue* Queue, int32x CommandType,
                 void const* CommandBuffer, int32x CommandSize)
{
    Log0(ErrorLogMessage, PS3SubsystemLogMessage, "Not supported in this version of Granny!");
    return 0;
}

#endif
