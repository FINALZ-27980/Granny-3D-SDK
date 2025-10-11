// ========================================================================
// $File: //jeffr/granny_29/rt/granny_stack_allocator.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_stack_allocator.h"

#include "granny_aggr_alloc.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode StackAllocatorLogMessage

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

struct allocated_block
{
    int32x UsedUnitCount;
    uint8 *Base;

    int32x FirstIndex;

    allocated_block *Previous;
};

END_GRANNY_NAMESPACE;

static allocated_block *
AllocateBlock(stack_allocator &Allocator)
{
    int32x BlockSize = Allocator.UnitSize * Allocator.UnitsPerBlock;

    aggr_allocator AggrAllocator;
    InitializeAggrAlloc(AggrAllocator);

    allocated_block *Block;
    AggrAllocPtr(AggrAllocator, Block);
    AggrAllocOffsetSizePtr(AggrAllocator, Block, BlockSize, Base);
    if(EndAggrAlloc(AggrAllocator, AllocationLongTerm))
    {
        Block->UsedUnitCount = 0;
        Block->FirstIndex = 0;
        Block->Previous = 0;
    }

    return(Block);
}

bool GRANNY
StackInitialize(stack_allocator &Allocator, int32x UnitSize, int32x UnitsPerBlock)
{
    CheckGreater0(UnitSize, return false);
    CheckGreater0(UnitsPerBlock, return false);

    Allocator.UnitSize = UnitSize;
    Allocator.UnitsPerBlock = UnitsPerBlock;
    Allocator.TotalUsedUnitCount = 0;
    Allocator.LastBlock = 0;
    Allocator.MaxUnits = -1;
    Allocator.ActiveBlocks = -1;
    Allocator.MaxActiveBlocks = -1;
    Allocator.BlockDirectory = 0;

    return true;
}

bool GRANNY
StackInitializeWithDirectory(stack_allocator &Allocator, int32x UnitSize, int32x UnitsPerBlock, int32x MaxUnits)
{
    ZeroStructure(Allocator);

    CheckGreater0(UnitSize,      return false);
    CheckGreater0(UnitsPerBlock, return false);
    CheckGreaterEqual0(MaxUnits, return false);

    Allocator.UnitSize = UnitSize;
    Allocator.UnitsPerBlock = UnitsPerBlock;
    Allocator.TotalUsedUnitCount = 0;
    Allocator.LastBlock = 0;

    Allocator.MaxUnits = MaxUnits;
    Allocator.ActiveBlocks = 0;
    Allocator.MaxActiveBlocks = (MaxUnits + (UnitsPerBlock-1)) / UnitsPerBlock;;
    Allocator.BlockDirectory = AllocateArray(Allocator.MaxActiveBlocks, allocated_block*, AllocationTemporary);
    ZeroArray(Allocator.MaxActiveBlocks, Allocator.BlockDirectory);

    return true;
}

void GRANNY
StackCleanUp(stack_allocator &Allocator)
{
    PopStackUnits(Allocator, Allocator.TotalUsedUnitCount);
    Assert(Allocator.TotalUsedUnitCount == 0);
    Assert(Allocator.LastBlock == 0);

    if (Allocator.BlockDirectory)
    {
        Deallocate(Allocator.BlockDirectory);
        Allocator.BlockDirectory = 0;
    }
}

int32x GRANNY
GetStackUnitCount(stack_allocator const &Allocator)
{
    return(Allocator.TotalUsedUnitCount);
}

void* GRANNY
NewStackUnit(stack_allocator &Allocator, int32x* ResultIndex)
{
    allocated_block* InBlock = Allocator.LastBlock;

    int32x DummyIndex;
    if (!ResultIndex)
        ResultIndex = &DummyIndex;
    *ResultIndex = -1;

    if (!InBlock || (InBlock->UsedUnitCount == Allocator.UnitsPerBlock))
    {
        if (Allocator.BlockDirectory == 0 ||
            (Allocator.ActiveBlocks < Allocator.MaxActiveBlocks))
        {
            InBlock = AllocateBlock(Allocator);
        }
        else
        {
            InBlock = 0;
        }

        if (InBlock)
        {
            InBlock->FirstIndex = Allocator.TotalUsedUnitCount;
            InBlock->Previous = Allocator.LastBlock;
            Allocator.LastBlock = InBlock;

            if (Allocator.BlockDirectory != 0)
            {
                Allocator.BlockDirectory[Allocator.ActiveBlocks++] = InBlock;
            }
        }
    }

    void* NewItem = 0;
    if (InBlock)
    {
        NewItem = InBlock->Base + (InBlock->UsedUnitCount * Allocator.UnitSize);
        ++InBlock->UsedUnitCount;
        *ResultIndex = Allocator.TotalUsedUnitCount++;
    }

    return NewItem;
}

    // allocated_block *InBlock = Allocator.LastBlock;

    // // Ensure that the result points /somewhere/
    // int32x DummyIndex;
    // if (!Result)
    //     Result = &DummyIndex;
    // *Result = -1;

    // if(!InBlock || (InBlock->UsedUnitCount == Allocator.UnitsPerBlock))
    // {
    //     if (Allocator.BlockDirectory == 0 ||
    //         (Allocator.ActiveBlocks < Allocator.MaxActiveBlocks))
    //     {
    //         InBlock = AllocateBlock(Allocator);
    //     }

    //     if(InBlock)
    //     {
    //         InBlock->FirstIndex = Allocator.TotalUsedUnitCount;
    //         InBlock->Previous = Allocator.LastBlock;
    //         Allocator.LastBlock = InBlock;

    //         if (Allocator.BlockDirectory != 0)
    //         {
    //             Allocator.BlockDirectory[Allocator.ActiveBlocks++] = InBlock;
    //         }
    //     }
    // }

    // void* NewItem = NULL;
    // if(InBlock)
    // {
    //     NewItem = InBlock->Base + (InBlock->UsedUnitCount * Allocator.UnitSize);
    //     ++InBlock->UsedUnitCount;
    //     *Result = Allocator.TotalUsedUnitCount++;
    // }

    // return NewItem;

bool GRANNY
MultipleNewStackUnits(stack_allocator &Allocator,
                      int32x NumNewIndices,
                      int32x &NewIndicesStart,
                      void const* InitialValue)
{
    // Todo: work in blocks...
    // Very simple implementation for now

    if (NumNewIndices <= 0)
    {
        NewIndicesStart = -1;
        return false;
    }

    // First index is special
    int32x CurrentIndex;
    if (!NewStackUnit(Allocator, &CurrentIndex))
    {
        return false;
    }

    if (InitialValue != 0)
    {
        Copy(Allocator.UnitSize, InitialValue, GetStackUnit(Allocator, CurrentIndex));
    }
    NewIndicesStart = CurrentIndex;
    NumNewIndices--;

    while (NumNewIndices--)
    {
        void* Item = NewStackUnit(Allocator, &CurrentIndex);
        if (!Item)
            return false;

        if (InitialValue != 0)
            Copy(Allocator.UnitSize, InitialValue, Item);
    }

    return true;
}


void *GRANNY
GetStackUnit(stack_allocator &Allocator, int32x Index)
{
    Assert(Index >= 0);
    Assert(Index < Allocator.TotalUsedUnitCount);

    allocated_block *Block;
    if (Allocator.BlockDirectory == 0)
    {
        Block = Allocator.LastBlock;
        while(Block->FirstIndex > Index)
        {
            Block = Block->Previous;
        }

    }
    else
    {
        int32x DirectoryIndex = (Index / Allocator.UnitsPerBlock);
        Assert(DirectoryIndex < Allocator.ActiveBlocks);

        Block = Allocator.BlockDirectory[DirectoryIndex];
        Assert(Block->FirstIndex <= Index);
    }

    Index -= Block->FirstIndex;
    return(Block->Base + (Index * Allocator.UnitSize));
}

void GRANNY
PopStackUnits(stack_allocator &Allocator, int32x UnitCount)
{
    Assert(UnitCount <= Allocator.TotalUsedUnitCount);

    while(Allocator.LastBlock &&
          (Allocator.LastBlock->UsedUnitCount <= UnitCount))
    {
        Allocator.TotalUsedUnitCount -= Allocator.LastBlock->UsedUnitCount;
        UnitCount -= Allocator.LastBlock->UsedUnitCount;

        allocated_block *DeleteBlock = Allocator.LastBlock;
        Allocator.LastBlock = Allocator.LastBlock->Previous;
        Deallocate(DeleteBlock);

        if (Allocator.BlockDirectory != 0)
        {
            Assert(Allocator.ActiveBlocks > 0);
            Allocator.BlockDirectory[Allocator.ActiveBlocks - 1] = 0;
            Allocator.ActiveBlocks--;
        }
    }

    if(Allocator.LastBlock)
    {
        Assert(Allocator.LastBlock->UsedUnitCount > UnitCount);
        Allocator.TotalUsedUnitCount -= UnitCount;
        Allocator.LastBlock->UsedUnitCount -= UnitCount;
        UnitCount = 0;
    }
    else
    {
        Assert(UnitCount == 0);
    }

    Assert(UnitCount == 0);
}

void GRANNY
SerializeStack(stack_allocator const &Allocator, void *DestInit)
{
    uint8 *Dest = (uint8 *)DestInit;
    Dest += GetStackUnitCount(Allocator) * Allocator.UnitSize;
    {for(allocated_block *Block = Allocator.LastBlock;
         Block;
         Block = Block->Previous)
    {
        int32x const BlockSize = (Block->UsedUnitCount * Allocator.UnitSize);
        Dest -= BlockSize;
        Copy(BlockSize, Block->Base, Dest);
    }}

    Assert(Dest == DestInit);
}
