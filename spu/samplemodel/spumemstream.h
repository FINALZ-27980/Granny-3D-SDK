#if defined(BINK_SPU_PROCESS) || defined(MSS_SPU_PROCESS) || defined(PLATFORM_PS3_SPU) || defined(GRANNY_SPU_PROCESS)

#ifndef __SPUMEMSTREAMH__
#define __SPUMEMSTREAMH__

#ifndef __RADRR_COREH__
  #include "rrCore.h"
#endif

  RADDEFSTART

  typedef struct SPU_MEM_STREAM
  {
    char * base_ppu_addr;
    char * end_ppu_addr;
    char * base_spu_addr;
    char * end_spu_addr;

    char * start_ppu_range;
    char * start_spu_range;
    char * end_spu_range;
  } SPU_MEM_STREAM;

  // spu_size here should be the maximum size you will ask for in spu_mem_stream PLUS SPU_STREAM_EXTRA!!
  //    you have to make the buffer this big, *AND* tell us that the buffer is that big!
  void * spu_mem_init( SPU_MEM_STREAM * ms, void * spu_addr, U32 spu_size, void const * ppu_addr, U32 ppu_size );

  // need_bytes has to be smaller than (spu_size minus SPU_STREAM_EXTRA) in spu_mem_init
  S32 spu_mem_stream( SPU_MEM_STREAM * ms, void * spu_addr, U32 need_bytes, U32 prefetch_bytes, int tag );

  void * spu_mem_stream_addr_to_ppu_addr( SPU_MEM_STREAM * ms, void * addr );

  void * spu_mem_stream_addr_to_spu_addr( SPU_MEM_STREAM * ms, void * addr );

  void * align_memcpy_from_PPU( void * spu_address, void const * ppu_address, unsigned int bytes, int tag );

  #define SPU_STREAM_EXTRA ( 128 + 128 )


  // copy to and from circular buffers
  void wrap_copy_to_ppu( void * ppu, void * spu, S32 start, S32 count, S32 len, int tag );

  void wrap_copy_to_spu( void * spu, void * ppu, S32 start, S32 count, S32 len, int tag );

  // macro for allocating a safe amount of memory for doing aligned copies from the PPU
  #define SPU_EXTRA ( 128 + 16 + 16 )

  // >16K from ppu
  int big_copy( void * spu_address, void const * ppu_address, unsigned int bytes, int tag );

  // >16K to ppu
  int big_copy_to_PPU( void * ppu_address, void const * spu_address, unsigned int bytes, int tag );

  RADDEFEND

#endif

#endif
