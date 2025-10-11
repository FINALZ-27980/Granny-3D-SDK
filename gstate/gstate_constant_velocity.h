// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_constant_velocity.h $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_CONSTANT_VELOCITY_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class constant_velocity : public node
{
    typedef node parent;

public:
    virtual void AcceptNodeVisitor(node_visitor* Visitor);
    virtual granny_real32 GetDuration(granny_int32x OnOutput);

    virtual granny_local_pose* SamplePoseOutput(granny_int32x      OutputIdx,
                                                granny_real32      AtT,
                                                granny_real32      AllowedError,
                                                granny_pose_cache* PoseCache);

    virtual bool GetRootMotionVectors(granny_int32x  OutputIdx,
                                      granny_real32  AtT,
                                      granny_real32  DeltaT,
                                      granny_real32* ResultTranslation,
                                      granny_real32* ResultRotation,
                                      bool           Inverse);

    void          GetAxis(granny_real32* Axis);
    granny_real32 GetSpeed();

    void SetAxis(granny_real32 const* Axis);
    void SetSpeed(granny_real32 Speed);

    DECL_CONCRETE_CLASS_TOKEN(constant_velocity);

public:
    virtual void Activate(granny_int32x OnOutput, granny_real32 AtT);
    virtual bool DidLoopOccur(granny_int32x OnOutput, granny_real32 AtT, granny_real32 DeltaT);
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_CONSTANT_VELOCITY_H
#endif /* GSTATE_CONSTANT_VELOCITY_H */
