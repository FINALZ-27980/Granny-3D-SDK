#ifndef __RADMEMH__
  #define __RADMEMH__

  #ifndef __RADRR_COREH__
    #include "rrCore.h"
  #endif

  RADDEFSTART

  typedef void * (RADLINK * RADMEMALLOC) (U32 bytes);
  typedef void   (RADLINK * RADMEMFREE)  (void * ptr);

// Granny has it's own version of these to prevent link collisions.  Note that Granny
// doesn't define SetMemory, since if you need to hijack Granny's allocation, it goes
// through GrannySetAllocator.
#if ( defined( BUILDING_GRANNY ) && BUILDING_GRANNY ) || ( defined( BUILDING_GRANNY_STATIC ) && BUILDING_GRANNY_STATIC )
  #define radmalloc g_radmalloc
  #define radfree   g_radfree

  RADDEFFUNC void PTR4* RADLINK g_radmalloc(U32 numbytes);
  RADDEFFUNC void RADLINK g_radfree(void PTR4* ptr);
#else
  RADDEFFUNC void RADLINK RADSetMemory(RADMEMALLOC a,RADMEMFREE f);

  #ifdef TELEMEMORY
    #define radmalloc( b ) radmalloci( b, __FILE__, __LINE__ )
    RADDEFFUNC void PTR4* RADLINK radmalloci(U32 numbytes,char const * info, U32 line );
  #else
    RADDEFFUNC void PTR4* RADLINK radmalloc(U32 numbytes);
  #endif

    RADDEFFUNC void RADLINK radfree(void PTR4* ptr);
#endif

  RADDEFEND

#endif
