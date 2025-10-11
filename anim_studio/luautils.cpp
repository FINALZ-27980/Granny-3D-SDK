// ========================================================================
// $File: //jeffr/granny_29/statement/luautils.cpp $
// $DateTime: 2012/04/18 17:40:39 $
// $Change: 37079 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "luautils.h"

#include "statement.h"
#include "statement_editstate.h"
#include "ui_core.h"

#include "granny_parameter_checking.h"
#include "granny_cpp_settings.h"

#include "gstate_node.h"

#define SubsystemCode ApplicationLogMessage
using namespace std;
USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;

void GRANNY
LuaRect::Union(LuaRect const& Other)
{
    int32x MinX = min(x, Other.x);
    int32x MinY = min(y, Other.y);
    int32x MaxX = max(x + w, Other.x + Other.w);
    int32x MaxY = max(y + h, Other.y + Other.h);

    x = MinX;
    y = MinY;
    w = MaxX - x;
    h = MaxY - y;
}


void GRANNY
stackDump (lua_State *L)
{
    int i;
    int top = lua_gettop(L);
    for (i = 1; i <= top; i++) {  /* repeat for each level */
        int t = lua_type(L, i);
        switch (t) {
            case LUA_TSTRING:  /* strings */
                printf("`%s'", lua_tostring(L, i));
                break;
            case LUA_TBOOLEAN:  /* booleans */
                printf(lua_toboolean(L, i) ? "true" : "false");
                break;
            case LUA_TNUMBER:  /* numbers */
                printf("%g", lua_tonumber(L, i));
                break;
            case LUA_TFUNCTION:
                printf("function(%p)", lua_topointer(L, i));
                break;
            default:  /* other values */
                printf("%s", lua_typename(L, t));
                break;
        }
        printf("  ");  /* put a separator */
    }
    printf("\n");  /* end the listing */
}

int GRANNY
GetTableField(lua_State* L, const char* Name, int TablePos)
{
    lua_pushstring(L, Name);
    lua_gettable(L, TablePos-1);

    int Result = 0;
    if (lua_isnumber(L, -1))
    {
        lua_Integer LuaInt = lua_tointeger(L, -1);
        CheckConvertToInt32(Result, LuaInt, return 0);
    }
    else
    {
        // TODO: Error
    }
    lua_pop(L, 1);

    return Result;
}

bool GRANNY
GetGlobalBoolean(lua_State* L, char const* Name)
{
    lua_getglobal(L, Name);
    bool Result = !!lua_toboolean(L, -1);
    lua_pop(L, 1);

    return Result;
}

std::string GRANNY
GetGlobalString(lua_State* L, char const* Name)
{
    lua_getglobal(L, Name);
    char const* Result = lua_tostring(L, -1);
    std::string RetVal = Result;
    lua_pop(L, 1);

    return RetVal;
}


bool GRANNY
ProtectedSimpleCall(lua_State* L,
                    char const* Scope,
                    char const* Name,
                    int         NumReturnValues /* = 0 */)
{
    if (Scope == NULL)
        PushTableFunction(L, "_G", Name);
    else
        PushTableFunction(L, Scope, Name);

    if (lua_pcall(L, 0, NumReturnValues, 0))
    {
        StTMPrintfERR("Problem calling %s: %s\n",
                      StTMDynString(Name),
                      StTMDynString(lua_tostring(L, -1)));
        lua_pop(L, 1);

        return false;
    }

    return true;
}

void GRANNY
PushTableFunction(lua_State* L, char const* Table, char const* FnName)
{
    // todo: handle error
    if (Table == NULL)
    {
        lua_getglobal(L, FnName);
    }
    else
    {
        lua_getglobal(L, Table);
        lua_pushstring(L, FnName);
        lua_gettable(L, -2);
        lua_remove(L, -2);
    }
}

// Make a local id from the parent id on the stack, and leaves it as the top stack entry
void GRANNY
MakeLocalSubID(lua_State* L, int ParentIdx, char const* LocalName)
{
    PushTableFunction(L, "IDItem", "CreateSub");
    lua_pushvalue(L, ParentIdx-1);
    lua_pushstring(L, LocalName);
    if (lua_pcall(L, 2, 1, 0))
    {
        // todo: handle error
    }
}

// Make a control id from the parent id and control id on the stack, and leaves it as the
// top stack entry
void GRANNY
MakeLocalControlID(lua_State* L, int ParentIdx, int ControlIdx)
{
    // After this stack = // ... BC BC.MakeID
    PushTableFunction(L, "IDItem", "MakeID");

    lua_pushvalue(L, ParentIdx-1);
    lua_pushvalue(L, ControlIdx-2);
    if (lua_pcall(L, 2, 1, 0))
    {
        // todo: handle error
    }
}

bool GRANNY
ExtractRect(lua_State* L, int index, LuaRect& Rect)
{
    if (!lua_istable(L, index))
    {
        // todo: error
        return false;
    }

    Rect.x = GetTableField(L, "x", index);
    Rect.y = GetTableField(L, "y", index);
    Rect.w = GetTableField(L, "w", index);
    Rect.h = GetTableField(L, "h", index);

    return true;
}

bool GRANNY
ExtractPoint(lua_State* L, int index, LuaPoint& Point)
{
    if (!lua_istable(L, index))
    {
        // todo: error
        return false;
    }

    Point.x = GetTableField(L, "x", index);
    Point.y = GetTableField(L, "y", index);

    return true;
}

bool GRANNY
ExtractColor(lua_State* L, int index, LuaColor& Color)
{
    if (!lua_istable(L, index))
    {
        // todo: error
        return false;
    }

    // Remember that arrays are 1-indexed in lua
    lua_pushinteger(L, 4);
    lua_pushinteger(L, 3);
    lua_pushinteger(L, 2);
    lua_pushinteger(L, 1); // c ...stuff... 4 3 2 1

    lua_gettable(L, index - 4);   // c ...stuff... 4 3 2 r
    Color.r = (real32)lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_gettable(L, index - 3);   // c ...stuff... 4 3 2
    Color.g = (real32)lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_gettable(L, index - 2);   // c ...stuff... 4 3
    Color.b = (real32)lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_gettable(L, index - 1);   // c ...stuff... 4
    Color.a = (real32)lua_tonumber(L, -1);
    lua_pop(L, 1);

    return true;
}

bool GRANNY
ExtractStringArray(lua_State* L, int index, vector<const char*>& Strings)
{
    if (!lua_istable(L, index))
    {
        // todo: error
        return false;
    }

    Strings.clear();

    // Remember that arrays are 1-indexed in lua
    lua_pushnil(L);  /* first key */
    while (lua_next(L, index - 1) != 0)
    {
        Strings.push_back(lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    return true;
}

bool GRANNY
ExtractIntArray(lua_State* L, int index, std::vector<int32x>& Ints)
{
    Ints.clear();

    if (!lua_istable(L, index))
    {
        // todo: error
        return false;
    }

    // Remember that arrays are 1-indexed in lua
    lua_pushnil(L);  /* first key */
    while (lua_next(L, index - 1) != 0)
    {
        Ints.push_back((int32x)lua_tointeger(L, -1));
        lua_pop(L, 1);
    }

    return true;
}

bool GRANNY
PushIntArray(lua_State* L, std::vector<int32x>& Ints)
{
    lua_createtable(L, (int)Ints.size(), 0);

    {for(uint32x Idx = 0; Idx < Ints.size(); ++Idx)
    {
        lua_pushinteger(L, Idx + 1);
        lua_pushinteger(L, Ints[Idx]);
        lua_settable(L, -3);
    }}

    return true;
}


bool GRANNY
PushRect(lua_State* L, LuaRect const& Rect)
{
    lua_getglobal(L, "MakeRect");
    lua_pushinteger(L, Rect.x);
    lua_pushinteger(L, Rect.y);
    lua_pushinteger(L, Rect.w);
    lua_pushinteger(L, Rect.h);
    if (lua_pcall(L, 4, 1, 0))
    {
        StTMPrintfERR("Problem with call MakeRect: '%s'\n", StTMDynString(lua_tostring(L, -1)));
        lua_pop(L, 1);
        return false;
    }

    return true;
}

// bool
// PushRectArray(lua_State* L, std::vector<LuaRect>& Rects)
// {
//     lua_createtable(L, (int)Rects.size(), 0);

//     {for(uint32x Idx = 0; Idx < Rects.size(); ++Idx)
//     {
//         lua_pushinteger(L, Idx + 1);
//         PushRect(L, Rects[Idx]);
//         lua_settable(L, -3);
//     }}

//     return true;
// }


bool GRANNY
PushPoint(lua_State* L, LuaPoint const& Point)
{
    lua_createtable(L, 0, 2);
    lua_pushstring(L, "y");
    lua_pushstring(L, "x");

    lua_pushnumber(L, Point.x);
    lua_settable(L, -4);

    lua_pushnumber(L, Point.y);
    lua_settable(L, -3);

    return true;
}

bool GRANNY
PushColor(lua_State* L, LuaColor const& Color)
{
    lua_createtable(L, 4, 0);
    lua_pushinteger(L, 4);
    lua_pushinteger(L, 3);
    lua_pushinteger(L, 2);
    lua_pushinteger(L, 1);

    lua_pushnumber(L, Color.r);
    lua_settable(L, -6);

    lua_pushnumber(L, Color.g);
    lua_settable(L, -5);

    lua_pushnumber(L, Color.b);
    lua_settable(L, -4);

    lua_pushnumber(L, Color.a);
    lua_settable(L, -3);

    return true;
}

bool GRANNY
PushStringArray(lua_State* L, std::vector<char const*>& Strings)
{
    lua_createtable(L, (int)Strings.size(), 0);

    {for(uint32x Idx = 0; Idx < Strings.size(); ++Idx)
    {
        lua_pushinteger(L, Idx + 1);
        lua_pushstring(L, Strings[Idx]);
        lua_settable(L, -3);
    }}

    return true;
}

int GRANNY
LuaInteger(lua_State* L, int retval)
{
    lua_pushinteger(L, retval);
    return 1;
}

int GRANNY
LuaBoolean(lua_State* L, bool retval)
{
    lua_pushboolean(L, retval);
    return 1;
}

int GRANNY
LuaString(lua_State* L, char const* String)
{
    lua_pushstring(L, String);
    return 1;
}

int GRANNY
LuaNil(lua_State* L)
{
    lua_pushnil(L);
    return 1;
}

int GRANNY
LuaNode(lua_State* L, node* Node)
{
    if (Node)
        lua_pushinteger(L, edit::TokenizedToID(Node));
    else
        lua_pushnil(L);

    return 1;
}

void GRANNY
PushTokenizedClosure(tokenized* Node, tokenized_closure Fn, int extraArgs)
{
    lua_State* State = UILuaState();
    lua_pushinteger(State, edit::TokenizedToID(Node));
    lua_pushcclosure(State, Fn, extraArgs + 1);
}

void GRANNY
PushTokenizedIntClosure(tokenized* Tokenized, tokenized_closure Fn, int argument)
{
    lua_State* State = UILuaState();
    lua_pushinteger(State, argument);
    PushTokenizedClosure(Tokenized, Fn, 1);
}

