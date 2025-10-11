// ========================================================================
// $File: //jeffr/granny_29/statement/ui_editfield.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(UI_EDITFIELD_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

struct lua_State;
BEGIN_GRANNY_NAMESPACE;

bool UIEditField_Register(lua_State* L);

END_GRANNY_NAMESPACE;

#define UI_EDITFIELD_H
#endif /* UI_EDITFIELD_H */
