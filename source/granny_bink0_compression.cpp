// ========================================================================
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_bink0_compression.h"

#include "granny_assert.h"
#include "granny_compression_tools.h"
#include "granny_conversions.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"

#include "rrMath.h"
#include <math.h>
#include <stdlib.h>

#if PLATFORM_LINUX || PLATFORM_N3DS
#include <stdlib.h>
#endif

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode Undefined_LogMessage
USING_GRANNY_NAMESPACE;

/* ========================================================================
   From binktc.h (Jeff's code)
   ======================================================================== */

// Jeff uses __RADLITTLEENDIAN__, so bridge between that and
// PROCESSOR_LITTLE_ENDIAN
#if PROCESSOR_LITTLE_ENDIAN
#define __RADLITTLEENDIAN__
#endif

//=========================================================================
// input arrays are S16 data with pixels inside -2048 to 2047
//=========================================================================

// check the width and height for validity (usually just make divisible by 16)
static void BinkTC_check_sizes( U32 * width, U32 * height );


//=========================================================================
// input is BinkTC formatted data - output are the S16 planes
//=========================================================================

// decompresses a Bink compressed memory block
static void from_BinkTC( S16** output,
                         U32 planes,
                         void const * bink_buf,
                         U32 width,
                         U32 height,
                         void * temp,
                         U32 temp_size );

// tells how much temp memory to allocate for decompression
static U32 from_BinkTC_temp_mem( void const * binkbuf );


/* ========================================================================
   From encode.h (Jeff's code)
   ======================================================================== */
#define ENCODE_MIN_PLANE_SIZE (64 + 8)

// decode from the compressed data into a decomposed plane
static U32 plane_decode( void const * comp,
                         S16 * output,
                         U32 width,
                         U32 height,
                         U8 * row_mask,
                         void * temp );

/* ========================================================================
   From wavelet.h (Jeff's code)
   ======================================================================== */

#define SMALLEST_DWT_ROW 12
#define SMALLEST_DWT_COL 10

// perform inverse DWT (9/7) wavelet by rows
static void iDWTrow( S16 * dest, S32 pitch, S16 const * input, S32 ppitch, S32 width, S32 height, U8 const * row_mask, S32 starty, S32 subheight );

// perform inverse DWT (9/7) wavelet by columns
static void iDWTcol( S16 * dest, S32 pitch, S16 const * input, S32 ppitch, S32 width, S32 height, S32 starty, S32 subheight );

// perform Harr wavelet by rows
static void iHarrrow( S16 * dest, S32 pitch, S16 const * input, S32 ppitch, S32 width, S32 height, U8 const * row_mask, S32 starty, S32 subheight );

// perform Harr wavelet by columns
static void iHarrcol( S16 * dest, S32 pitch, S16 const * input, S32 ppitch, S32 width, S32 height, S32 starty, S32 subheight );

// perform inverse DWT (9/7) wavelet across two dimensions with a mask (flips to Harr when too small)
static void iDWT2D( S16* buffer, S32 pitch, S32 width, S32 height, U8 const * row_mask, S16* temp );

/* ========================================================================
   From binktc.c (Jeff's code)
   ======================================================================== */
#define iWave2D iDWT2D
#define iWave2D_mask iDWT2D_mask


// how much temp memory to we need during decompression
static U32 from_BinkTC_temp_mem( void const * binkbuf )
{
  return( ( ( U32* ) binkbuf )[ 0 ] );
}

// check the width and height for validity (usually just make divisible by 16)
static void BinkTC_check_sizes( U32 * width, U32 * height )
{
  *width =  ( *width + 15 ) & ~15;
  *height = ( *height + 15 ) & ~15;
}

//=========================================================================
// input is BinkTC formatted data - output is the S16 planes
//=========================================================================

// decompress the Bink texture compressed block
static void from_BinkTC( S16** output, U32 planes, void const * bink_buf, U32 width, U32 height, void * temp, U32 temp_size )
{
  U32 i;
  S16 * tplane;
  U8 * row;

  // make sure we have enough memory to decompress
  i = from_BinkTC_temp_mem( bink_buf );
  if ( ( temp == 0 ) || ( temp_size < i ) )
  {
    temp = 0;
    row = ( U8 *)AllocateSize( i, AllocationTemporary );
    temp_size = i;
  }
  else
  {
    row = ( U8* ) temp;
  }

  // skip the decompressed temp size
  bink_buf = ( ( U32 * ) bink_buf ) + 1;

  tplane = ( S16* ) ( row + ( ( height + 15 ) & ~15 ) );



  for ( i = 0 ; i < planes ; i++ )
  {
    // decode into the planes and skip to the next plane
    bink_buf = ( ( U8* ) bink_buf ) +
      plane_decode( bink_buf, output[ i ], width, height, ( U8* ) temp, tplane );

    // recompose the planes with the wavelet
    iWave2D( output[ i ], width * 2 * 8, width / 8, height / 8, 0, tplane );
    iWave2D( output[ i ], width * 2 * 4, width / 4, height / 4, 0, tplane );
    iWave2D( output[ i ], width * 2 * 2, width / 2, height / 2, 0, tplane );
    iWave2D( output[ i ], width * 2, width, height, ( U8* ) temp, tplane );
  }

  if ( temp == 0 )
  {
    Deallocate( row );
  }
}

/* ========================================================================
   From encode.c (Jeff's code)
   ======================================================================== */
// gets the bitlevel of a int
#define GET_BIT_LEVEL( val )  ( (U32)getbitlevel( val ) )

// literal vs. zero-run settings
#define MIN_ZERO_LENGTH 3
#define LIT_LENGTH_BITS 6
#define ZERO_LENGTH_BITS 8
#define LIT_LENGTH_LIMIT ( ( 1 << LIT_LENGTH_BITS ) - 1 )
#define ZERO_LENGTH_LIMIT ( ( 1 << ZERO_LENGTH_BITS ) - 1 )

#define EXTRA_LENGTHS 4
static U32 extra_lit_lengths[ EXTRA_LENGTHS ] = { 128, 256, 512, 1024 };
static U32 extra_zero_lengths[ EXTRA_LENGTHS ] = { 512, 1024, 2048, 3072 };


// few small debug macros
//#define DEBUG_PLANES
#define SEND_ARITH_MARKER(ari)  { ArithBitsPutValue(ari,43,77); }
#define CHECK_ARITH_MARKER(ari) { U32 v; v = ArithBitsGetValue(ari,77); radassert( v == 43 ); }
#define SEND_VAR_MARKER(vb)     { VarBitsPut(vb,0x8743,16); }
#define CHECK_VAR_MARKER(vb)    { U32 v; VarBitsGet(v, U32, vb,16); radassert( v == 0x8743 ); }


// read the string of escapes
static void read_escapes( ARITHBITS* ab, U8* mask, U32 count )
{
  U32 i;
  U32 zeros;

  zeros = ArithBitsGetValue ( ab, count + 1 );

  Assert( zeros <= count );

  for( i = count ; i-- ; )
  {
    if ( ArithBitsGet( ab, count ) >= zeros )
    {
      *mask++ = 1;
      ArithBitsRemove( ab, zeros, count - zeros, count );
    }
    else
    {
      *mask++ = 0;
      ArithBitsRemove( ab, 0, zeros, count );
    }
  }
}


// fills a rect with a particular value
static void fill_rect( S16 * outp, U32 pixel_pitch, U32 width, U32 height, S32 val )
{
  U32 h, yadj;

  yadj = pixel_pitch - width;

  for( h = height ; h-- ; )
  {
    U32 w;

    for( w = width ; w-- ; )
    {
      *outp++ = (S16) val;
    }

    outp += yadj;
  }
}


//=========================================================================

// decode a low-pass plane (no prediction from other-planes)
static void decode_low( ARITHBITS* ab, VARBITS* vb, S16 * outp, U32 pixel_pitch, U32 enc_width, U32 enc_height, void * temp )
{
  U32 w, h, yadj;
  S32 max, prev;
  U32 num;
  ARITH a;
  U8 * buf;
  S16 * from;

  // See if all bytes are a single value
  if ( VarBitsGet1( *vb, prev ) )  // prev used as temporary
  {
    VarBitsGet( prev, S32, *vb, 16 );
    fill_rect( outp, pixel_pitch, enc_width, enc_height, prev );
    return;
  }

  VarBitsGet( max, S32, *vb, 16 );

  num = max + 1;

  buf = (U8*) temp;

  a = Arith_open( buf, 0, max, num );

  yadj = pixel_pitch - enc_width;

  // get the first pixel with no prediction
  VarBitsGet( prev, U32, *vb, 16 );

  *outp++ = (S16) prev;

  // do the first row just predicting from the left pixel
  for( w = ( enc_width - 1 ) ; w-- ; )
  {
    RAD_UINTa cur = Arith_decompress( a, ab );

    if ( Arith_was_escaped( cur ) )
    {
      U32 escaped;

      escaped = ArithBitsGetValue( ab, num );
      Arith_set_decompressed_symbol( cur, escaped );

      cur = escaped;
    }

    if ( cur )
    {
      U32 tmp;
      S32 v = -( S32 ) VarBitsGet1( *vb, tmp );
      cur = (cur ^ v ) - v;
    }

    //Assert(cur <= Int32Maximum);
    prev = (S32)cur + prev;
    *outp++ = (S16) prev;
  }

  // do the rest of the pixels predicting from the left and upper pixel
  for( h = ( enc_height - 1 ) ; h-- ; )
  {
    RAD_UINTa cur;

    outp += yadj;
    from = outp - pixel_pitch;

    // do the first pixel in the row (predict from the top)
    cur = Arith_decompress( a, ab );
    if ( Arith_was_escaped( cur ) )
    {
      U32 escaped;

      escaped = ArithBitsGetValue( ab, num );
      Arith_set_decompressed_symbol( cur, escaped );

      cur = escaped;
    }
    if ( cur )
    {
      S32 v = -( S32 ) VarBitsGet1( *vb, w ); // w used as temp
      cur = (cur ^ v ) - v;
    }

    //Assert(cur <= Int32Maximum);
    prev = (S32)cur + *from;
    *outp++ = (S16) prev;
    ++from;

    // do the rest of the row
    for( w = ( enc_width - 1 ) ; w-- ; )
    {
      cur = Arith_decompress( a, ab );

      if ( Arith_was_escaped( cur ) )
      {
        U32 escaped;

        escaped = ArithBitsGetValue( ab, num );
        Arith_set_decompressed_symbol( cur, escaped );

        cur = escaped;
      }

      if ( cur )
      {
        U32 tmp;
        S32 v = -( S32 ) VarBitsGet1( *vb, tmp );
        cur = (cur ^ v ) - v;
      }

      //Assert(cur < Int32Maximum);
      prev = (S32)cur + ( ( prev + *from ) / 2 );
      *outp++ = (S16) prev;
      ++from;
    }
  }
}


// allocates the contexts for the bits and the signs
static void * create_decomp_contexts( ARITH** a, ARITH * lits, ARITH * zeros, U32 max, U32 num, U32 numl, void * temp )
{
  U32 i;
  U32 comp_size, lit_comp_size, zero_comp_size;
  U8 * buf;

  comp_size = Arith_decompress_alloc_size( num );
  lit_comp_size = Arith_decompress_alloc_size( LIT_LENGTH_LIMIT + 1 );
  zero_comp_size = Arith_decompress_alloc_size( ZERO_LENGTH_LIMIT + 1 );

  buf = (U8*) temp;

  *a = (ARITH *) buf;

  buf += ( sizeof(ARITH*) * numl );

  // initialize each of the contexts
  for( i = 0 ; i < numl ; i++ )
  {
    (*a)[ i ] = Arith_open( buf, 0, max, num );
    buf += comp_size;
  }

  // init the lit run and zero run contexts
  *lits = Arith_open( buf, 0, LIT_LENGTH_LIMIT, LIT_LENGTH_LIMIT + 1 );
  buf += lit_comp_size;

  *zeros = Arith_open( buf, 0, ZERO_LENGTH_LIMIT, ZERO_LENGTH_LIMIT +1 );
  buf += zero_comp_size;

  return( buf );
}


// decode a high-pass plane (internal plane prediction with order 1)
static void decode_high_1( ARITHBITS* ab, VARBITS* vb, S16 * outp, U32 pixel_pitch, U32 enc_width, U32 enc_height, void * temp )
{
  U32 w, h, yadj;
  S32 max;
  U32 num, numl;
  U32 qlevel;
  ARITH* a;
  ARITH lits, zeros;
  S16 const* from;
  S32 prev, above, above_left;
  RAD_UINTa lit_len = 0, zero_len = 0;

  #ifdef _DEBUG
    S16* out = outp + pixel_pitch;
  #endif

  #ifdef DEBUG_PLANES
  CHECK_ARITH_MARKER( ab );
  CHECK_VAR_MARKER( *vb );
  #endif

  // get the quantization level
  VarBitsGet( qlevel, U32, *vb, 16 );

  // See if all bytes are a single value
  if ( VarBitsGet1( *vb, prev ) ) // use prev as temp
  {
    VarBitsGet( prev, S32, *vb, 16 );

    fill_rect( outp, pixel_pitch, enc_width, enc_height, prev * qlevel );

    return;
  }

  VarBitsGet( max, U32, *vb, 16 );



  num = max + 1;
  numl = GET_BIT_LEVEL( max * qlevel ) + 1;

  // create the arith decoders
  create_decomp_contexts( &a, &lits, &zeros, max, num, numl, temp );

  yadj = pixel_pitch - enc_width;
  h = enc_height;

  // get the first pixel plain
  above = ArithBitsGetValue( ab, num );
  if ( above )
  {
    // get the sign and invert above if the sign was set
    U32 v = -( S32 ) VarBitsGet1( *vb, prev ); // use prev as temp
    above = (above ^ v ) - v;
    above *= qlevel;
  }

  *outp = (S16)above;

  above_left = above;
  prev = above;

  from = outp;
  ++outp;

  if ( enc_width == 1 )
    goto after_first;

  w = enc_width - 1;

  while ( 1 )
  {
    lit_len = Arith_decompress( lits, ab );

    if ( Arith_was_escaped( lit_len ) )
    {
      U32 escaped;

      VarBitsGet( escaped, U32, *vb, LIT_LENGTH_BITS );
      Arith_set_decompressed_symbol( lit_len, escaped );

      lit_len = escaped;
    }

    // handle the escape lengths
    if ( lit_len >= ( LIT_LENGTH_LIMIT - EXTRA_LENGTHS + 1 ) )
    {
      Assert(lit_len - ( LIT_LENGTH_LIMIT - EXTRA_LENGTHS + 1 ) < ArrayLength(extra_lit_lengths));
      lit_len = extra_lit_lengths[ lit_len - ( LIT_LENGTH_LIMIT - EXTRA_LENGTHS + 1 ) ];
    }

    zero_len = Arith_decompress( zeros, ab );

    if ( Arith_was_escaped( zero_len ) )
    {
      U32 escaped;

      VarBitsGet( escaped, U32, *vb, ZERO_LENGTH_BITS );
      Arith_set_decompressed_symbol( zero_len, escaped );

      zero_len = escaped;
    }

    // handle the escape lengths
    if ( zero_len >= ( ZERO_LENGTH_LIMIT - EXTRA_LENGTHS + 1 ) )
    {
        Assert(zero_len - ( ZERO_LENGTH_LIMIT - EXTRA_LENGTHS + 1 ) < ArrayLength(extra_zero_lengths));
        zero_len = (extra_zero_lengths[ zero_len - ( ZERO_LENGTH_LIMIT - EXTRA_LENGTHS + 1 ) ]
                    + MIN_ZERO_LENGTH - 1);
    }
    else
    {
      if ( zero_len )
      {
        zero_len += MIN_ZERO_LENGTH - 1;
      }
    }

    while( lit_len )
    {
      if ( w <= 1 )
      {
        if ( w )
        {
          RAD_SINTa cur;
          U32 context = GET_BIT_LEVEL( ( ( U32 ) abs( prev * 2 ) + abs ( above_left ) + abs( above ) ) / 4 );

          // get the absolute value of the pixel
          cur = Arith_decompress( a[ context ], ab );

          if ( Arith_was_escaped( cur ) )
          {
            U32 escaped;

            escaped = ArithBitsGetValue( ab, num );
            Arith_set_decompressed_symbol( cur, escaped );

            cur = escaped;
          }

          // get the sign bit if we don't have a zero
          if ( cur )
          {
            S32 v = -( S32 ) VarBitsGet1( *vb, w ); // use w as temp
            cur = (cur ^ v ) - v;
            cur *= qlevel;
          }

          *outp++ = (S16) cur;

          --lit_len;
        }

       after_first:
        if ( ( --h ) == 0 )
          return;

        w = enc_width;
        outp += yadj;

        #ifdef _DEBUG
        Assert( outp == out );
        out += pixel_pitch;
        #endif

        from = outp - pixel_pitch;

        above = *from++;
        above_left = above;
        prev = above;
      }
      else
      {
        RAD_SINTa cur;
        S32 above_right = *from;

        U32 context = GET_BIT_LEVEL( ( ( U32 ) abs( prev ) + abs ( above_left ) + abs( above ) + abs( above_right ) ) / 4 );

        // send the absolute value of the pixel
        cur = Arith_decompress( a[ context ], ab );

        if ( Arith_was_escaped( cur ) )
        {
          U32 escaped;

          escaped = ArithBitsGetValue( ab, num );
          Arith_set_decompressed_symbol( cur, escaped );

          cur = escaped;
        }

        // get the sign bit if we don't have zero
        if ( cur )
        {
          S32 v = -( S32 ) VarBitsGet1( *vb, prev ); // use prev as temp
          cur = (cur ^ v ) - v;
          cur *= qlevel;
        }

        *outp = (S16) cur;

        above_left = above;
        above = above_right;

        Assert(cur < Int32Maximum);
        prev = (S32)cur;  // prev has been used as a temp ten lines up!

        ++outp;
        ++from;
        --w;
        --lit_len;
      }
    }

    while ( zero_len )
    {
      if ( zero_len >= w )
      {
        zero_len -= w;
        from += w;

        while ( w-- )
          *outp++ = 0;

        if ( ( --h ) == 0 )
          return;

        w = enc_width;
        outp += yadj;

        #ifdef _DEBUG
        Assert( outp == out );
        out += pixel_pitch;
        #endif

        from = outp - pixel_pitch;

        above = *from++;
        above_left = above;
        prev = above;
      }
      else
      {
        // we know from the test that zero_len < w
        w -= (U32)zero_len;
        from += zero_len;

        do
        {
          *outp++ = 0;
        } while ( --zero_len );

        prev = 0;
        above = from[ -1 ];
        above_left = from[ -2 ];
      }
    }
  }
}


// decode the compressed data into decomposed planes
static U32 plane_decode( void const * comp,
                         S16 * output,
                         U32 width,
                         U32 height,
                         U8* row_mask,
                         void * temp )
{
  ARITHBITS ab;
  VARBITS vb;
  U32 const * sizes = ( U32 const * ) comp;

  // open the probablity bits dumper
  ArithBitsGetStart( &ab, sizes + 2 );

  comp = ( (U8 const*) comp ) + 8 + sizes[ 0 ];

  // open the uncompressed reader
  VarBitsOpen( vb, ( void* ) comp );


  // Tired of this falling over semi-silently every time we get a new
  // platform...  Note that some versions of gcc will generate a
  // subscript out of range warning for the third assert here.  This
  // is because it does not properly trim one branch of the ternary
  // operator.
  Assert ( GET_BIT_LEVEL ( 0x1 ) == 1 );
  Assert ( GET_BIT_LEVEL ( 0x55 ) == 7 );
  // Assert ( GET_BIT_LEVEL ( 0x7fff ) == 15 );


  #ifdef _DEBUG
  SetUInt8(width * height * 2, 0, output);
  #endif

  // do the lowest low pass first
  decode_low( &ab, &vb, output, width * 16, width / 16, height / 16, temp );

  decode_high_1( &ab, &vb, output + ( width / 16 ),                 width * 16, width / 16, height / 16, temp );
  decode_high_1( &ab, &vb, output + ( width * 8 ),                  width * 16, width / 16, height / 16, temp );
  decode_high_1( &ab, &vb, output + ( width / 16 ) + ( width * 8 ), width * 16, width / 16, height / 16, temp );

  decode_high_1( &ab, &vb, output + ( width / 8 ),                 width * 8, width / 8, height / 8, temp );
  decode_high_1( &ab, &vb, output + ( width * 4 ),                 width * 8, width / 8, height / 8, temp );
  decode_high_1( &ab, &vb, output + ( width / 8 ) + ( width * 4 ), width * 8, width / 8, height / 8, temp );

  decode_high_1( &ab, &vb, output + ( width / 4 ),                 width * 4, width / 4, height / 4, temp );
  decode_high_1( &ab, &vb, output + ( width * 2 ),                 width * 4, width / 4, height / 4, temp );
  decode_high_1( &ab, &vb, output + ( width / 4 ) + ( width * 2 ), width * 4, width / 4, height / 4, temp );

  decode_high_1( &ab, &vb, output + ( width / 2 ),                 width * 2, width / 2, height / 2, temp );
  decode_high_1( &ab, &vb, output + width,                         width * 2, width / 2, height / 2, temp );
  decode_high_1( &ab, &vb, output + ( width / 2 ) + width,         width * 2, width / 2, height / 2, temp );

  if ( row_mask )
  {
    read_escapes( &ab, row_mask, height );
  }

  return( sizes[ 0 ] + sizes[ 1 ] + 8 );
}

/* ========================================================================
   From wavelet.c (Jeff's code)
   ======================================================================== */
// uncomment this comment to break when writing unaligned - this is slow!
//#define ENSURE_ALIGNMENT

#if PROCESSOR_X86 && !PLATFORM_IPHONE

#ifdef ENSURE_ALIGNMENT

#define MOVE_8( d, s )                                      \
      if (( ((U32)(d))&7 ) || ( ((U32)(s))&7 )) BREAK_POINT(); \
      { ( ( U32 * )( d ) )[ 0 ] = ( ( U32* ) ( s ) )[ 0 ];  \
        ( ( U32 * )( d ) )[ 1 ] = ( ( U32* ) ( s ) )[ 1 ]; }

#define MOVE_8_d( d1, d2, s )                                                           \
      if (( ((U32)(d2))&7 ) || (((U32)(d1))&7 ) || ( ((U32)(s))&7 )) BREAK_POINT(); \
      { ( ( U32 * )( d1 ) )[ 0 ] = ( ( U32 * )( d2 ) )[ 0 ] = ( ( U32* ) ( s ) )[ 0 ];  \
        ( ( U32 * )( d1 ) )[ 1 ] = ( ( U32 * )( d2 ) )[ 1 ] = ( ( U32* ) ( s ) )[ 1 ]; }

#define MOVE_8_4a( d, s )                                   \
      if (( ((U32)(d))&3 ) || ( ((U32)(s))&3 )) BREAK_POINT(); \
      { ( ( U32 * )( d ) )[ 0 ] = ( ( U32* ) ( s ) )[ 0 ];  \
        ( ( U32 * )( d ) )[ 1 ] = ( ( U32* ) ( s ) )[ 1 ]; }

#define MOVE_8_d_4a( d1, d2, s )                                                        \
      if (( ((U32)(d2))&3 ) || (((U32)(d1))&3 ) || ( ((U32)(s))&3 )) BREAK_POINT(); \
      { ( ( U32 * )( d1 ) )[ 0 ] = ( ( U32 * )( d2 ) )[ 0 ] = ( ( U32* ) ( s ) )[ 0 ];  \
        ( ( U32 * )( d1 ) )[ 1 ] = ( ( U32 * )( d2 ) )[ 1 ] = ( ( U32* ) ( s ) )[ 1 ]; }

#else

#define MOVE_8( d, s )                                      \
      { ( ( U32 * )(void*)( d ) )[ 0 ] = ( ( U32* )(void*) ( s ) )[ 0 ];  \
        ( ( U32 * )(void*)( d ) )[ 1 ] = ( ( U32* )(void*) ( s ) )[ 1 ]; }

#define MOVE_8_d( d1, d2, s )                                                           \
      { ( ( U32 * )(void*)( d1 ) )[ 0 ] = ( ( U32 * )(void*)( d2 ) )[ 0 ] = ( ( U32* )(void*) ( s ) )[ 0 ];  \
        ( ( U32 * )(void*)( d1 ) )[ 1 ] = ( ( U32 * )(void*)( d2 ) )[ 1 ] = ( ( U32* )(void*) ( s ) )[ 1 ]; }

#define MOVE_8_4a( d, s )                                   \
      { ( ( U32 * )(void*)( d ) )[ 0 ] = ( ( U32* ) ( s ) )[ 0 ];  \
        ( ( U32 * )(void*)( d ) )[ 1 ] = ( ( U32* ) ( s ) )[ 1 ]; }

#define MOVE_8_d_4a( d1, d2, s )                                                        \
      { ( ( U32 * )(void*)( d1 ) )[ 0 ] = ( ( U32 * )(void*)( d2 ) )[ 0 ] = ( ( U32* )(void*) ( s ) )[ 0 ];  \
        ( ( U32 * )(void*)( d1 ) )[ 1 ] = ( ( U32 * )(void*)( d2 ) )[ 1 ] = ( ( U32* )(void*) ( s ) )[ 1 ]; }

#endif

#define ALIGN_8_TYPE double __declspec(align(8))

#else

CompileAssert(sizeof(F64) == 8);

#define MOVE_8( d, s )                                      \
        ( ( F64 * )(void*)( d ) )[ 0 ] = ( ( F64* )(void*) ( s ) )[ 0 ];

#define MOVE_8_d( d1, d2, s )                               \
        ( ( F64 * )(void*)(void*)( d1 ) )[ 0 ] = ( ( F64 * )(void*)( d2 ) )[ 0 ] = ( ( F64* )(void*) ( s ) )[ 0 ];

#define MOVE_8_4a( d, s )                                   \
      { ( ( U32 * )(void*)( d ) )[ 0 ] = ( ( U32* )(void*) ( s ) )[ 0 ];  \
        ( ( U32 * )(void*)( d ) )[ 1 ] = ( ( U32* )(void*) ( s ) )[ 1 ]; }

#define MOVE_8_d_4a( d1, d2, s )                                                        \
      { ( ( U32 * )(void*)( d1 ) )[ 0 ] = ( ( U32 * )(void*)( d2 ) )[ 0 ] = ( ( U32* )(void*) ( s ) )[ 0 ];  \
        ( ( U32 * )(void*)( d1 ) )[ 1 ] = ( ( U32 * )(void*)( d2 ) )[ 1 ] = ( ( U32* )(void*) ( s ) )[ 1 ]; }

#define ALIGN_8_TYPE double

#endif

// inverse transforms a row with the DWT (this routine is unrolled to do 4 pixels at once)
static void iDWTrow( S16 * dest, S32 pitch, S16 const * in, S32 ppitch, S32 width, S32 height, U8 const * row_mask, S32 starty, S32 subheight )
{
  U32 y;
  char *out, *hin, *lin;
  U32 halfwidth;

  Assert( width >= SMALLEST_DWT_ROW );

  halfwidth = width / 2;

  out = (char*) dest;
  lin = (char*) in;
  hin = ( (char*) lin ) + width;  // really 2 * halfwidth (which is just width)

  out += starty * pitch;
  hin += starty * ppitch;
  lin += starty * ppitch;
  row_mask += starty;

  for( y = subheight ; y-- ; )
  {
    U32 x;
    S32 next;
    char *xout, *xhin, *xlin;

    struct
    {
      ALIGN_8_TYPE align1;
      S16 lp[ 4 + 3 + 1 ];  // plus 1 to maintain hp's alignment
      S16 hp[ 5 + 3 ];
    } a;

    next = 2;

    xout = out;
    xlin = lin;
    xhin = hin;

    // we read in the first 6 pixels

    // the first 6 pixels are guaranteed linear
    a.lp[ 0 + 1 ] = * ( S16* ) xlin;
    a.lp[ -1 + 1 ] = a.lp[ 1 + 1 ] = * ( S16* ) ( xlin + 2 );
    a.lp[ 2 + 1 ] = * ( S16* ) ( xlin + 4 );
    a.lp[ 2 + 1 + 1 ] = * ( S16* ) ( xlin + 6 );
    a.lp[ 2 + 1 + 2 ] = * ( S16* ) ( xlin + 8 );
    a.lp[ 2 + 1 + 3 ] = * ( S16* ) ( xlin + 10 );
    xlin += 12;

    MOVE_8_4a( &a.hp[ 0 + 2 ], xhin );
    a.hp[ 2 + 2 + 2 ] =  * ( S16* ) ( xhin + 8 );
    a.hp[ 2 + 2 + 3 ] =  * ( S16* ) ( xhin + 10 );
    a.hp[ -2 + 2 ] = a.hp[ 1 + 2 ];
    a.hp[ -1 + 2 ] = a.hp[ 0 + 2 ];
    xhin += 12;

    x = ( halfwidth < 8 ) ? 0 : ( ( halfwidth - 8 ) / 4 );

    if ( ( ( ( RAD_UINTa ) row_mask ) <= (U32)height ) || ( *row_mask ) )
    {
      // do groups of four pixels
      while(  x-- )
      {
        S32 e1, o1, e2, o2, e3, o3, e4, o4;

        e1 =
          (     ( a.lp[ 0 + 1 ] * 51674 )
            - ( ( a.lp[ -1 + 1 ] + a.lp[ 1 + 1 ] ) * 2667 )
            - ( ( a.hp[ -2 + 2 ] + a.hp[ 1 + 2 ] ) * 1563 )
            + ( ( a.hp[ -1 + 2 ] + a.hp[ 0 + 2 ] ) * 24733 )
          );

        o1 =
          ( (   ( a.lp[ 0 + 1 ] + a.lp[ 1 + 1 ] ) * 27400 )
            - ( ( a.lp[ -1 + 1 ] + a.lp[ 2 + 1 ] ) * 4230 )
            -   ( a.hp[ 0 + 2 ] * 55882 )
            - ( ( a.hp[ -2 + 2 ] + a.hp[ 2 + 2 ] ) * 2479 )
            + ( ( a.hp[ -1 + 2 ] + a.hp[ 1 + 2 ] ) * 7250 )
          );

        e2 =
          (     ( a.lp[ 0 + 1 + 1 ] * 51674 )
            - ( ( a.lp[ -1 + 1 + 1 ] + a.lp[ 1 + 1 + 1 ] ) * 2667 )
            - ( ( a.hp[ -2 + 2 + 1 ] + a.hp[ 1 + 2 + 1 ] ) * 1563 )
            + ( ( a.hp[ -1 + 2 + 1 ] + a.hp[ 0 + 2 + 1 ] ) * 24733 )
          );

        o2 =
          ( (   ( a.lp[ 0 + 1 + 1 ] + a.lp[ 1 + 1 + 1 ] ) * 27400 )
            - ( ( a.lp[ -1 + 1 + 1 ] + a.lp[ 2 + 1 + 1 ] ) * 4230 )
            -   ( a.hp[ 0 + 2 + 1 ] * 55882 )
            - ( ( a.hp[ -2 + 2 + 1 ] + a.hp[ 2 + 2 + 1 ] ) * 2479 )
            + ( ( a.hp[ -1 + 2 + 1 ] + a.hp[ 1 + 2 + 1 ] ) * 7250 )
          );

        e3 =
          (     ( a.lp[ 0 + 1 + 2 ] * 51674 )
            - ( ( a.lp[ -1 + 1 + 2 ] + a.lp[ 1 + 1 + 2 ] ) * 2667 )
            - ( ( a.hp[ -2 + 2 + 2 ] + a.hp[ 1 + 2 + 2 ] ) * 1563 )
            + ( ( a.hp[ -1 + 2 + 2 ] + a.hp[ 0 + 2 + 2 ] ) * 24733 )
          );

        o3 =
          ( (   ( a.lp[ 0 + 1 + 2 ] + a.lp[ 1 + 1 + 2 ] ) * 27400 )
            - ( ( a.lp[ -1 + 1 + 2 ] + a.lp[ 2 + 1 + 2 ] ) * 4230 )
            -   ( a.hp[ 0 + 2 + 2 ] * 55882 )
            - ( ( a.hp[ -2 + 2 + 2 ] + a.hp[ 2 + 2 + 2 ] ) * 2479 )
            + ( ( a.hp[ -1 + 2 + 2 ] + a.hp[ 1 + 2 + 2 ] ) * 7250 )
          );

        e4 =
          (     ( a.lp[ 0 + 1 + 3 ] * 51674 )
            - ( ( a.lp[ -1 + 1 + 3 ] + a.lp[ 1 + 1 + 3 ] ) * 2667 )
            - ( ( a.hp[ -2 + 2 + 3 ] + a.hp[ 1 + 2 + 3 ] ) * 1563 )
            + ( ( a.hp[ -1 + 2 + 3 ] + a.hp[ 0 + 2 + 3 ] ) * 24733 )
          );

        o4 =
          ( (   ( a.lp[ 0 + 1 + 3 ] + a.lp[ 1 + 1 + 3 ] ) * 27400 )
            - ( ( a.lp[ -1 + 1 + 3 ] + a.lp[ 2 + 1 + 3 ] ) * 4230 )
            -   ( a.hp[ 0 + 2 + 3 ] * 55882 )
            - ( ( a.hp[ -2 + 2 + 3 ] + a.hp[ 2 + 2 + 3 ] ) * 2479 )
            + ( ( a.hp[ -1 + 2 + 3 ] + a.hp[ 1 + 2 + 3 ] ) * 7250 )
          );

        // round up and truncate back to 16-bit
        e1 = ( e1 + ( 32767 ^ ( e1 >> 31 ) ) ) / 65536;
        o1 = ( o1 + ( 32767 ^ ( o1 >> 31 ) ) ) / 65536;
        e2 = ( e2 + ( 32767 ^ ( e2 >> 31 ) ) ) / 65536;
        o2 = ( o2 + ( 32767 ^ ( o2 >> 31 ) ) ) / 65536;
        e3 = ( e3 + ( 32767 ^ ( e3 >> 31 ) ) ) / 65536;
        o3 = ( o3 + ( 32767 ^ ( o3 >> 31 ) ) ) / 65536;
        e4 = ( e4 + ( 32767 ^ ( e4 >> 31 ) ) ) / 65536;
        o4 = ( o4 + ( 32767 ^ ( o4 >> 31 ) ) ) / 65536;

  #ifdef __RADLITTLEENDIAN__
        ( (S32*) xout )[ 0 ] = ( o1 << 16 ) | ( e1 & 0xffff );
        ( (S32*) xout )[ 1 ] = ( o2 << 16 ) | ( e2 & 0xffff );
        ( (S32*) xout )[ 2 ] = ( o3 << 16 ) | ( e3 & 0xffff );
        ( (S32*) xout )[ 3 ] = ( o4 << 16 ) | ( e4 & 0xffff );
  #else
        ( (S32*) xout )[ 0 ] = ( e1 << 16 ) | ( o1 & 0xffff );
        ( (S32*) xout )[ 1 ] = ( e2 << 16 ) | ( o2 & 0xffff );
        ( (S32*) xout )[ 2 ] = ( e3 << 16 ) | ( o3 & 0xffff );
        ( (S32*) xout )[ 3 ] = ( e4 << 16 ) | ( o4 & 0xffff );
  #endif

        xout += 16;

        ( ( S32* ) &a.lp[ 0 ] )[ 0 ] = ( ( S32* ) ( &a.lp[ 4 ] ) )[ 0 ];
        a.lp[ 2 ] = a.lp[ 6 ];
        a.lp[ 3 ] = * ( S16* ) xlin;  // can't block copy since these two addresses are unaligned
        a.lp[ 4 ] = * ( S16* ) ( xlin + 2 );
        a.lp[ 5 ] = * ( S16* ) ( xlin + 4 );
        a.lp[ 6 ] = * ( S16* ) ( xlin + 6 );

        MOVE_8( &a.hp[ 0 ], &a.hp[ 4 ] );
        MOVE_8_4a( &a.hp[ 4 ], xhin );

        xlin += 8;
        xhin += 8;
      }

      // do the remenants
      x = ( halfwidth < 8 ) ? halfwidth : ( ( halfwidth & 3 ) + 8 );

      while ( x-- )
      {
        S32 e, o;

        if ( xlin == hin )
        {
          xlin = xlin - next;
          xhin = xhin - next - next;
          next = -next;
        }

        e =
          (     ( a.lp[ 0 + 1 ] * 51674 )
            - ( ( a.lp[ -1 + 1 ] + a.lp[ 1 + 1 ] ) * 2667 )
            - ( ( a.hp[ -2 + 2 ] + a.hp[ 1 + 2 ] ) * 1563 )
            + ( ( a.hp[ -1 + 2 ] + a.hp[ 0 + 2 ] ) * 24733 )
          );

        o =
          ( (   ( a.lp[ 0 + 1 ] + a.lp[ 1 + 1 ] ) * 27400 )
            - ( ( a.lp[ -1 + 1 ] + a.lp[ 2 + 1 ] ) * 4230 )
            -   ( a.hp[ 0 + 2 ] * 55882 )
            - ( ( a.hp[ -2 + 2 ] + a.hp[ 2 + 2 ] ) * 2479 )
            + ( ( a.hp[ -1 + 2 ] + a.hp[ 1 + 2 ] ) * 7250 )
          );

        // round up and truncate back to 16-bit
        e = ( e + ( 32767 ^ ( e >> 31 ) ) ) / 65536;
        o = ( o + ( 32767 ^ ( o >> 31 ) ) ) / 65536;

  #ifdef __RADLITTLEENDIAN__
        ( (S32*) xout )[ 0 ] = ( o << 16 ) | ( e & 0xffff );
  #else
        ( (S32*) xout )[ 0 ] = ( e << 16 ) | ( o & 0xffff );
  #endif

        xout += 2 + 2;

        Assert( xlin >= lin );
        Assert( xhin >= ( hin - 2) );
        Assert( xlin < ( lin + width ) );
        Assert( xhin < ( hin + width ) );

        a.lp[ 0 ] = a.lp[ 1 ];
        a.lp[ 1 ] = a.lp[ 2 ];
        a.lp[ 2 ] = a.lp[ 3 ];
        a.lp[ 3 ] = a.lp[ 4 ];
        a.lp[ 4 ] = a.lp[ 5 ];
        a.lp[ 5 ] = a.lp[ 6 ];
        a.lp[ 6 ] = * ( S16* ) xlin;

        a.hp[ 0 ] = a.hp[ 1 ];
        a.hp[ 1 ] = a.hp[ 2 ];
        a.hp[ 2 ] = a.hp[ 3 ];
        a.hp[ 3 ] = a.hp[ 4 ];
        a.hp[ 4 ] = a.hp[ 5 ];
        a.hp[ 5 ] = a.hp[ 6 ];
        a.hp[ 6 ] = a.hp[ 7 ];
        a.hp[ 7 ] = * ( S16* ) xhin;

        xlin += next;
        xhin += next;
      }
    }
    else
    {
      // do a zero row

      // do groups of four pixels
      while(  x-- )
      {
        S32 e1, o1, e2, o2, e3, o3, e4, o4;

        e1 =
          (     ( a.lp[ 0 + 1 ] * 51674 )
            - ( ( a.lp[ -1 + 1 ] + a.lp[ 1 + 1 ] ) * 2667 )
          );

        o1 =
          ( (   ( a.lp[ 0 + 1 ] + a.lp[ 1 + 1 ] ) * 27400 )
            - ( ( a.lp[ -1 + 1 ] + a.lp[ 2 + 1 ] ) * 4230 )
          );

        e2 =
          (     ( a.lp[ 0 + 1 + 1 ] * 51674 )
            - ( ( a.lp[ -1 + 1 + 1 ] + a.lp[ 1 + 1 + 1 ] ) * 2667 )
          );

        o2 =
          ( (   ( a.lp[ 0 + 1 + 1 ] + a.lp[ 1 + 1 + 1 ] ) * 27400 )
            - ( ( a.lp[ -1 + 1 + 1 ] + a.lp[ 2 + 1 + 1 ] ) * 4230 )
          );

        e3 =
          (     ( a.lp[ 0 + 1 + 2 ] * 51674 )
            - ( ( a.lp[ -1 + 1 + 2 ] + a.lp[ 1 + 1 + 2 ] ) * 2667 )
          );

        o3 =
          ( (   ( a.lp[ 0 + 1 + 2 ] + a.lp[ 1 + 1 + 2 ] ) * 27400 )
            - ( ( a.lp[ -1 + 1 + 2 ] + a.lp[ 2 + 1 + 2 ] ) * 4230 )
          );

        e4 =
          (     ( a.lp[ 0 + 1 + 3 ] * 51674 )
            - ( ( a.lp[ -1 + 1 + 3 ] + a.lp[ 1 + 1 + 3 ] ) * 2667 )
          );

        o4 =
          ( (   ( a.lp[ 0 + 1 + 3 ] + a.lp[ 1 + 1 + 3 ] ) * 27400 )
            - ( ( a.lp[ -1 + 1 + 3 ] + a.lp[ 2 + 1 + 3 ] ) * 4230 )
          );


        // round up and truncate back to 16-bit
        e1 = ( e1 + ( 32767 ^ ( e1 >> 31 ) ) ) / 65536;
        o1 = ( o1 + ( 32767 ^ ( o1 >> 31 ) ) ) / 65536;
        e2 = ( e2 + ( 32767 ^ ( e2 >> 31 ) ) ) / 65536;
        o2 = ( o2 + ( 32767 ^ ( o2 >> 31 ) ) ) / 65536;
        e3 = ( e3 + ( 32767 ^ ( e3 >> 31 ) ) ) / 65536;
        o3 = ( o3 + ( 32767 ^ ( o3 >> 31 ) ) ) / 65536;
        e4 = ( e4 + ( 32767 ^ ( e4 >> 31 ) ) ) / 65536;
        o4 = ( o4 + ( 32767 ^ ( o4 >> 31 ) ) ) / 65536;

  #ifdef __RADLITTLEENDIAN__
        ( (S32*) xout )[ 0 ] = ( o1 << 16 ) | ( e1 & 0xffff );
        ( (S32*) xout )[ 1 ] = ( o2 << 16 ) | ( e2 & 0xffff );
        ( (S32*) xout )[ 2 ] = ( o3 << 16 ) | ( e3 & 0xffff );
        ( (S32*) xout )[ 3 ] = ( o4 << 16 ) | ( e4 & 0xffff );
  #else
        ( (S32*) xout )[ 0 ] = ( e1 << 16 ) | ( o1 & 0xffff );
        ( (S32*) xout )[ 1 ] = ( e2 << 16 ) | ( o2 & 0xffff );
        ( (S32*) xout )[ 2 ] = ( e3 << 16 ) | ( o3 & 0xffff );
        ( (S32*) xout )[ 3 ] = ( e4 << 16 ) | ( o4 & 0xffff );
  #endif

        xout += 16;

        ( ( S32* ) &a.lp[ 0 ] )[ 0 ] = ( ( S32* ) ( &a.lp[ 4 ] ) )[ 0 ];
        a.lp[ 2 ] = a.lp[ 6 ];
        a.lp[ 3 ] = * ( S16* ) xlin;  // can't dword copy since these two addresses are unaligned
        a.lp[ 4 ] = * ( S16* ) ( xlin + 2 );
        a.lp[ 5 ] = * ( S16* ) ( xlin + 4 );
        a.lp[ 6 ] = * ( S16* ) ( xlin + 6 );

        xlin += 8;
      }

      // do the remenants
      x = ( halfwidth < 8 ) ? halfwidth : ( ( halfwidth & 3 ) + 8 );

      while ( x-- )
      {
        S32 e, o;

        if ( xlin == hin )
        {
          xlin = xlin - 2;
          next = -2;
        }

        e =
          (     ( a.lp[ 0 + 1 ] * 51674 )
            - ( ( a.lp[ -1 + 1 ] + a.lp[ 1 + 1 ] ) * 2667 )
          );

        o =
          ( (   ( a.lp[ 0 + 1 ] + a.lp[ 1 + 1 ] ) * 27400 )
            - ( ( a.lp[ -1 + 1 ] + a.lp[ 2 + 1 ] ) * 4230 )
          );

        // round up and truncate back to 16-bit
        e = ( e + ( 32767 ^ ( e >> 31 ) ) ) / 65536;
        o = ( o + ( 32767 ^ ( o >> 31 ) ) ) / 65536;

  #ifdef __RADLITTLEENDIAN__
        ( (S32*) xout )[ 0 ] = ( o << 16 ) | ( e & 0xffff );
  #else
        ( (S32*) xout )[ 0 ] = ( e << 16 ) | ( o & 0xffff );
  #endif

        xout += 2 + 2;

        a.lp[ 0 ] = a.lp[ 1 ];
        a.lp[ 1 ] = a.lp[ 2 ];
        a.lp[ 2 ] = a.lp[ 3 ];
        a.lp[ 3 ] = a.lp[ 4 ];
        a.lp[ 4 ] = a.lp[ 5 ];
        a.lp[ 5 ] = a.lp[ 6 ];
        a.lp[ 6 ] = * ( S16* ) xlin;

        xlin += next;
      }
    }

    out += pitch;
    hin += ppitch;
    lin += ppitch;
    ++row_mask;
  }
}


// inverse transforms a column with the DWT (this routine is unrolled to do 4 columns at once)
static void iDWTcol( S16 * dest, S32 pitch, S16 const * input, S32 ppitch, S32 width, S32 height, S32 starty, S32 subheight )
{
  U32 x;
  char *out;
  char const *lin;
  char const *hin;
  char const *lend;
  U32 halfheight;

  Assert( height >= SMALLEST_DWT_COL );

  halfheight = subheight / 2;

  out = (char*) dest;
  lin = (char const *) input;
  hin = lin + ppitch;
  lend = lin + ( ppitch * height );

  ppitch *= 2;

  if ( starty )
  {
    out += starty * pitch;
    lin += ( ( starty / 2 ) - 1 ) * ppitch;
    hin += ( ( starty / 2 ) - 2 ) * ppitch;
  }

  for( x = ( width / 4 ) ; x-- ; )
  {
    U32 y;
    S32 next;
    char *yout;
    char const *yhin;
    char const *ylin;

    struct
    {
      ALIGN_8_TYPE align;
      S16 lp[ 16 + 4 ][ 4 ];
      S16 hp[ 16 + 5 ][ 4 ];
    } a;
    S16 ( * lp )[ 4 ];
    S16 ( * hp )[ 4 ];

    lp = &a.lp[ 0 ];
    hp = &a.hp[ 0 ];

    next = ppitch;

    yout = out;
    ylin = lin;
    yhin = hin;

    if ( starty )
    {
      MOVE_8( &a.lp[ -1 + 1 ][ 0 ], ylin );
      ylin += next;
      MOVE_8( &a.hp[ -2 + 2 ][ 0 ], yhin );
      yhin += next;
      MOVE_8( &a.lp[ 0 + 1 ][ 0 ], ylin );
      ylin += next;
      MOVE_8( &a.hp[ -1 + 2 ][ 0 ], yhin );
      yhin += next;
      MOVE_8( &a.lp[ 1 + 1 ][ 0 ], ylin );
      ylin += next;
      MOVE_8( &a.hp[ 0 + 2 ][ 0 ], yhin );
      yhin += next;
      MOVE_8( &a.lp[ 2 + 1 ][ 0 ], ylin );
      ylin += next;
      MOVE_8( &a.hp[ 1 + 2 ][ 0 ], yhin );
      yhin += next;
      MOVE_8( &a.hp[ 2 + 2 ][ 0 ], yhin );
      yhin += next;
    }
    else
    {
      MOVE_8( &a.lp[ 0 + 1 ][ 0 ], ylin );
      ylin += next;
      MOVE_8_d( &a.lp[ -1 + 1 ][ 0 ], &a.lp[ 1 + 1 ][ 0 ], ylin );
      ylin += next;
      MOVE_8( &a.lp[ 2 + 1 ][ 0 ], ylin );
      ylin += next;
      MOVE_8_d( &a.hp[ -1 + 2 ][ 0 ], &a.hp[ 0 + 2 ][ 0 ], yhin );
      yhin += next;
      MOVE_8_d( &a.hp[ -2 + 2 ][ 0 ], &a.hp[ 1 + 2 ][ 0 ], yhin );
      yhin += next;
      MOVE_8( &a.hp[ 2 + 2 ][ 0 ], yhin );
      yhin += next;
    }

    for( y = halfheight ; y-- ; )
    {
      S32 e1, o1, e2, o2, e3, o3, e4, o4;


      if ( ylin == lend )
      {
        ylin = ylin - next;
        yhin = yhin - next - next;
        next = -next;
      }

      e1 =
        (     ( ( S32 ) lp[ 0 + 1 ][ 0 ] * 51674 )
          - ( ( ( S32 ) lp[ -1 + 1 ][ 0 ] + ( S32 ) lp[ 1 + 1 ][ 0 ] ) * 2667 )
          - ( ( ( S32 ) hp[ -2 + 2 ][ 0 ] + ( S32 ) hp[ 1 + 2 ][ 0 ] ) * 1563 )
          + ( ( ( S32 ) hp[ -1 + 2 ][ 0 ] + ( S32 ) hp[ 0 + 2 ][ 0 ] ) * 24733 )
        );

      e2 =
        (     ( ( S32 ) lp[ 0 + 1 ][ 1 ] * 51674 )
          - ( ( ( S32 ) lp[ -1 + 1 ][ 1 ] + ( S32 ) lp[ 1 + 1 ][ 1 ] ) * 2667 )
          - ( ( ( S32 ) hp[ -2 + 2 ][ 1 ] + ( S32 ) hp[ 1 + 2 ][ 1 ] ) * 1563 )
          + ( ( ( S32 ) hp[ -1 + 2 ][ 1 ] + ( S32 ) hp[ 0 + 2 ][ 1 ] ) * 24733 )
        );

      e3 =
        (     ( ( S32 ) lp[ 0 + 1 ][ 2 ] * 51674 )
          - ( ( ( S32 ) lp[ -1 + 1 ][ 2 ] + ( S32 ) lp[ 1 + 1 ][ 2 ] ) * 2667 )
          - ( ( ( S32 ) hp[ -2 + 2 ][ 2 ] + ( S32 ) hp[ 1 + 2 ][ 2 ] ) * 1563 )
          + ( ( ( S32 ) hp[ -1 + 2 ][ 2 ] + ( S32 ) hp[ 0 + 2 ][ 2 ] ) * 24733 )
        );

      e4 =
        (     ( ( S32 ) lp[ 0 + 1 ][ 3 ] * 51674 )
          - ( ( ( S32 ) lp[ -1 + 1 ][ 3 ] + ( S32 ) lp[ 1 + 1 ][ 3 ] ) * 2667 )
          - ( ( ( S32 ) hp[ -2 + 2 ][ 3 ] + ( S32 ) hp[ 1 + 2 ][ 3 ] ) * 1563 )
          + ( ( ( S32 ) hp[ -1 + 2 ][ 3 ] + ( S32 ) hp[ 0 + 2 ][ 3 ] ) * 24733 )
        );

      o1 =
        ( (   ( ( S32 ) lp[ 0 + 1 ][ 0 ] + ( S32 ) lp[ 1 + 1 ][ 0 ] ) * 27400 )
          - ( ( ( S32 ) lp[ -1 + 1 ][ 0 ] + ( S32 ) lp[ 2 + 1 ][ 0 ] ) * 4230 )
          -   ( ( S32 ) hp[ 0 + 2 ][ 0 ] * 55882 )
          - ( ( ( S32 ) hp[ -2 + 2 ][ 0 ] + ( S32 ) hp[ 2 + 2 ][ 0 ] ) * 2479 )
          + ( ( ( S32 ) hp[ -1 + 2 ][ 0 ] + ( S32 ) hp[ 1 + 2 ][ 0 ] ) * 7250 )
        );

      o2 =
        ( (   ( ( S32 ) lp[ 0 + 1 ][ 1 ] + ( S32 ) lp[ 1 + 1 ][ 1 ] ) * 27400 )
          - ( ( ( S32 ) lp[ -1 + 1 ][ 1 ] + ( S32 ) lp[ 2 + 1 ][ 1 ] ) * 4230 )
          -   ( ( S32 ) hp[ 0 + 2 ][ 1 ] * 55882 )
          - ( ( ( S32 ) hp[ -2 + 2 ][ 1 ] + ( S32 ) hp[ 2 + 2 ][ 1 ] ) * 2479 )
          + ( ( ( S32 ) hp[ -1 + 2 ][ 1 ] + ( S32 ) hp[ 1 + 2 ][ 1 ] ) * 7250 )
        );

      o3 =
        ( (   ( ( S32 ) lp[ 0 + 1 ][ 2 ] + ( S32 ) lp[ 1 + 1 ][ 2 ] ) * 27400 )
          - ( ( ( S32 ) lp[ -1 + 1 ][ 2 ] + ( S32 ) lp[ 2 + 1 ][ 2 ] ) * 4230 )
          -   ( ( S32 ) hp[ 0 + 2 ][ 2 ] * 55882 )
          - ( ( ( S32 ) hp[ -2 + 2 ][ 2 ] + ( S32 ) hp[ 2 + 2 ][ 2 ] ) * 2479 )
          + ( ( ( S32 ) hp[ -1 + 2 ][ 2 ] + ( S32 ) hp[ 1 + 2 ][ 2 ] ) * 7250 )
        );

      o4 =
        ( (   ( ( S32 ) lp[ 0 + 1 ][ 3 ] + ( S32 ) lp[ 1 + 1 ][ 3 ] ) * 27400 )
          - ( ( ( S32 ) lp[ -1 + 1 ][ 3 ] + ( S32 ) lp[ 2 + 1 ][ 3 ] ) * 4230 )
          -   ( ( S32 ) hp[ 0 + 2 ][ 3 ] * 55882 )
          - ( ( ( S32 ) hp[ -2 + 2 ][ 3 ] + ( S32 ) hp[ 2 + 2 ][ 3 ] ) * 2479 )
          + ( ( ( S32 ) hp[ -1 + 2 ][ 3 ] + ( S32 ) hp[ 1 + 2 ][ 3 ] ) * 7250 )
        );

      // round up and truncate back to 16-bit
      e1 = ( e1 + ( 32767 ^ ( e1 >> 31 ) ) ) / 65536;
      e2 = ( e2 + ( 32767 ^ ( e2 >> 31 ) ) ) / 65536;
      e3 = ( e3 + ( 32767 ^ ( e3 >> 31 ) ) ) / 65536;
      e4 = ( e4 + ( 32767 ^ ( e4 >> 31 ) ) ) / 65536;

      o1 = ( o1 + ( 32767 ^ ( o1 >> 31 ) ) ) / 65536;
      o2 = ( o2 + ( 32767 ^ ( o2 >> 31 ) ) ) / 65536;
      o3 = ( o3 + ( 32767 ^ ( o3 >> 31 ) ) ) / 65536;
      o4 = ( o4 + ( 32767 ^ ( o4 >> 31 ) ) ) / 65536;


#ifdef __RADLITTLEENDIAN__
      ( (S32*) yout )[ 0 ] = ( e2 << 16 ) | ( e1 & 0xffff );
      ( (S32*) yout )[ 1 ] = ( e4 << 16 ) | ( e3 & 0xffff );
      ( (S32*) ( yout + pitch ) )[ 0 ] = ( o2 << 16 ) | ( o1 & 0xffff );
      ( (S32*) ( yout + pitch ) )[ 1 ] = ( o4 << 16 ) | ( o3 & 0xffff );
#else
      ( (S32*) yout )[ 0 ] = ( e1 << 16 ) | ( e2 & 0xffff );
      ( (S32*) yout )[ 1 ] = ( e3 << 16 ) | ( e4 & 0xffff );
      ( (S32*) ( yout + pitch ) )[ 0 ] = ( o1 << 16 ) | ( o2 & 0xffff );
      ( (S32*) ( yout + pitch ) )[ 1 ] = ( o3 << 16 ) | ( o4 & 0xffff );
#endif

      yout += pitch + pitch;

      lp++;
      hp++;

      // if we've hit the end of our local buffer, re-center at the beginning of the buffer
      if ( &lp[ 3 ][ 0 ] == &a.hp[ 0 ][ 0 ] )
      {
        MOVE_8( &a.lp[ 0 ][ 0 ], &lp[ 1 ][ 0 ] );
        MOVE_8( &a.lp[ 1 ][ 0 ], &lp[ 2 ][ 0 ] );
        MOVE_8( &a.lp[ 2 ][ 0 ], &lp[ 3 ][ 0 ] );
        MOVE_8( &a.hp[ 0 ][ 0 ], &hp[ 1 ][ 0 ] );
        MOVE_8( &a.hp[ 1 ][ 0 ], &hp[ 2 ][ 0 ] );
        MOVE_8( &a.hp[ 2 ][ 0 ], &hp[ 3 ][ 0 ] );
        MOVE_8( &a.hp[ 3 ][ 0 ], &hp[ 4 ][ 0 ] );
        lp = &a.lp[ 0 ];
        hp = &a.hp[ 0 ];
      }

      Assert( ylin >= lin );
      Assert( yhin >= hin );

      Assert( ylin < ( lin + ( height * ppitch ) ) );
      Assert( yhin < ( hin + ( height * ppitch ) ) );

      MOVE_8( &lp[ 3 ][ 0 ], ylin );
      MOVE_8( &hp[ 4 ][ 0 ], yhin );

      ylin += next;
      yhin += next;
    }

    out += 8;
    hin += 8;
    lin += 8;
    lend += 8;
  }

  // do remaining columns
  for( x = ( width & 3 ) ; x-- ; )
  {
    U32 y;
    S32 next;
    char *yout;
    char const *ylin;
    char const *yhin;

    S32 lp[ 4 ];
    S32 hp[ 5 ];

    next = ppitch;

    yout = out;
    ylin = lin;
    yhin = hin;

    if ( starty )
    {
      lp[ -1 + 1 ] = * ( S16* ) ylin;
      ylin += next;
      lp[ 0 + 1 ] = * ( S16* ) ylin;
      ylin += next;
      lp[ 1 + 1 ] = * ( S16* ) ylin;
      ylin += next;
      lp[ 2 + 1 ] = * ( S16* ) ylin;
      ylin += next;

      hp[ -2 + 2 ] =  * ( S16* ) yhin;
      yhin += next;
      hp[ -1 + 2 ] =  * ( S16* ) yhin;
      yhin += next;
      hp[ 0 + 2 ] =  * ( S16* ) yhin;
      yhin += next;
      hp[ 1 + 2 ] =  * ( S16* ) yhin;
      yhin += next;
      hp[ 2 + 2 ] =  * ( S16* ) yhin;
      yhin += next;
    }
    else
    {
      lp[ 0 + 1 ] = * ( S16* ) ylin;
      ylin += next;
      lp[ -1 + 1 ] = lp[ 1 + 1 ] = * ( S16* ) ylin;
      ylin += next;
      lp[ 2 + 1 ] = * ( S16* ) ylin;
      ylin += next;

      hp[ -1 + 2 ] = hp[ 0 + 2 ] =  * ( S16* ) yhin;
      yhin += next;
      hp[ -2 + 2 ] = hp[ 1 + 2 ] =  * ( S16* ) yhin;
      yhin += next;
      hp[ 2 + 2 ] =  * ( S16* ) yhin;
      yhin += next;
    }

    for( y = halfheight ; y-- ; )
    {
      S32 e, o;

      if ( ylin == lend )
      {
        ylin = ylin - next;
        yhin = yhin - next - next;
        next = -next;
      }

      e =
        (     ( lp[ 0 + 1 ] * 51674 )
          - ( ( lp[ -1 + 1 ] + lp[ 1 + 1 ] ) * 2667 )
          - ( ( hp[ -2 + 2 ] + hp[ 1 + 2 ] ) * 1563 )
          + ( ( hp[ -1 + 2 ] + hp[ 0 + 2 ] ) * 24733 )
        );

      o =
        ( (   ( lp[ 0 + 1 ] + lp[ 1 + 1 ] ) * 27400 )
          - ( ( lp[ -1 + 1 ] + lp[ 2 + 1 ] ) * 4230 )
          -   ( hp[ 0 + 2 ] * 55882 )
          - ( ( hp[ -2 + 2 ] + hp[ 2 + 2 ] ) * 2479 )
          + ( ( hp[ -1 + 2 ] + hp[ 1 + 2 ] ) * 7250 )
        );

      // round up and truncate back to 16-bit
      e = ( e + ( 32767 ^ ( e >> 31 ) ) ) / 65536;
      o = ( o + ( 32767 ^ ( o >> 31 ) ) ) / 65536;

      ( (S16*) yout )[ 0 ] = ( S16 ) e;
      ( (S16*) ( yout + pitch ) )[ 0 ] = ( S16 ) o;

      yout += pitch + pitch;

      lp[ 0 ] = lp[ 1 ];
      lp[ 1 ] = lp[ 2 ];
      lp[ 2 ] = lp[ 3 ];
      lp[ 3 ] = * ( S16* ) ylin;

      hp[ 0 ] = hp[ 1 ];
      hp[ 1 ] = hp[ 2 ];
      hp[ 2 ] = hp[ 3 ];
      hp[ 3 ] = hp[ 4 ];
      hp[ 4 ] = * ( S16* ) yhin;

      ylin += next;
      yhin += next;
    }

    out += 2;
    hin += 2;
    lin += 2;
    lend += 2;
  }
}


// inverse transforms a row with the Harr
static void iHarrrow( S16 * dest, S32 pitch, S16 const * in, S32 ppitch, S32 width, S32 height, U8 const * row_mask, S32 starty, S32 subheight )
{
  U32 y;
  char *out, *hin, *lin;
  U32 halfwidth;

  Assert( ( ( width & 1 ) == 0 ) );

  halfwidth = width / 2;

  out = (char*) dest;
  lin = (char*) in;
  hin = ( (char*) in ) + width;  // really 2 * halfwidth (which is just width)

  out += starty * pitch;
  hin += starty * ppitch;
  lin += starty * ppitch;
  row_mask += starty;

  for( y = subheight ; y-- ; )
  {
    U32 x;
    char *xout, *xhin, *xlin;

    xout = out;
    xlin = lin;
    xhin = hin;

    if ( ( ( ( RAD_UINTa ) row_mask ) <= (U32)height ) || ( *row_mask ) )
    {
      for( x = halfwidth ; x-- ; )
      {
        S32 e, o;

        e = ( (S32) ( ( S16* ) xlin )[ 0 ] * 2 + (S32) ( ( S16* ) xhin )[ 0 ] );
        o = ( (S32) ( ( S16* ) xlin )[ 0 ] * 2 - (S32) ( ( S16* ) xhin )[ 0 ] );

        // round up by 1
        e = ( e + ( 1 ^ ( e >> 31 ) ) ) / 2;
        o = ( o + ( 1 ^ ( o >> 31 ) ) ) / 2;

  #ifdef __RADLITTLEENDIAN__
        ( (S32*) xout )[ 0 ] = ( o << 16 ) | ( e & 0xffff );
  #else
        ( (S32*) xout )[ 0 ] = ( e << 16 ) | ( o & 0xffff );
  #endif

        xout += 2 + 2;

        Assert( xlin >= lin );
        Assert( xhin >= ( hin - 2) );
        Assert( xlin < ( lin + width ) );
        Assert( xhin < ( hin + width ) );

        xlin += 2;
        xhin += 2;
      }
    }
    else
    {
      // do a zero row

      for( x = halfwidth ; x-- ; )
      {
        S32 e = ( ( S16* ) xlin )[ 0 ];

        ( (S32*) xout )[ 0 ] = ( e << 16 ) | ( e & 0xffff );

        xout += 2 + 2;

        Assert( xlin >= lin );
        Assert( xlin < ( lin + width ) );

        xlin += 2;
      }
    }

    out += pitch;
    hin += ppitch;
    lin += ppitch;
    ++row_mask;
  }
}


// inverse transforms a column with the Harr
static void iHarrcol( S16 * dest, S32 pitch, S16 const * input, S32 ppitch, S32 width, S32 height, S32 starty, S32 subheight )
{
  U32 x;
  char *out;
  char const *lin;
  char const *hin;
  U32 halfheight;

  Assert( ( ( height & 1 ) == 0 ) );

  halfheight = subheight / 2;

  out = (char*) dest;
  lin = (char const *) input;
  hin = lin + ppitch;

  ppitch *= 2;

  if ( starty )
  {
    out += starty * pitch;
    lin += ( starty / 2 ) * ppitch;
    hin += ( starty / 2 ) * ppitch;
  }

  for( x = width ; x-- ; )
  {
    U32 y;
    char *yout;
    char const *ylin;
    char const *yhin;

    yout = out;
    ylin = lin;
    yhin = hin;

    for( y = halfheight ; y-- ; )
    {
      S32 e, o;

      e = ( (S32) ( ( S16* ) ylin )[ 0 ] * 2 + (S32) ( ( S16* ) yhin )[ 0 ] );
      o = ( (S32) ( ( S16* ) ylin )[ 0 ] * 2 - (S32) ( ( S16* ) yhin )[ 0 ] );

      // round up by 1
      e = ( e + ( 1 ^ ( e >> 31 ) ) ) / 2;
      o = ( o + ( 1 ^ ( o >> 31 ) ) ) / 2;

      ( (S16*) yout )[ 0 ] = ( S16 ) e;
      ( (S16*) ( yout + pitch ) )[ 0 ] = ( S16 ) o;

      yout += pitch + pitch;
      ylin += ppitch;
      yhin += ppitch;
    }

    out += 2;
    hin += 2;
    lin += 2;
  }
}


typedef void (*IWAVEROW)( S16 * dest, S32 pitch, S16 const * in, S32 ppitch, S32 width, S32 height, U8 const * row_mask, S32 starty, S32 subheight );
typedef void (*IWAVECOL)( S16 * dest, S32 pitch, S16 const * input, S32 ppitch, S32 width, S32 height, S32 starty, S32 subheight );

// does a inverse 2D wavelet in place (using DWT until too small and then flipping to Harr)
// this routine alternates between rows and columns to keep everything in the cache
static void iDWT2D( S16* output, S32 pitch, S32 width, S32 height, U8 const * row_mask, S16* temp )
{
  #define FLIPSIZE 8

  U32 rh, ch, ry, cy;

  IWAVEROW row = ( width >= SMALLEST_DWT_ROW ) ? iDWTrow : iHarrrow;
  IWAVECOL col = ( height >= SMALLEST_DWT_COL ) ? iDWTcol : iHarrcol;

  ry = ( height <= ( FLIPSIZE + 4 + 4 ) ) ? height : ( FLIPSIZE + 4 );
  rh = height - ry;
  cy = 0;
  ch = height;

  row( temp, pitch, output, pitch, width, height, row_mask, 0, ry );

  do
  {
    U32 next;

      next = ( ch <= ( FLIPSIZE + 4 ) ) ? ch : FLIPSIZE;

    col( output, pitch, temp, pitch, width, height, cy, next );
    cy += next;
    ch -= next;

    if ( rh )
    {
      next = ( rh <= ( FLIPSIZE + 4 ) ) ? rh : FLIPSIZE;
      row( temp, pitch, output, pitch, width, height, row_mask, ry, next );
      ry += next;
      rh -= next;
    }

  } while ( ch );

  #undef FLIPSIZE
}

/* ========================================================================
   This is the glue code
   ======================================================================== */
// uint32 GRANNY
// ToBinkTC0(void *output,
//           uint32 compress_to,
//           int16 **input,
//           real32 const *plane_weights,
//           uint32 planes,
//           uint32 width,
//           uint32 height,
//           void *temp, uint32 temp_size )
// {
//     return(to_BinkTC(output, compress_to, input, plane_weights, planes,
//                      width, height, temp, temp_size));
// }

// uint32 GRANNY
// ToBinkTCTempMem0(uint32 width, uint32 height)
// {
//     return(to_BinkTC_temp_mem(width, height));
// }

// uint32 GRANNY
// ToBinkTCOutputMem0(uint32 width, uint32 height,
//                    uint32 planes, uint32 compress_to)
// {
//     return(to_BinkTC_output_mem(width, height, planes, compress_to));
// }

void GRANNY
BinkTCCheckSizes0(uint32 *width, uint32 *height)
{
    BinkTC_check_sizes(width, height);
}

void GRANNY
FromBinkTC0(int16 **output,
            uint32 planes,
            void const * bink_buf,
            uint32 width,
            uint32 height,
            void *temp,
            uint32 temp_size)
{
    from_BinkTC(output, planes, bink_buf, width, height, temp, temp_size);
}

uint32 GRANNY
FromBinkTCTempMem0(void const *binkbuf)
{
    return(from_BinkTC_temp_mem(binkbuf));
}
