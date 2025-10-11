// ========================================================================
// $File: //jeffr/granny_29/statement/ui_editfield.cpp $
// $DateTime: 2012/05/09 10:21:42 $
// $Change: 37340 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "ui_editfield.h"

#include "rrCore.h"
#include "ui_core.h"
#include "luautils.h"
#include "ui_font.h"

#include <string>
#include <ctype.h>
#include <assert.h>

// todo: good god, cleanup

struct edit_field_state;

static void LayoutSTBRow(void* r,
                         edit_field_state* obj,
                         int i);
static float GetSTBWidth(edit_field_state* obj,
                         int n,
                         int i);
static int
STBInsertChars(edit_field_state* obj,
               int at,
               char* chars,
               int numChars);
static void
STBDeleteChars(edit_field_state* obj,
               int at,
               int nChars);
static int STBStringLen(edit_field_state*);
static char STBGetChar(edit_field_state*, int n);
static char STBKeyToText(int k);


#define STB_TEXTEDIT_CHARTYPE                    char
#define STB_TEXTEDIT_POSITIONTYPE                int
#define STB_TEXTEDIT_STRING                      edit_field_state
#define STB_TEXTEDIT_STRINGLEN(obj)              STBStringLen(obj)
#define STB_TEXTEDIT_LAYOUTROW(pEditRot, obj, i) LayoutSTBRow(pEditRot, obj, i)
#define STB_TEXTEDIT_GETWIDTH(obj,n,i)           GetSTBWidth(obj, n, i)
#define STB_TEXTEDIT_KEYTOTEXT(k)                STBKeyToText(k)
#define STB_TEXTEDIT_GETCHAR(obj,i)              STBGetChar(obj, i)
#define STB_TEXTEDIT_NEWLINE                     ('\n')

#define STB_TEXTEDIT_DELETECHARS(obj, i, n)  STBDeleteChars(obj,i,n)

#define STB_TEXTEDIT_INSERTCHARS(obj,i,c,n) STBInsertChars(obj,i,c,n)


#define STB_TEXTEDIT_K_SHIFT              (1 << 16)
#define STB_TEXTEDIT_K_KEYDOWN            (1 << 17)
#define STB_TEXTEDIT_K_CONTROL            (1 << 18)
#define STB_TEXTEDIT_K_ALT                (1 << 19)
#define STB_TEXTEDIT_K_COMMAND            (1 << 20)

#define STB_TEXTEDIT_K_DIRECTION          (1 << 21)
#define STB_TEXTEDIT_K_CONTROLKEY         (1 << 22)

#define STB_TEXTEDIT_K_MASK                     \
    (STB_TEXTEDIT_K_SHIFT      |                \
     STB_TEXTEDIT_K_KEYDOWN    |                \
     STB_TEXTEDIT_K_CONTROL    |                \
     STB_TEXTEDIT_K_ALT        |                \
     STB_TEXTEDIT_K_COMMAND    |                \
     STB_TEXTEDIT_K_DIRECTION  |                \
     STB_TEXTEDIT_K_CONTROLKEY)


#define STB_TEXTEDIT_K_DOWN               (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_DIRECTION | GRANNY Direction_KeyDown)
#define STB_TEXTEDIT_K_LEFT               (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_DIRECTION | GRANNY Direction_KeyLeft)
#define STB_TEXTEDIT_K_UP                 (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_DIRECTION | GRANNY Direction_KeyUp)
#define STB_TEXTEDIT_K_RIGHT              (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_DIRECTION | GRANNY Direction_KeyRight)

#define STB_TEXTEDIT_K_TEXTSTART          (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_DIRECTION | GRANNY Direction_KeyHome)
#define STB_TEXTEDIT_K_TEXTEND            (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_DIRECTION | GRANNY Direction_KeyEnd)
#define STB_TEXTEDIT_K_DELETE             (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_CONTROLKEY | GRANNY Control_KeyDelete )
#define STB_TEXTEDIT_K_BACKSPACE          (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_CONTROLKEY | GRANNY Control_KeyBackspace )
#define STB_TEXTEDIT_K_PGUP               (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_DIRECTION | GRANNY Direction_KeyNext)
#define STB_TEXTEDIT_K_PGDOWN             (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_DIRECTION | GRANNY Direction_KeyPrev)
#define STB_TEXTEDIT_K_LINESTART          (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_DIRECTION | GRANNY Direction_KeyHome | STB_TEXTEDIT_K_CONTROL)
#define STB_TEXTEDIT_K_LINEEND            (STB_TEXTEDIT_K_KEYDOWN | STB_TEXTEDIT_K_DIRECTION | GRANNY Direction_KeyEnd  | STB_TEXTEDIT_K_CONTROL)
#define STB_TEXTEDIT_K_SELECTALL          (STB_TEXTEDIT_K_KEYDOWN | 'A' | STB_TEXTEDIT_K_CONTROL)

#define STB_TEXTEDIT_K_UNDO               (STB_TEXTEDIT_K_KEYDOWN | 'Z' | STB_TEXTEDIT_K_CONTROL)
#define STB_TEXTEDIT_K_REDO               (STB_TEXTEDIT_K_KEYDOWN | 'Y' | STB_TEXTEDIT_K_CONTROL)

#define STB_TEXTEDIT_IMPLEMENTATION
#include "stb_textedit.h"

USING_GRANNY_NAMESPACE;
using namespace std;


struct edit_field_state
{
    std::string       CurrentString;
    STB_TexteditState EditState;

    int32x FontHandle;
    int32x FontHeight;
    int32x FontBaseline;
};



static int
STBStringLen(edit_field_state* obj)
{
    return obj->CurrentString.length();
}

static char
STBGetChar(edit_field_state* obj, int n)
{
    // asksean: ??
    if (n < int(obj->CurrentString.size()))
        return obj->CurrentString[n];

    return 0;
}

static char
STBKeyToText(int k)
{
    // Mask off keydown
    char base = (char)tolower(k & ~STB_TEXTEDIT_K_MASK);

    if (k & STB_TEXTEDIT_K_SHIFT)
        base = (char)toupper(base);

    return base;
}

static void
LayoutSTBRow(void* p,
             edit_field_state* obj,
             int i)
{
    StbTexteditRow* pRow = (StbTexteditRow*)p;

    int32x StrWidth, StrHeight;
    UIGetTextDimension(obj->FontHandle,
                       &obj->CurrentString[i], 0,
                       StrWidth, StrHeight);

    pRow->x0               = (float)0;
    pRow->x1               = (float)StrWidth;
    pRow->baseline_y_delta = (float)obj->FontHeight;
    pRow->ymax             = (float)obj->FontBaseline;
    pRow->ymin             = (float)obj->FontHeight - obj->FontBaseline;
    pRow->num_chars        = obj->CurrentString.size() - i;
}

static int
SubStrWidth(edit_field_state* obj,
            int begin,
            int end)
{
    std::string sub;
    sub.insert(sub.begin(), obj->CurrentString.begin() + begin, obj->CurrentString.begin() + (begin+end));

    int32x Width, Ignore;
    UIGetTextDimension(obj->FontHandle, sub.c_str(), 0, Width, Ignore);
    return Width;
}

static float
GetSTBWidth(edit_field_state* obj,
            int n,
            int i)
{
    int32x FirstWidth = 0;
    if (i != n)
        FirstWidth = SubStrWidth(obj, n, i);

    int32x SecondWidth = SubStrWidth(obj, n, i+1);

    return float(SecondWidth - FirstWidth);
}

static void
STBDeleteChars(edit_field_state* obj,
               int at,
               int nChars)
{
    obj->CurrentString.erase(obj->CurrentString.begin() + at,
                             obj->CurrentString.begin() + (at + nChars));
}


static int
STBInsertChars(edit_field_state* obj,
               int at,
               char* chars,
               int numChars)
{
    // always succeed
    obj->CurrentString.insert(obj->CurrentString.begin() + at, chars, chars + numChars);
    return 1;
}

static int
STB_MouseDown(lua_State* L)
{
    LuaPoint pt;
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -2);
    if (!ExtractPoint(L, -1, pt) || pState == 0)
    {
        assert(!"shouldn't happen, bad args");
        return 0;
    }

    stb_textedit_click(pState, &pState->EditState, (float)pt.x, (float)pt.y);
    return 0;
}

static int
STB_MouseDrag(lua_State* L)
{
    LuaPoint pt;
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -2);
    if (!ExtractPoint(L, -1, pt) || pState == 0)
    {
        assert(!"shouldn't happen, bad args");
        return 0;
    }

    stb_textedit_drag(pState, &pState->EditState, (float)pt.x, (float)pt.y);
    return 0;
}

static int
ReleaseSTBTextEdit(lua_State* L)
{
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -2);
    if (!pState)
        return 0;

    // Release the string.
    pState->CurrentString.~string();

    // No other action required, stb_textedit doesn't need to be cleaned up, and Lua will
    // free the userdata block...
    return 0;
}

//   STB_DirectionKey(Item.STBEdit, Event.Key, Event.Shift, Event.Ctrl)
static int
STB_DirectionKey(lua_State* L)
{
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -4);
    if (!pState)
        return 0;

    int32x Key   = lua_tointeger(L, -3);
    bool   Shift = !!lua_toboolean(L, -2);
    bool   Ctrl  = !!lua_toboolean(L, -1);

    stb_textedit_key(pState, &pState->EditState,
                     (STB_TEXTEDIT_K_KEYDOWN               |
                      STB_TEXTEDIT_K_DIRECTION             |
                      (Shift ? STB_TEXTEDIT_K_SHIFT : 0)   |
                      (Ctrl  ? STB_TEXTEDIT_K_CONTROL : 0) |
                      Key));
    return 0;
}


// STB_EditKey(Item.STBEdit, Event.Key, Event.Shift, Event.Ctrl, AllowNonPrint)
static int
STB_EditKey(lua_State* L)
{
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -5);
    if (!pState)
        return 0;

    int32x Key           = lua_tointeger(L, -4);
    bool   Shift         = !!lua_toboolean(L, -3);
    bool   Ctrl          = !!lua_toboolean(L, -2);
    bool   AllowNonPrint  = !!lua_toboolean(L, -1);

    if (isprint(Key) || AllowNonPrint)
    {
        stb_textedit_key(pState, &pState->EditState,
                         (STB_TEXTEDIT_K_KEYDOWN               |
                          (Shift ? STB_TEXTEDIT_K_SHIFT : 0)   |
                          (Ctrl  ? STB_TEXTEDIT_K_CONTROL : 0) |
                          Key));
    }

    return 0;
}

static int
STB_SelectAllPosEnd(lua_State* L)
{
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -1);
    if (!pState)
        return 0;

    // Just use the keys for this.  "Home" followed by "Shift-End"
    stb_textedit_key(pState, &pState->EditState,
                     (STB_TEXTEDIT_K_KEYDOWN |
                      STB_TEXTEDIT_K_TEXTSTART));
    stb_textedit_key(pState, &pState->EditState,
                     (STB_TEXTEDIT_K_KEYDOWN |
                      STB_TEXTEDIT_K_SHIFT   |
                      STB_TEXTEDIT_K_TEXTEND));
    return 0;
}

static int
STB_SetText(lua_State* L)
{
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -2);
    if (!pState)
        return 0;

    char const* Text = lua_tostring(L, -1);
    if (!Text)
        return 0;

    stb_textedit_clear_state(&pState->EditState, 1);
    pState->CurrentString = Text;
    stb_textedit_key(pState, &pState->EditState,
                     (STB_TEXTEDIT_K_KEYDOWN |
                      STB_TEXTEDIT_K_TEXTEND));


    return 0;
}

// STB_EditCtrlKey(Item.STBEdit, Event.Key, Event.Shift, Event.Ctrl)
static int
STB_EditCtrlKey(lua_State* L)
{
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -4);
    if (!pState)
        return 0;

    int32x Key   = lua_tointeger(L, -3);
    bool   Shift = !!lua_toboolean(L, -2);
    bool   Ctrl  = !!lua_toboolean(L, -1);

    stb_textedit_key(pState, &pState->EditState,
                     (STB_TEXTEDIT_K_KEYDOWN               |
                      STB_TEXTEDIT_K_CONTROLKEY            |
                      (Shift ? STB_TEXTEDIT_K_SHIFT : 0)   |
                      (Ctrl  ? STB_TEXTEDIT_K_CONTROL : 0) |
                      Key));
    return 0;
}


static int
AllocSTBTextEdit(lua_State* L)
{
    int32x FontHandle = lua_tointeger(L, -1);
    char const* InitString = lua_tostring(L, -2);
    lua_pop(L, 2);

    void* p = lua_newuserdata(L, sizeof(edit_field_state));

    edit_field_state* pState = (edit_field_state*)p;

    // Setup the basics...
    {
        new (&pState->CurrentString) std::string;
        stb_textedit_clear_state(&pState->EditState, 1);

        pState->CurrentString = InitString ? InitString : "";
        pState->FontHandle    = FontHandle;
        UIGetFontHeightAndBaseline(FontHandle, pState->FontHeight, pState->FontBaseline);
    }

    lua_createtable(L, 0, 1);
    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, ReleaseSTBTextEdit);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);

    return 1;
}


static int
STB_GetSelectedText(lua_State* L)
{
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -1);
    if (!pState)
        return 0;

    if (STB_TEXT_HAS_SELECTION(&pState->EditState))
    {
        lua_pushstring(L, pState->CurrentString.substr(pState->EditState.select_start,
                                                       (pState->EditState.select_end -
                                                        pState->EditState.select_start)).c_str());
    }
    else
    {
        lua_pushnil(L);
    }

    return 1;
}


// local Text, SelStart, SelEnd = STB_GetTextParams(Item.STBEdit)
static int
STB_GetTextParams(lua_State* L)
{
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -1);
    if (!pState)
        return 0;

    lua_pushstring(L, pState->CurrentString.c_str());

    if (pState->EditState.select_start != pState->EditState.select_end)
    {
        int32x SelStart = SubStrWidth(pState, 0, pState->EditState.select_start);
        int32x SelEnd   = SubStrWidth(pState, 0, pState->EditState.select_end);
        lua_pushinteger(L, min(SelStart, SelEnd));
        lua_pushinteger(L, max(SelStart, SelEnd));
    }
    else
    {
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
    }

    int32x Cursor   = SubStrWidth(pState, 0, pState->EditState.cursor);
    lua_pushinteger(L, Cursor);

    return 4;
}

static int
STB_Cut(lua_State* L)
{
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -1);
    if (!pState)
        return 0;

    stb_textedit_cut(pState, &pState->EditState);
    return 0;
}

static int
STB_Paste(lua_State* L)
{
    edit_field_state* pState = (edit_field_state*)lua_touserdata(L, -2);
    if (!pState)
        return 0;

    char const* String = lua_tostring(L, -1);
    stb_textedit_paste(pState, &pState->EditState, (char*)String, strlen(String));

    return 0;
}



//static int stb_textedit_cut(STB_TEXTEDIT_STRING *str, STB_TexteditState *state)
//static void stb_textedit_key(STB_TEXTEDIT_STRING *str, STB_TexteditState *state, int key)
//static void stb_textedit_clear_state(STB_TexteditState *state)




bool GRANNY
UIEditField_Register(lua_State* State)
{
    lua_register(State, "STB_MouseDown", STB_MouseDown);
    lua_register(State, "STB_MouseDrag", STB_MouseDrag);

    lua_register(State, "AllocSTBTextEdit", AllocSTBTextEdit);
    lua_register(State, "ReleaseSTBTextEdit", ReleaseSTBTextEdit);
    lua_register(State, "STB_GetTextParams", STB_GetTextParams);
    lua_register(State, "STB_DirectionKey", STB_DirectionKey);
    lua_register(State, "STB_EditKey", STB_EditKey);
    lua_register(State, "STB_EditCtrlKey", STB_EditCtrlKey);

    lua_register(State, "STB_SelectAllPosEnd", STB_SelectAllPosEnd);
    lua_register(State, "STB_SetText", STB_SetText);

    lua_register(State, "STB_GetSelectedText", STB_GetSelectedText);
    lua_register(State, "STB_Cut", STB_Cut);
    lua_register(State, "STB_Paste", STB_Paste);

    return true;
}
