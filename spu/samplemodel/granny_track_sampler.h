#if !defined(GRANNY_TRACK_SAMPLER_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_track_sampler.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(TrackSamplerGroup);

struct transform_track;
struct bound_transform_track;
struct transform;
struct curve2;

EXPTYPE_EPHEMERAL struct sample_context
{
    real32 LocalClock;
    real32 LocalDuration;
    bool32 UnderflowLoop;
    bool32 OverflowLoop;

    // This is for keyframe animations only
    int32x FrameIndex;
};

EXPAPI typedef CALLBACK_FN(void) track_sampler(sample_context const &Context,
                                               transform_track const *SourceTrack,
                                               bound_transform_track *Track,
                                               transform const &RestTransform,
                                               real32 const *ParentMatrix,
                                               real32 *WorldResult);

#define TrackSampler(Name)                                          \
    static CALLBACK_FN(void) Name(sample_context const &Context,    \
                                  transform_track const *SourceTrack,   \
                                  bound_transform_track *Track,         \
                                  transform const &RestTransform,       \
                                  real32 const *ParentMatrixAliased,    \
                                  real32 *WorldResultAliased)

EXPAPI GS_READ void SampleTrackUUULocal(sample_context const&  Context,
                                        transform_track const* SourceTrack,
                                        bound_transform_track* Track,
                                        transform&             Result);

EXPAPI GS_READ void SampleTrackPOLocal(sample_context const&  Context,
                                       transform_track const* SourceTrack,
                                       bound_transform_track* Track,
                                       real32*                ResultPosition,
                                       real32*                ResultOrientation);

EXPAPI GS_READ void SampleTrackUUULocalAtTime0(transform_track const* SourceTrack,
                                               transform&             Result);

EXPAPI GS_READ track_sampler *GetTrackSamplerFor(transform_track const &Track);
EXPAPI GS_READ track_sampler *GetTrackSamplerUUU(void);
EXPAPI GS_READ track_sampler *GetTrackSamplerSSS(void);
EXPAPI GS_READ track_sampler *GetTrackSamplerIII(void);
EXPAPI GS_READ track_sampler *GetTrackSamplerIIU(void);


// handy utility
EXPAPI void SetContextFrameIndex(sample_context& Context,
                                 real32 Clock,
                                 real32 Duration,
                                 real32 TimeStep);


void SampleTrackUUUAtTime0(sample_context const &Context,
                           transform_track const *SourceTrack,
                           bound_transform_track *Track,
                           transform const &RestTransform,
                           real32 const *ParentMatrix,
                           real32 *WorldResult);

void SampleTrackUUUBlendWithTime0(sample_context const &Context,
                                  transform_track const *SourceTrack,
                                  bound_transform_track *Track,
                                  transform const &RestTransform,
                                  real32 const *ParentMatrix,
                                  real32 *WorldResult,
                                  real32 BlendAmount);



int32x CurveKnotCharacter(curve2 const &Curve);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_TRACK_SAMPLER_H
#endif
