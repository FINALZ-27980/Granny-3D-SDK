// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_anim_source.h $
// $DateTime: 2012/10/22 16:29:12 $
// $Change: 39907 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_ANIM_SOURCE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

struct animation_slot;

class anim_source : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    virtual void AcceptNodeVisitor(node_visitor* Visitor);

    virtual granny_real32 GetDuration(granny_int32x OnOutput);
    virtual bool GetPhaseBoundaries(granny_int32x  OnOutput,
                                    granny_real32  AtT,
                                    granny_real32* PhaseStart,
                                    granny_real32* PhaseEnd);

    virtual granny_int32x AddOutput(node_edge_type EdgeType, char const* EdgeName);
    virtual bool          DeleteOutput(granny_int32x OutputIndex);

    animation_slot* GetAnimationSlot();
    bool            GetOutputLoopClamping() const;
    granny_real32   GetOutputSpeed() const;

    void SetAnimationSlot(animation_slot* Slot);
    void SetOutputLoopClamping(bool Clamped);
    void SetOutputSpeed(granny_real32 Speed);

    bool        GetEventUseAnimEvents(granny_int32x ForOutput);
    char const* GetEventTrackName(granny_int32x ForOutput);
    bool        SetEventTrack(granny_int32x ForOutput, bool UseAnimEvents, char const* TrackName);

    bool GetScalarTrack(granny_int32x  ForOutput,
                        char const*&   Name,
                        granny_uint32& TrackKey);
    bool SetScalarTrack(granny_int32x       ForOutput,
                        char const*         TrackName,
                        granny_uint32 const TrackKey);

    char const* GetMorphTrackName(granny_int32x  ForOutput);
    bool SetMorphTrackName(granny_int32x       ForOutput,
                           char const*         TrackName);

    virtual granny_local_pose* SamplePoseOutput(granny_int32x      OutputIdx,
                                                granny_real32      AtT,
                                                granny_real32      AllowedError,
                                                granny_pose_cache* PoseCache);

    virtual granny_int32x GetNumMorphChannels(granny_int32x OutputIdx);
    virtual bool SampleMorphOutput(granny_int32x  OutputIdx,
                                   granny_real32  AtT,
                                   granny_real32* MorphWeights,
                                   granny_int32x NumMorphWeights);

    virtual granny_real32 SampleScalarOutput(granny_int32x OutputIdx,
                                             granny_real32 AtT);
    virtual bool SampleEventOutput(granny_int32x            OutputIdx,
                                   granny_real32            AtT,
                                   granny_real32            DeltaT,
                                   granny_text_track_entry* EventBuffer,
                                   granny_int32x const      EventBufferSize,
                                   granny_int32x*           NumEvents);

    bool GetAllEvents(granny_int32x            OutputIdx,
                      granny_text_track_entry* EventBuffer,
                      granny_int32x const      EventBufferSize,
                      granny_int32x*           NumEvents);

    bool GetCloseEventTimes(granny_int32x  OutputIdx,
                            granny_real32  AtT,
                            char const*    TextToFind,
                            granny_real32* PrevTime,
                            granny_real32* NextTime);
    
    bool GetRootMotionVectors(granny_int32x  OutputIdx,
                              granny_real32  AtT,
                              granny_real32  DeltaT,
                              granny_real32* Translation,
                              granny_real32* Rotation,
                              bool Inverse);

    DECL_CONCRETE_CLASS_TOKEN(anim_source);
    DECL_SNAPPABLE();
    IMPL_CASTABLE_INTERFACE(anim_source);

public:
    virtual void Activate(granny_int32x OnOutput, granny_real32 AtT);
    virtual void Synchronize(granny_int32x OnOutput,
                             granny_real32 AtT,
                             granny_real32 ReferenceStart,
                             granny_real32 ReferenceEnd,
                             granny_real32 LocalStart,
                             granny_real32 LocalEnd);

    bool DidLoopOccur(granny_int32x OnOutput,
                      granny_real32 AtT,
                      granny_real32 DeltaT);

protected:
    void NoteAnimationSlotDeleted(animation_slot* Slot);


private:
    granny_controlled_animation* m_Animation;
    granny_text_track**          m_EventTracks;   // sparse?

    granny_curve2**              m_ScalarTracks;  // sparse?
    granny_real32*               m_ScalarDefaults;

    granny_track_group**         m_MorphTracks;  // sparse?
    
    granny_real32                m_LocalOffset;
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_ANIM_SOURCE_H
#endif /* GSTATE_ANIM_SOURCE_H */
