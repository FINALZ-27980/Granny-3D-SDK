// ========================================================================
// $File: //jeffr/granny_29/statement/ui_font.cpp $
// $DateTime: 2012/01/09 10:59:52 $
// $Change: 36208 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#include "ui_font.h"
#include "statement.h"

#include "luautils.h"
#include "ui_core.h"
#include "ui_area.h"

#include "granny_assert.h"
#include "granny_memory.h"
#include "granny_system_clock.h"
#include "granny_parameter_checking.h"
#include "granny_string.h"

#include <windows.h>
#include <gl/gl.h>
#include <vector>
#include <map>

// Should always be the last header included
#include "granny_cpp_settings.h"

#define SubsystemCode ApplicationLogMessage
USING_GRANNY_NAMESPACE;
using namespace std;

static const double s_StringTimeout = 1.0;

struct FontDescriptor
{
    char   FaceName[64];
    int32x PointSize;
    bool   IsBold;
    bool   IsItalic;

    HFONT  FontHandle;

    // Cached for speed
    int32x FontHeight;
    int32x FontBaseline;
    int32x FontOverhang;

    bool Matches(char const* TestFaceName, int32x TestPointSize, bool TestBold, bool TestItalic) const
    {
        return  (StringsAreEqualLowercase(FaceName, TestFaceName) &&
                 TestPointSize == PointSize &&
                 TestBold == IsBold &&
                 TestItalic == IsItalic);
    }
};

struct TextDescriptor
{
    int32x      FontHandle;
    uint32x     Flags;
    char const* String;

    bool operator<(TextDescriptor const& Other) const
    {
        if (FontHandle < Other.FontHandle)
            return true;
        else if (FontHandle > Other.FontHandle)
            return false;

        if (Flags < Other.Flags)
            return true;
        else if (Flags > Other.Flags)
            return false;

        return StringDifference(String, Other.String) < 0;
    }
};

struct TextEntry
{
    int32x  Width;
    int32x  Height;
    int32x  Baseline;
    real32  UScale;
    real32  VScale;
    GLuint  TextureHandle;

    // Last used so we can cull these
    system_clock LastUsed;
};

typedef vector<FontDescriptor>         FontList;
typedef map<TextDescriptor, TextEntry> TextEntryMap;

static DWORD
UIFontFlagsToDTFlags(uint32x Flags)
{
    DWORD DTFlags = 0;
    DTFlags |= (Flags & eMultiline) ? 0 : DT_SINGLELINE;
    DTFlags |= (Flags & eCentered) ?  DT_CENTER : 0;
    DTFlags |= (Flags & eBreakWords) ? DT_WORDBREAK : 0;

    return DTFlags;
}


// =============================================================================
//  File scope objects
HDC          s_FontDrawDC = 0;
FontList     s_KnownFonts;
TextEntryMap s_CreatedTexts;

system_clock s_ThisFrameTime = { 0 };


static bool
CreateGLTextureForString(int32x      FontHandle,
                         char const* String,
                         uint32x     Flags,
                         int32x      Width,
                         int32x      Height,
                         int32x      Baseline,
                         real32&     UScale,
                         real32&     VScale,
                         GLuint&     TextureHandle)
{
    Assert(FontHandle >= 0 && FontHandle < int(s_KnownFonts.size()));
    Assert(String);
    Assert(Width <= 1024);
    Assert(Height <= 1024);

    // Compute the pow2 width/height for this texture
    int32x TexWidth  = 1;
    int32x TexHeight = 1;
    while (TexWidth  < Width)  TexWidth  <<= 1;
    while (TexHeight < Height) TexHeight <<= 1;

    HDC hDC = GetDC(UIGetHWnd());
    BITMAPINFO Init = {0};
    {
        Init.bmiHeader.biBitCount = 32;
        Init.bmiHeader.biHeight   = Height;
        Init.bmiHeader.biWidth    = Width;
        Init.bmiHeader.biPlanes   = 1;
        Init.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        Init.bmiHeader.biSizeImage = Width * Height * 4;
    }

    void* bits = 0;
    HBITMAP hBitmap = CreateDIBSection(hDC, &Init, DIB_RGB_COLORS, &bits, 0, 0);
    if (hBitmap == 0)
    {
        return false;
    }

    ReleaseDC(UIGetHWnd(), hDC);

    // Setup the DC for rendering and render!
    {
        FontDescriptor const& Font = s_KnownFonts[FontHandle];
        HANDLE hOldFont   = SelectObject(s_FontDrawDC, Font.FontHandle);
        HANDLE hOldBitmap = SelectObject(s_FontDrawDC, hBitmap);

        SetTextColor(s_FontDrawDC, 0x00FFFFFF);
        SetBkColor(s_FontDrawDC, 0x00000000);

        // Clear out the bitmap
        {
            HBRUSH BackBrush = CreateSolidBrush(0x00000000);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(s_FontDrawDC, BackBrush);
            PatBlt(s_FontDrawDC, 0, 0, Width, Height, PATCOPY);
            SelectObject(s_FontDrawDC, hOldBrush);
            DeleteObject(BackBrush);
        }

        DWORD DrawingFlags = UIFontFlagsToDTFlags(Flags);
        RECT R;
        SetRect(&R, 0, 0, Width, Height);
        int32x drawnHeight = DrawTextA(s_FontDrawDC, String, -1, &R, DrawingFlags);
        drawnHeight = drawnHeight;


        // Clear out our objects
        SelectObject(s_FontDrawDC, hOldFont);
        SelectObject(s_FontDrawDC, hOldBitmap);
    }


    BITMAPINFO Info[2] = { 0 };
    Info[0].bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

    vector<uint8> SrcBytes(Width*Height*4, 0);
    if (!GetDIBits(s_FontDrawDC, hBitmap, 0, Height, 0, Info, DIB_RGB_COLORS) ||
        !GetDIBits(s_FontDrawDC, hBitmap, 0, Height, &SrcBytes[0], Info, DIB_RGB_COLORS))
    {
        DeleteObject(hBitmap);
        return false;
    }

    // We are done with this bitmap now...
    DeleteObject(hBitmap);
    hBitmap = 0;

    vector<uint8> TexBytes(TexWidth*TexHeight*4, 0xff);
    for (int y = 0; y < Height; y++)
    {
        {for (int x = 0; x < Width; ++x)
        {
            TexBytes[(y*TexWidth+x)*4 + 0] = 0xff;
            TexBytes[(y*TexWidth+x)*4 + 1] = 0xff;
            TexBytes[(y*TexWidth+x)*4 + 2] = 0xff;
            TexBytes[(y*TexWidth+x)*4 + 3] = SrcBytes[(y*Width+x)*4 + 1];
        }}
    }

    glGenTextures(1, &TextureHandle);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, TextureHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TexWidth, TexHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, &TexBytes[0]);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

    UScale = real32(Width) / TexWidth;
    VScale = real32(Height)/ TexHeight;

    return true;
}

// Looks up a text entry, and creates it if it doesn't yet exist
static TextEntry*
GetTextEntry(int32x      FontHandle,
             char const* String,
             uint32x     Flags)
{
    CheckInt32Index(FontHandle, int(s_KnownFonts.size()), return 0);
    CheckPointerNotNull(String, return 0);

    // First, look for a matching entry that we may already have...
    TextDescriptor td;
    td.FontHandle = FontHandle;
    td.Flags      = Flags;
    td.String     = String;

    TextEntryMap::iterator itr = s_CreatedTexts.find(td);
    if (itr != s_CreatedTexts.end())
    {
        itr->second.LastUsed = s_ThisFrameTime;
        return &(itr->second);
    }

    // Need to create a new entry in this case.
    TextEntry NewEntry;
    NewEntry.LastUsed = s_ThisFrameTime;

    int32x Ignored;
    if (!UIGetFontHeightAndBaseline(FontHandle, Ignored, NewEntry.Baseline) ||
        !UIGetTextDimension(FontHandle, String, Flags, NewEntry.Width, NewEntry.Height))
    {
        // todo log error
        return 0;
    }

    if (!CreateGLTextureForString(FontHandle, String, Flags,
                                  NewEntry.Width, NewEntry.Height, NewEntry.Baseline,
                                  NewEntry.UScale,
                                  NewEntry.VScale,
                                  NewEntry.TextureHandle))
    {
        // todo log error
        return 0;
    }

    // Make sure we have a cloned copy of the string, since it might go away...
    td.String = CloneString(String);

    // Insert into the map
    s_CreatedTexts[td] = NewEntry;
    return &s_CreatedTexts[td];
}


static void
DestroyTextEntry(TextDescriptor& Descriptor, TextEntry& Entry)
{
    Deallocate((void*)Descriptor.String);
    Descriptor.String = 0;

    glDeleteTextures(1, &Entry.TextureHandle);
    Entry.TextureHandle = 0;
}

int32x GRANNY
UIGetFontHandle(char const* FaceName,
                int32x      PointSize,
                bool        Bold,
                bool        Italic)
{
    StTMZone(__FUNCTION__);

    // Windows puts a 32 char limit on the facename.  FontDescriptor exceeds that, but
    // check...
    CheckCondition(StringLength(FaceName) < 32, return eInvalidFont);

    {for (size_t Idx = 0; Idx < s_KnownFonts.size(); ++Idx)
    {
        if (s_KnownFonts[Idx].Matches(FaceName, PointSize, Bold, Italic))
            return int32x(Idx);
    }}

    FontDescriptor fd;
    CopyString(FaceName, fd.FaceName);
    fd.PointSize  = PointSize;
    fd.IsBold     = Bold;
    fd.IsItalic   = Italic;
    fd.FontHandle = 0;

    HDC hDC = GetDC(UIGetHWnd());

    // Fixed 96 log pixels for now...
    fd.FontHandle = CreateFont(-MulDiv(PointSize, 96, 72) - 1,
                               0, 0, 0,  // width, escapement, angle (default)
                               Bold ? FW_BOLD : FW_NORMAL,
                               Italic ? TRUE : FALSE,
                               FALSE,
                               FALSE,
                               ANSI_CHARSET,  // todo: utf-8 for i18n?
                               OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS,
                               ANTIALIASED_QUALITY|PROOF_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE,
                               FaceName);
    {
        SelectObject(s_FontDrawDC, fd.FontHandle);
        TEXTMETRIC tm;
        if (!GetTextMetrics(s_FontDrawDC, &tm))
            return false;

        fd.FontHeight   = tm.tmHeight;
        fd.FontBaseline = tm.tmHeight - tm.tmDescent;
        fd.FontOverhang = tm.tmOverhang;
    }

    ReleaseDC(UIGetHWnd(), hDC);

    if (fd.FontHandle != 0)
    {
        s_KnownFonts.push_back(fd);
        return int(s_KnownFonts.size() - 1);
    }
    else
    {
        return eInvalidFont;
    }
}


bool GRANNY
UIGetFontHeightAndBaseline(int32x FontHandle,
                           int32x& FontHeight,
                           int32x& BaseLine)
{
    StTMZone(__FUNCTION__);

    CheckInt32Index(FontHandle, int32x(s_KnownFonts.size()), return false);

    FontDescriptor const& Font = s_KnownFonts[FontHandle];
    FontHeight = Font.FontHeight;
    BaseLine   = Font.FontBaseline;
    return true;
}


int DimensionCacheHits   = 0;
int DimensionCacheMisses = 0;

bool GRANNY
UIGetTextDimension(int32x      FontHandle,
                   char const* String,
                   uint32x     Flags,
                   int32x&     Width,
                   int32x&     Height)
{
    StTMZone(__FUNCTION__);

    CheckInt32Index(FontHandle, int32x(s_KnownFonts.size()), return false);
    CheckPointerNotNull(String, return false);

    // Before we go to windows, let's look to see if the object already exists
    {
        // First, look for a matching entry that we may already have...
        TextDescriptor td;
        td.FontHandle = FontHandle;
        td.Flags      = Flags;
        td.String     = String;

        TextEntryMap::iterator itr = s_CreatedTexts.find(td);
        if (itr != s_CreatedTexts.end())
        {
            // Awesome!  We can just use the stored parameters for this object.  Note that
            // this doesn't touch the LastUsed field
            Width  = itr->second.Width;
            Height = itr->second.Height;

            ++DimensionCacheHits;
            return true;
        }
    }
    ++DimensionCacheMisses;

    FontDescriptor const& Font = s_KnownFonts[FontHandle];
    SelectObject(s_FontDrawDC, Font.FontHandle);

    DWORD DrawingFlags = UIFontFlagsToDTFlags(Flags);
    RECT R;
    SetRect(&R, 0, 0, 0, 0);
    if (Flags & eMultiline)
    {
        R.right = 0xffffffff;
    }
    // todo: clipped multiline

    DrawTextA(s_FontDrawDC, String, -1, &R, DrawingFlags | DT_CALCRECT);

    Width  = R.right;
    Height = R.bottom;

    // for f***'s sake, MS.
#if 0
    // Compute the right overhang
    char LastChar = String[strlen(String)-1];
    ABC abc;
    if (GetCharABCWidths(s_FontDrawDC, LastChar, LastChar, &abc))
    {
        Width -= abc.abcC;
    }
#endif

    return true;
}


// Unclipped render
int GRANNY
RenderText(int32x FontHandle, char const* String, uint32x Flags, int PosX, int PosY, LuaColor const& Color)
{
    StTMZone(__FUNCTION__);

    CheckInt32Index(FontHandle, int(s_KnownFonts.size()), return false);
    CheckPointerNotNull(String, return false);

    // Get the text object first
    TextEntry const* Entry = GetTextEntry(FontHandle, String, Flags);
    if (!Entry)
        return false;

    {
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, Entry->TextureHandle);

        glColor4f(Color.r, Color.g, Color.b, Color.a);
        glBegin(GL_QUADS);
        {
            glTexCoord2f(0,             Entry->VScale); glVertex2f(real32(PosX),                real32(PosY - Entry->Baseline));
            glTexCoord2f(Entry->UScale, Entry->VScale); glVertex2f(real32(PosX + Entry->Width), real32(PosY - Entry->Baseline));
            glTexCoord2f(Entry->UScale, 0);             glVertex2f(real32(PosX + Entry->Width), real32(PosY - Entry->Baseline + Entry->Height));
            glTexCoord2f(0,             0);             glVertex2f(real32(PosX),                real32(PosY - Entry->Baseline + Entry->Height));
        }
        glEnd();
    }

    return Entry->Width;
}


int GRANNY
RenderDropText(int32x FontHandle, char const* String, uint32x Flags,
               int PosX, int PosY,
               LuaColor const& Color,
               LuaColor const& DropColor)
{
    CheckInt32Index(FontHandle, int(s_KnownFonts.size()), return false);
    CheckPointerNotNull(String, return false);

    int Width = RenderText(FontHandle, String, Flags, PosX, PosY+1, DropColor);
    RenderText(FontHandle, String, Flags, PosX, PosY, Color);

    // Account for the shift...
    return Width + 1;
}

static bool
RenderTextClipped(int32x FontHandle, char const* String, uint32x Flags, LuaRect const& Rect, LuaColor const& Color)
{
    StTMZone(__FUNCTION__);

    CheckInt32Index(FontHandle, int(s_KnownFonts.size()), return false);
    CheckPointerNotNull(String, return false);

    // Use the draw area code to set the scissor, and test for an empty region
    if (!UIAreaPush(Rect, LuaRect(0, 0, Rect.w, Rect.h)))
    {
        UIAreaPop();
        return true;
    }

    // Get the text object first
    TextEntry const* Entry = GetTextEntry(FontHandle, String, Flags);
    if (!Entry)
        return false;


    int Center = 0;
    if (Flags & eCentered)
    {
        // We want this to be an int to land on pixel boundaries...
        Center = int((Rect.w - Entry->Width) / 2);
    }

    {
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindTexture(GL_TEXTURE_2D, Entry->TextureHandle);

        glColor3f(Color.r, Color.g, Color.b);
        glBegin(GL_QUADS);
        {
            glTexCoord2f(0,             Entry->VScale); glVertex2f(real32(Center),                real32(0));
            glTexCoord2f(Entry->UScale, Entry->VScale); glVertex2f(real32(Center + Entry->Width), real32(0));
            glTexCoord2f(Entry->UScale, 0);             glVertex2f(real32(Center + Entry->Width), real32(0 + Entry->Height));
            glTexCoord2f(0,             0);             glVertex2f(real32(Center),                real32(0 + Entry->Height));
        }
        glEnd();
    }

    UIAreaPop();
    return true;
}


void GRANNY
UIFontFrameStart()
{
    GetSystemSeconds(&s_ThisFrameTime);
}


void GRANNY
UIFontFrameEnd()
{
    StTMZone(__FUNCTION__);

    // Loop through and find old strings to cull
    {for (TextEntryMap::iterator itr = s_CreatedTexts.begin(); itr != s_CreatedTexts.end(); ++itr)
    {
        if (GetSecondsElapsed(itr->second.LastUsed, s_ThisFrameTime) > s_StringTimeout)
        {
            // Kill that string!
            TextDescriptor td = itr->first;
            TextEntry      te = itr->second;
            s_CreatedTexts.erase(itr);

            DestroyTextEntry(td, te);

            // One per frame only.
            break;
        }
    }}

    StTMPlotInt(DimensionCacheHits+DimensionCacheMisses, "AnimStudio/Font/DimensionQueries(dimcache)");
    StTMPlotInt(DimensionCacheHits,   "AnimStudio/Font/DimensionCacheHits(dimcache)");
    StTMPlotInt(DimensionCacheMisses, "AnimStudio/Font/DimensionCacheMisses(dimcache)");

    DimensionCacheHits   = 0;
    DimensionCacheMisses = 0;
}


// =============================================================================
// Lua gateway
int
l_UIFontGet(lua_State* L)
{
    char const* FaceName = lua_tostring(L, -4);
    int32x      Size     = (int32x)lua_tointeger(L, -3);
    bool        Bold     = lua_toboolean(L, -2) != 0;
    bool        Italic   = lua_toboolean(L, -1) != 0;

    lua_pushinteger(L, UIGetFontHandle(FaceName, Size, Bold, Italic));
    return 1;
}

int
l_GetFontHeightAndBaseline(lua_State* L)
{
    int32x FontHandle = (int32x)lua_tointeger(L, -1);

    int32x h, bl;
    if (UIGetFontHeightAndBaseline(FontHandle, h, bl))
    {
        lua_pushinteger(L, h);
        lua_pushinteger(L, bl);
        return 2;
    }
    else
    {
        // todo: error
        return 0;
    }
}

int
l_GetSimpleTextDimension(lua_State* L)
{
    int32x      FontHandle = (int32x)lua_tointeger(L, -2);
    char const* String     = lua_tostring(L, -1);

    int32x w,h;
    if (UIGetTextDimension(FontHandle, String, 0, w, h))
    {
        lua_pushinteger(L, w);
        lua_pushinteger(L, h);
        return 2;
    }
    else
    {
        // todo: error
        return 0;
    }
}

int
l_RenderText(lua_State* L)
{
    int32x      FontHandle = (int32x)lua_tointeger(L, -5);
    char const* String     = lua_tostring(L, -4);
    uint32x     Flags      = (uint32x)lua_tointeger(L, -3);

    LuaPoint pt;
    LuaColor col;
    if (!ExtractPoint(L, -2, pt) || !ExtractColor(L, -1, col))
    {
        // todo log param
        return 0;
    }

    lua_pushnumber(L, RenderText(FontHandle, String, Flags, pt.x, pt.y, col));
    return 1;
};

int
l_RenderDropText(lua_State* L)
{
    int32x      FontHandle = (int32x)lua_tointeger(L, -6);
    char const* String     = lua_tostring(L, -5);
    uint32x     Flags      = (uint32x)lua_tointeger(L, -4);

    LuaPoint pt;
    LuaColor col;
    LuaColor col_drop;
    if (!ExtractPoint(L, -3, pt) ||
        !ExtractColor(L, -2, col) ||
        !ExtractColor(L, -1, col_drop))
    {
        // todo log param
        return 0;
    }

    RenderDropText(FontHandle, String, Flags, pt.x, pt.y, col, col_drop);

    return 0;
};

int
l_RenderTextClipped(lua_State* L)
{
    int32x      FontHandle = (int32x)lua_tointeger(L, -5);
    char const* String     = lua_tostring(L, -4);
    uint32x     Flags      = (uint32x)lua_tointeger(L, -3);

    LuaRect r;
    LuaColor col;
    if (!ExtractRect(L, -2, r) || !ExtractColor(L, -1, col))
    {
        // todo log param
        return 0;
    }

    RenderTextClipped(FontHandle, String, Flags, r, col);
    return 0;
};

bool GRANNY
UIFont_Register(lua_State* State)
{
    Assert(s_FontDrawDC == 0);
    s_FontDrawDC = CreateCompatibleDC(0);
    if (s_FontDrawDC == 0)
        return false;

    lua_register(State, "UIFont_Get", l_UIFontGet);
    lua_register(State, "GetSimpleTextDimension",   l_GetSimpleTextDimension);
    lua_register(State, "GetFontHeightAndBaseline", l_GetFontHeightAndBaseline);
    lua_register(State, "RenderText",        l_RenderText);
    lua_register(State, "RenderTextClipped", l_RenderTextClipped);

    lua_register(State, "RenderDropText",        l_RenderDropText);

    return true;
}

void GRANNY
UIFont_Shutdown()
{
    {for (TextEntryMap::iterator itr = s_CreatedTexts.begin(); itr != s_CreatedTexts.end(); ++itr)
    {
        // Oh, I'm a bad monkey...
        DestroyTextEntry((TextDescriptor&)itr->first, itr->second);
    }}
    s_CreatedTexts.clear();

    {for (size_t Idx = 0; Idx < s_KnownFonts.size(); ++Idx)
    {
        DeleteObject(s_KnownFonts[Idx].FontHandle);
        s_KnownFonts[Idx].FontHandle = 0;
    }}
    s_KnownFonts.clear();


    // Close down the drawing DC
    DeleteDC(s_FontDrawDC);
    s_FontDrawDC = 0;
}
