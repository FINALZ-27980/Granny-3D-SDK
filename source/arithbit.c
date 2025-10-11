#include "arithbit.h"
#include "raddebug.h"

// this is a byte aligned arithmetic encoder (goofyily called a range encoder)

// CB : ArithBit is almost 100% bitwise identical to my arithc.c

// we start the ptr one value early, because we'll always increment once before the next byte
//   (since we know that we'll never carry, this is safe)

RADDEFFUNC void ArithBitsPutStart( ARITHBITS* ab, void * ptr )
{
  ab->low = 0;
  ab->range = 0x80000000;
  ab->underflow = 0;
  
  ab->ptr = ( (U8*) ptr ) - 1;
  ab->start = (U8*) ptr;
}

#define handle_underflow( value ) \
if ( ab->underflow ) \
{ \
  U32 i; \
  i = ab->underflow; \
  while ( i ) \
  { \
    *ab->ptr++ = value; \
    --i; \
  } \
  ab->underflow = 0; \
}

// this is a little weird, because we actually the buffered output byte *in* the output stream,
//   we might just have to increment it before moving the next byte...
// CB : the convention in here is that ArithBitsEncRenorm should be called
//	*before* each encoding operation, and the state of the coder after an op is left un-normed

RADDEFFUNC void ArithBitsEncRenorm( ARITHBITS* ab )
{
  while ( ab->range <= 0x0800000 )    // ARITHBITS_MINRANGE
  {
    // can we ever affect the buffered byte?
    if ( ab->low < ( 0xFF << 23 ) )	// ARITHBITS_SHIFT_BITS
    {
      // nope, no adjustment, so move on
      ++ab->ptr;
      handle_underflow( 255 );
      *ab->ptr = (U8) ( ab->low >> 23 );	// ARITHBITS_SHIFT_BITS
    }
    else
    {
      // yep, but are we going to carry?
      if ( ab->low & 0x80000000 )	// ARITHBITS_ONE
      {
        // yup, so increment the carry, and then move on
        ++ab->ptr[ 0 ];
        ++ab->ptr;
        handle_underflow( 0 );
        *ab->ptr = (U8) ( ab->low >> 23 );
      }
      else 
      {
        ++ab->underflow;
      }
    }

    // resize the range back up
    ab->range <<= 8;
    ab->low = ( ab->low << 8 ) & 0x7fffffff; // ARITHBITS_CODE_MASK
  }
}
 

RADDEFFUNC void ArithBitsFlush( ARITHBITS* ab )
{
  U32 tmp;

  ArithBitsEncRenorm( ab );

  tmp = ab->low >> 23; 

  // same thing as the put - check for overflow and an increment only if necessary
  if ( tmp > 0xff )
  {
    ++ab->ptr[ 0 ];
    ++ab->ptr;
    handle_underflow( 0 );
  }
  else 
  {
    ++ab->ptr;
    handle_underflow( 255 );
  }
  
  // CB : this flush is a waste of a few bytes in most cases
  //	you can figure out the minimal amount needed to specify the current code
  // see crblib/arithc.c/arithEncodeDoneMinimal
  //	(only works if the decoder reads zeros past the compressed len)
  
  *ab->ptr++ = (U8) tmp;  
  *ab->ptr++ = (U8) ( ab->low >> 15 );  
  *ab->ptr++ = (U8) ( ab->low >> 7 );  
  *ab->ptr++ = (U8) ( ab->low << 1 );  
}


RADDEFFUNC void ArithBitsPut( ARITHBITS* ab, U32 start, U32 range, U32 scale )
{
  U32 r, tmp;

	radassert( start+range <= scale );

  ArithBitsEncRenorm( ab );

  r = ab->range / scale;
  tmp = r * start;
  
  // if there is any extra range, give it to the final symbol
  if ( ( start + range ) < scale )
    ab->range = r * range;
  else
    ab->range -= tmp;

  ab->low += tmp;
}

RADDEFFUNC void ArithBitsPutBits( ARITHBITS* ab, U32 start, U32 range, U32 bits, U32 scale )
{
  U32 r, tmp;

	radassert( start+range <= scale && scale <= (1UL<<bits) );
	
  ArithBitsEncRenorm( ab );

  r = ab->range >> bits;
  tmp = r * start;
  
  // if there is any extra range, give it to the final symbol
  if ( ( start + range ) < scale )
    ab->range = r * range;
  else
    ab->range -= tmp;

  ab->low += tmp;
}

RADDEFFUNC void ArithBitsPutBitsValue ( ARITHBITS* ab, U32 value, U32 bits, U32 scale )
{
  U32 r, tmp;

	radassert( value < scale && scale <= (1UL<<bits) );
	
  ArithBitsEncRenorm( ab );

  r = ab->range >> bits;
  tmp = r * value;
  
  // if there is any extra range, give it to the final symbol
  if ( ( value + 1 ) < scale )
    ab->range = r;
  else
    ab->range -= tmp;

  ab->low += tmp;
}

RADDEFFUNC void ArithBitsPutBitsValueRaw( ARITHBITS* ab, U32 value, U32 bits )
{
  U32 r, tmp;

	radassert( value < (1UL<<bits) );

  ArithBitsEncRenorm( ab );

  r = ab->range >> bits;
  tmp = r * value;
  
  //ASSERT( (value+1) < (1UL<<bits) );
  
  ab->range = r;
  ab->low += tmp;
}


RADDEFFUNC void ArithBitsPutValue ( ARITHBITS* ab, U32 value, U32 scale )
{
  U32 r, tmp;

  radassert( value < scale );
  
  ArithBitsEncRenorm( ab );

  r = ab->range / scale;
  tmp = r * value;
  
  // if there is any extra range, give it to the final symbol
  if ( ( value + 1 ) < scale )
    ab->range = r;
  else
    ab->range -= tmp;

  ab->low += tmp;
}

// ArithBitsSize is in *bits* not bytes
RADDEFFUNC U32 ArithBitsSize( ARITHBITS* ab )
{
	// CB : not including current underflow count ; only accurate after Flush
  return (U32)( ( ( (UINTa) ab->ptr ) - ( (UINTa) ArithBitsStartPtr(ab) ) ) * 8 );
}

RADDEFFUNC U32 ArithBitsSizeBytes( ARITHBITS* ab )
{
	return ArithBitsSize(ab)/8;
}

//=============

RADDEFFUNC void ArithBitsGetStart( ARITHBITS* ab, void const* ptr )
{
  U32 buf;

  radassert( ( ( (UINTa)ptr ) & 3 ) == 0 );

  ab->ptr = ( (U8*) ptr ) + 1;
  buf = ( (U8*) ptr )[ 0 ];

   // we keep the last bit in the low bit of the start pointer
  ab->start = (U8*) ( ( (UINTa) ptr ) + ( buf & 1 ) );
  ab->low = buf >> 1;
  ab->range = 1 << 7;
}

RADDEFFUNC U8 * ArithBitsStartPtr( ARITHBITS* ab ) // safe version of ->start
{
	return (U8*) ( ( ( (UINTa) ab->start ) & ~1 ) );
}

RADDEFFUNC void ArithBitsDecRenorm( ARITHBITS *ab )
{
  U32 range;

  range = ab->range;

  // is the range too small?
  if ( range <= 0x800000 )
  {
    U32 low, buf;
    U8 * ptr;

    low = ab->low;
    // we keep the last bit in the low bit of the start pointer
    buf = (U32)(( (UINTa) ab->start ) & 1);
    ptr = ab->ptr;

    // read the next byte and put it into the range
    do
    {
      low = ( low + low + buf ) << 7;
      buf = *ptr++;
      low |= ( buf >> 1 );
      buf &= 1;
      range <<= 8;
    } while ( range <= 0x800000 );

    ab->low = low;
    // we keep the last bit in the low bit of the start pointer
    ab->start = (U8*) ( ( ( (UINTa) ab->start ) & ~1 ) + buf );
    ab->range = range;
    ab->ptr = ptr;
  }
}

RADDEFFUNC U32 ArithBitsGet( ARITHBITS* ab, U32 scale )
{
  U32 tmp;

  ArithBitsDecRenorm( ab );

	// underflow is used as a temp variable between Get() and Remove() to not repeat the divide
  ab->underflow = ab->range / scale;
  tmp = ab->low / ab->underflow;

  // round off goes to the last symbol
  return( ( tmp >= scale ) ? ( scale - 1 ) : tmp );
}

RADDEFFUNC void ArithBitsRemove( ARITHBITS * ab, U32 start, U32 range, U32 scale )
{
  U32 tmp;

  tmp = ab->underflow * start;
  ab->low -= tmp;

  // round off goes to the last range
  if ( ( start + range ) < scale )
    ab->range = ab->underflow * range;
  else
    ab->range -= tmp;
}

RADDEFFUNC U32 ArithBitsGetValue ( ARITHBITS * ab, U32 scale )
{
  U32 tmp, div, start;

  ArithBitsDecRenorm( ab );

  div = ab->range / scale;
  start = ab->low / div;

  // round off goes to the last symbol
  if ( start >= scale ) 
    start = ( scale - 1 );

  tmp = div * start;
  ab->low -= tmp;

  // round off goes to the last range
  if ( ( start + 1 ) < scale )
    ab->range = div;
  else
    ab->range -= tmp;

  return( start );
}

RADDEFFUNC U32 ArithBitsGetBits( ARITHBITS* ab, U32 bits, U32 scale )
{
  U32 tmp;

  ArithBitsDecRenorm( ab );

  ab->underflow = ab->range >> bits;
  tmp = ab->low / ab->underflow;

  return( ( tmp >= scale ) ? ( scale - 1 ) : tmp );
}

RADDEFFUNC U32 ArithBitsGetBitsValue ( ARITHBITS * ab, U32 bits, U32 scale )
{
  U32 tmp, div, start;

  ArithBitsDecRenorm( ab );

  div = ab->range >> bits;
  start = ab->low / div;

  // round off goes to the last symbol
  if ( start >= scale ) 
    start = ( scale - 1 );

  tmp = div * start;
  ab->low -= tmp;

  // round off goes to the last range
  if ( ( start + 1 ) < scale )
    ab->range = div;
  else
    ab->range -= tmp;

  return( start );
}

RADDEFFUNC U32 ArithBitsGetBitsValueRaw( ARITHBITS * ab, U32 bits )
{
  U32 tmp, div, start;

  ArithBitsDecRenorm( ab );

  div = ab->range >> bits;
  start = ab->low / div;

  radassert( start < (1UL<<bits) );
  
  tmp = div * start;
  ab->low -= tmp;
  ab->range = div;

  return( start );
}

//=======================================================================
//CB091408 some true raw bit IO :

RADDEFFUNC void ArithBitsPutBit( ARITHBITS* ab, int bit )
{	
	U32 r;
	
	ArithBitsEncRenorm(ab);

	r = ab->range>>1;

	if ( bit )
	{
		ab->low		+= r;
		ab->range	-= r;
	}
	else
	{
		ab->range = r;
	}

}

RADDEFFUNC int ArithBitsGetBit( ARITHBITS* ab )
{
	U32 r;

	ArithBitsDecRenorm(ab);
	
	r = ab->range >> 1;

	if ( ab->low >= r )
	{
		ab->low  -= r;
		ab->range -= r;
		return 1;
	}
	else 
	{
		ab->range = r;
		return 0;
	}
}

