// ========================================================================
// $File: //jeffr/granny_29/rt/granny_pointer_hash.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_pointer_hash.h"

#include "granny_log.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_telemetry.h"
#include "granny_parameter_checking.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode HashLogMessage

USING_GRANNY_NAMESPACE;
BEGIN_GRANNY_NAMESPACE;

struct hash_entry
{
    void const *Key;
    void *Data;

    hash_entry *Left;
    hash_entry *Right;
};

struct pointer_hash_iterator
{
    hash_entry* Entry;
};

inline int32x HashKeyDifference(const void* a, const void* b)
{
    return PtrDiffSignOnly(a, b);
}

#define CONTAINER_NAME pointer_hash
#define CONTAINER_ITEM_TYPE hash_entry
#define CONTAINER_ADD_FIELDS void const *Key, void *Data
#define CONTAINER_ADD_ASSIGN(Item) (Item)->Key = Key; (Item)->Data = Data;
#define CONTAINER_COMPARE_ITEMS(Item1, Item2) HashKeyDifference((Item1)->Key, (Item2)->Key)
#define CONTAINER_FIND_FIELDS void const *Key
#define CONTAINER_COMPARE_FIND_FIELDS(Item) HashKeyDifference(Key, (Item)->Key)
#define CONTAINER_SORTED 1
#define CONTAINER_KEEP_LINKED_LIST 0
#define CONTAINER_SUPPORT_DUPES 0
#define CONTAINER_NUM_PER_ALLOCATION 1024
#include "granny_contain.inl"

END_GRANNY_NAMESPACE;

struct pointer_hash_status
{
    int32x NumQueries;
    int32x NumExistChecks;
    int32x NumSets;
    int32x NumInserts;
    int32x NumDeletes;
};
static pointer_hash_status HashStatus = { 0 };


pointer_hash *GRANNY
NewPointerHashWithSize(int32x NumElements)
{
    CheckCondition(NumElements >= 0, return 0);

    pointer_hash *Hash = Allocate(pointer_hash, AllocationUnknown);
    if (Hash)
    {
        if (!Initialize(Hash, NumElements))
        {
            Deallocate(Hash);
            Hash = 0;
        }
    }

    return Hash;
}

pointer_hash *GRANNY
NewPointerHash(void)
{
    return NewPointerHashWithSize(0);
}

void GRANNY
DeletePointerHash(pointer_hash *Hash)
{
    if(Hash)
    {
        FreeMemory(Hash);
        Deallocate(Hash);
    }
}

void GRANNY
ClearPointerHash(pointer_hash *Hash)
{
    if (Hash)
    {
        FreeMemory(Hash);
    }
}


bool GRANNY
AddPointerToHash(pointer_hash &Hash, void const *Key, void *Data)
{
    Assert(Find(&Hash, Key) == 0);

    GRANNY_INC_INT_ACCUMULATOR(HashStatus.NumInserts);

    hash_entry *Entry = Add(&Hash, Key, Data);
    if (!Entry)
    {
        Log0(ErrorLogMessage, HashLogMessage, "Out of space in pointer hash table");
    }

    return (Entry != 0);
}

void GRANNY
RemovePointerFromHash(pointer_hash &Hash, void const *Key)
{
    GRANNY_INC_INT_ACCUMULATOR(HashStatus.NumDeletes);

    hash_entry *Entry = Find(&Hash, Key);
    if (Entry)
        Remove(&Hash, Entry);
}


bool GRANNY
SetHashedPointerData(pointer_hash &Hash, void const *Key, void *Data)
{
    GRANNY_INC_INT_ACCUMULATOR(HashStatus.NumSets);

    hash_entry *Entry = Find(&Hash, Key);
    if (Entry)
    {
        Entry->Data = Data;
    }
    else
    {
        if (Add(&Hash, Key, Data) == 0)
        {
            Log0(ErrorLogMessage, HashLogMessage, "Out of space in pointer hash table");
            return false;
        }
    }

    return true;
}

bool GRANNY
GetHashedPointerData(pointer_hash &Hash, void const *Key, void *&Data)
{
    GRANNY_INC_INT_ACCUMULATOR(HashStatus.NumQueries);

    hash_entry *Entry = Find(&Hash, Key);
    if(Entry)
    {
        Data = Entry->Data;
        return true;
    }

    Data = 0;

    return false;
}

bool GRANNY
HashedPointerKeyExists(pointer_hash &Hash, void const *Key)
{
    GRANNY_INC_INT_ACCUMULATOR(HashStatus.NumExistChecks);

    return (Find(&Hash, Key) != NULL);
}


pointer_hash_iterator* GRANNY
PointerHashBegin(pointer_hash& Hash)
{
    pointer_hash_iterator* Iter = Allocate(pointer_hash_iterator, AllocationUnknown);
    if (!Iter)
    {
        return 0;
    }

    Iter->Entry = First(&Hash);
    if (Iter->Entry == 0)
    {
        DeletePointerHashIterator(Hash, Iter);
        Iter = 0;
    }

    return Iter;
}

void GRANNY
DeletePointerHashIterator(pointer_hash& Hash, pointer_hash_iterator* Iter)
{
    if (Iter)
        Deallocate(Iter);
}

void GRANNY
PointerHashIteratorNext(pointer_hash& Hash, pointer_hash_iterator* Iter)
{
    if (!Iter || !Iter->Entry)
        return;

    Iter->Entry = Next(&Hash, Iter->Entry);
}

bool GRANNY
PointerHashIteratorValid(pointer_hash& Hash, pointer_hash_iterator* Iter)
{
    if (!Iter || !Iter->Entry)
        return false;

    return Iter->Entry != 0;
}

void const* GRANNY
PointerHashIteratorKey(pointer_hash_iterator* Iter)
{
    CheckPointerNotNull(Iter,        return 0);
    CheckPointerNotNull(Iter->Entry, return 0);

    return Iter->Entry->Key;
}

void* GRANNY
PointerHashIteratorData(pointer_hash_iterator* Iter)
{
    CheckPointerNotNull(Iter,        return 0);
    CheckPointerNotNull(Iter->Entry, return 0);

    return Iter->Entry->Data;
}


BEGIN_GRANNY_NAMESPACE;
void PointerHashFrameStats()
{
#define PRE "Granny/PointerHash/"
    GRANNY_EMIT_INT_ACCUMULATOR(PRE "NumQueries(ptrhash)", HashStatus.NumQueries);
    GRANNY_EMIT_INT_ACCUMULATOR(PRE "NumExistChecks(ptrhash)", HashStatus.NumExistChecks);
    GRANNY_EMIT_INT_ACCUMULATOR(PRE "NumSets(ptrhash)", HashStatus.NumSets);
    GRANNY_EMIT_INT_ACCUMULATOR(PRE "NumInserts(ptrhash)", HashStatus.NumInserts);
    GRANNY_EMIT_INT_ACCUMULATOR(PRE "NumDeletes(ptrhash)", HashStatus.NumDeletes);
}
END_GRANNY_NAMESPACE;

