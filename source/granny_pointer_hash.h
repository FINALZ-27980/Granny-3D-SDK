#if !defined(GRANNY_POINTER_HASH_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_pointer_hash.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(MemoryAllocatorGroup);

EXPTYPE struct pointer_hash;
EXPTYPE struct pointer_hash_iterator;

EXPAPI pointer_hash *NewPointerHash(void);
EXPAPI pointer_hash *NewPointerHashWithSize(int32x NumElements);
EXPAPI void DeletePointerHash(pointer_hash *Hash);
EXPAPI void ClearPointerHash(pointer_hash *Hash);

EXPAPI bool AddPointerToHash(pointer_hash &Hash, void const *Key, void *Data);
EXPAPI void RemovePointerFromHash(pointer_hash &Hash, void const *Key);

EXPAPI bool HashedPointerKeyExists(pointer_hash &Hash, void const *Key);

EXPAPI bool SetHashedPointerData(pointer_hash &Hash, void const *Key, void *Data);
EXPAPI bool GetHashedPointerData(pointer_hash &Hash, void const *Key, void *&Data);

EXPAPI pointer_hash_iterator* PointerHashBegin(pointer_hash &Hash);

EXPAPI void DeletePointerHashIterator(pointer_hash &Hash, pointer_hash_iterator* Iter);
EXPAPI void PointerHashIteratorNext(pointer_hash &Hash, pointer_hash_iterator* Iter);
EXPAPI bool PointerHashIteratorValid(pointer_hash &Hash, pointer_hash_iterator* Iter);

EXPAPI void const* PointerHashIteratorKey(pointer_hash_iterator* Iter);
EXPAPI void*       PointerHashIteratorData(pointer_hash_iterator* Iter);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_POINTER_HASH_H
#endif
