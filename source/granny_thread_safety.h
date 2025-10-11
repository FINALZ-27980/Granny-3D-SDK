#if !defined(GRANNY_THREAD_SAFETY_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_thread_safety.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(ThreadSafetyGroup);

EXPAPI GS_SPECIAL void SetAllowGlobalStateChanges(bool Allowed);
EXPAPI GS_SPECIAL bool GetAllowGlobalStateChanges();

EXPAPI typedef CALLBACK_FN(uint32x) thread_id_callback();
extern thread_id_callback *ThreadIDCallback;

EXPAPI GS_SPECIAL thread_id_callback *SetThreadIDCallback(thread_id_callback *Callback);
EXPAPI GS_SPECIAL bool ThreadAllowedToCallGranny();

// If we have a callback to identify the thread, use it.  Otherwise,
// simply return 0 all the time.
inline uint32 GetThreadID()
{
    if (ThreadIDCallback)
    {
        return (*ThreadIDCallback)();
    }
    else
    {
        return 0;
    }
}


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_THREAD_SAFETY_H
#endif
