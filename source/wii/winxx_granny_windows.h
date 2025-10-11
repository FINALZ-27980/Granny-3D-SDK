#if !defined(WINXX_GRANNY_WINDOWS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/winxx/winxx_granny_windows.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#ifndef GRANNY_PLATFORM_H
#include "granny_platform.h"
#endif

#if !PLATFORM_WINXX
#error "This is a Windows file for windows people"
#endif

#ifndef STRICT
#define STRICT
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef WIN32_EXTRA_LEAN
#define WIN32_EXTRA_LEAN
#endif

#ifndef WIN64_LEAN_AND_MEAN
#define WIN64_LEAN_AND_MEAN
#endif
#ifndef WIN64_EXTRA_LEAN
#define WIN64_EXTRA_LEAN
#endif

#include <windows.h>

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

// WinXXLogErrorAsWarning and WinXXLogErrorAsError will check the last
// error code.  If it is set, it will log the reported WinXX string as
// a warning or an error (respectively) in the error log.
#define WinXXLogErrorAsWarning(FailedWinXXFunction) \
        WinXXLogLastError_(false, __FILE__, __LINE__, #FailedWinXXFunction);
#define WinXXLogErrorAsError(FailedWinXXFunction) \
        WinXXLogLastError_(true, __FILE__, __LINE__, #FailedWinXXFunction);

// This is a more convenient form of the standard WinXX SetFilePointer call
int32x WinXXSeek(HANDLE WinXXFileHandle, int32x Offset, DWORD MoveMethod);

// The following functions should not be called directly.  You
// should use the macros defined above.
void WinXXLogLastError_(bool IsError,
                        char const *SourceFile, int32x SourceLineNumber,
                        char const *FailedWinXXFunction);

// Simple function to pop up a message box without allocations
enum message_box_type
{
    eOK, eAbortRetryIgnore
};
DWORD WinXXPopMessageBox(char const* Message, char const* Title, message_box_type Type);


// NOTE: These are #defines that Windows _says_ it defines (if you
// read MSDN), but they _don't_ actually define them anywhere.  I assume
// this is because they've added the #define's since MSVC 6.x shipped,
// or something similar (either that or its just totally bogus docs).
#if !defined(INVALID_SET_FILE_POINTER)
#define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#endif

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define WINXX_GRANNY_WINDOWS_H
#endif /* WINXX_GRANNY_WINDOWS_H */
