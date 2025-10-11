#if !defined(X86_GRANNY_MATRIX_OPERATIONS_OPS_INL)
// ========================================================================
// $File: //jeffr/granny/rt/x86/x86_granny_cpu_queries.h $
// $DateTime: 2006/10/16 14:57:23 $
// $Change: 13583 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

// Registers are set for all of the matrix ops in the same fashion
#define A  esi
#define B  edx
#define AB edi

#define T0 xmm0
#define T1 xmm1
#define T2 xmm2
#define T3 xmm3

#define BROW0 xmm4
#define BROW1 xmm5
#define BROW2 xmm6
#define BROW3 xmm7

// The setup operations do basically:
//
//      LOAD_OP  BROW0,  [ B +  0 ];
//      LOAD_OP  BROW1,  [ B + 16 ];
//      LOAD_OP  BROW2,  [ B + 32 ];
//      LOAD_OP  BROW3,  [ B + 48 ]
//
// Where LOAD_OP is either movaps or movups, depending on whether or not B is
// aligned.  We assume that A/B/AB have all been setup at this point.
#define UnalignedSetup()                        \
    __asm movups  BROW0,  [ B +  0 ]            \
        __asm movups  BROW1,  [ B + 16 ]        \
        __asm movups  BROW2,  [ B + 32 ]        \
        __asm movups  BROW3,  [ B + 48 ]


#define AlignedSetup()                          \
    __asm movaps  BROW0,  [ B +  0 ]            \
        __asm movaps  BROW1,  [ B + 16 ]        \
        __asm movaps  BROW2,  [ B + 32 ]        \
        __asm movaps  BROW3,  [ B + 48 ]


#define AdvancePointers4x4()                    \
    __asm add A,  16                            \
        __asm add AB, 16


#define RowMultiply4x3()                        \
    __asm movss    T0,  [ A + 0 ]               \
        __asm movss    T1,  [ A + 4 ]           \
        __asm movss    T2,  [ A + 8 ]           \
        __asm shufps   T0, T0, 0                \
        __asm shufps   T1, T1, 0                \
        __asm shufps   T2, T2, 0                \
        __asm mulps    T0, BROW0                \
        __asm mulps    T1, BROW1                \
        __asm addps    T0, T1                   \
        __asm mulps    T2, BROW2                \
        __asm addps    T0, T2


#define RowMultiply4x3_With1()                  \
    __asm movss    T0,  [ A + 0 ]               \
        __asm movss    T1,  [ A + 4 ]           \
        __asm movss    T2,  [ A + 8 ]           \
        __asm shufps   T0, T0, 0                \
        __asm shufps   T1, T1, 0                \
        __asm shufps   T2, T2, 0                \
        __asm mulps    T0, BROW0                \
        __asm mulps    T1, BROW1                \
        __asm addps    T0, T1                   \
        __asm mulps    T2, BROW2                \
        __asm addps    T0, T2                   \
        __asm addps    T0, BROW3


#define RowMultiply4x4()                        \
    __asm movss    T0,  [ A +  0 ]              \
        __asm movss    T1,  [ A +  4 ]          \
        __asm movss    T2,  [ A +  8 ]          \
        __asm movss    T3,  [ A + 12 ]          \
        __asm shufps   T0, T0, 0                \
        __asm shufps   T1, T1, 0                \
        __asm shufps   T2, T2, 0                \
        __asm shufps   T3, T3, 0                \
        __asm mulps    T0, BROW0                \
        __asm mulps    T1, BROW1                \
        __asm addps    T0, T1                   \
        __asm mulps    T2, BROW2                \
        __asm addps    T0, T2                   \
        __asm mulps    T3, BROW3                \
        __asm addps    T0, T3


#define StoreRowUnaligned() __asm movups [ AB ], T0
#define StoreRowAligned()   __asm movaps [ AB ], T0


#define X86_GRANNY_MATRIX_OPERATIONS_OPS_INL
#endif
