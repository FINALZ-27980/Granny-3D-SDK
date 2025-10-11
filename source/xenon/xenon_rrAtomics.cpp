#include "rrAtomics.h"

#undef S8
#define NOD3D
#include <xtl.h>
#include <PPCIntrinsics.h>
#define S8 signed char

/**

CB : note on the Xenon implementation of these :

we are currently implementing full SeqCst atomic ops everywhere to match x86
that adds a lot of inefficiency on Xenon

In most cases AcqRel atomics would work and would be much faster.  Perhaps todo someday.

It's ugly to have to make _Acq , _Rel, _AcqRel, _SeqCst versions of everything though.

**/

/*************************************************************************************************/
// get intrinsics in global namespace :


extern "C"
{
	// these are the *optimizer* barriers, not memory barriers :
	#if _MSC_VER >= 1400
	#define HAS_READ_BARRIER
	void _ReadBarrier(void); // ReadBarrier is VC2005
	#endif
	
	void _WriteBarrier(void);
	void _ReadWriteBarrier(void);
}

// _ReadBarrier, _WriteBarrier, and _ReadWriteBarrier
// these are just *compiler* memory operation barriers, they are NOT fences :

#pragma intrinsic(_WriteBarrier)
#pragma intrinsic(_ReadWriteBarrier)

#define CompilerReadWriteBarrier	_ReadWriteBarrier
#define CompilerWriteBarrier	_WriteBarrier

#ifdef HAS_READ_BARRIER
#pragma intrinsic(_ReadBarrier)	// ReadBarrier is VC2005
#define CompilerReadBarrier		_ReadBarrier	
#else
#define CompilerReadBarrier		_ReadWriteBarrier
#endif

/*************************************************************************************************/

RADDEFFUNC void RADLINK rrAtomicMemoryBarrierFull( void )
{
   __sync(); // Implicit compiler barrier
}

/*************************************************************************************************/

U32 rrAtomicLoadAcquire32(U32 volatile const * ptr)
{
	RR_ASSERT(	(((UINTa)ptr)&3) == 0 );
	CompilerReadBarrier();
	U32 ret = *((volatile U32 *)ptr);
   AcquireLockBarrier();
	return ret;
}

U64 rrAtomicLoadAcquire64(U64 volatile const * ptr)
{
	RR_ASSERT(	(((UINTa)ptr)&7) == 0 );
	CompilerReadBarrier();
	U64 ret = *((volatile U64 *)ptr);
   AcquireLockBarrier();
	return ret;
}

// *ptr = val; // gaurantees earlier stores do not move afterthis
void rrAtomicStoreRelease32(U32 volatile * ptr,U32 val)
{
	RR_ASSERT(	(((UINTa)ptr)&3) == 0 );
   ReleaseLockBarrier();
	*((volatile U32 *)ptr) = val;
	CompilerWriteBarrier();
}

void rrAtomicStoreRelease64(U64 volatile * ptr,U64 val)
{
	RR_ASSERT(	(((UINTa)ptr)&7) == 0 );
   ReleaseLockBarrier();
	*((volatile U64 *)ptr) = val;
	CompilerWriteBarrier();
}

// Windows style CmpXchg 
U32 rrAtomicCmpXchg32(U32 volatile * pDestVal, U32 newVal, U32 compareVal)
{
   rrAtomicMemoryBarrierFull();
	U32 oldVal = InterlockedCompareExchange((LONG *)pDestVal,(LONG)newVal,(LONG)compareVal);
   __lwsync();
   return oldVal;
}

U64 rrAtomicCmpXchg64(U64 volatile * pDestVal, U64 newVal, U64 compareVal)
{
   rrAtomicMemoryBarrierFull();
	U64 oldVal = InterlockedCompareExchange64((__int64 *)pDestVal,(__int64)newVal,(__int64)compareVal);
   __lwsync();
   return oldVal;
}

// CAS like C++0x
// atomically { if ( *pDestVal == *pOldVal ) { *pDestVal = newVal; } else { *pOldVal = *pDestVal; } }
rrbool rrAtomicCAS32(U32 volatile * pDestVal, U32 * pOldVal, U32 newVal)
{
	U32 old = rrAtomicCmpXchg32(pDestVal,newVal,*pOldVal);
	if ( *pOldVal == old ) return true;
	*pOldVal = old;
	return false;
}
rrbool rrAtomicCAS64(U64 volatile * pDestVal, U64 * pOldVal, U64 newVal)
{
	U64 old = rrAtomicCmpXchg64(pDestVal,newVal,*pOldVal);
	if ( *pOldVal == old ) return true;
	*pOldVal = old;
	return false;
}

// atomically { *pOldVal = *pDestVal; *pDestVal = newVal; return *pOldVal; };
U32 rrAtomicExchange32(U32 volatile * pDestVal, U32 newVal)
{
   rrAtomicMemoryBarrierFull();
	U32 oldVal = InterlockedExchange((LONG *)pDestVal,newVal);
   __lwsync();
   return oldVal;
}

U64 rrAtomicExchange64(U64 volatile * pDestVal, U64 newVal)
{
   rrAtomicMemoryBarrierFull();
   U64 oldVal = InterlockedExchange64((__int64 *)pDestVal,newVal);
   __lwsync();
   return oldVal;
}

// atomically { old = *pDestVal; *pDestVal += incVal; return old; }
U32 rrAtomicAddExchange32(U32 volatile * pDestVal, S32 incVal)
{
   rrAtomicMemoryBarrierFull();
	U32 oldVal = InterlockedExchangeAdd((LONG *)pDestVal,incVal);
   __lwsync();
   return oldVal;
}

U64 rrAtomicAddExchange64(U64 volatile * pDestVal, S64 incVal)
{
   rrAtomicMemoryBarrierFull();
   U64 oldVal = InterlockedExchangeAdd64((__int64 *)pDestVal,incVal);
   __lwsync();
   return oldVal;
}

/*

// Xenon :
               
  asm
  long         // <- Zero on failure, one on success (r3).
AtomicCAS(
  long prev,   // -> Previous value (r3).
  long next,   // -> New value (r4).
  void *addr ) // -> Location to update (r5).
{
retry:
  lwarx  r6, 0, r5 // current = *addr;
  cmpw   r6, r3    // if( current != prev )
  bne    fail      // goto fail;
  stwcx. r4, 0, r5 // if( reservation == addr ) *addr = next;
  bne-   retry     // else goto retry;
  li      r3, 1    // Return true.
  blr              // We're outta here.
fail:
  stwcx. r6, 0, r5 // Clear reservation.
  li     r3, 0     // Return false.
  blr              // We're outta here.
}

*/
