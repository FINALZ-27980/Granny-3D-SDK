#if !defined(GRANNY_STRING_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_string.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(StringGroup);

#define VALIDSTRING AnalysisCond([SA_Pre(Valid = SA_Yes, Null=SA_No, NullTerminated=SA_Yes)])
#define VALIDDEST   AnalysisCond([SA_Pre(Valid = SA_Yes, Null=SA_No)])

int32x StringLength(VALIDSTRING char const* String);

bool StringsAreEqual(VALIDSTRING char const* StringA,
                     VALIDSTRING char const* StringB);

bool StringsAreEqual(AnalysisCond([SA_Pre(Valid=SA_Yes)]) int32x ALength,
                     VALIDSTRING char const* StringA,
                     VALIDSTRING char const* StringB);

EXPAPI GS_SAFE int32x StringDifference(char const* StringA,
                                       char const* StringB);

int32x StringDifferenceLowercase(VALIDSTRING char const* StringA,
                                 VALIDSTRING char const* StringB);
bool LengthStringIsEqualTo(int32x LengthA,
                           VALIDSTRING char const* StringA,
                           VALIDSTRING char const* StringB);

bool StringBeginsWith(VALIDSTRING char const* String,
                      VALIDSTRING char const* Beginning);
bool StringBeginsWithLowercase(VALIDSTRING char const* String,
                               VALIDSTRING char const* Beginning);
bool StringsAreEqualLowercase(VALIDSTRING char const* StringA,
                              VALIDSTRING char const* StringB);
bool StringsAreEqualLowercaseNoSlash(VALIDSTRING char const* StringA,
                                     VALIDSTRING char const* StringB);
bool StringsAreEqualLowercase(VALIDSTRING char const* StringAStart,
                              VALIDSTRING char const* StringAEnd,
                              VALIDSTRING char const* StringB);
bool WildCardMatch(VALIDSTRING char const* Name,
                   VALIDSTRING char const* Wildcard,
                   VALIDDEST   char* Out);

bool IsPlainWildcard(VALIDSTRING char const* Wildcard);

void StringEquals(VALIDDEST char *Dest,
                  int32x MaxChars,
                  VALIDSTRING char const* Source);

int32x CopyString(VALIDDEST   char const* From,
                  VALIDSTRING char *To);

void CopyStringMaxLength(VALIDSTRING char const* From,
                         char **To,
                         int32x *MaxLength);
int32 AppendStringMaxLength(VALIDSTRING char const* From,
                            VALIDDEST   char *To,
                            int32x MaxLength);
char* CloneString(VALIDSTRING char const* Source);

char const* StringContains(VALIDSTRING char const* String,
                           VALIDSTRING char const* Substring);
char const* StringContainsLowercase(VALIDSTRING char const*  String,
                                    VALIDSTRING char const*  Substring);
char const* FindLastSlash(VALIDSTRING char const* Filename);
char const* FindLastInstanceOf(VALIDSTRING char const* String, char Char);

int32 ConvertToInt32(VALIDSTRING char const* String);
uint32 ConvertToUInt32(VALIDSTRING char const* String);
real32 ConvertToReal32(VALIDSTRING char const* String,
                       bool const AllowFractions = true);
real64x ConvertToReal64(VALIDSTRING char const* String,
                        bool const AllowFractions = true);

bool IsWhitespace(char const Character);
bool IsAlphabetic(char const Character);
bool IsLowercase(char const Character);
bool IsUppercase(char const Character);
bool IsDecimal(char const Character);
bool IsPunctuation(char const Character);
bool IsHexadecimal(char const Character);
bool IsSlash(char Char);

uint8 ConvertToUInt8(char Character);
char ConvertToLowercase(char Character);
char ConvertToUppercase(char Character);

int32x StringDifferenceOrCallback(VALIDSTRING char const* StringA,
                                  VALIDSTRING char const* StringB);
bool StringsAreEqualOrCallback(VALIDSTRING char const* StringA,
                               VALIDSTRING char const* StringB);
bool StringsAreEqualLowercaseOrCallback(VALIDSTRING char const* StringA,
                                        VALIDSTRING char const* StringB);

// Same as StringDifference, but callback suitable
CALLBACK_FN(int32x) StringDifferenceInternal(VALIDSTRING char const* StringA,
                                             VALIDSTRING char const* StringB);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_STRING_H
#endif
