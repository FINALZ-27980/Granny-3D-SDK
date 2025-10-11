// ========================================================================
// $File: //jeffr/granny_29/rt/winxx/winxx_granny_windows.cpp $
// $DateTime: 2012/07/18 09:46:00 $
// $Change: 38396 $
// $Revision: #3 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_platform.h"

// The build system sometimes tries to build this even on non-Win32 platforms.
#if PLATFORM_WINXX

#include "winxx_granny_windows.h"
#include "granny_log.h"
#include "granny_assert.h"

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

static DWORD
MyMBIndirect(LPVOID Param)
{
    if (Param == 0)
        return IDABORT;

    MSGBOXPARAMSA* MessageBoxParameters = (MSGBOXPARAMSA*)Param;
    return MessageBoxA(MessageBoxParameters->hwndOwner,
                       MessageBoxParameters->lpszText,
                       MessageBoxParameters->lpszCaption,
                       MessageBoxParameters->dwStyle);
}

void GRANNY
WinXXLogLastError_(bool IsError,
                   char const *SourceFile, int32x SourceLineNumber,
                   char const *FailedWinXXFunction)
{
    DWORD LastError = GetLastError();
    if(LastError != NO_ERROR)
    {
        LPVOID MessageBuffer;
        if(FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM,
            0,
            LastError,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
            (LPTSTR)&MessageBuffer,
            0,
            0) == 0)
        {
            // WinXX couldn't supply us with an error code
            MessageBuffer = "unknown windows error";
        }

        Log4(IsError ? ErrorLogMessage : WarningLogMessage,
             WinXXSubsystemLogMessage,
             "%s failed with error \"%s\" at %s(%d)",
             FailedWinXXFunction, (char *)MessageBuffer,
             SourceFile, SourceLineNumber);

        LocalFree(MessageBuffer);
    }
}

int32x GRANNY
WinXXSeek(HANDLE WinXXFileHandle, int32x Offset, DWORD MoveMethod)
{
    int32x Result = SetFilePointer(WinXXFileHandle, Offset, 0, MoveMethod);
    if((Result == INVALID_SET_FILE_POINTER) &&
       (GetLastError() != 0))
    {
        WinXXLogErrorAsWarning(SetFilePointer);
    }

    return(Result);
}

DWORD GRANNY
WinXXPopMessageBox(char const* Message, char const* Title, message_box_type Type)
{
    DWORD ThreadID;
    MSGBOXPARAMSA MessageBoxParameters;
    MessageBoxParameters.cbSize = sizeof(MessageBoxParameters);
    MessageBoxParameters.hwndOwner = 0;
    MessageBoxParameters.hInstance = 0;
    MessageBoxParameters.lpszText = Message;
    MessageBoxParameters.lpszCaption = Title;
    MessageBoxParameters.dwStyle = MB_TASKMODAL;
    MessageBoxParameters.lpszIcon = MAKEINTRESOURCEA(32513); //IDI_ERROR;
    MessageBoxParameters.dwContextHelpId = 0;
    MessageBoxParameters.lpfnMsgBoxCallback = 0;
    MessageBoxParameters.dwLanguageId = 0;

    switch (Type)
    {
        default:
        case eAbortRetryIgnore:
            MessageBoxParameters.dwStyle |= MB_ABORTRETRYIGNORE;
            break;

        case eOK:
            MessageBoxParameters.dwStyle |= MB_OK;
            break;
    }

    // Many thanks to Todd Laney for pointing out the following "correct"
    // way to pop up an assertion box such that the asserting thread
    // will no longer get paint requests or other events that could
    // trigger recursive asserting.
    // TODO: should use _beginthreadex/_endthreadex?
    HANDLE Thread =
        CreateThread(0, 0,
                     (LPTHREAD_START_ROUTINE)MyMBIndirect,
                     (LPVOID)&MessageBoxParameters,
                     0, &ThreadID);
    if (Thread)
    {
        WaitForSingleObject(Thread, INFINITE);
        DWORD ExitCode = IDABORT;
        GetExitCodeThread(Thread, &ExitCode);
        CloseHandle(Thread);

        return ExitCode;
    }
    else
    {
        return IDABORT;
    }
}




#endif //#if !PLATFORM_WINXX
