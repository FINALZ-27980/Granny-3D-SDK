// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_additive_blend.h $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_ADDITIVE_BLEND_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class additive_blend : public node
{
    typedef node parent;

public:
    virtual void AcceptNodeVisitor(node_visitor*);

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

    // Todo: duration?
    //virtual granny_real32 GetDuration(granny_int32x OnOutput);

    virtual void Activate(granny_int32x OnOutput, granny_real32 AtT);
    // todo: DidLoopOccur only for eIntoEdge?
    
protected:
    DECL_CONCRETE_CLASS_TOKEN(additive_blend);
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_ADDITIVE_BLEND_H
#endif /* GSTATE_ADDITIVE_BLEND_H */
