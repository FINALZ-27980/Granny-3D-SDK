// ========================================================================
// $File: //jeffr/granny_29/rt/wii/wii_granny_accelerated_deformers.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_deformers.h"
#include "granny_accelerated_deformers.h"

#include "granny_assert.h"
#include "granny_memory.h"
#include "granny_vertex_data.h"
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

static real32 UtilityFloats[2] = { 0, 1.0f / 255.0f };

#define LOAD_INDIRECT_ADDR(Idx)                 \
    lbz         Index, (SIDX + Idx)(Source);    \
    li          Temp,  2;                       \
    slw         Index, Index, Temp;             \
    lwzx        Index, TransformTable, Index;   \
    li          Temp,  6;                       \
    slw         Index, Index, Temp;             \
    add         Index, Transforms, Index

#define LOAD_DIRECT_ADDR(Idx)                   \
    lbz         Index, (SIDX + Idx)(Source);    \
    li          Temp,  6;                       \
    slw         Index, Index, Temp;             \
    add         Index, Transforms, Index

#define LOAD_INDIRECT_ADDR_DWORD                \
    lwz         Index, (SIDX)(Source);          \
    li          Temp,  2;                       \
    slw         Index, Index, Temp;             \
    lwzx        Index, TransformTable, Index;   \
    li          Temp,  6;                       \
    slw         Index, Index, Temp;             \
    add         Index, Transforms, Index

#define LOAD_DIRECT_ADDR_DWORD                  \
    lwz         Index, (SIDX)(Source);          \
    li          Temp,  6;                       \
    slw         Index, Index, Temp;             \
    add         Index, Transforms, Index


#define LOAD_WEIGHT(Idx)                                    \
    psq_l       CURR_WEIGHT, (SWGHT + Idx)(Source), 1, 6;   \
    ps_muls1.   CURR_WEIGHT, CURR_WEIGHT, UTIL

// load 4x3 (with transpose, arg.)
#define LOAD4x3()                                 \
    psq_l       TEMP0, ( 0 + 0)(Index), 0, 0;       \
    psq_l       TEMP1, (16 + 0)(Index), 0, 0;       \
    ps_merge00  MAT_0_01, TEMP0, TEMP1;             \
    ps_merge11  MAT_1_01, TEMP0, TEMP1;             \
                                                    \
    psq_l       MAT_2_01, ( 8 + 0)(Index), 0, 0;    \
    psq_l       TEMP0,    (16 + 8)(Index), 0, 0;    \
    ps_merge00  MAT_2_01, MAT_2_01, TEMP0;          \
                                                    \
    psq_l       TEMP0, (32 + 0)(Index), 0, 0;       \
    psq_l       TEMP1, (48 + 0)(Index), 0, 0;       \
    ps_merge00  MAT_0_23, TEMP0, TEMP1;             \
    ps_merge11  MAT_1_23, TEMP0, TEMP1;             \
                                                    \
    psq_l       MAT_2_23, (32 + 8)(Index), 0, 0;    \
    psq_l       TEMP0,    (48 + 8)(Index), 0, 0;    \
    ps_merge00  MAT_2_23, MAT_2_23, TEMP0

#define MULVEC4x3(Acc, Src, Temp)               \
    ps_mul      Temp##0, Src##01, MAT_0_01;        \
    ps_madd     Temp##0, Src##23, MAT_0_23, Temp##0;  \
    ps_sum0     Acc##01, Temp##0, Temp##0, Temp##0;      \
                                                \
    ps_mul      Temp##1, Src##01, MAT_1_01;        \
    ps_madd     Temp##1, Src##23, MAT_1_23, Temp##1;  \
    ps_sum1     Acc##01, Temp##1, Acc##01, Temp##1;   \
                                                \
    ps_mul      Temp##2, Src##01, MAT_2_01;        \
    ps_madd     Temp##2, Src##23, MAT_2_23, Temp##2;  \
    ps_sum0     Acc##23, Temp##2, Temp##2, Temp##2;      \


#define WEIGHT_MUL(Dst, Acc)                    \
    ps_muls0   Dst##01,  Acc##01, CURR_WEIGHT;  \
    ps_muls0   Dst##23,  Acc##23, CURR_WEIGHT

#define WEIGHT_MADD(Dst, Acc)                               \
    ps_madds0   Dst##01,  Acc##01, CURR_WEIGHT, Dst##01;    \
    ps_madds0   Dst##23,  Acc##23, CURR_WEIGHT, Dst##23


#define BRANCH_0_WEIGHT(ExitPoint) \
    ps_cmpo0   cr0, CURR_WEIGHT, UTIL; \
    beq ExitPoint


static void
WiiDeformPWNT3132D(int32x register            Count,
                   void const*                SourceInit,
                   void*                      DestInit,
                   matrix_4x4 const register* Transforms,
                   int32x                     TailCopy32Count,
                   int32x register            SourceStride,
                   int32x register            DestStride)
{
    if (Count == 0)
        return;

#define SPOS  0
#define SIDX  12
#define SNRM  16
#define SUV   28
#define DPOS  0
#define DNRM  12
#define DUV   24

#define POS01         fp0
#define POS23         fp1
#define NORM01        fp2
#define NORM23        fp3
#define UTIL          fp4
#define DSTPOS01      fp5
#define DSTPOS23      fp6
#define DSTNORM01     fp7
#define DSTNORM23     fp8
#define MAT_0_01      fp9
#define MAT_0_23      fp10
#define MAT_1_01      fp11
#define MAT_1_23      fp12
#define MAT_2_01      fp13
#define MAT_2_23      fp14
#define TEMP0         fp15
#define TEMP1         fp16
#define TEMP2         fp17
    CompileAssert(OffsetFromType(pwnt3132_vertex, Position)  == SPOS);
    CompileAssert(OffsetFromType(pwnt3132_vertex, BoneIndex) == SIDX);
    CompileAssert(OffsetFromType(pwnt3132_vertex, Normal)    == SNRM);
    CompileAssert(OffsetFromType(pwnt3132_vertex, UV)        == SUV);
    CompileAssert(OffsetFromType(pnt332_vertex, Position) == DPOS);
    CompileAssert(OffsetFromType(pnt332_vertex, Normal)   == DNRM);
    CompileAssert(OffsetFromType(pnt332_vertex, UV)       == DUV);

    void const register* Source  = SourceInit;
    void const register* Dest    = DestInit;
    real32 register*     Utility = &UtilityFloats[0];

    register uint32  Index;
    register uint32  Temp;

    asm
    {
        // Setup the quantization load register for unsigned 8 bit load, no scale (we need
        // 1/255, not 1/256)
        lis Temp, 0x0004;
        mtspr     GQR6, Temp

        // Load the utility register
        psq_l   UTIL,  0(Utility), 0, 0;

        // Loop for Count vertices
        mtctr Count;
Loop:
        // Load position
        psq_l       POS01,  (SPOS + 0)(Source), 0, 0;
        psq_l       POS23,  (SPOS + 8)(Source), 1, 0;

        // Load normal
        psq_l       NORM01, (SNRM + 0)(Source), 0, 0;	
        psq_l       NORM23, (SNRM + 8)(Source), 1, 0;
        ps_merge00  NORM23, NORM23, UTIL;

        // ----------------------------------------------------------
        // Index 0
        LOAD_DIRECT_ADDR_DWORD;
        LOAD4x3();
        MULVEC4x3(DSTPOS,  POS,  TEMP);
        MULVEC4x3(DSTNORM, NORM, TEMP);

ExitVert:
        psq_st   DSTPOS01,  (DPOS + 0)(Dest),  0, 0;
        psq_st   DSTPOS23,  (DPOS + 8)(Dest),  1, 0;
        psq_st   DSTNORM01, (DNRM + 0)(Dest),  0, 0;
        psq_st   DSTNORM23, (DNRM + 8)(Dest),  1, 0;
        psq_l    TEMP0, SUV(Source), 0, 0;
        psq_st   TEMP0, DUV(Dest),   0, 0;

        add Source, Source, SourceStride;
        add Dest,   Dest, DestStride;

        bdnz+ Loop;    // -------------------------- LOOP END
    }

#undef SPOS
#undef SIDX
#undef SNRM
#undef SUV
#undef DPOS
#undef DNRM
#undef DUV

#undef POS01
#undef POS23
#undef NORM01
#undef NORM23
#undef UTIL
#undef DSTPOS01
#undef DSTPOS23
#undef DSTNORM01
#undef DSTNORM23
#undef MAT_0_01
#undef MAT_0_23
#undef MAT_1_01
#undef MAT_1_23
#undef MAT_2_01
#undef MAT_2_23
#undef TEMP0
#undef TEMP1
#undef TEMP2
}


static void
WiiDeformPWNT3132I(int32x register            Count,
                   void const*                SourceInit,
                   void*                      DestInit,
                   int32x const register*     TransformTable,
                   matrix_4x4 const register* Transforms,
                   int32x                     TailCopy32Count,
                   int32x register            SourceStride,
                   int32x register            DestStride)
{
    if (Count == 0)
        return;

#define SPOS  0
#define SIDX  12
#define SNRM  16
#define SUV   28
#define DPOS  0
#define DNRM  12
#define DUV   24

#define POS01         fp0
#define POS23         fp1
#define NORM01        fp2
#define NORM23        fp3
#define UTIL          fp4
#define DSTPOS01      fp5
#define DSTPOS23      fp6
#define DSTNORM01     fp7
#define DSTNORM23     fp8
#define MAT_0_01      fp9
#define MAT_0_23      fp10
#define MAT_1_01      fp11
#define MAT_1_23      fp12
#define MAT_2_01      fp13
#define MAT_2_23      fp14
#define TEMP0         fp15
#define TEMP1         fp16
#define TEMP2         fp17
    CompileAssert(OffsetFromType(pwnt3132_vertex, Position)  == SPOS);
    CompileAssert(OffsetFromType(pwnt3132_vertex, BoneIndex) == SIDX);
    CompileAssert(OffsetFromType(pwnt3132_vertex, Normal)    == SNRM);
    CompileAssert(OffsetFromType(pwnt3132_vertex, UV)        == SUV);
    CompileAssert(OffsetFromType(pnt332_vertex, Position) == DPOS);
    CompileAssert(OffsetFromType(pnt332_vertex, Normal)   == DNRM);
    CompileAssert(OffsetFromType(pnt332_vertex, UV)       == DUV);

    void const register* Source  = SourceInit;
    void const register* Dest    = DestInit;
    real32 register*     Utility = &UtilityFloats[0];

    register uint32  Index;
    register uint32  Temp;

    asm
    {
        // Setup the quantization load register for unsigned 8 bit load, no scale (we need
        // 1/255, not 1/256)
        lis Temp, 0x0004;
        mtspr     GQR6, Temp

        // Load the utility register
        psq_l   UTIL,  0(Utility), 0, 0;

        // Loop for Count vertices
        mtctr Count;
Loop:
        // Load position
        psq_l       POS01,  (SPOS + 0)(Source), 0, 0;
        psq_l       POS23,  (SPOS + 8)(Source), 1, 0;

        // Load normal
        psq_l       NORM01, (SNRM + 0)(Source), 0, 0;	
        psq_l       NORM23, (SNRM + 8)(Source), 1, 0;
        ps_merge00  NORM23, NORM23, UTIL;

        // ----------------------------------------------------------
        // Index 0
        LOAD_INDIRECT_ADDR_DWORD;
        LOAD4x3();
        MULVEC4x3(DSTPOS, POS,  TEMP);
        MULVEC4x3(DSTNORM, NORM, TEMP);

ExitVert:
        psq_st   DSTPOS01,  (DPOS + 0)(Dest),  0, 0;
        psq_st   DSTPOS23,  (DPOS + 8)(Dest),  1, 0;
        psq_st   DSTNORM01, (DNRM + 0)(Dest),  0, 0;
        psq_st   DSTNORM23, (DNRM + 8)(Dest),  1, 0;
        psq_l    TEMP0, SUV(Source), 0, 0;
        psq_st   TEMP0, DUV(Dest),   0, 0;

        add Source, Source, SourceStride;
        add Dest,   Dest, DestStride;

        bdnz+ Loop;    // -------------------------- LOOP END
    }

#undef SPOS
#undef SIDX
#undef SNRM
#undef SUV
#undef DPOS
#undef DNRM
#undef DUV

#undef POS01
#undef POS23
#undef NORM01
#undef NORM23
#undef UTIL
#undef DSTPOS01
#undef DSTPOS23
#undef DSTNORM01
#undef DSTNORM23
#undef MAT_0_01
#undef MAT_0_23
#undef MAT_1_01
#undef MAT_1_23
#undef MAT_2_01
#undef MAT_2_23
#undef TEMP0
#undef TEMP1
#undef TEMP2
}


static void
WiiDeformPWNT3232D(int32x register            Count,
                   void const*                SourceInit,
                   void*                      DestInit,
                   matrix_4x4 const register* Transforms,
                   int32x                     TailCopy32Count,
                   int32x register            SourceStride,
                   int32x register            DestStride)
{
    if (Count == 0)
        return;

#define SPOS  0
#define SWGHT 12
#define SIDX  14
#define SNRM  16
#define SUV   28
#define DPOS  0
#define DNRM  12
#define DUV   24

#define POS01         fp0
#define POS23         fp1
#define NORM01        fp2
#define NORM23        fp3
#define UTIL          fp4
#define DSTPOS01      fp5
#define DSTPOS23      fp6
#define DSTNORM01     fp7
#define DSTNORM23     fp8
#define MAT_0_01      fp9
#define MAT_0_23      fp10
#define MAT_1_01      fp11
#define MAT_1_23      fp12
#define MAT_2_01      fp13
#define MAT_2_23      fp14
#define TEMP0         fp15
#define TEMP1         fp16
#define TEMP2         fp17
#define ACCUM_P01     fp18
#define ACCUM_P23     fp19
#define ACCUM_N01     fp20
#define ACCUM_N23     fp21
#define CURR_WEIGHT   fp22
    CompileAssert(OffsetFromType(pwnt3232_vertex, Position)    == SPOS);
    CompileAssert(OffsetFromType(pwnt3232_vertex, BoneWeights) == SWGHT);
    CompileAssert(OffsetFromType(pwnt3232_vertex, BoneIndices) == SIDX);
    CompileAssert(OffsetFromType(pwnt3232_vertex, Normal)      == SNRM);
    CompileAssert(OffsetFromType(pwnt3232_vertex, UV)          == SUV);
    CompileAssert(OffsetFromType(pnt332_vertex, Position) == DPOS);
    CompileAssert(OffsetFromType(pnt332_vertex, Normal)   == DNRM);
    CompileAssert(OffsetFromType(pnt332_vertex, UV)       == DUV);

    void const register* Source  = SourceInit;
    void const register* Dest    = DestInit;
    real32 register*     Utility = &UtilityFloats[0];

    register uint32  Index;
    register uint32  Temp;

    asm
    {
        // Setup the quantization load register for unsigned 8 bit load, no scale (we need
        // 1/255, not 1/256)
        lis Temp, 0x0004;
        mtspr     GQR6, Temp

        // Load the utility register
        psq_l   UTIL,  0(Utility), 0, 0;

        // Loop for Count vertices
        mtctr Count;
Loop:
        // Load position
        psq_l       POS01,  (SPOS + 0)(Source), 0, 0;
        psq_l       POS23,  (SPOS + 8)(Source), 1, 0;

        // Load normal
        psq_l       NORM01, (SNRM + 0)(Source), 0, 0;	
        psq_l       NORM23, (SNRM + 8)(Source), 1, 0;
        ps_merge00  NORM23, NORM23, UTIL;

        // ----------------------------------------------------------
        // Index 0
        LOAD_DIRECT_ADDR(0);
        LOAD4x3();
        LOAD_WEIGHT(0);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MUL(DSTPOS, ACCUM_P);    // First index does a mul into the dest
        WEIGHT_MUL(DSTNORM, ACCUM_N);

        // ----------------------------------------------------------
        // Index 1
        LOAD_DIRECT_ADDR(1);
        LOAD4x3();
        LOAD_WEIGHT(1);
        BRANCH_0_WEIGHT(ExitVert);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MADD(DSTPOS, ACCUM_P);   // Subsequent loads madd in
        WEIGHT_MADD(DSTNORM, ACCUM_N);

ExitVert:
        psq_st   DSTPOS01,  (DPOS + 0)(Dest),  0, 0;
        psq_st   DSTPOS23,  (DPOS + 8)(Dest),  1, 0;
        psq_st   DSTNORM01, (DNRM + 0)(Dest),  0, 0;
        psq_st   DSTNORM23, (DNRM + 8)(Dest),  1, 0;
        psq_l    TEMP0, SUV(Source), 0, 0;
        psq_st   TEMP0, DUV(Dest),   0, 0;

        add Source, Source, SourceStride;
        add Dest,   Dest, DestStride;

        bdnz+ Loop;    // -------------------------- LOOP END
    }

#undef SPOS
#undef SWGHT
#undef SIDX
#undef SNRM
#undef SUV
#undef DPOS
#undef DNRM
#undef DUV

#undef POS01
#undef POS23
#undef NORM01
#undef NORM23
#undef UTIL
#undef DSTPOS01
#undef DSTPOS23
#undef DSTNORM01
#undef DSTNORM23
#undef MAT_0_01
#undef MAT_0_23
#undef MAT_1_01
#undef MAT_1_23
#undef MAT_2_01
#undef MAT_2_23
#undef TEMP0
#undef TEMP1
#undef TEMP2
#undef ACCUM_P01
#undef ACCUM_P23
#undef ACCUM_N01
#undef ACCUM_N23
#undef CURR_WEIGHT
}


static void
WiiDeformPWNT3232I(int32x register            Count,
                   void const*                SourceInit,
                   void*                      DestInit,
                   int32x const register*     TransformTable,
                   matrix_4x4 const register* Transforms,
                   int32x                     TailCopy32Count,
                   int32x register            SourceStride,
                   int32x register            DestStride)
{
    if (Count == 0)
        return;

#define SPOS  0
#define SWGHT 12
#define SIDX  14
#define SNRM  16
#define SUV   28
#define DPOS  0
#define DNRM  12
#define DUV   24

#define POS01         fp0
#define POS23         fp1
#define NORM01        fp2
#define NORM23        fp3
#define UTIL          fp4
#define DSTPOS01      fp5
#define DSTPOS23      fp6
#define DSTNORM01     fp7
#define DSTNORM23     fp8
#define MAT_0_01      fp9
#define MAT_0_23      fp10
#define MAT_1_01      fp11
#define MAT_1_23      fp12
#define MAT_2_01      fp13
#define MAT_2_23      fp14
#define TEMP0         fp15
#define TEMP1         fp16
#define TEMP2         fp17
#define ACCUM_P01     fp18
#define ACCUM_P23     fp19
#define ACCUM_N01     fp20
#define ACCUM_N23     fp21
#define CURR_WEIGHT   fp22
    CompileAssert(OffsetFromType(pwnt3232_vertex, Position)    == SPOS);
    CompileAssert(OffsetFromType(pwnt3232_vertex, BoneWeights) == SWGHT);
    CompileAssert(OffsetFromType(pwnt3232_vertex, BoneIndices) == SIDX);
    CompileAssert(OffsetFromType(pwnt3232_vertex, Normal)      == SNRM);
    CompileAssert(OffsetFromType(pwnt3232_vertex, UV)          == SUV);
    CompileAssert(OffsetFromType(pnt332_vertex, Position) == DPOS);
    CompileAssert(OffsetFromType(pnt332_vertex, Normal)   == DNRM);
    CompileAssert(OffsetFromType(pnt332_vertex, UV)       == DUV);

    void const register* Source  = SourceInit;
    void const register* Dest    = DestInit;
    real32 register*     Utility = &UtilityFloats[0];

    register uint32  Index;
    register uint32  Temp;

    asm
    {
        // Setup the quantization load register for unsigned 8 bit load, no scale (we need
        // 1/255, not 1/256)
        lis Temp, 0x0004;
        mtspr     GQR6, Temp

        // Load the utility register
        psq_l   UTIL,  0(Utility), 0, 0;

        // Loop for Count vertices
        mtctr Count;
Loop:
        // Load position
        psq_l       POS01,  (SPOS + 0)(Source), 0, 0;
        psq_l       POS23,  (SPOS + 8)(Source), 1, 0;

        // Load normal
        psq_l       NORM01, (SNRM + 0)(Source), 0, 0;	
        psq_l       NORM23, (SNRM + 8)(Source), 1, 0;
        ps_merge00  NORM23, NORM23, UTIL;

        // ----------------------------------------------------------
        // Index 0
        LOAD_INDIRECT_ADDR(0);
        LOAD4x3();
        LOAD_WEIGHT(0);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MUL(DSTPOS, ACCUM_P);    // First index does a mul into the dest
        WEIGHT_MUL(DSTNORM, ACCUM_N);

        // ----------------------------------------------------------
        // Index 1
        LOAD_INDIRECT_ADDR(1);
        LOAD4x3();
        LOAD_WEIGHT(1);
        BRANCH_0_WEIGHT(ExitVert);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MADD(DSTPOS, ACCUM_P);   // Subsequent loads madd in
        WEIGHT_MADD(DSTNORM, ACCUM_N);

ExitVert:
        psq_st   DSTPOS01,  (DPOS + 0)(Dest),  0, 0;
        psq_st   DSTPOS23,  (DPOS + 8)(Dest),  1, 0;
        psq_st   DSTNORM01, (DNRM + 0)(Dest),  0, 0;
        psq_st   DSTNORM23, (DNRM + 8)(Dest),  1, 0;
        psq_l    TEMP0, SUV(Source), 0, 0;
        psq_st   TEMP0, DUV(Dest),   0, 0;

        add Source, Source, SourceStride;
        add Dest,   Dest, DestStride;

        bdnz+ Loop;    // -------------------------- LOOP END
    }

#undef SPOS
#undef SWGHT
#undef SIDX
#undef SNRM
#undef SUV
#undef DPOS
#undef DNRM
#undef DUV

#undef POS01
#undef POS23
#undef NORM01
#undef NORM23
#undef UTIL
#undef DSTPOS01
#undef DSTPOS23
#undef DSTNORM01
#undef DSTNORM23
#undef MAT_0_01
#undef MAT_0_23
#undef MAT_1_01
#undef MAT_1_23
#undef MAT_2_01
#undef MAT_2_23
#undef TEMP0
#undef TEMP1
#undef TEMP2
#undef ACCUM_P01
#undef ACCUM_P23
#undef ACCUM_N01
#undef ACCUM_N23
#undef CURR_WEIGHT
}


static void
WiiDeformPWNT3432D(int32x register            Count,
                   void const*                SourceInit,
                   void*                      DestInit,
                   matrix_4x4 const register* Transforms,
                   int32x                     TailCopy32Count,
                   int32x register            SourceStride,
                   int32x register            DestStride)
{
    if (Count == 0)
        return;

#define SPOS  0
#define SWGHT 12
#define SIDX  16
#define SNRM  20
#define SUV   32
#define DPOS  0
#define DNRM  12
#define DUV   24

#define POS01         fp0
#define POS23         fp1
#define NORM01        fp2
#define NORM23        fp3
#define UTIL          fp4
#define DSTPOS01      fp5
#define DSTPOS23      fp6
#define DSTNORM01     fp7
#define DSTNORM23     fp8
#define MAT_0_01      fp9
#define MAT_0_23      fp10
#define MAT_1_01      fp11
#define MAT_1_23      fp12
#define MAT_2_01      fp13
#define MAT_2_23      fp14
#define TEMP0         fp15
#define TEMP1         fp16
#define TEMP2         fp17
#define ACCUM_P01     fp18
#define ACCUM_P23     fp19
#define ACCUM_N01     fp20
#define ACCUM_N23     fp21
#define CURR_WEIGHT   fp22
    CompileAssert(OffsetFromType(pwnt3432_vertex, Position)    == SPOS);
    CompileAssert(OffsetFromType(pwnt3432_vertex, BoneWeights) == SWGHT);
    CompileAssert(OffsetFromType(pwnt3432_vertex, BoneIndices) == SIDX);
    CompileAssert(OffsetFromType(pwnt3432_vertex, Normal)      == SNRM);
    CompileAssert(OffsetFromType(pwnt3432_vertex, UV)          == SUV);
    CompileAssert(OffsetFromType(pnt332_vertex, Position) == DPOS);
    CompileAssert(OffsetFromType(pnt332_vertex, Normal)   == DNRM);
    CompileAssert(OffsetFromType(pnt332_vertex, UV)       == DUV);

    void const register* Source  = SourceInit;
    void const register* Dest    = DestInit;
    real32 register*     Utility = &UtilityFloats[0];

    register uint32  Index;
    register uint32  Temp;

    asm
    {
        // Setup the quantization load register for unsigned 8 bit load, no scale (we need
        // 1/255, not 1/256)
        lis Temp, 0x0004;
        mtspr     GQR6, Temp

        // Load the utility register
        psq_l   UTIL,  0(Utility), 0, 0;

        // Loop for Count vertices
        mtctr Count;
Loop:
        // Load position
        psq_l       POS01,  (SPOS + 0)(Source), 0, 0;
        psq_l       POS23,  (SPOS + 8)(Source), 1, 0;

        // Load normal
        psq_l       NORM01, (SNRM + 0)(Source), 0, 0;	
        psq_l       NORM23, (SNRM + 8)(Source), 1, 0;
        ps_merge00  NORM23, NORM23, UTIL;

        // ----------------------------------------------------------
        // Index 0
        LOAD_DIRECT_ADDR(0);
        LOAD4x3();
        LOAD_WEIGHT(0);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MUL(DSTPOS, ACCUM_P);    // First index does a mul into the dest
        WEIGHT_MUL(DSTNORM, ACCUM_N);

        // ----------------------------------------------------------
        // Index 1
        LOAD_DIRECT_ADDR(1);
        LOAD4x3();
        LOAD_WEIGHT(1);
        BRANCH_0_WEIGHT(ExitVert);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MADD(DSTPOS, ACCUM_P);   // Subsequent loads madd in
        WEIGHT_MADD(DSTNORM, ACCUM_N);

        // ----------------------------------------------------------
        // Index 2
        LOAD_DIRECT_ADDR(2);
        LOAD4x3();
        LOAD_WEIGHT(2);
        BRANCH_0_WEIGHT(ExitVert);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MADD(DSTPOS,  ACCUM_P);
        WEIGHT_MADD(DSTNORM, ACCUM_N);

        // ----------------------------------------------------------
        // Index 3
        LOAD_DIRECT_ADDR(3);
        LOAD4x3();
        LOAD_WEIGHT(3);
        BRANCH_0_WEIGHT(ExitVert);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MADD(DSTPOS,  ACCUM_P);
        WEIGHT_MADD(DSTNORM, ACCUM_N);

ExitVert:
        psq_st   DSTPOS01,  (DPOS + 0)(Dest),  0, 0;
        psq_st   DSTPOS23,  (DPOS + 8)(Dest),  1, 0;
        psq_st   DSTNORM01, (DNRM + 0)(Dest),  0, 0;
        psq_st   DSTNORM23, (DNRM + 8)(Dest),  1, 0;
        psq_l    TEMP0, SUV(Source), 0, 0;
        psq_st   TEMP0, DUV(Dest),   0, 0;

        add Source, Source, SourceStride;
        add Dest,   Dest, DestStride;

        bdnz+ Loop;    // -------------------------- LOOP END
    }

#undef SPOS
#undef SWGHT
#undef SIDX
#undef SNRM
#undef SUV
#undef DPOS
#undef DNRM
#undef DUV

#undef POS01
#undef POS23
#undef NORM01
#undef NORM23
#undef UTIL
#undef DSTPOS01
#undef DSTPOS23
#undef DSTNORM01
#undef DSTNORM23
#undef MAT_0_01
#undef MAT_0_23
#undef MAT_1_01
#undef MAT_1_23
#undef MAT_2_01
#undef MAT_2_23
#undef TEMP0
#undef TEMP1
#undef TEMP2
#undef ACCUM_P01
#undef ACCUM_P23
#undef ACCUM_N01
#undef ACCUM_N23
#undef CURR_WEIGHT
}


static void
WiiDeformPWNT3432I(int32x register            Count,
                   void const*                SourceInit,
                   void*                      DestInit,
                   int32x const register*     TransformTable,
                   matrix_4x4 const register* Transforms,
                   int32x                     TailCopy32Count,
                   int32x register            SourceStride,
                   int32x register            DestStride)
{
    if (Count == 0)
        return;

#define SPOS  0
#define SWGHT 12
#define SIDX  16
#define SNRM  20
#define SUV   32
#define DPOS  0
#define DNRM  12
#define DUV   24

#define POS01         fp0
#define POS23         fp1
#define NORM01        fp2
#define NORM23        fp3
#define UTIL          fp4
#define DSTPOS01      fp5
#define DSTPOS23      fp6
#define DSTNORM01     fp7
#define DSTNORM23     fp8
#define MAT_0_01      fp9
#define MAT_0_23      fp10
#define MAT_1_01      fp11
#define MAT_1_23      fp12
#define MAT_2_01      fp13
#define MAT_2_23      fp14
#define TEMP0         fp15
#define TEMP1         fp16
#define TEMP2         fp17
#define ACCUM_P01     fp18
#define ACCUM_P23     fp19
#define ACCUM_N01     fp20
#define ACCUM_N23     fp21
#define CURR_WEIGHT   fp22
    CompileAssert(OffsetFromType(pwnt3432_vertex, Position)    == SPOS);
    CompileAssert(OffsetFromType(pwnt3432_vertex, BoneWeights) == SWGHT);
    CompileAssert(OffsetFromType(pwnt3432_vertex, BoneIndices) == SIDX);
    CompileAssert(OffsetFromType(pwnt3432_vertex, Normal)      == SNRM);
    CompileAssert(OffsetFromType(pwnt3432_vertex, UV)          == SUV);
    CompileAssert(OffsetFromType(pnt332_vertex, Position) == DPOS);
    CompileAssert(OffsetFromType(pnt332_vertex, Normal)   == DNRM);
    CompileAssert(OffsetFromType(pnt332_vertex, UV)       == DUV);

    void const register* Source  = SourceInit;
    void const register* Dest    = DestInit;
    real32 register*     Utility = &UtilityFloats[0];

    register uint32  Index;
    register uint32  Temp;

    asm
    {
        // Setup the quantization load register for unsigned 8 bit load, no scale (we need
        // 1/255, not 1/256)
        lis Temp, 0x0004;
        mtspr     GQR6, Temp

        // Load the utility register
        psq_l   UTIL,  0(Utility), 0, 0;

        // Loop for Count vertices
        mtctr Count;
Loop:
        // Load position
        psq_l       POS01,  (SPOS + 0)(Source), 0, 0;
        psq_l       POS23,  (SPOS + 8)(Source), 1, 0;

        // Load normal
        psq_l       NORM01, (SNRM + 0)(Source), 0, 0;	
        psq_l       NORM23, (SNRM + 8)(Source), 1, 0;
        ps_merge00  NORM23, NORM23, UTIL;

        // ----------------------------------------------------------
        // Index 0
        LOAD_INDIRECT_ADDR(0);
        LOAD4x3();
        LOAD_WEIGHT(0);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MUL(DSTPOS, ACCUM_P);    // First index does a mul into the dest
        WEIGHT_MUL(DSTNORM, ACCUM_N);

        // ----------------------------------------------------------
        // Index 1
        LOAD_INDIRECT_ADDR(1);
        LOAD4x3();
        LOAD_WEIGHT(1);
        BRANCH_0_WEIGHT(ExitVert);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MADD(DSTPOS, ACCUM_P);   // Subsequent loads madd in
        WEIGHT_MADD(DSTNORM, ACCUM_N);

        // ----------------------------------------------------------
        // Index 2
        LOAD_INDIRECT_ADDR(2);
        LOAD4x3();
        LOAD_WEIGHT(2);
        BRANCH_0_WEIGHT(ExitVert);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MADD(DSTPOS,  ACCUM_P);
        WEIGHT_MADD(DSTNORM, ACCUM_N);

        // ----------------------------------------------------------
        // Index 3
        LOAD_INDIRECT_ADDR(3);
        LOAD4x3();
        LOAD_WEIGHT(3);
        BRANCH_0_WEIGHT(ExitVert);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MADD(DSTPOS,  ACCUM_P);
        WEIGHT_MADD(DSTNORM, ACCUM_N);

ExitVert:
        psq_st   DSTPOS01,  (DPOS + 0)(Dest),  0, 0;
        psq_st   DSTPOS23,  (DPOS + 8)(Dest),  1, 0;
        psq_st   DSTNORM01, (DNRM + 0)(Dest),  0, 0;
        psq_st   DSTNORM23, (DNRM + 8)(Dest),  1, 0;
        psq_l    TEMP0, SUV(Source), 0, 0;
        psq_st   TEMP0, DUV(Dest),   0, 0;

        add Source, Source, SourceStride;
        add Dest,   Dest, DestStride;

        bdnz+ Loop;    // -------------------------- LOOP END
    }

#undef SPOS
#undef SWGHT
#undef SIDX
#undef SNRM
#undef SUV
#undef DPOS
#undef DNRM
#undef DUV

#undef POS01
#undef POS23
#undef NORM01
#undef NORM23
#undef UTIL
#undef DSTPOS01
#undef DSTPOS23
#undef DSTNORM01
#undef DSTNORM23
#undef MAT_0_01
#undef MAT_0_23
#undef MAT_1_01
#undef MAT_1_23
#undef MAT_2_01
#undef MAT_2_23
#undef TEMP0
#undef TEMP1
#undef TEMP2
#undef ACCUM_P01
#undef ACCUM_P23
#undef ACCUM_N01
#undef ACCUM_N23
#undef CURR_WEIGHT
}


static void
WiiDeformPWN323D(int32x register            Count,
                 void const*                SourceInit,
                 void*                      DestInit,
                 matrix_4x4 const register* Transforms,
                 int32x                     TailCopy32Count,
                 int32x register            SourceStride,
                 int32x register            DestStride)
{
    if (Count == 0)
        return;

#define SPOS  0
#define SWGHT 12
#define SIDX  14
#define SNRM  16
#define DPOS  0
#define DNRM  12

#define POS01         fp0
#define POS23         fp1
#define NORM01        fp2
#define NORM23        fp3
#define UTIL          fp4
#define DSTPOS01      fp5
#define DSTPOS23      fp6
#define DSTNORM01     fp7
#define DSTNORM23     fp8
#define MAT_0_01      fp9
#define MAT_0_23      fp10
#define MAT_1_01      fp11
#define MAT_1_23      fp12
#define MAT_2_01      fp13
#define MAT_2_23      fp14
#define TEMP0         fp15
#define TEMP1         fp16
#define TEMP2         fp17
#define ACCUM_P01     fp18
#define ACCUM_P23     fp19
#define ACCUM_N01     fp20
#define ACCUM_N23     fp21
#define CURR_WEIGHT   fp22
    CompileAssert(OffsetFromType(pwnt3232_vertex, Position)    == SPOS);
    CompileAssert(OffsetFromType(pwnt3232_vertex, BoneWeights) == SWGHT);
    CompileAssert(OffsetFromType(pwnt3232_vertex, BoneIndices) == SIDX);
    CompileAssert(OffsetFromType(pwnt3232_vertex, Normal)      == SNRM);
    CompileAssert(OffsetFromType(pnt332_vertex, Position) == DPOS);
    CompileAssert(OffsetFromType(pnt332_vertex, Normal)   == DNRM);

    void const register* Source  = SourceInit;
    void const register* Dest    = DestInit;
    real32 register*     Utility = &UtilityFloats[0];

    register uint32  Index;
    register uint32  Temp;

    asm
    {
        // Setup the quantization load register for unsigned 8 bit load, no scale (we need
        // 1/255, not 1/256)
        lis Temp, 0x0004;
        mtspr     GQR6, Temp

        // Load the utility register
        psq_l   UTIL,  0(Utility), 0, 0;

        // Loop for Count vertices
        mtctr Count;
Loop:
        // Load position
        psq_l       POS01,  (SPOS + 0)(Source), 0, 0;
        psq_l       POS23,  (SPOS + 8)(Source), 1, 0;

        // Load normal
        psq_l       NORM01, (SNRM + 0)(Source), 0, 0;	
        psq_l       NORM23, (SNRM + 8)(Source), 1, 0;
        ps_merge00  NORM23, NORM23, UTIL;

        // ----------------------------------------------------------
        // Index 0
        LOAD_DIRECT_ADDR(0);
        LOAD4x3();
        LOAD_WEIGHT(0);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MUL(DSTPOS, ACCUM_P);    // First index does a mul into the dest
        WEIGHT_MUL(DSTNORM, ACCUM_N);

        // ----------------------------------------------------------
        // Index 1
        LOAD_DIRECT_ADDR(1);
        LOAD4x3();
        LOAD_WEIGHT(1);
        BRANCH_0_WEIGHT(ExitVert);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MADD(DSTPOS, ACCUM_P);   // Subsequent loads madd in
        WEIGHT_MADD(DSTNORM, ACCUM_N);

ExitVert:
        psq_st   DSTPOS01,  (DPOS + 0)(Dest),  0, 0;
        psq_st   DSTPOS23,  (DPOS + 8)(Dest),  1, 0;
        psq_st   DSTNORM01, (DNRM + 0)(Dest),  0, 0;
        psq_st   DSTNORM23, (DNRM + 8)(Dest),  1, 0;

        add Source, Source, SourceStride;
        add Dest,   Dest, DestStride;

        bdnz+ Loop;    // -------------------------- LOOP END
    }

#undef SPOS
#undef SWGHT
#undef SIDX
#undef SNRM
#undef DPOS
#undef DNRM

#undef POS01
#undef POS23
#undef NORM01
#undef NORM23
#undef UTIL
#undef DSTPOS01
#undef DSTPOS23
#undef DSTNORM01
#undef DSTNORM23
#undef MAT_0_01
#undef MAT_0_23
#undef MAT_1_01
#undef MAT_1_23
#undef MAT_2_01
#undef MAT_2_23
#undef TEMP0
#undef TEMP1
#undef TEMP2
#undef ACCUM_P01
#undef ACCUM_P23
#undef ACCUM_N01
#undef ACCUM_N23
#undef CURR_WEIGHT
}


static void
WiiDeformPWN323I(int32x register            Count,
                 void const*                SourceInit,
                 void*                      DestInit,
                 int32x const register*     TransformTable,
                 matrix_4x4 const register* Transforms,
                 int32x                     TailCopy32Count,
                 int32x register            SourceStride,
                 int32x register            DestStride)
{
    if (Count == 0)
        return;

#define SPOS  0
#define SWGHT 12
#define SIDX  14
#define SNRM  16
#define DPOS  0
#define DNRM  12

#define POS01         fp0
#define POS23         fp1
#define NORM01        fp2
#define NORM23        fp3
#define UTIL          fp4
#define DSTPOS01      fp5
#define DSTPOS23      fp6
#define DSTNORM01     fp7
#define DSTNORM23     fp8
#define MAT_0_01      fp9
#define MAT_0_23      fp10
#define MAT_1_01      fp11
#define MAT_1_23      fp12
#define MAT_2_01      fp13
#define MAT_2_23      fp14
#define TEMP0         fp15
#define TEMP1         fp16
#define TEMP2         fp17
#define ACCUM_P01     fp18
#define ACCUM_P23     fp19
#define ACCUM_N01     fp20
#define ACCUM_N23     fp21
#define CURR_WEIGHT   fp22
    CompileAssert(OffsetFromType(pwnt3232_vertex, Position)    == SPOS);
    CompileAssert(OffsetFromType(pwnt3232_vertex, BoneWeights) == SWGHT);
    CompileAssert(OffsetFromType(pwnt3232_vertex, BoneIndices) == SIDX);
    CompileAssert(OffsetFromType(pwnt3232_vertex, Normal)      == SNRM);
    CompileAssert(OffsetFromType(pnt332_vertex, Position) == DPOS);
    CompileAssert(OffsetFromType(pnt332_vertex, Normal)   == DNRM);

    void const register* Source  = SourceInit;
    void const register* Dest    = DestInit;
    real32 register*     Utility = &UtilityFloats[0];

    register uint32  Index;
    register uint32  Temp;

    asm
    {
        // Setup the quantization load register for unsigned 8 bit load, no scale (we need
        // 1/255, not 1/256)
        lis Temp, 0x0004;
        mtspr     GQR6, Temp

        // Load the utility register
        psq_l   UTIL,  0(Utility), 0, 0;

        // Loop for Count vertices
        mtctr Count;
Loop:
        // Load position
        psq_l       POS01,  (SPOS + 0)(Source), 0, 0;
        psq_l       POS23,  (SPOS + 8)(Source), 1, 0;

        // Load normal
        psq_l       NORM01, (SNRM + 0)(Source), 0, 0;	
        psq_l       NORM23, (SNRM + 8)(Source), 1, 0;
        ps_merge00  NORM23, NORM23, UTIL;

        // ----------------------------------------------------------
        // Index 0
        LOAD_INDIRECT_ADDR(0);
        LOAD4x3();
        LOAD_WEIGHT(0);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MUL(DSTPOS, ACCUM_P);    // First index does a mul into the dest
        WEIGHT_MUL(DSTNORM, ACCUM_N);

        // ----------------------------------------------------------
        // Index 1
        LOAD_INDIRECT_ADDR(1);
        LOAD4x3();
        LOAD_WEIGHT(1);
        BRANCH_0_WEIGHT(ExitVert);
        MULVEC4x3(ACCUM_P, POS,  TEMP);
        MULVEC4x3(ACCUM_N, NORM, TEMP);
        WEIGHT_MADD(DSTPOS, ACCUM_P);   // Subsequent loads madd in
        WEIGHT_MADD(DSTNORM, ACCUM_N);

ExitVert:
        psq_st   DSTPOS01,  (DPOS + 0)(Dest),  0, 0;
        psq_st   DSTPOS23,  (DPOS + 8)(Dest),  1, 0;
        psq_st   DSTNORM01, (DNRM + 0)(Dest),  0, 0;
        psq_st   DSTNORM23, (DNRM + 8)(Dest),  1, 0;

        add Source, Source, SourceStride;
        add Dest,   Dest, DestStride;

        bdnz+ Loop;    // -------------------------- LOOP END
    }

#undef SPOS
#undef SWGHT
#undef SIDX
#undef SNRM
#undef DPOS
#undef DNRM

#undef POS01
#undef POS23
#undef NORM01
#undef NORM23
#undef UTIL
#undef DSTPOS01
#undef DSTPOS23
#undef DSTNORM01
#undef DSTNORM23
#undef MAT_0_01
#undef MAT_0_23
#undef MAT_1_01
#undef MAT_1_23
#undef MAT_2_01
#undef MAT_2_23
#undef TEMP0
#undef TEMP1
#undef TEMP2
#undef ACCUM_P01
#undef ACCUM_P23
#undef ACCUM_N01
#undef ACCUM_N23
#undef CURR_WEIGHT
}



void GRANNY
AddAcceleratedDeformers(void)
{
    bone_deformer WiiDeformers[] = {
        { DeformPositionNormal, WiiDeformPWNT3132D, WiiDeformPWNT3132I, NULL, PWNT3132VertexType, PNT332VertexType, false, false, true },
        { DeformPositionNormal, WiiDeformPWNT3232D, WiiDeformPWNT3232I, NULL, PWNT3232VertexType, PNT332VertexType, false, false, true },
        { DeformPositionNormal, WiiDeformPWNT3432D, WiiDeformPWNT3432I, NULL, PWNT3432VertexType, PNT332VertexType, false, false, true },
        { DeformPositionNormal, WiiDeformPWN323D,   WiiDeformPWN323I,   NULL, PWN323VertexType,   PN33VertexType,   false, false, true },
    };

    AddBoneDeformerTable(ArrayLength(WiiDeformers), WiiDeformers);
}
