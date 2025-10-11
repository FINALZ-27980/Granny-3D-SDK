#include <revolution/os.h>
#include <revolution/base/PPCArch.h>

#include "rrAtomics.h"

static void comp_barrier( void )
{
  static U32 volatile dummy;  
  dummy = 0;
}


#define CompilerReadWriteBarrier() comp_barrier()
#define CompilerWriteBarrier() comp_barrier()
#define CompilerReadBarrier() comp_barrier()

/*************************************************************************************************/

RADDEFFUNC void RADLINK rrAtomicMemoryBarrierFull( void )
{
  PPCSync();
}

#define AcquireLockBarrier()
#define ReleaseLockBarrier()

#define __lwsync()

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
  BOOL i;
	RR_ASSERT(	(((UINTa)ptr)&7) == 0 );
	CompilerReadBarrier();
  i = OSDisableInterrupts();
	U64 ret = *((volatile U64 *)ptr);
  OSRestoreInterrupts(i);
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
  BOOL i;
	RR_ASSERT(	(((UINTa)ptr)&7) == 0 );
  ReleaseLockBarrier();
  i = OSDisableInterrupts();
	*((volatile U64 *)ptr) = val;
  OSRestoreInterrupts(i);
	CompilerWriteBarrier();
}

// Windows style CmpXchg 
U32 rrAtomicCmpXchg32(U32 volatile * pDestVal, U32 newVal, U32 compareVal)
{
  BOOL i;
  U32 oldVal;
  
  rrAtomicMemoryBarrierFull();
  
  i = OSDisableInterrupts();
  oldVal = *pDestVal;
  if ( oldVal == compareVal )
    *pDestVal = newVal;
  OSRestoreInterrupts(i);

  __lwsync();
  return oldVal;
}

U64 rrAtomicCmpXchg64(U64 volatile * pDestVal, U64 newVal, U64 compareVal)
{
  BOOL i;
  U64 oldVal;

  rrAtomicMemoryBarrierFull();

  i = OSDisableInterrupts();
  oldVal = *pDestVal;
  if ( oldVal == compareVal )
    *pDestVal = newVal;
  OSRestoreInterrupts(i);

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
  BOOL i;
  U32 oldVal;

  rrAtomicMemoryBarrierFull();
  
  i = OSDisableInterrupts();
  oldVal = *pDestVal;
  *pDestVal = newVal;
  OSRestoreInterrupts(i);
 
   __lwsync();
   return oldVal;
}

U64 rrAtomicExchange64(U64 volatile * pDestVal, U64 newVal)
{
  BOOL i;
  U64 oldVal;

  rrAtomicMemoryBarrierFull();
  
  i = OSDisableInterrupts();
  oldVal = *pDestVal;
  *pDestVal = newVal;
  OSRestoreInterrupts(i);
 
   __lwsync();
   return oldVal;
}

// atomically { old = *pDestVal; *pDestVal += incVal; return old; }
U32 rrAtomicAddExchange32(U32 volatile * pDestVal, S32 incVal)
{
  BOOL i;
  U32 oldVal;

  rrAtomicMemoryBarrierFull();

  i = OSDisableInterrupts();
  oldVal = *pDestVal;
  *pDestVal = oldVal + incVal;
  OSRestoreInterrupts(i);

   __lwsync();
   return oldVal;
}

U64 rrAtomicAddExchange64(U64 volatile * pDestVal, S64 incVal)
{
  BOOL i;
  U64 oldVal;

  rrAtomicMemoryBarrierFull();

  i = OSDisableInterrupts();
  oldVal = *pDestVal;
  *pDestVal = oldVal + incVal;
  OSRestoreInterrupts(i);

  __lwsync();
  return oldVal;
}

