#if !defined(GRANNY_RETARGETER_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_retargeter.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GRANNY_ASSERT_H)
#include "granny_assert.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(ModelGroup);

struct model;
struct local_pose;
struct skeleton;
struct transform;

struct retarget_identifier
{
    model *FromBasis;
    model *ToBasis;
};

EXPTYPE struct retargeter;
struct retargeter
{
    retarget_identifier ID;

    intaddrx   NumBones;
    intaddrx   ReferenceCount;

    int32*     SourceIndices;
    transform* RetargetingTransforms;

    retargeter* Right;
    retargeter* Left;
};
CompileAssert(IS_ALIGNED_16(SizeOf(retargeter)));

EXPAPI retargeter* AcquireRetargeter(model* From, model* To);
EXPAPI void ReleaseRetargeter(retargeter* Retargeter);

EXPAPI bool RetargetPose(local_pose const& FromPose,
                         local_pose& DestPose,
                         int32x DestBoneStart,
                         int32x DestBoneCount,
                         retargeter& Retargeter);

EXPAPI int32* GetRetargeterSourceIndices(retargeter& Retargeter);
EXPAPI model* GetRetargeterSourceModel(retargeter& Retargeter);
EXPAPI model* GetRetargeterTargetModel(retargeter& Retargeter);

void RebasingTransform(transform& Dest,
                       transform const& Source,
                       transform const& Rebaser);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_RETARGETER_H
#endif
