#if !defined(GRANNY_MEMORY_ARENA_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_memory_arena.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GRANNY_MEMORY_H)
#include "granny_memory.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(MemoryArenaGroup);

#define ArenaPush(Arena, type) (type *)MemoryArenaPush(Arena, SizeOf(type))
#define ArenaPushSize(Arena, Size) MemoryArenaPush(Arena, Size)
#define ArenaPushArray(Arena, Count, type) (type *)MemoryArenaPush(Arena, (Count) * SizeOf(type))

EXPTYPE struct memory_arena;

EXPAPI GS_SAFE memory_arena *NewMemoryArena(void);
EXPAPI GS_PARAM void SetArenaAlignment(memory_arena& Arena, uintaddrx Align);
uintaddrx GetArenaAlignment(memory_arena& Arena);
EXPAPI GS_PARAM void ClearArena(memory_arena *Arena);
EXPAPI GS_PARAM void FreeMemoryArena(memory_arena *Arena);

EXPAPI GS_PARAM void* MemoryArenaPush(memory_arena &Arena, uintaddrx Size);

EXPAPI GS_PARAM char* MemoryArenaPushString(memory_arena &Arena, char const *String);
EXPAPI GS_PARAM void* MemoryArenaPushBinary(memory_arena &Arena, uintaddrx Size, void const *Data);
EXPAPI GS_PARAM char* MemoryArenaPushConcat(memory_arena &Arena, char const* First, char const* Second);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MEMORY_ARENA_H
#endif
