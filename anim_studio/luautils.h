#if !defined(LUAUTILS_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/statement/luautils.h $
// $DateTime: 2012/03/21 11:09:06 $
// $Change: 36825 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#include <lua.hpp>
#include <vector>
#include <string>

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

#if !defined(GSTATE_BASE_H)
#include "gstate_base.h"
#endif


extern "C"
{
#include <lauxlib.h>
#include <lualib.h>
}

// For working with node/tokenized closures
BEGIN_GSTATE_NAMESPACE;
class tokenized;
class node;
END_GSTATE_NAMESPACE;
typedef int tokenized_closure(lua_State*);


BEGIN_GRANNY_NAMESPACE;

// sb float?
struct LuaPoint
{
    int32 x;
    int32 y;

    LuaPoint() : x(0), y(0) { }
    LuaPoint(int32x in_x, int32x in_y) : x(in_x), y(in_y) { }
};

struct LuaRect
{
    int32 x;
    int32 y;
    int32 w;
    int32 h;

    LuaRect() : x(0), y(0), w(0), h(0) { }
    LuaRect(int32x in_x, int32x in_y, int32x in_w, int32x in_h) :
      x(in_x), y(in_y), w(in_w), h(in_h) { }

    bool Contains(int testX, int testY) const {
        return ((testX >= x && testX < (x + w)) &&
                (testY >= y && testY < (y + h)));
    };
    bool Contains(LuaPoint const& pt) const { return Contains(pt.x, pt.y); }

    void Union(LuaRect const& Other);
};

struct LuaColor
{
    real32 r;
    real32 g;
    real32 b;
    real32 a;

    LuaColor() : r(0), g(0), b(0), a(1) { }
    LuaColor(real32 in_r, real32 in_g, real32 in_b) : r(in_r), g(in_g), b(in_b), a(1) { }
    LuaColor(real32 in_r, real32 in_g, real32 in_b, real32 in_a) : r(in_r), g(in_g), b(in_b), a(in_a) { }

    LuaColor operator*(float Scale) const
    {
        return LuaColor(r * Scale,
                        g * Scale,
                        b * Scale,
                        a);
    }
};


void stackDump (lua_State *L);
int  GetTableField(lua_State* L, const char* Name, int TablePos);

bool GetGlobalBoolean(lua_State* L, char const* Name);
std::string GetGlobalString(lua_State* L, char const* Name);

bool ProtectedSimpleCall(lua_State* L, char const* Scope, char const* Name, int NumReturnValues = 0);
void PushTableFunction(lua_State* L, char const* Table, char const* FnName);

void MakeLocalSubID(lua_State* L, int ParentIdx, char const* LocalName);
void MakeLocalControlID(lua_State* L, int ParentIdx, int ControlIndex);

// void MakeSimpleAutoPlacer(lua_State* L, bool IsHorizontal, int32 Spacing);
// void MakeAutoPlacer(lua_State* L, bool IsHorizontal, int32 Spacing,
//                     int32x X, int32x Y, int32x W, int32x H);

// void NextAutoplacerRect(lua_State* L, int PlacerIndex, int w, int h,
//                         int& OutX, int& OutY, int& OutW, int& OutH);
// void CurrentAutoplacerCursor(lua_State* L, int PlacerIndex, int& OutX, int& OutY);
// void PushAutoplacerState(lua_State* L, int PlacerIndex);
// void PopAutoplacerState(lua_State* L, int PlacerIndex);
// void DiscardAutoplacerState(lua_State* L, int PlacerIndex);

bool ExtractColor(lua_State* L, int index, LuaColor& Color);
bool ExtractIntArray(lua_State* L, int index, std::vector<int32x>& Ints);
bool ExtractPoint(lua_State* L, int index, LuaPoint& Point);
bool ExtractRect(lua_State* L, int index, LuaRect& Rect);
bool ExtractStringArray(lua_State* L, int index, std::vector<const char*>& Strings);

bool PushColor(lua_State* L, LuaColor const& Color);
bool PushIntArray(lua_State* L, std::vector<int32x>& Ints);
bool PushPoint(lua_State* L, LuaPoint const& Point);
bool PushRect(lua_State* L, LuaRect const& Rect);
bool PushStringArray(lua_State* L, std::vector<char const*>&);

// For return clauses
int LuaInteger(lua_State* L, int retval);
int LuaBoolean(lua_State* L, bool retval);
int LuaString(lua_State* L, char const* String);
int LuaNil(lua_State* L);
int LuaNode(lua_State*, GSTATE node*);

// For working with node/tokenized closures
void PushTokenizedClosure(GSTATE tokenized* Tokenized, tokenized_closure Fn, int extraArgs);
void PushTokenizedIntClosure(GSTATE tokenized* Tokenized, tokenized_closure Fn, int argument);




END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define LUAUTILS_H
#endif
