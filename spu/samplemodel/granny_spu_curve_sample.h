#if !defined(GRANNY_SPU_CURVE_SAMPLE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/cell/spu/granny_spu_curve_sample.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif


BEGIN_GRANNY_NAMESPACE;

struct sample_context;

void EvaluateSPUCurve(sample_context const& NOALIAS Context,
                      uint8 const*          NOALIAS CurveBytes,
                      int32x                        ResultDimension,
                      real32*               NOALIAS Result);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_SPU_CURVE_SAMPLE_H
#endif
