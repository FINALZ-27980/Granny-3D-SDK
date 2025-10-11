#ifndef __ARITHBITSH__
#define __ARITHBITSH__

#include "rrCore.h"

// probability bit macros

typedef struct _ARITHBITS
{
  U32 low;
  U32 range;
  U8 * ptr;
  U32 underflow;
  U8 * start; // warning : decoder uses the bottom bit of this as temp space!
} ARITHBITS;


RADDEFFUNC void ArithBitsPutStart( ARITHBITS* ab, void * ptr );

RADDEFFUNC void ArithBitsGetStart( ARITHBITS* ab, void const * ptr );

RADDEFFUNC U8 * ArithBitsStartPtr( ARITHBITS* ab ); // safe version of ->start

// CB: ArithBitsSize is only correct after a Flush
RADDEFFUNC U32 ArithBitsSize( ARITHBITS* ab ); // this is in *bits*
RADDEFFUNC U32 ArithBitsSizeBytes( ARITHBITS* ab );

RADDEFFUNC void ArithBitsFlush( ARITHBITS* ab ); // flush the encoder


// start = low cumfreq , range = hi cumfreq - start , scale = total freq
//	scale must be <= ARITHBITS_CUMPROBMAX
RADDEFFUNC void ArithBitsPut ( ARITHBITS* ab, U32 start, U32 range, U32 scale );
// avoid div if scale is nearly pow 2 ; 2^bits >= scale
RADDEFFUNC void ArithBitsPutBits( ARITHBITS* ab, U32 start, U32 range, U32 bits, U32 scale );

// Value encodes a variable-max integer with equal probabilities
RADDEFFUNC void ArithBitsPutValue ( ARITHBITS* ab, U32 value, U32 scale );
// PutBitsValue : normally scale = 1<<bits
RADDEFFUNC void ArithBitsPutBitsValue ( ARITHBITS* ab, U32 value, U32 bits, U32 scale );

// "Get" returns a cum freq ; you find the symbol then call remove
RADDEFFUNC U32 ArithBitsGet( ARITHBITS* ab, U32 scale );
RADDEFFUNC U32 ArithBitsGetBits( ARITHBITS* ab, U32 bits, U32 scale );
RADDEFFUNC void ArithBitsRemove ( ARITHBITS * ab, U32 start, U32 range, U32 scale );

// for Value , do the "Get" and "Remove" together :
RADDEFFUNC U32 ArithBitsGetValue ( ARITHBITS * ab, U32 scale );
RADDEFFUNC U32 ArithBitsGetBitsValue ( ARITHBITS * ab, U32 bits, U32 scale );

//=======================================================================
//CB091408 some true raw bit IO :

RADDEFFUNC void ArithBitsPutBit( ARITHBITS* ab, int bit );
RADDEFFUNC int ArithBitsGetBit( ARITHBITS* ab );

// similar to ArithBitsPutBitsValue but scale = 1<<bits
RADDEFFUNC void ArithBitsPutBitsValueRaw( ARITHBITS* ab, U32 value, U32 bits );
RADDEFFUNC U32 ArithBitsGetBitsValueRaw( ARITHBITS * ab, U32 bits );

//=======================================================================
//CB091408 for experts only :

RADDEFFUNC void ArithBitsEncRenorm( ARITHBITS* ab );
RADDEFFUNC void ArithBitsDecRenorm( ARITHBITS* ab );

#define ARITHBITS_CODE_BITS			31
#define ARITHBITS_SHIFT_BITS		(ARITHBITS_CODE_BITS - 8)
#define ARITHBITS_CODE_BYTES		((ARITHBITS_CODE_BITS+7)/8)

#define ARITHBITS_PRECISION_BITS	9		// coding is done to this accuracy (in terms of range>>PRECISION_BITS)

#define ARITHBITS_MINRANGE 		((U32)1<<ARITHBITS_SHIFT_BITS)
#define ARITHBITS_ONE   		((U32)1<<ARITHBITS_CODE_BITS)
#define ARITHBITS_CUMPROBMAX  	(ARITHBITS_MINRANGE>>ARITHBITS_PRECISION_BITS)
#define ARITHBITS_CODE_MASK		(ARITHBITS_ONE - 1)

#define ARITHBITS_EXTRA_BITS		(((ARITHBITS_CODE_BITS-1) % 8) + 1)		// == 7	== CODE_BITS - (8*(CODE_BYTES-1))
#define ARITHBITS_TAIL_EXTRA_BITS	(8 - ARITHBITS_EXTRA_BITS)			// == 1
	// extra_bits are the bits in "code" that don't quite fit in bytes


#endif
