// ========================================================================
// $File: //jeffr/granny_29/rt/winxx/winxx_granny_startup.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#include "winxx_granny_windows.h"
#include "granny_startup.h"
#include "winxx_granny_dll.h"

/* @cdep pre
   $requires($clipfilename($file)/winxx_granny_msvc_stubs.cpp)
 */

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

#if PLATFORM_WIN32 && (!BUILDING_GRANNY_STATIC && !BUILDING_MAYA && !BUILDING_MAX && !BUILDING_XSI)

extern "C"
{
    __declspec(noinline) void __cdecl __security_init_cookie( void );

    BOOL WINAPI
    _DllMainCRTStartup(HANDLE ThisDLL, DWORD Operation, LPVOID)
    {
        bool Result = true;

        if(Operation == DLL_PROCESS_ATTACH)
        {
            __security_init_cookie();

            // Whenever a process attaches to us, we want to make sure we
            // don't hear from them _every_ time they create a new thread
            // (since we don't care).  DisableThreadLibraryCalls() tells
            // Windows never to bother us with DLL_THREAD_ATTACH and
            // DLL_THREAD_DETACH calls.
            DisableThreadLibraryCalls((HINSTANCE)ThisDLL);

            if (DLLIsNotInWindowsPath(ThisDLL) == false)
            {
                Result = false;
            }
            else
            {
                // Any work needed to do on startup
            }
        }
        else if(Operation == DLL_PROCESS_DETACH)
        {
            // Now we let the platform non-specific part of Granny do any
            // work it needs to do on shutdown.
        }

        return(Result);
    }
}

#endif // PLATFORM_WIN32 && !BUILDING_GRANNY_STATIC

