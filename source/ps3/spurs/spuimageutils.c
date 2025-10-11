#include "spuimageutils.h"

static int countbits( unsigned long long b )
{
  int i;

  i = 0;
  while ( b )
  {
    if ( b & 1 )
      ++i;
    b >>= 1;
  }

  return( i );
}


void or_ls_pattern( unsigned long long * pat, unsigned int start, unsigned int end )
{
  int i;
  unsigned int pos = 0;
  unsigned long long b;

  start = start & ~2047;
  end = ( end + 2047 ) & ~2047;

  b = 0x8000000000000000;
  for( i = 0 ; i < 64 ; i++ )
  {
    if ( ( pos >= start ) && ( pos < end ) )
      pat[ 0 ] |= b;
    b >>= 1;
    pos += 2048;
  }

  b = 0x8000000000000000;
  for( i = 0 ; i < 64 ; i++ )
  {
    if ( ( pos >= start ) && ( pos < end ) )
      pat[ 1 ] |= b;
    b >>= 1;
    pos += 2048;
  }
}


typedef struct
{
  unsigned char id[16];
  unsigned short type;
  unsigned short machine;
  unsigned int version;
  unsigned int entry_point;
  unsigned int program_header_offset;
  unsigned int section_header_offset;
  unsigned int flags;
  unsigned short header_size;
  unsigned short program_header_size;
  unsigned short program_header_count;
  unsigned short section_header_size;
  unsigned short section_header_count;
  unsigned short section_header_string_index;
} ELFHEADER;

typedef struct
{
  unsigned int name_index;
  unsigned int type;
  unsigned int flags;
  unsigned int virtual_addr;
  unsigned int file_offset;
  unsigned int size;
  unsigned int link;
  unsigned int extra_info;
  unsigned int alignment;
  unsigned int entry_size;
} ELFSECTIONHEADER;

#define SECTION_WRITEABLE 1
#define SECTION_ALLOCATE  2


unsigned int get_spu_writable_ls_pattern( unsigned long long * pat, void const * img )
{
  ELFHEADER * h;
  ELFSECTIONHEADER * s;
  int i;
  unsigned int high;

  pat[ 0 ] = 0;
  pat[ 1 ] = 0;

  h = (ELFHEADER*) img;
  s = (ELFSECTIONHEADER*) ( (char const *)img + h->section_header_offset );

  high = 0;
  for( i = 0 ; i < h->section_header_count ; i++ )
  {
    unsigned int e;

    e = s->virtual_addr + s->size;

    if ( ( s->flags & (SECTION_WRITEABLE|SECTION_ALLOCATE) ) == (SECTION_WRITEABLE|SECTION_ALLOCATE) )
      or_ls_pattern( pat, s->virtual_addr, e );

    if ( ( s->flags & SECTION_ALLOCATE ) == SECTION_ALLOCATE )
      if ( e > high )
        high = e;
    ++s;
  }

  return high;
}


unsigned int get_ls_pattern_size( unsigned long long * pat )
{
  return ( ( countbits( pat[ 0 ] ) + countbits( pat[ 1 ] ) ) * 2048 ) + 1024;
}

#include <string.h>

int get_spu_section_info( void ** spu_address, void ** ppu_address, unsigned int * size, void const * img, char const * name )
{
  ELFHEADER * h;
  ELFSECTIONHEADER * s;
  char const * str;
  int i;

  h = (ELFHEADER*) img;
  s = (ELFSECTIONHEADER*) ( (char const*)img + h->section_header_offset );

  str = ( (char const *) img ) + s[ h->section_header_string_index ].file_offset;

  for( i = 0 ; i < h->section_header_count ; i++ )
  {
    if ( strcmp( str + s->name_index, name ) == 0 )
    {
      if ( spu_address ) *spu_address = (void*) s->virtual_addr;
      if ( ppu_address ) *ppu_address = ( (char*)img ) + s->file_offset;
      if ( size ) *size = s->size;

      return 1;
    }
    ++s;
  }

  if ( spu_address ) *spu_address = 0;
  if ( ppu_address ) *ppu_address = 0;
  if ( size ) *size = 0;

  return 0;
}


