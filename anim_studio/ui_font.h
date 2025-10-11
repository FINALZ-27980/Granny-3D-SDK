#if !defined(UI_FONT_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/statement/ui_font.h $
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

struct LuaColor;
enum EFontConstants
{
    eInvalidFont = -1
};

enum EFontRenderFlags
{
    eCentered   = 1 << 0,
    eMultiline  = 1 << 1,
    eBreakWords = 1 << 2,
};

// Font functions based on DanT's in the Miles tool.  Allows use of Windows fonts with
// proper AA and kerning.

int32x UIGetFontHandle(char const* FaceName, int32x PointSize, bool Bold, bool Italic);

bool UIGetFontHeightAndBaseline(int32x FontHandle, int32x& FontHeight, int32x& Baseline);
bool UIGetTextDimension(int32x      FontHandle,
                        char const* String,
                        uint32x     Flags,
                        int32x&     Width,
                        int32x&     Height);

int RenderText(int32x FontHandle, char const* String, uint32x Flags, int PosX, int PosY, LuaColor const& Color);
int RenderDropText(int32x FontHandle, char const* String, uint32x Flags, int PosX, int PosY, LuaColor const& Color, LuaColor const& DropColor);

void UIFontFrameStart();
void UIFontFrameEnd();



bool UIFont_Register(lua_State*);
void UIFont_Shutdown();

END_GRANNY_NAMESPACE;


#include "header_postfix.h"
#define UI_FONT_H
#endif /* UI_FONT_H */
