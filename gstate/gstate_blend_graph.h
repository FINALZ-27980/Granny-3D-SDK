// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_blend_graph.h $
// $DateTime: 2012/10/22 16:29:12 $
// $Change: 39907 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_BLEND_GRAPH_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_CONTAINER_H
#include "gstate_container.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class blend_graph : public container
{
    typedef container parent;

public:
    virtual void AcceptNodeVisitor(node_visitor*);

    virtual granny_real32      SampleScalarOutput(granny_int32x OutputIdx,
                                                  granny_real32 AtT);
    virtual bool               GetScalarOutputRange(granny_int32x OutputIdx,
                                                    granny_real32* OutMin, granny_real32* OutMax);

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

    virtual granny_int32x GetNumMorphChannels(granny_int32x OutputIdx);
    virtual bool SampleMorphOutput(granny_int32x  OutputIdx,
                                   granny_real32  AtT,
                                   granny_real32* MorphWeights,
                                   granny_int32x NumMorphWeights);

    virtual bool SampleMaskOutput(granny_int32x OutputIdx,
                                  granny_real32 AtT,
                                  granny_track_mask* Mask);
    virtual bool SampleEventOutput(granny_int32x            OutputIdx,
                                   granny_real32            AtT,
                                   granny_real32            DeltaT,
                                   granny_text_track_entry* EventBuffer,
                                   granny_int32x const      EventBufferSize,
                                   granny_int32x*           NumEvents);
    virtual bool GetAllEvents(granny_int32x            OutputIdx,
                              granny_text_track_entry* EventBuffer,
                              granny_int32x const      EventBufferSize,
                              granny_int32x*           NumEvents);
    virtual bool GetCloseEventTimes(granny_int32x  OutputIdx,
                                    granny_real32  AtT,
                                    char const*    TextToFind,
                                    granny_real32* PreviousTime,
                                    granny_real32* NextTime);


public:
    virtual void Activate(granny_int32x OnOutput, granny_real32 AtT);

    virtual bool DidSubLoopOccur(node*         SubNode,
                                 granny_int32  OnOutput,
                                 granny_real32 AtT,
                                 granny_real32 DeltaT);

    bool DidLoopOccur(granny_int32x OnOutput,
                      granny_real32 AtT,
                      granny_real32 DeltaT);

protected:
    DECL_CONCRETE_CLASS_TOKEN(blend_graph);
    IMPL_CASTABLE_INTERFACE(blend_graph);
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_BLEND_GRAPH_H
#endif /* GSTATE_BLEND_GRAPH_H */
