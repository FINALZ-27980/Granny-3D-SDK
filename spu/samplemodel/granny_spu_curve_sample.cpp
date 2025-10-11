// ========================================================================
// $File: //jeffr/granny_29/rt/cell/spu/granny_spu_curve_sample.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny_spu_curve_sample.h"

#include "granny_spu_sample_model_animations.h"

#include "granny_bspline.h"
#include "granny_conversions.h"
#include "granny_cpp_settings.h"
// ***VERSION_CHECK***
#include "granny_find_knot.h"
#include "granny_limits.h"
#include "granny_math.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_quaternion_scaleoffset.h"
#include "granny_spu_curve.h"
#include "granny_track_sampler.h"

#include "granny_cpp_settings.h"

USING_GRANNY_NAMESPACE;

#if !PROCESSOR_CELL_SPU
#define COND_EXPECT(x, v) (x)
#else
#define COND_EXPECT(x, v) __builtin_expect((x), (v))
#endif

static void EnsureQuaternionContinuitySPU(int32x QuaternionCount, vec_float4* Quaternions)
{
    vec_float4 LastQuat = { 0, 0, 0, 0 };
    while(QuaternionCount--)
    {
        if (_vmathVfDot4(LastQuat, Quaternions[0])[0] < 0.0f)
            Quaternions[0] = -Quaternions[0];

        LastQuat = Quaternions[0];
        ++Quaternions;
    }
}

static int32x FindReal32Knot(real32 const* NOALIAS Knots, int32x KnotCount, real32 t)
{
    for (int i = 0; i < KnotCount; i++)
    {
        if (COND_EXPECT(Knots[i] > t, false))
        {
            return i;
        }
    }

    return KnotCount - 1;
}

static int32x FindQuantizedKnot8(uint8 const* NOALIAS Knots, int32x KnotCount,
                                 real32 t, real32 OneOverKnotScale)
{
    const int32x MaxVal = UInt8Maximum;

    real32 ScaledT = t * OneOverKnotScale;
    Assert ( ScaledT >= 0.0f );
    Assert ( ScaledT < 1.0f + (real32)MaxVal );
    int32x QuantisedT = FloorReal32ToInt32( ScaledT );
    Assert ( QuantisedT >= 0 );
    Assert ( QuantisedT <= MaxVal );

    for (int i = 0; i < KnotCount; i++)
    {
        if (COND_EXPECT(Knots[i] > QuantisedT, false))
        {
            return i;
        }
    }

    return KnotCount - 1;
}

static int32x
FindQuantizedKnot16(uint16 const* NOALIAS Knots, int32x KnotCount,
                    real32 t, real32 OneOverKnotScale)
{
    const int32x MaxVal = UInt16Maximum;

    real32 ScaledT = t * OneOverKnotScale;
    Assert ( ScaledT >= 0.0f );
    Assert ( ScaledT < 1.0f + (real32)MaxVal );
    int32x QuantisedT = FloorReal32ToInt32( ScaledT );
    Assert ( QuantisedT >= 0 );
    Assert ( QuantisedT <= MaxVal );

    for (int i = 0; i < KnotCount; i++)
    {
        if (COND_EXPECT(Knots[i] > QuantisedT, false))
        {
            return i;
        }
    }

    return KnotCount - 1;
}


static void
HandleReplication(spu_replication_type ReplicationType,
                  real32 const* NOALIAS CurveResult, int32x CurveDimension,
                  real32* NOALIAS Result, int32x ResultDimension)
{
    switch (ReplicationType)
    {
        case SPUReplicationRaw:
        {
            Assert(CurveDimension == ResultDimension);
            Copy32(ResultDimension, CurveResult, Result);
        } break;

        case SPUReplicationNormOri:
        {
            Assert(CurveDimension == 4);
            Assert(ResultDimension == 4);

            Copy32(4, CurveResult, Result);
            Normalize4(Result);
        } break;

        case SPUReplicationDiagonalSS:
        {
            Assert(ResultDimension == 9);
            Assert(CurveDimension == 1 || CurveDimension == 3);

            real32 Values[3];
            if (CurveDimension == 1)
            {
                Values[0] = CurveResult[0];
                Values[1] = CurveResult[0];
                Values[2] = CurveResult[0];
            }
            else
            {
                Assert(CurveDimension == 3);
                Values[0] = CurveResult[0];
                Values[1] = CurveResult[1];
                Values[2] = CurveResult[2];
            }

            Result[0] = Values[0]; Result[1] = 0.0f;      Result[2] = 0.0f;
            Result[3] = 0.0f;      Result[4] = Values[1]; Result[5] = 0.0f;
            Result[6] = 0.0f;      Result[7] = 0.0f;      Result[8] = Values[2];
        } break;

        default: InvalidCodePath("nyi");
            break;
    }
}

static void
HandleScaling(spu_replication_type ReplicationType,
              real32 const* NOALIAS ScaleOffsets,
              real32 const* NOALIAS Control,
              int32x ControlDimension,
              real32* NOALIAS ResultControl,
              int32x ResultDimension)
{
    // This gets just a little tricky because the number of scale/offset pairs are
    // dependent on the curve dimension in certain cases.  ScaleOffets is a flat array of
    // [scale][offset][scale][offset]...

    switch (ReplicationType)
    {
        case SPUReplicationRaw:
        {
            // If the dimension is 1, and the replication is Raw, then we scale each
            // channel individually.
            if (ControlDimension == 1)
            {
                {for(int32x Dim = 0; Dim < ResultDimension; ++Dim)
                {
                    ResultControl[Dim] = (Control[0] * ScaleOffsets[Dim*2 + 0] + ScaleOffsets[Dim*2 + 1]);
                }}
            }
            else
            {
                Assert(ControlDimension == ResultDimension);
                {for(int32x Dim = 0; Dim < ResultDimension; ++Dim)
                {
                    ResultControl[Dim] = (Control[Dim] * ScaleOffsets[Dim*2 + 0] + ScaleOffsets[Dim*2 + 1]);
                }}
            }
        } break;

        case SPUReplicationNormOri:
        {
            Assert(ControlDimension == 4 && ResultDimension == 4);
            {for(int32x Dim = 0; Dim < ResultDimension; ++Dim)
            {
                ResultControl[Dim] = (Control[Dim] * ScaleOffsets[Dim*2 + 0] + ScaleOffsets[Dim*2 + 1]);
            }}
        } break;

        case SPUReplicationDiagonalSS:
        {
            Assert(ResultDimension == 9);
            Assert(ControlDimension == 1 || ControlDimension == 3);

            real32 Values[3];
            if (ControlDimension == 1)
            {
                Values[0] =
                    Values[1] =
                    Values[2] = (Control[0] * ScaleOffsets[0*2 + 0]) + ScaleOffsets[0*2 + 1];
            }
            else
            {
                Assert(ControlDimension == 3);
                Values[0] = (Control[0] * ScaleOffsets[0*2 + 0]) + ScaleOffsets[0*2 + 1];
                Values[1] = (Control[1] * ScaleOffsets[1*2 + 0]) + ScaleOffsets[1*2 + 1];
                Values[2] = (Control[2] * ScaleOffsets[2*2 + 0]) + ScaleOffsets[2*2 + 1];
            }

            ResultControl[0] = Values[0]; ResultControl[1] = 0.0f;      ResultControl[2] = 0.0f;
            ResultControl[3] = 0.0f;      ResultControl[4] = Values[1]; ResultControl[5] = 0.0f;
            ResultControl[6] = 0.0f;      ResultControl[7] = 0.0f;      ResultControl[8] = Values[2];
        } break;

        default: InvalidCodePath("nyi");
            break;
    }
}

// Elements are numbered 0123
inline vec_float4
SelectElements(vec_float4 vec0,
               vec_float4 vec1,
               int Elem0,
               int Elem1,
               int Elem2,
               int Elem3)
{
    vec_uchar16 Sel = { Elem0*4 + 0, Elem0*4 + 1, Elem0*4 + 2, Elem0*4 + 3,
                        Elem1*4 + 0, Elem1*4 + 1, Elem1*4 + 2, Elem1*4 + 3,
                        Elem2*4 + 0, Elem2*4 + 1, Elem2*4 + 2, Elem2*4 + 3,
                        Elem3*4 + 0, Elem3*4 + 1, Elem3*4 + 2, Elem3*4 + 3 };

    return spu_shuffle(vec0, vec1, Sel);
}

inline vec_float4
VectorDivide(vec_float4 Numerator, vec_float4 Den)
{
    vector float One    = spu_splats(1.0f);

    vector float est = spu_re(Den);
    est = (One - est * Den) * est + est;
    est = (One - est * Den) * est + est;

    return est * Numerator;
}


inline vec_float4
SPUQuadraticCoefficients(vec_float4 t_vec, real32 const t)
{
    // Not much explanation here.  This is a straightforward translation of the algo in
    // granny_bspline_inlines.h
    // t_vec[0] = ti_2
    // t_vec[1] = ti_1
    // t_vec[2] = ti
    // t_vec[3] = ti1

    // Num = [ t - ti_1, t - ti_2, t - ti_1, DontCare]
    vec_float4 Num = spu_splats(t) - SelectElements(t_vec, t_vec, 1, 0, 1, 1);

    // Den = [ ti - ti_1, ti - ti_2, ti1 - ti_1, DontCare ]
    vec_float4 DenP1 = SelectElements(t_vec, t_vec, 2, 2, 3, 2);
    vec_float4 DenP2 = SelectElements(t_vec, t_vec, 1, 0, 1, 1);
    vec_float4 Den = (DenP1 - DenP2);

    vec_float4 Fac     = VectorDivide(Num, Den);
    vec_float4 OneMFac = spu_splats(1.0f) - Fac;

    // These are the final coeff equations.  Compute the second bit of the C1 equation in
    // the 3rd component of the result vector, and then add it back in.
    // C0 = OneMFac[0] * OneMFac[1];
    // C1 = (OneMFac[0] * Fac[1]) + (Fac[0] + OneMFac[2]);
    // C2 = Fac[0] * Fac[2];

    vec_float4 Coeffs = (SelectElements(Fac, OneMFac, 4, 4, 0, 0) *
                         SelectElements(Fac, OneMFac, 5, 1, 2, 6));
    Coeffs[1] += Coeffs[3];

    return Coeffs;
}


static vec_float4
GetCoefficients(vec_float4 tiBuffer, real32 t, int32x Degree)
{
    Assert(Degree >= 0 && Degree <= 2);

    switch (Degree)
    {
        default: InvalidCodePath("nyi");
        case 0:
        {
            vec_float4 Unity = { 1, 0, 0, 0 };
            return Unity;
        } break;

        case 1:
        {
            real32 Coeff0, Coeff1;
            LinearCoefficients(tiBuffer[0], tiBuffer[1],
                               t,
                               &Coeff0, &Coeff1);

            vec_float4 Result = { Coeff0, Coeff1 };
            return Result;
        } break;

        case 2:
        {
            return SPUQuadraticCoefficients(tiBuffer, t);
        } break;
    }
}

static vec_float4
CoeffEval16(int32x Degree,
            uint16 const* NOALIAS Knots,
            int32x KnotCount, real32 OneOverKnotScale,
            int32x Knot, real32 t,
            bool Underflow, bool Overflow)
{
    Assert(Degree >= 0 && Degree <= 2);

    // Always step past on the knots, it will work out ok
    Knots += (Knot - Degree);
    vec_float4 tiBuffer = { Knots[0], Knots[1], Knots[2], Knots[3] };

    if (Knot < Degree)
    {
        int32x ZeroPoint = Degree - Knot;
        vec_uint4  Mask  = spu_maskw(0xF & ~((1 << (3 - (ZeroPoint - 1))) - 1));

        // Wrap properly.  We've arranged that the wrapping underflow values are
        // 65535 off their actual value
        if (Underflow)
        {
            vec_float4 Offset = spu_splats((real32)UInt16Maximum);
            tiBuffer -= spu_sel(spu_splats(0.0f), Offset, Mask);
        }
        else
        {
            // Clamp
            Assert(tiBuffer[ZeroPoint] == 0.0f);
            tiBuffer = spu_sel(spu_splats(0.0f), tiBuffer, ~Mask);
        }
    }

    if (Knot + (Degree-1) >= KnotCount)
    {
        int32x EndPoint       = KnotCount - 1 - Knot + Degree;

        vec_uint4 Mask        = spu_maskw((1 << (3 - EndPoint)) - 1);
        vec_float4 EndKnotVec = spu_splats(tiBuffer[EndPoint]);

        if (Overflow)
            tiBuffer += spu_sel(spu_splats(0.0f), EndKnotVec, Mask);
        else
            tiBuffer = spu_sel(tiBuffer, EndKnotVec, Mask);
    }

    return GetCoefficients(tiBuffer, t * OneOverKnotScale, Degree);
}

static vec_float4
CoeffEval8(int32x Degree,
           uint8 const* NOALIAS Knots,
           int32x KnotCount, real32 OneOverKnotScale,
           int32x Knot, real32 t,
           bool Underflow, bool Overflow)
{
    Assert(Degree >= 0 && Degree <= 2);

    // Always step past on the knots, it will work out ok
    Knots += (Knot - Degree);
    vec_float4 tiBuffer = { Knots[0], Knots[1], Knots[2], Knots[3] };

    if (Knot < Degree)
    {
        int32x ZeroPoint = Degree - Knot;
        vec_uint4  Mask  = spu_maskw(0xF & ~((1 << (3 - (ZeroPoint - 1))) - 1));

        // Wrap properly.  We've arranged that the wrapping underflow values are
        // 65535 off their actual value
        if (Underflow)
        {
            vec_float4 Offset = spu_splats((real32)UInt16Maximum);
            tiBuffer -= spu_sel(spu_splats(0.0f), Offset, Mask);
        }
        else
        {
            // Clamp
            Assert(tiBuffer[ZeroPoint] == 0.0f);
            tiBuffer = spu_sel(spu_splats(0.0f), tiBuffer, ~Mask);
        }
    }

    if (Knot + (Degree-1) >= KnotCount)
    {
        int32x EndPoint       = KnotCount - 1 - Knot + Degree;

        vec_uint4 Mask        = spu_maskw((1 << (3 - EndPoint)) - 1);
        vec_float4 EndKnotVec = spu_splats(tiBuffer[EndPoint]);

        if (Overflow)
            tiBuffer += spu_sel(spu_splats(0.0f), EndKnotVec, Mask);
        else
            tiBuffer = spu_sel(tiBuffer, EndKnotVec, Mask);
    }

    return GetCoefficients(tiBuffer, t * OneOverKnotScale, Degree);
}

// Common computation for handling wrapping controls
#define COMPUTE_REALIDX                             \
    int32x RealIdx = Idx + Knot - Header->Degree;   \
    do {                                            \
        if (RealIdx < 0)                            \
        {                                           \
            if (Context.UnderflowLoop)              \
                RealIdx += Header->KnotCount-1;     \
            else                                    \
                RealIdx = 0;                        \
        }                                           \
        else if (RealIdx >= Header->KnotCount-1)    \
        {                                           \
            if (Context.OverflowLoop)               \
                RealIdx -= Header->KnotCount-1;     \
            else if (RealIdx >= Header->KnotCount)  \
                RealIdx = Header->KnotCount-1;      \
        }                                           \
    } while (false)


static void
EvaluateSPUReal32Curve(sample_context const& NOALIAS Context,
                       spu_curve_header_basic const* NOALIAS Header,
                       int32x ResultDimension,
                       real32* NOALIAS Result)
{
    real32 const* NOALIAS Knots    = (real32 const*)(((uint8 const*)Header) + Header->KnotStart);
    real32 const* NOALIAS Controls = (real32 const*)(((uint8 const*)Header) + Header->ControlStart);

    //int32x Knot = FindKnot(Header->KnotCount, Knots, Context.LocalClock);
    int32x Knot = FindReal32Knot(Knots, Header->KnotCount, Context.LocalClock);
    Assert(Knot >= 0 && Knot < Header->KnotCount);

    vec_float4 Coefficients;
    {
        real32 tiBuffer[8];
        Copy32(2 * Header->Degree, Knots + Knot - Header->Degree, tiBuffer);

        // It's not necessary to handle the wrapping case for real32, since they're
        // already setup correctly.  It /is/ necessary to handle clamping
        if (!Context.UnderflowLoop && Knot < Header->Degree)
        {
            // Back clamp
            int32x ZeroPoint = Header->Degree - Knot;
            {for(int32x Idx = ZeroPoint - 1; Idx >= 0; --Idx)
            {
                tiBuffer[Idx] = 0.0f;
            }}
        }

        if (!Context.OverflowLoop && Knot + (Header->Degree-1) >= Header->KnotCount)
        {
            // Forward clamp
            int32x EndPoint = Header->KnotCount - 1 - Knot + Header->Degree;
            real32 EndKnot  = tiBuffer[EndPoint];
            {for(int32x Idx = EndPoint + 1; Idx < 2*Header->Degree; ++Idx)
            {
                tiBuffer[Idx] = EndKnot;
            }}
        }

        vec_float4 tiVec = { tiBuffer[0], tiBuffer[1], tiBuffer[2], tiBuffer[3] };
        Coefficients = GetCoefficients(tiVec, Context.LocalClock, Header->Degree);
    }

    // This is a little lame, but handle 3 cases here.
    // 1. 4 dim, normalized quat curve.  These need to be neighborhooded.
    // 2. N-dim, general curve.  Go through the replication phase
    // 3. N-dim, dimension reduced.

    real32 ResultBuffer[MaximumBSplineDimension] = { 0 };
    if (ResultDimension == Header->Dimension)
    {
        if ((spu_replication_type)Header->ReplicationType == SPUReplicationNormOri)
        {
            Assert(Header->Dimension == 4);
            Assert(Header->Degree+1 <= 9);

            vec_float4 NormQuats[9];
            {for(int32x Idx = 0; Idx < Header->Degree+1; ++Idx)
            {
                COMPUTE_REALIDX;
                Copy32(4, &Controls[Header->Dimension * RealIdx], &NormQuats[Idx]);
            }}

            EnsureQuaternionContinuitySPU(Header->Degree+1, NormQuats);

            vec_float4 ResultQuat = { 0, 0, 0, 0 };
            {for(int32x Idx = 0; Idx < Header->Degree+1; ++Idx)
            {
                ResultQuat += spu_splats(Coefficients[Idx]) * NormQuats[Idx];
            }}

            Assert(IS_ALIGNED_16(Result));

            // Replicate to the dest
            {
                vec_float4 NormScale   = VecInvSquareRootFast(_vmathVfDot4(ResultQuat, ResultQuat));
                *((vec_float4*)Result) = ResultQuat * NormScale;
            }
        }
        else
        {
            {for(int32x Idx = 0; Idx < Header->Degree+1; ++Idx)
            {
                COMPUTE_REALIDX;
                {for(int32x Dim = 0; Dim < Header->Dimension; ++Dim)
                {
                    ResultBuffer[Dim] +=
                        Coefficients[Idx] * Controls[Header->Dimension * RealIdx + Dim];
                }}
            }}

            HandleReplication((spu_replication_type)Header->ReplicationType,
                              ResultBuffer, Header->Dimension,
                              Result, ResultDimension);
        }
    }
    else
    {
        // For float 32 curves, only d3i1 has scale/offset pairs.
        Assert(Header->Dimension == 1);
        Assert(Header->ReplicationType == SPUReplicationRaw);

        real32 const* NOALIAS ScaleOffsets = (real32 const*)(Header + 1);
        {for(int32x Idx = 0; Idx < Header->Degree+1; ++Idx)
        {
            COMPUTE_REALIDX;
            {for(int32x Dim = 0; Dim < ResultDimension; ++Dim)
            {
                real32 DimComponent = Controls[RealIdx] * ScaleOffsets[Dim*2 + 0] + ScaleOffsets[Dim*2 + 1];
                ResultBuffer[Dim] += Coefficients[Idx] * DimComponent;
            }}
        }}

        // No need to handle the replication, handled above by the scale/offset process.
        Copy32(ResultDimension, ResultBuffer, Result);
    }
}


static void
EvaluateSPUK16Curve(sample_context const& NOALIAS Context,
                    spu_curve_header_basic const* NOALIAS Header,
                    int32x ResultDimension,
                    real32* NOALIAS Result)
{
    spu_curve_header_quantized const* NOALIAS Quantized = (spu_curve_header_quantized const*)Header;

    // Scales and offsets directly follow the header
    real32 const* NOALIAS ScaleOffsets = (real32 const*)(Quantized + 1);

    // Knots and controls as before
    uint16 const* NOALIAS Knots    = (uint16 const*)(((uint8 const*)Header) + Header->KnotStart);
    uint16 const* NOALIAS Controls = (uint16 const*)(((uint8 const*)Header) + Header->ControlStart);

    int32x Knot = FindQuantizedKnot16(Knots,
                                      Header->KnotCount,
                                      Context.LocalClock,
                                      Quantized->OneOverKnotScale);
    Assert(Knot >= 0 && Knot < Header->KnotCount);

    vec_float4 Coefficients = CoeffEval16(Header->Degree,
                                          Knots, Header->KnotCount,
                                          Quantized->OneOverKnotScale,
                                          Knot, Context.LocalClock,
                                          Context.UnderflowLoop, Context.OverflowLoop);

    real32 ResultBuffer[MaximumBSplineDimension] = { 0 };
    {
        real32 ScaleBuffer[MaximumBSplineDimension];
        real32 ControlBuffer[MaximumBSplineDimension];
        {for(int32x Idx = 0; Idx < Header->Degree+1; ++Idx)
        {
            COMPUTE_REALIDX;
            {for(int32x Dim = 0; Dim < Header->Dimension; ++Dim)
            {
                ScaleBuffer[Dim] = Controls[RealIdx * Header->Dimension + Dim];
            }}

            HandleScaling((spu_replication_type)Header->ReplicationType, ScaleOffsets,
                          ScaleBuffer, Header->Dimension,
                          ControlBuffer, ResultDimension);

            {for(int32x Dim = 0; Dim < ResultDimension; ++Dim)
            {
                ResultBuffer[Dim] += Coefficients[Idx] * ControlBuffer[Dim];
            }}
        }}
    }

    switch (Header->ReplicationType)
    {
        case SPUReplicationNormOri:
        {
            Normalize4(ResultBuffer);
        } break;

        case SPUReplicationRaw:
        case SPUReplicationDiagonalSS:
            // HandleScaling blows out the result to the correct dimension
            break;

        default: InvalidCodePath("invalid replication type");
            break;
    }

    Copy32(ResultDimension, ResultBuffer, Result);
}


static void
EvaluateSPUK8Curve(sample_context const& NOALIAS Context,
                   spu_curve_header_basic const* NOALIAS Header,
                   int32x ResultDimension,
                   real32* NOALIAS Result)
{
    spu_curve_header_quantized const* NOALIAS Quantized = (spu_curve_header_quantized const*)Header;

    // Scales and offsets directly follow the header
    real32 const* NOALIAS ScaleOffsets = (real32 const*)(Quantized + 1);

    // Knots and controls as before
    uint8 const* NOALIAS Knots    = ((uint8 const*)Header) + Header->KnotStart;
    uint8 const* NOALIAS Controls = ((uint8 const*)Header) + Header->ControlStart;

    int32x Knot = FindQuantizedKnot8(Knots,
                                     Header->KnotCount,
                                     Context.LocalClock,
                                     Quantized->OneOverKnotScale);
    Assert(Knot >= 0 && Knot < Header->KnotCount);

    vec_float4 Coefficients = CoeffEval8(Header->Degree,
                                         Knots, Header->KnotCount,
                                         Quantized->OneOverKnotScale,
                                         Knot, Context.LocalClock,
                                         Context.UnderflowLoop, Context.OverflowLoop);

    real32 ResultBuffer[MaximumBSplineDimension] = { 0 };
    {
        real32 ScaleBuffer[MaximumBSplineDimension];
        real32 ControlBuffer[MaximumBSplineDimension];
        {for(int32x Idx = 0; Idx < Header->Degree+1; ++Idx)
        {
            COMPUTE_REALIDX;
            {for(int32x Dim = 0; Dim < Header->Dimension; ++Dim)
            {
                ScaleBuffer[Dim] = Controls[RealIdx * Header->Dimension + Dim];
            }}

            HandleScaling((spu_replication_type)Header->ReplicationType, ScaleOffsets,
                          ScaleBuffer, Header->Dimension,
                          ControlBuffer, ResultDimension);

            {for(int32x Dim = 0; Dim < ResultDimension; ++Dim)
            {
                ResultBuffer[Dim] += Coefficients[Idx] * ControlBuffer[Dim];
            }}
        }}
    }

    switch (Header->ReplicationType)
    {
        case SPUReplicationNormOri:
        {
            Normalize4(ResultBuffer);
        } break;

        case SPUReplicationRaw:
        case SPUReplicationDiagonalSS:
            // HandleScaling blows out the result to the correct dimension
            break;

        default: InvalidCodePath("invalid replication type");
            break;
    }

    Copy32(ResultDimension, ResultBuffer, Result);
}


vec_float4
RotateToMissing(vec_float4 v, int Missing)
{
    Missing = 3 - Missing;
    vec_uchar16 const Perm =
        { 0  + Missing * 4, 1  + Missing * 4, 2  + Missing * 4, 3  + Missing * 4,
          4  + Missing * 4, 5  + Missing * 4, 6  + Missing * 4, 7  + Missing * 4,
          8  + Missing * 4, 9  + Missing * 4, 10 + Missing * 4, 11 + Missing * 4,
          12 + Missing * 4, 13 + Missing * 4, 14 + Missing * 4, 15 + Missing * 4 };

    return spu_shuffle(v, v, Perm);
}

static void
EvaluateSPU4nK8Curve(sample_context const& Context,
                     spu_curve_header_basic const* Header,
                     int32x ResultDimension,
                     real32* Result)
{
    spu_curve_header_quantized4n const* NOALIAS Quantized = (spu_curve_header_quantized4n const*)Header;

    uint8 const* NOALIAS Knots    = ((uint8 const*)Header) + Header->KnotStart;
    uint8 const* NOALIAS Controls = ((uint8 const*)Header) + Header->ControlStart;

    int32x Knot = FindQuantizedKnot8(Knots,
                                     Header->KnotCount,
                                     Context.LocalClock,
                                     Quantized->OneOverKnotScale);
    Assert(Knot >= 0 && Knot < Header->KnotCount);

    vec_float4 Coefficients = CoeffEval8(Header->Degree,
                                         Knots, Header->KnotCount,
                                         Quantized->OneOverKnotScale,
                                         Knot, Context.LocalClock,
                                         Context.UnderflowLoop, Context.OverflowLoop);

    // Extract the scales and offsets.

    int32 ScaleOffsetTableEntry = Quantized->ScaleOffsetTableEntries;
    vec_float4 ControlScales;
    vec_float4 ControlOffsets;
    {for ( int32x Dim = 0; Dim < 4; Dim++ )
    {
        int32x TableIndex = ScaleOffsetTableEntry & 0xf;
        ScaleOffsetTableEntry >>= 4;
        ControlScales [Dim] = QUATERNION_SCALE_ENTRY(TableIndex, 0) * (1.0f / ((real32)((1<<7)-1)));
        ControlOffsets[Dim] = QUATERNION_SCALE_ENTRY(TableIndex, 1);
    }}

    vec_float4 ResultQuat = { 0.0f, 0.0f, 0.0f, 0.0f };
    {
        vec_float4 LastQuat = { 0.0f, 0.0f, 0.0f, 0.0f };

        {for(int32x Idx = 0; Idx < Header->Degree+1; ++Idx)
        {
            COMPUTE_REALIDX;
            vec_uint4 Data = {
                Controls[RealIdx * 3 + 0],
                Controls[RealIdx * 3 + 1],
                Controls[RealIdx * 3 + 2]
            };

            vec_uint4 AndMask = spu_splats((unsigned int)0x7f);
            vec_float4 SourceControl = spu_convtf(spu_and(Data, AndMask), 0);

            // The upper bits are the special ones.
            // The first is the sign of the missing component.
            // The others are the index of the missing component.
            vec_uint4 NegMask = spu_cmpgt(spu_splats(Data[0]), 0x80);
            int32x MissingComponentIndex      = ( ( Data[1] >> 6 ) & 0x2 ) | ( Data[2] >> 7 );

            // Swizzle, scale and offset
            vec_float4 RotatedSource = RotateToMissing(SourceControl, MissingComponentIndex);

            // We only need to select off the offsets, since the source control contains a
            // 0.0 in the missing positionm, which multiplies out.
            vec_float4 ResultControl = ControlOffsets + ControlScales * RotatedSource;
            ResultControl[MissingComponentIndex] = 0.0f;

            vec_float4 MissingResult =
                VecFastSquareRootGreater0(spu_splats(1.0f) -
                                          _vmathVfDot4(ResultControl, ResultControl));
            MissingResult *= spu_sel(spu_splats(1.0f), spu_splats(-1.0f), NegMask);
            ResultControl[MissingComponentIndex] = MissingResult[MissingComponentIndex];

            if (_vmathVfDot4(LastQuat, ResultControl)[0] < 0.0f)
            {
                ResultControl = -ResultControl;
            }
            LastQuat = ResultControl;

            // Add the control into the ResultBuffer
            ResultQuat += spu_splats(Coefficients[Idx]) * ResultControl;
        }}
    }

    Assert(Header->ReplicationType == SPUReplicationNormOri);
    Assert(IS_ALIGNED_16(Result));

    // Replicate to the dest
    {
        vec_float4 NormScale   = VecInvSquareRootFast(_vmathVfDot4(ResultQuat, ResultQuat));
        *((vec_float4*)Result) = ResultQuat * NormScale;
    }
}


static void
EvaluateSPU4nK16Curve(sample_context const& Context,
                      spu_curve_header_basic const* Header,
                      int32x ResultDimension,
                      real32* Result)
{
    spu_curve_header_quantized4n const* NOALIAS Quantized = (spu_curve_header_quantized4n const*)Header;

    uint16 const* NOALIAS Knots    = (uint16 const*)(((uint8 const*)Header) + Header->KnotStart);
    uint16 const* NOALIAS Controls = (uint16 const*)(((uint8 const*)Header) + Header->ControlStart);

    int32x Knot = FindQuantizedKnot16(Knots,
                                      Header->KnotCount,
                                      Context.LocalClock,
                                      Quantized->OneOverKnotScale);
    Assert(Knot >= 0 && Knot < Header->KnotCount);

    vec_float4 Coefficients = CoeffEval16(Header->Degree,
                                          Knots, Header->KnotCount,
                                          Quantized->OneOverKnotScale,
                                          Knot, Context.LocalClock,
                                          Context.UnderflowLoop, Context.OverflowLoop);

    // Extract the scales and offsets.
    real32 ControlScales[4];
    real32 ControlOffsets[4];
    int32 ScaleOffsetTableEntry = Quantized->ScaleOffsetTableEntries;
    {for ( int32x Dim = 0; Dim < 4; Dim++ )
    {
        int32x TableIndex = ScaleOffsetTableEntry & 0xf;
        ScaleOffsetTableEntry >>= 4;
        ControlScales [Dim] = QUATERNION_SCALE_ENTRY(TableIndex, 0) * (1.0f / ((real32)((1<<15)-1)));
        ControlOffsets[Dim] = QUATERNION_SCALE_ENTRY(TableIndex, 1);
    }}

    vec_float4 ResultQuat = { 0.0f, 0.0f, 0.0f, 0.0f };
    {
        vec_float4 LastQuat = { 0.0f, 0.0f, 0.0f, 0.0f };

        {for(int32x Idx = 0; Idx < Header->Degree+1; ++Idx)
        {
            COMPUTE_REALIDX;
            uint16 Data[3] = {
                Controls[RealIdx * 3 + 0],
                Controls[RealIdx * 3 + 1],
                Controls[RealIdx * 3 + 2]
            };

            vec_float4 ResultControl;

            // The upper bits are the special ones.
            // The first is the sign of the missing component.
            // The others are the index of the missing component.
            bool   MissingComponentIsNegative = ( Data[0] & 0x8000 ) != 0;
            int32x MissingComponentIndex      = ( ( Data[1] >> 14 ) & 0x2 ) | ( Data[2] >> 15 );

            // Swizzle, scale and offset the others in (remembering to mask off the top bits).
            int32x DstComp = MissingComponentIndex;
            real32 SummedSq = 0.0f;
            {for ( int32x SrcComp = 0; SrcComp < 3; SrcComp++ )
            {
                DstComp++;
                DstComp &= 0x3;
                real32 Result = ControlOffsets[DstComp] + ControlScales[DstComp] * (real32)( Data[SrcComp] & 0x7fff );
                Assert ( AbsoluteValue ( Result ) < 0.71f );
                SummedSq += Result * Result;
                ResultControl[DstComp] = Result;
            }}

            // Now compute the missing member.
            //real32 MissingResult = SquareRoot ( 1.0f - SummedSq );
            real32 MissingResult = FastSquareRootGreater0(1.0f - SummedSq);
            Assert ( MissingResult > 0.4999f );
            if ( MissingComponentIsNegative )
            {
                MissingResult = -MissingResult;
            }
            ResultControl[MissingComponentIndex] = MissingResult;

            if (_vmathVfDot4(LastQuat, ResultControl)[0] < 0.0f)
            {
                ResultControl = -ResultControl;
            }
            LastQuat = ResultControl;

            // Add the control into the ResultBuffer
            ResultQuat += spu_splats(Coefficients[Idx]) * ResultControl;
        }}
    }

    Assert(Header->ReplicationType == SPUReplicationNormOri);
    Assert(IS_ALIGNED_16(Result));

    // Replicate to the dest
    {
        vec_float4 NormScale   = VecInvSquareRootFast(_vmathVfDot4(ResultQuat, ResultQuat));
        *((vec_float4*)Result) = ResultQuat * NormScale;
    }
}


void GRANNY
EvaluateSPUCurve(sample_context const& NOALIAS Context,
                 uint8 const* NOALIAS          CurveBytes,
                 int32x                        ResultDimension,
                 real32* NOALIAS               Result)
{
    Assert(CurveBytes);
    Assert(Result);

    spu_curve_header_basic const* NOALIAS Header = (spu_curve_header_basic const*)CurveBytes;
    switch (Header->CurveType)
    {
        case SPUCurveTypeReal32:
        {
            EvaluateSPUReal32Curve(Context, Header, ResultDimension, Result);
        } break;

        case SPUCurveTypeK16:
        {
            EvaluateSPUK16Curve(Context, Header, ResultDimension, Result);
        } break;

        case SPUCurveTypeK8:
        {
            EvaluateSPUK8Curve(Context, Header, ResultDimension, Result);
        } break;

        case SPUCurveType4nK16:
        {
            EvaluateSPU4nK16Curve(Context, Header, ResultDimension, Result);
        } break;

        case SPUCurveType4nK8:
        {
            EvaluateSPU4nK8Curve(Context, Header, ResultDimension, Result);
        } break;

        default:
            // Invalid curve type
            {for (int32x Idx = 0; Idx < ResultDimension; ++Idx)
            {
                Result[Idx] = 0;
            }}
            break;
    }
}
