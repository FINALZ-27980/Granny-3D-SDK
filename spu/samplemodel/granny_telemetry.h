#if !defined(GRANNY_TELEMETRY_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_telemetry.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

// Pertinent macros:
//
//  GRANNY_TELEMETRY 0/1
//    GRANNY_DEBUG_MEMORY_CALLSTACK 0/1


#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if defined(GRANNY_TELEMETRY) && GRANNY_TELEMETRY

#include "granny_clear_exp_interface.h"
#if defined(GRANNY_REAL_TELEMETRY)
#include "telemetry.h"
#else
#include "TmAPI.h"
#endif

#include "granny_exp_interface.h"

extern "C" { extern void* global_Granny_TmContext; }
#define GrannyTMContext ((HTELEMETRY)global_Granny_TmContext)

#define GRANNY_AUTO_ZONE(ZoneName)  tmZone((HTELEMETRY)global_Granny_TmContext,TMZF_NONE,#ZoneName)
#define GRANNY_ENTER_ZONE(ZoneName) do { tmEnter((HTELEMETRY)global_Granny_TmContext, TMZF_NONE, #ZoneName); } while (false)

#define GRANNY_AUTO_IDLE_ZONE(name)   tmZone((HTELEMETRY)global_Granny_TmContext,TMZF_IDLE,#name)

#define GRANNY_AUTO_ZONE_FN()       tmZone((HTELEMETRY)global_Granny_TmContext,TMZF_NONE,__FUNCTION__)
#define GRANNY_ENTER_ZONE_FN()      do { tmEnter((HTELEMETRY)global_Granny_TmContext, __FUNCTION__,0,TMZF_NONE); } while (false)

#define GRANNY_LEAVE_ZONE()         do { tmLeave((HTELEMETRY)global_Granny_TmContext); } while (false)


#define GRANNY_INC_INT_ACCUMULATOR(Var) ++(Var)

#define GRANNY_EMIT_INT_VALUE(Name, Var)                                            \
    do {                                                                            \
        tmPlotI32((HTELEMETRY)global_Granny_TmContext,TMPT_INTEGER,(Var),(Name));   \
    } while (false)

#define GRANNY_EMIT_FLOAT(Name, Var)                                        \
    do {                                                                    \
        tmPlotF64((HTELEMETRY)global_Granny_TmContext,TMPT_NONE,(Var),(Name));  \
    } while (false)

#define GRANNY_EMIT_INT_ACCUMULATOR(Name, Var)                                      \
    do {                                                                            \
        tmPlotI32((HTELEMETRY)global_Granny_TmContext,TMPT_INTEGER,(Var),(Name));   \
        (Var) = 0;                                                                  \
    } while (false)

#define GRANNY_TM_MESSAGE(MsgFlag, Fmt, ...)                \
    tmMessage((HTELEMETRY)global_Granny_TmContext, MsgFlag, Fmt, ## __VA_ARGS__)

#define GRANNY_TM_LOG(Fmt, ...)                                         \
    tmMessage((HTELEMETRY)global_Granny_TmContext,  TMMF_SEVERITY_LOG, Fmt, ## __VA_ARGS__)


#define GRANNY_TM_DYNSTRING(Str) tmDynamicString((HTELEMETRY)global_Granny_TmContext, (Str))


#define GRANNY_DECL_ACCUM_ZONE(ZoneName) TmU64 ZoneName ## _AccumCtr = 0; TmU32 ZoneName ## _AccumCnt = 0
#define GRANNY_ENTER_ACCUM_ZONE(ZoneName)                                           \
    do {                                                                            \
        ++(ZoneName ## _AccumCnt);                                                  \
        tmEnterAccumulationZone(global_Granny_TmContext, (ZoneName ## _AccumCtr));  \
    } while (false)
#define GRANNY_LEAVE_ACCUM_ZONE(ZoneName) tmLeaveAccumulationZone(global_Granny_TmContext, (ZoneName ## _AccumCtr))

#define GRANNY_EMIT_ACCUM_ZONE(ZoneName)                                    \
    do {                                                                    \
        static char const* ZoneStr_ ## ZoneName = #ZoneName;                \
        tmEmitAccumulationZones((HTELEMETRY)global_Granny_TmContext, 0, 1,  \
                                &(ZoneName ## _AccumCnt),                   \
                                &(ZoneName ## _AccumCtr),                   \
                                &(ZoneStr_ ## ZoneName),                    \
                                0);                                         \
    } while (false)

#define GRANNY_EMIT_ACCUM_ZONE_2(ZoneName1, ZoneName2)                                  \
    do {                                                                                \
        TmU32 ZoneCounts[] = { (ZoneName1 ## _AccumCnt), (ZoneName2 ## _AccumCnt) };    \
        TmU64 ZoneTimes[] = { (ZoneName1 ## _AccumCtr), (ZoneName2 ## _AccumCtr) };     \
        char const* ZoneNames[] = { #ZoneName1, #ZoneName2 };                           \
        tmEmitAccumulationZones((HTELEMETRY)global_Granny_TmContext, 0, 2,              \
                                ZoneCounts, ZoneTimes, ZoneNames,                       \
                                0);                                                     \
    } while (false)


#if defined(GRANNY_DEBUG_MEMORY_CALLSTACK) && GRANNY_DEBUG_MEMORY_CALLSTACK
  #define GRANNY_TM_ALLOCEX(ptr, file, line, size, ...)                                               \
      do {                                                                                            \
          TmCallStack cs;                                                                             \
          tmGetCallStack( (HTELEMETRY)global_Granny_TmContext, &cs );                                 \
          tmAllocEx((HTELEMETRY)global_Granny_TmContext, (file), (line), (ptr), (size), ## __VA_ARGS__); \
      } while (false)
#else
  #define GRANNY_TM_ALLOCEX(ptr, file, line, size, ...)                                               \
    tmAllocEx((HTELEMETRY)global_Granny_TmContext, (file), (line), (ptr), (size), ## __VA_ARGS__)
#endif

#define GRANNY_TM_FREE(ptr)  tmFree((HTELEMETRY)global_Granny_TmContext, ptr)

#else // GRANNY_TELEMETRY==0 || !defined(GRANNY_TELEMETRY)

#define GRANNY_AUTO_ZONE(ZoneName) (void)sizeof(#ZoneName)
#define GRANNY_AUTO_ZONE_FN() (void)sizeof(void*)
#define GRANNY_ENTER_ZONE(ZoneName) (void)sizeof(#ZoneName)
#define GRANNY_ENTER_ZONE_FN() (void)sizeof(void*)
#define GRANNY_LEAVE_ZONE() (void)sizeof(void*)

#define GRANNY_AUTO_IDLE_ZONE(name) (void)sizeof(void*)

#define GRANNY_DECL_ACCUM_ZONE(ZoneName) (void)sizeof(#ZoneName)
#define GRANNY_ENTER_ACCUM_ZONE(ZoneName) (void)sizeof(#ZoneName)
#define GRANNY_LEAVE_ACCUM_ZONE(ZoneName) (void)sizeof(#ZoneName)
#define GRANNY_EMIT_ACCUM_ZONE(ZoneName) (void)sizeof(#ZoneName)
#define GRANNY_EMIT_ACCUM_ZONE_2(ZoneName1, ZoneName2) (void)sizeof(#ZoneName1)

#define GRANNY_INC_INT_ACCUMULATOR(Var) (void)sizeof(Var)
#define GRANNY_EMIT_INT_VALUE(Name, Var) (void)sizeof(Var)
#define GRANNY_EMIT_FLOAT(Name, Var) (void)sizeof(Var)
#define GRANNY_EMIT_INT_ACCUMULATOR(Name, Var) (void)sizeof(Var)

#if !defined(COMPILER_SUPPORTS_VAR_MACRO) || !COMPILER_SUPPORTS_VAR_MACRO
// These will cause a warning, but it works.
#define GRANNY_TM_ALLOCEX(ptr, file, line, size) sizeof(#size)
#define GRANNY_TM_MESSAGE(MsgFlag, Fmt) (void)sizeof(Fmt)
#define GRANNY_TM_LOG(Fmt) (void)sizeof(Fmt)
#else
#define GRANNY_TM_ALLOCEX(ptr, file, line, size, fmt, ...) (void)sizeof(#size)
#define GRANNY_TM_MESSAGE(MsgFlag, Fmt, ...) (void)sizeof(Fmt)
#define GRANNY_TM_LOG(Fmt, ...) (void)sizeof(Fmt)
#endif

#define GRANNY_TM_FREE(ptr) (void)sizeof(#ptr)

#endif // GRANNY_TELEMETRY

// What is the difference between a Value and an Accumulator?  Glad you asked!  A value is
// persisted across frames, and may affect behavior.  (Think "CurrentBindings" in the
// animation cache, which affects what happens on Release().)  These are incremented even
// when GRANNY_TELEMETRY is disabled.
// @@ atomic
#define GRANNY_ADD_INT_VALUE(Var, addend)  (Var) += (addend)
#define GRANNY_SUB_INT_VALUE(Var, minuend) (Var) -= (minuend)
#define GRANNY_INC_INT_VALUE(Var) GRANNY_ADD_INT_VALUE(Var, 1)
#define GRANNY_DEC_INT_VALUE(Var) GRANNY_SUB_INT_VALUE(Var, 1)

// @@ #evals
#define GRANNY_NOTE_INT_MAXVALUE(Var, i) ((Var) = (Var) < (i) ? (i) : (Var))



BEGIN_GRANNY_NAMESPACE EXPGROUP(TelemetryGroup);

EXPAPI GS_MODIFY bool UseTmLite(void* Context);
EXPAPI GS_MODIFY bool UseTelemetry(void* Context);
EXPAPI GS_SAFE   void TelemetryFrameStats();
EXPAPI GS_MODIFY void TelemetryComplexStats();

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_TELEMETRY_H
#endif /* GRANNY_TELEMETRY_H */
