#include "rrBase.h"

#ifdef _DEBUG
  typedef char bool_size_error[ ( sizeof(rrbool) == 4 ) ? 1 : -1 ];  // make sure our rrbool is 4 bytes
#endif

/*
// CB : enum might be a little better than the rrbool as S32

// no it's not, none of this works rrbool in C++ needs to be interchangeable with bool
//	which pretty much means it must be just an int

// best solution it to just not use bool as much as possible

typedef S32 rrbool;

//-------------------------------------------------

typedef enum { rrfalse=0, rrtrue=1, rrforce32 = 2000000000 } rrboole;

RR_COMPILER_ASSERT( sizeof(rrboole) == 4 );

//-------------------------------------------------

typedef struct { int fakery; } * rrbools; // the int value does do anything.

#define strue ((rrbools)1)
#define sfalse ((rrbools)0)

//-------------------------------------------------

*/
