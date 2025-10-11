#if !defined(GRANNY_SPU_SAMPLE_MODEL_ANIMATIONS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/cell/spu/granny_spu_sample_model_animations.h $
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

BEGIN_GRANNY_NAMESPACE;

bool DispatchSampleModelCommand(int         CommandType,
                                void const* CommandBuffer);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_SPU_SAMPLE_MODEL_ANIMATIONS_H
#endif
