// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_timeselect.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_TIMESELECT_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class timeselect : public node
{
    typedef node parent;

public:
    virtual void AcceptNodeVisitor(node_visitor* Visitor);

    virtual granny_local_pose* SamplePoseOutput(granny_int32x      OutputIdx,
                                                granny_real32      AtT,
                                                granny_real32      AllowedError,
                                                granny_pose_cache* PoseCache);
    virtual bool GetRootMotionVectors(granny_int32x  OutputIdx,
                                      granny_real32  AtT,
                                      granny_real32  DeltaT,
                                      granny_real32* Translation,
                                      granny_real32* Rotation,
                                      bool Inverse);

    virtual granny_int32x AddOutput(node_edge_type EdgeType, char const* EdgeName);
    virtual bool          DeleteOutput(granny_int32x OutputIndex);
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

    // todo: handle, or do we go with Zeno? virtual bool DidLoopOccur

    bool GetTimeIsRelative() const;
    void SetTimeIsRelative(bool IsRelative);

    DECL_CONCRETE_CLASS_TOKEN(timeselect);
private:
    granny_real32 GetPosition(granny_real32 AtT);
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_TIMESELECT_H
#endif /* GSTATE_TIMESELECT_H */
