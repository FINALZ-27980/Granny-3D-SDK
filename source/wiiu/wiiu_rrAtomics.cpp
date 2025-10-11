#include "rrAtomics.h"
#include <cafe/os.h>
#include <ppc_ghs.h>

// build_ppc.pdf:791 says this is an optimization barrier, and says it can't interpret, so probably won't reorder.
#define CompilerReadWriteBarrier() __asm volatile ("")

// OS Atomic ops are *not* memory barriers!

#define AcquireLockBarrier	__LWSYNC
#define ReleaseLockBarrier	__LWSYNC

#define PreAtomicMemBarrier	__SYNC
#define PostAtomicMemBarrier	__LWSYNC

// helpers to cast away volatile :

RADINLINE U32 * nv(U32 volatile * p) { return (U32 *) p; }
RADINLINE U64 * nv(U64 volatile * p) { return (U64 *) p; }

/*************************************************************************************************/

RADDEFFUNC void RADLINK rrAtomicMemoryBarrierFull( void )
{
    __SYNC();
}

/*************************************************************************************************/

U32 rrAtomicLoadAcquire32(U32 volatile const * ptr)
{
    RR_ASSERT(	(((UINTa)ptr)&3) == 0 );
    U32 ret = *((volatile U32 *)ptr);
    AcquireLockBarrier();
    return ret;
}

U64 rrAtomicLoadAcquire64(U64 volatile const * ptr)
{
    RR_ASSERT(	(((UINTa)ptr)&7) == 0 );
    U64 ret = *((volatile U64 *)ptr);
    AcquireLockBarrier();
    return ret;
}

void rrAtomicStoreRelease32(U32 volatile * ptr,U32 val)
{
    RR_ASSERT(	(((UINTa)ptr)&3) == 0 );
    ReleaseLockBarrier();
    *((volatile U32 *)ptr) = val;
}

void rrAtomicStoreRelease64(U64 volatile * ptr,U64 val)
{
    RR_ASSERT(	(((UINTa)ptr)&7) == 0 );
    ReleaseLockBarrier();
    *((volatile U64 *)ptr) = val;
}

// Windows style CmpXchg 
U32 rrAtomicCmpXchg32(U32 volatile * pDestVal, U32 newVal, U32 compareVal)
{
    rrAtomicMemoryBarrierFull();

    U32 origVal;
    OSCompareAndSwapAtomicEx((OSAtomicVar*)pDestVal, compareVal, newVal, &origVal);
    __LWSYNC();

    return origVal;
}

U64 rrAtomicCmpXchg64(U64 volatile * pDestVal, U64 newVal, U64 compareVal)
{
    rrAtomicMemoryBarrierFull();

    U64 origVal;
    OSCompareAndSwapAtomicEx64((OSAtomicVar64*)pDestVal, compareVal, newVal, &origVal);
    __LWSYNC();

    return origVal;
}


// CAS like C++0x
// atomically { if ( *pDestVal == *pOldVal ) { *pDestVal = newVal; } else { *pOldVal = *pDestVal; } }
rrbool rrAtomicCAS32(U32 volatile * pDestVal, U32 * pOldVal, U32 newVal)
{
    RR_ASSERT( (((UINTa)pDestVal) & (sizeof(U32)-1) ) == 0 );

    PreAtomicMemBarrier();

    U32 old;
    BOOL worked = OSCompareAndSwapAtomicEx((OSAtomicVar*)pDestVal, *pOldVal, newVal, &old);

    if (worked == 0)
        *pOldVal = old;

    PostAtomicMemBarrier();

    return worked;
}

rrbool rrAtomicCAS64(U64 volatile * pDestVal, U64 * pOldVal, U64 newVal)
{
    RR_ASSERT( (((UINTa)pDestVal) & (sizeof(U64)-1) ) == 0 );

    PreAtomicMemBarrier();

    U64 old;
    BOOL worked = OSCompareAndSwapAtomicEx64((OSAtomicVar64*)pDestVal, *pOldVal, newVal, &old);

    if (worked == 0)
        *pOldVal = old;

    PostAtomicMemBarrier();

    return worked;
}

// atomically { *pOldVal = *pDestVal; *pDestVal = newVal; return *pOldVal; };
U32 rrAtomicExchange32(U32 volatile * pDestVal, U32 newVal)
{
    PreAtomicMemBarrier();

    U32 oldVal = OSSwapAtomic((OSAtomicVar*)pDestVal, newVal);

    PostAtomicMemBarrier();

    return oldVal;
}

U64 rrAtomicExchange64(U64 volatile * pDestVal, U64 newVal)
{
    PreAtomicMemBarrier();

    U64 oldVal = OSSwapAtomic64((OSAtomicVar64*)pDestVal, newVal);

    PostAtomicMemBarrier();
    return oldVal;
}

// atomically { old = *pDestVal; *pDestVal += incVal; return old; }
U32 rrAtomicAddExchange32(U32 volatile * pDestVal, S32 incVal)
{
    PreAtomicMemBarrier();
    S32 old = OSAddAtomic((OSAtomicVar*)pDestVal, incVal);
    PostAtomicMemBarrier();

    return old;
}

U64 rrAtomicAddExchange64(U64 volatile * pDestVal, S64 incVal)
{
    PreAtomicMemBarrier();
    S64 old = OSAddAtomic64((OSAtomicVar64*)pDestVal, incVal);
    PostAtomicMemBarrier();

    return old;
}

