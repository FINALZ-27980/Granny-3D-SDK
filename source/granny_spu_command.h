#if !defined(GRANNY_SPU_COMMAND_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_spu_command.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

USING_GRANNY_NAMESPACE EXPGROUP(SPUGroup);

enum radspu_command_type
{
    // 0 is reserved for Shutdown
    // 1 is reserved for a FenceNOP
    SPUCommand_Shutdown = 0,
    SPUCommand_FenceNOP = 1,

    radspu_command_type_forceint = 0x7fffffff
};

struct radspu_command
{
    uint64x CommandType;
    uint64x FenceValue;

    char CommandBuffer[128 - 2*SizeOf(uint64x)];
};

EXPTYPE enum samplemodel_command_type
{
    // 0 is reserved for Shutdown
    // 1 is reserved for a FenceNOP
    SPUCommand_Reserved0 = 0,
    SPUCommand_Reserved1 = 1,

    SPUCommand_SampleModelAnims           = 2,
    SPUCommand_SampleModelAnimAccelerated = 3,
    SPUCommand_SampleSingleControl        = 4,

    samplemodel_command_type_forceint = 0x7fffffff
};

EXPTYPE struct radspu_command_sma
{
    uint64x  InstanceEA;
    uint64x  BoneCount;
    uint64x  LocalPoseTransformsEA;

    float     LocalPoseFillThreshold;
    float     AllowedError;
};


EXPTYPE struct radspu_command_ssc
{
    uint64x  InstanceEA;
    uint64x  ControlEA;
    uint64x  BoneCount;

    uint64x  LocalPoseTransformsEA;
    float    LocalPoseFillThreshold;

    float    AllowedError;
};


EXPTYPE struct radspu_command_sma_accel
{
    uint64x  InstanceEA;
    uint64x  BoneCount;
    uint64x  OffsetEA;
    uint64x  WorldPoseEA;
    uint64x  CompositeEA;

    float    LocalPoseFillThreshold;
    float    AllowedError;
};


#include "header_postfix.h"
#define GRANNY_SPU_COMMAND_H
#endif /* GRANNY_SPU_COMMAND_H */
