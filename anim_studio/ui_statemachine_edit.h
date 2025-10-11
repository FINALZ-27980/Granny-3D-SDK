// ========================================================================
// $File: //jeffr/granny_29/statement/ui_statemachine_edit.h $
// $DateTime: 2012/03/15 17:00:19 $
// $Change: 36783 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#if !defined(UI_STATEMACHINE_EDIT_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

struct lua_State;

BEGIN_GSTATE_NAMESPACE;
class state_machine;
END_GSTATE_NAMESPACE;


BEGIN_GRANNY_NAMESPACE;

// Calling from C
int EditStateMachineNode(lua_State* L, GSTATE state_machine* Machine, int StartY);

END_GRANNY_NAMESPACE;

#define UI_STATEMACHINE_EDIT_H
#endif /* UI_STATEMACHINE_EDIT_H */
