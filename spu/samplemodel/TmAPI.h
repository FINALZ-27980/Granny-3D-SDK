// RAD Game Tools

// IMPORTANT!!  Neither this file, nor Telemetry.h is redistributable
//    to end-users (only licensed middleware developers can use it to
//    make TmLite enabled middleware).

#include "telemetry.h"

#define TM_CONTEXT_LITE( cx ) ( (HTELEMETRY)( (UINTa) cx ^ 0x87692345 ) )
#define TM_CONTEXT_FULL( cx ) ( (HTELEMETRY) cx )
