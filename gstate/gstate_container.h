// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_container.h $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #5 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_CONTAINER_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class container : public node
{
    typedef node parent;

public:
    bool ValidInputOutput() const;  // debug check...

    // The container stores a position to facilitate editing
    bool GetEditPosition(int& x, int& y);
    bool SetEditPosition(int x, int y);

    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    int   GetNumChildren() const;
    node* GetChild(int ChildIdx) const;
    int   GetChildIdx(node* Child) const;

    int   FindChildIdxByName(char const* Name) const;
    node* FindChildByName(char const* Name) const;

    virtual bool MoveNodeToFront(node* Node);

    // Allows paths of the form
    //  - name|name|name or
    //  - name|*|name
    // * matches any container at ONE level.
    node* FindChildRecursively(char const* Path) const;

    virtual int   AddChild(node* Child);
    virtual bool  RemoveChildByIdx(int ChildIdx);
    bool          RemoveChild(node* Child);  // forwards to removebyidx, no need to override

    bool DetectLoops() const;

    // Container overrides these to add the internal edges at the same time
    virtual granny_int32x AddInput(node_edge_type EdgeType, char const* EdgeName);
    virtual bool          DeleteInput(granny_int32x InputIndex);
    virtual granny_int32x AddOutput(node_edge_type EdgeType, char const* EdgeName);
    virtual bool          DeleteOutput(granny_int32x OutputIndex);

    virtual bool   SetInputName(granny_int32x InputIdx, char const* NewEdgeName);
    virtual bool   SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName);

    // Did a subnode have a loop event?
    virtual bool DidSubLoopOccur(node*         SubNode,
                                 granny_int32  OnOutput,
                                 granny_real32 AtT,
                                 granny_real32 DeltaT) = 0;

    virtual void AdvanceT(granny_real32 CurrentTime, granny_real32 DeltaT);

    
    // Debug check
    virtual void CheckConnections();

    int GetNumComments();
    int AddComment(char const* InitialText,
                   granny_int32x PosX, granny_int32x PosY,
                   granny_int32x SizeX, granny_int32x SizeY);
    void DeleteComment(granny_int32x CommentIdx);
    bool MoveCommentToFront(granny_int32x CommentIdx);

    char const* GetCommentText(granny_int32x CommentIdx);
    bool SetCommentText(granny_int32x CommentIdx, char const* NewText);

    bool GetCommentPosition(granny_int32x CommentIdx,
                            granny_int32x* PosX,
                            granny_int32x* PosY,
                            granny_int32x* SizeX,
                            granny_int32x* SizeY);
    bool SetCommentPosition(granny_int32x CommentIdx,
                            granny_int32x PosX,
                            granny_int32x PosY,
                            granny_int32x SizeX,
                            granny_int32x SizeY);
public:
    enum EConstants
    {
        eInvalidChild = -1
    };

private:
    // Cached version of the child token array
    node** m_ChildNodes;

    DECL_CLASS_TOKEN(container);
    IMPL_CASTABLE_INTERFACE(container);

public:
    bool ConnectedInput(granny_int32x ForOutput, node** Node, granny_int32x* EdgeIdx);

    // Only happens in the editor
    virtual void NoteAnimationSlotDeleted(animation_slot* Slot);

    // Only for the use of internal node functions, if you please
protected:
    friend class node;

    virtual void NoteOutputRemoval_Pre(node* AffectedNode, granny_int32x ToBeRemoved);
    virtual void NoteOutputRemoval_Post(node* AffectedNode, bool WasExternal);

    virtual void NoteOutputAddition(node* AffectedNode, granny_int32x InsertionIndex);

    virtual void LinkContainerChildren();
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_CONTAINER_H
#endif /* GSTATE_CONTAINER_H */
