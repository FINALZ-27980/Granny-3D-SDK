// ========================================================================
// $File: //jeffr/granny_29/statement/ui_statemachine_drawing.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(UI_STATEMACHINE_DRAWING_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

#ifndef GSTATE_STATE_MACHINE_H
#include "gstate_state_machine.h"
#endif


struct lua_State;

BEGIN_GRANNY_NAMESPACE;

struct LuaRect;


int StateMachineRender(lua_State* L, GSTATE state_machine* Machine);

LuaRect ComputeStateNodeRect(GSTATE node* Node);

END_GRANNY_NAMESPACE;

#define UI_STATEMACHINE_DRAWING_H
#endif /* UI_STATEMACHINE_DRAWING_H */
