#if !defined(GRANNY_MIRROR_SPECIFICATION_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_mirror_specification.h $
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

struct skeleton;
struct local_pose;
struct transform;
struct track_mask;

// Return true for success, even if the name is simply copied from base to mirrored
EXPAPI typedef CALLBACK_FN(bool) mirror_name_callback(char const* BaseName,
                                                      char*       MirroredNameBuffer,
                                                      int32x      BufferSize,
                                                      void*       UserData);

EXPTYPE_EPHEMERAL enum mirror_axis
{
    MirrorXAxis   = 0,
    MirrorYAxis   = 1,
    MirrorZAxis   = 2,

    // Special for the post flip axis, not valid for ComputedMirroredPose, or the
    // FlipAround argument of BuildMirroringBuffer
    MirrorAllAxes = 3,

    mirror_axis_forceint = 0x7fffffff
};

EXPTYPE_EPHEMERAL struct mirror_specification
{
    mirror_axis MirrorAxis;

    int32   RemapCount;
    uint16* Remaps;

    uint8*  PostFlips;  // This variable is uint8(mirror_axis)
};

EXPAPI GS_SAFE mirror_specification* NewMirrorSpecification(int32x BoneCount, mirror_axis MirrorAround);
EXPAPI GS_PARAM void FreeMirrorSpecification(mirror_specification* Specification);

EXPAPI GS_PARAM bool BuildMirroringIndices(mirror_specification* Specification,
                                           skeleton* Skeleton,
                                           mirror_name_callback* NameCallback,
                                           void *UserData);

EXPAPI bool MirrorLocalPose(mirror_specification* Specification,
                            skeleton* Skeleton,
                            local_pose* Pose);

void MirrorPoseTransforms(mirror_specification* Specification,
                          int32x TransformStride, transform* Transforms,
                          int32x SourceParentStride, int32 const *SourceParents,
                          int32x TransformCount);

void MaskedMirrorPoseTransforms(mirror_specification* Specification,
                                int32x TransformStride, transform* Transforms,
                                int32x SourceParentStride, int32 const *SourceParents,
                                int32x TransformCount, track_mask const* ModelMask);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MIRROR_SPECIFICATION_H
#endif /* GRANNY_MIRROR_SPECIFICATION_H */
