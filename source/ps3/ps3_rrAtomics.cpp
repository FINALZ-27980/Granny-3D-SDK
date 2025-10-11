#include "rrAtomics.h"
#include <ppu_intrinsics.h>
#include <cell/atomic.h>

/*************************************************************************************************/
// get intrinsics in global namespace :

/***************************

CB : notes on Atomics and the PS3

PS3 is a single core in-order PPC with two hardware threads
We are mainly worried about the atomics catching interaction between the two hardware threads
but we can also detect memory touches from SPU DMAs (I think)


We're on GCC which offers the __sync built-ins
The __sync builtins appear to offer full sequential consistency

The cell/atomics.h header also provides primitive atomic ops with just ll-sc
  (load linked , store conditional)
 but no memory order constraints


The in-order PPC will not reorder instructions
 but the compiler might
 and the cache load or store buffer or LHS might cause out-of-order memory appearance

Because our two hardware threads are on the same cache, we 
 don't actually have to worry about cache communication issues

=============

CB: big change 07-27-11 :

no longer doing memory sync ops here
the reason is that PPU threads all run on the same cache so no issue

the ops *do* still need to be actually atomic, so make sure variables are aligned,
  and for things like CAS we still need spin loops

WARNING : this means you can't use these for doing atomic ops with the SPU, but you didn't want that anyway
did you?

***************************/

// CB : I'm having trouble finding out whether asm volatile is actually a
//	scheduling barrier.
//	could also use __nop() from ppu_intrinsics which is asm volatile
//	but also not clear if that is an actual scheduling barrier

// #define __nop() __asm__ volatile ("ori 0,0,0" : : : "memory")

//#define CompilerReadWriteBarrier() __asm__ volatile ("ori 0,0,0" : : : "memory")

#ifdef __SNC__
#define CompilerReadWriteBarrier() __builtin_fence()
#else
#define CompilerReadWriteBarrier() __asm__ __volatile__ ("" : : : "memory", "cc")
#endif


/**

CB :

GCC __sync builtins are not well defined
unclear whether they are just atomic or also barriers
should probably change to PPU/atomic.h calls

http://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html
http://gcc.gnu.org/bugzilla/show_bug.cgi?id=36793
http://kerneltrap.org/Linux/Compiler_Misoptimizations
http://gcc.gnu.org/ml/gcc-patches/2008-06/msg01693.html

Unclear if ASM volatile is even a scheduling barrier  !?

http://gcc.gnu.org/bugzilla/show_bug.cgi?id=17884
http://gcc.gnu.org/ml/gcc-patches/2004-10/msg01048.html

**/

/*************************************************************************************************/

#if 1

// just compiler barriers :
//	okay for single core PPC

#define AcquireLockBarrier		CompilerReadWriteBarrier
#define ReleaseLockBarrier		CompilerReadWriteBarrier

#define PreAtomicMemBarrier		CompilerReadWriteBarrier
#define PostAtomicMemBarrier	CompilerReadWriteBarrier

#else

// use real memory barriers :
//	!! you need this for multi-core PPC !!


#define AcquireLockBarrier	__lwsync
#define ReleaseLockBarrier	__lwsync

#define PreAtomicMemBarrier	__sync
#define PostAtomicMemBarrier	__lwsync

#endif

// helpers to cast away volatile :

RADINLINE U32 * nv(U32 volatile * p) { return (U32 *) p; }
RADINLINE U64 * nv(U64 volatile * p) { return (U64 *) p; }

/*************************************************************************************************/

RADDEFFUNC void RADLINK rrAtomicMemoryBarrierFull( void )
{
#ifdef __SNC__
  __sync();
#else
  __asm__ volatile ("sync" : : : "memory");
#endif
}

/*************************************************************************************************/

U32 rrAtomicLoadAcquire32(U32 volatile const * ptr)
{
	RR_ASSERT(	(((UINTa)ptr)&3) == 0 );
	//CompilerReadBarrier();
	//U32 ret = cellAtomicNop32( nv(const_cast<U32 volatile *>(ptr)) );
	U32 ret = *((volatile U32 *)ptr);
    AcquireLockBarrier();
	return ret;
}

U64 rrAtomicLoadAcquire64(U64 volatile const * ptr)
{
	RR_ASSERT(	(((UINTa)ptr)&7) == 0 );
	//CompilerReadBarrier();
	//U64 ret = cellAtomicNop64( nv(const_cast<U64 volatile *>(ptr)) );
	U64 ret = *((volatile U64 *)ptr);
    AcquireLockBarrier();
	return ret;
}

// *ptr = val; // gaurantees earlier stores do not move afterthis
void rrAtomicStoreRelease32(U32 volatile * ptr,U32 val)
{
	RR_ASSERT(	(((UINTa)ptr)&3) == 0 );
    ReleaseLockBarrier();
    //cellAtomicStore32( nv(ptr), val );
	*((volatile U32 *)ptr) = val;
	//CompilerWriteBarrier();
}

void rrAtomicStoreRelease64(U64 volatile * ptr,U64 val)
{
	RR_ASSERT(	(((UINTa)ptr)&7) == 0 );
    ReleaseLockBarrier();
	*((volatile U64 *)ptr) = val;
    //cellAtomicStore64( nv(ptr), val );
	//CompilerWriteBarrier();
}

// Windows style CmpXchg 
U32 rrAtomicCmpXchg32(U32 volatile * pDestVal, U32 newVal, U32 compareVal)
{
	//rrAtomicMemoryBarrierFull();
#ifdef __SNC__	
	U32 oldVal = __builtin_cellAtomicCompareAndSwap32( pDestVal, compareVal, newVal );
#else
	U32 oldVal = cellAtomicCompareAndSwap32( nv(pDestVal), compareVal, newVal );
#endif
	//__lwsync();
	return oldVal;
}

U64 rrAtomicCmpXchg64(U64 volatile * pDestVal, U64 newVal, U64 compareVal)
{
	//rrAtomicMemoryBarrierFull();
#ifdef __SNC__
	U64 oldVal = __builtin_cellAtomicCompareAndSwap64( pDestVal, compareVal, newVal );
#else
	U64 oldVal = cellAtomicCompareAndSwap64( nv(pDestVal), compareVal, newVal );
#endif
	//__lwsync();
	return oldVal;
}


// CAS like C++0x
// atomically { if ( *pDestVal == *pOldVal ) { *pDestVal = newVal; } else { *pOldVal = *pDestVal; } }
rrbool rrAtomicCAS32(U32 volatile * pDestVal, U32 * pOldVal, U32 newVal)
{
	RR_ASSERT( (((UINTa)pDestVal) & (sizeof(U32)-1) ) == 0 );
    
    PreAtomicMemBarrier();
   
	U32 old;
	do
	{
		old = __lwarx(pDestVal);
		if (old != *pOldVal)
		{
			*pOldVal = old;
			PostAtomicMemBarrier(); // @@ ? not sure if this is needed
			return false;
		}
	}
	while ( ! __stwcx(pDestVal, newVal));
	
    PostAtomicMemBarrier();
   
	return true;
}
rrbool rrAtomicCAS64(U64 volatile * pDestVal, U64 * pOldVal, U64 newVal)
{
	RR_ASSERT( (((UINTa)pDestVal) & (sizeof(U64)-1) ) == 0 );
    PreAtomicMemBarrier();
   
	U64 old;
	do
	{
		old = __ldarx(pDestVal);
		if (old != *pOldVal)
		{
			*pOldVal = old;
			PostAtomicMemBarrier(); // @@ ? not sure if this is needed
			return false;
		}
	}
	while ( ! __stdcx(pDestVal, newVal));
	
	// old == *pOldVal
    PostAtomicMemBarrier();
   
	return true;
}

// BTW cellAtomicStore32 is actually an exchange
// atomically { *pOldVal = *pDestVal; *pDestVal = newVal; return *pOldVal; };
U32 rrAtomicExchange32(U32 volatile * pDestVal, U32 newVal)
{
    PreAtomicMemBarrier();
    
    // cellAtomicStore32
    
	U32 oldVal;
	do {
		oldVal = __lwarx(pDestVal);
	} while ( ! __stwcx(pDestVal, newVal) );
  
   PostAtomicMemBarrier();
   
   return oldVal;
}

U64 rrAtomicExchange64(U64 volatile * pDestVal, U64 newVal)
{
    PreAtomicMemBarrier();
   
	U64 oldVal;
	do {
		oldVal = __ldarx(pDestVal);
	} while ( ! __stdcx(pDestVal, newVal) );
  
   PostAtomicMemBarrier();
   return oldVal;
}

// atomically { old = *pDestVal; *pDestVal += incVal; return old; }
U32 rrAtomicAddExchange32(U32 volatile * pDestVal, S32 incVal)
{
#ifdef __SNC__
	U32 oldVal = __builtin_cellAtomicAdd32( pDestVal, incVal );
#else	
	U32 oldVal = cellAtomicAdd32( nv(pDestVal), incVal );
#endif
	return oldVal;
}

U64 rrAtomicAddExchange64(U64 volatile * pDestVal, S64 incVal)
{
#ifdef __SNC__
	U64 oldVal = __builtin_cellAtomicAdd64( pDestVal, incVal );
#else
	U64 oldVal = cellAtomicAdd64( nv(pDestVal), incVal );
#endif
	return oldVal;
}

