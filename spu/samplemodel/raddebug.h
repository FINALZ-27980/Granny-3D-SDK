#ifndef __RADDEBUGH__
#define __RADDEBUGH__

#ifndef __RADRR_COREH__
  #include "rrCore.h"
#endif

#define radassert rrassert
#define radassertfail() rrassert(0)

#define BreakPoint() RR_BREAK()

#endif
