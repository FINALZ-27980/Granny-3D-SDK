#if !defined(GRANNY_CONTROLLED_ANIMATION_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_controlled_animation.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(ControlledAnimationGroup);

struct animation;
struct animation_binding;
struct control;
struct local_pose;
struct mirror_specification;
struct model;
struct model_control_binding;
struct model_instance;
struct sample_context;
struct skeleton;
struct spu_animation_binding;
struct track_group;
struct track_mask;

EXPTYPE enum accumulation_mode
{
    NoAccumulation,
    ConstantExtractionAccumulation,
    VariableDeltaAccumulation,

    accumulation_mode_forceint = 0x7fffffff
};

EXPTYPE struct controlled_animation
{
    // These data structures all have to fit inside model_control_binding::Derived[]
    animation_binding *Binding;
    accumulation_mode AccumulationMode;
    track_mask const *TrackMask;
    track_mask const *ModelMask;
    mirror_specification* MirrorSpec;
};

EXPTYPE struct controlled_animation_builder;

EXPAPI GS_MODIFY control *PlayControlledAnimation(real32 StartTime,
                                                  animation const &Animation,
                                                  model_instance &Model);
EXPAPI GS_MODIFY control *PlayControlledAnimationBinding(real32 StartTime,
                                                         animation const &Animation,
                                                         animation_binding &Binding,
                                                         model_instance &Model);

EXPAPI GS_MODIFY controlled_animation_builder *BeginControlledAnimation(
    real32 StartTime, animation const &Animation);
EXPAPI GS_MODIFY control *EndControlledAnimation(controlled_animation_builder *Builder);

EXPAPI GS_MODIFY void UseExistingControlForAnimation(controlled_animation_builder *Builder,
                                                     control *Control);

EXPAPI GS_MODIFY void SetTrackMatchRule(controlled_animation_builder &Builder,
                                        int32x TrackGroupIndex,
                                        char const *TrackPattern,
                                        char const *BonePattern);

EXPAPI GS_MODIFY void SetTrackMatchMapping(controlled_animation_builder &Builder,
                                           int32x TrackGroupIndex,
                                           int32* MappingFromTrackToBone,
                                           int32x MapCount);

EXPAPI GS_MODIFY void SetTrackGroupTarget(controlled_animation_builder &Builder,
                                          int32x TrackGroupIndex,
                                          model_instance &Model);
EXPAPI GS_MODIFY void SetTrackGroupBinding(controlled_animation_builder &Builder,
                                           int32x TrackGroupIndex,
                                           animation_binding &Binding);
EXPAPI GS_MODIFY void SetTrackGroupBasisTransform(controlled_animation_builder &Builder,
                                                  int32x TrackGroupIndex,
                                                  model &FromModel, model &ToModel);
EXPAPI GS_MODIFY void SetTrackGroupTrackMask(controlled_animation_builder &Builder,
                                             int32x TrackGroupIndex,
                                             track_mask &TrackMask);
EXPAPI GS_MODIFY void SetTrackGroupModelMask(controlled_animation_builder &Builder,
                                             int32x TrackGroupIndex,
                                             track_mask &ModelMask);
EXPAPI GS_MODIFY void SetTrackGroupAccumulation(controlled_animation_builder &Builder,
                                                int32x TrackGroupIndex, accumulation_mode Mode);

EXPAPI GS_MODIFY void SetTrackGroupLOD(controlled_animation_builder &Builder,
                                       int32x TrackGroupIndex,
                                       bool CopyValuesFromAnimation,
                                       real32 ManualScaler);

EXPAPI GS_MODIFY void SetTrackGroupMirrorSpecification(controlled_animation_builder& Builder,
                                                       int32x TrackGroupIndex,
                                                       mirror_specification& Specification);

EXPAPI GS_READ animation_binding *GetAnimationBindingFromControlBinding(model_control_binding &Binding);
EXPAPI GS_READ spu_animation_binding *GetSPUAnimationBindingFromControlBinding(model_control_binding &Binding);


EXPAPI GS_READ real32 GetGlobalLODFadingFactor(void);
EXPAPI GS_MODIFY void SetGlobalLODFadingFactor(real32 NewValue);

void FindAllowedErrorNumbers ( real32 AllowedError, real32 *AllowedErrorEnd, real32 *AllowedErrorScaler );

struct bound_transform_track;
struct transform_track;
bool CheckLODTransform ( bound_transform_track const &Track, transform_track const &SourceTrack );

EXPAPI accumulation_mode AccumulationModeFromTrackGroup(track_group& TrackGroup);

bool ControlledAnimIsMirrored(controlled_animation& State);

// Allows sampling animation_bindings in an immediate mode manner
EXPAPI void AccumulateControlledAnimation(controlled_animation& State, sample_context& Context,
                                          real32 BaseWeight,
                                          skeleton& Skeleton, int32x FirstBone, int32x BoneCount,
                                          local_pose& Result,
                                          real32 AllowedError,
                                          int32x const* SparseBoneArray);

void AccumulateMirroredAnimation(controlled_animation& State,
                                 sample_context& Context,
                                 real32 BaseWeight,
                                 model_instance& Instance,
                                 skeleton& Skeleton, int32x FirstBone, int32x BoneCount,
                                 local_pose &Result,
                                 real32 AllowedError,
                                 int32x const* SparseBoneArray);

EXPAPI void AccumulateControlledAnimationMotionVectors(controlled_animation* State,
                                                       real32 StartLocalClock,
                                                       real32 EndLocalClock,
                                                       real32 LocalDuration,
                                                       real32 Weight,
                                                       bool   FlipResult,
                                                       bound_transform_track *BoundTrack,
                                                       transform_track const *SourceTrack,
                                                       real32* TotalWeight,
                                                       real32* ResultTranslation,
                                                       real32* ResultRotation);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_CONTROLLED_ANIMATION_H
#endif
