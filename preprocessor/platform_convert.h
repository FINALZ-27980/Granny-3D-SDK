// ========================================================================
// $File: //jeffr/granny_29/preprocessor/platform_convert.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(PLATFORM_CONVERT_H)

#include "preprocessor.h"

bool GetPlatformMVFromKeyValues(key_value_pair* KeyValues,
                                granny_int32x   NumKeyValues,
                                granny_grn_file_magic_value& MagicValue);

#define PLATFORM_CONVERT_H
#endif /* PLATFORM_CONVERT_H */
