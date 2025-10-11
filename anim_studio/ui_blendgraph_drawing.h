// ========================================================================
// $File: //jeffr/granny_29/statement/ui_blendgraph_drawing.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(UI_BLENDGRAPH_DRAWING_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

#ifndef GSTATE_BLEND_GRAPH_H
#include "gstate_blend_graph.h"
#endif

struct lua_State;


BEGIN_GRANNY_NAMESPACE;

struct LuaRect;


int BlendGraphRender(lua_State* L, GSTATE blend_graph* BlendGraph);

LuaRect ComputeBlendGraphNodeRect(GSTATE node* Node);

END_GRANNY_NAMESPACE;

#define UI_BLENDGRAPH_DRAWING_H
#endif /* UI_BLENDGRAPH_DRAWING_H */
