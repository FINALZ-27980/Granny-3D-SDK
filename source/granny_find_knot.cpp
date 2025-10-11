// ========================================================================
// $File: //jeffr/granny_29/rt/granny_find_knot.cpp $
// $DateTime: 2012/03/30 12:26:04 $
// $Change: 36903 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_find_knot.h"

#include "granny_assert.h"
#include "granny_floats.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

USING_GRANNY_NAMESPACE;

#define CHECK_FIND_KNOT 0
#if CHECK_FIND_KNOT
static int32x OriginalFindKnot(int32x KnotCount, real32 const *Knots, real32 t);
#endif

/*
  Note: the FindKnot routines return the knot that's _ahead of_ where you
  are at time t. However, it clamps to KnotCount-1, so it will never reference
  off the end of the array.
*/

// Local version of std::upper_bound so we don't have to bring in std::, Android doesn't
// have the stl at this point.
template <class int_type>
int_type const*
local_upper_bound(int_type const* _First,
                  int_type const* _Last,
                  int_type        _Val)
{
    Assert(_First <= _Last);

    int_type const* First = _First;
    int_type const* Last  = _Last;
    intaddrx Count = Last - First;
    for (; 0 < Count; )
    {
        // divide and conquer, find half that contains answer
        intaddrx Count2 = Count / 2;
        int_type const* Mid = First + Count2;
        Assert(Mid <= Last);

        if (!(_Val < *Mid))
        {
            First = ++Mid, Count -= Count2 + 1;
        }
        else
        {
            Count = Count2;
        }
    }

    return First;
}

template uint8  const* local_upper_bound<uint8> (uint8  const* _First, uint8  const* _Last, uint8  _Val);
template uint16 const* local_upper_bound<uint16>(uint16 const* _First, uint16 const* _Last, uint16 _Val);
template uint32 const* local_upper_bound<uint32>(uint32 const* _First, uint32 const* _Last, uint32 _Val);


template <class int_type>
static int32x
FindKnotInteger(int32x KnotCount, int_type const *Knots, int_type const IntT)
{
    Assert(KnotCount != 0);

    intaddrx KnotIndex =
        local_upper_bound(Knots, Knots + KnotCount, IntT) - Knots;

#if DEBUG
    Assert((KnotIndex >= 0) && (KnotIndex <= KnotCount));
    if ( KnotIndex < KnotCount )
    {
        Assert ( Knots[KnotIndex] > IntT );
    }
    if ( KnotIndex > 0 )
    {
        Assert ( Knots[KnotIndex-1] <= IntT );
    }
#endif

    if (KnotIndex == KnotCount)
    {
        // Stop it falling off the end of the knot array.
        // This is slightly artificial, but calling code gets confused otherwise.
        --KnotIndex;
    }

    return int32x(KnotIndex);
}


int32x GRANNY
FindKnot(int32x KnotCount, real32 const *Knots, real32 t)
{
    Assert(KnotCount != 0);

    uint32 const* IntKnots = (uint32 const*)(void*)Knots;

    // even with the clamp fix, fastmath switches can cause -0 to show up here.  Just mask
    // for that specific value.
    uint32 IntT = GetInt32FromReal32(t);
    Assert((IntT & 0x80000000) == 0 || IntT == 0x80000000);

    int32x const KnotIndex = FindKnotInteger(KnotCount, IntKnots, IntT & ~0x80000000);

#if CHECK_FIND_KNOT
    int32x const OrigKnotIndex = OriginalFindKnot(KnotCount, Knots, t);
    Assert(OrigKnotIndex == KnotIndex);
#endif

    return KnotIndex;
}

int32x GRANNY
FindKnotUint16(int32x KnotCount, uint16 const *Knots, uint16 t)
{
    Assert(KnotCount != 0);

    return FindKnotInteger(KnotCount, Knots, t);
}

int32x GRANNY
FindKnotUint8(int32x KnotCount, uint8 const *Knots, uint8 t)
{
    Assert(KnotCount != 0);

    return FindKnotInteger(KnotCount, Knots, t);
}


// Note: Need to make these linear again...

int32x GRANNY
FindCloseKnot(int32x KnotCount, real32 const *Knots,
              real32 t, int32x StartingIndex)
{
    return FindKnot ( KnotCount, Knots, t );
}

int32x GRANNY
FindCloseKnotUint16(int32x KnotCount, uint16 const *Knots,
                    uint16 t, int32x StartingIndex)
{
    return FindKnotUint16 ( KnotCount, Knots, t );
}

int32x GRANNY
FindCloseKnotUint8(int32x KnotCount, uint8 const *Knots,
                   uint8 t, int32x StartingIndex)
{
    return FindKnotUint8 ( KnotCount, Knots, t );
}



#if CHECK_FIND_KNOT
static int32x OriginalFindKnot(int32x KnotCount, real32 const *Knots, real32 t)
{
#if DEBUG
    real32 const* OriginalKnots = Knots;
#endif
    int32x KnotWindow = KnotCount;
    int32x KnotIndex = 0;

    while(KnotWindow > 1)
    {
        if(KnotWindow & 1)
        {
            // It's odd
            KnotWindow = (KnotWindow / 2);
            Assert ( KnotIndex+KnotWindow < KnotCount );
            if(t >= Knots[KnotWindow])
            {
                KnotIndex += KnotWindow;
                Knots += KnotWindow;
            }

            ++KnotWindow;
        }
        else
        {
            // It's even
            KnotWindow = (KnotWindow / 2);
            Assert ( KnotIndex+KnotWindow < KnotCount );
            if(t >= Knots[KnotWindow])
            {
                if((KnotIndex+KnotWindow+1 >= KnotCount) || (t < Knots[KnotWindow + 1]))
                {
                    KnotIndex += KnotWindow;
                    Knots += KnotWindow;
                    break;
                }
                else
                {
                    KnotIndex += KnotWindow + 1;
                    Knots += KnotWindow + 1;
                    --KnotWindow;
                }
            }
        }
    }

    // Slight adjustment for duplicate knots.
    while ( ( KnotIndex < KnotCount ) && ( Knots[0] <= t ) )
    {
        ++KnotIndex;
        ++Knots;
    }

#if DEBUG
    Assert((KnotIndex >= 0) && (KnotIndex <= KnotCount));
    if ( KnotIndex < KnotCount )
    {
        Assert ( OriginalKnots[KnotIndex] > t );
    }
    if ( KnotIndex > 0 )
    {
        Assert ( OriginalKnots[KnotIndex-1] <= t );
    }
#endif

    if ( ( KnotIndex == KnotCount ) && ( KnotIndex > 0 ) )
    {
        // Stop it falling off the end of the knot array.
        // This is slightly artificial, but calling code gets confused otherwise.
        KnotIndex--;
    }

    return(KnotIndex);
}
#endif
