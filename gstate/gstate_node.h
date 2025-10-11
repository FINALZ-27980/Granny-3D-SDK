// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_node.h $
// $DateTime: 2012/10/22 16:29:12 $
// $Change: 39907 $
// $Revision: #8 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_NODE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

#ifndef GSTATE_TOKENIZED_H
#include "gstate_tokenized.h"
#endif


BEGIN_GSTATE_NAMESPACE;

class container;
class transition;
class node_visitor;
struct animation_slot;

enum node_edge_type
{
    // These may NOT change.  Ever.
    PoseEdge   = 0,
    ScalarEdge = 1,
    MaskEdge   = 2,
    EventEdge  = 3,
    MorphEdge  = 4,

    // explicit because the above are explicit
    EdgeTypeCount = 5
};

class node : public tokenized
{
    typedef tokenized parent;

public:
    // Nodes are the first place we need to get character specific information...
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();
    gstate_character_instance* GetBoundCharacter();

    virtual void AcceptNodeVisitor(node_visitor*);

    virtual char const* GetName() const;
    virtual bool        SetName(char const* NewName);

    virtual void        MarkNameUserSpecified();
    virtual bool        IsNameUserSpecified();

    container* GetParent();
    void       SetParent(container*);

    // Nodes store a position to facilitate editing
    bool GetPosition(granny_int32x& x, granny_int32x& y);
    bool SetPosition(granny_int32x x, granny_int32x y);

    // If this returns < 0, the node does not support duration querying.
    virtual granny_real32 GetDuration(granny_int32x OnOutput);

    virtual bool GetPhaseBoundaries(granny_int32x OnOutput,
                                    granny_real32 AtT,
                                    granny_real32* PhaseStart,
                                    granny_real32* PhaseEnd);

    // =========================================================
    // Input/Output API
    // =========================================================
    granny_int32x GetNumInputs() const;
    granny_int32x GetNumOutputs() const;

    granny_int32x GetNthExternalInput(granny_int32x n) const;
    granny_int32x GetNthExternalOutput(granny_int32x n) const;
    granny_int32x GetNthInternalInput(granny_int32x n) const;
    granny_int32x GetNthInternalOutput(granny_int32x n) const;

    granny_int32x WhichExternalInput(granny_int32x RawIndex) const;
    granny_int32x WhichExternalOutput(granny_int32x RawIndex) const;

    virtual granny_int32x AddInput(node_edge_type EdgeType, char const* EdgeName);
    virtual bool          DeleteInput(granny_int32x InputIndex);

    virtual granny_int32x AddOutput(node_edge_type EdgeType, char const* EdgeName);
    virtual bool          DeleteOutput(granny_int32x OutputIndex);

    void           GetInputConnection(granny_int32x   InputIdx,
                                      node**          OtherNode,
                                      granny_int32x*  NodeOutputIdx) const;
    char const*    GetInputName(granny_int32x InputIdx) const;
    node_edge_type GetInputType(granny_int32x InputIdx) const;
    bool           IsInputExternal(granny_int32x InputIdx) const;
    bool           IsInputInternal(granny_int32x InputIdx) const { return !IsInputExternal(InputIdx); }
    bool           SetInputConnection(granny_int32x InputIdx,
                                      node*         OtherNode,
                                      granny_int32x OtherOutputIdx);
    virtual bool   SetInputName(granny_int32x InputIdx, char const* NewEdgeName);

    node_edge_type GetOutputType(granny_int32x OutputIdx) const;
    char const*    GetOutputName(granny_int32x OutputIdx) const;
    bool           IsOutputExternal(granny_int32x OutputIdx) const;
    bool           IsOutputInternal(granny_int32x OutputIdx) const { return !IsOutputExternal(OutputIdx); }
    virtual bool   SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName);

    // =========================================================
    // Sampling API
    // =========================================================

    virtual granny_real32 SampleScalarOutput(granny_int32x OutputIdx,
                                             granny_real32 AtT);
    virtual bool GetScalarOutputRange(granny_int32x  OutputIdx,
                                      granny_real32* OutMin,
                                      granny_real32* OutMax);  // returns false if range query unsupported


    // -- Animation pose API
    virtual granny_local_pose* SamplePoseOutput(granny_int32x OutputIdx,
                                                granny_real32 AtT,
                                                granny_real32 AllowedError,
                                                granny_pose_cache* PoseCache);
    virtual bool GetRootMotionVectors(granny_int32x  OutputIdx,
                                      granny_real32  AtT,
                                      granny_real32  DeltaT,
                                      granny_real32* Translation,
                                      granny_real32* Rotation,
                                      bool           Inverse);

    // todo: may have to adjust this for state_machine
    virtual granny_int32x GetNumMorphChannels(granny_int32x OutputIdx);
    virtual bool SampleMorphOutput(granny_int32x  OutputIdx,
                                   granny_real32  AtT,
                                   granny_real32* MorphWeights,
                                   granny_int32x NumMorphWeights);
    
    virtual bool SampleMaskOutput(granny_int32x OutputIdx,
                                  granny_real32 AtT,
                                  granny_track_mask*);

    // -- Event api
    virtual bool SampleEventOutput(granny_int32x            OutputIdx,
                                   granny_real32            AtT,
                                   granny_real32            DeltaT,
                                   granny_text_track_entry* EventBuffer,
                                   granny_int32x const      EventBufferSize,
                                   granny_int32x*           NumEvents);
    virtual bool GetNextEvent(granny_int32x OutputIdx,
                              granny_real32 AtT,
                              granny_text_track_entry* Event);
    virtual bool GetAllEvents(granny_int32x            OutputIdx,
                              granny_text_track_entry* EventBuffer,
                              granny_int32x const      EventBufferSize,
                              granny_int32x*           NumEvents); // Returns all possible events on this edge
    virtual bool GetCloseEventTimes(granny_int32x  OutputIdx,
                                    granny_real32  AtT,
                                    char const*    TextToFind,
                                    granny_real32* PreviousTime,
                                    granny_real32* NextTime);


    virtual void AdvanceT(granny_real32 CurrentTime, granny_real32 DeltaT) { }

    // =========================================================
    // Transition API
    // =========================================================
    granny_int32x GetNumTransitions() const;
    transition*   GetTransition(granny_int32x TransitionIdx);

    granny_int32x AddTransition(transition* Transition);
    bool          DeleteTransitionByIdx(granny_int32x TransitionIdx);
    bool          DeleteTransition(transition* Transition);

    // Used to validate edits in the tool.  Asserts if connections are found to be
    // incorrect after changes are baked.
    virtual void CheckConnections();

    // todo: need to use a vistor for this to seal it off?
public:
    virtual bool DidLoopOccur(granny_int32x OnOutput,
                              granny_real32 AtT,
                              granny_real32 DeltaT);

public:
    enum EConstants
    {
        eInvalidChild = -1
    };

    DECL_CLASS_TOKEN(node);
    IMPL_CASTABLE_INTERFACE(node);

public:
    // todo: need to loft this out into a "state_element" interface.
    //   tokenized->state_element->(node | transition)
    virtual void Activate(granny_int32x OnOutput, granny_real32 AtT);

    virtual void Synchronize(granny_int32x OnOutput,
                             granny_real32 AtT,
                             granny_real32 ReferenceStart,
                             granny_real32 ReferenceEnd,
                             granny_real32 LocalStart,
                             granny_real32 LocalEnd);

public:
    // Should only be called by container at creation time.
    virtual void CaptureSiblingLinks();
    virtual void CaptureSiblingData();

    // For advanced input/output walking
public:
    virtual bool GetOutputForward(granny_int32x OutputIdx, granny_int32x& ResultIdx) const;
    virtual bool GetInputForward(granny_int32x InputIdx, granny_int32x& ResultIdx) const;

    // Only happens in the editor
    virtual void NoteAnimationSlotDeleted(animation_slot* Slot);

    // For the editor only
public:
    void TransferTransitionsTo(node* OtherNode);  // transfers all transitions owned by this node
    void RetargetTransitionsTo(node* OtherNode);  // changes the *target* of the transitions owned by this node

    // Only for editor display
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;
    
protected:
    // Allows manipulation of internal inputs/outputs
    void SetInputInternal(granny_int32x InputIdx, granny_int32x ExternalOutput);
    void SetOutputInternal(granny_int32x OutputIdx, granny_int32x ExternalInput);

    // Notification of input changes, happens *after* the input has been switched
    virtual void NoteInputChange(granny_int32x InputIndex);

private:
    node**       m_Inputs;
    transition** m_Transitions;

    gstate_character_instance* m_BoundCharacter;
    container*          m_Parent;
};


END_GSTATE_NAMESPACE;

// Handy macros
#define INPUT_CONNECTION_ON(ptr, EdgeIdx, VarPrefix)                            \
    node* VarPrefix ## Node = 0;                                                \
    granny_int32x VarPrefix ## Edge = -1;                                       \
    (ptr)->GetInputConnection((EdgeIdx), &(VarPrefix##Node), &(VarPrefix##Edge))

#define INPUT_CONNECTION(EdgeIdx, VarPrefix) INPUT_CONNECTION_ON(this, EdgeIdx, VarPrefix)



#include "gstate_header_postfix.h"
#define GSTATE_NODE_H
#endif /* GSTATE_NODE_H */
