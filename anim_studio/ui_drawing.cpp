// ========================================================================
// $File: //jeffr/granny_29/statement/ui_drawing.cpp $
// $DateTime: 2012/03/14 17:48:04 $
// $Change: 36772 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#include "ui_drawing.h"

#include "ui_bitmap.h"
#include "ui_core.h"
#include "ui_font.h"
#include "ui_preferences.h"

#include "luautils.h"
#include "statement.h"

#include "granny_assert.h"
#include "granny_parameter_checking.h"

#include <windows.h>
#include <math.h>
#include <gl/gl.h>


// Should always be the last header included
#include "granny_cpp_settings.h"

#define SubsystemCode Undefined_LogMessage
USING_GRANNY_NAMESPACE;

void GRANNY
DrawRect(LuaRect const& Rect, LuaColor const& Color)
{
    // Draw a solid rect
    {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glColor4fv(&Color.r);
        glRecti(Rect.x, Rect.y, Rect.x + Rect.w, Rect.y + Rect.h);
    }
}

void GRANNY
DrawRectOutline(LuaRect const& Rect,
                LuaColor const& Color)
{
    glDisable(GL_TEXTURE_2D);
    glColor4fv(&Color.r);

    // Super annoying, but the only way to be sure...
    glRecti(Rect.x, Rect.y, Rect.x + Rect.w, Rect.y + 1);
    glRecti(Rect.x, Rect.y + Rect.h - 1, Rect.x + Rect.w, Rect.y + Rect.h);

    glRecti(Rect.x, Rect.y, Rect.x + 1, Rect.y + Rect.h);
    glRecti(Rect.x + Rect.w - 1, Rect.y, Rect.x + Rect.w, Rect.y + Rect.h);
}


void GRANNY
DrawOutlineRect(LuaRect const& Rect,
                LuaColor const& Color,
                LuaColor const& OutlineColor)
{
    DrawRect(Rect, Color);
    DrawRectOutline(Rect, OutlineColor);
}


// DrawRect(Rect, Color);
static int
l_DrawRect(lua_State* L)
{
    LuaRect Rect;
    if (!ExtractRect(L, -2, Rect))
    {
        StTMPrintfERR("ClearRect, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }

    LuaColor Color;
    if (!ExtractColor(L, -1, Color))
    {
        StTMPrintfERR("ClearRect, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }
    lua_pop(L, 2);

    DrawRect(Rect, Color);

    return 0;
}

// DrawRect(Rect, Color);
static int
l_DrawRectOutline(lua_State* L)
{
    LuaRect Rect;
    if (!ExtractRect(L, -2, Rect))
    {
        StTMPrintfERR("ClearRect, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }

    LuaColor Color;
    if (!ExtractColor(L, -1, Color))
    {
        StTMPrintfERR("ClearRect, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }
    lua_pop(L, 2);

    // Draw a solid rect
    {
        glDisable(GL_TEXTURE_2D);
        glColor4fv(&Color.r);

        // Super annoying, but the only way to be sure...
        glRecti(Rect.x, Rect.y, Rect.x + Rect.w, Rect.y + 1);
        glRecti(Rect.x, Rect.y + Rect.h - 1, Rect.x + Rect.w, Rect.y + Rect.h);

        glRecti(Rect.x, Rect.y, Rect.x + 1, Rect.y + Rect.h);
        glRecti(Rect.x + Rect.w - 1, Rect.y, Rect.x + Rect.w, Rect.y + Rect.h);
    }

    return 0;
}

// DrawStippleRect(Rect, Color0, Color1);
static int
l_DrawStippleRect(lua_State* L)
{
    LuaRect Rect;
    if (!ExtractRect(L, -3, Rect))
    {
        StTMPrintfERR("ClearRect, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }

    LuaColor Color0, Color1;
    if (!ExtractColor(L, -2, Color0) ||
        !ExtractColor(L, -1, Color1))
    {
        StTMPrintfERR("ClearRect, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }
    lua_pop(L, 3);

    int32x  TexHandle = UIGetBitmapHandle("stipple");
    uint32x TexName   = UIGetBitmapGLName(TexHandle);

    int32x w, h;
    if (!UIGetBitmapWidthAndHeight(TexHandle, w, h))
        return 0;

    if ((w & (w-1)) != 0 ||
        (h & (h-1)) != 0)
    {
        // Only works with pow2 bitmaps
        // @@ log
        return 0;
    }
        
    real32 u0 = Rect.w / real32(w);
    real32 v0 = Rect.h / real32(h);

    {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, TexName);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        glBegin(GL_POLYGON);
        {
            glColor4fv(&Color1.r);
            glTexCoord2f( 0 + 1.5f/w,  0); glVertex2i(Rect.x,          Rect.y);
            glTexCoord2f(u0 + 1.5f/w,  0); glVertex2i(Rect.x + Rect.w, Rect.y);
            glTexCoord2f(u0 + 1.5f/w, v0); glVertex2i(Rect.x + Rect.w, Rect.y + Rect.h);
            glTexCoord2f( 0 + 1.5f/w, v0); glVertex2i(Rect.x,          Rect.y + Rect.h);
        }
        glEnd();

        // glBegin(GL_POLYGON);
        // {
        //     glColor4fv(&Color0.r);
        //     glTexCoord2f(0,  0);  glVertex2i(Rect.x,          Rect.y);
        //     glTexCoord2f(u0, 0);  glVertex2i(Rect.x + Rect.w, Rect.y);
        //     glTexCoord2f(u0, v0); glVertex2i(Rect.x + Rect.w, Rect.y + Rect.h);
        //     glTexCoord2f(0,  v0); glVertex2i(Rect.x,          Rect.y + Rect.h);
        // }
        glEnd();
    }

    return 0;
}

static int
l_DrawBeveledRect(lua_State* L)
{
    LuaRect Rect;
    if (!ExtractRect(L, -4, Rect))
    {
        StTMPrintfERR("ClearRect, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }

    bool IsRaised = lua_toboolean(L, -3) != 0;
    LuaColor CentralColor;
    LuaColor BevelColor;
    if (!ExtractColor(L, -2, BevelColor) ||
        !ExtractColor(L, -1, CentralColor))
    {
        StTMPrintfERR("ClearRect, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }
    lua_pop(L, 4);

    // Precompute these, they reverse for lowered
    LuaColor topLeft0, topLeft1;
    LuaColor botRight0, botRight1;
    if (!IsRaised)
    {
        topLeft0  = LuaColor(BevelColor.r, BevelColor.g, BevelColor.b);
        topLeft1  = LuaColor(BevelColor.r/2, BevelColor.g/2, BevelColor.b/2);
        botRight0 = LuaColor(BevelColor.r/4, BevelColor.g/4, BevelColor.b/4);
        botRight1 = LuaColor(3*BevelColor.r/4, 3*BevelColor.g/4, 3*BevelColor.b/4);
    }
    else
    {
        botRight0 = LuaColor(BevelColor.r, BevelColor.g, BevelColor.b);
        botRight1 = LuaColor(BevelColor.r/2, BevelColor.g/2, BevelColor.b/2);
        topLeft0  = LuaColor(BevelColor.r/4, BevelColor.g/4, BevelColor.b/4);
        topLeft1  = LuaColor(3*BevelColor.r/4, 3*BevelColor.g/4, 3*BevelColor.b/4);
    }

    {
        glDisable(GL_TEXTURE_2D);

        glColor3fv(&topLeft0.r);
        glRecti(Rect.x+1, Rect.y+Rect.h-1, Rect.x + Rect.w, Rect.y+Rect.h);
        glRecti(Rect.x+Rect.w-1, Rect.y, Rect.x + Rect.w, Rect.y+Rect.h);

        glColor3fv(&topLeft1.r);
        glRecti(Rect.x, Rect.y, Rect.x + Rect.w, Rect.y+1);
        glRecti(Rect.x, Rect.y, Rect.x+1, Rect.y+Rect.h);

        glColor3fv(&botRight0.r);
        glRecti(Rect.x+1, Rect.y+1, Rect.x + Rect.w - 1, Rect.y+2);
        glRecti(Rect.x+1, Rect.y+1, Rect.x+2, Rect.y+Rect.h-1);

        glColor3fv(&botRight1.r);
        glRecti(Rect.x+1, Rect.y+Rect.h-2, Rect.x + Rect.w - 1, Rect.y+Rect.h-1);
        glRecti(Rect.x+Rect.w-2, Rect.y+1, Rect.x+Rect.w-1, Rect.y+Rect.h-1);

        glColor3fv(&CentralColor.r);
        glRecti(Rect.x+2, Rect.y+2, Rect.x+Rect.w-2, Rect.y+Rect.h-2);
    }

    return 0;
}


// ClearScreen(Color);
static int
l_ClearScreen(lua_State* L)
{
    LuaColor Color;
    if (!ExtractColor(L, -1, Color))
    {
        StTMPrintfERR("ClearScreen, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }
    lua_pop(L, 1);

    // Draw a solid rect
    {
        glDisable(GL_SCISSOR_TEST);
        glClearColor(Color.r, Color.g, Color.b, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
    }

    return 0;
}


static int l_DrawTimeCompass(lua_State* L)
{
    LuaRect Rect;
    if (!ExtractRect(L, -4, Rect))
    {
        printf("DrawTimeCompass: error in arg list\n");
        return 0;
    }

    // Copied from older code, just map the variables from LuaRect
    int x = Rect.x;
    int y = Rect.y;
    int w = Rect.w;
    int h = Rect.h;

    real32 CurrT     = (real32)lua_tonumber(L, -3);
    real32 PixPerSec = (real32)lua_tonumber(L, -2);
    real32 LeastVal  = (real32)lua_tonumber(L, -1);

    // Remove the arguments from the stack
    lua_pop(L, 3);
    DrawRect(Rect, LuaColor(0.3f, 0.3f, 0.3f));

    int const SightPosition = x + w/2;

    {
        // Draw the valid range...
        int const ZeroPosition  = int(SightPosition - (CurrT - 0)        * PixPerSec);
        int const LeastPosition = int(SightPosition - (CurrT - LeastVal) * PixPerSec);
        DrawRect(LuaRect(LeastPosition, y + h/4, ZeroPosition - LeastPosition, h/2),
                 LuaColor(0.2f, 0.6f, 0.2f, 0.6f));
    }

    int32x Arial = UIGetFontHandle("Arial", 11, false, false);
    int Height, BL;
    UIGetFontHeightAndBaseline(Arial, Height, BL);

    // Draw the hatches down
    {
        int32x const FirstBelow    = (int32x)floor(CurrT);
        real32 const FirstPosition = SightPosition - (CurrT - FirstBelow) * PixPerSec;

        real32 TickLens[10] = {
            h * 0.75f, h * 0.25f, h * 0.25f, h * 0.25f, h * 0.25f,
            h * 0.50f, h * 0.25f, h * 0.25f, h * 0.25f, h * 0.25f
        };

        // Draw down.  This one owns the first position.  We'll be stepping by 10ths,
        {
            real32 CurrPosition = FirstPosition;
            int    CurrentTenth = 0;

            while (CurrPosition > x)
            {
                real32 Length = TickLens[CurrentTenth % 10];

                DrawRect(LuaRect(int(CurrPosition), y, 1, int(Length)),
                         LuaColor(0.5f, 0.5f, 0.5f));

                if ((CurrentTenth % 10) == 0)
                {
                    // Draw the seconds
                    int32x ThisSec = FirstBelow - (CurrentTenth/10);

                    char buffer[64];
                    sprintf(buffer, "%d", ThisSec);

                    RenderText(Arial, buffer, 0,
                               (int)(CurrPosition + 3),
                               (int)(y + h*0.75f - Height + BL),
                               LuaColor(1, 1, 1));
                }

                // advance
                CurrPosition -= PixPerSec / 10.0f;
                CurrentTenth += 1;
            }
        }

        // Draw up.
        {
            real32 CurrPosition = FirstPosition + PixPerSec/10.0f;
            int    CurrentTenth = 1;

            while (CurrPosition < (x + w))
            {
                real32 Length = TickLens[CurrentTenth % 10];

                DrawRect(LuaRect(int(CurrPosition), y, 1, int(Length)),
                         LuaColor(0.5f, 0.5f, 0.5f));

                if ((CurrentTenth % 10) == 0)
                {
                    // Draw the seconds
                    int32x ThisSec = FirstBelow + (CurrentTenth/10);

                    char buffer[64];
                    sprintf(buffer, "%d", ThisSec);

                    RenderText(Arial, buffer, 0,
                               (int)(CurrPosition + 3),
                               (int)(y + h*0.75f - Height + BL),
                               LuaColor(1, 1, 1));
                }

                // advance
                CurrPosition += PixPerSec * 0.1f;
                CurrentTenth += 1;
            }
        }
    }

    // Draw the bombsight
    {
        glPushAttrib(GL_ALL_ATTRIB_BITS);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        DrawRect(LuaRect(SightPosition-20, y, 40, h), LuaColor(1, 1, 1, 0.15f));
        DrawRect(LuaRect(SightPosition-10, y, 20, h), LuaColor(1, 1, 1, 0.15f));
        DrawRect(LuaRect(SightPosition-1, y, 2, h), LuaColor(1, 1, 1, 1));
        DrawRect(LuaRect(SightPosition-20, y, 1, h), LuaColor(0, 0, 0));
        DrawRect(LuaRect(SightPosition+20, y, 1, h), LuaColor(0, 0, 0));

        glPopAttrib();
    }

    return 0;
}

static int
l_DrawToolTip(lua_State* L)
{
    char const* TipText = lua_tostring(L, -2);
    if (TipText == 0)
        return 0;

    LuaPoint pt;
    if (!ExtractPoint(L, -1, pt))
        return 0;

    int handle = GetPreferenceFont("tooltip");
    int h, bl;
    UIGetFontHeightAndBaseline(handle, h, bl);

    int32x Width, Height;
    if (!UIGetTextDimension(handle, TipText, eMultiline, Width, Height))
    {
        return 0;
    }

    int const s_TipGapDown  = 25;
    int const s_TipGapUp    = 5;
    int const s_TipPadding  = 4;

    // Adjust the point for the screen position
    {
        int ScreenW, ScreenH;
        UIGetSize(&ScreenW, &ScreenH);
        if (pt.y + Height < ScreenH - s_TipPadding*3)
        {
            // Mouse is in upper half of screen, move the box down
            pt.y += s_TipGapDown;
        }
        else
        {
            // Lower half, first offset the point up by the height of the resulting box
            pt.y -= Height;
            pt.y -= s_TipGapUp;
        }

        pt.x += s_TipPadding;
        if (pt.x + Width >= ScreenW - s_TipPadding*3)
        {
            // Mouse is in left half of screen, slide it left
            pt.x = ScreenW - s_TipPadding*3 - Width;
        }
    }

    LuaColor hiColor(1, 1, 0.85f);
    LuaColor backColor(0, 0, 0);
    DrawOutlineRect(LuaRect(pt.x - s_TipPadding,
                            pt.y - s_TipPadding,
                            Width  + 2 * s_TipPadding,
                            Height + 2 * s_TipPadding),
                    hiColor, backColor);

    RenderText(handle, TipText, eMultiline, pt.x, pt.y + bl, backColor);

    return 0;
}

// @@ Make inf precise
double s_CurrZoomFactor = 1.0f;

void GRANNY
SetLineWidth(real32 Width)
{
    glEnable (GL_LINE_SMOOTH);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(float(Width / s_CurrZoomFactor));
}

real32 GRANNY
GetZoomFactor()
{
    return real32(s_CurrZoomFactor);
}


static int
l_UINoteZoom(lua_State* L)
{
    double n = lua_tonumber(L, -1);
    s_CurrZoomFactor *= n;
    return 0;
}
static int
l_UIRemoveZoom(lua_State* L)
{
    double n = lua_tonumber(L, -1);
    s_CurrZoomFactor /= n;
    return 0;
}

bool GRANNY
UIDrawing_Register(lua_State* State)
{
    lua_register(State, "DrawRect",        l_DrawRect);
    lua_register(State, "DrawRectOutline", l_DrawRectOutline);
    lua_register(State, "DrawStippleRect", l_DrawStippleRect);
    lua_register(State, "DrawBeveledRect", l_DrawBeveledRect);
    lua_register(State, "ClearScreen",     l_ClearScreen);
    lua_register(State, "DrawTimeCompass", l_DrawTimeCompass);
    lua_register(State, "DrawToolTip",     l_DrawToolTip);

    // For tracking zoom, so we can get the line widths right...
    lua_register(State, "UINoteZoom", l_UINoteZoom);
    lua_register(State, "UIRemoveZoom", l_UIRemoveZoom);
    
    return true;
}

