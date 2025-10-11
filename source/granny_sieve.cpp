// ========================================================================
// $File: //jeffr/granny_29/rt/granny_sieve.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_sieve.h"

#include "granny_aggr_alloc.h"
#include "granny_memory.h"
#include "granny_stack_allocator.h"

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

BEGIN_GRANNY_NAMESPACE;

struct sieve
{
    int32x GroupCount;
    stack_allocator *Groups;
};

END_GRANNY_NAMESPACE;

sieve *GRANNY
NewSieve(int32x GroupCount, int32x UnitSize)
{
    sieve *Sieve;

    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    AggrAllocPtr(Allocator, Sieve);
    AggrAllocOffsetArrayPtr(Allocator, Sieve, GroupCount, GroupCount, Groups);
    if(EndAggrAlloc(Allocator, AllocationUnknown))
    {
        {for(int32x GroupIndex = 0;
             GroupIndex < GroupCount;
             ++GroupIndex)
        {
            StackInitialize(Sieve->Groups[GroupIndex], UnitSize, 1024);
        }}
    }

    return(Sieve);
}

int32x GRANNY
GetUsedGroupCount(sieve &Sieve)
{
    int32x Count = 0;

    {for(int32x GroupIndex = 0;
         GroupIndex < Sieve.GroupCount;
         ++GroupIndex)
    {
        if(GetStackUnitCount(Sieve.Groups[GroupIndex]) > 0)
        {
            ++Count;
        }
    }}

    return(Count);
}

void GRANNY
ClearSieve(sieve &Sieve)
{
    {for(int32x GroupIndex = 0;
         GroupIndex < Sieve.GroupCount;
         ++GroupIndex)
    {
        StackCleanUp(Sieve.Groups[GroupIndex]);
    }}
}

void GRANNY
FreeSieve(sieve *Sieve)
{
    if(Sieve)
    {
        ClearSieve(*Sieve);
        Deallocate(Sieve);
    }
}

void *GRANNY
AddSieveUnit(sieve &Sieve, int32x GroupIndex)
{
    Assert(GroupIndex < Sieve.GroupCount);

    int32x UnitIndex;
    if(NewStackUnit(Sieve.Groups[GroupIndex], &UnitIndex))
    {
        return(GetStackUnit(Sieve.Groups[GroupIndex], UnitIndex));
    }

    return(0);
}

int32x GRANNY
GetSieveGroupUnitCount(sieve &Sieve, int32x GroupIndex)
{
    Assert(GroupIndex < Sieve.GroupCount);

    return(GetStackUnitCount(Sieve.Groups[GroupIndex]));
}

void GRANNY
SerializeSieveGroup(sieve &Sieve, int32x GroupIndex, void *Dest)
{
    Assert(GroupIndex < Sieve.GroupCount);

    SerializeStack(Sieve.Groups[GroupIndex], Dest);
}
