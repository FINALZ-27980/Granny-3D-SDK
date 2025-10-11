#if !defined(GRANNY_FIXED_ALLOCATOR_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_fixed_allocator.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(MemoryAllocatorGroup);

struct fixed_allocator_unit
{
    fixed_allocator_unit *Next;
};

struct fixed_allocator_block
{
    int32x UnitCount;
    uint8 *Units;

    fixed_allocator_unit *FirstFreeUnit;

    fixed_allocator_block *Next;
    fixed_allocator_block *Previous;
};

struct fixed_allocator
{
    // You MUST set this in your static definition
    int32x UnitSize;

    // This is optional - if you don't set it, it will automatically
    // be set to an appropriate size
    int32x UnitsPerBlock;

    // These will all be initialized to zero by the static constructor
    fixed_allocator_block Sentinel;
};

void* AllocateFixed(fixed_allocator &Allocator);
void  DeallocateFixed(fixed_allocator &Allocator, void *Memory);
void  DeallocateAllFixed(fixed_allocator &Allocator);

// If you don't do it with the static initializer (like, you allocated
// one of these or something), you have to call this before using.
bool InitializeFixedAllocator(fixed_allocator &Allocator, int32x UnitSize);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_FIXED_ALLOCATOR_H
#endif
