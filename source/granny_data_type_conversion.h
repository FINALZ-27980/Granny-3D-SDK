#if !defined(GRANNY_DATA_TYPE_CONVERSION_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_data_type_conversion.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(DataTypeConversionGroup);

struct data_type_definition;
struct variant;

EXPAPI typedef CALLBACK_FN(bool) conversion_handler(data_type_definition const* SourceType, void const* SourceMember,
                                                    data_type_definition const* DestType,   void*       DestMember);

EXPAPI typedef CALLBACK_FN(data_type_definition const*) variant_replace_fn(data_type_definition const* SourceType,
                                                                           void const* SourceMember);


EXPAPI GS_READ bool FindMatchingMember(data_type_definition const *SourceType,
                                       void const *SourceObject,
                                       char const *DestMemberName,
                                       variant* Result);

EXPAPI GS_PARAM void ConvertSingleObject(data_type_definition const *SourceType,
                                         void const *SourceObject,
                                         data_type_definition const *DestType,
                                         void *DestObject,
                                         conversion_handler* OverrideHandler);
EXPAPI GS_PARAM void MergeSingleObject(data_type_definition const *SourceType,
                                       void const *SourceObject,
                                       data_type_definition const *DestType,
                                       void *DestObject,
                                       conversion_handler* OverrideHandler);

EXPAPI GS_PARAM bool RemoveMember(data_type_definition* Type,
                                  void*                 Object,
                                  char const*           MemberName);

EXPAPI GS_SAFE void *ConvertTree(data_type_definition const *SourceType,
                                 void const *SourceTree,
                                 data_type_definition const *DestType,
                                 variant_replace_fn* VariantReplacer);

EXPAPI GS_READ int32x GetConvertedTreeSize(data_type_definition const *SourceType,
                                           void const *SourceTree,
                                           data_type_definition const *DestType,
                                           variant_replace_fn* VariantReplacer);
EXPAPI GS_PARAM void *ConvertTreeInPlace(data_type_definition const *SourceType,
                                         void const *SourceTree,
                                         data_type_definition const *DestType,
                                         void *Memory,
                                         variant_replace_fn* VariantReplacer);

EXPAPI typedef CALLBACK_FN(char *) rebase_pointers_string_callback(void *Data, uint32 Identifier);

EXPAPI GS_PARAM bool RebasePointers(data_type_definition const *Type,
                                    void *Data,
                                    intaddrx Offset,
                                    bool RebaseStrings);
EXPAPI GS_READ bool RebasePointersStringCallback(data_type_definition const *Type,
                                                 void *Data,
                                                 intaddrx Offset,
                                                 rebase_pointers_string_callback *Callback,
                                                 void *CallbackData);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_DATA_TYPE_CONVERSION_H
#endif
