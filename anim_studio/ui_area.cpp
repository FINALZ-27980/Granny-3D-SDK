// ========================================================================
// $File: //jeffr/granny_29/statement/ui_area.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "ui_area.h"

#include "statement.h"
#include "luautils.h"
#include "ui_core.h"
#include "ui_controls.h"

#include "granny_assert.h"
#include "granny_parameter_checking.h"

#include <windows.h>
#include <gl/gl.h>
#include <vector>

// Should always be the last header included
#include "granny_cpp_settings.h"

#define SubsystemCode Undefined_LogMessage

USING_GRANNY_NAMESPACE;
using namespace std;


struct UIAreaEntry
{
    LuaRect ParentArea;
    LuaRect ChildArea;

    // // Controls if we need to stop traversing up the stack on activate.  This allows
    // // global rendering for menus, etc.
    // bool Terminate;
};

static vector<UIAreaEntry> s_AreaStack;

// For projecting into/out of the currently active region
double s_CurrScaleX  = 1;
double s_CurrOffsetX = 0;
double s_CurrScaleY  = 1;
double s_CurrOffsetY = 0;

double s_CurrScissorX = 0;
double s_CurrScissorY = 0;
double s_CurrScissorW = 0;
double s_CurrScissorH = 0;

bool   MousePointInScreen = false;
int32x MousePointX = -1;
int32x MousePointY = -1;

static void
UIAreaClear()
{
    s_AreaStack.clear();
}

static int
UIAreaDepth()
{
    return int(s_AreaStack.size());
}

static bool
UIAreaActivate()
{
    Assert(!s_AreaStack.empty());

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    int ScreenW, ScreenH;
    UIGetSize(&ScreenW, &ScreenH);

    // Scissor start out as the whole screen.
    double SXMin, SXMax, SYMin, SYMax;
    {
        SXMin = 0;
        SXMax = double(ScreenW);
        SYMin = 0;
        SYMax = double(ScreenH);
    }

    {for (size_t Idx = 0; Idx < s_AreaStack.size(); ++Idx)
    {
        UIAreaEntry const& Entry = s_AreaStack[Idx];

        // This isn't the most efficient, since we're going to the model view stack each
        // time, but it's fine for now...
        {
            double MView[16];
            glGetDoublev(GL_MODELVIEW_MATRIX, MView);

            // We're going to take advantage of the fact that we know that only the x/y
            // components matter here, and that there is no off-diagonal component
            double PXMin = double(Entry.ParentArea.x);
            double PXMax = double(Entry.ParentArea.x + Entry.ParentArea.w);
            double PYMin = double(Entry.ParentArea.y);
            double PYMax = double(Entry.ParentArea.y + Entry.ParentArea.h);

            double NewMinX = MView[0*4 + 0] * PXMin + MView[3*4 + 0];
            double NewMaxX = MView[0*4 + 0] * PXMax + MView[3*4 + 0];
            double NewMinY = MView[1*4 + 1] * PYMin + MView[3*4 + 1];
            double NewMaxY = MView[1*4 + 1] * PYMax + MView[3*4 + 1];

            SXMin = max(SXMin, NewMinX);
            SYMin = max(SYMin, NewMinY);
            SXMax = min(SXMax, NewMaxX);
            SYMax = min(SYMax, NewMaxY);
        }

        // Eqn is:
        //  Translate(ParentBR)
        //  Translate(ParentDim / ClientDim);
        //  Translate(-ClientBR);

        glTranslated(double(Entry.ParentArea.x), double(Entry.ParentArea.y), 0);
        glScaled(double(Entry.ParentArea.w) / double(Entry.ChildArea.w),
                 double(Entry.ParentArea.h) / double(Entry.ChildArea.h),
                 1.0);
        glTranslated(double(-Entry.ChildArea.x), double(-Entry.ChildArea.y), 0);
    }}

    // Capture the final scale/offset factors
    {
        double MView[16];
        glGetDoublev(GL_MODELVIEW_MATRIX, MView);

        s_CurrScaleX  = MView[0*4 + 0];
        s_CurrScaleY  = MView[1*4 + 1];
        s_CurrOffsetX = MView[3*4 + 0];
        s_CurrOffsetY = MView[3*4 + 1];
    }

    // Set the scissor.  Note that above, we've assumed that we're working with 0,0 in the
    // top left, correct that in the call to glScissor, which assumes bottom left
    s_CurrScissorX = SXMin;
    s_CurrScissorY = SYMax;
    s_CurrScissorW = SXMax - SXMin;
    s_CurrScissorH = SYMax - SYMin;

    // TODO: rounding could cause problems here?
    glScissor(int(s_CurrScissorX), int(ScreenH - s_CurrScissorY),
              int(s_CurrScissorW), int(s_CurrScissorH));

    // Return true iff there is a non-empty drawing region
    return (s_CurrScissorW > 0 && s_CurrScissorH > 0);
}

static void
UIAreaNoteMousePos()
{
    POINT mousePt;
    RECT clientRect;
    if (GetCursorPos(&mousePt) &&
        ScreenToClient(UIGetHWnd(), &mousePt) &&
        GetClientRect(UIGetHWnd(), &clientRect))
    {
        MousePointX = mousePt.x;
        MousePointY = mousePt.y;

        MousePointInScreen = ((MousePointX >= 0 && MousePointX < clientRect.right) &&
                              (MousePointY >= 0 && MousePointY < clientRect.bottom));
    }
    else
    {
        MousePointInScreen = false;
    }
}



void GRANNY
UIAreaBeginFrame(LuaRect const& ScreenRect)
{
    StTMZone(__FUNCTION__);

    UIAreaNoteMousePos();
    UIAreaClear();
    UIAreaPush(ScreenRect, LuaRect(0, 0, ScreenRect.w, ScreenRect.h));
}

void GRANNY
UIAreaEndFrame()
{
    UIAreaPop();
    Assert(s_AreaStack.empty());
}

bool GRANNY
UIAreaPush(LuaRect const& Parent, LuaRect const& Child)
{
    UIAreaEntry NewEntry = { Parent, Child };
    s_AreaStack.push_back(NewEntry);

    return UIAreaActivate();
}

void GRANNY
UIAreaPop()
{
    Assert(!s_AreaStack.empty());
    s_AreaStack.pop_back();

    if (!s_AreaStack.empty())
        UIAreaActivate();
}


// The current rectangle is the client area of the back of the stack..
LuaRect GRANNY
UIAreaGet()
{
    Assert(!s_AreaStack.empty());

    return s_AreaStack.back().ChildArea;
}

void GRANNY
ScreenToArea(LuaPoint& pt)
{
    // todo: truncarific!
    pt.x = int((pt.x - s_CurrOffsetX) / s_CurrScaleX);
    pt.y = int((pt.y - s_CurrOffsetY) / s_CurrScaleY);
}

void GRANNY
AreaToScreen(LuaPoint& pt)
{
    // todo: truncarific!
    pt.x = int((pt.x * s_CurrScaleX) + s_CurrOffsetX);
    pt.y = int((pt.y * s_CurrScaleY) + s_CurrOffsetY);
}

// UIAreaPush(ParentRect, ChildRect)
static int
l_UIAreaPush(lua_State* L)
{
    LuaRect Parent;
    if (!ExtractRect(L, -2, Parent))
    {
        StTMPrintfERR("UIAreaPush, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }

    LuaRect Child;
    if (!ExtractRect(L, -1, Child))
    {
        StTMPrintfERR("UIAreaPush, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }

    lua_pushboolean(L, UIAreaPush(Parent, Child));
    return 1;
}

// UIAreaPop()
static int
l_UIAreaPop(lua_State* L)
{
    UIAreaPop();
    return 0;
}

// UIAreaGet()
static int
l_UIAreaGet(lua_State* L)
{
    LuaRect Current = UIAreaGet();
    if (PushRect(L, Current) == false)
    {
        StTMPrintfERR("UIArea.Get: unable to push result?\n");
        return 0;
    }

    return 1;
}

// UIAreaGet()
static int
l_UIAreaActivate(lua_State* L)
{
    UIAreaActivate();
    return 0;
}

static bool
PushAreaCapture(lua_State* L)
{
    lua_createtable(L, 8, 0);

    lua_pushinteger(L, 1); lua_pushnumber(L, s_CurrScaleX);   lua_settable(L, -3);
    lua_pushinteger(L, 2); lua_pushnumber(L, s_CurrOffsetX);  lua_settable(L, -3);
    lua_pushinteger(L, 3); lua_pushnumber(L, s_CurrScaleY);   lua_settable(L, -3);
    lua_pushinteger(L, 4); lua_pushnumber(L, s_CurrOffsetY);  lua_settable(L, -3);
    lua_pushinteger(L, 5); lua_pushnumber(L, s_CurrScissorX); lua_settable(L, -3);
    lua_pushinteger(L, 6); lua_pushnumber(L, s_CurrScissorY); lua_settable(L, -3);
    lua_pushinteger(L, 7); lua_pushnumber(L, s_CurrScissorW); lua_settable(L, -3);
    lua_pushinteger(L, 8); lua_pushnumber(L, s_CurrScissorH); lua_settable(L, -3);

    return true;
}

static bool
ExtractAreaCapture(lua_State* L, int index,
                   real32& CapScaleX,
                   real32& CapOffsetX,
                   real32& CapScaleY,
                   real32& CapOffsetY,
                   real32& CapScissorX,
                   real32& CapScissorY,
                   real32& CapScissorW,
                   real32& CapScissorH)
{
    if (!lua_istable(L, index))
    {
        // todo: error
        return false;
    }

    // Remember that arrays are 1-indexed in lua
    lua_pushinteger(L, 1); lua_gettable(L, index - 1); CapScaleX   = (real32)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_pushinteger(L, 2); lua_gettable(L, index - 1); CapOffsetX  = (real32)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_pushinteger(L, 3); lua_gettable(L, index - 1); CapScaleY   = (real32)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_pushinteger(L, 4); lua_gettable(L, index - 1); CapOffsetY  = (real32)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_pushinteger(L, 5); lua_gettable(L, index - 1); CapScissorX = (real32)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_pushinteger(L, 6); lua_gettable(L, index - 1); CapScissorY = (real32)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_pushinteger(L, 7); lua_gettable(L, index - 1); CapScissorW = (real32)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_pushinteger(L, 8); lua_gettable(L, index - 1); CapScissorH = (real32)lua_tonumber(L, -1); lua_pop(L, 1);

    return true;
}


static int
l_UIAreaCapture(lua_State* L)
{
    Assert(!s_AreaStack.empty());

    // Returns a simple table that captures the current state of the area stack, plus a
    // boolean indicating whether the scissor region is empty.
    bool const ScissorEmpty = (s_CurrScissorW > 0 && s_CurrScissorH > 0);

    PushAreaCapture(L);
    lua_pushboolean(L, ScissorEmpty);

    return 2;
}


static int
l_UIAreaTransformToCapture(lua_State* L)
{
    Assert(!s_AreaStack.empty());

    real32 CapScaleX;
    real32 CapOffsetX;
    real32 CapScaleY;
    real32 CapOffsetY;
    real32 CapScissorX;
    real32 CapScissorY;
    real32 CapScissorW;
    real32 CapScissorH;

    LuaPoint ScreenPt;
    if (!ExtractAreaCapture(L, -2,
                            CapScaleX, CapOffsetX,
                            CapScaleY, CapOffsetY,
                            CapScissorX, CapScissorY,
                            CapScissorW, CapScissorH) ||
        !ExtractPoint(L, -1, ScreenPt))
    {
        StTMPrintfERR("UIAreaScreenToClient, arg list incorrect?\n");
        stackDump(L);
        return false;
    }

    LuaPoint NewPoint;
    NewPoint.x = int((ScreenPt.x - CapOffsetX) / CapScaleX);
    NewPoint.y = int((ScreenPt.y - CapOffsetY) / CapScaleY);

    if (!PushPoint(L, NewPoint))
    {
        StTMPrintfERR("UIAreaScreenToClient: unable to push result?\n");
        return 0;
    }

    bool Clipped = false;
    if (ScreenPt.x < CapScissorX || ScreenPt.x > CapScissorX + CapScissorW)
        Clipped = true;
    else if (ScreenPt.y > CapScissorY || ScreenPt.y < CapScissorY - CapScissorH)
        Clipped = true;

    lua_pushboolean(L, Clipped);

    return 2;
}

static int
l_UIAreaTransformFromCapture(lua_State* L)
{
    Assert(!s_AreaStack.empty());

    real32 CapScaleX;
    real32 CapOffsetX;
    real32 CapScaleY;
    real32 CapOffsetY;
    real32 CapScissorX;
    real32 CapScissorY;
    real32 CapScissorW;
    real32 CapScissorH;

    LuaPoint CapturePt;

    if (!ExtractAreaCapture(L, -2,
                            CapScaleX, CapOffsetX,
                            CapScaleY, CapOffsetY,
                            CapScissorX, CapScissorY,
                            CapScissorW, CapScissorH) ||
        !ExtractPoint(L, -1, CapturePt))
    {
        StTMPrintfERR("UIAreaScreenToClient, arg list incorrect?\n");
        stackDump(L);
        return false;
    }

    LuaPoint NewPoint;
    NewPoint.x = int(CapturePt.x * CapScaleX + CapOffsetX);
    NewPoint.y = int(CapturePt.y * CapScaleY + CapOffsetY);

    if (!PushPoint(L, NewPoint))
    {
        StTMPrintfERR("UIAreaScreenToClient: unable to push result?\n");
        return 0;
    }

    bool Clipped = false;
    if (NewPoint.x < CapScissorX || NewPoint.x > CapScissorX + CapScissorW)
        Clipped = true;
    else if (NewPoint.y > CapScissorY || NewPoint.y < CapScissorY - CapScissorH)
        Clipped = true;

    lua_pushboolean(L, Clipped);

    return 2;
}


static bool
UIAreaRectContainsMouse(LuaRect* R)
{
    Assert(!s_AreaStack.empty());

    if (!MousePointInScreen)
    {
        return false;
    }

    // Test if the mouse cursor is scissored
    if ((MousePointX < s_CurrScissorX || MousePointX > s_CurrScissorX + s_CurrScissorW) ||
        (MousePointY > s_CurrScissorY || MousePointY < s_CurrScissorY - s_CurrScissorH))
    {
        return false;
    }

    // In the client and not scissored, test the rect, if it's non-null
    if (R != 0)
    {
        // Transform to client space
        int ClientX = int((MousePointX - s_CurrOffsetX) / s_CurrScaleX);
        int ClientY = int((MousePointY - s_CurrOffsetY) / s_CurrScaleY);
        if (!R->Contains(ClientX, ClientY))
        {
            return false;
        }
    }

    // Passed all the tests!
    return true;
}

static int
l_UIAreaContainsMouse(lua_State* L)
{
    Assert(!s_AreaStack.empty());

    lua_pushboolean(L, UIAreaRectContainsMouse(NULL));
    return 1;
}

static int
l_UIAreaRectContainsMouse(lua_State* L)
{
    Assert(!s_AreaStack.empty());

    LuaRect testRect;
    if (!ExtractRect(L, -1, testRect))
    {
        StTMPrintfERR("UIAreaPush, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }

    lua_pushboolean(L, UIAreaRectContainsMouse(&testRect));
    return 1;
}

static int
l_UIAreaCaptureIsRectOnScreen(lua_State* L)
{
    real32 CapScaleX;
    real32 CapOffsetX;
    real32 CapScaleY;
    real32 CapOffsetY;
    real32 CapScissorX;
    real32 CapScissorY;
    real32 CapScissorW;
    real32 CapScissorH;

    if (!ExtractAreaCapture(L, -2,
                            CapScaleX, CapOffsetX,
                            CapScaleY, CapOffsetY,
                            CapScissorX, CapScissorY,
                            CapScissorW, CapScissorH))
    {
        StTMPrintfERR("UIAreaCaptureIsRectOnScreen, arg list incorrect?\n");
        stackDump(L);
        return false;
    }

    LuaRect CaptureRect;
    if (!ExtractRect(L, -1, CaptureRect))
    {
        StTMPrintfERR("UIAreaCaptureIsRectOnScreen, arg list incorrect?\n");
        stackDump(L);
        return 0;
    }

    int ScreenW, ScreenH;
    UIGetSize(&ScreenW, &ScreenH);


    LuaPoint NewPointUL;
    NewPointUL.x = int(CaptureRect.x * CapScaleX + CapOffsetX);
    NewPointUL.y = int(CaptureRect.y * CapScaleY + CapOffsetY);

    LuaPoint NewPointLR;
    NewPointLR.x = int((CaptureRect.x + CaptureRect.w) * CapScaleX + CapOffsetX);
    NewPointLR.y = int((CaptureRect.y + CaptureRect.h) * CapScaleY + CapOffsetY);

    if (NewPointUL.x < 0 || NewPointUL.y < 0)
        lua_pushboolean(L, false);
    else if (NewPointLR.x > ScreenW || NewPointLR.y > ScreenH)
        lua_pushboolean(L, false);
    else
        lua_pushboolean(L, true);

    return 1;
}

bool GRANNY
UIArea_Register(lua_State* State)
{
    lua_register(State, "UIAreaPush",     l_UIAreaPush);
    lua_register(State, "UIAreaPop",      l_UIAreaPop);
    lua_register(State, "UIAreaGet",      l_UIAreaGet);
    lua_register(State, "UIAreaActivate", l_UIAreaActivate);

    lua_register(State, "UIAreaCapture",            l_UIAreaCapture);
    lua_register(State, "UIAreaTransformToCapture", l_UIAreaTransformToCapture);
    lua_register(State, "UIAreaTransformFromCapture", l_UIAreaTransformFromCapture);

    lua_register(State, "UIAreaCaptureRectOnScreen", l_UIAreaCaptureIsRectOnScreen);

    lua_register(State, "UIAreaContainsMouse",     l_UIAreaContainsMouse);
    lua_register(State, "UIAreaRectContainsMouse", l_UIAreaRectContainsMouse);

    return true;
}


