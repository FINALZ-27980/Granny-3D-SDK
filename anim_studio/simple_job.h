// ========================================================================
// $File: //jeffr/granny_29/statement/simple_job.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(SIMPLE_JOB_H)

// todo pimpl to get rid of windows?
#include <windows.h>

// Tracked job for telemetry
class SimpleJob
{
private:
    char* mName;

    HANDLE hThread;
    HANDLE hEvent;
    HANDLE hSemaphore;

    bool   Kicked;
    bool   Running;

    static DWORD WINAPI thread_function( void *p );

public:
    SimpleJob(char const* Name);
    virtual ~SimpleJob();

    void DoJob();
    bool JobDone() const;
    void WaitForJob();

    virtual void TheJob() = 0;
};

#define SIMPLE_JOB_H
#endif /* SIMPLE_JOB_H */
