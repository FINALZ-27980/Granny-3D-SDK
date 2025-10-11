// ========================================================================
// $File: //jeffr/granny_29/rt/granny_bink1_compression.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_bink1_compression.h"

#include "granny_memory.h"

// NOTE!  This include has to be here, to avoid the C/C++ include conflict
// that comes from including Jeff's C code.
#include <math.h>

#include "rrCore.h"
extern "C"
{
#include "binktc.h"
}

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

USING_GRANNY_NAMESPACE;

/* ========================================================================
   This is the glue code
   ======================================================================== */
uint32 GRANNY
ToBinkTC1(void *output,
          uint32 compress_to,
          int16 **input,
          real32 const *plane_weights,
          uint32 planes,
          uint32 width,
          uint32 height,
          void *temp, uint32 temp_size )
{
    return(to_BinkTC(output, compress_to, input, plane_weights, planes,
                     width, height, temp, temp_size));
}

uint32 GRANNY
ToBinkTCTempMem1(uint32 width, uint32 height)
{
    return(to_BinkTC_temp_mem(width, height));
}

uint32 GRANNY
ToBinkTCOutputMem1(uint32 width, uint32 height,
                   uint32 planes, uint32 compress_to)
{
    return(to_BinkTC_output_mem(width, height, planes, compress_to));
}

void GRANNY
BinkTCCheckSizes1(uint32 *width, uint32 *height)
{
    U32 wtemp, htemp;
    wtemp = *width;
    htemp = *height;
    BinkTC_check_sizes(&wtemp, &htemp);
    *width = wtemp;
    *height = htemp;
}

void GRANNY
FromBinkTC1(int16 **output,
            uint32 planes,
            void const * bink_buf,
            uint32 width,
            uint32 height,
            void *temp,
            uint32 temp_size)
{
    from_BinkTC(output, planes, bink_buf, width, height, temp, temp_size);
}

uint32 GRANNY
FromBinkTCTempMem1(void const *binkbuf)
{
    return(from_BinkTC_temp_mem(binkbuf));
}


RADDEFFUNC void PTR4* RADLINK g_radmalloc(U32 numbytes)
{
    // If we're linked with bink, we need to return an allocation aligned to at least
    // 32 bytes, which bink expects.  Depending on the order of the libraries on the
    // linker command line, we might wind up being the radmalloc, rather than the bink
    // version.
#if DefaultAllocationAlignment < 32
    int32 const Alignment = 32;
#else
    int32 const Alignment = DefaultAllocationAlignment;
#endif

    return(AllocateSizeAligned(Alignment, numbytes, AllocationUnknown));
}

RADDEFFUNC void RADLINK g_radfree(void PTR4* ptr)
{
    Deallocate(ptr);
}
