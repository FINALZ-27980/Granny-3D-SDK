#if !defined(GRANNY_SPU_ANIMATION_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_spu_animation.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_DATA_TYPE_DEFINITION_H)
#include "granny_data_type_definition.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(SPUGroup);

struct spu_track_group;

EXPTYPE struct spu_animation
{
    char const *Name;

    real32 Duration;
    real32 TimeStep;
    real32 Oversampling;

    int32 TrackGroupCount;
    spu_track_group** TrackGroups;
};
EXPCONST EXPGROUP(spu_animation) extern data_type_definition SPUAnimationType[];

EXPAPI GS_READ bool FindSPUTrackGroupForModel(spu_animation const& Animation,
                                              char const *ModelName,
                                              int32x &TrackGroupIndex);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_SPU_ANIMATION_H
#endif
