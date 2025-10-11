#if !defined(STATEMENT_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/statement/statement.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if STATEMENT_TELEMETRY

#include "telemetry.h"
#include "granny_exp_interface.h"

extern HTELEMETRY g_context;

#define StTMEnter(zone)        tmEnter(g_context, TMZF_NONE, zone)
#define StTMEnterIdle(zone)    tmEnter(g_context, TMZF_IDLE, zone)
#define StTMLeave()            tmLeave(g_context)
#define StTMZone(zone)         tmZone(g_context, TMZF_NONE, zone)
#define StTMIdle(zone)         tmZone(g_context, TMZF_IDLE, zone)
#define StTMBlocked(zone)      tmZone(g_context, TMZF_STALL, zone)

#define StTMSection(name)      tmSetTimelineSectionName(g_context, name)

#define StTMPlotInt(val, name) tmPlotI32(g_context, TMPT_INTEGER, (val), (name))
#define StTMPlotSeconds(val, name) tmPlot(g_context,TMPT_TIME_MS, ((val) * 1000), (name))

#define StTMNameLock(lock, ...) tmLockName(g_context, (void*)(lock), ## __VA_ARGS__)
#define StTMNameThread(tid, ...) tmThreadName(g_context, (tid), ## __VA_ARGS__)

#define StTMScopeLockAcquire(lock, ...) (void)sizeof(lock)
#define StTMScopeLockHold(lock) (void)sizeof(lock)

#define StTMPrintf(fmt, ...)    tmMessage(g_context, TMMF_SEVERITY_LOG, fmt, ## __VA_ARGS__)
#define StTMPrintfERR(fmt, ...) tmMessage(g_context, TMMF_SEVERITY_ERROR, fmt, ## __VA_ARGS__)

#define StTMEnter(zone)        tmEnter(g_context, TMZF_NONE, zone)

#define StTMDynString(DynString)  tmDynamicString(g_context, (DynString))

#else

#define StTMEnter(zone) 0
#define StTMEnterIdle(zone) 0
#define StTMLeave() 0
#define StTMZone(zone) 0
#define StTMIdle(zone) 0
#define StTMBlocked(zone) 0

#define StTMSection(name) 0

#define StTMPlotInt(val, name) (void)sizeof(val)
#define StTMPlotSeconds(val, name) (void)sizeof(val)

#define StTMNameLock(lock, ...) (void)sizeof(lock)
#define StTMNameThread(tid, ...) (void)sizeof(tid)

#define StTMScopeLockAcquire(lock, ...) (void)sizeof(lock)
#define StTMScopeLockHold(lock) (void)sizeof(lock)

#define StTMPrintf(fmt, ...)    (void)sizeof(fmt)
#define StTMPrintfERR(fmt, ...) (void)sizeof(fmt)

#define StTMDynString(DynString)  (DynString)


#endif // STATEMENT_TELEMETRY

#define SizeI(vec) ((int)((vec).size()))

#include "header_postfix.h"
#define STATEMENT_H
#endif /* STATEMENT_H */
