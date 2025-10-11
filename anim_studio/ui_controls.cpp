// ========================================================================
// $File: //jeffr/granny_29/statement/ui_controls.cpp $
// $DateTime: 2012/07/20 17:10:16 $
// $Change: 38457 $
// $Revision: #11 $
//
// $Notice: $
// ========================================================================
#include "ui_controls.h"

#include "statement_editstate.h"
#include "statement_undostack.h"
#include "ui_character_render.h"

#include "granny_assert.h"
#include "granny_camera.h"
#include "granny_model.h"
#include "granny_skeleton.h"
#include "granny_track_mask.h"
#include "gstate_character_instance.h"
#include "gstate_mask_source.h"
#include "gstate_aimat_ik.h"
#include "ui_area.h"
#include "ui_core.h"
#include "ui_drawing.h"
#include "ui_font.h"
#include "ui_preferences.h"

#include "luautils.h"

#include <windows.h>
#include <gl/gl.h>

USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;
using namespace std;

static int ControlHPadding = 5;
static int ControlVPadding = 5;


static LuaPoint
Justify(LuaPoint const& pt,
        int32x          Width,
        Justification   just)
{
    switch (just)
    {
        case eLeft:   return pt;
        case eRight:  return LuaPoint(pt.x - Width, pt.y);
        case eCenter: return LuaPoint(pt.x - (Width / 2), pt.y);
    }

    InvalidCodePath("should not reach here, invalid justification");
    return pt;
}


static int32x
LabelWidthImpl(const char* String, int Handle)
{
    if (!String)
        return 0;

    int32x w,h;
    UIGetTextDimension(Handle, String, 0, w, h);

    return w;
}


GRANNY
ControlIDGen::ControlIDGen(char const* IDFmt, ...)
  : CurrID(1)
{
    va_list ap;
    va_start(ap, IDFmt);
    vsnprintf(ParentID, sizeof(ParentID), IDFmt, ap);
    va_end(ap);
}

void GRANNY
ControlIDGen::ConsumeNextID()
{
    ++CurrID;
}

void GRANNY
ControlIDGen::PushNextID()
{
    lua_State* L = UILuaState();

    PushTableFunction(L, "IDItem", "MakeID");
    lua_pushstring(L,  ParentID);
    lua_pushinteger(L, CurrID);
    if (lua_pcall(L, 2, 1, 0))
    {
        // todo: handle error
    }

    ConsumeNextID();
}


// =============================================================================
// ========== Layout assistance         
static LuaRect s_LastControlRect(0, 0, 0, 0);

LuaRect const& GRANNY
LastControlRect()
{
    return s_LastControlRect;
}

static void
SetLastControlRect(LuaRect const& Rect)
{
    s_LastControlRect = Rect;
}

void GRANNY
AdvanceY(int32x& CurrY, int32x By /* = 0*/)
{
    CurrY += By + ControlVPadding;
}

void GRANNY
AdvanceYRect(int32x& CurrY, LuaRect const& Rect)
{
    AdvanceY(CurrY, Rect.h);
}

void GRANNY
AdvanceYLastControl(int32x& CurrY)
{
    AdvanceY(CurrY, LastControlRect().h);
}

void GRANNY
AdvanceX(int32x& CurrX, int32x By /*= 0*/)
{
    CurrX += By + ControlHPadding;
}

void GRANNY
AdvanceXRect(int32x& CurrX, LuaRect const& Rect)
{
    AdvanceX(CurrX, Rect.w);
}



// =============================================================================
//  Controls...
// =============================================================================

// ========== Draws a separation line
void GRANNY
SeparatorAt(int32x YVal, LuaRect const& drawArea)
{
    // Draw a vertical separator, and return it's height
    LuaRect sepRect = LuaRect(drawArea.x + ControlHPadding, YVal, drawArea.w - 2*ControlHPadding, 2);
    DrawRect(sepRect, LuaColor(0.85f, 0.85f, 0.85f));
    SetLastControlRect(sepRect);
}

// ========== Draws a separation header
void GRANNY
SectionHeaderAt(char const* Header,
                int32x OldYVal,
                LuaRect const& drawArea)
{
    int32x YVal = OldYVal;

    SeparatorAt(YVal, drawArea);
    AdvanceYLastControl(YVal);

    // @@todo really need to cache this off somewhere
    int h, bl;
    int handle = GetPreferenceFont("label_text");
    UIGetFontHeightAndBaseline(handle, h, bl);

    LuaRect labelRect = LuaRect(drawArea.x + ControlHPadding,
                                YVal - ControlVPadding,
                                drawArea.w - 2*ControlHPadding,
                                h + 2*ControlVPadding);
    DrawRect(labelRect, GetPreferenceColor("section_header"));
    
    LabelAt(Header, LuaPoint(CenterX(drawArea), YVal), eCenter);
    AdvanceYLastControl(YVal);

    SeparatorAt(YVal, drawArea);
    AdvanceY(YVal, LastControlRect().h - ControlVPadding);

    SetLastControlRect(LuaRect(labelRect.x, OldYVal, labelRect.w, YVal - OldYVal));
}


// ========== Labels
static void
ColorLabelImpl(char const*     String,
               LuaPoint const& pt,
               LuaColor const& color,
               int             FontHandle,
               Justification   just)
{
    if (!String)
        return;

    int Width         = LabelWidthImpl(String, FontHandle);
    LuaPoint renderPt = Justify(pt, Width, just);

    int h, bl;
    UIGetFontHeightAndBaseline(FontHandle, h, bl);

    RenderText(FontHandle, String, 0, renderPt.x, renderPt.y + bl, color);
    SetLastControlRect(LuaRect(renderPt.x, renderPt.y, Width, h));
}

void GRANNY
ColorLabelAt(char const*     String,
             LuaPoint const& pt,
             LuaColor const& color,
             Justification   just)
{
    int handle = GetPreferenceFont("label_text");
    ColorLabelImpl(String, pt, color, handle, just);
}

void GRANNY
LabelAt(char const* String,
        LuaPoint const& pt,
        Justification just)
{
    ColorLabelAt(String, pt, LuaColor(1, 1, 1), just);
}

void GRANNY
SmallLabelAt(char const* String,
             LuaPoint const& pt,
             Justification just)
{
    int handle = GetPreferenceFont("small_label_text");
    ColorLabelImpl(String, pt, LuaColor(1, 1, 1), handle, just);
}


// ========== Edit boxes
bool GRANNY
EditLabelOn(std::string& Label,
            LuaRect const& Rect,
            ControlIDGen& IDGen)
{
    lua_State* L = UILuaState();

    PushTableFunction(L, "Editbox", "Do");
    IDGen.PushNextID();
    PushRect(L, Rect);
    lua_pushstring(L, Label.c_str());
    lua_pushinteger(L, GetPreferenceFont("label_text"));
    lua_pushboolean(L, true);
    lua_pcall(L, 5, 2, 0);

    char const* NewString = lua_tostring(L, -2);
    bool const  Changed = !!lua_toboolean(L, -1);

    // the return values...
    lua_pop(L, 2);

    if (Changed)
        Label = NewString;

    SetLastControlRect(Rect);
    return Changed;
}

void GRANNY
EditLabelPlaceholder(std::string& Label,
                     LuaRect const& Rect,
                     ControlIDGen& IDGen)
{
    lua_State* L = UILuaState();

    IDGen.ConsumeNextID();
    
    PushTableFunction(L, "Editbox", "DrawPlaceholder");
    PushRect(L, Rect);
    lua_pushstring(L, Label.c_str());
    lua_pushinteger(L, GetPreferenceFont("label_text"));
    lua_pushboolean(L, true);
    lua_pcall(L, 4, 0, 0);

    SetLastControlRect(Rect);
}



// ========== Checkbox & small checkbox
static LuaRect
CheckboxDimGen(char const* Text,
               char const* DimFun)
{
    lua_State* L = UILuaState();
    PushTableFunction(L, "Checkbox", DimFun);
    lua_pushstring(L, Text);

    lua_pcall(L, 1, 2, 0);

    LuaRect ret(0, 0, lua_tointeger(L, -2), lua_tointeger(L, -1));
    lua_pop(L, 2);

    return ret;
}

static bool
CheckboxOnGen(bool& Value,
              char const* Text,
              LuaRect const& Rect,
              ControlIDGen& IDGen,
              char const* DoFun)
{
    lua_State* L = UILuaState();

    //CheckBoxDo(ID, Rect, Label, State)
    PushTableFunction(L, "Checkbox", DoFun);
    IDGen.PushNextID();
    PushRect(L, Rect);
    lua_pushstring(L, Text);
    lua_pushboolean(L, Value);
    lua_pcall(L, 4, 2, 0);

    bool const Changed = !!lua_toboolean(L, -2);
    Value = !!lua_toboolean(L, -1);

    // the return values...
    lua_pop(L, 2);

    SetLastControlRect(Rect);
    return Changed;
}


LuaRect GRANNY
CheckboxDim(char const* Text)
{
    return CheckboxDimGen(Text, "Dim");
}

bool GRANNY
CheckboxOn(bool& Value,
           char const* Text,
           LuaRect const& Rect,
           ControlIDGen& IDGen)
{
    return CheckboxOnGen(Value, Text, Rect, IDGen, "Do");
}

LuaRect GRANNY
SmallCheckboxDim(char const* Text)
{
    return CheckboxDimGen(Text, "DimSmall");
}

bool GRANNY
SmallCheckboxOn(bool& Value,
                char const* Text,
                LuaRect const& Rect,
                ControlIDGen& IDGen)
{
    return CheckboxOnGen(Value, Text, Rect, IDGen, "DoSmall");
}



// ========== Numerical slider
int GRANNY
SliderHeight()
{
    return 18;
}

int GRANNY
SmallSliderHeight()
{
    return 16;
}

static bool
SliderOnInternal(real32 Min,
                 real32 Max,
                 real32& Value,
                 bool ClampToInts,
                 LuaRect const& Rect,
                 ControlIDGen& IDGen,
                 bool Small)
{
    lua_State* L = UILuaState();

    LuaRect client = Rect;
    client.x = client.y = 0;

    bool Changed = false;
    if (UIAreaPush(Rect, client))
    {
        if (Small)
            PushTableFunction(L, "Slider", "DoSmall");
        else
            PushTableFunction(L, "Slider", "Do");

        IDGen.PushNextID();
        lua_pushnumber(L, Value);
        lua_pushnumber(L, Min);
        lua_pushnumber(L, Max);
        lua_pushboolean(L, ClampToInts);

        // Stack: Slider.Do, ControlID, Current, Min, Max, IntClamp
        lua_pcall(L, 5, 2, 0);

        // Stack: ParentID, FrameID, AutoPlacer, ControlID, Rect, Changed, NewStart, NewEnd, NewCurrent
        //stackDump(L);
        real32 NewValue = (real32)lua_tonumber(L, -2);
        Changed = !!lua_toboolean(L, -1);
        if (Changed)
            Value = NewValue;

        lua_pop(L, 2);  // ret val
    }
    else
    {
        IDGen.ConsumeNextID();
    }
    UIAreaPop();

    SetLastControlRect(Rect);
    return Changed;
}

bool GRANNY
SliderOn(real32 Min,
         real32 Max,
         real32& Value,
         bool ClampToInts,
         LuaRect const& Rect,
         ControlIDGen& IDGen)
{
    return SliderOnInternal(Min, Max, Value, ClampToInts, Rect, IDGen, false);
}

bool GRANNY
SmallSliderOn(real32 Min,
              real32 Max,
              real32& Value,
              bool ClampToInts,
              LuaRect const& Rect,
              ControlIDGen& IDGen)
{
    return SliderOnInternal(Min, Max, Value, ClampToInts, Rect, IDGen, true);
}



// ========== Combobox
void GRANNY
ComboboxPlaceholder(char const* Label,  // may be null
                    LuaRect const& Rect,
                    ControlIDGen& IDGen)
{
    lua_State* L = UILuaState();

    IDGen.ConsumeNextID();

    PushTableFunction(L, "ITunesBox", "DrawPlaceholder");
    PushRect(L, Rect);
    lua_pushstring(L, Label);
    lua_pcall(L, 2, 0, 0);

    SetLastControlRect(Rect);
}

void GRANNY
SmallComboboxPlaceholder(char const* Label,  // may be null
                         LuaRect const& Rect,
                         ControlIDGen& IDGen)
{
    lua_State* L = UILuaState();

    IDGen.ConsumeNextID();

    PushTableFunction(L, "Combobox", "DrawPlaceholderSmall");
    PushRect(L, Rect);
    lua_pushstring(L, Label);
    lua_pcall(L, 2, 0, 0);

    SetLastControlRect(Rect);
}

bool GRANNY
ComboboxOn(vector<char const*>& Strings,
           int32x& Selection,
           LuaRect const& Rect,
           ControlIDGen& IDGen)
{
    if (Strings.empty())
    {
        // Sets the control rect for us
        ComboboxPlaceholder(0, Rect, IDGen);
        return false;
    }
   
    lua_State* L = UILuaState();

    PushTableFunction(L, "Combobox", "Do");
    IDGen.PushNextID();
    PushRect(L, Rect);
    PushStringArray(L, Strings);
    lua_pushinteger(L, Selection + 1);  // account for 1 indexing
    lua_pcall(L, 4, 2, 0);

    // return stack: Changed, Idx
    bool Changed = !!lua_toboolean(L, -1);
    if (Changed)
        Selection = ((int32x)lua_tointeger(L, -2)) - 1;

    // the return values...
    lua_pop(L, 2);

    SetLastControlRect(Rect);
    return Changed;
}


bool GRANNY
SmallComboboxOn(vector<char const*>& Strings,
                int32x& Selection,
                LuaRect const& Rect,
                ControlIDGen& IDGen)
{
    if (Strings.empty())
    {
        // Sets the control rect for us
        SmallComboboxPlaceholder(0, Rect, IDGen);
        return false;
    }
   
    lua_State* L = UILuaState();

    PushTableFunction(L, "Combobox", "DoSmall");
    IDGen.PushNextID();
    PushRect(L, Rect);
    PushStringArray(L, Strings);
    lua_pushinteger(L, Selection + 1);  // account for 1 indexing
    lua_pcall(L, 4, 2, 0);

    // return stack: Changed, Idx
    bool Changed = !!lua_toboolean(L, -1);
    if (Changed)
        Selection = ((int32x)lua_tointeger(L, -2)) - 1;

    // the return values...
    lua_pop(L, 2);

    SetLastControlRect(Rect);
    return Changed;
}


// ========== Itunes version
bool GRANNY
ITunesComboboxOn(vector<char const*>& Strings,
                 int32x& Selection,
                 LuaRect const& Rect,
                 ControlIDGen& IDGen)
{
    if (Strings.empty())
    {
        ComboboxPlaceholder(0, Rect, IDGen);
        return false;
    }
   
    lua_State* L = UILuaState();

    PushTableFunction(L, "ITunesBox", "Do");
    IDGen.PushNextID();
    PushRect(L, Rect);
    PushStringArray(L, Strings);
    lua_pushinteger(L, Selection + 1);  // account for 1 indexing
    lua_pcall(L, 4, 2, 0);

    // return stack: Changed, Idx
    bool Changed = !!lua_toboolean(L, -1);
    if (Changed)
        Selection = ((int32x)lua_tointeger(L, -2)) - 1;

    // the return values...
    lua_pop(L, 2);

    SetLastControlRect(Rect);
    return Changed;
}


// ========== Buttons
bool GRANNY
ButtonAt(char const* Label,
         LuaPoint const& At,
         bool Enabled,
         ControlIDGen& IDGen)
{
    lua_State* L = UILuaState();
    lua_pushnil(L);
    return ButtonAtWithCallback(Label, At, Enabled, IDGen);
}

bool GRANNY
ButtonAtWithCallback(char const* Label,
                     LuaPoint const& At,
                     bool Enabled,
                     ControlIDGen& IDGen)
{
    lua_State* L = UILuaState();

    PushTableFunction(L, "Button", "Dim");
    lua_pushstring(L, Label);
    lua_pcall(L, 1, 2, 0);

    LuaRect RectUsed(At.x, At.y,
                     lua_tointeger(L, -2),
                     lua_tointeger(L, -1));
    SetLastControlRect(RectUsed);

    lua_pop(L, 2);

    PushTableFunction(L, "Button", "Do");
    IDGen.PushNextID();
    PushRect(L, RectUsed);
    lua_pushstring(L, Label);
    lua_pushboolean(L, !Enabled);

    // disabled, label, rect, id, function, controlid, idstring, ... fn()
    lua_pushvalue(L, -1 - 5);

    lua_pcall(L, 5, 1, 0);

    bool Clicked = !!lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_pop(L, 1);  // pull off the function

    return Clicked;
}


bool GRANNY
SmallButtonAtRectWithCallback(char const* Label,
                              LuaRect const& Rect,
                              bool Enabled,
                              ControlIDGen& IDGen)
{
    lua_State* L = UILuaState();

    PushTableFunction(L, "Button", "DoSmall");
    IDGen.PushNextID();
    PushRect(L, Rect);
    lua_pushstring(L, Label);
    lua_pushboolean(L, !Enabled);

    // label, rect, id, function, controlid, idstring, ... fn()
    lua_pushvalue(L, -1 - 5);

    lua_pcall(L, 5, 1, 0);

    bool Clicked = !!lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_pop(L, 1);  // pull off the function

    SetLastControlRect(Rect);
    return Clicked;
}

bool GRANNY
SmallButtonAt(char const* Label,
              LuaPoint const& At,
              bool Enabled,
              ControlIDGen& IDGen)
{
    lua_State* L = UILuaState();
    lua_pushnil(L);
    return SmallButtonAtWithCallback(Label, At, Enabled, IDGen);
}


bool GRANNY
SmallButtonAtWithCallback(char const* Label,
                          LuaPoint const& At,
                          bool Enabled,
                          ControlIDGen& IDGen)
{
    lua_State* L = UILuaState();

    PushTableFunction(L, "Button", "DimSmall");
    lua_pushstring(L, Label);
    lua_pcall(L, 1, 2, 0);

    LuaRect RectUsed;
    RectUsed.x = At.x;
    RectUsed.y = At.y;
    RectUsed.w = lua_tointeger(L, -2);
    RectUsed.h = lua_tointeger(L, -1);

    lua_pop(L, 2);

    return SmallButtonAtRectWithCallback(Label, RectUsed, Enabled, IDGen);
}

bool GRANNY
BitmapButtonAt(LuaPoint const& At,
               int BitmapHandle,
               ControlIDGen& IDGen)
{
    lua_State* L = UILuaState();
    lua_pushnil(L);
    return BitmapButtonAtWithCallback(At, BitmapHandle, IDGen);
}


LuaRect GRANNY
BitmapButtonDim(int BitmapHandle)
{
    lua_State* L = UILuaState();

    PushTableFunction(L, "Button", "BitmapDim");
    lua_pushinteger(L, BitmapHandle);
    lua_pcall(L, 1, 2, 0);

    LuaRect RectUsed(0, 0, 0, 0);
    RectUsed.w = lua_tointeger(L, -2);
    RectUsed.h = lua_tointeger(L, -1);
    lua_pop(L, 2);

    return RectUsed;
}

bool GRANNY
BitmapButtonAtWithCallback(LuaPoint const& At,
                           int BitmapHandle,
                           ControlIDGen& IDGen)
{
    lua_State* L = UILuaState();

    LuaRect RectUsed = BitmapButtonDim(BitmapHandle);
    RectUsed.x = At.x;
    RectUsed.y = At.y;
    SetLastControlRect(RectUsed);

    PushTableFunction(L, "Button", "DoBitmap");
    IDGen.PushNextID();
    PushRect(L, RectUsed);
    lua_pushinteger(L, BitmapHandle);

    // rect, id, function, controlid, idstring, ... fn()
    lua_pushvalue(L, -1 - 4);

    lua_pcall(L, 4, 1, 0);

    bool Clicked = !!lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_pop(L, 1);  // pull off the function

    return Clicked;
}

   // return w + 28, 26
   // return w + 20, 22
int GRANNY
ButtonHeight()
{
    // Keep synced with ui_button.lua
    static int s_ButtonHeight = -1;
    if (s_ButtonHeight == -1)
    {
        lua_State* L = UILuaState();

        PushTableFunction(L, "Button", "Dim");
        lua_pushstring(L, "empty");
        lua_pcall(L, 1, 2, 0);

        s_ButtonHeight = lua_tointeger(L, -1);
    }

    return s_ButtonHeight;
}

int GRANNY
SmallButtonHeight()
{
    // Keep synced with ui_button.lua
    static int s_ButtonHeight = -1;
    if (s_ButtonHeight == -1)
    {
        lua_State* L = UILuaState();

        PushTableFunction(L, "Button", "DimSmall");
        lua_pushstring(L, "empty");
        lua_pcall(L, 1, 2, 0);

        s_ButtonHeight = lua_tointeger(L, -1);
    }

    return s_ButtonHeight;
}

int GRANNY
ButtonWidth(char const* Label)
{
    lua_State* L = UILuaState();

    PushTableFunction(L, "Button", "Dim");
    lua_pushstring(L, Label);
    lua_pcall(L, 1, 2, 0);

    int Width = lua_tointeger(L, -2);
    lua_pop(L, 2);

    return Width;
}

int GRANNY
SmallButtonWidth(char const* Label)
{
    lua_State* L = UILuaState();

    PushTableFunction(L, "Button", "DimSmall");
    lua_pushstring(L, Label);
    lua_pcall(L, 1, 2, 0);

    int Width = lua_tointeger(L, -2);
    lua_pop(L, 2);

    return Width;
}


// ========== Shortcut combobox for selecting normals, axes, etc.
bool GRANNY
AxisSelector(int&            Axis,
             LuaRect const&  Rect,
             bool            Enabled,
             ControlIDGen&   IDGen)
{
    vector<char const*> AxisNames(6);
    {
        AxisNames[0] = "+X Axis";
        AxisNames[1] = "+Y Axis";
        AxisNames[2] = "+Z Axis";
        AxisNames[3] = "-X Axis";
        AxisNames[4] = "-Y Axis";
        AxisNames[5] = "-Z Axis";
    }
    CompileAssert(0 == eAimAxis_XAxis    &&
                  1 == eAimAxis_YAxis    &&
                  2 == eAimAxis_ZAxis    &&
                  3 == eAimAxis_NegXAxis &&
                  4 == eAimAxis_NegYAxis &&
                  5 == eAimAxis_NegZAxis);
    Assert(Axis >= -1 && Axis <= eAimAxis_NegZAxis);

    // Note that both paths set the control rect for us...
    if (Enabled)
    {
        return ComboboxOn(AxisNames, Axis, Rect, IDGen);
    }
    else
    {
        char const* PlaceholdingName = Axis != -1 ? AxisNames[Axis] : 0;
        ComboboxPlaceholder(PlaceholdingName, Rect, IDGen);
        return false;
    }
}

// ========== Editor for track masks
bool GRANNY
MaskEditOn(GSTATE mask_source* SourceNode,
           ControlIDGen&       IDGen)
{
    lua_State* L = UILuaState();

    gstate_character_instance* Instance = SourceNode->GetBoundCharacter();
    if (!Instance)
    {
        SetLastControlRect(LuaRect(0, 0, 0, 0));
        return false;
    }

    PushTableFunction(L, "MaskEdit", "Do");
    IDGen.PushNextID();
    lua_pushinteger(L, edit::TokenizedToID(SourceNode));
    lua_pcall(L, 2, 2, 0);

    bool Changed = false;
    if (lua_istable(L, -2))
    {
        PushUndoPos("Edit mask");

        model* Model = (model*)GetSourceModelForCharacter(Instance);
        unbound_track_mask* NewMask = NewUnboundTrackMask(0, Model->Skeleton->BoneCount);
        {for (int Idx = 0; Idx < Model->Skeleton->BoneCount; ++Idx)
        {
            lua_pushinteger(L, Idx+1);
            lua_gettable(L, -3);

            int Val = lua_toboolean(L, -1);
            lua_pop(L, 1);

            NewMask->Weights[Idx].Name   = (char*)Model->Skeleton->Bones[Idx].Name;
            NewMask->Weights[Idx].Weight = real32(Val);
        }}

        SourceNode->SetMask((granny_unbound_track_mask*)NewMask);
        FreeUnboundTrackMask(NewMask);

        Changed = true;
    }

    // Always returns the size...
    LuaPoint pt;
    if (ExtractPoint(L, -1, pt))
    {
        SetLastControlRect(LuaRect(0, 0, pt.x, pt.y));
    }
    else
    {
        // todo: log, that's really bad
        SetLastControlRect(LuaRect(0, 0, 0, 0));
    }

    // Ret, Pt
    lua_pop(L, 2);

    return Changed;
}


// ========== Tool tips...
void GRANNY
RegisterToolTip(char const* Tip,
                LuaRect const& Rect)
{
    lua_State* LuaState = UILuaState();

    PushTableFunction(LuaState, "ToolTip", "Register");
    lua_pushstring(LuaState, Tip);
    PushRect(LuaState, Rect);
    lua_pcall(LuaState, 2, 0, 0);
}

void GRANNY
LastControlTip(char const* Tip)
{
    RegisterToolTip(Tip, LastControlRect());
}


// ========== Helper functions for computing dimensions, label widths, etc,
int32x GRANNY CenterX(LuaRect const& area)
{
    return area.x + int32x(area.w/2);
}

int32x GRANNY HPadding()
{
    return ControlHPadding;
}

int32x GRANNY VPadding()
{
    return ControlVPadding;
}


void GRANNY
LabeledControlsAtWidth(int32x Width, int32x* LabelX, int32x* ControlX)
{
    *LabelX   = ControlHPadding + Width;
    *ControlX = *LabelX + ControlHPadding;
}

void GRANNY
LabeledControlsInArea(int32x Width, LuaRect const& Rect, int32x* LabelX, int32x* ControlX)
{
    *LabelX   = Rect.x + ControlHPadding + Width;
    *ControlX = *LabelX + ControlHPadding;
}

LuaPoint GRANNY
HPadPtAtY(int32x AtY)
{
    return LuaPoint(ControlHPadding, AtY);
}

LuaRect GRANNY
FillRectToRight(int32x AtX, int32x Width, int32x CurrY, int32x Height)
{
    return LuaRect(AtX, CurrY, Width - AtX - ControlHPadding, Height);
}

LuaRect GRANNY
FillRectToLeft(int32x AtX, int32x CurrY, int32x Height)
{
    return LuaRect(ControlHPadding, CurrY, AtX - ControlHPadding, Height);
}


int32x GRANNY
LabelWidth(const char* String)
{
    int handle = GetPreferenceFont("label_text");
    return LabelWidthImpl(String, handle);
}

int32x GRANNY
SmallLabelWidth(const char* String)
{
    int handle = GetPreferenceFont("small_label_text");
    return LabelWidthImpl(String, handle);
}

int32x GRANNY
CheckboxLabelAdjust(int32x Y)
{
    return Y - 1;
}

int32x GRANNY
ComboLabelAdjust(int32x Y)
{
    // todo: auto-determine
    return Y + 7;
}

int32x GRANNY
SmallComboLabelAdjust(int32x Y)
{
    // todo: auto-determine
    return Y + 3;
}

int32x GRANNY
EditLabelAdjust(int32x Y)
{
    // todo: auto-determine
    return Y + 3;
}

int32x GRANNY
SmallButtonLabelAdjust(int32x Y)
{
    // todo: auto-determine
    return Y + 1;
}



bool GRANNY
DraggableLocation(real32*       Position,
                  ControlIDGen& IDGen,
                  real32 LineLength)
{
    lua_State* L = UILuaState();

    // This one is a bit weird, in that we register a lua side click to get dragging
    // feedback, but the drawing is all on this side, where we have the 3d transform.
    camera Camera;
    bool IgnoreTracks;
    GetSceneCamera(&Camera, &IgnoreTracks);

    LuaRect uiArea = UIAreaGet();

    // Granny treats lower left as (0,0), while we use upper left
    real32 CurrPosition[3];
    {
        GrannyWorldSpaceToWindowSpace((granny_camera*)&Camera,
                                      real32(uiArea.w),
                                      real32(uiArea.h),
                                      Position, CurrPosition);
        CurrPosition[1] = uiArea.h - CurrPosition[1];
    }

    LuaPoint UIPosition((int32x)CurrPosition[0],
                        (int32x)CurrPosition[1]);

    PushTableFunction(L, "DraggableLoc", "Do");
    IDGen.PushNextID();
    PushPoint(L, UIPosition);
    lua_pcall(L, 2, 4, 0);
    // todo: handle error

    bool Dragging = !!lua_toboolean(L, -4);
    bool Changed  = !!lua_toboolean(L, -3);
    LuaPoint NewPoint;
    ExtractPoint(L, -2, NewPoint);
    bool Hover = !!lua_toboolean(L, -1);

    // Recompute a 3d position based on the motion in camera space...
    real32 NewWindowSpace[3] = {
        real32(NewPoint.x), real32(uiArea.h - NewPoint.y), CurrPosition[2]
    };

    real32 NewWorldSpace[3];
    GrannyWindowSpaceToWorldSpace((granny_camera*)&Camera,
                                  real32(uiArea.w), real32(uiArea.h),
                                  NewWindowSpace, NewWorldSpace);
    if (Changed)
    {
        // Send it back to the caller
        Position[0] = NewWorldSpace[0];
        Position[1] = NewWorldSpace[1];
        Position[2] = NewWorldSpace[2];
    }

    // Draw the target
    glDisable(GL_LINE_SMOOTH);
    if (Hover || Dragging)
        glLineWidth(4);
    else
        glLineWidth(2);

    glBegin(GL_LINES);
    {
        glColor3f(1, 0, 0);
        glVertex3fv(NewWorldSpace);
        glVertex3f(NewWorldSpace[0] + LineLength, NewWorldSpace[1], NewWorldSpace[2]);

        glColor3f(0, 1, 0);
        glVertex3fv(NewWorldSpace);
        glVertex3f(NewWorldSpace[0], NewWorldSpace[1] + LineLength, NewWorldSpace[2]);

        glColor3f(0, 0, 1);
        glVertex3fv(NewWorldSpace);
        glVertex3f(NewWorldSpace[0], NewWorldSpace[1], NewWorldSpace[2] + LineLength);
    }
    glEnd();

    return Changed;
}


