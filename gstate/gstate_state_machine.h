// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_state_machine.h $
// $DateTime: 2012/10/22 16:29:12 $
// $Change: 39907 $
// $Revision: #10 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_STATE_MACHINE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_CONTAINER_H
#include "gstate_container.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class parameters;
class conditional;

class state_machine : public container
{
    typedef container parent;

public:
    virtual void AcceptNodeVisitor(node_visitor*);

    virtual int   AddChild(node* Child);
    virtual bool  RemoveChildByIdx(int ChildIdx);

    int   GetStartStateIdx() const;
    void  SetStartStateIdx(int StartState);
    void  SetStartState(node* State);

    tokenized* GetActiveElement();

    bool MoveNodeToFront(node* Node);
    
    // By name API
    bool RequestChangeTo(granny_real32 AtT, granny_real32 DeltaT, node* State);
    bool RequestChangeToState(granny_real32 AtT, granny_real32 DeltaT, char const* StateName);
    bool ForceChangeToState(granny_real32 AtT, char const* StateName);

    // Normally, you should use these
    bool StartTransitionByName(granny_real32 AtT, char const* TransitionName);
    bool StartTransition(granny_real32 AtT, transition* Transition);
    bool ForceStartTransition(granny_real32 AtT, transition* Transition);

    // But if necessary: hit with hammer!
    bool ForceState(granny_real32 AtT, node* State);
    bool ForceTransition(granny_real32 AtT, transition* Transition);

    virtual granny_real32      SampleScalarOutput(granny_int32x OutputIdx,
                                                  granny_real32 AtT);

    virtual granny_local_pose* SamplePoseOutput(granny_int32       OutputIdx,
                                                granny_real32      AtT,
                                                granny_real32      AllowedError,
                                                granny_pose_cache* PoseCache);

    virtual granny_int32x GetNumMorphChannels(granny_int32x OutputIdx);
    virtual bool SampleMorphOutput(granny_int32x  OutputIdx,
                                   granny_real32  AtT,
                                   granny_real32* MorphWeights,
                                   granny_int32x NumMorphWeights);

    virtual bool               SampleMaskOutput(granny_int32x OutputIdx,
                                                granny_real32 AtT,
                                                granny_track_mask*);
                               
    virtual bool               SampleEventOutput(granny_int32x            OutputIdx,
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

    virtual bool GetRootMotionVectors(granny_int32x OutputIdx,
                                      granny_real32 AtT,
                                      granny_real32 DeltaT,
                                      granny_real32* Translation,
                                      granny_real32* Rotation,
                                      bool Inverse);

    virtual void AdvanceT(granny_real32 CurrentTime, granny_real32 DeltaT);

    DECL_CONCRETE_CLASS_TOKEN(state_machine);
    DECL_SNAPPABLE();
    IMPL_CASTABLE_INTERFACE(state_machine);

    // Published parameters (corresponds to edges other than 0)
public:
    virtual granny_int32x AddOutput(node_edge_type EdgeType, char const* EdgeName);
    virtual bool          DeleteOutput(granny_int32x OutputIndex);

    virtual bool SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName);

    // Conditionals for transitions, etc.
public:
    granny_int32x GetNumConditionals();
    conditional*  GetConditional(granny_int32x Index);
    // bool          IsConditionalTrue(granny_int32x Index, granny_real32 AtT);

    granny_int32x AddConditional(conditional* NewCondition);
    bool ReplaceConditional(granny_int32x ConditionalIndex, conditional* NewCondition);
    void DeleteConditional(granny_int32x ConditionalIndex);

    static bool IsStateNode(node*);

public:
    virtual void Activate(granny_int32x OnOutput, granny_real32 AtT);
    virtual bool BindToCharacter(gstate_character_instance* Instance);

    virtual bool DidSubLoopOccur(node*         SubNode,
                                 granny_int32  OnOutput,
                                 granny_real32 AtT,
                                 granny_real32 DeltaT);

    // For keeping the edge names consistent.  Not really for client use
    void NoteParameterNameChange(node* ParamOrEvent, int OutputIdx);
    virtual void NoteOutputRemoval_Pre(node* AffectedNode, granny_int32x ToBeRemoved);
    virtual void NoteOutputRemoval_Post(node* AffectedNode, bool WasExternal);
    virtual void NoteOutputAddition(node* AffectedNode, granny_int32x InsertionIndex);

    void NoteDeleteTransition(transition* Transition);

    virtual granny_int32x AddInput(node_edge_type EdgeType, char const* EdgeName);
    virtual bool          DeleteInput(granny_int32x InputIndex);
    
    virtual void CheckConnections();

private:
    tokenized*    m_Active;

    conditional** m_Conditionals;

private:
    void AdjustChildInputs();
    void AddInputsForNode(node* Node);
    void ActivateConditionals(granny_real32 AtT);
    
    // bool AddParameterToChildren(node* Param, int OutputIdx);
    // bool RemoveParameterFromChildren(node* Param, int OutputIdx);

    bool AddOutputToChildren(int NewOutputIdx);
    bool RemoveOutputFromChildren(int DelOutputIdx);
    void RemoveOutputFromConditionals(node* AffectedNode, int DelOutputIdx);
    void RefreshChildOutputName(granny_int32x OutputIdx);

    void DefaultStartState();
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_STATE_MACHINE_H
#endif /* GSTATE_STATE_MACHINE_H */
