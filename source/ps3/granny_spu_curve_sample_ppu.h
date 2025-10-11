// ========================================================================
// $File: //jeffr/granny_29/rt/cell/granny_spu_curve_sample_ppu.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#if !defined(GRANNY_SPU_CURVE_SAMPLE_PPU_H)

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif


BEGIN_GRANNY_NAMESPACE;

struct sample_context;

// For computing total delta
void ExtractFirstAndLastKnots(uint8 const*          NOALIAS CurveBytes,
                              int32x                        ResultDimension,
                              real32*               NOALIAS ResultFirst,
                              real32*               NOALIAS ResultLast);

void EvaluateSPUCurve(sample_context const& NOALIAS Context,
                      uint8 const*          NOALIAS CurveBytes,
                      int32x                        ResultDimension,
                      real32*               NOALIAS Result);

END_GRANNY_NAMESPACE;

#define GRANNY_SPU_CURVE_SAMPLE_PPU_H
#endif /* GRANNY_SPU_CURVE_SAMPLE_PPU_H */
