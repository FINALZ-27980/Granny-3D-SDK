#if !defined(GRANNY_ANIMATION_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_animation.h $
// $DateTime: 2012/10/22 16:12:50 $
// $Change: 39903 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GRANNY_DATA_TYPE_DEFINITION_H)
#include "granny_data_type_definition.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(AnimationGroup);

struct track_group;

EXPTYPE enum animation_flags
{
    AnimationDefaultLoopCountValid = 0x1,

    animation_flags_forceint = 0x7fffffff
};

EXPTYPE struct animation
{
    char const *Name;

    real32 Duration;
    real32 TimeStep;
    real32 Oversampling;

    int32 TrackGroupCount;
    track_group **TrackGroups;

    int32 DefaultLoopCount;
    int32 Flags;

    variant ExtendedData;
};
EXPCONST EXPGROUP(animation) extern data_type_definition AnimationType[];

EXPAPI GS_READ bool FindTrackGroupForModel(animation const &Animation,
                                           char const *ModelName,
                                           int32x &TrackGroupIndex);

EXPAPI GS_READ bool FindMorphTrackGroupForMesh(animation const &Animation,
                                               char const* MeshName,
                                               int32x &TrackGroupIndex);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_ANIMATION_H
#endif
