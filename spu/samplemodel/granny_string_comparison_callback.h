#if !defined(GRANNY_STRING_COMPARISON_CALLBACK_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_string_comparison_callback.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(StringGroup);

EXPAPI typedef CALLBACK_FN(int32x) string_comparison_callback(char const *A, char const *B);
extern string_comparison_callback *StringComparisonCallback;
EXPAPI GS_MODIFY void SetStringComparisonCallback(string_comparison_callback *Callback);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_STRING_COMPARISON_CALLBACK_H
#endif /* GRANNY_STRING_COMPARISON_CALLBACK_H */
