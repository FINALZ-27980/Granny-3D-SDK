#if !defined(GRANNY2_SPU_SAMPLEMODEL_H)

/* ========================================================================
   $Notice: $
   ======================================================================== */
#if !defined(GRANNY_CALLBACK) || !defined(GRANNY_DYNLINK) || !defined(GRANNY_DYNLINKDATA)
#undef GRANNY_CALLBACK
#define GRANNY_CALLBACK(ret) ret
#undef GRANNY_DYNLINK
#define GRANNY_DYNLINK(ret) ret
#undef GRANNY_DYNLINKDATA
#define GRANNY_DYNLINKDATA(type) type
#endif

// Assume gcc style __attribute__ packing directives
#define GrannyAlignAttribute __attribute__ ((__packed__))

// This will fire with a negative subscript error if the type sizes
// are set incorrectly by the compiler, and compile out if not.
#define GrannyTypeSizeCheck(expr) \
    typedef int GrannyTypeSizeCheck__[(expr) ? 1 : -1]

#define GRANNY_STRUCT(x) x

// #define GRANNY granny::
// #define USING_GRANNY_NAMESPACE using namespace granny

typedef unsigned long long uint64x;

// #ifdef __cplusplus
// namespace granny {
// #endif


typedef struct radspu_command_queue_spuside radspu_command_queue_spuside;

typedef struct radspu_command radspu_command;
struct radspu_command_queue_spuside;
struct radspu_command;

/* granny_spu_commands_spuside.h bindings: */
GRANNY_DYNLINK(int) GrannySPUMain(radspu_command_queue_spuside *  Queue,
                                  int DMATag);


// Don't export this macro...
#undef GrannyTypeSizeCheck


#undef GrannyAlignAttribute

#define GRANNY2_SPU_SAMPLEMODEL_H
#endif
