// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_EVENT_RENAMER_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class event_renamer : public node
{
    typedef node parent;

public:
    virtual void AcceptNodeVisitor(node_visitor* Visitor);

    granny_int32x NumRenames();
    void          AddRename(char const* NameInput, char const* NameOutput);
    void          DeleteRename(granny_int32x Index);

    char const*   GetRenameInput(granny_int32x Index);
    char const*   GetRenameOutput(granny_int32x Index);

    void SetRename(granny_int32x Index, char const* NameInput, char const* NameOutput);

    virtual bool SampleEventOutput(granny_int32x            OutputIdx,
                                   granny_real32            AtT,
                                   granny_real32            DeltaT,
                                   granny_text_track_entry* EventBuffer,
                                   granny_int32x const      EventBufferSize,
                                   granny_int32x*           NumEvents);
    virtual bool GetNextEvent(granny_int32x OutputIdx,
                              granny_real32 AtT,
                              granny_text_track_entry* Event);

    // Returns all possible events on this edge
    virtual bool GetAllEvents(granny_int32x            OutputIdx,
                              granny_text_track_entry* EventBuffer,
                              granny_int32x const      EventBufferSize,
                              granny_int32x*           NumEvents);

    virtual bool GetCloseEventTimes(granny_int32x  OutputIdx,
                                    granny_real32  AtT,
                                    char const*    TextToFind,
                                    granny_real32* PreviousTime,
                                    granny_real32* NextTime);

    DECL_CONCRETE_CLASS_TOKEN(event_renamer);

public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_EVENT_RENAMER_H
#endif /* GSTATE_EVENT_RENAMER_H */

