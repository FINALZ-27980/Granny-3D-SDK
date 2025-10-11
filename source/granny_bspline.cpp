// ========================================================================
// $File: //jeffr/granny_29/rt/granny_bspline.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_bspline.h"

#include "granny_memory.h"
#include "granny_memory_ops.h"
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

#undef SubsystemCode
#define SubsystemCode BSplineLogMessage

USING_GRANNY_NAMESPACE;

// Make sure that the compiler knows that these arrays don't
// overlap...
#if DEBUG
  #define UNALIASED_SPLINE_ARGS_NOTI(Deg, Dim)                                \
      {                                                                       \
          real32 const* tiStart = tiAliased - Deg;                            \
          real32 const* tiEnd   = tiAliased + Deg;                            \
          real32 const* piStart = piAliased - (Deg*Dim);                      \
          real32 const* piEnd   = piAliased + Dim;                            \
          real32* ResultStart   = ResultAliased;                              \
          real32* ResultEnd     = ResultAliased + Dim;                        \
          Assert(!PtrRangesOverlap(tiStart, tiEnd, piStart, piEnd));          \
          Assert(!PtrRangesOverlap(piStart, piEnd, ResultStart, ResultEnd));  \
          Assert(!PtrRangesOverlap(tiStart, tiEnd, ResultStart, ResultEnd));  \
      }                                                                       \
      real32 const* NOALIAS pi = piAliased;                                   \
      real32* NOALIAS Result   = ResultAliased
#else
  #define UNALIASED_SPLINE_ARGS_NOTI(Deg, Dim)    \
      real32 const* NOALIAS pi = piAliased;       \
      real32* NOALIAS Result   = ResultAliased
#endif

#define UNALIASED_SPLINE_ARGS(Deg, Dim)         \
    UNALIASED_SPLINE_ARGS_NOTI(Deg, Dim);       \
    real32 const* NOALIAS ti = tiAliased


static void
RecursiveCoefficients(
    int32x const d,
    int32x const k,
    real32 const *ti,
    real32 const t,
    real32 *ci,
    real32 const C)
{
    if(k == 0)
    {
        ci[0] += C;
    }
    else
    {
        real32 Mint = ti[-1];
        real32 Maxt = ti[d - k];

        real32 Blend = (t - Mint) / (Maxt - Mint);

        RecursiveCoefficients(d, k-1, ti - 1, t, ci,  C * (1.0f-Blend));
        RecursiveCoefficients(d, k-1, ti, t, ci, C*Blend);
    }
}

void GRANNY
Coefficients(int32x const d,
             real32 const *ti,
             real32 const t,
             real32 *ci)
{
    Assert(d >= 0);

    switch(d)
    {
        case 0:
        {
            // This is a silly case, but included for completeness
            ci[0] = 1.0f;
        } break;

        case 1:
        {
            // Linear spline
            LinearCoefficients(ti[-1], ti[0], t,
                               &ci[-1], &ci[0]);
        } break;

        case 2:
        {
            // Quadratic spline
            QuadraticCoefficients(ti[-2], ti[-1], ti[0], ti[1], t,
                                  &ci[-2], &ci[-1], &ci[0]);
        } break;

        case 3:
        {
            // Cubic spline
            CubicCoefficients(ti[-3], ti[-2], ti[-1], ti[0], ti[1], ti[2], t,
                              &ci[-3], &ci[-2], &ci[-1], &ci[0]);
        } break;

        default:
        {
            // Higher-order spline
            RecursiveCoefficients(d, d, ti, t, ci, 1.0f);
        } break;
    }
}

void GRANNY
SampleBSpline0x1(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS_NOTI(0,1);

    Result[0] = pi[0];
}

void GRANNY
SampleBSpline0x2(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS_NOTI(0,2);

    Result[0] = pi[0];
    Result[1] = pi[1];
}

void GRANNY
SampleBSpline0x3(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS_NOTI(0,3);

    Result[0] = pi[0];
    Result[1] = pi[1];
    Result[2] = pi[2];
}

void GRANNY
SampleBSpline0x4(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS_NOTI(0,4);

    Result[0] = pi[0];
    Result[1] = pi[1];
    Result[2] = pi[2];
    Result[3] = pi[3];
}

void GRANNY
SampleBSpline0x9(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS_NOTI(0,9);

    Result[0] = pi[0];
    Result[1] = pi[1];
    Result[2] = pi[2];
    Result[3] = pi[3];
    Result[4] = pi[4];
    Result[5] = pi[5];
    Result[6] = pi[6];
    Result[7] = pi[7];
    Result[8] = pi[8];
}

void GRANNY
SampleBSpline0xN(real32 const *tiAliased, real32 const *piAliased,
                 int32 Dimension, bool Normalize,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS_NOTI(0,Dimension);

    {for(int32x d = 0; d < Dimension; ++d)
    {
        Result[d] = pi[d];
    }}

    if (Normalize)
        NormalizeN(Result, Dimension);
}


void GRANNY
SampleBSpline1x1(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(1,1);

    real32 c1, c0;
    LinearCoefficients(ti[-1], ti[0], t, &c1, &c0);

    Result[0] = c1*pi[-1] + c0*pi[0];
}

void GRANNY
SampleBSpline1x2(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(0,2);

    real32 c1, c0;
    LinearCoefficients(ti[-1], ti[0], t, &c1, &c0);

    Result[0] = c1*pi[-2] + c0*pi[0];
    Result[1] = c1*pi[-1] + c0*pi[1];
}

void GRANNY
SampleBSpline1x3(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(1,3);

    real32 c1, c0;
    LinearCoefficients(ti[-1], ti[0], t, &c1, &c0);

    Result[0] = c1*pi[-3] + c0*pi[0];
    Result[1] = c1*pi[-2] + c0*pi[1];
    Result[2] = c1*pi[-1] + c0*pi[2];
}

void GRANNY
SampleBSpline1x3n(real32 const *tiAliased, real32 const *piAliased,
                  real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(1,3);

    real32 c1, c0;
    LinearCoefficients(ti[-1], ti[0], t, &c1, &c0);

    Result[0] = (c1*pi[-3] + c0*pi[0]);
    Result[1] = (c1*pi[-2] + c0*pi[1]);
    Result[2] = (c1*pi[-1] + c0*pi[2]);

    NormalizeCloseToOne3(Result);
}
void GRANNY
SampleBSpline1x4(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(1,4);

    real32 c1, c0;
    LinearCoefficients(ti[-1], ti[0], t, &c1, &c0);

    Result[0] = c1*pi[-4] + c0*pi[0];
    Result[1] = c1*pi[-3] + c0*pi[1];
    Result[2] = c1*pi[-2] + c0*pi[2];
    Result[3] = c1*pi[-1] + c0*pi[3];
}

void GRANNY
SampleBSpline1x4n(real32 const *tiAliased, real32 const *piAliased,
                  real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(1,4);

    real32 c1, c0;
    LinearCoefficients(ti[-1], ti[0], t, &c1, &c0);

    Result[0] = (c1*pi[-4] + c0*pi[0]);
    Result[1] = (c1*pi[-3] + c0*pi[1]);
    Result[2] = (c1*pi[-2] + c0*pi[2]);
    Result[3] = (c1*pi[-1] + c0*pi[3]);

    Normalize4(Result);
}

void GRANNY
SampleBSpline1x9(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(1,9);

    real32 c1, c0;
    LinearCoefficients(ti[-1], ti[0], t, &c1, &c0);

    Result[0] = c1*pi[-9] + c0*pi[0];
    Result[1] = c1*pi[-8] + c0*pi[1];
    Result[2] = c1*pi[-7] + c0*pi[2];
    Result[3] = c1*pi[-6] + c0*pi[3];
    Result[4] = c1*pi[-5] + c0*pi[4];
    Result[5] = c1*pi[-4] + c0*pi[5];
    Result[6] = c1*pi[-3] + c0*pi[6];
    Result[7] = c1*pi[-2] + c0*pi[7];
    Result[8] = c1*pi[-1] + c0*pi[8];
}

void GRANNY
SampleBSpline1xN(real32 const *tiAliased, real32 const *piAliased,
                 int32 Dimension, bool Normalize,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(1,Dimension);

    real32 c1, c0;
    LinearCoefficients(ti[-1], ti[0], t, &c1, &c0);

    {for(int32x d = 0; d < Dimension; ++d)
    {
        Result[d] = (c1 * pi[(Dimension * -1) + d] +
                     c0 * pi[(Dimension *  0) + d]);
    }}

    if (Normalize)
        NormalizeN(Result, Dimension);
}

void GRANNY
SampleBSpline2x1(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(2,1);

    real32 c2, c1, c0;
    QuadraticCoefficients(ti[-2], ti[-1], ti[0], ti[1],
                          t, &c2, &c1, &c0);

    Result[0] = c2*pi[-2] + c1*pi[-1] + c0*pi[0];
}

void GRANNY
SampleBSpline2x2(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(2,2);

    real32 c2, c1, c0;
    QuadraticCoefficients(ti[-2], ti[-1], ti[0], ti[1],
                          t, &c2, &c1, &c0);

    Result[0] = c2*pi[-4] + c1*pi[-2] + c0*pi[0];
    Result[1] = c2*pi[-3] + c1*pi[-1] + c0*pi[1];
}

void GRANNY
SampleBSpline2x3(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(2,3);

    real32 c2, c1, c0;
    QuadraticCoefficients(ti[-2], ti[-1], ti[0], ti[1],
                          t, &c2, &c1, &c0);

    Result[0] = c2*pi[-6] + c1*pi[-3] + c0*pi[0];
    Result[1] = c2*pi[-5] + c1*pi[-2] + c0*pi[1];
    Result[2] = c2*pi[-4] + c1*pi[-1] + c0*pi[2];
}

void GRANNY
SampleBSpline2x3n(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(2,3);

    real32 c2, c1, c0;
    QuadraticCoefficients(ti[-2], ti[-1], ti[0], ti[1],
                          t, &c2, &c1, &c0);

    Result[0] = c2*pi[-6] + c1*pi[-3] + c0*pi[0];
    Result[1] = c2*pi[-5] + c1*pi[-2] + c0*pi[1];
    Result[2] = c2*pi[-4] + c1*pi[-1] + c0*pi[2];

    NormalizeCloseToOne3 ( Result );
}

void GRANNY
SampleBSpline2x4(real32 const *tiAliased, real32 const *piAliased,
                  real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(2,4);

    real32 c2, c1, c0;
    QuadraticCoefficients(ti[-2], ti[-1], ti[0], ti[1],
                          t, &c2, &c1, &c0);

    Result[0] = c2*pi[-8] + c1*pi[-4] + c0*pi[0];
    Result[1] = c2*pi[-7] + c1*pi[-3] + c0*pi[1];
    Result[2] = c2*pi[-6] + c1*pi[-2] + c0*pi[2];
    Result[3] = c2*pi[-5] + c1*pi[-1] + c0*pi[3];
}

void GRANNY
SampleBSpline2x4n(real32 const *tiAliased, real32 const *piAliased,
                  real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(2,4);

    real32 c2, c1, c0;
    QuadraticCoefficients(ti[-2], ti[-1], ti[0], ti[1],
                          t, &c2, &c1, &c0);

    Result[0] = c2*pi[-8] + c1*pi[-4] + c0*pi[0];
    Result[1] = c2*pi[-7] + c1*pi[-3] + c0*pi[1];
    Result[2] = c2*pi[-6] + c1*pi[-2] + c0*pi[2];
    Result[3] = c2*pi[-5] + c1*pi[-1] + c0*pi[3];

    Normalize4(Result);
}

void GRANNY
SampleBSpline2x9(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(2,9);

    real32 c2, c1, c0;
    QuadraticCoefficients(ti[-2], ti[-1], ti[0], ti[1],
                          t, &c2, &c1, &c0);

    Result[0] = c2*pi[-18] + c1*pi[-9] + c0*pi[0];
    Result[1] = c2*pi[-17] + c1*pi[-8] + c0*pi[1];
    Result[2] = c2*pi[-16] + c1*pi[-7] + c0*pi[2];
    Result[3] = c2*pi[-15] + c1*pi[-6] + c0*pi[3];
    Result[4] = c2*pi[-14] + c1*pi[-5] + c0*pi[4];
    Result[5] = c2*pi[-13] + c1*pi[-4] + c0*pi[5];
    Result[6] = c2*pi[-12] + c1*pi[-3] + c0*pi[6];
    Result[7] = c2*pi[-11] + c1*pi[-2] + c0*pi[7];
    Result[8] = c2*pi[-10] + c1*pi[-1] + c0*pi[8];
}

void GRANNY
SampleBSpline2xN(real32 const *tiAliased, real32 const *piAliased,
                 int32 Dimension, bool Normalize,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(2,Dimension);

    real32 c2, c1, c0;
    QuadraticCoefficients(ti[-2], ti[-1], ti[0], ti[1],
                          t, &c2, &c1, &c0);

    {for(int32x d = 0; d < Dimension; ++d)
    {
        Result[d] = (c2 * pi[(Dimension * -2) + d] +
                     c1 * pi[(Dimension * -1) + d] +
                     c0 * pi[(Dimension *  0) + d]);
    }}

    if (Normalize)
        NormalizeN(Result, Dimension);
}


void GRANNY
SampleBSpline3x1(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(3,1);

    real32 c3, c2, c1, c0;
    CubicCoefficients(ti[-3], ti[-2], ti[-1], ti[0], ti[1], ti[2],
                      t, &c3, &c2, &c1, &c0);

    Result[0] = c3*pi[-3] + c2*pi[-2] + c1*pi[-1] + c0*pi[0];
}

void GRANNY
SampleBSpline3x2(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(3,2);

    real32 c3, c2, c1, c0;
    CubicCoefficients(ti[-3], ti[-2], ti[-1], ti[0], ti[1], ti[2],
                      t, &c3, &c2, &c1, &c0);

    Result[0] = c3*pi[-6] + c2*pi[-4] + c1*pi[-2] + c0*pi[0];
    Result[1] = c3*pi[-5] + c2*pi[-3] + c1*pi[-1] + c0*pi[1];
}

void GRANNY
SampleBSpline3x3(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(3,3);

    real32 c3, c2, c1, c0;
    CubicCoefficients(ti[-3], ti[-2], ti[-1], ti[0], ti[1], ti[2],
                      t, &c3, &c2, &c1, &c0);

    Result[0] = c3*pi[-9] + c2*pi[-6] + c1*pi[-3] + c0*pi[0];
    Result[1] = c3*pi[-8] + c2*pi[-5] + c1*pi[-2] + c0*pi[1];
    Result[2] = c3*pi[-7] + c2*pi[-4] + c1*pi[-1] + c0*pi[2];
}

void GRANNY
SampleBSpline3x3n(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(3,3);

    real32 c3, c2, c1, c0;
    CubicCoefficients(ti[-3], ti[-2], ti[-1], ti[0], ti[1], ti[2],
                      t, &c3, &c2, &c1, &c0);

    Result[0] = c3*pi[-9] + c2*pi[-6] + c1*pi[-3] + c0*pi[0];
    Result[1] = c3*pi[-8] + c2*pi[-5] + c1*pi[-2] + c0*pi[1];
    Result[2] = c3*pi[-7] + c2*pi[-4] + c1*pi[-1] + c0*pi[2];

    NormalizeCloseToOne3 ( Result );
}

void GRANNY
SampleBSpline3x4(real32 const *tiAliased, real32 const *piAliased,
                  real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(3,4);

    real32 c3, c2, c1, c0;
    CubicCoefficients(ti[-3], ti[-2], ti[-1], ti[0], ti[1], ti[2],
                      t, &c3, &c2, &c1, &c0);

    Result[0] = c3*pi[-12] + c2*pi[-8] + c1*pi[-4] + c0*pi[0];
    Result[1] = c3*pi[-11] + c2*pi[-7] + c1*pi[-3] + c0*pi[1];
    Result[2] = c3*pi[-10] + c2*pi[-6] + c1*pi[-2] + c0*pi[2];
    Result[3] = c3*pi[ -9] + c2*pi[-5] + c1*pi[-1] + c0*pi[3];
}

void GRANNY
SampleBSpline3x4n(real32 const *tiAliased, real32 const *piAliased,
                  real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(3,4);

    real32 c3, c2, c1, c0;
    CubicCoefficients(ti[-3], ti[-2], ti[-1], ti[0], ti[1], ti[2],
                      t, &c3, &c2, &c1, &c0);

    Result[0] = c3*pi[-12] + c2*pi[-8] + c1*pi[-4] + c0*pi[0];
    Result[1] = c3*pi[-11] + c2*pi[-7] + c1*pi[-3] + c0*pi[1];
    Result[2] = c3*pi[-10] + c2*pi[-6] + c1*pi[-2] + c0*pi[2];
    Result[3] = c3*pi[ -9] + c2*pi[-5] + c1*pi[-1] + c0*pi[3];

    Normalize4(Result);
}

void GRANNY
SampleBSpline3x9(real32 const *tiAliased, real32 const *piAliased,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(3,9);

    real32 c3, c2, c1, c0;
    CubicCoefficients(ti[-3], ti[-2], ti[-1], ti[0], ti[1], ti[2],
                      t, &c3, &c2, &c1, &c0);

    Result[0] = c3*pi[-27] + c2*pi[-18] + c1*pi[-9] + c0*pi[0];
    Result[1] = c3*pi[-26] + c2*pi[-17] + c1*pi[-8] + c0*pi[1];
    Result[2] = c3*pi[-25] + c2*pi[-16] + c1*pi[-7] + c0*pi[2];
    Result[3] = c3*pi[-24] + c2*pi[-15] + c1*pi[-6] + c0*pi[3];
    Result[4] = c3*pi[-23] + c2*pi[-14] + c1*pi[-5] + c0*pi[4];
    Result[5] = c3*pi[-22] + c2*pi[-13] + c1*pi[-4] + c0*pi[5];
    Result[6] = c3*pi[-21] + c2*pi[-12] + c1*pi[-3] + c0*pi[6];
    Result[7] = c3*pi[-20] + c2*pi[-11] + c1*pi[-2] + c0*pi[7];
    Result[8] = c3*pi[-19] + c2*pi[-10] + c1*pi[-1] + c0*pi[8];
}

void GRANNY
SampleBSpline3xN(real32 const *tiAliased, real32 const *piAliased,
                 int32 Dimension, bool Normalize,
                 real32 t, real32 *ResultAliased)
{
    UNALIASED_SPLINE_ARGS(3,Dimension);

    real32 c3, c2, c1, c0;
    CubicCoefficients(ti[-3], ti[-2], ti[-1], ti[0], ti[1], ti[2],
                      t, &c3, &c2, &c1, &c0);

    {for(int32x d = 0; d < Dimension; ++d)
    {
        Result[d] = (c3 * pi[(Dimension * -3) + d] +
                     c2 * pi[(Dimension * -2) + d] +
                     c1 * pi[(Dimension * -1) + d] +
                     c0 * pi[(Dimension *  0) + d]);
    }}

    if (Normalize)
        NormalizeN(Result, Dimension);
}

typedef void bspline_evaluator(real32 const *tiAliased, real32 const *piAliased,
                               real32 t, real32 *ResultAliased);

typedef void bspline_general_evaluator(real32 const *tiAliased, real32 const *piAliased,
                                       int32 Dimension, bool Normalize,
                                       real32 t, real32 *ResultAliased);

static bspline_evaluator *Evaluators[4][10] =
{
    {0, SampleBSpline0x1, SampleBSpline0x2, SampleBSpline0x3, SampleBSpline0x4, 0, 0, 0, 0, SampleBSpline0x9},
    {0, SampleBSpline1x1, SampleBSpline1x2, SampleBSpline1x3, SampleBSpline1x4, 0, 0, 0, 0, SampleBSpline1x9},
    {0, SampleBSpline2x1, SampleBSpline2x2, SampleBSpline2x3, SampleBSpline2x4, 0, 0, 0, 0, SampleBSpline2x9},
    {0, SampleBSpline3x1, SampleBSpline3x2, SampleBSpline3x3, SampleBSpline3x4, 0, 0, 0, 0, SampleBSpline3x9},
};

static bspline_evaluator *NormalizedEvaluators[4][10] =
{
    {0, SampleBSpline0x1, SampleBSpline0x2, SampleBSpline0x3, SampleBSpline0x4,  0, 0, 0, 0, SampleBSpline0x9},
    {0, 0, 0, SampleBSpline1x3n, SampleBSpline1x4n, 0, 0, 0, 0, 0},
    {0, 0, 0, SampleBSpline2x3n, SampleBSpline2x4n, 0, 0, 0, 0, 0},
    {0, 0, 0, SampleBSpline3x3n, SampleBSpline3x4n, 0, 0, 0, 0, 0},
};

static bspline_general_evaluator *GeneralEvaluators[4] =
{
    SampleBSpline0xN, SampleBSpline1xN, SampleBSpline2xN, SampleBSpline3xN
};


void GRANNY
SampleBSpline(int32x Degree, int32x Dimension,
              bool Normalize,
              real32 const *ti, real32 const *pi,
              real32 t, real32 *Result)
{
    CheckInt32Index(Degree, 4, return);

    bspline_evaluator* Evaluator = 0;
    if (Dimension < 10)
    {
        Evaluator = (Normalize ?
                     NormalizedEvaluators[Degree][Dimension] :
                     Evaluators[Degree][Dimension]);
    }

    if(Evaluator)
    {
        Evaluator(ti, pi, t, Result);
    }
    else
    {
        bspline_general_evaluator* General = GeneralEvaluators[Degree];
        CheckPointerNotNull(General, return);

        General(ti, pi, Dimension, Normalize, t, Result);
    }
}

void GRANNY
UncheckedSampleBSpline(int32x Degree, int32x Dimension,
                       real32 const *ti, real32 const *pi,
                       real32 t, real32 *Result)
{
    Evaluators[Degree][Dimension](ti, pi, t, Result);
}

void GRANNY
UncheckedSampleBSplineN(int32x Degree, int32x Dimension,
                        real32 const *ti, real32 const *pi,
                        real32 t, real32 *Result)
{
    NormalizedEvaluators[Degree][Dimension](ti, pi, t, Result);
}
