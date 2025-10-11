// ========================================================================
// $File: //jeffr/granny_29/statement/source_monitor.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "source_monitor.h"
#include "statement.h"

#include "granny_assert.h"
#include "granny_file.h"
#include "granny_file_info.h"
#include "granny_memory_ops.h"
#include "granny_system_clock.h"

#include "statement_editstate.h"
#include "simple_job.h"

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include <map>
#include <windows.h>
#include <shlwapi.h>

USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;
using namespace std;


namespace
{
    struct source_attributes
    {
        source_file_ref*          SourceRef;
        string                    AbsPath;
        WIN32_FILE_ATTRIBUTE_DATA FileAttr;
        bool                      NeedsUpdate;
    };

    struct change_spec
    {
        source_file_ref*          SourceRef;
        file*                     NewFile;
        file_info*                NewInfo;
    };

    typedef vector<source_attributes> source_list;

    class MonitorJob : public SimpleJob
    {
    public:
        MonitorJob(char const* Name) : SimpleJob(Name), LastPos(0), LastCheck(0) { }
        virtual ~MonitorJob() { }

        void TheJob();

    public:
        int32x              LastPos;
        DWORD               LastCheck;
        source_list         MonitoredSources;
        vector<change_spec> ChangeSpecs;
    };

    MonitorJob* s_MonitorJob = 0;
}


void MonitorJob::TheJob()
{
    int32x ChecksThisRun = 0;
    int32x LoadsThisRun = 0;
    if (MonitoredSources.size() == 0)
        return;

    // Only allow checks every 10 ms or so to avoid banging the disk too hard.  We always
    // do the loop to check for loads that need to happen
    int32x const MaxLoadsPerRun  = 1;
    int32x const MaxChecksPerRun = 5;
    DWORD  const MinTimeBetweenMS = 250;

    bool ChecksAllowed = false;
    DWORD ThisTicks = GetTickCount();
    if ((ThisTicks - LastCheck) > MinTimeBetweenMS)
    {
        ChecksAllowed = true;
        LastCheck = ThisTicks;
    }

    // Compute possibly altered start pos
    // for (0...Monitored.Size(), loads < maxloads, checks < maxchecks)
    //   if (Source[Offset].NeedsUpdate)
    //      try to load
    //   else
    //      look at file attr
    //      if (mod, mark needs update)
    // Sleep to be nice

    // Ensures we step to the next on failures...
    LastPos = (LastPos+1) % MonitoredSources.size();

    {for (int32x Offset = 0;
          ( Offset < int(MonitoredSources.size()) &&
            ChecksThisRun < MaxChecksPerRun &&
            LoadsThisRun < MaxLoadsPerRun );
          ++Offset)
    {
        LastPos = (LastPos+1) % MonitoredSources.size();
        source_attributes& Attr = MonitoredSources[LastPos];

        if (Attr.NeedsUpdate)
        {
            WIN32_FILE_ATTRIBUTE_DATA newFAD;
            if (GetFileAttributesEx(Attr.AbsPath.c_str(),
                                    GetFileExInfoStandard,
                                    &newFAD))
            {
                change_spec NewSpec;
                NewSpec.SourceRef = Attr.SourceRef;
                NewSpec.NewFile = ReadEntireFile(Attr.AbsPath.c_str());
                if (NewSpec.NewFile != 0)
                {
                    NewSpec.NewInfo = GetFileInfo(*NewSpec.NewFile);
                    ChangeSpecs.push_back(NewSpec);

                    Attr.NeedsUpdate = false;
                    Attr.FileAttr = newFAD;
                    StTMPrintf("Successfully loaded: %s", StTMDynString(Attr.AbsPath.c_str()));
                }
                else
                {
                    StTMPrintf("Failed to load: %s", StTMDynString(Attr.AbsPath.c_str()));
                }
            }
            else
            {
                // wait for the next go around
            }

            ++LoadsThisRun;
        }
        else if (ChecksAllowed)
        {
            // Try to update the file attributes
            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (GetFileAttributesEx(Attr.AbsPath.c_str(),
                                    GetFileExInfoStandard,
                                    &fad))
            {
                if (memcmp(&Attr.FileAttr.ftLastWriteTime, &fad.ftLastWriteTime, sizeof(FILETIME)) != 0 ||
                    Attr.FileAttr.nFileSizeHigh != fad.nFileSizeHigh ||
                    Attr.FileAttr.nFileSizeLow  != fad.nFileSizeLow)
                {
                    Attr.NeedsUpdate = true;
                }
            }
            else
            {
                // Should not really happen, but go ahead and wait for the next go-around
            }

            ++ChecksThisRun;
        }
    }}

    StTMPlotInt(LoadsThisRun, "AnimStudio/Monitor/Loads");
    StTMPlotInt(ChecksThisRun, "AnimStudio/Monitor/Checks");
}


void StartMonitoring(source_file_ref* SourceRef)
{
    if (!SourceRef || !SourceRef->SourceFilename)
        return;

    if (s_MonitorJob != 0)
    {
        s_MonitorJob->WaitForJob();

        {for (size_t Idx = 0; Idx < s_MonitorJob->MonitoredSources.size(); ++Idx)
        {
            if (s_MonitorJob->MonitoredSources[Idx].SourceRef == SourceRef)
            {
                InvalidCodePath("already monitoring that source");
                return;
            }
        }}
    }

    source_attributes NewAttr;
    NewAttr.SourceRef = SourceRef;
    NewAttr.AbsPath = edit::GetSourcePath(SourceRef->SourceFilename);
    ZeroStructure(NewAttr.FileAttr);
    NewAttr.NeedsUpdate = false;

    if (!GetFileAttributesEx(NewAttr.AbsPath.c_str(),
                             GetFileExInfoStandard,
                             &NewAttr.FileAttr))
    {
        // Fail... hm.
        return;
    }

    if (s_MonitorJob == 0)
        s_MonitorJob = new MonitorJob("SourceMonitor");

    s_MonitorJob->MonitoredSources.push_back(NewAttr);
}

void StopMonitoring(source_file_ref* SourceRef)
{
    if (s_MonitorJob)
    {
        s_MonitorJob->WaitForJob();

        {for (size_t Idx = 0; Idx < s_MonitorJob->MonitoredSources.size(); ++Idx)
        {
            if (s_MonitorJob->MonitoredSources[Idx].SourceRef == SourceRef)
            {
                s_MonitorJob->MonitoredSources.erase(s_MonitorJob->MonitoredSources.begin() + Idx);
                break;
            }
        }}

        // Remove any references to the source ref that might be on the change list...
        {for (int32x Idx = int32x(s_MonitorJob->ChangeSpecs.size()) - 1; Idx >= 0; --Idx)
        {
            if (s_MonitorJob->ChangeSpecs[Idx].SourceRef == SourceRef)
            {
                FreeFile(s_MonitorJob->ChangeSpecs[Idx].NewFile);
                s_MonitorJob->ChangeSpecs.erase(s_MonitorJob->ChangeSpecs.begin() + Idx);
            }
        }}

        if (s_MonitorJob->MonitoredSources.empty())
        {
            delete s_MonitorJob;
            s_MonitorJob = 0;
        }
    }
}

bool GetChangedSourceRef(source_file_ref*& ChangedRef,
                         file*&            ChangedFile,
                         file_info*&       ChangedInfo)
{
    if (!s_MonitorJob || !s_MonitorJob->JobDone())
        return false;

    s_MonitorJob->WaitForJob();
    if (s_MonitorJob->ChangeSpecs.empty() == false)
    {
        ChangedRef  = s_MonitorJob->ChangeSpecs.front().SourceRef;
        ChangedFile = s_MonitorJob->ChangeSpecs.front().NewFile;
        ChangedInfo = s_MonitorJob->ChangeSpecs.front().NewInfo;
        s_MonitorJob->ChangeSpecs.erase(s_MonitorJob->ChangeSpecs.begin());
        return true;
    }

    // Last time the function is called, we kick the next monitor
    s_MonitorJob->DoJob();

    return false;
}
