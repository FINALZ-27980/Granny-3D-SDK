#if !defined(UI_CORE_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/statement/ui_core.h $
// $DateTime: 2012/04/19 11:20:44 $
// $Change: 37081 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================

#include <windows.h>
#include <string>

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif


struct lua_State;
BEGIN_GRANNY_NAMESPACE;

// Startup
bool UIStartup(HWND hwnd);
void UIShutdown();


// Must match event.lua
#define MOUSE_EVENT_RANGE 0
#define KEYBOARD_EVENT_RANGE 100
#define LOSS_OF_FOCUS_EVENT 1000
#define NULL_EVENT 2000

// Event interface
enum EMouseEvent
{
    eLeftDown    = MOUSE_EVENT_RANGE + 0,
    eLeftUp      = MOUSE_EVENT_RANGE + 1,
    eRightDown   = MOUSE_EVENT_RANGE + 2,
    eRightUp     = MOUSE_EVENT_RANGE + 3,
    eMiddleDown  = MOUSE_EVENT_RANGE + 4,
    eMiddleUp    = MOUSE_EVENT_RANGE + 5,
    eMouseMove   = MOUSE_EVENT_RANGE + 6,
    eMouseWheel  = MOUSE_EVENT_RANGE + 7,

    eBackDown    = MOUSE_EVENT_RANGE + 8,
    eBackUp      = MOUSE_EVENT_RANGE + 9,
    eForwardDown = MOUSE_EVENT_RANGE + 10,
    eForwardUp   = MOUSE_EVENT_RANGE + 11
};

enum EKeyBoardEvent
{
    eKeyDown     = KEYBOARD_EVENT_RANGE + 0,
    eKeyUp       = KEYBOARD_EVENT_RANGE + 1,
    eDirKeyDown  = KEYBOARD_EVENT_RANGE + 2,
    eDirKeyUp    = KEYBOARD_EVENT_RANGE + 3,
    eCtrlKeyDown = KEYBOARD_EVENT_RANGE + 4,
    eCtrlKeyUp   = KEYBOARD_EVENT_RANGE + 5,
    eFnKeyDown   = KEYBOARD_EVENT_RANGE + 6,
    eFnKeyUp     = KEYBOARD_EVENT_RANGE + 7,
};

// todo: shitty! matches event.lua
enum DirectionKeys
{
    Direction_KeyUp    = 0,
    Direction_KeyDown  = 1,
    Direction_KeyRight = 2,
    Direction_KeyLeft  = 3,
    Direction_KeyHome  = 4,
    Direction_KeyEnd   = 5,
    Direction_KeyNext  = 6,
    Direction_KeyPrev  = 7
};

enum ControlKeys
{
    Control_KeyBackspace = 0,
    Control_KeyDelete    = 1,
    Control_KeyEnter     = 2,
    Control_KeyEscape    = 3,
    Control_KeyBackward  = 4,
    Control_KeyForward   = 5,
    Control_KeyStop      = 6,
    Control_KeyPlayPause = 7
};

void UIMouseMove(int x, int y);
void UIMouseWheel(int x, int y, int delta);
void UIMouseEvent(int x, int y, EMouseEvent EventType);


void UICtrlKey(int keyCode, bool down);
void UIDirKey(int keyCode, bool down);
void UINormalKey(int keyCode, bool down);
void UIFnKey(int FnNumber, bool down);

void UILossOfFocusEvent();

// size control
void UINoteSize(int w, int h);
void UIGetSize(int* w, int* h);

// For dealing with Win32
HWND UIGetHWnd();

// Main paint/think function
void UIInteract();

// Global UI Lua state
lua_State* UILuaState();

// Persistent values stored in the registry
bool GetApplicationInt(char const* ValName, uint32 Default, uint32* Value);
bool SetApplicationInt(char const* ValName, uint32 NewVal);

bool GetApplicationString(char const* ValName, char const* Default, std::string* Value);
bool SetApplicationString(char const* ValName, char const* String);

// Print to the fading log buffer
struct LuaColor;
extern LuaColor gNotifyColor;
extern LuaColor gNormalColor;
extern LuaColor gWarningColor;
extern LuaColor gErrorColor;
void FadePrintf(LuaColor const& Color,
                float FadeTime,
                char const* Fmt, ...);

// Needed for loading...
void UIZeroClock();

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define UI_CORE_H
#endif /* UI_CORE_H */
