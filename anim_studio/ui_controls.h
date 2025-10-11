// ========================================================================
// $File: //jeffr/granny_29/statement/ui_controls.h $
// $DateTime: 2012/07/20 17:10:16 $
// $Change: 38457 $
// $Revision: #7 $
//
// $Notice: $
// ========================================================================
#if !defined(UI_CONTROLS_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

#if !defined(LUAUTILS_H)
#include "luautils.h"
#endif


BEGIN_GSTATE_NAMESPACE;
class mask_source;
END_GSTATE_NAMESPACE;


#include <vector>
#include <string>

BEGIN_GRANNY_NAMESPACE;

struct ControlIDGen
{
    char   ParentID[256];
    int32x CurrID;

public:
    ControlIDGen(char const* IDFmt, ...);

    void PushNextID();

    // For placeholders...
    void ConsumeNextID();

private:
    ControlIDGen();
    ControlIDGen(const ControlIDGen&);
    ControlIDGen& operator=(const ControlIDGen&);
};

enum Justification
{
    eLeft   = 0,
    eRight  = 1,
    eCenter = 2
};

// =======================================================================================
// Control section
//
//  Note that the convention for all controls that return a boolean value is that they
//  return "true" when the user has commited the change.
//
//  All controls here report their dimension through the LastControlRect function.
// =======================================================================================

// ========== Layout assistance         
LuaRect const& LastControlRect();

void AdvanceY(int32x& CurrY, int32x By = 0);
void AdvanceYRect(int32x& CurrY, LuaRect const&);
void AdvanceYLastControl(int32x& CurrY);

void AdvanceX(int32x& CurrY, int32x By = 0);
void AdvanceXRect(int32x& CurrY, LuaRect const&);

int32x CenterX(LuaRect const& area);
int32x HPadding();
int32x VPadding();

void LabeledControlsAtWidth(int32x Width, int32x* LabelX, int32x* ControlX);
void LabeledControlsInArea(int32x Width, LuaRect const& Area, int32x* LabelX, int32x* ControlX);

LuaPoint HPadPtAtY(int32x AtY);
LuaRect FillRectToRight(int32x AtX, int32x Width, int32x CurrY = 0, int32x Height = 0);
LuaRect FillRectToLeft(int32x AtX, int32x CurrY = 0, int32x Height = 0);


// ========== Draws a separation line
//
void SeparatorAt(int32x YVal, LuaRect const& drawArea);

// ========== Draws a separation header
//
void SectionHeaderAt(char const* Header, int32x YVal, LuaRect const& drawArea);

// ========== Labels
//
void ColorLabelAt(char const* String, LuaPoint const& pt, LuaColor const& color, Justification just = eLeft);
void LabelAt(char const* String, LuaPoint const& pt, Justification just = eLeft);
void SmallLabelAt(char const* String, LuaPoint const& pt, Justification just = eLeft);


// ========== Edit boxes
//
bool EditLabelOn(std::string& Label,
                 LuaRect const& Rect,
                 ControlIDGen& IDGen);
void EditLabelPlaceholder(std::string& Label,
                          LuaRect const& Rect,
                          ControlIDGen& IDGen);

// ========== Checkbox & small checkbox
//
LuaRect CheckboxDim(char const* Text);
bool    CheckboxOn(bool& Value,
                   char const* Text,
                   LuaRect const& Rect,
                   ControlIDGen& IDGen);

LuaRect SmallCheckboxDim(char const* Text);
bool    SmallCheckboxOn(bool& Value,
                        char const* Text,
                        LuaRect const& Rect,
                        ControlIDGen& IDGen);

// ========== Numerical slider
//
int SliderHeight();
bool SliderOn(real32 Min,
              real32 Max,
              real32& Value,
              bool ClampToInts,
              LuaRect const& Rect,
              ControlIDGen& IDGen);

int SmallSliderHeight();
bool SmallSliderOn(real32 Min,
                   real32 Max,
                   real32& Value,
                   bool ClampToInts,
                   LuaRect const& Rect,
                   ControlIDGen& IDGen);


// ========== Combobox
bool ComboboxOn(std::vector<char const*>& Strings,
                int32x& Selection,
                LuaRect const& Rect,
                ControlIDGen& IDGen);

bool SmallComboboxOn(std::vector<char const*>& Strings,
                     int32x& Selection,
                     LuaRect const& Rect,
                     ControlIDGen& IDGen);

void ComboboxPlaceholder(char const* Label,  // may be null
                         LuaRect const& Rect,
                         ControlIDGen& IDGen);
void SmallComboboxPlaceholder(char const* Label,  // may be null
                              LuaRect const& Rect,
                              ControlIDGen& IDGen);

// ========== Itunes version
bool ITunesComboboxOn(std::vector<char const*>& Strings,
                      int32x& Selection,
                      LuaRect const& Rect,
                      ControlIDGen& IDGen);


// ========== Buttons
bool ButtonAt(char const* Label,
              LuaPoint const& At,
              bool Enabled,
              ControlIDGen& IDGen);
bool ButtonAtWithCallback(char const* Label,
                          LuaPoint const& At,
                          bool Enabled,
                          ControlIDGen& IDGen);

bool SmallButtonAt(char const* Label,
                   LuaPoint const& At,
                   bool Enabled,
                   ControlIDGen& IDGen);
bool SmallButtonAtWithCallback(char const* Label,
                               LuaPoint const& At,
                               bool Enabled,
                               ControlIDGen& IDGen);
bool SmallButtonAtRectWithCallback(char const* Label,
                                   LuaRect const& Rect,
                                   bool Enabled,
                                   ControlIDGen& IDGen);

bool BitmapButtonAt(LuaPoint const& At,
                    int BitmapHandle,
                    ControlIDGen& IDGen);
bool BitmapButtonAtWithCallback(LuaPoint const& At,
                                int BitmapHandle,
                                ControlIDGen& IDGen);

int ButtonHeight();
int ButtonWidth(char const* Label);
int SmallButtonHeight();
int SmallButtonWidth(char const* Label);
LuaRect BitmapButtonDim(int BitmapHandle);


// ========== Shortcut combobox for selecting normals, axes, etc.
//
bool AxisSelector(int&            Axis,
                  LuaRect const&  Rect,
                  bool            Enabled,
                  ControlIDGen&   IDGen);


// ========== Editor for track masks
//
bool MaskEditOn(GSTATE mask_source* SourceNode,
                ControlIDGen& IDGen);

// ========== Tool tips...
void RegisterToolTip(char const* Tip, LuaRect const& Rect);
void LastControlTip(char const* Tip);




// ========== Helper functions for computing dimensions, label widths, etc,
int32x LabelWidth(char const* String);
int32x SmallLabelWidth(char const* String);

int32x CheckboxLabelAdjust(int32x Y);
int32x ComboLabelAdjust(int32x Y);
int32x SmallComboLabelAdjust(int32x Y);
int32x EditLabelAdjust(int32x Y);
int32x SmallButtonLabelAdjust(int32x Y);



// Special case, should really move...
bool
DraggableLocation(real32*       Position,
                  ControlIDGen& IDGen,
                  real32 LineLength);


END_GRANNY_NAMESPACE;

#define UI_CONTROLS_H
#endif /* UI_CONTROLS_H */
