// ========================================================================
// $Notice: $
// ========================================================================
#include "statement_xinput.h"
#include "statement_editstate.h"
#include "statement_undostack.h"

#include "granny_data_type_conversion.h"
#include "granny_data_type_definition.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_string.h"

#include "luautils.h"
#include "ui_controls.h"
#include "ui_area.h"

#include "gstate_event_source.h"
#include "gstate_parameters.h"
#include "gstate_state_machine.h"
#include "gstate_token_context.h"

#include <vector>
#include <string>
#include <deque>


USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;
using namespace std;

// Handle to the library
#define BAIL_IF_UNLOADED(...) if (s_XInputLibrary == 0) return __VA_ARGS__;
static HMODULE s_XInputLibrary = 0;

// We only use 3 functions here...
#define GetXFn(Name)                                                                            \
    do {                                                                                        \
        XFn ## Name = (XInput ## Name ## _t *)GetProcAddress(s_XInputLibrary, "XInput" #Name);  \
        if ((XFn ## Name) == 0)                                                                 \
        {                                                                                       \
            ReleaseLibrary();                                                                   \
            return;                                                                             \
        }                                                                                       \
    } while (false)

#define XFn(Name) (*XFn ## Name)

typedef DWORD WINAPI XInputGetState_t(DWORD dwUserIndex, XINPUT_STATE* pState);
typedef DWORD WINAPI XInputSetState_t(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
typedef DWORD WINAPI XInputGetCapabilities_t(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);
static XInputGetState_t*        XFnGetState        = 0;
static XInputSetState_t*        XFnSetState        = 0;
static XInputGetCapabilities_t* XFnGetCapabilities = 0;


// This is the previous input state for edge detection.  It's reset to the default every
// time the input scheme is changed....
struct ControllerState
{
    XINPUT_STATE XState;
    WORD         ButtonEdges;     // detected by using the previous state

    BYTE LeftLastTrigger;
    BYTE RightLastTrigger;

    // Sets defaults...
    ControllerState()
    {
        ZeroStructure(XState);
        ButtonEdges = 0;
        LeftLastTrigger = 0;
        RightLastTrigger = 0;
    }
};

static ControllerState s_LastInput;


// Xinput defines 16 buttons
static const int c_NumButtons = 14;
static const char* s_ButtonNames[c_NumButtons] =
{
    "DPad Up",
    "DPad Down",
    "DPad Left",
    "DPad Right",
    "Start",
    "Back",
    "Left Thumb",
    "Right Thumb",
    "Left Shoulder",
    "Right Shoulder",
    "A",
    "B",
    "X",
    "Y"
};
static const uint16 s_ButtonMasks[c_NumButtons] =
{
    XINPUT_GAMEPAD_DPAD_UP,
    XINPUT_GAMEPAD_DPAD_DOWN,
    XINPUT_GAMEPAD_DPAD_LEFT,
    XINPUT_GAMEPAD_DPAD_RIGHT,
    XINPUT_GAMEPAD_START,
    XINPUT_GAMEPAD_BACK,
    XINPUT_GAMEPAD_LEFT_THUMB,
    XINPUT_GAMEPAD_RIGHT_THUMB,
    XINPUT_GAMEPAD_LEFT_SHOULDER,
    XINPUT_GAMEPAD_RIGHT_SHOULDER,
    XINPUT_GAMEPAD_A,
    XINPUT_GAMEPAD_B,
    XINPUT_GAMEPAD_X,
    XINPUT_GAMEPAD_Y,
};


static const int c_NumAxes = 6;
static const char* s_AxisNames[c_NumAxes] =
{
    "Left Trigger",
    "Right Trigger",
    "LeftStick X",
    "LeftStick Y",
    "RightStick X",
    "RightStick Y",
};



struct button_action_encoding
{
    granny_int32  Action;
    variant       NodeToken;
    granny_int32  Edge;
    char*         Name;
};
data_type_definition ButtonActionEncodingType[] =
{
    { Int32Member,            "Action" },
    { VariantReferenceMember, "NodeToken" },
    { Int32Member,            "Edge" },
    { StringMember,           "Name" },
    { EndMember },
};

struct axis_action_encoding
{
    variant       NodeToken;
    granny_int32  Edge;
};
data_type_definition AxisActionEncodingType[] =
{
    { VariantReferenceMember, "NodeToken" },
    { Int32Member,            "Edge" },
    { EndMember },
};


struct scheme_encoding
{
    char* Name;
    button_action_encoding Buttons[c_NumButtons];
    axis_action_encoding Axes[c_NumAxes];
};

data_type_definition SchemeEncodingType[] =
{
    { StringMember, "Name" },
    { InlineMember, "Buttons", ButtonActionEncodingType, c_NumButtons },
    { InlineMember, "Axes",    AxisActionEncodingType,   c_NumAxes    },
    { EndMember }
};



struct ButtonAction
{
    enum EActionType {
        eNone             = 0, // Nothing configured, no action
        eRequestState     = 1, // RequestChangeToState on the owner of the specified node
        eForceState       = 2, // ForceChangeToState on the specified node
        eTransitionByName = 3, // StartTransitionByName
        eSetParameter     = 4, // Parameter to 0|1
        eTriggerEvent     = 5, // TriggerEvent

        eNumButtonActions
    };

    EActionType   action;
    variant       NodeToken;      // We look this up every time
    granny_int32x Edge;
    std::string   Name;

    ButtonAction();
    void TakeAction(real32 AtT, real32 DeltaT, bool Down, bool Edge);
    void NoteEdgeDelete(node*, int Edge);
    void NoteNodeDelete(node*);
};

static const char* s_ActionNames[ButtonAction::eNumButtonActions] =
{
    "None",
    "RequestState",     // RequestChangeToState on the owner of the specified node
    "ForceState",       // ForceChangeToState on the specified node
    "TransitionByName", // StartTransitionByName
    "SetParameter",     // Parameter to 0|1
    "TriggerEvent",     // TriggerEvent
};

struct AxisAction
{
    variant       NodeToken;      // We look this up every time
    granny_int32x Edge;

    AxisAction();
    void TakeAction(real32 Value);
    void NoteEdgeDelete(node*, int Edge);
    void NoteNodeDelete(node*);
};

struct ControllerScheme
{
    std::string Name;

    ButtonAction buttons[c_NumButtons];
    AxisAction axes[c_NumAxes];

    void ToEncoding(scheme_encoding*);
    void FromEncoding(scheme_encoding const&);
};

void
ControllerScheme::ToEncoding(scheme_encoding* Encoding)
{
    Encoding->Name = CloneString(Name.c_str());

    for (int Idx = 0; Idx < c_NumButtons; ++Idx)
    {
        Encoding->Buttons[Idx].Action = buttons[Idx].action;

        Encoding->Buttons[Idx].NodeToken = buttons[Idx].NodeToken;
        Encoding->Buttons[Idx].Edge      = buttons[Idx].Edge;
        Encoding->Buttons[Idx].Name      = CloneString(buttons[Idx].Name.c_str());
    }

    for (int Idx = 0; Idx < c_NumAxes; ++Idx)
    {
        Encoding->Axes[Idx].NodeToken = axes[Idx].NodeToken;
        Encoding->Axes[Idx].Edge      = axes[Idx].Edge;
    }
}

void
ControllerScheme::FromEncoding(scheme_encoding const& Encoding)
{
    Name = Encoding.Name;

    for (int Idx = 0; Idx < c_NumButtons; ++Idx)
    {
        buttons[Idx].action = (ButtonAction::EActionType)Encoding.Buttons[Idx].Action;

        buttons[Idx].NodeToken = Encoding.Buttons[Idx].NodeToken;
        buttons[Idx].Edge = Encoding.Buttons[Idx].Edge;
        buttons[Idx].Name = Encoding.Buttons[Idx].Name;
    }

    for (int Idx = 0; Idx < c_NumAxes; ++Idx)
    {
        axes[Idx].NodeToken = Encoding.Axes[Idx].NodeToken;
        axes[Idx].Edge      = Encoding.Axes[Idx].Edge;
    }
}


static std::vector<ControllerScheme> s_Schemes;
static int s_CurrentScheme = -1;


// =============================================================================
// =============================================================================

static void
ReleaseLibrary()
{
    XFnGetState        = 0;
    XFnSetState        = 0;
    XFnGetCapabilities = 0;

    if (s_XInputLibrary != 0)
    {
        FreeLibrary(s_XInputLibrary);
        s_XInputLibrary = 0;
    }
}

void GRANNY
InitXInput()
{
    if (s_XInputLibrary == 0)
    {
        s_XInputLibrary = LoadLibrary(XINPUT_DLL);
        if (s_XInputLibrary)
        {
            // These bail if they fail and release the dll
            GetXFn(GetState);
            GetXFn(SetState);
            GetXFn(GetCapabilities);
        }
    }
}

void GRANNY
ShutdownXInput()
{
    if (s_XInputLibrary != 0)
    {
        s_XInputLibrary = 0;
    }
}

static bool
ControllerAttached()
{
    BAIL_IF_UNLOADED(false);

    // We only look at controller 0
    XINPUT_CAPABILITIES xic;
    DWORD Ret = XFn(GetCapabilities)(0, XINPUT_DEVTYPE_GAMEPAD, &xic);

    return (Ret == ERROR_SUCCESS);
}

static bool
GetControllerState(ControllerState*       Output,
                   ControllerState const* Previous /* = 0*/)
{
    BAIL_IF_UNLOADED(false);

    // Keep separate until we have resolved the Previous stuff, might point to Output
    ControllerState NewState;

    if (XFn(GetState)(0, &NewState.XState) != ERROR_SUCCESS)
        return false;

    // Handle the edges
    if (Previous != 0)
    {
        NewState.ButtonEdges =
            (NewState.XState.Gamepad.wButtons ^ Previous->XState.Gamepad.wButtons);

        NewState.LeftLastTrigger  = Previous->XState.Gamepad.bLeftTrigger;
        NewState.RightLastTrigger = Previous->XState.Gamepad.bRightTrigger;
    }

    *Output = NewState;
    return true;
}


#define MAP_UBYTE(x) ((x) / 255.0f)
#define MAP_SHORT(x) ((((x) / 32767.0f) + 1) * 0.5f)

#define EXCEEDS_DEAD(x) (abs(x) > 3500)

void GRANNY
ProcessXInput(real32 AtT, real32 DeltaT)
{
    BAIL_IF_UNLOADED();

    if (s_CurrentScheme == -1)
        return;

    if (GetControllerState(&s_LastInput, &s_LastInput) == false)
        return;

    ControllerScheme& Scheme = s_Schemes[s_CurrentScheme];

    // Run through the controller scheme...
    for (int Idx = 0; Idx < c_NumButtons; ++Idx)
    {
        Scheme.buttons[Idx].TakeAction(AtT, DeltaT,
                                       (s_LastInput.XState.Gamepad.wButtons & s_ButtonMasks[Idx]) != 0,
                                       (s_LastInput.ButtonEdges & s_ButtonMasks[Idx]) != 0);
    }

    if (s_LastInput.XState.Gamepad.bLeftTrigger != s_LastInput.LeftLastTrigger)
        Scheme.axes[0].TakeAction(MAP_UBYTE(s_LastInput.XState.Gamepad.bLeftTrigger));

    if (s_LastInput.XState.Gamepad.bRightTrigger != s_LastInput.RightLastTrigger)
        Scheme.axes[1].TakeAction(MAP_UBYTE(s_LastInput.XState.Gamepad.bRightTrigger));

    // Dead zone the thumbs...
    if (EXCEEDS_DEAD(s_LastInput.XState.Gamepad.sThumbLX))
        Scheme.axes[2].TakeAction(MAP_SHORT(s_LastInput.XState.Gamepad.sThumbLX));
    if (EXCEEDS_DEAD(s_LastInput.XState.Gamepad.sThumbLY))
        Scheme.axes[3].TakeAction(MAP_SHORT(s_LastInput.XState.Gamepad.sThumbLY));
    if (EXCEEDS_DEAD(s_LastInput.XState.Gamepad.sThumbRX))
        Scheme.axes[4].TakeAction(MAP_SHORT(s_LastInput.XState.Gamepad.sThumbRX));
    if (EXCEEDS_DEAD(s_LastInput.XState.Gamepad.sThumbRY))
        Scheme.axes[5].TakeAction(MAP_SHORT(s_LastInput.XState.Gamepad.sThumbRY));
}




// =============================================================================
ButtonAction::ButtonAction()
  : action(eNone),
    Edge(-1)
{
    NodeToken.Type = 0;
    NodeToken.Object = 0;
}

void ButtonAction::TakeAction(real32 AtT,
                              real32 DeltaT,
                              bool Down,
                              bool ButtonEdge)
{
    bool const Pressed  = (Down && ButtonEdge);
    // bool const Released = (!Down && ButtonEdge);

    token_context* GlobalContext = token_context::GetGlobalContext();
    node* ReferencedNode = GSTATE_DYNCAST(GlobalContext->GetProductForToken(NodeToken.Object), node);
    if (NodeToken.Object != 0 && ReferencedNode == 0)
    {
        // Node has been deleted...
        NodeToken.Type   = 0;
        NodeToken.Object = 0;
        Edge = -1;
    }

    switch (action)
    {
        case eRequestState:
        {
            if (Pressed && ReferencedNode)
            {
                state_machine* MachineParent = GSTATE_DYNCAST(ReferencedNode->GetParent(), state_machine);
                if (MachineParent)
                    MachineParent->RequestChangeTo(AtT, DeltaT, ReferencedNode);
            }
        } break;

        case eForceState:
        {
            if (Pressed && ReferencedNode)
            {
                state_machine* MachineParent = GSTATE_DYNCAST(ReferencedNode->GetParent(), state_machine);
                if (MachineParent)
                    MachineParent->ForceState(AtT, ReferencedNode);
            }
        } break;

        case eTransitionByName:
        {
            if (Pressed && !Name.empty())
            {
                state_machine* Machine = GSTATE_DYNCAST(edit::GetCurrentContainer(), state_machine);
                if (Machine)
                    Machine->StartTransitionByName(AtT, Name.c_str());
            }
        } break;

        case eSetParameter:
        {
            if (ButtonEdge && ReferencedNode && Edge != -1)
            {
                parameters* Parameters = GSTATE_DYNCAST(ReferencedNode, parameters);
                real32 MinVal = 0, MaxVal = 1;
                Parameters->GetScalarOutputRange(Edge, &MinVal, &MaxVal);

                Parameters->SetParameter(Edge, Down ? MaxVal : MinVal);
            }
        } break;

        case eTriggerEvent:
        {
            if (Pressed && ReferencedNode && Edge != -1)
            {
                event_source* EventSource = GSTATE_DYNCAST(ReferencedNode, event_source);
                EventSource->PokeEventByIdx(Edge);
            }
        } break;

        default:
        case eNone:
            break;
    }
}

void
ButtonAction::NoteEdgeDelete(node* Node, int RemovedOutput)
{
    variant token;
    Node->GetTypeAndToken((granny_variant*)&token);

    if (NodeToken.Object == token.Object)
    {
        if (Edge == RemovedOutput)
        {
            // Kill the action, the edge is going away...
            Edge = -1;
        }
        else if (Edge > RemovedOutput)
        {
            // We'll be referring to the edge one down after this completes...
            --Edge;
        }
    }
}

void
ButtonAction::NoteNodeDelete(node* Node)
{
    variant token;
    Node->GetTypeAndToken((granny_variant*)&token);

    if (NodeToken.Object == token.Object)
    {
        // Node is going away, remove reference
        NodeToken.Type   = 0;
        NodeToken.Object = 0;
        Edge = -1;
    }
}


// =============================================================================
AxisAction::AxisAction()
  : Edge(-1)
{
    NodeToken.Object = 0;
    NodeToken.Type = 0;
}

void
AxisAction::TakeAction(real32 Value)
{
    token_context* GlobalContext = token_context::GetGlobalContext();
    node* ReferencedNode = GSTATE_DYNCAST(GlobalContext->GetProductForToken(NodeToken.Object), node);
    if (NodeToken.Object != 0 && ReferencedNode == 0)
    {
        // Node has been deleted...
        NodeToken.Type   = 0;
        NodeToken.Object = 0;
        Edge = -1;
    }

    if (ReferencedNode && Edge != -1)
    {
        parameters* Parameters = GSTATE_DYNCAST(ReferencedNode, parameters);
        real32 MinVal = 0, MaxVal = 1;
        Parameters->GetScalarOutputRange(Edge, &MinVal, &MaxVal);
        Parameters->SetParameter(Edge, MinVal + (MaxVal - MinVal) * Value);
    }
}

void
AxisAction::NoteNodeDelete(node* Node)
{
    variant token;
    Node->GetTypeAndToken((granny_variant*)&token);

    if (NodeToken.Object == token.Object)
    {
        // Node is going away, remove reference
        NodeToken.Type   = 0;
        NodeToken.Object = 0;
        Edge = -1;
    }
}


void
AxisAction::NoteEdgeDelete(node* Node, int RemovedOutput)
{
    variant token;
    Node->GetTypeAndToken((granny_variant*)&token);

    if (NodeToken.Object == token.Object)
    {
        if (Edge == RemovedOutput)
        {
            // Kill the action, the edge is going away...
            Edge = -1;
        }
        else if (Edge > RemovedOutput)
        {
            // We'll be referring to the edge one down after this completes...
            --Edge;
        }
    }
}


// =============================================================================
// // Scheme manipulation
static int l_AddScheme(lua_State* L)
{
    BAIL_IF_UNLOADED(LuaBoolean(L, false));

    PushUndoPos("Add controller scheme");

    if (lua_isnumber(L, -1))
    {
        // Make a copy of the referenced scheme
        int Idx = lua_tointeger(L, -1) - 1;
        if (Idx < 0 || Idx >= (int)s_Schemes.size())
            return LuaBoolean(L, false);

        s_Schemes.push_back(s_Schemes[Idx]);
        s_Schemes.back().Name += " copy";
    }
    else
    {
        // Make a default scheme with no connections...
        s_Schemes.push_back(ControllerScheme());
        s_Schemes.back().Name = "NewScheme";
    }

    s_CurrentScheme = int(s_Schemes.size()) - 1;
    return LuaBoolean(L, true);
}


static int l_DeleteCurrentScheme(lua_State* L)
{
    BAIL_IF_UNLOADED(LuaBoolean(L, false));

    if (s_Schemes.empty())
        return LuaBoolean(L, false);

    if (s_CurrentScheme < 0 || s_CurrentScheme >= (int)s_Schemes.size())
        return LuaBoolean(L, false);

    PushUndoPos("Delete controller scheme");

    s_Schemes.erase(s_Schemes.begin() + s_CurrentScheme);
    if (s_CurrentScheme >= (int)s_Schemes.size())
        --s_CurrentScheme;

    return LuaBoolean(L, true);
}


static int
l_GetSchemeNames(lua_State* L)
{
    BAIL_IF_UNLOADED(LuaNil(L));

    lua_createtable(L, (int)s_Schemes.size(), 0);

    {for(uint32x Idx = 0; Idx < s_Schemes.size(); ++Idx)
    {
        lua_pushinteger(L, Idx + 1);
        lua_pushstring(L, s_Schemes[Idx].Name.c_str());
        lua_settable(L, -3);
    }}

    return 1;
}

// XInput_SetSchemeName(idx, name)
static int
l_SetSchemeName(lua_State* L)
{
    BAIL_IF_UNLOADED(LuaBoolean(L, false));

    PushUndoPos("Set controller scheme name");

    int Idx          = lua_tointeger(L, -2) - 1;
    char const* Name = lua_tostring(L, -1);

    if (Idx < 0 || Idx >= (int)s_Schemes.size())
        return LuaBoolean(L, false);

    s_Schemes[Idx].Name = Name;
    return LuaBoolean(L, true);
}


static int
l_GetCurrentScheme(lua_State* L)
{
    return LuaInteger(L, s_CurrentScheme + 1);
}

static int
l_SetCurrentScheme(lua_State* L)
{
    BAIL_IF_UNLOADED(LuaBoolean(L, false));

    int Idx = lua_tointeger(L, -1) - 1;

    // Special case for empty array...
    if (!(Idx == -1 && s_Schemes.empty()) &&
        (Idx < 0 || Idx >= (int)s_Schemes.size()))
    {
        return LuaBoolean(L, false);
    }

    PushUndoPos("Change current controller scheme");

    s_CurrentScheme = Idx;
    return LuaBoolean(L, true);
}

static int
l_ControllerConnected(lua_State* L)
{
    BAIL_IF_UNLOADED(LuaBoolean(L, false));

    XINPUT_STATE state;
    return LuaBoolean(L, (XFn(GetState)(0,&state) == ERROR_SUCCESS));
}

static int l_EditCurrentScheme(lua_State* L);

bool GRANNY
UIXInput_Register(lua_State* State)
{
    lua_register(State, "XInput_AddScheme",           l_AddScheme);
    lua_register(State, "XInput_DeleteCurrentScheme", l_DeleteCurrentScheme);
    lua_register(State, "XInput_GetSchemeNames",      l_GetSchemeNames);
    lua_register(State, "XInput_GetCurrentScheme",    l_GetCurrentScheme);
    lua_register(State, "XInput_SetCurrentScheme",    l_SetCurrentScheme);
    lua_register(State, "XInput_SetSchemeName",       l_SetSchemeName);

    lua_register(State, "XInput_ControllerConnected", l_ControllerConnected);
    lua_register(State, "XInput_EditCurrentScheme",   l_EditCurrentScheme);

    return true;
}


const uint32 c_CurrentVersion = uint32(0x600Df00D) + 3;

struct scheme_state
{
    // Version this separately, we want to use the fast path unless we're loading from an
    // old file.
    granny_uint32    VersionTag;

    granny_int32     EncodingCount;
    scheme_encoding* Encodings;

    granny_int32     Selected;
};
data_type_definition SchemeStateType[] =
{
    { UInt32Member,           "VersionTag" },
    { ReferenceToArrayMember, "Encodings", SchemeEncodingType },
    { Int32Member,            "Selected" },
    { EndMember }
};

void GRANNY
CreateEncodedXInput(variant* EditorData)
{
    Assert(EditorData->Type == 0 && EditorData->Object == 0);

    // Leave null if no schemes...
    if (s_Schemes.size() == 0)
        return;

    scheme_state* State = Allocate(scheme_state, AllocationTemporary);
    State->VersionTag = c_CurrentVersion;

    State->EncodingCount = (granny_int32)s_Schemes.size();
    State->Encodings  = GStateAllocArray(scheme_encoding, s_Schemes.size());

    State->Selected = s_CurrentScheme;

    scheme_encoding* Encoded = State->Encodings;
    for (size_t Idx = 0; Idx < s_Schemes.size(); ++Idx)
        s_Schemes[Idx].ToEncoding(&Encoded[Idx]);

    EditorData->Object = State;
    EditorData->Type   = SchemeStateType;
}

void GRANNY
DestroyEncodedXInput(variant* EditorData)
{
    if (EditorData->Type == 0)
        return;

    scheme_state* State = (scheme_state*)EditorData->Object;
    for (int Idx = 0; Idx < State->EncodingCount; ++Idx)
    {
        Deallocate(State->Encodings[Idx].Name);
        for (int BIdx = 0; BIdx < c_NumButtons; ++BIdx)
            Deallocate(State->Encodings[Idx].Buttons[BIdx].Name);
    }

    Deallocate(State->Encodings);
    Deallocate(State);

    EditorData->Object = 0;
    EditorData->Type   = 0;
}

void GRANNY
ReadFromEncodedXInput(variant* EditorData)
{
    s_Schemes.clear();
    s_CurrentScheme = -1;

    if (EditorData->Type == 0 || EditorData->Object == 0)
        return;

    // The version tag is always the first piece, so just pull that first...
    granny_uint32 const thisVersion = *((granny_uint32*)EditorData->Object);

    void*         FreePointer = 0;
    scheme_state* State       = 0;
    if (thisVersion != c_CurrentVersion)
    {
        // Convert
        FreePointer = ConvertTree(EditorData->Type, EditorData->Object, SchemeStateType, 0);
        State = (scheme_state*)FreePointer;
    }
    else
    {
        State = (scheme_state*)EditorData->Object;
    }

    // Read from state
    if (State)
    {
        s_Schemes.resize(State->EncodingCount);
        for (int Idx = 0; Idx < State->EncodingCount; ++Idx)
            s_Schemes[Idx].FromEncoding(State->Encodings[Idx]);

        s_CurrentScheme = State->Selected;
    }

    // Free the conversion buffer if necessary
    Deallocate(FreePointer);
}

void GRANNY
ResetXInput()
{
    // Read from with null is a reset
    variant v = { 0, 0 };
    ReadFromEncodedXInput(&v);
}


enum EStatesParamsOrEvents { eStates, eParameters, eEvents };

static int
AllStatesOrParams(vector<node*>& Nodes,
                  vector<char const*>& NodeNames,
                  node* WatchFor,
                  EStatesParamsOrEvents spoe)
{
    int CurrentSelection = -1;

    Nodes.push_back(0);
    NodeNames.push_back("<not set>");
    if (WatchFor == 0)
        CurrentSelection = 0;

    deque<container*> StillToSearch(1, edit::GetRootMachine());
    while (!StillToSearch.empty())
    {
        container* Container = StillToSearch.front();
        StillToSearch.pop_front();

        for (int Idx = 0; Idx < Container->GetNumChildren(); ++Idx)
        {
            container* ContainerChild = GSTATE_DYNCAST(Container->GetChild(Idx), container);
            if (ContainerChild)
                StillToSearch.push_back(ContainerChild);
        }

        state_machine* AsMachine = GSTATE_DYNCAST(Container, state_machine);
        if (AsMachine)
        {
            for (int Idx = 0; Idx < AsMachine->GetNumChildren(); ++Idx)
            {
                node* Node = AsMachine->GetChild(Idx);
                if (spoe == eStates)
                {
                    if (!state_machine::IsStateNode(Node))
                        continue;
                }
                else if (spoe == eParameters)
                {
                    if (GSTATE_DYNCAST(Node, parameters) == 0)
                        continue;
                }
                else
                {
                    Assert(spoe == eEvents);
                    if (GSTATE_DYNCAST(Node, event_source) == 0)
                        continue;
                }
                Nodes.push_back(Node);
                NodeNames.push_back(Node->GetName());

                if (WatchFor == Node)
                    CurrentSelection = int(Nodes.size()) - 1;
            }
        }
    }

    return CurrentSelection;
}

static void
SetParameterNodeAndEdge(variant*       NodeToken,
                        granny_int32x* RefEdge,
                        int AtX,
                        int& CurrY,
                        ControlIDGen& IDGen)
{
    LuaRect drawArea = UIAreaGet();

    token_context* GlobalContext = token_context::GetGlobalContext();
    node* Node = GSTATE_DYNCAST(GlobalContext->GetProductForToken(NodeToken->Object), node);

    // Walk all of the valid states.  We look in *EVERYTHING* for now...
    vector<node*>  Nodes;
    vector<char const*> NodeNames;

    int CurrentSelection = AllStatesOrParams(Nodes, NodeNames, Node, eParameters);

    LuaRect SelectNodeRect = FillRectToRight(AtX, drawArea.w, CurrY, 21);
    if (SmallComboboxOn(NodeNames, CurrentSelection, SelectNodeRect, IDGen))
    {
        PushUndoPos("Change button target");

        Node = Nodes[CurrentSelection];
        if (Node)
            Node->GetTypeAndToken((granny_variant*)NodeToken);
        else
            ZeroStructure(*NodeToken);

        *RefEdge = -1;
    }
    AdvanceYLastControl(CurrY);

    LuaRect EdgeRect = FillRectToRight(AtX, drawArea.w, CurrY, 21);
    if (Node)
    {
        vector<char const*> EdgeNames;
        for (int Idx = 0; Idx < Node->GetNumOutputs(); ++Idx)
            EdgeNames.push_back(Node->GetOutputName(Idx));

        int32x NewEdge = *RefEdge;
        if (SmallComboboxOn(EdgeNames, NewEdge, EdgeRect, IDGen))
        {
            PushUndoPos("Change parameter edge");

            *RefEdge = NewEdge;
        }

    }
    else
    {
        SmallComboboxPlaceholder(0, EdgeRect, IDGen);
    }
    AdvanceYLastControl(CurrY);
}

static void
SetEventNodeAndIndex(variant*       NodeToken,
                     granny_int32x* RefEdge,
                     int AtX,
                     int& CurrY,
                     ControlIDGen& IDGen)
{
    LuaRect drawArea = UIAreaGet();

    token_context* GlobalContext = token_context::GetGlobalContext();
    node* Node = GSTATE_DYNCAST(GlobalContext->GetProductForToken(NodeToken->Object), node);

    // Walk all of the valid states.  We look in *EVERYTHING* for now...
    vector<node*>  Nodes;
    vector<char const*> NodeNames;

    int CurrentSelection = AllStatesOrParams(Nodes, NodeNames, Node, eEvents);

    LuaRect SelectNodeRect = FillRectToRight(AtX, drawArea.w, CurrY, 21);
    if (SmallComboboxOn(NodeNames, CurrentSelection, SelectNodeRect, IDGen))
    {
        PushUndoPos("Change button target");

        Node = Nodes[CurrentSelection];
        if (Node)
            Node->GetTypeAndToken((granny_variant*)NodeToken);
        else
            ZeroStructure(*NodeToken);

        *RefEdge = -1;
    }
    AdvanceYLastControl(CurrY);

    LuaRect EdgeRect = FillRectToRight(AtX, drawArea.w, CurrY, 21);
    if (Node)
    {
        event_source* AsEvent = GSTATE_DYNCAST(Node, event_source);

        vector<char const*> EdgeNames;
        for (int Idx = 0; Idx < AsEvent->NumPossibleEvents(); ++Idx)
            EdgeNames.push_back(AsEvent->GetPossibleEvent(Idx));

        int32x NewEdge = *RefEdge;
        if (SmallComboboxOn(EdgeNames, NewEdge, EdgeRect, IDGen))
        {
            PushUndoPos("Change event trigger");

            *RefEdge = NewEdge;
        }
    }
    else
    {
        SmallComboboxPlaceholder(0, EdgeRect, IDGen);
    }

    AdvanceYLastControl(CurrY);
}

// =============================================================================
// Meaty editing function...
static int
l_EditCurrentScheme(lua_State* L)
{
    int CurrY = lua_tointeger(L, -1);
    BAIL_IF_UNLOADED(LuaInteger(L, CurrY));

    if (s_CurrentScheme == -1)
        return LuaInteger(L, CurrY);

    LuaRect drawArea = UIAreaGet();

    ControllerScheme& Scheme = s_Schemes[s_CurrentScheme];

    ControlIDGen IDGen("control_scheme_edit");

    int32x LabelX, ControlX;
    LabeledControlsAtWidth(LabelWidth("Right Shoulder"), &LabelX, &ControlX);

    // Action box
    LuaRect ActionRect(ControlX, CurrY, 175, 21);
    int32x ExtraX = ActionRect.x + ActionRect.w + HPadding();


    std::vector<char const*> ActionStrings(s_ActionNames, s_ActionNames + ButtonAction::eNumButtonActions);

    // Do the buttons first...
    for (int BIdx = 0; BIdx < c_NumButtons; ++BIdx)
    {
        SmallLabelAt(s_ButtonNames[BIdx], LuaPoint(LabelX, SmallComboLabelAdjust(CurrY)), eRight);
        ButtonAction& Button = Scheme.buttons[BIdx];

        LuaRect Rect = ActionRect;
        Rect.y = CurrY;

        int32x CurrAction = Button.action;
        if (SmallComboboxOn(ActionStrings, CurrAction, Rect, IDGen))
        {
            PushUndoPos("Change button action");

            if (CurrAction >= 0 && CurrAction < ButtonAction::eNumButtonActions)
            {
                Button = ButtonAction();
                Button.action = (ButtonAction::EActionType)CurrAction;
            }
        }

        switch (CurrAction)
        {
            case ButtonAction::eRequestState:
            case ButtonAction::eForceState:
            {
                // Walk all of the valid states.  We look in *EVERYTHING* for now...
                vector<node*>  Nodes;
                vector<char const*> NodeNames;

                token_context* GlobalContext = token_context::GetGlobalContext();
                node* Node = GSTATE_DYNCAST(GlobalContext->GetProductForToken(Button.NodeToken.Object), node);

                int CurrentSelection = AllStatesOrParams(Nodes, NodeNames, Node, eStates);

                LuaRect SelectNodeRect = FillRectToRight(ExtraX, drawArea.w, CurrY, 21);
                if (SmallComboboxOn(NodeNames, CurrentSelection, SelectNodeRect, IDGen))
                {
                    PushUndoPos("Change button target");

                    if (Nodes[CurrentSelection])
                        Nodes[CurrentSelection]->GetTypeAndToken((granny_variant*)&Button.NodeToken);
                    else
                        ZeroStructure(Button.NodeToken);
                }

                AdvanceYLastControl(CurrY);
            } break;

            case ButtonAction::eSetParameter:
            {
                SetParameterNodeAndEdge(&Button.NodeToken, &Button.Edge, ExtraX, CurrY, IDGen);
            } break;

            case ButtonAction::eTriggerEvent:
            {
                SetEventNodeAndIndex(&Button.NodeToken, &Button.Edge, ExtraX, CurrY, IDGen);
            } break;

            case ButtonAction::eTransitionByName:
            {
                LuaRect EditRect = FillRectToRight(ExtraX, drawArea.w, CurrY, 24);

                std::string Transition = Button.Name;
                if (EditLabelOn(Transition, EditRect, IDGen))
                {
                    PushUndoPos("Change transition");
                    Button.Name = Transition;
                }
                AdvanceYLastControl(CurrY);
            } break;


            case ButtonAction::eNone:
            default:
                AdvanceYLastControl(CurrY);
                break;
        }
    }

    // Then the axes
    for (int AIdx = 0; AIdx < c_NumAxes; ++AIdx)
    {
        SmallLabelAt(s_AxisNames[AIdx], LuaPoint(LabelX, SmallComboLabelAdjust(CurrY)), eRight);
        AxisAction& Axis = Scheme.axes[AIdx];

        LuaRect Rect = ActionRect;
        Rect.y = CurrY;
        SmallComboboxPlaceholder(s_ActionNames[ButtonAction::eSetParameter], Rect, IDGen);


        SetParameterNodeAndEdge(&Axis.NodeToken, &Axis.Edge, ExtraX, CurrY, IDGen);
    }

    return LuaInteger(L, CurrY);
}

void GRANNY
XInput_NoteNodeDelete(GSTATE node* Node)
{
    BAIL_IF_UNLOADED();

    for (size_t Idx = 0; Idx < s_Schemes.size(); ++Idx)
    {
        for (int i = 0; i < c_NumButtons; ++i)
            s_Schemes[Idx].buttons[i].NoteNodeDelete(Node);

        for (int i = 0; i < c_NumAxes; ++i)
            s_Schemes[Idx].axes[i].NoteNodeDelete(Node);
    }
}

void GRANNY
XInput_NoteOutputDelete(node* Node, int Output)
{
    BAIL_IF_UNLOADED();

    for (size_t Idx = 0; Idx < s_Schemes.size(); ++Idx)
    {
        for (int i = 0; i < c_NumButtons; ++i)
            s_Schemes[Idx].buttons[i].NoteEdgeDelete(Node, Output);

        for (int i = 0; i < c_NumAxes; ++i)
            s_Schemes[Idx].axes[i].NoteEdgeDelete(Node, Output);
    }
}

void GRANNY
XInput_NotePossibleEventDelete(node* Node, int EventIdx)
{
    BAIL_IF_UNLOADED();

    for (size_t Idx = 0; Idx < s_Schemes.size(); ++Idx)
    {
        for (int i = 0; i < c_NumButtons; ++i)
            s_Schemes[Idx].buttons[i].NoteEdgeDelete(Node, EventIdx);

        // axes can't refer to event_source nodes for now...
        // s_Schemes[Idx].axes[i].NoteEdgeDelete(Node, EventIdx);
    }
}

