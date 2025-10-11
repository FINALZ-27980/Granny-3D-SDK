// ========================================================================
// $File: //jeffr/granny_29/rt/granny_memory_ops.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_memory_ops.h"
#include "granny_assert.h"
#include "granny_parameter_checking.h"

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
SetInt32(uintaddrx Count, int32 Value, void *BufferInit)
{
    CheckPointerNotNull(BufferInit, return);

    int32* Buffer = (int32*)BufferInit;
    while (Count--)
        *Buffer++ = Value;
}

void GRANNY
SetInt32x(uintaddrx Count, int32x Value, void *BufferInit)
{
    CheckPointerNotNull(BufferInit, return);

    int32x* Buffer = (int32x*)BufferInit;
    while (Count--)
        *Buffer++ = Value;
}

void GRANNY
SetUInt32(uintaddrx Count, uint32 Value, void* BufferInit)
{
    CheckPointerNotNull(BufferInit, return);

    uint32* Buffer = (uint32*)BufferInit;
    while (Count--)
        *Buffer++ = Value;
}

void GRANNY
SetReal32(uintaddrx Count, real32 Value, void* BufferInit)
{
    CheckPointerNotNull(BufferInit, return);

    real32 *Buffer = (real32 *)BufferInit;
    while (Count--)
        *Buffer++ = Value;
}

void GRANNY
SetPtrNULL(uintaddrx Count, void* BufferInit)
{
    CheckPointerNotNull(BufferInit, return);

    void** Buffer = (void**)BufferInit;
    while (Count--)
        *Buffer++ = 0;
}

void* GRANNY
Move(int32x Size, void const* From, void* To)
{
    void* ret = To;

    int32x count = Size;
    if (To <= From || (char *)To >= ((char *)From + count))
    {
        // copy from lower addresses to higher addresses
        while (count--)
        {
            *(char *)To = *(char *)From;
            To = (char *)To + 1;
            From = (char *)From + 1;
        }
    }
    else
    {
        // Overlapping Buffers
        // copy from higher addresses to lower addresses
        To   = (char *)To   + count - 1;
        From = (char *)From + count - 1;
        while (count--)
        {
            *(char *)To = *(char *)From;
            To = (char *)To - 1;
            From = (char *)From - 1;
        }
    }

    return ret;
}


bool GRANNY
Compare(uintaddrx Count, void const* Buffer0, void const* Buffer1)
{
    uint8 const* Check0 = (uint8 const*)Buffer0;
    uint8 const* Check1 = (uint8 const*)Buffer1;
    while (Count--)
    {
        if (*Check0++ != *Check1++)
            return false;
    }

    return true;
}

uintaddrx GRANNY
AlignN(uintaddrx Value, uintaddrx Alignment)
{
    uintaddrx const Pad = Alignment - (Value % Alignment);
    if (Pad != Alignment)
        Value += Pad;

    return Value;
}

void* GRANNY
AlignPtrN(void* Value, uintaddrx Alignment)
{
    return (void*)(AlignN((uintaddrx)Value, Alignment));
}

void GRANNY
Reverse64(uintaddrx BufferSize, void* BufferInit)
{
    Assert((BufferSize % 8) == 0);

    uintaddrx Count = BufferSize / 8;
    uint64 *Buffer = (uint64 *)BufferInit;
    while(Count--)
    {
        *Buffer = Reverse64(*Buffer);
        ++Buffer;
    }
}

void GRANNY
Reverse32(uintaddrx BufferSize, void* BufferInit)
{
    Assert((BufferSize % 4) == 0);

    uintaddrx Count = BufferSize / 4;
    uint32 *Buffer = (uint32 *)BufferInit;
    while(Count--)
    {
        *Buffer = Reverse32(*Buffer);
        ++Buffer;
    }
}

void GRANNY
Reverse16(uintaddrx BufferSize, void *BufferInit)
{
    Assert((BufferSize % 2) == 0);

    uintaddrx Count = BufferSize / 2;
    if ((Count % 2) == 0)
    {
        // We can reverse 32-bits at a time.
        Count /= 2;
        uint32* Buffer = (uint32*)BufferInit;
        while (Count--)
        {
            *Buffer = Reverse16_32(*Buffer);
            ++Buffer;
        }
    }
    else
    {
        uint16* Buffer = (uint16*)BufferInit;
        while (Count--)
        {
            *Buffer = Reverse16_16(*Buffer);
            ++Buffer;
        }
    }
}

uint64 GRANNY
Reverse64(uint64 Value)
{
    // This is slow, but safe, given newer gcc compilers' twitchiness about type aliasing
    union
    {
        uint64 Result;
        uint8  SwitchArray[8];
    } Reverser;
    CompileAssert(SizeOf(Reverser) == SizeOf(uint64));

    Reverser.Result = Value;
    {for(int32x i = 0; i < 4; ++i)
    {
        uint8 temp = Reverser.SwitchArray[i];
        Reverser.SwitchArray[i] = Reverser.SwitchArray[7 - i];
        Reverser.SwitchArray[7 - i] = temp;
    }}

    return Reverser.Result;
}

uint32 GRANNY
Reverse32(uint32 Value)
{
    return ((Value << 24)               |
            ((Value & 0x0000FF00) << 8) |
            ((Value & 0x00FF0000) >> 8) |
            (Value >> 24));
}

uint32 GRANNY
Reverse16_32(uint32 Value)
{
    return (((Value & 0x00FF00FF) << 8) |
            ((Value & 0xFF00FF00) >> 8));
}

uint16 GRANNY
Reverse16_16(uint16 Value)
{
    return (uint16)(((Value & (uint16)0x00FF) << 8) |
                    ((Value & (uint16)0xFF00) >> 8));
}
