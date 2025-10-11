#if !defined(GRANNY_SPU_DMA_UTILITIES_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/cell/spu/granny_spu_dma_utilities.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#ifdef USE_SPEW_EMULATOR
#if !defined(SPEW_DMA_H)
#include "spew_dma.h"
#endif
#else
#include <cell/dma.h>
#endif


BEGIN_GRANNY_NAMESPACE;

template <class T>
struct ClassAligner
{
    ALIGN16(uint8) Buffer[sizeof(T) + 16];

    T* TransferFromNB(uint64x EA, uint32 DMATag)
    {
        int32 ActualTransfer = int32(sizeof(T) + (EA & 0xF));

        cellDmaLargeGet(Buffer, EA & ~((uint64x)0xf), ActualTransfer, DMATag, 0, 0);

        return (T*)(Buffer + (EA & 0xF));
    }

    T* TransferFrom(uint64x EA, uint32 DMATag)
    {
        T* RetVal = TransferFromNB(EA, DMATag);
        cellDmaWaitTagStatusAll(1 << DMATag);
        return RetVal;
    }
};

template <class T, int MaxSize>
struct ArrayAligner
{
    ALIGN16(uint8) Buffer[sizeof(T) * MaxSize + 16];

    T* TransferElementsFrom(uint64x EA, int32x NumElem, uint32 DMATag)
    {
        int32 ActualTransfer = int32(sizeof(T) * NumElem + (EA & 0xF));

        cellDmaLargeGet(Buffer, EA & ~((uint64x)0xf), ActualTransfer, DMATag, 0, 0);
        cellDmaWaitTagStatusAll(1 << DMATag);

        return (T*)(Buffer + (EA & 0xF));
    }
};

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_SPU_DMA_UTILITIES_H
#endif
