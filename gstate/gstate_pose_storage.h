// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_pose_storage.h $
// $DateTime: 2012/03/16 15:41:10 $
// $Change: 36794 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_POSE_STORAGE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class pose_storage : public node
{
    typedef node parent;

public:
    void ReturnToRestPose();

    // For now, this is only runnable in the tool.  That restriction may relax over time.
    void CaptureInput(granny_real32 AtT);

    virtual void AcceptNodeVisitor(node_visitor* Visitor);
    virtual granny_local_pose* SamplePoseOutput(granny_int32x  OutputIdx,
                                                granny_real32 AtT,
                                                granny_real32 AllowedError,
                                                granny_pose_cache* PoseCache);

    virtual bool GetRootMotionVectors(granny_int32x  OutputIdx,
                                      granny_real32  AtT,
                                      granny_real32  DeltaT,
                                      granny_real32* ResultTranslation,
                                      granny_real32* ResultRotation,
                                      bool           Inverse);

    DECL_CONCRETE_CLASS_TOKEN(pose_storage);
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_POSE_STORAGE_H
#endif /* GSTATE_POSE_STORAGE_H */
