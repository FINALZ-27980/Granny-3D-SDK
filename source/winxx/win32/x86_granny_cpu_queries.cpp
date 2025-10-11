// ========================================================================
// $File: //jeffr/granny_29/rt/x86/x86_granny_cpu_queries.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_types.h"
#include "x86_granny_cpu_queries.h"

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

static uint32 SSEResult = 0;
static bool   SSETest = true;

bool GRANNY
SSEIsAvailable(void)
{
    if (SSETest)
    {
        // You might be thinking: Hey Granny!  Look, SSEResult is already 0, you don't
        // need to do this!  Normally, you'd be totally right.  HOWEVER, certain versions
        // of GCC will not notice the reference to SSEResult in the ASM block below, and
        // thus assume that it is safe to move SSEResult into readonly memory.  Sweet.
        // Please leave it alone.
        SSETest = false;
        SSEResult = 0;

        __asm
        {
            pushfd
            pop eax                 ;EAX = original eflags word

            mov ebx,eax             ;save it in EBX

            xor eax,1 << 21         ;toggle bit 21
            push eax
            popfd

            pushfd                  ;examine eflags again
            pop eax

            push ebx                ;restore original eflags value
            popfd

            cmp eax,ebx             ;bit changed?
            je __no_SSE             ;no CPUID, ergo no SSE

            mov eax,1               ;else OK to perform CPUID
            cpuid
            test edx,0x2000000      ;test bit 25
            jz __no_SSE

            mov dword ptr [SSEResult], 1

            __no_SSE:
        }
    }

    return (SSEResult != 0);
}
