// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_sequencer.h $
// $DateTime: 2012/03/16 15:41:10 $
// $Change: 36794 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_SEQUENCER_H)

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

struct granny_sequencer_specifcation;

BEGIN_GSTATE_NAMESPACE;

class sequencer : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

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

    DECL_CONCRETE_CLASS_TOKEN(sequencer);

public:
    virtual void Activate(granny_int32x OnOutput, granny_real32 AtT);
    virtual bool DidLoopOccur(granny_int32x OnOutput, granny_real32 AtT, granny_real32 DeltaT);

private:
    granny_controlled_animation** m_Animations;
};

END_GSTATE_NAMESPACE;

#define GSTATE_SEQUENCER_H
#endif /* GSTATE_SEQUENCER_H */

