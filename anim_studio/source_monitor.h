// ========================================================================
// $File: //jeffr/granny_29/statement/source_monitor.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(SOURCE_MONITOR_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

BEGIN_GSTATE_NAMESPACE;
struct source_file_ref;
END_GSTATE_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;
struct file;
struct file_info;
END_GRANNY_NAMESPACE;

// Turns on and off monitoring for a specific source ref.  This allows us to reload the
// file when it changes on disk...

void StartMonitoring(GSTATE source_file_ref*);
void StopMonitoring(GSTATE source_file_ref*);

bool GetChangedSourceRef(GSTATE source_file_ref*& ChangedRef,
                         GRANNY file*&            ChangedFile,
                         GRANNY file_info*&       ChangedFileInfo);

#define SOURCE_MONITOR_H
#endif /* SOURCE_MONITOR_H */
