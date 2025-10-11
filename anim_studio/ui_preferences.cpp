// ========================================================================
// $File: //jeffr/granny_29/statement/ui_preferences.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "ui_preferences.h"
#include "ui_core.h"
#include "granny_assert.h"
#include "luautils.h"

USING_GRANNY_NAMESPACE;

int32x GRANNY
GetPreferenceFont(const char* Fontkey)
{
    lua_State* L = UILuaState();
    Assert(L);

    lua_getglobal(L, "PreferenceFonts");
    lua_pushstring(L, Fontkey);
    lua_gettable(L, -2);

    int FontHandle = lua_tointeger(L, -1);
    lua_pop(L, 2);

    return FontHandle;
}

LuaColor GRANNY
GetPreferenceColor(const char* Colorkey)
{
    lua_State* L = UILuaState();
    Assert(L);

    lua_getglobal(L, "PreferenceColors");
    lua_pushstring(L, Colorkey);
    lua_gettable(L, -2);

    LuaColor RetVal;
    ExtractColor(L, -1, RetVal);
    lua_pop(L, 2);

    return RetVal;
}

int32x GRANNY
GetPreferenceBitmap(const char* Filename)
{
    lua_State* L = UILuaState();
    Assert(L);

    lua_getglobal(L, "PreferenceBitmaps");
    lua_pushstring(L, Filename);
    lua_gettable(L, -2);

    int BitmapHandle = lua_tointeger(L, -1);
    lua_pop(L, 2);

    return BitmapHandle;
}

