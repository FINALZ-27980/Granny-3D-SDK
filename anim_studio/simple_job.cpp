// ========================================================================
// $File: //jeffr/granny_29/statement/simple_job.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "simple_job.h"
#include "statement.h"

#include "granny_assert.h"
#include "granny_string.h"
#include "granny_memory.h"

#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL
#include "rad_dumphandler.h"
#endif

USING_GRANNY_NAMESPACE;

SimpleJob::SimpleJob(char const* Name)
{
    mName = CloneString(Name);

    hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
    StTMNameLock(hEvent, "Event: %s", StTMDynString(mName));

    hSemaphore = CreateSemaphore( NULL, 0, 255, NULL );
    StTMNameLock(hSemaphore, "Semaphore: %s", StTMDynString(mName));

    DWORD threadId = 0;
    hThread = CreateThread( NULL, 0,( LPTHREAD_START_ROUTINE ) &thread_function, this, 0, &threadId );
    StTMNameThread(threadId, "%s", StTMDynString(Name));

    Kicked = false;
    Running = true;
}

SimpleJob::~SimpleJob()
{
    Deallocate(mName);

    if (Kicked)
        WaitForJob();
    Assert(Kicked == false);

    // Lock mutex
    Running = false;

    if (hSemaphore)
    {
        // Signal semaphore

        if (hThread)
        {
            // Wait for thread
            CloseHandle(hThread);
            hThread = INVALID_HANDLE_VALUE;
        }

        CloseHandle(hSemaphore);
        hSemaphore = INVALID_HANDLE_VALUE;
    }

    if (hEvent)
    {
        CloseHandle(hEvent);
        hEvent = INVALID_HANDLE_VALUE;
    }
}

void SimpleJob::DoJob()
{
    Assert(Kicked == false);

    Kicked = true;
    ReleaseSemaphore(hSemaphore, 1, NULL);
}

bool SimpleJob::JobDone() const
{
    if (!Kicked || WaitForSingleObject(hEvent,0) == WAIT_OBJECT_0)
        return true;

    return false;
}

void SimpleJob::WaitForJob()
{
    if (!Kicked)
    {
        return;
    }

    // WaitForEvent
    {
        StTMScopeLockAcquire(hEvent,"Waiting for job: %s", mName);
        WaitForSingleObject(hEvent,INFINITE);
        ResetEvent(hEvent);
    }

    Kicked = false;
}

DWORD WINAPI
SimpleJob::thread_function( void *p )
{
    SimpleJob* Job = (SimpleJob*)p;

#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL && !DEBUG
    __try
#endif
    {

        while (Job->Running)
        {
            // Wait for semaphore
            WaitForSingleObject(Job->hSemaphore,INFINITE);
            StTMScopeLockHold(Job->hEvent);

            if (Job->Running)
            {
                Assert(Job->Kicked);
                Job->TheJob();
            }

            // Signal event
            SetEvent(Job->hEvent);
        }
    }
#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL && !DEBUG
    __except(RADDumpWritingHandler(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER)
    {
        Assert(!"Turn on exception handling");

        // Since we're in a thread, the main thread may dead-lock waiting for this to
        // complete, just nuke the whole process, we're already dead...
        ExitProcess(0xffffffff);
    }
#endif

    return 0;
}
