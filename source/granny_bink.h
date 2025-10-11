#if !defined(GRANNY_BINK_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_bink.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(TextureGroup);

struct pixel_layout;

EXPTYPE enum bink_texture_flags
{
    BinkEncodeAlpha = 0x1,
    BinkUseScaledRGBInsteadOfYUV = 0x2,
    BinkUseBink1 = 0x4,  // only for the decode now, Bink1 always used for encode

    bink_texture_flags_forceint = 0x7fffffff
};

EXPAPI GS_SAFE pixel_layout const &GetBinkPixelLayout(bool Alpha);

EXPAPI GS_SAFE int32x GetMaximumBinkImageSize(int32x Width, int32x Height,
                                              uint32x Flags, int32x Compression);

EXPAPI GS_PARAM int32x BinkCompressTexture(int32x Width, int32x Height,
                                           int32x SourceStride, void const *Source,
                                           uint32x Flags, int32x Compression, void *Dest);
EXPAPI GS_SAFE void BinkDecompressTexture(int32x Width, int32x Height,
                                          uint32x Flags,
                                          int32x SourceSize, void const *Source,
                                          pixel_layout const &DestLayout,
                                          int32x DestStride, void *Dest);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_BINK_H
#endif
