// ========================================================================
// $File: //jeffr/granny_29/statement/ui_blendnode_edit.h $
// $DateTime: 2012/03/15 17:00:19 $
// $Change: 36783 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#if !defined(UI_BLENDNODE_EDIT_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

#ifndef GSTATE_NODE_VISITOR_H
#include "gstate_node_visitor.h"
#endif


struct lua_State;

BEGIN_GRANNY_NAMESPACE;

int Edit_BlendNodeEdit(lua_State* L);
int Edit_StateNodeEdit(lua_State* L);

int Edit_TransitionEdit(lua_State* L);   // in ui_transition_edit.cpp


END_GRANNY_NAMESPACE;

#define UI_BLENDNODE_EDIT_H
#endif /* UI_BLENDNODE_EDIT_H */
