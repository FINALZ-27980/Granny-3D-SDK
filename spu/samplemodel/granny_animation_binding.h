#if !defined(GRANNY_ANIMATION_BINDING_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_animation_binding.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GRANNY_TRANSFORM_H)
#include "granny_transform.h"
#endif

#if !defined(GRANNY_TRACK_SAMPLER_H)
#include "granny_track_sampler.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(AnimationBindingGroup);

struct animation;
struct model;
struct retargeter;
struct track_group;
struct transform_track;
struct mesh_binding;

// Some people don't use LOD, and are severely memory-constrained.
// (e.g. mobile phones). So they get a special build that shrinks
// the size of the bound_transform_track quite a bit by
// removing the "LODTransform" item.
// If you are finding this to be a problem for you, email us and
// let us know.
// But even if you don't explicitly use LOD, this is still a
// decent speed improvement for tracks that are constant (which
// is a surprisingly large number).
#define BOUND_TRANSFORM_TRACK_HAS_LOD 1

EXPTYPE_EPHEMERAL enum bound_transform_track_flags
{
    // Flags to indicate the curve type associated with the various
    // channels of the track_group.  Using these instead of the more
    // general CurveIs* class of functions prevents a large number of
    // cache misses when the curve is constant (the common case).

    // Position curve
    BoundPositionCurveIsIdentity    = (0x0 << 0),
    BoundPositionCurveIsConstant    = (0x1 << 0),
    BoundPositionCurveIsKeyframed   = (0x2 << 0),
    BoundPositionCurveIsGeneral     = (0x3 << 0),
    BoundPositionCurveFlagMask      = (0x3 << 0),

    // Orientation curve
    BoundOrientationCurveIsIdentity    = (0x0 << 2),
    BoundOrientationCurveIsConstant    = (0x1 << 2),
    BoundOrientationCurveIsKeyframed   = (0x2 << 2),
    BoundOrientationCurveIsGeneral     = (0x3 << 2),
    BoundOrientationCurveFlagMask      = (0x3 << 2),

    // Orientation curve
    BoundScaleShearCurveIsIdentity    = (0x0 << 4),
    BoundScaleShearCurveIsConstant    = (0x1 << 4),
    BoundScaleShearCurveIsKeyframed   = (0x2 << 4),
    BoundScaleShearCurveIsGeneral     = (0x3 << 4),
    BoundScaleShearCurveFlagMask      = (0x3 << 4),

    bound_transform_track_flags_forceint = 0x7fffffff
};

EXPTYPE_EPHEMERAL struct bound_transform_track
{
    int16 SourceTrackIndex;
    int8  QuaternionMode;
    int8  Flags;

    transform_track const *SourceTrack;
    track_sampler *Sampler;
    real32 LODError;

    // This can be removed if BOUND_TRANSFORM_TRACK_HAS_LOD is 0.  It's not #if'd here
    // because of the way Granny extracts structures for inclusion in granny.h
    transform LODTransform;
};

// Handy macro for checking the flags field of the bound_transform_track.  Use as:
//  BOUND_CURVE_CHECK(BoundTrack, Position, IsKeyframed)
//  BOUND_CURVE_CHECK(BoundTrack, Orientation, IsConstant)
//  BOUND_CURVE_CHECK(BoundTrack, Orientation, IsGeneral)
// etc.
#define BOUND_CURVE_CHECK(BoundTrack, CurveID, Flag) (((BoundTrack).Flags & Bound ## CurveID ## CurveFlagMask) == Bound ## CurveID ## Curve ## Flag)


EXPTYPE_EPHEMERAL struct animation_binding_identifier
{
    animation const *Animation;
    int32x SourceTrackGroupIndex;

    char const*  TrackPattern;
    char const*  BonePattern;
    int32 const* TrackMapping;

    model const *OnModel;

    model* FromBasis;
    model* ToBasis;
};


EXPTYPE_EPHEMERAL struct animation_binding
{
    animation_binding_identifier ID;

    retargeter* Retargeter;

    int32x TrackBindingCount;
    bound_transform_track *TrackBindings;

    animation_binding *Left;
    animation_binding *Right;

    int32x UsedBy;
    animation_binding *PreviousUnused;
    animation_binding *NextUnused;
};

EXPAPI GS_READ void MakeDefaultAnimationBindingID(
    animation_binding_identifier &ID,
    animation const *Animation,
    int32x TrackGroupIndex);

EXPAPI GS_MODIFY animation_binding *AcquireAnimationBindingFromID(
    animation_binding_identifier &ID);

EXPAPI GS_MODIFY animation_binding *AcquireAnimationBinding(animation_binding *Binding);

EXPAPI GS_MODIFY void ReleaseAnimationBinding(animation_binding *Binding);

EXPAPI GS_READ   int32x GetMaximumAnimationBindingCount(void);
EXPAPI GS_MODIFY void SetMaximumAnimationBindingCount(int32x BindingCountMax);

EXPAPI GS_MODIFY void FlushAllUnusedAnimationBindings(void);
EXPAPI GS_MODIFY void FlushAllBindingsForAnimation(animation const *Animation);
EXPAPI GS_MODIFY void FlushAnimationBinding(animation_binding *Binding);

EXPAPI GS_READ animation_binding *GetFirstBindingForAnimation(animation const *Animation);
EXPAPI GS_READ animation_binding *GetNextBindingForAnimation(animation const *Animation,
                                                             animation_binding *Binding);

EXPAPI GS_READ animation_binding *GetFirstUnusedAnimationBinding(void);
EXPAPI GS_READ animation_binding *GetNextUnusedAnimationBinding(animation_binding *Binding);

EXPAPI GS_READ bool IsAnimationUsed(animation const *Animation);

EXPAPI GS_MODIFY void RemapAnimationBindingPointers(animation_binding *Binding,
                                                    animation const *NewAnimationPointer);
EXPAPI GS_MODIFY void RemapAllAnimationBindingPointers(animation const *OldAnimationPointer,
                                                       animation const *NewAnimationPointer);

animation_binding *GetFirstAnimationBinding(void);
animation_binding *GetNextAnimationBinding(animation_binding *Binding);

void ComputeTotalDeltasFromBinding(animation_binding* Binding,
                                   triple*            TotalPositionDelta,
                                   triple*            TotalOrientationDelta);


track_group* GetTrackGroup(animation_binding_identifier &ID);
track_group* GetTrackGroup(animation_binding *Binding);
bool AreTrackFlagsConsistent(bound_transform_track const &BoundTrack);

EXPTYPE struct animation_lod_builder;
EXPAPI GS_MODIFY animation_lod_builder *CalculateAnimationLODBegin(animation const &Animation,
                                                                   real32 ManualScaler);

EXPAPI GS_MODIFY void CalculateAnimationLODAddMeshBinding(animation_lod_builder *Builder,
                                                          model const &Model,
                                                          mesh_binding const &MeshBinding,
                                                          real32 ManualScaler);

EXPAPI GS_MODIFY void CalculateAnimationLODEnd(animation_lod_builder *Builder);
EXPAPI GS_MODIFY void CalculateAnimationLODCleanup(animation_lod_builder *Builder);
void CopyLODErrorValuesFromAnimation(animation_binding &Binding, real32 ManualScaler);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_ANIMATION_BINDING_H
#endif
