// ========================================================================
// $File: //jeffr/granny_29/rt/granny_string_table.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_string_table.h"

#include "granny_aggr_alloc.h"
#include "granny_limits.h"
#include "granny_memory.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"
#include "granny_string.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#undef SubsystemCode
#define SubsystemCode StringTableLogMessage

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

struct string_tree_entry
{
    char* String;
    string_tree_entry* Left;
    string_tree_entry* Right;
};

#define CONTAINER_NAME string_tree
#define CONTAINER_ITEM_TYPE string_tree_entry
#define CONTAINER_ADD_FIELDS char *String
#define CONTAINER_ADD_ASSIGN(Item) (Item)->String = String;
#define CONTAINER_COMPARE_ITEMS(Item1, Item2) StringDifference((Item1)->String, (Item2)->String)
#define CONTAINER_FIND_FIELDS char const *String
#define CONTAINER_COMPARE_FIND_FIELDS(Item) StringDifference(String, (Item)->String)
#define CONTAINER_SORTED 1
#define CONTAINER_KEEP_LINKED_LIST 0
#define CONTAINER_FUNCTION_DECORATE(return_type) return_type
#define CONTAINER_SUPPORT_DUPES 0
#define CONTAINER_LEFT_NAME  Left
#define CONTAINER_RIGHT_NAME Right
#define CONTAINER_ASSERT Assert
#define CONTAINER_USE_OVERLOADING 1
#define CONTAINER_EMIT_CODE 1
#define CONTAINER_MALLOC(Size) AllocateSize(Size, AllocationUnknown)
#define CONTAINER_FREE(Pointer) Deallocate(Pointer)
#include "contain.inl"

struct string_table_block
{
    char* DataStart;
    char* OnePastLastData;
    string_table_block* Previous;
};

struct string_table
{
    string_tree Tree;

    int32x BlockSize;
    string_table_block* LastBlock;

    memory_arena* Arena;
};

END_GRANNY_NAMESPACE;

string_table* GRANNY
NewArenaStringTable(memory_arena* Arena)
{
    string_table *Table = Allocate(string_table, AllocationUnknown);
    if (Table)
    {
        Initialize(&Table->Tree, 0);
        Table->BlockSize = ExpectedUsablePageSize;
        Table->LastBlock = 0;

        // Note that arena may be null if called from default version
        Table->Arena = Arena;
    }

    return(Table);
}

string_table *GRANNY
NewStringTable()
{
    return NewArenaStringTable((memory_arena*)0);
}

void GRANNY
FreeStringTable(string_table *Table)
{
    if (Table)
    {
        // If no arena, we need to free these, otherwise they hang around until the arena
        // is released.
        if (Table->Arena == 0)
        {
            while(Table->LastBlock)
            {
                string_table_block *FreeBlock = Table->LastBlock;
                if(FreeBlock)
                {
                    Table->LastBlock = FreeBlock->Previous;
                    Deallocate(FreeBlock);
                }
            }
        }
        else
        {
            // Burn it, it's the only way to be sure
            Table->Arena = 0;
        }

        FreeMemory(&Table->Tree);
        Deallocate(Table);
    }
}

static string_table_block *
AllocateStringBlock(string_table& Table, int32x Size)
{
    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    string_table_block *Block = 0;
    AggrAllocPtr(Allocator, Block);
    AggrAllocOffsetSizePtr(Allocator, Block, Size, DataStart);

    if (Table.Arena != 0)
    {
        EndAggrToArena(Allocator, *Table.Arena);
    }
    else
    {
        EndAggrAlloc(Allocator, AllocationUnknown);
    }

    if (Block)
    {
        Block->OnePastLastData = Block->DataStart;
        Block->Previous = 0;
    }

    return (Block);
}

char*
PushLengthString(string_table &Table, int32x LengthWithoutNullTerminator, char const *String)
{
    char *Result = 0;
    string_table_block *Block = 0;

    if(!Table.BlockSize)
    {
        // This stack was never initialized
        // TODO: Platform-specific page size determination
        Table.BlockSize = 4000;
        Table.LastBlock = 0;
    }

    int32x Length = LengthWithoutNullTerminator + 1;
    if(Length > Table.BlockSize)
    {
        // This string is too big to fit in a block, so we allocate a block specifically for it
        Block = AllocateStringBlock(Table, Length);
        if(Block)
        {
            if(Table.LastBlock)
            {
                // Since this block can't have anything else in it, insert it behind the
                // current last block so any empty space in that block can still be used
                Block->Previous = Table.LastBlock->Previous;
                Table.LastBlock->Previous = Block;
            }
            else
            {
                // There are no other blocks, so there's no harm in just shoving this one
                // in the front.
                Block->Previous = 0;
                Table.LastBlock = Block;
            }
        }
    }
    else
    {
        // See if there is space in the current block.
        int32x SpaceLeft = 0;
        if(Table.LastBlock)
        {
            const intaddrx WideSpaceLeft = (Table.BlockSize -
                                           (Table.LastBlock->OnePastLastData - Table.LastBlock->DataStart));
            CheckConvertToInt32(SpaceLeft, WideSpaceLeft, return 0);
        }

        if(Length > SpaceLeft)
        {
            // We don't have room, so make a new standard block
            Block = AllocateStringBlock(Table, Table.BlockSize);
            if(Block)
            {
                Block->Previous = Table.LastBlock;
                Table.LastBlock = Block;
            }
        }
        else
        {
            Assert(Table.LastBlock);
            Block = Table.LastBlock;
        }
    }

    if(Block)
    {
        Result = Block->OnePastLastData;
        if(String)
        {
            Copy(LengthWithoutNullTerminator, String, Block->OnePastLastData);
        }

        // Note that we don't COPY the null terminator, we manually add it, because we
        // may have been called by something that doesn't have a null terminator on
        // the end.
        Block->OnePastLastData[LengthWithoutNullTerminator] = '\0';
        Block->OnePastLastData += Length;
    }

    return(Result);
}

char *
PushBoundedString(string_table &Table, char const *FirstChar, char const *OnePastLastChar)
{
    int32 StringLen;
    CheckConvertToInt32(StringLen, OnePastLastChar - FirstChar, return 0);
    return(PushLengthString(Table, StringLen, FirstChar));
}

char *
PushZString(string_table &Table, char const *String)
{
    return(PushLengthString(Table, StringLength(String), String));
}

char *
PushSize(string_table &Table, int32x Size)
{
    return(PushLengthString(Table, Size, 0));
}

char const *GRANNY
MapString(string_table &Table, char const *String)
{
    string_tree_entry *Entry = Find(&Table.Tree, String);
    if(Entry)
    {
        return(Entry->String);
    }
    else
    {
        char *PushedString = PushZString(Table, String);
        Add(&Table.Tree, PushedString);
        return(PushedString);
    }
}
