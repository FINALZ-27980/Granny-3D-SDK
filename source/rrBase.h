// this header is used for base types for internal source files.
//   it's mostly a wrapper around rrcore.h, but includes a few
//   nice global things for us, that would cause trouble if we
//   defined them in someone's code base (warnings and null/true/false
//   kinds of things).

// this file also incldue rruserid.h, which allows you to add
//   your own custom build stuff that you don't expect anyone
//   else to use.

#ifndef __RADRR_BASEH__
#define __RADRR_BASEH__

   // get userId in very early and globally...
  #include "rrUserId.h"

  #ifndef __RADRR_COREH__
    #include "rrCore.h"
  #endif

    //===========================================================================

	#ifdef CBLOOM
	
		#undef RAD_STATEMENT_START
		#define RAD_STATEMENT_START			do {
    
		#undef RAD_STATEMENT_END_FALSE
		#define RAD_STATEMENT_END_FALSE	   } while ( 0 )
	
		#undef RAD_STATEMENT_END_TRUE
		#define RAD_STATEMENT_END_TRUE	   } while ( 1 )
	
	#endif
	
    //===========================================================================

	#ifdef _MSC_VER
		#define RR_PRAGMA_MESSAGE(str)     message( __FILE__ "(" RR_LINESTRING ") : message: " str)
		// use like #pragma RR_PRAGMA_MESSAGE(str) , no semicolon
		// this formatting makes it clickable in the VC output window
		// CB : I'd like to map this to RADTODO or something but
		//  you have to put the #pragma in yourself
	#else
		#define RR_PRAGMA_MESSAGE(str) 
	#endif

	#ifdef CBLOOM
		#define RR_CB_TODO(str)	RR_PRAGMA_MESSAGE("todo : " str) 
	#else
		#define RR_CB_TODO(str)
	#endif

	#define RR_NO_DEFAULT_CASE default: RR_CANT_GET_HERE(); break;

	#ifdef CBLOOM

	// remove the static Ignored variable :
	
	#undef RR_ASSERT_ALWAYS
	#define RR_ASSERT_ALWAYS(exp)      do { if ( ! (exp) ) { if ( rrDisplayAssertion(NULL,__FILE__,__LINE__,RR_FUNCTION_NAME,#exp) ) RR_ASSERT_BREAK(); } } while(0)

	#undef RR_ASSERT_FAILURE_ALWAYS
	#define RR_ASSERT_FAILURE_ALWAYS(str)           do { if ( rrDisplayAssertion(NULL,__FILE__,__LINE__,RR_FUNCTION_NAME,str) ) RR_ASSERT_BREAK(); } while(0)

	#endif // CBLOOM

    //===========================================================================
  
  // aliases :
  #define UINTn UINTa
  #define SINTn SINTa
  
  // ugly: no stdlib but I still want NULL
  #ifndef NULL
  #define NULL    (0)
  #endif
   
   #define sizeof32(obj)		((S32)sizeof(obj))
   
    //===========================================================================

    // NOTE(casey): RAD_KNOWN_UNSIGNED_TRUE/FALSE is for comparisons that you _know_
    // evaluate to a constant result due to the fact that the underlying type is unsigned, such
    // as comparisons of an unsigned int >= 0.  These need to be wrapped in macros so
    // that they can be eliminated from compilers who will issue a warning or an error
    // when you do such comparisons.
    #if defined(__RADPS3__) || defined(__RADWIIU__)
    #define RAD_KNOWN_UNSIGNED_TRUE(a) (1)
    #define RAD_KNOWN_UNSIGNED_FALSE(a) (0)
    #else
    #define RAD_KNOWN_UNSIGNED_TRUE(a) (a)
    #define RAD_KNOWN_UNSIGNED_FALSE(a) (a)
    #endif

    #ifdef __RADCELL__

    // RAD_ALIGN_SPU aligns to 16 if you're on CELL (Ps3 or SPU)
    //  otherwise is a NOP
    #define RAD_ALIGN_SPU           __attribute__((aligned (16)))

    #else

    #define RAD_ALIGN_SPU

    #endif // __RADCELL__

	#ifdef __GCC__

    // break compiler scheduling blocks :
    #define RAD_GCC_SCHEDULE_BARRIER    __asm__ volatile("");

	#else

    #define RAD_GCC_SCHEDULE_BARRIER

	#endif // __GCC__

    //---------------------------------------------------------------------------

    #define RR_BIT_FLAG(num)    ( (1UL<<(num)) )
    #define RR_BIT_MASK(num)    ( RR_BIT_FLAG(num) - 1 )

    //-------------------------------------------------------------------

    // RR_CLAMP32_0_N : use unsigned compare to check if its in range
    //  then one more to see what size of range it's off
    // another way would be since hi is usually a bit mask : 
    // if x already was unsigned, the compare against zero should get removed by the optimizer :
    #define RR_CLAMP32_0_N(x,hi)    ( ( ((U32)(x)) > (hi) ) ? ( ( (x) < 0 ) ? 0 : (hi) ) : (x) )
    // this can be faster if x is actually signed (use signed shift to make mask)
    //#define RR_CLAMP32_0_N(x,hi)  ( ( ((U32)(x)) > (hi) ) ? ( (~((x)>>31)) & (hi) )  : (x) )

    #define RR_CLAMP_255(x) RR_CLAMP32_0_N(x,255)

    #define RR_CLAMP_U8(x)  (U8)RR_CLAMP32_0_N(x,(S32)0xFF)
    #define RR_CLAMP_U16(x) (U16)RR_CLAMP32_0_N(x,(S32)0xFFFF)
    //#define RR_CLAMP_U32(x)   (U32)RR_CLAMP_0_N(x,(S64)0xFFFFFFFF)

    #define RR_MIN3(a,b,c)  RR_MIN(RR_MIN(a,b),c)
    #define RR_MAX3(a,b,c)  RR_MAX(RR_MAX(a,b),c)
    #define RR_MIN4(a,b,c,d)    RR_MIN(RR_MIN(a,b),RR_MIN(c,d))
    #define RR_MAX4(a,b,c,d)    RR_MAX(RR_MAX(a,b),RR_MAX(c,d))

    //-------------------------------------------------------------------

    #define RR_UNROLL_0(x)   do {  } while(0)
    #define RR_UNROLL_1(x)   do { x; } while(0)
    #define RR_UNROLL_2(x)   do { x; x; } while(0)
    #define RR_UNROLL_3(x)   do { x; x; x; } while(0)
    #define RR_UNROLL_4(x)   do { x; x; x; x; } while(0)
    #define RR_UNROLL_5(x)   do { RR_UNROLL_4(x); x; } while(0)
    #define RR_UNROLL_6(x)   do { RR_UNROLL_5(x); x; } while(0)
    #define RR_UNROLL_7(x)   do { RR_UNROLL_6(x); x; } while(0)
    #define RR_UNROLL_8(x)   do { RR_UNROLL_4(x); RR_UNROLL_4(x); } while(0)
    #define RR_UNROLL_9(x)   do { RR_UNROLL_8(x); x; } while(0)
    #define RR_UNROLL_10(x)  do { RR_UNROLL_9(x); x; } while(0)
    #define RR_UNROLL_11(x)  do { RR_UNROLL_10(x); x; } while(0)
    #define RR_UNROLL_12(x)  do { RR_UNROLL_11(x); x; } while(0)
    #define RR_UNROLL_13(x)  do { RR_UNROLL_12(x); x; } while(0)
    #define RR_UNROLL_14(x)  do { RR_UNROLL_13(x); x; } while(0)
    #define RR_UNROLL_15(x)  do { RR_UNROLL_14(x); x; } while(0)
    #define RR_UNROLL_16(x)  do { RR_UNROLL_8(x); RR_UNROLL_8(x); } while(0)

	// RR_UNROLL_I does x repeatedly with an index "i" set like you were a loop
	//	"b" is the start of the looop
	#define RR_UNROLL_I_0(b,x)	do {  } while(0)
	#define RR_UNROLL_I_1(b,x)	do { const int i = b; x; } while(0)
	#define RR_UNROLL_I_2(b,x)	do { RR_UNROLL_I_1(b,x); RR_UNROLL_I_1(b+1,x); } while(0)
	#define RR_UNROLL_I_3(b,x)	do { RR_UNROLL_I_2(b,x); RR_UNROLL_I_1(b+2,x); } while(0)
	#define RR_UNROLL_I_4(b,x)	do { RR_UNROLL_I_2(b,x); RR_UNROLL_I_2(b+2,x); } while(0)
	#define RR_UNROLL_I_5(b,x)	do { RR_UNROLL_I_4(b,x); RR_UNROLL_I_1(b+4,x); } while(0)
	#define RR_UNROLL_I_6(b,x)	do { RR_UNROLL_I_4(b,x); RR_UNROLL_I_2(b+4,x); } while(0)
	#define RR_UNROLL_I_7(b,x)	do { RR_UNROLL_I_4(b,x); RR_UNROLL_I_3(b+4,x); } while(0)
	#define RR_UNROLL_I_8(b,x)	do { RR_UNROLL_I_4(b,x); RR_UNROLL_I_4(b+4,x); } while(0)
	#define RR_UNROLL_I_9(b,x)	do { RR_UNROLL_I_8(b,x); RR_UNROLL_I_1(b+8,x); } while(0)
	#define RR_UNROLL_I_10(b,x)	do { RR_UNROLL_I_8(b,x); RR_UNROLL_I_2(b+8,x); } while(0)
	#define RR_UNROLL_I_11(b,x)	do { RR_UNROLL_I_8(b,x); RR_UNROLL_I_3(b+8,x); } while(0)
	#define RR_UNROLL_I_12(b,x)	do { RR_UNROLL_I_8(b,x); RR_UNROLL_I_4(b+8,x); } while(0)
	#define RR_UNROLL_I_13(b,x)	do { RR_UNROLL_I_8(b,x); RR_UNROLL_I_5(b+8,x); } while(0)
	#define RR_UNROLL_I_14(b,x)	do { RR_UNROLL_I_8(b,x); RR_UNROLL_I_6(b+8,x); } while(0)
	#define RR_UNROLL_I_15(b,x)	do { RR_UNROLL_I_8(b,x); RR_UNROLL_I_7(b+8,x); } while(0)
	#define RR_UNROLL_I_16(b,x)	do { RR_UNROLL_I_8(b,x); RR_UNROLL_I_8(b+8,x); } while(0)
	#define RR_UNROLL_I_17(b,x)	do { RR_UNROLL_I_16(b,x); RR_UNROLL_I_1(b+16,x); } while(0)
	#define RR_UNROLL_I_24(b,x)	do { RR_UNROLL_I_16(b,x); RR_UNROLL_I_8(b+16,x); } while(0)
	#define RR_UNROLL_I_32(b,x)	do { RR_UNROLL_I_16(b,x); RR_UNROLL_I_16(b+16,x); } while(0)

    // RR_STRING_JOIN_DELAY is useful here if count is itself a macro
    #define RR_UNROLL_N(count,x)    do { RR_STRING_JOIN_DELAY(RR_UNROLL_,count)(x); } while(0)

    #define RR_UNROLL_I_N(count,b,x)    do { RR_STRING_JOIN_DELAY(RR_UNROLL_I_,count)(b,x); } while(0)
    
    //===========================================================================

  // helper function to show I really am intending to put a pointer difference in an int :
  static RADINLINE SINTa rrPtrDiff(SINTa val) { return val; }
  static RADINLINE S32 rrPtrDiff32(SINTa val) { S32 ret = (S32) val; RR_ASSERT( (SINTa)ret == val ); return ret; }

  #ifndef __cplusplus
  
    #ifndef true
      #define true    (1)
    #endif
    
    #ifndef false
      #define false   (0)
    #endif
  
  #endif


  #ifdef __RADNT__
    // CB : RADTOOL means we are on the tool side, on a PC or something
    //  not in a console deployed build
    // @@ : this is not really defined right and questionable
    //	it should not be on for the PC game, just PC tools
    #define __RADTOOL__
  #endif

  #ifdef _MSC_VER
    // This warning is completely useless.  Turn it off in all files...
    #pragma warning( disable : 4514) // unreferenced inline function removed.
    #pragma warning( disable : 4505) // unreferenced local function has been removed

    // This security warning could be useful, but we get it on some non-useful cases, like fopen()
    #pragma warning( disable : 4996) // deprecated insecure crt call

    #pragma warning( disable : 4100) // unreferenced formal parameter
    #pragma warning( disable : 4127) // conditional expression is constant
     
      /**
      NOTEZ : cdep disables all of these :   

      -wd4100 
      -wd4127
      -wd4201 
      -wd4512
      -wd4740       
      -wd4748 
      -wd4800 
      
      **/

  #endif
  
#endif
