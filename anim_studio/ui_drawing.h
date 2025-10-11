#if !defined(UI_DRAWING_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/statement/ui_drawing.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif


struct lua_State;
BEGIN_GRANNY_NAMESPACE;

struct LuaRect;
struct LuaColor;

void DrawRect(LuaRect const& Rect, LuaColor const& Color);

// Note that DrawOutlineRect fills in the rectangle, while DrawRectOutline draws just the
// outline...
void DrawOutlineRect(LuaRect const& Rect, LuaColor const& Color, LuaColor const& OutlineColor);
void DrawRectOutline(LuaRect const& Rect, LuaColor const& Color);

void SetLineWidth(real32 Width);
real32 GetZoomFactor();

bool UIDrawing_Register(lua_State*);

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define UI_DRAWING_H
#endif /* UI_DRAWING_H */
