// ========================================================================
// $File: //jeffr/granny_29/statement/statement_undostack.h $
// $DateTime: 2012/05/17 11:49:45 $
// $Change: 37439 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#if !defined(STATEMENT_UNDOSTACK_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif


BEGIN_GRANNY_NAMESPACE;

// Matches ui_undostack.lua
enum EUndoClass
{
    eNeverCollapse = 0,
    eNodeMove      = 1,
    eNodeSelect    = 2,

    eSliderMotion  = 3,

    eCommentMove   = 4,
};

bool PushUndoPos(char const*   PositionName,
                 bool          DontMarkModified = false,
                 EUndoClass    Class            = eNeverCollapse,
                 uint32        UID              = 0);
bool PopUndoPos();

int32x PushCollapseMarker(char const* PositionName);
bool   CollapseToMarker(int32x Marker);

bool PushTimeScrub(real32 CurrT);
bool GetScrubWindow(real32& Minimum, real32& Maximum);
bool ScrubToTime(real32 ScrubTime);

void ClearUndoStack();
int  UndoStackCount();
int  UndoStackSize();

int  ScrubListSize();


END_GRANNY_NAMESPACE;

#define STATEMENT_UNDOSTACK_H
#endif /* STATEMENT_UNDOSTACK_H */
