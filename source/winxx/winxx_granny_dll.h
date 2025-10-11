#if !defined(WINXX_GRANNY_DLL_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/winxx/winxx_granny_dll.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(WINXX_GRANNY_WINDOWS_H)
#include "winxx_granny_windows.h"
#endif

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

bool DLLIsNotInWindowsPath(HANDLE DllHandle);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define WINXX_GRANNY_DLL_H
#endif /* WINXX_GRANNY_DLL_H */
