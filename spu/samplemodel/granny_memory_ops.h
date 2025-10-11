#if !defined(GRANNY_MEMORY_OPS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_memory_ops.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GRANNY_INTRINSICS_H)
#include "granny_intrinsics.h"
#endif

BEGIN_GRANNY_NAMESPACE;

// These are optional replacements for memset, memcmp, etc.
void SetInt32(uintaddrx Count, int32 Value,  void* Buffer);
void SetInt32x(uintaddrx Count, int32x Value,  void* Buffer);
void SetUInt32(uintaddrx Count, uint32 Value, void* Buffer);
void SetReal32(uintaddrx Count, real32 Value, void* Buffer);
void SetPtrNULL(uintaddrx Count, void* Buffer);

bool Compare(uintaddrx Count, void const *Buffer0, void const *Buffer1);

#define SetUInt8(sz, val, buf) IntrinsicMemset((buf), (val), (sz))
#define Copy(sz, From, To)     IntrinsicMemcpy ((To), (From), (sz))
#define Copy32(sz, From, To)   IntrinsicMemcpy((To), (From), (sz)*4)

void* Move(int32x Size, void const* From, void* To);

inline uintaddrx Align32(uintaddrx Size)
{
    return ((Size + 3) & ~3);
}

// Handy for computing arbitrary alignments.  Mod based rather than masked, so probably
// not an inner loop thing, thanks.
uintaddrx AlignN(uintaddrx Value, uintaddrx Alignment);
void*     AlignPtrN(void* Value, uintaddrx Alignment);

// The Reverse() functions do byte-swapping such that files from little endian machines
// can be read on big endian machines and vice versa.  Provided in both call-by-value
// utilities, plus as bulk operations
void Reverse64(uintaddrx BufferSize, void *BufferInit);
void Reverse32(uintaddrx BufferSize, void *BufferInit);
void Reverse16(uintaddrx BufferSize, void *BufferInit);

uint64 Reverse64(uint64 Value);
uint32 Reverse32(uint32 Value);
uint32 Reverse16_32(uint32 Value);
uint16 Reverse16_16(uint16 Value);


// Handy macros for working with random types...
#define Zero(Count, Ptr)         IntrinsicMemset((Ptr), 0, (Count))
#define ZeroArray(Count, Array)  IntrinsicMemset((Array), 0, ((Count) * SizeOf((Array)[0])))
#define ZeroStructure(Structure) IntrinsicMemset(&(Structure), 0, SizeOf(Structure))

#define CopyArray(Count, Source, Dest) Copy((Count) * SizeOf(*(Source)), Source, Dest)


#ifdef DEBUG
#define DebugFillStructure(Structure) SetUInt8(SizeOf(Structure), 0xBF, &(Structure))
#else
#define DebugFillStructure(Structure) ((void)0)
#endif


// Use this macro instead of straight pointer arithmetic when you're
// doing undefined comparisons between pointers from different
// allocations ranges, and only need the sign of the result to be
// correct.  A bug has been observed when compiling with the MacOSX
// version of gcc in which the computation used for the difference
// yields incorrect results when the pointers are offset by a number
// of bytes not a multiple of the structure size.  This is just
// faster, anyways, if all you need is the sign, the implied divide is
// unnecessary.
#define PtrDiffSignOnly(PtrA, PtrB) \
    ((((uintaddrx)(PtrA)) > ((uintaddrx)(PtrB))) ? 1 : ((((uintaddrx)(PtrB)) > ((uintaddrx)(PtrA))) ? -1 : 0))

// Evaluates to true if the two ranges overlap.  Handy for enforcing
// NOALIAS restrictions.  Note that the start and end follow STL
// conventions for this macro.
#define PtrRangesOverlap(AStart, AEnd, BStart, BEnd)    \
    (!(PtrDiffSignOnly(AEnd, BStart) <= 0 || PtrDiffSignOnly(AStart, BEnd) >= 0))

#define StructuresOverlap(A, B) PtrRangesOverlap(&(A), &(A) + 1, &(B), &(B) + 1)

// Handy macro
#define SetByValueNotNull(ptr, value) \
    do { if (ptr) { *(ptr) = value; } } while (false)

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MEMORY_OPS_H
#endif
