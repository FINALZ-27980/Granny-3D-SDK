#if !defined(GRANNY_SIEVE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_sieve.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE;

struct sieve;

sieve *NewSieve(int32x GroupCount, int32x UnitSize);
int32x GetUsedGroupCount(sieve &Sieve);
void ClearSieve(sieve &Sieve);
void FreeSieve(sieve *Sieve);
void *AddSieveUnit(sieve &Sieve, int32x GroupIndex);
int32x GetSieveGroupUnitCount(sieve &Sieve, int32x GroupIndex);
void SerializeSieveGroup(sieve &Sieve, int32x GroupIndex, void *Dest);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_SIEVE_H
#endif
