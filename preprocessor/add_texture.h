#if !defined(ADD_TEXTURE_H)
// ========================================================================
// $File: //jeffr/granny_29/preprocessor/add_texture.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"

bool AddTexture(input_file*,
                granny_int32x,
                key_value_pair* KeyValues,
                granny_int32x NumKeyValues,
                granny_memory_arena* Arena);

#define ADD_TEXTURE_H
#endif /* ADD_TEXTURE_H */
