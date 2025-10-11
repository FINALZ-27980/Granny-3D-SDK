#if !defined(UI_AREA_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/statement/ui_area.h $
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
struct LuaPoint;

bool UIArea_Register(lua_State*);

// Same as the interface from ui_area.lua.  Note that UIAreaPush returns false if the
// drawing area is completely clipped.
bool    UIAreaPush(LuaRect const& Parent, LuaRect const& Child);
void    UIAreaPop();
LuaRect UIAreaGet();

void ScreenToArea(LuaPoint&);
void AreaToScreen(LuaPoint&);

void UIAreaBeginFrame(LuaRect const& ScreenRect);
void UIAreaEndFrame();

struct AreaPusher
{
    AreaPusher(LuaRect const& ParentRect, LuaRect const& ChildRect)
    {
        UIAreaPush(ParentRect, ChildRect);
    }
            
    ~AreaPusher()
    {
        UIAreaPop();
    }

private:
    AreaPusher();
    AreaPusher(AreaPusher const&);
    AreaPusher& operator=(AreaPusher const&);
};

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define UI_AREA_H
#endif /* UI_AREA_H */
