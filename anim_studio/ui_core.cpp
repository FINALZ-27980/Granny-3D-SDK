// ========================================================================
// $File: //jeffr/granny_29/statement/ui_core.cpp $
// $DateTime: 2012/10/16 11:39:56 $
// $Change: 39799 $
// $Revision: #10 $
//
// $Notice: $
// ========================================================================
#include "statement.h"
#include "ui_core.h"

#include "luautils.h"
#include "simple_job.h"
#include "source_monitor.h"

#include "statement_editstate.h"
#include "statement_xinput.h"

#include "ui_area.h"
#include "ui_bitmap.h"
#include "ui_character_render.h"
#include "ui_drawing.h"
#include "ui_editfield.h"
#include "ui_font.h"
#include "ui_nodedrawing.h"

#include "gstate_container.h"

#include "granny_assert.h"
#include "granny_file_reader.h"
#include "granny_parameter_checking.h"
#include "granny_string.h"
#include "granny_system_clock.h"
#include "granny_version.h"

#include "statement_undostack.h"
#include "statement_xinput.h"

#include <windows.h>
#include <shlwapi.h>
#include <objbase.h>
#include <Wininet.h>
#include <gl/gl.h>
#include <vector>

extern "C"
{
#include <lauxlib.h>
#include <lualib.h>
}

#include <windows.h>

// Should always be the last header included
#include "granny_cpp_settings.h"

#define SubsystemCode Undefined_LogMessage
USING_GRANNY_NAMESPACE;
using namespace std;


#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1
  #include "luasources_list.h"
#else
  #include "luasources_disk.h"
  struct LuaFileEntry
  {
      char const* VarName;
      char*       VarPointer;
      int         VarLength;
      int         Run;
  };
#endif




// Relating the the current window
HGLRC s_hGLRC = 0;
HWND  s_hWnd  = 0;
int32x s_Width  = 0;
int32x s_Height = 0;

// The UI LUA state
lua_State* s_UILuaState = 0;

// Our background event pump worker
SimpleJob* s_EventPump = 0;

static LPCTSTR c_RADKey = TEXT("Software\\RAD Game Tools");
static LPCTSTR c_ApplicationKey = TEXT("Software\\RAD Game Tools\\Animation Studio");


// Defined below...
static bool StartupOpenGL(HWND hwnd);
static void ShutdownOpenGL();
static bool StartupLUA();
static void ShutdownLUA();
static bool LoadUserMacros();

LuaColor GRANNY gNotifyColor(0.7f, 1, 1);
LuaColor GRANNY gNormalColor(0.2f, 1, 0.2f);
LuaColor GRANNY gWarningColor(1, 1, 0.2f);
LuaColor GRANNY gErrorColor(1, 0.2f, 0.2f);

static void
PushShiftControlAlt(lua_State* LuaState)
{
    bool Shift = ((GetKeyState(VK_LSHIFT) & 1 << 15) ||
                  (GetKeyState(VK_RSHIFT) & 1 << 15));
    bool Control = ((GetKeyState(VK_LCONTROL) & 1 << 15) ||
                    (GetKeyState(VK_RCONTROL) & 1 << 15));
    bool Alt = ((GetKeyState(VK_LMENU) & 1 << 15) ||
                (GetKeyState(VK_RMENU) & 1 << 15));
    lua_pushboolean(LuaState, Shift);
    lua_pushboolean(LuaState, Control);
    lua_pushboolean(LuaState, Alt);
}

static void
CallSwapBuffers()
{
    StTMIdle("Waiting for Swap");
    HDC hdc = GetDC(s_hWnd);
    SwapBuffers(hdc);
    glFinish();
    ReleaseDC(s_hWnd, hdc);
}


bool GRANNY
UIStartup(HWND hwnd)
{
    if (!StartupOpenGL(hwnd))
    {
        Log0(ErrorLogMessage, ApplicationLogMessage, "Failed to initialize OpenGL\n");
        return false;
    }
    if (!StartupLUA())
    {
        Log0(ErrorLogMessage, ApplicationLogMessage, "Failed to initialize LUA\n");
        return false;
    }

    LoadUserMacros();

    return true;
}


void GRANNY
UIShutdown()
{
    // Ensure that this is cleared out...
    //  StopUpdateCheck();

    // Shutdown the EventPump job if it exists...
    if (s_EventPump)
    {
        delete s_EventPump;
        s_EventPump = 0;
    }
    ShutdownLUA();
    ShutdownOpenGL();
}

void GRANNY
UINoteSize(int w, int h)
{
    s_Width  = w;
    s_Height = h;
}

void GRANNY
UIGetSize(int* w, int* h)
{
    Assert(w && h);

    *w = s_Width;
    *h = s_Height;
}

void GRANNY
UIMouseMove(int x, int y)
{
    lua_State* LuaState = s_UILuaState;
    if (!LuaState)
        return;

    // TODO: control state
    lua_getglobal(LuaState, "MakeMouseEvent");
    lua_pushinteger(LuaState, eMouseMove);
    lua_pushinteger(LuaState, x);
    lua_pushinteger(LuaState, y);
    lua_pushinteger(LuaState, 0); // delta
    PushShiftControlAlt(LuaState);
    if (lua_pcall(LuaState, 7, 1, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call MakeMouseEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
        return;
    }

    // Call with the result of the previous function
    lua_getglobal(LuaState, "PushBackEvent");
    lua_pushvalue(LuaState, -2);
    lua_remove(LuaState, -3);
    if (lua_pcall(LuaState, 1, 0, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call PushEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
    }
}

void GRANNY
UIMouseWheel(int x, int y, int delta)
{
    lua_State* LuaState = s_UILuaState;
    if (!LuaState)
        return;

    // TODO: control state
    lua_getglobal(LuaState, "MakeMouseEvent");
    lua_pushinteger(LuaState, eMouseWheel);
    lua_pushinteger(LuaState, x);
    lua_pushinteger(LuaState, y);
    lua_pushinteger(LuaState, delta);
    PushShiftControlAlt(LuaState);
    if (lua_pcall(LuaState, 7, 1, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call MakeMouseEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
        return;
    }

    // Call with the result of the previous function
    lua_getglobal(LuaState, "PushBackEvent");
    lua_pushvalue(LuaState, -2);
    lua_remove(LuaState, -3);
    if (lua_pcall(LuaState, 1, 0, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call PushEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
    }
}

void GRANNY
UIMouseEvent(int x, int y, EMouseEvent EventType)
{
    lua_State* LuaState = s_UILuaState;
    if (!LuaState)
        return;

    // TODO: control state
    lua_getglobal(LuaState, "MakeMouseEvent");
    lua_pushinteger(LuaState, EventType);
    lua_pushinteger(LuaState, x);
    lua_pushinteger(LuaState, y);
    lua_pushinteger(LuaState, 0); // delta
    PushShiftControlAlt(LuaState);
    if (lua_pcall(LuaState, 7, 1, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call MakeMouseEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
        return;
    }

    // Call with the result of the previous function
    lua_getglobal(LuaState, "PushBackEvent");
    lua_pushvalue(LuaState, -2);
    lua_remove(LuaState, -3);
    if (lua_pcall(LuaState, 1, 0, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call PushEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
    }
}

void GRANNY
UICtrlKey(int keyCode, bool down)
{
    lua_State* LuaState = s_UILuaState;
    if (!LuaState)
        return;

    int MappedCode = -1;
    switch (keyCode)
    {
        case VK_ESCAPE:           MappedCode = Control_KeyEscape;    break;
        case VK_DELETE:           MappedCode = Control_KeyDelete;    break;
        case VK_RETURN:           MappedCode = Control_KeyEnter;     break;
        case VK_BACK:             MappedCode = Control_KeyBackspace; break;
        case VK_BROWSER_BACK:     MappedCode = Control_KeyBackward;  break;
        case VK_BROWSER_FORWARD:  MappedCode = Control_KeyForward;   break;
        case VK_MEDIA_STOP:       MappedCode = Control_KeyStop;      break;
        case VK_MEDIA_PLAY_PAUSE: MappedCode = Control_KeyPlayPause; break;

        default:
            Assert(false);
            return;
    }

    // function MakeKeyEvent(Type, Key, Shift, Ctrl, Alt)
    lua_getglobal(LuaState, "MakeKeyEvent");
    lua_pushinteger(LuaState, down ? eCtrlKeyDown : eCtrlKeyUp);
    lua_pushinteger(LuaState, MappedCode);
    PushShiftControlAlt(LuaState);
    if (lua_pcall(LuaState, 5, 1, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call MakeKeyEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
        return;
    }

    // Call with the result of the previous function
    lua_getglobal(LuaState, "PushBackEvent");
    lua_pushvalue(LuaState, -2);
    lua_remove(LuaState, -3);
    if (lua_pcall(LuaState, 1, 0, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call PushEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
    }
}

void GRANNY
UIDirKey(int keyCode, bool down)
{
    lua_State* LuaState = s_UILuaState;
    if (!LuaState)
        return;

    int MappedCode = -1;
    switch (keyCode)
    {
        case VK_UP:    MappedCode = Direction_KeyUp;    break;
        case VK_DOWN:  MappedCode = Direction_KeyDown;  break;
        case VK_RIGHT: MappedCode = Direction_KeyRight; break;
        case VK_LEFT:  MappedCode = Direction_KeyLeft;  break;
        case VK_HOME:  MappedCode = Direction_KeyHome;  break;
        case VK_END:   MappedCode = Direction_KeyEnd;   break;

        default:
            Assert(false);
            return;
    }

    // function MakeKeyEvent(Type, Key, Shift, Ctrl, Alt)
    lua_getglobal(LuaState, "MakeKeyEvent");
    lua_pushinteger(LuaState, down ? eDirKeyDown:eDirKeyUp);
    lua_pushinteger(LuaState, MappedCode);
    PushShiftControlAlt(LuaState);
    if (lua_pcall(LuaState, 5, 1, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call MakeKeyEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
        return;
    }

    // Call with the result of the previous function
    lua_getglobal(LuaState, "PushBackEvent");
    lua_pushvalue(LuaState, -2);
    lua_remove(LuaState, -3);
    if (lua_pcall(LuaState, 1, 0, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call PushEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
    }
}

void GRANNY
UIFnKey(int FnNumber, bool down)
{
    lua_State* LuaState = s_UILuaState;
    if (!LuaState)
        return;

    // function MakeFnKeyEvent(Type, Key, Shift, Ctrl, Alt)
    lua_getglobal(LuaState, "MakeKeyEvent");
    lua_pushinteger(LuaState, down ? eFnKeyDown : eFnKeyUp);
    lua_pushinteger(LuaState, FnNumber);
    PushShiftControlAlt(LuaState);
    if (lua_pcall(LuaState, 5, 1, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call MakeKeyEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
        return;
    }

    // Call with the result of the previous function
    lua_getglobal(LuaState, "PushBackEvent");
    lua_pushvalue(LuaState, -2);
    lua_remove(LuaState, -3);
    if (lua_pcall(LuaState, 1, 0, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call PushEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
    }
}


void GRANNY
UINormalKey(int keyCode, bool down)
{
    lua_State* LuaState = s_UILuaState;
    if (!LuaState)
        return;

    // function MakeKeyEvent(Type, Key, Shift, Ctrl, Alt)
    lua_getglobal(LuaState, "MakeKeyEvent");
    lua_pushinteger(LuaState, down ? eKeyDown : eKeyUp);
    lua_pushinteger(LuaState, keyCode);
    PushShiftControlAlt(LuaState);
    if (lua_pcall(LuaState, 5, 1, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call MakeKeyEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
        return;
    }

    // Call with the result of the previous function
    lua_getglobal(LuaState, "PushBackEvent");
    lua_pushvalue(LuaState, -2);
    lua_remove(LuaState, -3);
    if (lua_pcall(LuaState, 1, 0, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call PushEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
    }
}

void GRANNY
UILossOfFocusEvent()
{
    lua_State* LuaState = s_UILuaState;
    if (!LuaState)
        return;

    // function MakeKeyEvent(Type, Key, Shift, Ctrl, Alt)
    if (!ProtectedSimpleCall(LuaState, 0, "MakeLossOfFocusEvent", 1))
        return;

    // Call with the result of the previous function
    lua_getglobal(LuaState, "PushBackEvent");
    lua_pushvalue(LuaState, -2);
    lua_remove(LuaState, -3);
    if (lua_pcall(LuaState, 1, 0, 0))
    {
        Log1(ErrorLogMessage, ApplicationLogMessage,
             "Problem with call PushEvent: '%s'\n", lua_tostring(LuaState, -1));
        lua_pop(LuaState, 1);
    }
}



// todo: holy globals batman!
bool         g_InScrubMode = false;
bool         g_GlobalPause = false;
float        g_GlobalClock = -1;
float        g_GlobalDelta = 0;
float        g_GlobalClockAdjust = 0;
float        g_ScrubMultiplier = 0;

system_clock g_StartClock;
system_clock g_ScrubStartClock;
float        g_ScrubStartPoint;

void GRANNY
UIZeroClock()
{
    GetSystemSeconds(&g_StartClock);
    g_GlobalClock = 0;
}

static void
AdvanceUIClock()
{
    StTMZone(__FUNCTION__);

    if (g_GlobalClock < 0)
        UIZeroClock();

    system_clock ThisClock;
    GetSystemSeconds(&ThisClock);

    if (g_GlobalPause == false)
    {
        real32 const NewClock = (float)GetSecondsElapsed(g_StartClock, ThisClock) + g_GlobalClockAdjust;
        g_GlobalDelta = NewClock - g_GlobalClock;
        g_GlobalClock = NewClock;
    }
    else
    {
        real32 const NewClock = (float)GetSecondsElapsed(g_StartClock, ThisClock);
        g_GlobalDelta = 0;
        g_GlobalClockAdjust = g_GlobalClock - NewClock;
    }
}

static void
AdvanceScrubClock()
{
    if (g_ScrubMultiplier == 0)
    {
        GetSystemSeconds(&g_ScrubStartClock);
        return;
    }

    system_clock ThisClock;
    GetSystemSeconds(&ThisClock);
    real32 const NewClock = g_ScrubMultiplier * (float)GetSecondsElapsed(g_ScrubStartClock, ThisClock);

    real32 ScrubTime = g_ScrubStartPoint + NewClock;

    real32 Minimum, Maximum;
    GetScrubWindow(Minimum, Maximum);
    if (ScrubTime <= Minimum)
        ScrubTime = Minimum;
    if (ScrubTime >= Maximum)
        ScrubTime = Maximum;

    bool ScrubSuccess = ScrubToTime(ScrubTime);
    if (ScrubSuccess)
    {
        g_GlobalClock = ScrubTime;
        g_GlobalDelta = 0;
    }
}


class PumpJob : public SimpleJob
{
public:
    PumpJob(char const* Name) : SimpleJob(Name) { }
    virtual ~PumpJob() { }

    void TheJob()
    {
        // Pump the events
        {
            StTMZone("EventPump");

            PushTableFunction(s_UILuaState, "Interactions", "PumpEvents");
            if (!lua_pcall(s_UILuaState, 0, 1, 0))
            {
                int NumInteractions = lua_tointeger(s_UILuaState, -1);
                StTMPlotInt(NumInteractions, "AnimStudio/UI/NumInteractions");
                lua_pop(s_UILuaState, 1);
            }
            ProtectedSimpleCall(s_UILuaState, "IDItem", "FlushInactive");
        }
    }
};

static void
CallPumpEvents()
{
    if (s_EventPump == 0)
    {
        s_EventPump = new PumpJob("EventPump");
    }

    s_EventPump->DoJob();
}

static void
WaitPumpEvents()
{
    Assert(s_EventPump);

    StTMBlocked("WaitPumpEvents");
    s_EventPump->WaitForJob();
}


void GRANNY
UIInteract()
{
    if (s_hGLRC == 0 || s_hWnd == 0)
        return;

    StTMZone("UIInteract");

    // Setup the projection
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, s_Width, s_Height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glEnable(GL_SCISSOR_TEST);
        glScissor (0, 0, s_Width, s_Height);
        glViewport(0, 0, s_Width, s_Height);
    }


    // Draw the ui, pushing the global rect onto the stack
    if (g_InScrubMode == false)
    {
        StTMSection("Editing");
        {
            StTMZone("FrameStartup");

            // Look for changed sources
            edit::LookForChangedSources();

            AdvanceUIClock();
            UIFontFrameStart();
            UIAreaBeginFrame(LuaRect(0, 0, s_Width, s_Height));
            {
                StTMZone("ToolTipBegin");
                ProtectedSimpleCall(s_UILuaState, "ToolTip", "Begin");
            }
        }
    }
    else
    {
        StTMSection("Scrub");

        // Render the screen
        {
            StTMZone("FrameStartup");
            AdvanceScrubClock();
            UIFontFrameStart();
            UIAreaBeginFrame(LuaRect(0, 0, s_Width, s_Height));
        }
    }

    ProtectedSimpleCall(s_UILuaState, "Interactions", "ClearInactive");
    ProtectedSimpleCall(s_UILuaState, "Hotkeys", "Clear");

    // Main interface
    {
        StTMZone("MainInterface");
        ProtectedSimpleCall(s_UILuaState, NULL, "DoMainInterface");
        ProtectedSimpleCall(s_UILuaState, NULL, "ApplicationKeys");
    }

    // Modal interactions
    {
        StTMZone("Modal interactions and Dialogs");
        ProtectedSimpleCall(s_UILuaState, NULL, "DoModals");
    }
    // Tooltips
    {
        StTMZone("ToolTips");
        POINT mousePt;
        RECT clientRect;
        if (GetCursorPos(&mousePt) &&
            ScreenToClient(UIGetHWnd(), &mousePt) &&
            GetClientRect(UIGetHWnd(), &clientRect))
        {
            PushTableFunction(s_UILuaState, "ToolTip", "ProcessMouse");
            PushPoint(s_UILuaState, LuaPoint(mousePt.x, mousePt.y));
            lua_pcall(s_UILuaState, 1, 0, 0);
        }
    }

    // Modal interactions
    {
        StTMZone("Faders");
        ProtectedSimpleCall(s_UILuaState, NULL, "DoFaders");
    }


    // We can do the event pump in parallel
    CallPumpEvents();

    // Swap the buffers
    CallSwapBuffers();

    // We can do the event pump in parallel
    WaitPumpEvents();

    {
        StTMZone("FrameCleanup");
        UIAreaEndFrame();
        UIFontFrameEnd();
    }

    if (g_InScrubMode == false)
    {
        PushTimeScrub(g_GlobalClock);
    }

    if (edit::GetCurrentContainer())
    {
        StTMPlotInt(edit::GetCurrentContainer()->GetNumChildren(), "AnimStudio/UI/Nodes/Total(nodes)");

        real32 MinScrub, MaxScrub;
        GetScrubWindow(MinScrub, MaxScrub);
        StTMPlotSeconds(MinScrub, "AnimStudio/Minimum Scrub");
        StTMPlotSeconds(MaxScrub, "AnimStudio/Maximum Scrub");
    }

#if STATEMENT_TELEMETRY
    GrannyTelemetryFrameStats();
    GrannyTelemetryComplexStats();
    tmTick(g_context);
#endif
}


lua_State* GRANNY
UILuaState()
{
    return s_UILuaState;
}

HWND GRANNY
UIGetHWnd()
{
    return s_hWnd;
}



// =============================================================================
// Startup functions...
// =============================================================================
static bool
LoadWithIncludes(LuaFileEntry* Entries,
                 int32x const NumEntries,
                 int32x Which,
                 int Depth = 0)
{
    Assert(NumEntries > 0);
    Assert(Which >= 0 && Which < NumEntries);

    if (Entries[Which].Run)
        return true;
    Entries[Which].Run = 1;

    LuaFileEntry& Entry = Entries[Which];

    // This is NOT NULL TERMINATED!!!
    std::vector<char> TermBuffer;
    TermBuffer.insert(TermBuffer.begin(), Entry.VarPointer, Entry.VarPointer + Entry.VarLength);
    TermBuffer.push_back(0);

    char const* Require = strstr(&TermBuffer[0], "require \"");
    while (Require)
    {
        Require += strlen("require \"");
        char const* EndQuote = strchr(Require, '"');
        Assert(EndQuote);

        {for(int32x Idx = 0; Idx < NumEntries; ++Idx)
        {
            if (_strnicmp(Entries[Idx].VarName, Require, EndQuote - Require) == 0)
            {
                if (!LoadWithIncludes(Entries, NumEntries, Idx, Depth+1))
                    return false;

                break;
            }
        }}

        // Advance
        Require = strstr(Require, "require \"");
    }

    if (luaL_loadbuffer(s_UILuaState,
                        (char const*)Entries[Which].VarPointer,
                        Entries[Which].VarLength,
                        Entries[Which].VarName) ||
        lua_pcall(s_UILuaState, 0, 0, 0))
    {
        MessageBox(0, lua_tostring(s_UILuaState, -1), "Lua Error", MB_OK);
        Log2(ErrorLogMessage, ApplicationLogMessage, "Problem with load/pcall of %s: '%s'\n",
             Entries[Which].VarName, lua_tostring(s_UILuaState, -1));
        return false;
    }

    return true;
}

static bool
LoadLuaFileFromDisk(char const* Filename,
                    LuaFileEntry* Entry)
{
    file_reader* Reader = OpenFileReader(Filename);
    bool Success = false;
    if (Reader)
    {
        int32x NumBytes;
        if (GetReaderSize(Reader, &NumBytes))
        {
            char const* VarName = Filename + strlen(Filename);
            while (VarName > Filename)
            {
                if (VarName[-1] == '/' || VarName[-1] == '\\')
                    break;
                --VarName;
            }

            Entry->VarName    = VarName;
            Entry->VarPointer = GStateAllocArray(char, NumBytes);
            Entry->VarLength  = NumBytes;
            Entry->Run        = 0;

            if (Entry->VarPointer && ReadExactly(Reader, 0, NumBytes, (char*)Entry->VarPointer))
            {
               Success = true;
            }
        }

        CloseFileReader(Reader);
    }
    return Success;
}

static bool
CreateLUAEnvironment()
{
    // Make the module filename into a proper path and push it into lua...
    {
        char BaseFilename[512] = { 0 };
        HMODULE ExeModule = GetModuleHandle(0);
        GetModuleFileName(ExeModule, BaseFilename, sizeof(BaseFilename));
        char const* LastSlash = FindLastSlash(BaseFilename);
        if (LastSlash == 0)
            return false;
        {
            *((char*)LastSlash + 1) = 0;
            char* ForSlash = &BaseFilename[0];
            while (*ForSlash)
            {
                if (IsSlash(*ForSlash))
                    *ForSlash = '/';
                ++ForSlash;
            }
        }

        lua_pushstring(s_UILuaState, BaseFilename);
        lua_setglobal(s_UILuaState, "ExePathName");
    }

    // If we're not the official version, load our lua files from disk, and send them
    // through the normal path...
#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1
    int const NumLuaFiles = ArrayLength(LuaFileEntries);
#else
    int const NumLuaFiles = ArrayLength(LuaFileList);
    LuaFileEntry* LuaFileEntries = GStateAllocArray(LuaFileEntry, NumLuaFiles);
    memset(LuaFileEntries, 0, sizeof(LuaFileEntry) * NumLuaFiles);

    for (int Idx = 0; Idx < NumLuaFiles; ++Idx)
    {
        if (LoadLuaFileFromDisk(LuaFileList[Idx], &LuaFileEntries[Idx]) == false)
        {
            Log1(ErrorLogMessage, ApplicationLogMessage, "Unable to load '%s' from disk\n", LuaFileList[Idx]);
            return false;
        }
    }
#endif


    // Load the LUA libraries (note that LuaFileEntries might come from the header, or the
    // disk, in the above #ifdef
    {
        if (luaL_loadbuffer(s_UILuaState,
                            (char const*)LuaFileEntries[0].VarPointer,
                            LuaFileEntries[0].VarLength,
                            LuaFileEntries[0].VarName) ||
            lua_pcall(s_UILuaState, 0, 0, 0))
        {
            char const* LuaError = lua_tostring(s_UILuaState, -1);
            Log2(ErrorLogMessage, ApplicationLogMessage, "Problem with load/pcall of %s: '%s'\n",
                 LuaFileEntries[0].VarName, LuaError);
            return false;
        }

        bool Success = true;
        {for(int32x Idx = 1; Idx < NumLuaFiles; ++Idx)
        {
            if (!LoadWithIncludes(LuaFileEntries, NumLuaFiles, Idx))
            {
                Success = false;
                break;
            }
        }}
        if (!Success)
        {
            Log0(ErrorLogMessage, ApplicationLogMessage, "Failure loading lua source\n");
            return false;
        }
    }

#if !(defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1)
    // Discard our lua file array...
    for (int Idx = 0; Idx < NumLuaFiles; ++Idx)
    {
        GStateDeallocate(LuaFileEntries[Idx].VarPointer);
    }
    GStateDeallocate(LuaFileEntries);
#endif

    return true;
}


static bool
StartupOpenGL(HWND hwnd)
{
    Assert(s_hWnd == 0);
    s_hWnd = hwnd;
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof( pfd );
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    // Choose a pixel format, create a context and make it current
    // TODO: error check
    {
        HDC hDC = GetDC(hwnd);
        const int iFormat = ChoosePixelFormat( hDC, &pfd );
        SetPixelFormat( hDC, iFormat, &pfd );

        s_hGLRC = wglCreateContext( hDC );
        wglMakeCurrent( hDC, s_hGLRC);

        // Set the flip control
        {
            typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);

            PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
            wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
            if (wglSwapIntervalEXT)
                wglSwapIntervalEXT(1);
        }

        ReleaseDC(hwnd, hDC);
    }

    return true;
}

static void
ShutdownOpenGL()
{
    if (s_hGLRC == 0 || s_hWnd == 0)
        return;

    // Release the hglrc
    HDC hDC = GetDC(s_hWnd);
    wglMakeCurrent( hDC, 0);
    ReleaseDC(s_hWnd, hDC);

    wglDeleteContext(s_hGLRC);
    s_hGLRC = 0;

    // Release the handle to the window
    s_hWnd = 0;
}


#define RegisterUIFuncs(ZoneName)                   \
    if (!UI ## ZoneName ## _Register(s_UILuaState)) \
        return false

#define ShutdownUIFuncs(ZoneName) UI ## ZoneName ## _Shutdown()


bool GRANNY
GetApplicationInt(char const* ValName, uint32 Default, uint32* Value)
{
    HKEY hKey = 0;
    if (FAILED(RegOpenKeyEx(HKEY_CURRENT_USER, c_ApplicationKey, 0, KEY_READ, &hKey)))
    {
        *Value = Default;
        return false;
    }

    DWORD size = sizeof(Value);
    DWORD type;
    if (FAILED(RegQueryValueEx(hKey, ValName, 0, &type, (LPBYTE)Value, &size)) || type != REG_DWORD)
    {
        *Value = Default;
        return false;
    }

    RegCloseKey(hKey);
    return true;
}

bool GRANNY
SetApplicationInt(char const* ValName, uint32 NewVal)
{
    HKEY hRad;
    LONG error = RegCreateKeyEx(HKEY_CURRENT_USER, c_RADKey, 0, 0, 0, KEY_WRITE, 0, &hRad, 0);
    if (error != ERROR_SUCCESS)
    {
        StTMPrintfERR("unable to open hRad\n");
        return false;
    }

    HKEY hKey = 0;
    error = RegCreateKeyEx(HKEY_CURRENT_USER, c_ApplicationKey, 0, 0, 0, KEY_WRITE, 0, &hKey, 0);
    if (error != ERROR_SUCCESS)
    {
        StTMPrintfERR("Failed HKCU (%s)\n", c_ApplicationKey);
        RegCloseKey(hRad);
        return false;
    }

    error = RegSetValueEx(hKey, ValName, 0, REG_DWORD, (LPBYTE)&NewVal, sizeof(NewVal));
    if (error != ERROR_SUCCESS)
    {
        LPVOID lpMsgBuf;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
            (LPTSTR) &lpMsgBuf,
            0,
            NULL);
        // Process any inserts in lpMsgBuf.
        // ...
        StTMPrintfERR("Failed RegSetValueEx for %s %x (%s)\n",
                      StTMDynString(ValName),
                      hKey,
                      StTMDynString((char*)lpMsgBuf));
        // Free the buffer.
        LocalFree( lpMsgBuf );
    }

    RegCloseKey(hKey);
    RegCloseKey(hRad);

    return true;
}

bool GRANNY
GetApplicationString(char const* ValName, char const* Default, string* Value)
{
    HKEY hKey = 0;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, c_ApplicationKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        *Value = Default;
        return false;
    }

    vector<char> HugeBuffer(1 << 16);
    DWORD size = HugeBuffer.size();

    DWORD dataType;
    HRESULT retval = RegQueryValueEx(hKey, ValName, 0, &dataType, (LPBYTE)&HugeBuffer[0], &size);
    if (FAILED(retval) || dataType != REG_SZ)
    {
        StTMPrintf("Failed to get key: %s", StTMDynString(ValName));
        *Value = Default;
        return false;
    }
    else
    {
        StTMPrintf("Parsing into RecentFiles");
        StTMPrintf("  \"%s\"", StTMDynString(&HugeBuffer[0]));
        *Value = &HugeBuffer[0];
        return true;
    }
}

bool GRANNY
SetApplicationString(char const* ValName, char const* String)
{
    HKEY hKey = 0;
    if (FAILED(RegOpenKeyEx(HKEY_CURRENT_USER, c_ApplicationKey, 0, KEY_WRITE, &hKey)))
    {
        return false;
    }

    RegSetValueEx(hKey, ValName, 0, REG_SZ, (LPBYTE)String, strlen(String) + 1);
    RegCloseKey(hKey);

    return true;
}

static int
App_SetPersistentString(lua_State* L)
{
    char const* ValName = lua_tostring(L, -2);
    char const* Value   = lua_tostring(L, -1);

    if (ValName && Value)
        return LuaBoolean(L, SetApplicationString(ValName, Value));
    else
        return LuaBoolean(L, false);
}

static int
App_GetPersistentString(lua_State* L)
{
    char const* ValName    = lua_tostring(L, -2);
    char const* DefaultVal = lua_tostring(L, -1);

    if (ValName == 0)
        return LuaString(L, DefaultVal);

    std::string Value;
    if (!GetApplicationString(ValName, DefaultVal, &Value))
    {
        if (DefaultVal)
            SetApplicationString(ValName, DefaultVal);
    }

    return LuaString(L, Value.c_str());
}


static int
App_GetPersistentInt(lua_State* L)
{
    char const* ValName    = lua_tostring(L, -2);
    int         DefaultVal = lua_tointeger(L, -1);

    if (ValName == 0)
    {
        lua_pushinteger(L, DefaultVal);
    }
    else
    {
        uint32 Value;
        if (!GetApplicationInt(ValName, DefaultVal, &Value))
        {
            // Try to stuff it back...
            SetApplicationInt(ValName, DefaultVal);
        }

        lua_pushinteger(L, (int)Value);
    }

    return 1;
}

static int
App_SetPersistentInt(lua_State* L)
{
    char const* ValName = lua_tostring(L, -2);
    int         NewVal  = lua_tointeger(L, -1);

    if (ValName == 0)
    {
        lua_pushboolean(L, false);
    }
    else
    {
        lua_pushboolean(L, SetApplicationInt(ValName, NewVal));
    }

    return 1;
}

static int
App_GetClipboardString(lua_State* L)
{
    if (OpenClipboard(UIGetHWnd()) == FALSE)
        return LuaNil(L);

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData)
    {
        // Make a copy of this so we can modify it to clear out any Carriage returns...
        char* String = (char*)GlobalLock(hData);
        string NoCR = String;
        GlobalUnlock( hData );

        string::size_type pos = NoCR.find('\r');
        if (pos != string::npos)
            NoCR.erase(pos);
        pos = NoCR.find('\n');
        if (pos != string::npos)
            NoCR.erase(pos);

        lua_pushstring(L, NoCR.c_str());
    }
    else
    {
        lua_pushnil(L);
    }

    CloseClipboard();
    return 1;
}

static int
App_SetClipboardString(lua_State* L)
{
    if (OpenClipboard(UIGetHWnd()) == FALSE)
        return LuaBoolean(L, false);
    EmptyClipboard();

    char const* String = lua_tostring(L, -1);
    size_t Size = strlen(String) + 1;

    HGLOBAL hResult = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT, Size);
    if (hResult == 0)
    {
        CloseClipboard();
        return LuaBoolean(L, false);
    }

    char* StringCopy = (char*)GlobalLock(hResult);
    strcpy(StringCopy, String);
    GlobalUnlock(hResult);

    HANDLE hCD = SetClipboardData(CF_TEXT, hResult);
    if (hCD == 0)
    {
        CloseClipboard();
        GlobalFree(hResult);
        return LuaBoolean(L, false);
    }

    return LuaBoolean(L, CloseClipboard() == TRUE);
}

static int
App_GetShiftCtrlAlt(lua_State* L)
{
    PushShiftControlAlt(L);
    return 3;
}


static bool
App_Register(lua_State* State)
{
    lua_register(State, "App_GetPersistentInt",  App_GetPersistentInt);
    lua_register(State, "App_SetPersistentInt",  App_SetPersistentInt);

    lua_register(State, "App_GetPersistentString",  App_GetPersistentString);
    lua_register(State, "App_SetPersistentString",  App_SetPersistentString);
    
    lua_register(State, "App_GetClipboardString", App_GetClipboardString);
    lua_register(State, "App_SetClipboardString", App_SetClipboardString);

    lua_register(State, "App_GetShiftCtrlAlt",    App_GetShiftCtrlAlt);

    return true;
}


void GRANNY
FadePrintf(LuaColor const& Color,
           float FadeTime,
           char const* Fmt, ...)
{
    char buffer[1024];

    va_list args;
    va_start(args, Fmt);
    _vsnprintf(buffer, sizeof(buffer), Fmt, args);

    PushTableFunction(s_UILuaState, 0, "PushFaderString");
    lua_pushstring(s_UILuaState, buffer);
    PushColor(s_UILuaState, Color);
    lua_pushnumber(s_UILuaState, FadeTime);
    lua_pcall(s_UILuaState, 3, 0, 0);
}


static bool
StartupLUA()
{
    Assert(s_UILuaState == 0);

    if ((s_UILuaState = lua_open()) == 0)
        return false;

    // For now, load in everything.  We could slice this down a bit...
    luaL_openlibs(s_UILuaState);

    // Register the extension functions
    {
        App_Register(s_UILuaState);
        Edit_Register(s_UILuaState);
        UICharacterDrawing_Register(s_UILuaState);
        RegisterUIFuncs(Area);
        RegisterUIFuncs(Bitmap);
        RegisterUIFuncs(Drawing);
        RegisterUIFuncs(Font);
        RegisterUIFuncs(EditField);
        RegisterUIFuncs(NodeDrawing);
        RegisterUIFuncs(XInput);
    }

    if (!CreateLUAEnvironment())
        return false;

    return true;
}

static bool
LoadUserMacros()
{
    if (!s_UILuaState)
        return false;

    // todo: platform factor
    // Get the module path and look in that directory for the user_macros.lua file...
    char EXEPath[MAX_PATH];
    UINT ResultLen = GetModuleFileNameA((HINSTANCE)GetModuleHandle(0), EXEPath, SizeOf(EXEPath));
    if (ResultLen == 0 || (ResultLen == sizeof(EXEPath) && GetLastError() == ERROR_INSUFFICIENT_BUFFER))
    {
        return false;
    }

    char MacroPath[MAX_PATH];
    strcpy(MacroPath, EXEPath);
    PathRemoveFileSpec(MacroPath);
    PathAppend(MacroPath, "user_macros.lua");

    // If the version in the exe directory doesn't work, try the local directory...
    if (!PathFileExists(MacroPath) && PathFileExists("user_macros.lua"))
        strcpy(MacroPath, "user_macros.lua");

    if (luaL_loadfile(s_UILuaState, MacroPath) == 0)
    {
        if (lua_pcall(s_UILuaState, 0, 0, 0))
        {
            char const* LuaError = lua_tostring(s_UILuaState, -1);
            Log2(ErrorLogMessage, ApplicationLogMessage, "Problem with load/pcall of user_macros.lua: '%s'\n",
                 MacroPath, LuaError);

            return false;
        }
    }

    return true;
}



static void
ShutdownLUA()
{
    if (!s_UILuaState)
        return;

    // Register the extension functions
    {
        // ShutdownUIFuncs(Area);
        ShutdownUIFuncs(NodeDrawing);
        ShutdownUIFuncs(Bitmap);
        // ShutdownUIFuncs(Drawing);
        ShutdownUIFuncs(Font);
        // ShutdownUIFuncs(Preferences);
    }

    lua_close(s_UILuaState);
    s_UILuaState = 0;
}

