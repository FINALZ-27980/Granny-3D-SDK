// @@ CB : spumemstream.h turns itself off unless you do this
//	should I define this somewhere else ?
#define PLATFORM_PS3_SPU

#include "spumemstream.h"
#include "raddebug.h"
#include "radspu.h"
#include <string.h>

#define ADDRESS_ALIGN 128

#define rounddown( val ) ( ( ( val ) ) & (~(ADDRESS_ALIGN-1)) )
#define roundup( val ) ( ( ( val ) + (ADDRESS_ALIGN-1) ) & (~(ADDRESS_ALIGN-1)) )

void * spu_mem_init( SPU_MEM_STREAM * ms, void * spu_addr, U32 spu_size, void const * ppu_addr, U32 ppu_size )
{
  char * s_addr;
  char * p_addr;
  char * se_addr;
  char * pe_addr;

  s_addr = (char *)roundup( (UINTa) spu_addr );
  p_addr = (char *)rounddown( (UINTa) ppu_addr );

  se_addr = (char *)rounddown( (UINTa) ( ( (char*) spu_addr ) + spu_size ) );
  pe_addr = (char *)roundup( (UINTa) ( ( (char*) ppu_addr ) + ppu_size ) );

  ms->base_ppu_addr = p_addr;
  ms->end_ppu_addr = pe_addr;
  ms->base_spu_addr = s_addr;
  ms->end_spu_addr = se_addr;

  ms->start_ppu_range = p_addr;
  ms->start_spu_range = s_addr;
  ms->end_spu_range = s_addr;

  return( s_addr + ( ( (char*) ppu_addr ) - p_addr ) );
}


int big_copy( void * spu_address, void const * ppu_address, unsigned int bytes, int tag )
{
  U8 * s, * p;
  int t;

  t = ( tag < 0 ) ? SPU_DEFAULT_FROM_PPU : tag;

  s = (U8*) spu_address;
  p = (U8*) ppu_address;

  do
  {
    U32 amt;
    amt = ( bytes > 16384 ) ? 16384 : bytes;

    if ( !RAD_memcpy_from_PPU( s, p, amt, t ) )
      return( 0 );

    bytes -= amt;
    s += amt;
    p += amt;
  } while ( bytes );

  if ( tag < 0 )
    return( RAD_wait_on_memcpy( t ) );

  return( 1 );
}


#ifdef DEBUGMEMMOVE

#ifdef __RADFINAL__
#error "You have debug spu memory moves on..."
#endif

static char buf[65536+128];

static void check_mem( SPU_MEM_STREAM * ms )
{
  char * b;
  b=(char*) (( ((UINTa)buf) + 127 ) &~127 );

 memset(b,0,sizeof(buf));
  big_copy( b, ms->start_ppu_range, ms->end_spu_range - ms->start_spu_range, -1 );
  if ( memcmp( b, ms->start_spu_range, ms->end_spu_range - ms->start_spu_range ) )
    BreakPoint();
}

#define do_debug check_mem( ms );

#else

#define do_debug

#endif


S32 spu_mem_stream( SPU_MEM_STREAM * ms, void * spu_addr, U32 need_bytes, U32 prefetch_bytes, int tag )
{
  char * start;
  char * end_need;
  char * end_pre;
  char * clamp;
  UINTa adj_align;

  start = (char*) *(void**)spu_addr;
  clamp = (char*) ( (UINTa) start - (UINTa) ms->start_spu_range + (UINTa) ms->start_ppu_range );

  if ( ( clamp > ms->end_ppu_addr ) || ( clamp < ms->base_ppu_addr ) )
  {
#ifdef DEBUGMEMMOVE
    BreakPoint();
#endif
    return( 0 );
  }

  clamp += need_bytes;
  if ( clamp > ms->end_ppu_addr )
  {
    need_bytes -= ( clamp - ms->end_ppu_addr );
    prefetch_bytes = 0;
    clamp = ms->end_ppu_addr;
  }
  end_need = start + need_bytes;

  clamp += prefetch_bytes;
  if ( clamp > ms->end_ppu_addr )
    prefetch_bytes -= ( clamp - ms->end_ppu_addr );
  end_pre = start + need_bytes + prefetch_bytes;

  if ( start >= ms->end_spu_range )
  {
    // new range is all the way off
    goto no_match;
  }
  else if ( start >= ms->start_spu_range )
  {
    if ( end_need <= ms->end_spu_range )
    {
      // new range is totally within the buffered range
      *(void**)spu_addr = start;
      return( 1 );
    }
    else
    {
      // new range runs off the back of the buffered range
      U32 room_needed, room_left;

      room_needed = end_pre - ms->end_spu_range;
      #ifdef _DEBUG
      if ( ms->end_spu_range > ms->end_spu_addr ) BreakPoint();
      #endif

      room_left = ms->end_spu_addr - ms->end_spu_range;

      if ( room_needed > room_left )
      {
        U32 to_copy;

        // we need to move the data to the start of the spu buffer and then load the new data at the end
        adj_align = ( (UINTa) start ) & (ADDRESS_ALIGN-1);
        start -= adj_align;
        to_copy = ms->end_spu_range - start;

        memcpy( (void*) ms->base_spu_addr, (void*)start, to_copy );
        ms->start_ppu_range += ( start - ms->start_spu_range );

        start = ms->base_spu_addr + adj_align;
        ms->start_spu_range = ms->base_spu_addr;
        ms->end_spu_range = ms->base_spu_addr + to_copy;
        #ifdef _DEBUG
        if ( ms->end_spu_range > ms->end_spu_addr ) BreakPoint();
        #endif
      }

      // now we have enough room to load the new data into the buffer at the end of the range

      room_needed = roundup( room_needed );

      if ( room_needed )
      {
        big_copy( ms->end_spu_range, ms->start_ppu_range + ( ms->end_spu_range - ms->start_spu_range ), room_needed, tag );
        ms->end_spu_range += room_needed;
        #ifdef _DEBUG
        if ( ms->end_spu_range > ms->end_spu_addr ) BreakPoint();
        #endif
      }

      *(void**)spu_addr = start;

      do_debug
      return( 1 );
    }
  }
  else if ( end_pre > ms->start_spu_range )
  {
    // new range is off the front of the buffered range
    U32 room_needed, room_left;

    adj_align = ( (UINTa) start ) & (ADDRESS_ALIGN-1);

    room_needed = ms->start_spu_range - ( start - adj_align );
    room_left = ms->start_spu_range - ms->base_spu_addr;

    if ( room_needed > room_left )
    {
      // we need to move the data towards the end the spu buffer and then load the new data at the front
      U32 adj_pos, move_bytes;

      adj_pos = room_needed - room_left;
      move_bytes = roundup( need_bytes + prefetch_bytes - adj_pos + adj_align );

      memmove( (void*) ( ms->start_spu_range + adj_pos ), (void*) ms->start_spu_range, move_bytes );
      ms->start_spu_range += adj_pos;
      ms->end_spu_range = ms->start_spu_range + move_bytes;
      #ifdef _DEBUG
      if ( ms->end_spu_range > ms->end_spu_addr ) BreakPoint();
      #endif
    }

    ms->start_ppu_range -= room_needed;
    ms->start_spu_range -= room_needed;

    big_copy( ms->start_spu_range, ms->start_ppu_range, room_needed, tag );
    start = ms->base_spu_addr + adj_align;

    *(void**)spu_addr = start;

    do_debug
    return( 1 );
  }
  else
  {
    // range is completely missing
   no_match:
    adj_align = ( (UINTa) start ) & (ADDRESS_ALIGN-1);

    ms->start_ppu_range += ( ( start - adj_align ) - ms->start_spu_range );

    need_bytes = roundup( need_bytes + prefetch_bytes + adj_align );

    start = ms->base_spu_addr;
    big_copy( start, ms->start_ppu_range, need_bytes, tag );
    ms->end_spu_range = start + need_bytes;
    ms->start_spu_range = start;

    #ifdef _DEBUG
    if ( ms->end_spu_range > ms->end_spu_addr ) BreakPoint();
    #endif

    *(void**)spu_addr = start + adj_align;

    do_debug
    return( 1 );
  }
}

void * spu_mem_stream_addr_to_ppu_addr( SPU_MEM_STREAM * ms, void * addr )
{
  return( (void*) ( ((UINTa)addr) - ((UINTa)ms->start_spu_range) + ((UINTa)ms->start_ppu_range) ) );
}

void * spu_mem_stream_addr_to_spu_addr( SPU_MEM_STREAM * ms, void * addr )
{
  return( (void*) ( ((UINTa)addr) + ((UINTa)ms->start_spu_range) - ((UINTa)ms->start_ppu_range) ) );
}


void * align_memcpy_from_PPU( void * spu_address, void const * ppu_address, unsigned int bytes, int tag )
{
  UINTa adj, adj2;

  adj2 = ( (UINTa) ppu_address ) & 15;
  if ( adj2 )
  {
    ppu_address = (void*) ( ( (UINTa) ppu_address ) & ~15 );
    bytes += adj2;
  }
  bytes = ( bytes + 15 ) & ~15;

  spu_address = (void*) ( ( ( (UINTa) spu_address ) + 15 ) & ~15 );

  adj = ( 128 + (UINTa) ppu_address - (UINTa) spu_address ) & 127;

  spu_address = ( (U8*) spu_address ) + adj;

  if ( big_copy( spu_address, ppu_address, bytes, tag ) )
    return( ( (U8*) spu_address ) + adj2 );

  return( 0 );
}

void wrap_copy_to_spu( void * spu, void * ppu, S32 start, S32 count, S32 len, int tag )
{
  void * s = ( (U8*) spu ) + start;
  void * p = ( (U8*) ppu ) + start;
  void * se = ( (U8*) spu ) + start + count;
  void * sbe = ( (U8*) spu ) + len;

  s = (void*) ( ( (UINTa) s ) & ~15 );
  p = (void*) ( ( (UINTa) p ) & ~15 );

  se = (void*) ( ( ( (UINTa) se ) + 15 ) & ~15 );

  if ( se <= sbe )
  {
    RAD_memcpy_from_PPU( s, p, ( (UINTa) se ) - ( (UINTa) s ), tag );
  }
  else
  {
    // wrapped
    RAD_memcpy_from_PPU( s, p, ( (UINTa) sbe ) - ( (UINTa) s ), ( tag < 0 ) ? SPU_DEFAULT_TO_SPU : tag );
    RAD_memcpy_from_PPU( spu, ppu, ( (UINTa) se ) - ( (UINTa) sbe ), tag );
  }
}

void wrap_copy_to_ppu( void * ppu, void * spu, S32 start, S32 count, S32 len, int tag )
{
  void * s = ( (U8*) spu ) + start;
  void * p = ( (U8*) ppu ) + start;
  void * se = ( (U8*) spu ) + start + count;
  void * sbe = ( (U8*) spu ) + len;

  s = (void*) ( ( (UINTa) s ) & ~15 );
  p = (void*) ( ( (UINTa) p ) & ~15 );

  se = (void*) ( ( ( (UINTa) se ) + 15 ) & ~15 );

  if ( se <= sbe )
  {
    RAD_memcpy_to_PPU( p, s, ( (UINTa) se ) - ( (UINTa) s ), tag );
  }
  else
  {
    // wrapped
    RAD_memcpy_to_PPU( p, s, ( (UINTa) sbe ) - ( (UINTa) s ), ( tag < 0 ) ? SPU_DEFAULT_TO_PPU : tag );
    RAD_memcpy_to_PPU( ppu, spu, ( (UINTa) se ) - ( (UINTa) sbe ), tag );
  }
}

int big_copy_to_PPU( void * ppu_address, void const * spu_address, unsigned int bytes, int tag )
{
  U8 * s, * p;
  int t;

  t = ( tag < 0 ) ? SPU_DEFAULT_TO_PPU : tag;

  s = (U8*) spu_address;
  p = (U8*) ppu_address;

  do
  {
    U32 amt;
    amt = ( bytes > 16384 ) ? 16384 : bytes;

    if ( !RAD_memcpy_to_PPU( p, s, amt, t ) )
      return( 0 );

    bytes -= amt;
    s += amt;
    p += amt;
  } while ( bytes );

  if ( tag < 0 )
    return( RAD_wait_on_memcpy( t ) );

  return( 1 );
}


