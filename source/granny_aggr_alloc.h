#if !defined(GRANNY_AGGR_ALLOC_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_aggr_alloc.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

// Nice big number for this, 64 is pretty large in the normal way AggrAlloc is used
#define MaximumAggregateCount (1 << 6)


struct memory_arena;

struct aggregate_allocation
{
    bool32 UseRawOffset;     // If true, write offsets instead of pointers
    void **WriteToPointer; // If non-zero, write the pointer back here
    uintaddrx WriteToOffset; // If WriteToPointer was zero, write to this offset
    uintaddrx Offset;        // The offset of the allocation inside the block

    int32x Count;           // If count is not -1, write the count to this
    uintaddrx CountOffset;   // offset

    uintaddrx Alignment;     // Alignment for this sub-allocation
};

struct aggr_allocator
{
    uintaddrx AllocationAlignment;
    uintaddrx TotalAggregateSize;
    int32x    NextUnusedAggregate;
    bool32    ZeroFill;
    aggregate_allocation Aggregates[MaximumAggregateCount];
};


// Call this to initialize an aggregate allocator.  Note that the structure
//  returned is rendered useless by EndAggr*
// Use as:
//  aggr_allocator Allocator;
//  InitializeAggrAlloc(Allocator);
#define InitializeAggrAlloc(Allocator) InitializeAggregateAllocation_(&Allocator, __FILE__, __LINE__)

// The aggregation functions are useful for grouping small allocations into
// a large whole, or even just to ensure that several large allocations
// all occur successfully.  Call AggrAlloc*() as many times as you like,
// then finish it all with a call to EndAggrAlloc().  The memory will
// only be valid after EndAggrAlloc(), so don't try to use the memory
// in between AggrAlloc() and EndAggrAlloc().
#define AggrAllocPtr(Allocator, ReturnPointer)                                          \
    AggregateAllocate_(Allocator, (void **)&(ReturnPointer), SizeOf(*(ReturnPointer)))

#define AggrAllocArrayPtr(Allocator, Count, ReturnPointer)                                          \
    AggregateAllocate_(Allocator, (void **)&(ReturnPointer), (Count) * SizeOf(*(ReturnPointer)))

#define AggrAllocSizePtr(Allocator, Size, ReturnPointer)            \
    AggregateAllocate_(Allocator, (void **)&(ReturnPointer), Size)

#define AggrAllocOffset(Allocator, OwnerPointer, type, Member)      \
    AggregateAllocateOffset_(Allocator, OwnerPointer, SizeOf(type), \
                             OffsetFromPtr(OwnerPointer, Member))

#define AggrAllocOffsetSize(Allocator, OwnerPointer, Size, Member)  \
    AggregateAllocateOffset_(Allocator, OwnerPointer, Size,         \
                             OffsetFromPtr(OwnerPointer, Member))

#define AggrAllocOffsetPtr(Allocator, OwnerPointer, Member)                                         \
    AggregateAllocate_(Allocator, OwnerPointer, OffsetFromPtr(OwnerPointer, Member), SizeOf(*OwnerPointer->Member))

#define AggrAllocOffsetArrayPtr(Allocator, OwnerPointer, Count, CountMember, PtrMember)     \
    AggregateAllocate_(Allocator, OwnerPointer, OffsetFromPtr(OwnerPointer, CountMember),   \
                       OffsetFromPtr(OwnerPointer, PtrMember),                              \
                       Count, SizeOf(*OwnerPointer->PtrMember))

#define AggrAllocOffsetArraySizePtr(Allocator, OwnerPointer, Count, Size, CountMember, PtrMember)   \
    AggregateAllocate_(Allocator, OwnerPointer, OffsetFromPtr(OwnerPointer, CountMember),           \
                       OffsetFromPtr(OwnerPointer, PtrMember), Count, Size)

#define AggrAllocOffsetSizePtr(Allocator, OwnerPointer, Size, Member)                       \
    AggregateAllocate_(Allocator, OwnerPointer, OffsetFromPtr(OwnerPointer, Member), Size)

#define AggrAllocOffsetCountlessArrayPtr(Allocator, OwnerPointer, Count, Member)        \
    AggregateAllocate_(Allocator, OwnerPointer, OffsetFromPtr(OwnerPointer, Member),    \
                       ((uintaddrx)(Count)) * ((uintaddrx)SizeOf(*OwnerPointer->Member)))

#define SubAggrAllocPtr(Allocator, OwnerOffset, Type, Member)                                       \
    AggregateAllocate_(Allocator, OwnerOffset + (uint8)(&((Type *)0)->Member), SizeOf(((Type *)0)->Member))
#define SubAggrAllocArrayPtr(Allocator, OwnerOffset, Type, Count, Member)                           \
    AggregateAllocate_(Allocator, OwnerOffset + (uint8)(&((Type *)0)->Member), SizeOf(*((Type *)0)->Member) * (Count))
#define SubAggrAllocSizePtr(Allocator, OwnerOffset, Type, Size, Member)                 \
    AggregateAllocate_(Allocator, OwnerOffset + (uint8)(&((Type *)0)->Member), Size)

// Note that EndAggrAlloc() will return the correct pointer to pass
// to Deallocate(), which is always the first one that was AggrAlloc'd.
// However, if you don't want to care about which one that was, just
// keep an extra pointer around and store the result of EndAggrAlloc,
// then pass it to Deallocate when you're done.
#define EndAggrAlloc(Allocator, Intent) EndAggregateAllocation_(&Allocator, __FILE__, __LINE__, Intent)

#define EndAggrToArena(Allocator, Arena) EndAggregateToArena_(&Allocator, Arena, __FILE__, __LINE__)

// EndAggrPlacement() does just what EndAggrAlloc() does, only it expects
// that you've already allocated the memory.
#define EndAggrPlacement(Allocator, Memory) EndAggregatePlacement_(&Allocator, __FILE__, __LINE__, Memory)

// EndAggrSize() doesn't do any allocation or placing, it just lets
// you know how much you used.
#define EndAggrSize(Allocator) EndAggregateSize_(&Allocator, __FILE__, __LINE__)

// These calls calibrate the way the aggregate allocator works
void SetAggrAlignment(aggr_allocator &Allocator, uintaddrx Alignment);
void SetAggrZeroFill(aggr_allocator &Allocator);



// The following functions should generally not be called directly,
// but rather through their macros defined above.
uintaddrx AggregateAllocate_(aggr_allocator &Allocator,
                             void **ReturnPointer, uintaddrx Size);

uintaddrx AggregateAllocate_(aggr_allocator &Allocator,
                             uintaddrx Offset, uintaddrx Size);

uintaddrx AggregateAllocate_(aggr_allocator &Allocator,
                             void *OwnerPointer,
                             uintaddrx Offset, uintaddrx Size);

uintaddrx AggregateAllocate_(aggr_allocator &Allocator,
                             void *OwnerPointer, uintaddrx CountOffset,
                             uintaddrx PtrOffset, int32x Count, uintaddrx Size);

uintaddrx AggregateAllocateOffset_(aggr_allocator &Allocator,
                                   void *OwnerPointer, uintaddrx Size,
                                   uintaddrx Offset);

uintaddrx AggregateAllocateOffset_(aggr_allocator &Allocator,
                                   void *OwnerPointer,
                                   int32x Count, uintaddrx UnitSize,
                                   uintaddrx CountOffset,
                                   uintaddrx OffsetOffset);

void InitializeAggregateAllocation_(aggr_allocator *Allocator,
                                    char const *File, int32 Line);

void *EndAggregateAllocation_(aggr_allocator *Allocator,
                              char const *File, int32x Line,
                              int32x Intent);
void* EndAggregateToArena_(aggr_allocator *Allocator,
                           memory_arena& Arena,
                           char const *File, int32x Line);
void *EndAggregatePlacement_(aggr_allocator *Allocator,
                             char const *File, int32x Line,
                             void *Memory);
uintaddrx EndAggregateSize_(aggr_allocator *Allocator,
                            char const *File, int32x Line);


END_GRANNY_NAMESPACE;


#include "header_postfix.h"
#define GRANNY_AGGR_ALLOC_H
#endif
