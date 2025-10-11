// ========================================================================
// $File: //jeffr/granny_29/statement/statement.cpp $
// $DateTime: 2012/07/23 10:35:33 $
// $Change: 38478 $
// $Revision: #6 $
//
// $Notice: $
// ========================================================================
#include <io.h>
#include <winsock2.h>
#include <shellapi.h>

#include "statement.h"
#include "statement_editstate.h"

#include "ui_core.h"
#include "ui_font.h"
#include "luautils.h"

#include "granny_assert.h"
#include "granny_memory.h"
#include "granny_parameter_checking.h"
#include "granny_version.h"


#include "gstate_token_context.h"
#include "gstate_state_machine.h"
#include "gstate_blend_graph.h"
#include "gstate_anim_source.h"

#include "gstate_character_info.h"
#include "gstate_character_instance.h"
#include "statement_undostack.h"
#include "statement_xinput.h"

#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL
#include "rad_dumphandler.h"
#endif

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"


// Should always be the last header included
#include "granny_cpp_settings.h"

#define SubsystemCode Undefined_LogMessage
USING_GSTATE_NAMESPACE;
USING_GRANNY_NAMESPACE;

// Look below, if you please...
static LRESULT WINAPI WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


#if STATEMENT_TELEMETRY

#include "telemetry.h"

TmU8*    g_arena = 0;        // Declare this somewhere in your global scope
HTELEMETRY g_context;   // Declare this somewhere in your global scope

static void InitTelemetry()
{
    WSAData data;
    WSAStartup(MAKEWORD(2,2), &data);

    //start_network();        // Do whatever is necessary to initialize your network layer, e.g. WSANetwork
    tmLoadTelemetry(false);  // Set up DLLs on Windows
    tmStartup();         // Only call this once

    const int ARENA_SIZE = 8 * 1024 * 1024; // How much memory you want Telemetry to use
    g_arena = (TmU8*)malloc( ARENA_SIZE ); // Or new, whatever...can even make this a static if you want

    // For shipping code check returns, etc.
    tmInitializeContext( &g_context, (void*)g_arena, ARENA_SIZE );

    if (tmOpen(g_context, "GrannyAnimationStudio",
                __DATE__ __TIME__, "localhost",
                TMCT_TCP, 9180, TMOF_DEFAULT, 1000 ) != TM_OK)
    {
        // Couldn't connect!
        return;
    }

    GrannyUseTelemetry(g_context);
}


static void ShutdownTelemetry()
{
    tmClose(g_context);
    tmShutdownContext(g_context);
    tmShutdown();
}

#endif // STATEMENT_TELEMETRY

GRANNY_CALLBACK(void) LogCallback(granny_log_message_type Type,
                                  granny_log_message_origin Origin,
                                  char const* File, granny_int32x Line,
                                  char const * Message,
                                  void* /*UserData*/)
{
    Assert(Message);

    // Route to the debug output.  Note that Granny already routes to Telemetry
    // internally...
    char Buffer[1024];
    _snprintf(Buffer, 1024, "  %s(%d): %s\n", File, Line, Message);
    OutputDebugString(Buffer);
}

//-----------------------------------------------------------------------------
// Dan's Window Loop Get!
//-----------------------------------------------------------------------------
int WINAPI
WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmd, int nShowCmd)
{
    int Result = EXIT_SUCCESS;

    // Surround the whole program in a guard to catch crashes, but only for official
    // builds from RAD.
#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL && !DEBUG

    // Setup our dump parameters
    rad_dump_params DumpParams;
    RADDumpDefaultParameters(&DumpParams);

    DumpParams.AppName       = "AnimationStudio";
    DumpParams.VersionString = ProductVersion;
    DumpParams.SupportEmail  = ProductSupportAddress;
#if DEBUG
    DumpParams.Comment = "Debug Version";
#else
    DumpParams.Comment = "Release Version";
#endif

    // Full dump argument passed to the app?
    if (strstr(cmd, "-fulldump") != 0)
        DumpParams.UseFullMemoryDump = true;

    RADDumpSetParameters(&DumpParams);

    __try
#endif
    {
        // Start up xinput if possible
        InitXInput();

#if STATEMENT_TELEMETRY
        InitTelemetry();
        tmThreadName(g_context, GetCurrentThreadId(), "AnimStudio Main");
#endif // STATEMENT_TELEMETRY

        MSG msg;

#if DEBUG
        granny_log_callback NewCallback;
        NewCallback.Function = LogCallback;
        NewCallback.UserData = NULL;
        GrannySetLogCallback(&NewCallback);

        void* AllocationCheck = GrannyBeginAllocationCheck();

        { // open a console for printf
            AllocConsole();

            HANDLE hOutput = CreateFile("CONOUT$", GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
            SetStdHandle(STD_OUTPUT_HANDLE, hOutput);

            int fd = _open_osfhandle((INT_PTR)hOutput, 0);
            FILE* fp = _fdopen(fd, "w");
            *stdout = *fp;
            *stderr = *fp;
            setvbuf(stdout, NULL, _IONBF, 0);
        }
#endif // DEBUG

        // Register the window class
        WNDCLASSEX wndclass = { sizeof(wndclass) };
        wndclass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        wndclass.lpfnWndProc = (WNDPROC)WindowProc;
        wndclass.hInstance = hinst;
        wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
        wndclass.hbrBackground = NULL;
        wndclass.lpszMenuName = NULL;
        wndclass.lpszClassName = "GrannyAnimationStudio";
        RegisterClassEx(&wndclass);

        // Window rect: TODO (from memory)
        RECT R;
        uint32 Style;
        DWORD XPos = (DWORD)CW_USEDEFAULT;
        DWORD YPos = (DWORD)CW_USEDEFAULT;
        if (edit::RecallWindowPos(&R, &Style))
        {
            XPos = R.left;
            YPos = R.top;
        }

        HWND hWnd = CreateWindow("GrannyAnimationStudio", "Granny Animation Studio",
                                 Style,
                                 XPos, YPos,
                                 R.right - R.left,
                                 R.bottom - R.top,
                                 NULL, NULL, hinst, 0);

        if (!UIStartup(hWnd))
        {
            return EXIT_FAILURE;
        }

        edit::CreateDefaultFile();

        // Try to call user macro startup function
        {
            lua_State* L = UILuaState();

            PushTableFunction(L, "_G", "UserStartupCallback");
            if (lua_isfunction(L, -1))
            {
                lua_pushstring(L, cmd);
                if (lua_pcall(L, 1, 0, 0))
                {
                    // Failed to call user macro startup, but ignore for the moment...
                    lua_pop(L, 1);
                }
            }
            else
            {
                lua_pop(L, 1);
            }
        }

        ShowWindow(hWnd, nShowCmd);
        if (Style & WS_MAXIMIZE)
            ShowWindow(hWnd, SW_MAXIMIZE);

        // Accept dragged gsf files...
        DragAcceptFiles(hWnd, true);

        for (;;)
        {
            if (PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE))
            {
                StTMEnterIdle("MessagePump");

                int MsgState = GetMessage(&msg, 0, 0, 0);
                if (MsgState == -1 || MsgState == 0)
                {
                    // Error or WM_QUIT
                    break;
                }

                TranslateMessage(&msg);
                DispatchMessage(&msg);

                StTMLeave();
            }
            else
            {
                UIInteract();
                Sleep(0);
            }
        }

        UIShutdown();
        edit::ResetAll(true);
        ClearUndoStack();
        token_context::DestroyGlobalContext();

#if DEBUG
        GrannyEndAllocationCheck(AllocationCheck);
#endif

#if STATEMENT_TELEMETRY
        ShutdownTelemetry();
#endif // STATEMENT_TELEMETRY

        ShutdownXInput();
    }
#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL && !DEBUG
    __except(RADDumpWritingHandler(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER)
    {
        Assert(!"Turn on exception handling");
        Result = EXIT_FAILURE;
    }
#endif

    return Result;
}


void
AcceptDraggedFile(char const* FilenameBuffer)
{
    if (edit::CheckForSaveModified())
        edit::LoadFromFilename(FilenameBuffer);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static LRESULT WINAPI
WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int MouseCaptureCount = 0;
    Assert(MouseCaptureCount >= 0);

    switch (msg)
    {
        case WM_SIZE:
        {
            StTMZone("WM_SIZE");
            UINoteSize(LOWORD(lParam), HIWORD(lParam));

            // Clear all the interactions so they can note the new size
            lua_State* State = UILuaState();
            if (State)
            {
                ProtectedSimpleCall(State, "Interactions", "ClearInactive");
                UIInteract();
            }
        } break;

        case WM_ACTIVATE:
        {
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                UILossOfFocusEvent();
            }
        } break;

        case WM_MOUSEMOVE:
        {
            POINTS p = MAKEPOINTS(lParam);
            UIMouseMove(p.x, p.y);
        } break;

        case WM_MOUSEWHEEL:
        {
            POINT p;
            p.x = LOWORD(lParam);
            p.y = HIWORD(lParam);
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            ScreenToClient(hWnd, &p);
            UIMouseWheel(p.x, p.y, zDelta);
        } break;

        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
        {
            bool Down = (msg == WM_SYSKEYDOWN);
            switch (wParam)
            {
                case VK_LEFT:
                case VK_RIGHT:
                case VK_UP:
                case VK_DOWN:
                case VK_HOME:
                case VK_END:
                    UIDirKey(wParam, Down);
                    break;

                case VK_BACK:
                case VK_DELETE:
                case VK_RETURN:
                case VK_ESCAPE:
                    UICtrlKey(wParam, Down);
                    break;

                case VK_LWIN:
                case VK_RWIN:
                    break;

                case VK_F1:
                case VK_F2:
                case VK_F3:
                case VK_F4:
                case VK_F5:
                case VK_F6:
                case VK_F7:
                case VK_F8:
                case VK_F9:
                case VK_F10:
                case VK_F11:
                case VK_F12:
                case VK_F13:
                case VK_F14:
                case VK_F15:
                case VK_F16:
                case VK_F17:
                case VK_F18:
                case VK_F19:
                case VK_F20:
                case VK_F21:
                case VK_F22:
                case VK_F23:
                case VK_F24:
                    UIFnKey(wParam - VK_F1 + 1, Down);
                    break;
            }
        } break;

        case WM_KEYUP:
        case WM_KEYDOWN:
        {
            bool Down = (msg == WM_KEYDOWN);
            switch (wParam)
            {
                case VK_LEFT:
                case VK_RIGHT:
                case VK_UP:
                case VK_DOWN:
                case VK_HOME:
                case VK_END:
                    UIDirKey(wParam, Down);
                    break;

                case VK_BROWSER_BACK:
                case VK_BROWSER_FORWARD:
                case VK_MEDIA_STOP:
                case VK_MEDIA_PLAY_PAUSE:
                    UICtrlKey(wParam, Down);
                    break;

                case VK_BACK:
                case VK_DELETE:
                case VK_RETURN:
                case VK_ESCAPE:
                    UICtrlKey(wParam, Down);
                    break;

                case VK_LWIN:
                case VK_RWIN:
                    break;

                case VK_F1:
                case VK_F2:
                case VK_F3:
                case VK_F4:
                case VK_F5:
                case VK_F6:
                case VK_F7:
                case VK_F8:
                case VK_F9:
                case VK_F10:
                case VK_F11:
                case VK_F12:
                case VK_F13:
                case VK_F14:
                case VK_F15:
                case VK_F16:
                case VK_F17:
                case VK_F18:
                case VK_F19:
                case VK_F20:
                case VK_F21:
                case VK_F22:
                case VK_F23:
                case VK_F24:
                    UIFnKey(wParam - VK_F1 + 1, Down);
                    break;

                default:
                {
                    BYTE keyState[256];
                    char Result[2];
                    GetKeyboardState(keyState);

                    // Kill the control char so we get ascii in all cases, we'll handle control explicitly...
                    // todo: not the greatest here.
                    keyState[17] = 0;

                    int NumChars = ToAsciiEx(wParam,
                                             MapVirtualKeyEx(wParam, 0, GetKeyboardLayout(0)),
                                             keyState,
                                             (LPWORD)&Result[0],
                                             0, 0);
                    if (NumChars == 0)
                    {
                        StTMPrintf("No translation for %x\n", wParam);
                    }
                    else if (NumChars == 1)
                    {
                        UINormalKey(Result[0], Down);
                    }
                    else if (NumChars == 2)
                    {
                        UINormalKey(Result[0], Down);
                        UINormalKey(Result[1], Down);
                    }
                } break;
            }
        } break;

        case WM_LBUTTONUP:
        {
            if (MouseCaptureCount > 0)
            {
                --MouseCaptureCount;
                ReleaseCapture();
            }

            POINTS p = MAKEPOINTS(lParam);
            UIMouseEvent(p.x, p.y, eLeftUp);
        } break;

        case WM_LBUTTONDOWN:
        {
            if (MouseCaptureCount++ == 0)
                SetCapture(hWnd);

            POINTS p = MAKEPOINTS(lParam);
            UIMouseEvent(p.x, p.y, eLeftDown);
        } break;

        case WM_RBUTTONUP:
        {
            if (MouseCaptureCount > 0)
            {
                --MouseCaptureCount;
                ReleaseCapture();
            }

            POINTS p = MAKEPOINTS(lParam);
            UIMouseEvent(p.x, p.y, eRightUp);
        } break;

        case WM_RBUTTONDOWN:
        {
            if (MouseCaptureCount++ == 0)
                SetCapture(hWnd);

            POINTS p = MAKEPOINTS(lParam);
            UIMouseEvent(p.x, p.y, eRightDown);
        } break;

        case WM_MBUTTONUP:
        {
            if (MouseCaptureCount > 0)
            {
                --MouseCaptureCount;
                ReleaseCapture();
            }

            POINTS p = MAKEPOINTS(lParam);
            UIMouseEvent(p.x, p.y, eMiddleUp);
        } break;

        case WM_MBUTTONDOWN:
        {
            if (MouseCaptureCount++ == 0)
                SetCapture(hWnd);

            POINTS p = MAKEPOINTS(lParam);
            UIMouseEvent(p.x, p.y, eMiddleDown);
        } break;

        case WM_XBUTTONUP:
        {
            int which = HIWORD(wParam);
            if (which == 1 || which == 2)
            {
                if (MouseCaptureCount > 0)
                {
                    --MouseCaptureCount;
                    ReleaseCapture();
                }

                POINTS p = MAKEPOINTS(lParam);
                UIMouseEvent(p.x, p.y, which == 1 ? eBackUp : eForwardUp);
            }
        } break;

        case WM_XBUTTONDOWN:
        {
            int which = HIWORD(wParam);
            if (which == 1 || which == 2)
            {
                if (MouseCaptureCount++ == 0)
                    SetCapture(hWnd);

                POINTS p = MAKEPOINTS(lParam);
                UIMouseEvent(p.x, p.y, which == 1 ? eBackDown : eForwardDown);
            }
        } break;

        case WM_CLOSE:
        {
            edit::StoreWindowPos();
            if (edit::CheckForSaveModified())
                PostQuitMessage(0);
            else
                return 0;
        } break;

        case WM_DROPFILES:
        {
            HDROP DropHandle = (HDROP)wParam;

            char FilenameBuffer[1024];
            int NumFilesDropped = DragQueryFile(DropHandle, 0xFFFFFFFF, FilenameBuffer, sizeof(FilenameBuffer));
            for (int i = 0; i < NumFilesDropped; ++i)
            {
                DragQueryFile(DropHandle, i, FilenameBuffer, sizeof(FilenameBuffer));

                AcceptDraggedFile(FilenameBuffer);
            }
            DragFinish(DropHandle);
        } break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Some notes for the RAD build system
/* @cdep pre
   $requires(stb_image.c)
*/
