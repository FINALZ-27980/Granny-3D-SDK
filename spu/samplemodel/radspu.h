// This header is the interface that Bink and Miles uses on the SPU side.

// We supply a SPU threads version of this interface, but any SPU implementation
//   should work as long as you implement this API and link it with the
//   Binkspu.a library (for example). This file has to match the PPU API that
//   you linked the Binkps3.a library with.

#ifdef __cplusplus
extern "C" {
#endif  


// macro to align an spu address so that it is aligned within an 128 byte cacheline (needs at least 112 extra bytes)
#define align_spu_to_ppu( spu, ppu ) ( (void*) ( ( (UINTa) spu ) + ( ( 128 + ( (UINTa) ppu ) - ( (UINTa) spu ) ) & 127 ) ) )


// This function copies memory from the PPU into SPU memory. The SPU and PPU
//   addresses will be 16-byte aligned and it will only be asked to copy up to
//   16384 bytes at a time.  The bytes to copy should also be a multiple of 16.
//   If you specify a negative number for the tag, this function should use tag SPU_DEFAULT_FROM_PPU
//   and will wait until the copy completes.  If you specify a normal dma tag
//   index, then this function will return immediately (then use RAD_wait_on_memcpy).

// Note all calls to this function will have the ppu and spu addresses the
//   same offset off cacheline.  That is, if the ppu is 32 bytes off the start
//   of a cache line, so will the spu address (this is for speed).

int RAD_memcpy_from_PPU( void * spu_address, void const * ppu_address, unsigned int bytes, int tag );
#define RAD_memcpy_to_SPU RAD_memcpy_from_PPU
#define SPU_DEFAULT_TO_SPU 0
#define SPU_DEFAULT_FROM_PPU 0

// This function copies memory from the SPU into PPU memory. The SPU and PPU
//   addresses will be 16-byte aligned and it will only be asked to copy up to
//   16384 bytes at a time.  The bytes to copy should also be a multiple of 16.
//   If you specify a negative number for the tag, this function will use tag SPU_DEFAULT_TO_PPU
//   and will wait until the copy completes.  If you specify a normal dma tag
//   index, then this function will return immediately (then use RAD_wait_on_memcpy).

int RAD_memcpy_to_PPU( void * ppu_address, void const * spu_address, unsigned int bytes, int tag );
#define RAD_memcpy_from_SPU RAD_memcpy_to_PPU
#define SPU_DEFAULT_TO_PPU 1
#define SPU_DEFAULT_FROM_SPU 1


// This function waits for a previously tagged async memcpy to complete.

int RAD_wait_on_memcpy( int tag );


// This function sticks a command into a queue that the ppu can read from.
//   Only 24-bits of data2 are sent with spu threads, but other interface libs
//   may not have this restriction.

int RAD_send_to_ppu( unsigned int data1, unsigned int data2_24bits );


// This function waits for a specified period of time for data coming from the ppu.
//   "us" is a microsecond count, or zero for no wait (just try/fail), or a negative
//   number which means wait forever.  The data parameters return the data, but 
//   the pointers can be null if you don't want any of the values.  This function
//   will return 0 if no data, or 1 if data.  If "data1" is ever zero, then the
//   spu program should exit as soon as possible (the ppu is waiting for the spu
//   app to exit).

int RAD_receive_from_ppu( int us, unsigned int * data1, unsigned int * data2 );


// This function can return an address and size that can be used for temporary
//   memory.  This will usually be data in the ".reload" section that we stick
//   our unloadable data into.  You must reload this section with the
//   RAD_start_spu_reload function before calling any functions in the reload 
//   section. If the spu lib provider doesn't have spu memory, then it just
//   returns zeros for the two parameters.

void RAD_get_spu_reload_mem_info( void ** spu_address, unsigned int * size );


// This function starts reloading the reload section from the PPU cached image.

int RAD_start_spu_reload( void );


// This function waits until the reload section is restored.

void RAD_wait_spu_reload( void );


// This function returns a timer value in whatever internal format the interface
//   library chooses.  Use RAD_delta_timer_to_us or RAD_delta_timer_to_ms
//   to turn these values into usable times.

unsigned long long RAD_timer_read( void );


// This function takes two timer values and returns the time in microseconds.

unsigned long long RAD_delta_timer_to_us( unsigned long long start, unsigned long long end );


// This function takes two timer values and returns the time in milliseconds.

unsigned long long RAD_delta_timer_to_ms( unsigned long long start, unsigned long long end );


// This function is supplied inside the RAD library that is calling these interface
//   functions.  So, the interface library calls this function from the spu startup 
//   code (usually from "main()").  All RAD work will be done inside this function.

void RAD_main( void );

#ifdef __cplusplus
}
#endif  
