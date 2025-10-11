#if !defined(GRANNY_TRACK_GROUP_BUILDER_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_track_group_builder.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(TrackGroupBuilderGroup);

EXPTYPE struct track_group_builder;
struct memory_arena;
struct curve2;
struct track_group;

EXPAPI GS_SAFE track_group_builder *BeginTrackGroup(char const *Name,
                                                    int32x VectorTrackCount,
                                                    int32x TransformTrackCount,
                                                    int32x TextTrackCount,
                                                    bool IncludeLODErrorSpace);
EXPAPI GS_PARAM track_group *EndTrackGroup(track_group_builder *Builder);

EXPAPI GS_READ int32x GetResultingTrackGroupSize(track_group_builder const &Builder);
EXPAPI GS_PARAM track_group *EndTrackGroupInPlace(track_group_builder *Builder,
                                                  void *Memory);

track_group *EndTrackGroupToArena(track_group_builder *Builder,
                                  memory_arena& Arena);

EXPAPI GS_PARAM void PushVectorTrackCurve(track_group_builder &Builder,
                                          char const *Name,
                                          uint32 TrackKey,
                                          curve2 const *SourceCurve);

EXPAPI GS_PARAM void BeginTransformTrack(track_group_builder &Builder, char const *Name, int32x Flags);
EXPAPI GS_PARAM void SetTransformTrackPositionCurve(track_group_builder &Builder, curve2 const *SourceCurve);
EXPAPI GS_PARAM void SetTransformTrackOrientationCurve(track_group_builder &Builder, curve2 const *SourceCurve);
EXPAPI GS_PARAM void SetTransformTrackScaleShearCurve(track_group_builder &Builder, curve2 const *SourceCurve);
EXPAPI GS_PARAM void EndTransformTrack(track_group_builder &Builder);

EXPAPI GS_PARAM void BeginTextTrack(track_group_builder &Builder, char const *Name);
EXPAPI GS_PARAM void AddTextEntry(track_group_builder &Builder,
                                  real32 TimeStamp, char const *Text);
EXPAPI GS_PARAM void EndTextTrack(track_group_builder &Builder);

EXPAPI GS_PARAM void ResortTrackGroup(track_group &Group);

EXPAPI GS_PARAM void AllocateLODErrorSpace(track_group &Group);
EXPAPI GS_PARAM void FreeLODErrorSpace(track_group &Group);
EXPAPI GS_PARAM void SetAllLODErrorSpace(track_group &Group, real32 Value);
EXPAPI GS_PARAM void ResetLODErrorSpace(track_group &Group);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_TRACK_GROUP_BUILDER_H
#endif
