// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_masked_combine.h $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_MASKED_COMBINE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class masked_combine : public node
{
    typedef node parent;

public:
    virtual void AcceptNodeVisitor(node_visitor* Visitor);
    virtual granny_local_pose* SamplePoseOutput(granny_int32x OutputIdx,
                                                granny_real32 AtT,
                                                granny_real32 AllowedError,
                                                granny_pose_cache* PoseCache);
    virtual bool GetRootMotionVectors(granny_int32x OutputIdx,
                                      granny_real32 AtT,
                                      granny_real32 DeltaT,
                                      granny_real32* Translation,
                                      granny_real32* Rotation,
                                      bool Inverse);

    // todo: duration?
    //virtual granny_real32 GetDuration(granny_int32x OnOutput);

    virtual void Activate(granny_int32x OutputIdx, granny_real32 AtT);

    // todo: on FromEdge? virtual bool DidLoopOccur(granny_int32x OutputIdx, granny_real32 AtT, granny_real32 DeltaT);
    
    DECL_CONCRETE_CLASS_TOKEN(masked_combine);
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_MASKED_COMBINE_H
#endif /* GSTATE_MASKED_COMBINE_H */
