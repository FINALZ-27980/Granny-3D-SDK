// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#if !defined(SPUIMAGEUTILS_H)

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


  void or_ls_pattern( unsigned long long * pat, unsigned int start, unsigned int end );

  unsigned int get_spu_writable_ls_pattern( unsigned long long * pat, void const * img );

  unsigned int get_ls_pattern_size( unsigned long long * pat );

  int get_spu_section_info( void ** spu_address, void ** ppu_address, unsigned int * size, void const * img, char const * name );

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#define SPUIMAGEUTILS_H
#endif /* SPUIMAGEUTILS_H */



