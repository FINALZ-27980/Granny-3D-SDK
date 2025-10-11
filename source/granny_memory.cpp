// ========================================================================
// $File: //jeffr/granny_29/rt/granny_memory.cpp $
// $DateTime: 2012/05/29 16:14:07 $
// $Change: 37605 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_memory.h"

#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"
#include "granny_telemetry.h"
#include "rrAtomics.h"
#include "rrCore.h"
#include "granny_threads.h"

#if DIALOG_ON_MEMORY_EXHAUSTION
#if !PLATFORM_WINXX
#error "Only on windows, please."
#endif
#include "winxx_granny_windows.h"
#endif

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
#define SubsystemCode MemoryLogMessage

USING_GRANNY_NAMESPACE;

// Only modify this when holding the memory lock.

uintaddrx       s_AllocationNumber = 0;
uintaddrx const s_AllocNumMask     = 1ULL << (sizeof(uintaddrx) * 8 - 1);
uintaddrx       s_BreakAlloc       = s_AllocNumMask;

// For debugging purposes
char const* GRANNY
AllocationIntentString(int32x Intent)
{
    switch (Intent)
    {
        case AllocationUnknown:   return "GrannyAllocationUnknown";
        case AllocationTemporary: return "GrannyAllocationTemporary";
        case AllocationInstance:  return "GrannyAllocationInstance";
        case AllocationFileData:  return "GrannyAllocationFileData";
        case AllocationLongTerm:  return "GrannyAllocationLongTerm";
        case AllocationBuilder:   return "GrannyAllocationBuilder";
        default:
            InvalidCodePath("unknown intent!");
            return "InvalidIntent";
    }
}

/* ========================================================================
   Basic allocator
   ======================================================================== */
static allocation_header MemorySentinel =
{
    (uint32)allocation_header::AllocationHeaderMV, 0,
    0, 0,
    __FILE__, __LINE__,
    &MemorySentinel,
    &MemorySentinel,
};

static void *
GetMemoryFromHeader(allocation_header const *Header)
{
    return (void*)(Header + 1);
}

allocation_header* GRANNY
GetHeaderFromMemory(void *Memory)
{
    // Back up the pointer to the header we stored before it
    allocation_header *Header = (allocation_header *)Memory;
    --Header;

    return(Header);
}

void
SetBlockToBAADF00D(uintaddrx Size, void *Pointer)
{
    SetUInt32(Size / 4, 0xBAADF00D, Pointer);
}

CALLBACK_FN(void*)
BasicTrackedAllocate(char const *File, int32x Line,
                     uintaddrx Alignment,
                     uintaddrx Size,
                     int32x Intent)
{
    uint8 *Memory = 0;

    // Allocate the requested space, plus space for our hidden header,
    // plus space for the alignment we might need to do.
    uintaddrx const MemorySize =
        (sizeof(allocation_header) + (Alignment - 1) + Size);

    void *ActualPointer = PlatformAllocate(MemorySize);
    if (ActualPointer)
    {
        Memory = (uint8 *)ActualPointer;
        Memory += sizeof(allocation_header);
        Memory = (uint8*)AlignN((uintaddrx)Memory, Alignment);

        allocation_header *Header = GetHeaderFromMemory(Memory);
        Assert(((uintaddrx)Memory % Alignment) == 0);
        Assert(PtrDiffSignOnly(Header, ActualPointer) >= 0);

        // Record the information about this allocation
        Header->MagicValue = (uint32)allocation_header::AllocationHeaderMV;
        Header->ActualPointer = ActualPointer;
        Header->SourceFileName = File;
        Header->SourceLineNumber = Line;
        Header->Size = Size;
        Header->ActualSize = MemorySize;

#if DEBUG_MEMORY
        // Initialize the entire block with bogus values
        SetBlockToBAADF00D(Header->Size, Memory);
#endif

#if DEBUG
        bool BreakAlloc = false;
#endif

        // Lock this to protect the list while we're changing it...
        {
            AcquireMemorySpinlock();

            // Do this little dance so that the default break alloc will never cause an
            // int3
            s_AllocationNumber = (s_AllocationNumber + 1) & ~s_AllocNumMask;
#if DEBUG
            if (s_AllocationNumber == s_BreakAlloc)
                BreakAlloc = true;
#endif

            // Link us in with the rest of the memory
            Header->Next = MemorySentinel.Next;
            Header->Previous = &MemorySentinel;


            Header->AllocationNumber = s_AllocationNumber;
            MemorySentinel.Next->Previous = Header;
            MemorySentinel.Next = Header;

            ReleaseMemorySpinlock();
        }

#if DEBUG
        if (BreakAlloc)
        {
            RR_BREAK();
        }
#endif
    }
    else
    {
#if DIALOG_ON_MEMORY_EXHAUSTION
        WinXXPopMessageBox(("Granny was unable to allocate memory from the operating system.\r\n"
                            "If this has occurred in the exporter, this typically indicates\r\n"
                            "that you're exporting a scene that is extremely large, or has\r\n"
                            "otherwise fragmented virtual memory to the point that few large\r\n"
                            "continuous regions exist in the address space.  Write to us at\r\n"
                            "granny3@radgametools.com for assistance."),
                           "Memory exhaustion", eOK);
        DWORD WinXXPopMessageBox(char const* Message, char const* Title);
#endif

        Log4(ErrorLogMessage, MemoryLogMessage,
             "Unable to allocate %d bytes (%d requested by %s:%d).",
             MemorySize, Size, File, Line);
    }

    return Memory;
}

static bool
AllocationHeaderIsValid(allocation_header *Header)
{
    return (Header &&
            (Header->MagicValue == allocation_header::AllocationHeaderMV) &&
            (Header->Size != (~(uintaddrx)0)));
}

CALLBACK_FN(void)
BasicTrackedDeallocate(char const *File, int32x Line, void *Memory)
{
    if (Memory)
    {
        // Back up the pointer to the header we stored before it
        allocation_header *Header = GetHeaderFromMemory(Memory);

        // We must grab this *before* checking allocation header is valid to ensure that
        // it's not blown away underneath us.
        AcquireMemorySpinlock();

        // Make sure we actually allocated it in the first place
        if (AllocationHeaderIsValid(Header))
        {
#if DEBUG_MEMORY
            // Overwrite the entire block with bogus values
            SetBlockToBAADF00D(Header->Size, Memory);
#endif

            // Overwrite the size so we can catch quick double-frees.  This causes
            // HeaderIsValid to return false, btw.
            Header->Size = ~(uintaddrx)0;

            // Unlink
            Assert(AllocationHeaderIsValid(Header->Next));
            Assert(AllocationHeaderIsValid(Header->Previous));
            Header->Next->Previous = Header->Previous;
            Header->Previous->Next = Header->Next;

            // At this point we can release the spinlock, since the header is out of the
            // list, and if the OS puts us to sleep on the free(), we don't want to
            // monopolize that resource.
            ReleaseMemorySpinlock();

            // Nuke the OS memory
            PlatformDeallocate(Header->ActualPointer);
        }
        else
        {
            ReleaseMemorySpinlock();

            Log3(ErrorLogMessage, MemoryLogMessage,
                 "%s:%d Attempted to free 0x%x, which was not allocated "
                 "by this allocator.", File, Line, Memory);
#if DEBUG_MEMORY
            Assert ( false );
#endif
        }
    }
}

allocation_header *GRANNY
AllocationsBegin(void)
{
    return(MemorySentinel.Next);
}

allocation_header *GRANNY
NextAllocation(allocation_header *Current)
{
    // Make sure it's actually a header
    Assert(AllocationHeaderIsValid(Current));

    // Make sure people aren't trying to iterate past the end
    Assert(Current != &MemorySentinel);

    Current = Current->Next;

    // Double check to make sure the next allocation block is valid
    Assert(AllocationHeaderIsValid(Current));

    return(Current);
}

allocation_header *GRANNY
AllocationsEnd(void)
{
    return(&MemorySentinel);
}

void GRANNY
GetAllocationInformation(allocation_header const *Header,
                         allocation_information &Information)
{
    Assert(Header);
    Assert(Header->MagicValue == allocation_header::AllocationHeaderMV);

    // TODO: Report actual size for real
    Information.Memory = GetMemoryFromHeader(Header);
    Information.RequestedSize = Header->Size;
    Information.ActualSize = Header->Size + sizeof(allocation_header);
    Information.SourceFileName = Header->SourceFileName;
    Information.SourceLineNumber = Header->SourceLineNumber;
    Information.AllocationNumber = Header->AllocationNumber;
}

void *GRANNY
BeginAllocationCheck(void)
{
    return(AllocateSize(0, AllocationUnknown));
}

allocation_header *GRANNY
CheckedAllocationsEnd(void *CheckIdentifier)
{
    allocation_header *Header = GetHeaderFromMemory(CheckIdentifier);
    Assert(AllocationHeaderIsValid(Header));

    return(Header);
}

bool GRANNY
EndAllocationCheck(void *CheckIdentifier)
{
    bool Result = (GetHeaderFromMemory(CheckIdentifier) == MemorySentinel.Next);
    if(!Result)
    {
        // The entire iteration must hold the lock...
        {
            AcquireMemorySpinlock();

            {for(allocation_header *Current = AllocationsBegin();
                 Current != CheckedAllocationsEnd(CheckIdentifier);
                 Current = NextAllocation(Current))
            {
                allocation_information Information;
                GetAllocationInformation(Current, Information);
                Log5(WarningLogMessage, MemoryLogMessage,
                    "Unfreed block\n%s(%d): (%d bytes at 0x%x, allocnum: %d)",
                     Information.SourceFileName,
                     Information.SourceLineNumber,
                     Information.RequestedSize,
                     Information.Memory,
                     Information.AllocationNumber);
                Result = false;
            }}

            ReleaseMemorySpinlock();
        }

        Assert(!"Blockable allocation check failed - allocations written to log");
    }

    Deallocate(CheckIdentifier);

    return(Result);
}

void GRANNY
SetBreakAllocation(uintaddrx AllocNum)
{
    s_BreakAlloc = AllocNum;
}


void GRANNY
FreeBuilderResult(void *Result)
{
    Deallocate(Result);
}

static allocate_callback*   AllocateCallback   = BasicTrackedAllocate;
static deallocate_callback* DeallocateCallback = BasicTrackedDeallocate;

void GRANNY
GetAllocator(allocate_callback *&ReturnAllocateCallback,
             deallocate_callback *&ReturnDeallocateCallback)
{
    ReturnAllocateCallback = AllocateCallback;
    ReturnDeallocateCallback = DeallocateCallback;
}

void GRANNY
SetAllocator(allocate_callback *AllocateCallbackInit,
             deallocate_callback *DeallocateCallbackInit)
{
    AllocateCallback = AllocateCallbackInit;
    DeallocateCallback = DeallocateCallbackInit;
}


void* GRANNY
CallAllocateCallback(char const *File, int32x Line,
                     uintaddrx Alignment, uintaddrx Size,
                     int32x AllocationIntent)
{
    void* ReturnPointer = (*AllocateCallback)(File, Line, Alignment, Size, AllocationIntent);

    GRANNY_TM_ALLOCEX(ReturnPointer, File, Line, Size, "%s", AllocationIntentString(AllocationIntent));

    return ReturnPointer;
}

void GRANNY
CallDeallocateCallback(char const *File, int32x Line, void *Memory)
{
    GRANNY_TM_FREE(Memory);
    (*DeallocateCallback)(File, Line, Memory);
}



// For hooking into the granny allocator from C layer (mostly the shared rad bits...)
RADDEFFUNC void* GrannyAllocHook(unsigned int bytes)
{
    return AllocateSize(bytes, AllocationUnknown);
}

RADDEFFUNC void GrannyDeallocateHook(void* ptr)
{
    return Deallocate(ptr);
}

// The spinlock is nothing more than an integer
static ALIGN16(U32) volatile s_MemorySpinlockInteger = 0;

#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY
static bool volatile s_LockNamed = false;
#endif

void GRANNY
AcquireMemorySpinlock()
{
#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY
    TmU64 matcher=0;
    tmTryLockEx(GrannyTMContext, &matcher, 500,
                __FILE__, __LINE__,
                (const void*)&s_MemorySpinlockInteger, "Grabbing Granny Memory Lock");
#endif

    U32 oldVal = 0;
    while (!rrAtomicCAS32(&s_MemorySpinlockInteger, &oldVal, 1))
    {
        // Spin!
        oldVal = 0;
        ThreadYieldToAny();
    }

#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY
    tmEndTryLockEx(GrannyTMContext, matcher, __FILE__,__LINE__,
                   (const void*)&s_MemorySpinlockInteger, TMLR_SUCCESS);
    tmSetLockState(GrannyTMContext, (const void*)&s_MemorySpinlockInteger,
                   TMLS_LOCKED, "Acquired Memory Lock");

    if (s_LockNamed == false)
    {
        tmLockName( GrannyTMContext, (const void*)&s_MemorySpinlockInteger, "Granny Memory Lock" );
        s_LockNamed = true;
    }
#endif

    Assert(s_MemorySpinlockInteger == 1);
    Assert(oldVal == 0);
}

void GRANNY
ReleaseMemorySpinlock()
{
    // The spinlock had better be 1, since we own it.
    Assert(s_MemorySpinlockInteger == 1);

    // Since we own it, we can just set it to 0, but put a write barrier down.
    rrAtomicStoreRelease32(&s_MemorySpinlockInteger, 0);

#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY
    tmSetLockState(GrannyTMContext,(const void*)&s_MemorySpinlockInteger,
                   TMLS_RELEASED, "Releasing Memory Lock");
#endif
}



