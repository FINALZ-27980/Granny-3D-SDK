// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(STATEMENT_XINPUT_H)

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GSTATE_BASE_H)
#include "gstate_base.h"
#endif


#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <xinput.h>

struct lua_State;

BEGIN_GSTATE_NAMESPACE;
class node;
END_GSTATE_NAMESPACE;


BEGIN_GRANNY_NAMESPACE;

struct variant;

void InitXInput();
void ShutdownXInput();
bool UIXInput_Register(lua_State* L);

void CreateEncodedXInput(variant* EditorData);
void DestroyEncodedXInput(variant* EditorData);
void ReadFromEncodedXInput(variant* EditorData);
void ResetXInput();

// These we can't watch for ourselves, so we must be notified.  It only happens in the
// editor, so we can arrange for that.
void XInput_NoteNodeDelete(GSTATE node*);
void XInput_NoteOutputDelete(GSTATE node*, int Output);
void XInput_NotePossibleEventDelete(GSTATE node*, int Output);

// Call once per frame to push events in...
void ProcessXInput(real32 AtT, real32 DeltaT);

END_GRANNY_NAMESPACE;


#define STATEMENT_XINPUT_H
#endif /* STATEMENT_XINPUT_H */
