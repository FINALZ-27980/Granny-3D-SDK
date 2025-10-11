#if !defined(GRANNY_MEMORY_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_memory.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

// By default, we turn on debug memory if DEBUG is on, but you can control
// it separately by defining DEBUG_MEMORY up front
#if !defined(DEBUG_MEMORY)
#define DEBUG_MEMORY DEBUG
#endif

BEGIN_GRANNY_NAMESPACE;

struct memory_arena;

// These are the default alignment for allocated memory
EXPGROUP(RootMemoryGroup)

#define DefaultAllocationAlignment 4 EXPMACRO
#define MatrixBufferAlignment 16 EXPMACRO
#define LocalPoseAlignment 16 EXPMACRO

// TODO: It'd be nice if I added tags to the memory allocator such that
// any Granny allocation is marked as "transient" or "permanent", or
// some sort of arena system, so people who override our allocators
// can make smart decisions about that sort of thing.

// TODO: At some point, it might be nice to extend the allocator
// functionality to include automated initialization and such, for
// inserting counts into structures and initializing blocks to constant
// 8-bit or 32-bit value (since these are the operations that seem
// to be used in many places).

// The basic allocate operates via the Allocate() and Deallocate()
// functions, which manage memory and also track allocations and
// deallocations.  These are _SLOW BY DESIGN_, in an effort to
// discourage people from calling them very often.  If you have lots
// of allocations to do, consider using the aggregation macros
// described later.
#define Allocate(Type, Intent)                                              \
    (Type *)CallAllocateCallback(__FILE__, __LINE__,                        \
                                 DefaultAllocationAlignment, sizeof(Type),  \
                                 Intent)

#define AllocateArray(Count, Type, Intent)                                          \
    (Type *)CallAllocateCallback(__FILE__, __LINE__,                                \
                                 DefaultAllocationAlignment, sizeof(Type)*(Count),  \
                                 Intent)

#define AllocateSize(Size, Intent)                          \
    CallAllocateCallback(__FILE__, __LINE__,                \
                         DefaultAllocationAlignment, Size,  \
                         Intent)

#define AllocateAligned(Alignment, Type, Intent)            \
    (Type *)CallAllocateCallback(__FILE__, __LINE__,        \
                                 Alignment, sizeof(Type),   \
                                 Intent)

#define AllocateArrayAligned(Alignment, Count, Type, Intent)        \
    (Type *)CallAllocateCallback(__FILE__, __LINE__,                \
                                 Alignment, sizeof(Type)*(Count),   \
                                 Intent)

#define AllocateSizeAligned(Alignment, Size, Intent)                    \
    CallAllocateCallback(__FILE__, __LINE__, Alignment, Size, Intent)

#define Deallocate(Memory)     CallDeallocateCallback(__FILE__, __LINE__, Memory)
#define DeallocateSafe(Memory) { CallDeallocateCallback(__FILE__, __LINE__, Memory); (Memory) = 0; }

// All allocations in Granny are tracked, period, whether you're running
// in release or debug or whatever.  The idea is that everybody should
// be very conscientious about memory usage, so every running program should
// be able to provide a profile of the memory.  The following functions
// allow you to iterate over the allocated memory blocks, and find out
// who allocated them (and how much they allocated).
EXPGROUP(MemoryIterationGroup)

EXPTYPE struct allocation_header;
struct allocation_header
{
    enum {AllocationHeaderMV = 0xCA5ECA5E};
    uint32 MagicValue;
    uintaddrx Size;

    void *ActualPointer;
    uintaddrx ActualSize;

    char const *SourceFileName;
    int32x SourceLineNumber;

    allocation_header *Next;
    allocation_header *Previous;

    uintaddrx AllocationNumber;
};


// Iteration (STL style, only instead of ++ you use NextAllocation)
EXPAPI GS_MODIFY allocation_header *AllocationsBegin(void);
EXPAPI GS_MODIFY allocation_header *NextAllocation(allocation_header *Current);
EXPAPI GS_MODIFY allocation_header *AllocationsEnd(void);

// Accessors
EXPTYPE_EPHEMERAL struct allocation_information
{
    void *Memory;
    uintaddrx RequestedSize;
    uintaddrx ActualSize;
    char const *SourceFileName;
    int32x SourceLineNumber;
    intaddrx AllocationNumber;
};
EXPAPI GS_READ void GetAllocationInformation(allocation_header const *Header,
                                             allocation_information &Information);

allocation_header* GetHeaderFromMemory(void* Memory);


// Blockable checks
EXPAPI GS_MODIFY void* BeginAllocationCheck(void);
EXPAPI GS_MODIFY allocation_header* CheckedAllocationsEnd(void *CheckIdentifier);
EXPAPI GS_MODIFY bool EndAllocationCheck(void *CheckIdentifier);

EXPAPI GS_MODIFY void SetBreakAllocation(uintaddrx AllocNum);

// For overriding the standard allocators, you can use the following
// types and functions.
// TODO: Define these directly as __cdecl?
EXPGROUP(MemoryCallbackGroup)

EXPTYPE enum allocation_intent
{
    AllocationUnknown   = 0,
    AllocationTemporary = 1,
    AllocationInstance  = 2,
    AllocationFileData  = 3,
    AllocationLongTerm  = 4,

    AllocationBuilder   = 5,

    allocation_intent_forceint = 0x7fffffff
};


EXPAPI typedef CALLBACK_FN(void *) allocate_callback(char const *File, int32x Line,
                                                     uintaddrx Alignment, uintaddrx Size,
                                                     int32x AllocationIntent);
EXPAPI typedef CALLBACK_FN(void) deallocate_callback(char const *File, int32x Line,
                                                     void *Memory);

// You're asking yourself why this is here.  The answer is that we want to send
// allocations to telemetry, even if they are overriden by the user.
void* CallAllocateCallback(char const *File, int32x Line,
                           uintaddrx Alignment, uintaddrx Size,
                           int32x AllocationIntent);
void CallDeallocateCallback(char const *File, int32x Line,
                            void *Memory);


EXPAPI GS_READ void GetAllocator(allocate_callback *&AllocateCallback,
                                 deallocate_callback *&DeallocateCallback);
EXPAPI GS_MODIFY void SetAllocator(allocate_callback *AllocateCallback,
                                   deallocate_callback *DeallocateCallback);

// For freeing objects allocated by the builders
EXPAPI GS_PARAM void FreeBuilderResult(void *Result);

EXPAPI GS_MODIFY void AcquireMemorySpinlock();
EXPAPI GS_MODIFY void ReleaseMemorySpinlock();

EXPAPI GS_SAFE char const* AllocationIntentString(int32x Intent);

// For computing offset sizes, these macros are helpful
#define MAXOFFSET(Store, Offset, Size) \
    if(Store < (Offset + Size)) {Store = Offset + Size;}

// Platform-specific operations
void *PlatformAllocate(uintaddrx Size);
void PlatformDeallocate(void *Memory);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MEMORY_H
#endif
