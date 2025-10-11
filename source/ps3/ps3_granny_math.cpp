// ========================================================================
// $File: //jeffr/granny_29/rt/ps3/ps3_granny_math.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "ps3_granny_std.h"

#include "granny_memory.h"
#include "rrCore.h"
#include <math.h>
#include <stdlib.h>

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary


extern "C"
{
    F32 radfsqrt( F32 x )
    {
#if COMPILER_GCC
        F32 result;
        asm("fsqrt %0, %1" : "=f" (result) : "f" (x));
        return( result );
#elif COMPILER_SNC
        return __builtin_fsqrt(x);
#else
#error "unsupported compiler"
#endif
    }

    F32 radsqrt( F32 x )
    {
#if COMPILER_GCC
        F32 result;
        asm("fsqrt %0, %1" : "=f" (result) : "f" (x));
        return( result );
#elif COMPILER_SNC
        return __builtin_fsqrt(x);
#else
#error "unsupported compiler"
#endif
    }

    typedef union conv
    {
        F32 f;
        S32 s;
    } conv;

    F32 radfloor( F32 x )
    {
        S32 ix,e;
        F32 ret;

        ix = ( (conv*) &x )->s;

        e = ( ( ix >> 23 ) & 0xff ) - 0x7f;

        if ( e < 0 )
            return( ( ( ((unsigned)ix) & 0x80000000 ) && ( ((unsigned)ix) != 0x80000000 ) ) ? -1.0f : 0.0f );

        e = 23 - e;
        if ( e <= 0 )
            return( x );

        ix &= ( 0xffffffff << e );

        ret = ( (conv*) &ix )->f;

        if ( ( x < 0 ) && ( ret != x ) )
            ret -= 1.0f;

        return( ret );
    }

    F32 radceil( F32 x )
    {
        F32 ret;

        ret = radfloor( x );
        if( ret < x )
            ret += 1.0f;

        return( ret );
    }


#define PI14  0.785398163f
#define PI24  1.570796327f
#define PI34  2.35619449f
#define PI44  3.141592654f
#define PI54  3.926990817f
#define PI64  4.71238898f
#define PI74  5.497787144f
#define PI84  6.283185307f

    static F32 ll_sinf( F32 x )
    {
        F32 x2, x3, x5, x7;

        if ( x < 2.0e-09f )
        {
            return( 0.0f );
        }

        x2 = x * x;
        x3 = x2 * x;
        x5 = x2 * x3;
        x7 = x5 * x2;

        return( x + ( x3 * -0.166666567325592041015625f ) + ( x5 * 0.00833220803f ) - ( x7 * 0.000195168955f ) );
    }

    static  F32 ll_cosf( F32 x )
    {
        F32 x2, x4, x6, x8;

        if ( x < 2.0e-09f )
        {
            return( 1.0f );
        }

        x2 = x * x;
        x4 = x2 * x2;
        x6 = x2 * x4;
        x8 = x4 * x4;

        return( 1.0f - ( x2 * 0.5f ) + ( x4 * 0.041666667908f ) + ( x6 * -0.0013888889225f ) + ( x8 * 0.000024801587642f ) );
    }

    F32 radcos_no_clamp( F32 x ) // positive values from 0 to 2PI
    {
        if ( x <= PI44 )
        {
            if ( x <= PI24 )
            {
                if ( x <= PI14 )
                {
                    return( ll_cosf( x ) );
                }
                else
                {
                    return( ll_sinf( PI24 - x ) );
                }
            }
            else
            {
                if ( x <= PI34 )
                {
                    return( -ll_sinf( x - PI24 ) );
                }
                else
                {
                    return( -ll_cosf( PI44 - x ) );
                }
            }
        }
        else
        {
            if ( x <= PI64 )
            {
                if ( x <= PI54 )
                {
                    return( -ll_cosf( x - PI44 ) );
                }
                else
                {
                    return( -ll_sinf( PI64 - x ) );
                }
            }
            else
            {
                if ( x <= PI74 )
                {
                    return( ll_sinf( x - PI64 ) );
                }
                else
                {
                    return( ll_cosf( PI84 - x ) );
                }
            }
        }
    }

    F32 radcos( F32 x )
    {
        if ( x < 0.0f )
            x = -x;

        if ( x > PI84 )
        {
            x -= radfloor( x / PI84 ) * PI84;
        }

        return( radcos_no_clamp( x ) );
    }

    F32 radsin( F32 x )
    {
        if ( ( x > PI84 ) || ( x < -PI84 ) )
        {
            x -= radfloor( x / PI84 ) * PI84;
        }

        x -= PI24;
        if ( x < 0.0f )
            x = -x;

        if ( x > PI84 )
            x -= PI84;

        return( radcos_no_clamp( x ) );
    }

    static F32 ll_atan( F32 x )
    {
        F32 x2;

        x2 = x * x;
        return( x * ( 1.6867629106f + x2 * 0.4378497304f ) / ( 1.6867633134f + x2 ) );
    }

#define TAN12THPI 0.267949192f
#define TAN6THPI 0.577350269f
#define PI16 0.523598776f
#define PI13 1.047197551f

    F32 radatan( F32 x )
    {
        if ( x < 0.0f )
        {
            x = -x;

            if ( x > 1.0f )
            {
                x = 1.0f / x;

                if ( x > TAN12THPI )
                    return( ll_atan( ( x - TAN6THPI ) / ( 1 + TAN6THPI * x ) ) - PI13 );
                else
                    return( ll_atan( x ) - PI24 );
            }
            else
            {
                if ( x > TAN12THPI )
                    return( - ( ll_atan( ( x - TAN6THPI ) / ( 1 + TAN6THPI * x ) ) + PI16 ) );
                else
                    return( -ll_atan( x ) );
            }
        }
        else
        {
            if ( x > 1.0f )
            {
                x = 1.0f / x;

                if ( x > TAN12THPI )
                    return( PI13 - ll_atan( ( x - TAN6THPI ) / ( 1 + TAN6THPI * x ) ) );
                else
                    return( PI24 - ll_atan( x ) );
            }
            else
            {
                if ( x > TAN12THPI )
                    return( ll_atan( ( x - TAN6THPI ) / ( 1 + TAN6THPI * x ) ) + PI16 );
                else
                    return( ll_atan( x ) );
            }
        }
    }

    F32 radatan2( F32 y, F32 x )
    {
        return atan2f(y, x);
    }

    F32 radfabs ( F32 arg )
    {
        if ( arg < 0.0f )
            return -arg;
        else
            return arg;
    }

    F32 radlog2 ( F32 arg )
    {
        S32 ix,e;

        ix = ( (conv*) &arg )->s;

        e = ( ( ix >> 23 ) & 0xff ) - 0x7f;

        return( (F32)e );
    }


#if 0
    F32 radpow ( F32 a, F32 b )
    {
        return powf ( a, b );
    }
#endif


}

