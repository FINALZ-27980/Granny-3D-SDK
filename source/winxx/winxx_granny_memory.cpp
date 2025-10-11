// ========================================================================
// $File: //jeffr/granny_29/rt/winxx/winxx_granny_memory.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "winxx_granny_windows.h"

#include "granny_assert.h"
#include "granny_memory.h"
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

#define SubsystemCode MemoryLogMessage

USING_GRANNY_NAMESPACE;

#if DEBUG_MEMORY

static const int32x  ExtraBytes =  2 * SizeOf(uint32) + SizeOf(void*);;
static const uint32x VirtualAllocationMagicValue = 0xCA5ECA5E;
static const uint32x RegularAllocationMagicValue = 0xCA5ECA5F;

static int32x PageSize             = 0;
static int32x TotalAllocationCount = 0;

struct HiddenMarker
{
    void*  PagePointer;
    uint32 AllocateSize;
    uint32 MagicValue;
};
CompileAssert(sizeof(HiddenMarker) == ExtraBytes);


void *GRANNY
PlatformAllocate(uintaddrx Size)
{
    void *Result = 0;

    if (TotalAllocationCount < 10000)
    {
        // Ask the operating system for the vm page size, which we store
        // in a global so we only have to grab it once.
        if (PageSize == 0)
        {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            PageSize = si.dwPageSize;
        }

        // To make our guard page work, we need to allocate enough space for
        // our extra storage bytes, the allocation, and at least one full
        // page of extra memory.  We use 2*PageSize to guarentee we get a
        // full extra page, but you could theoretically use just PageSize
        // if you trust the virtual allocator never to allocate out of the
        // remainder of your trailing page (which it probably never does on
        // most OSes).
        intaddrx AllocateSize = ((Size + ExtraBytes + 2*PageSize - 1) &
                                 ~(PageSize-1));

        void *PagePointer = VirtualAlloc(0, AllocateSize,
                                         MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

        if (PagePointer)
        {
            // Calculate the offset from our base page pointer to the
            // boundary between the part you can touch (all bytes before)
            // and the guard page (this byte and all bytes after)
            uintaddrx PageBoundaryOffset = (Size + ExtraBytes +
                                            (PageSize - 1)) & ~(PageSize - 1);

            // Use the offset to protect the entire guard page against
            // reading and writing
            DWORD Ignored;
            VirtualProtect((uint8 *)PagePointer + PageBoundaryOffset,
                           PageSize, PAGE_NOACCESS, &Ignored);

            // The actual memory location returned is the location of the
            // page boundary minus the requested size, thus ensuring that
            // a single byte overrun will immediately fault.  This may
            // unalign the memory location, so pre-align your allocation
            // sizes if you care.
            Result = (uint8 *)PagePointer + (PageBoundaryOffset - Size);

            // Now we shove our original base pointer, the total size we
            // allocated, and a super-secret magic value that no one knows
            // but us into the preceeding extra bytes.  PlatformDeallocate
            // will grab these and use them.
            HiddenMarker* Marker = ((HiddenMarker*)Result) - 1;
            Marker->PagePointer  = PagePointer;
            Marker->AllocateSize = (uint32)AllocateSize;
            Marker->MagicValue   = VirtualAllocationMagicValue;

            Assert((intaddrx)((uint32 *)Result)[-2] == AllocateSize);
        }
    }
    else
    {
        Result = HeapAlloc(GetProcessHeap(), 0, Size + 4);
        if(Result)
        {
            Result = (uint8 *)Result + 4;
            ((uint32 *)Result)[-1] = RegularAllocationMagicValue;
        }
    }

    if (Result)
    {
        ++TotalAllocationCount;
    }

    return(Result);
}

void GRANNY
PlatformDeallocate(void *Memory)
{
    if(Memory)
    {
        HiddenMarker* Marker = ((HiddenMarker*)Memory) - 1;
        if(Marker->MagicValue == VirtualAllocationMagicValue)
        {
            int32x AllocateSize = Marker->AllocateSize;
            void *PagePointer   = Marker->PagePointer;

            Assert(AllocateSize != 0);
            Assert(PagePointer != 0);

            if(PagePointer)
            {
#if 0
                VirtualFree(PagePointer, AllocateSize, MEM_DECOMMIT);

                DWORD Ignored;
                VirtualProtect(PagePointer, AllocateSize,
                               PAGE_NOACCESS, &Ignored);
#else
                VirtualFree(PagePointer, 0, MEM_RELEASE);
#endif

                --TotalAllocationCount;
            }
        }
        else
        {
            Assert(Marker->MagicValue == RegularAllocationMagicValue);
            HeapFree(GetProcessHeap(), 0, (uint8 *)Memory - 4);

            --TotalAllocationCount;
        }
    }
}

#else

void *GRANNY
PlatformAllocate(uintaddrx Size)
{
    return(HeapAlloc(GetProcessHeap(), 0, Size));
}

void GRANNY
PlatformDeallocate(void *Memory)
{
    HeapFree(GetProcessHeap(), 0, Memory);
}

#endif
