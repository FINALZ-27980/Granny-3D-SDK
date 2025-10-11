// ========================================================================
// $File: //jeffr/granny_29/rt/granny_aggr_alloc.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_aggr_alloc.h"

#include "granny_assert.h"
#include "granny_memory.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
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

#define SubsystemCode MemoryLogMessage
USING_GRANNY_NAMESPACE;


void GRANNY
InitializeAggregateAllocation_(aggr_allocator *Allocator,
                               char const *File, int32x Line)
{
    DebugFillStructure(*Allocator);

    Allocator->AllocationAlignment     = DefaultAllocationAlignment;
    Allocator->TotalAggregateSize      = 0;
    Allocator->NextUnusedAggregate     = 0;
    Allocator->ZeroFill                = false;
}

void GRANNY
SetAggrAlignment(aggr_allocator &Allocator, uintaddrx Alignment)
{
    // Since alignment is unsigned, this probably means that someone tried to pass a
    // negative alignment
    CheckGreater0(Alignment, return);
    CheckNotSignCoerced(Alignment, return);

    Allocator.AllocationAlignment = Alignment;
}

void GRANNY
SetAggrZeroFill(aggr_allocator &Allocator)
{
    Allocator.ZeroFill = true;
}


static int32x
PushPointerAggregate(aggr_allocator &Allocator,
                     void *&ReturnPointer, uintaddrx Size)
{
    // Align to the current alignment
    Allocator.TotalAggregateSize = AlignN(Allocator.TotalAggregateSize,
                                          Allocator.AllocationAlignment);

    // Grab the next available aggregate record
    Assert(Allocator.NextUnusedAggregate < MaximumAggregateCount);
    int32x const AggregateIndex = Allocator.NextUnusedAggregate++;
    aggregate_allocation &Allocation = Allocator.Aggregates[AggregateIndex];
    ZeroStructure(Allocation);

    // Fill it out
    Allocation.UseRawOffset = false;
    Allocation.WriteToPointer = &ReturnPointer;
    Allocation.WriteToOffset = 0;
    Allocation.Offset = Allocator.TotalAggregateSize;
    Allocation.Count = -1;

    Allocator.TotalAggregateSize += Size;

    // Overwrite the pointer with our offset, in case someone
    // wants to daisy-chain offsets.
    *(uintaddrx *)&ReturnPointer = Allocation.Offset;

    // Return the index of the record we used
    return(AggregateIndex);
}

static int32x
PushOffsetAggregatePtr(aggr_allocator &Allocator,
                       uintaddrx Offset, uintaddrx Size)
{
    // Align to the current alignment
    Allocator.TotalAggregateSize = AlignN(Allocator.TotalAggregateSize,
                                          Allocator.AllocationAlignment);

    Assert(Offset < Allocator.TotalAggregateSize);
    Assert((Offset % 4) == 0);

    // Grab the next available aggregate record
    Assert(Allocator.NextUnusedAggregate < MaximumAggregateCount);
    int32x const AggregateIndex = Allocator.NextUnusedAggregate++;
    aggregate_allocation &Allocation = Allocator.Aggregates[AggregateIndex];
    ZeroStructure(Allocation);

    // Fill it out
    Allocation.WriteToPointer = 0;
    Allocation.UseRawOffset = false;
    Allocation.WriteToOffset = Offset;
    Allocation.Offset = Allocator.TotalAggregateSize;
    Allocation.Count = -1;

    Allocator.TotalAggregateSize += Size;

    // Return the index of the record we used
    return(AggregateIndex);
}

static int32x
PushOffsetAggregatePtr(aggr_allocator &Allocator,
                       uintaddrx CountOffset, uintaddrx PtrOffset,
                       int32x Count, uintaddrx Size)
{
    // Align to the current alignment
    Allocator.TotalAggregateSize = AlignN(Allocator.TotalAggregateSize,
                                          Allocator.AllocationAlignment);

    Assert(CountOffset < Allocator.TotalAggregateSize);
    Assert((CountOffset % 4) == 0);

    Assert(PtrOffset < Allocator.TotalAggregateSize);
    Assert((PtrOffset % 4) == 0);

    // Grab the next available aggregate record
    Assert(Allocator.NextUnusedAggregate < MaximumAggregateCount);
    int32x const AggregateIndex = Allocator.NextUnusedAggregate++;
    aggregate_allocation &Allocation = Allocator.Aggregates[AggregateIndex];
    ZeroStructure(Allocation);

    // Fill it out
    Allocation.WriteToPointer = 0;
    Allocation.UseRawOffset = false;
    Allocation.WriteToOffset = PtrOffset;
    Allocation.Offset = Allocator.TotalAggregateSize;
    Allocation.Count = Count;
    Allocation.CountOffset = CountOffset;

    Allocator.TotalAggregateSize += Size;

    // Return the index of the record we used
    return(AggregateIndex);
}

static int32x
PushOffsetAggregate(aggr_allocator &Allocator,
                    uintaddrx CountOffset, uintaddrx OffsetOffset,
                    int32x Count, uintaddrx Size)
{
    // Align to the current alignment
    Allocator.TotalAggregateSize = AlignN(Allocator.TotalAggregateSize,
                                          Allocator.AllocationAlignment);

    Assert(OffsetOffset < Allocator.TotalAggregateSize);
    Assert((OffsetOffset % 4) == 0);

    Assert(CountOffset < Allocator.TotalAggregateSize);
    Assert((CountOffset % 4) == 0);

    // Grab the next available aggregate record
    Assert(Allocator.NextUnusedAggregate < MaximumAggregateCount);
    int32x const AggregateIndex = Allocator.NextUnusedAggregate++;
    aggregate_allocation &Allocation = Allocator.Aggregates[AggregateIndex];
    ZeroStructure(Allocation);

    // Fill it out
    Allocation.WriteToPointer = 0;
    Allocation.UseRawOffset = true;
    Allocation.WriteToOffset = OffsetOffset;
    Allocation.Offset = Allocator.TotalAggregateSize;
    Allocation.Count = Count;
    Allocation.CountOffset = CountOffset;

    Allocator.TotalAggregateSize += Size;

    // Return the index of the record we used
    return(AggregateIndex);
}

uintaddrx GRANNY
AggregateAllocate_(aggr_allocator &Allocator,
                   void **ReturnPointer, uintaddrx Size)
{
    int32x const AggregateIndex = PushPointerAggregate(Allocator,
                                                       *ReturnPointer, Size);
    return (Allocator.Aggregates[AggregateIndex].Offset);
}

uintaddrx GRANNY
AggregateAllocate_(aggr_allocator &Allocator,
                   uintaddrx Offset, uintaddrx Size)
{
    int32x const AggregateIndex =
        PushOffsetAggregatePtr(Allocator, Offset, Size);

    return (Allocator.Aggregates[AggregateIndex].Offset);
}

uintaddrx GRANNY
AggregateAllocate_(aggr_allocator &Allocator,
                   void *OwnerPointer, uintaddrx Offset, uintaddrx Size)
{
    int32x const AggregateIndex =
        PushOffsetAggregatePtr(Allocator, (uintaddrx)OwnerPointer + Offset, Size);

    return (Allocator.Aggregates[AggregateIndex].Offset);
}

uintaddrx GRANNY
AggregateAllocate_(aggr_allocator &Allocator,
                   void *OwnerPointer, uintaddrx CountOffset,
                   uintaddrx PtrOffset, int32x Count, uintaddrx UnitSize)
{
    int32x const Index =
        PushOffsetAggregatePtr(Allocator,
                               (uintaddrx)OwnerPointer + CountOffset,
                               (uintaddrx)OwnerPointer + PtrOffset,
                               Count, Count * UnitSize);

    return (Allocator.Aggregates[Index].Offset);
}

uintaddrx GRANNY
AggregateAllocateOffset_(aggr_allocator &Allocator,
                         void *OwnerPointer, uintaddrx Size, uintaddrx Offset)
{
    int32x const Index =
        PushOffsetAggregate(Allocator,
                            0, (uintaddrx)OwnerPointer + Offset,
                            -1, Size);

    return (Allocator.Aggregates[Index].Offset);
}

uintaddrx GRANNY
AggregateAllocateOffset_(aggr_allocator &Allocator,
                         void *OwnerPointer,
                         int32x Count, uintaddrx UnitSize,
                         uintaddrx CountOffset, uintaddrx OffsetOffset)
{
    int32x const Index =
        PushOffsetAggregate(Allocator,
                            (uintaddrx)OwnerPointer + CountOffset,
                            (uintaddrx)OwnerPointer + OffsetOffset,
                            Count, Count*UnitSize);

    return (Allocator.Aggregates[Index].Offset);
}

static void
WriteMemoryPointers(aggr_allocator &Allocator, void *Memory)
{
    if(Memory)
    {
        // We got the memory, so fix up all the pointers
        uint8 * const BasePointer = (uint8 *)Memory;
        {for(int32x AggregateIndex = 0;
             AggregateIndex < Allocator.NextUnusedAggregate;
             ++AggregateIndex)
        {
            aggregate_allocation &Aggregate = Allocator.Aggregates[AggregateIndex];

            Assert((Aggregate.Offset % 4) == 0);
            void *Value = Aggregate.UseRawOffset ?
                (void *)(Aggregate.Offset) :
                &BasePointer[Aggregate.Offset];

            if(Aggregate.WriteToPointer)
            {
                // This aggregate is returned by pointer
                *Aggregate.WriteToPointer = Value;
            }
            else
            {
                // This aggregate is written into another part of the
                // allocation.
                Assert((Aggregate.WriteToOffset % 4) == 0);
                void **Write = (void **)&BasePointer[Aggregate.WriteToOffset];
                void *Address = Value;
                *Write = Address;

                if (Aggregate.Count != -1)
                {
                    Assert((Aggregate.CountOffset % 4) == 0);
                    int32x* WriteInt = (int32x*)&BasePointer[Aggregate.CountOffset];
                    *WriteInt = Aggregate.Count;

                    // This is counted, and zero?  NULL the pointer
                    if (Aggregate.Count == 0)
                    {
                        void** WritePtr = (void **)&BasePointer[Aggregate.WriteToOffset];
                        *WritePtr = 0;
                    }
                }
            }
        }}
    }
    else
    {
        // Set all pointers to 0 so there will be big problems if anyone
        // tries to use the memory we didn't get.

        {for(int32x AggregateIndex = 0;
             AggregateIndex < Allocator.NextUnusedAggregate;
             ++AggregateIndex)
        {
            aggregate_allocation &Aggregate = Allocator.Aggregates[AggregateIndex];
            if(Aggregate.WriteToPointer)
            {
                *Aggregate.WriteToPointer = 0;
            }
        }}
    }
}

void *GRANNY
EndAggregateAllocation_(aggr_allocator *Allocator,
                        char const *File, int32x Line,
                        int32x Intent)
{
    // Ensure the final size is a multiple of the specified alignment
    Allocator->TotalAggregateSize =
        AlignN(Allocator->TotalAggregateSize, Allocator->AllocationAlignment);

    void *Memory =
        CallAllocateCallback(File, Line,
                             Allocator->AllocationAlignment, Allocator->TotalAggregateSize,
                             Intent);

    if (Memory && Allocator->ZeroFill)
    {
        Zero(Allocator->TotalAggregateSize, Memory);
    }

    WriteMemoryPointers(*Allocator, Memory);
    DebugFillStructure(*Allocator);

    return Memory;
}

void* GRANNY
EndAggregateToArena_(aggr_allocator *Allocator,
                     memory_arena& Arena,
                     char const *File, int32x Line)
{
    // Ensure the final size is a multiple of the specified alignment
    Allocator->TotalAggregateSize =
        AlignN(Allocator->TotalAggregateSize, Allocator->AllocationAlignment);

    // Deal with alignment issues.  We potentially need to adjust the arena allocation if
    // the alignment requested for the aggregate is larger.
    Assert(GetArenaAlignment(Arena) != 0);

    uintaddrx OldAlignment = 0;
    if (Allocator->AllocationAlignment > GetArenaAlignment(Arena))
    {
        OldAlignment = GetArenaAlignment(Arena);
        Assert(OldAlignment > 0);

        SetArenaAlignment(Arena, Allocator->AllocationAlignment);
    }
    else
    {
        // Alignments should still be a multiple of each other...
        CheckCondition((GetArenaAlignment(Arena) % Allocator->AllocationAlignment) == 0, return 0);
    }

    void *Mem = ArenaPushSize(Arena, (int32x)Allocator->TotalAggregateSize);

    // ---
    // Don't zero here, it will be handled in EndAggregatePlacement_
    // ---

    if (OldAlignment != 0)
    {
        SetArenaAlignment(Arena, OldAlignment);
    }

    // Note that WriteMemoryPointers will handle allocation failure for us, and return
    // NULL from Placement.
    return EndAggregatePlacement_(Allocator, File, Line, Mem);
}


void* GRANNY
EndAggregatePlacement_(aggr_allocator *Allocator,
                       char const *File, int32x Line, void *Memory)
{
    if (Memory && Allocator->ZeroFill)
    {
        Zero(Allocator->TotalAggregateSize, Memory);
    }

    WriteMemoryPointers(*Allocator, Memory);
    DebugFillStructure(*Allocator);

    return Memory;
}

uintaddrx GRANNY
EndAggregateSize_(aggr_allocator *Allocator,
                  char const *File, int32x Line)
{
    // Ensure the final size is a multiple of the specified alignment
    Allocator->TotalAggregateSize =
        AlignN(Allocator->TotalAggregateSize, Allocator->AllocationAlignment);

    uintaddrx Result = Allocator->TotalAggregateSize;
    ZeroStructure(*Allocator);

    return Result;
}

