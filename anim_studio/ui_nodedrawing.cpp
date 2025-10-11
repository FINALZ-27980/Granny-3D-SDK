// ========================================================================
// $File: //jeffr/granny_29/statement/ui_nodedrawing.cpp $
// $DateTime: 2012/10/22 16:29:12 $
// $Change: 39907 $
// $Revision: #14 $
//
// $Notice: $
// ========================================================================
#include "ui_nodedrawing.h"
#include "statement.h"
#include "statement_undostack.h"

#include "ui_area.h"
#include "ui_core.h"
#include "ui_drawing.h"
#include "ui_bitmap.h"
#include "ui_font.h"
#include "ui_preferences.h"

#include "ui_blendgraph_drawing.h"
#include "ui_statemachine_drawing.h"

#include "statement_editstate.h"

#include "gstate_blend_graph.h"
#include "gstate_event_mixer.h"
#include "gstate_event_source.h"
#include "gstate_node.h"
#include "gstate_state_machine.h"

#include "granny_constants.h"
#include "granny_math.h"

#include "luautils.h"

#include <utility>
#include <windows.h>
#include <gl/gl.h>

// class node : public tokenized

USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;
using namespace node_drawing;
using namespace std;

BEGIN_GRANNY_NAMESPACE;
namespace node_drawing
{
    int32x s_TitleBarFont         = 0;
    int32x s_TitleBarFontHeight   = 0;
    int32x s_TitleBarFontBaseline = 0;

    extern int32x s_TitleBarIconHeight = 24;

    int32x s_MinTitleBarWidth = 50;

    LuaColor s_NodeTitleText;
    LuaColor s_NodeTitleTextDrop;

    int32x s_EdgeFont         = 0;
    int32x s_EdgeFontHeight   = 0;
    int32x s_EdgeFontBaseline = 0;

    LuaColor s_EdgeText;
    LuaColor s_EdgeTextDrop;

    int s_TopPadding      = 2;
    int s_BelowBarPadding = 3;
    int s_BottomPadding   = 4;

    int s_SidePadding         = s_BottomPadding;
    int s_InternalEdgePadding = 8;

    int const s_MinEdgeWidth      = 25;
    int const s_MaxEdgeWidth      = 150;

    int const s_BGParamSliderWidth = 100;

    int const s_EdgeTopPadding    = 2;
    int const s_EdgeBottomPadding = 2;
    int const s_EdgeSidePadding   = 10;
    real32 const s_ConnectionRadius = 6;
    int s_EdgeHeight;

    LuaColor s_NodeColor;
    LuaColor s_DropShadowColor;
    LuaColor s_NodeSelectColor;

    // 0 = normal, 1 = highlight
    LuaColor s_EdgeColors[2][EdgeTypeCount];
    LuaColor s_TransitionColors[NumTransitionTypes];

    int32x s_ConnectionHandle = -1;
    int32x s_ConnectionW = 0;
    int32x s_ConnectionH = 0;

    int32x s_StateNodeGrid = -1;

    void InitNodeMetrics()
    {
        static bool Done = false;
        if (Done)
            return;
        Done = true;

        // Get the fonts
        {
            s_TitleBarFont = GetPreferenceFont("node_title");
            if (s_TitleBarFont < 0 ||
                !UIGetFontHeightAndBaseline(s_TitleBarFont, s_TitleBarFontHeight, s_TitleBarFontBaseline))
            {
                Assert(false);
                // todo: handle this
            }

            s_EdgeFont = GetPreferenceFont("node_edge");
            if (s_EdgeFont < 0 ||
                !UIGetFontHeightAndBaseline(s_EdgeFont, s_EdgeFontHeight, s_EdgeFontBaseline))
            {
                Assert(false);
                // todo: handle this
            }

            s_EdgeHeight = (s_EdgeTopPadding +
                            s_EdgeFontHeight +
                            s_EdgeBottomPadding);
        }

        // And the colors...
        s_NodeTitleText = GetPreferenceColor("node_title_text");
        s_NodeTitleTextDrop = GetPreferenceColor("node_title_text_drop");

        s_EdgeText = GetPreferenceColor("node_edge_text");
        s_EdgeTextDrop = GetPreferenceColor("node_edge_text_drop");

        s_NodeColor       = GetPreferenceColor("blend_node_base");
        s_DropShadowColor = GetPreferenceColor("blend_node_drop");
        s_NodeSelectColor = GetPreferenceColor("blend_node_select");

        s_EdgeColors[0][PoseEdge]   = GetPreferenceColor("node_edge_pose");
        s_EdgeColors[0][ScalarEdge] = GetPreferenceColor("node_edge_scalar");
        s_EdgeColors[0][MaskEdge]   = GetPreferenceColor("node_edge_mask");
        s_EdgeColors[0][EventEdge]  = GetPreferenceColor("node_edge_event");
        s_EdgeColors[0][MorphEdge]  = GetPreferenceColor("node_edge_morph");

        s_EdgeColors[1][PoseEdge]   = GetPreferenceColor("node_edge_pose_hl");
        s_EdgeColors[1][ScalarEdge] = GetPreferenceColor("node_edge_scalar_hl");
        s_EdgeColors[1][MaskEdge]   = GetPreferenceColor("node_edge_mask_hl");
        s_EdgeColors[1][EventEdge]  = GetPreferenceColor("node_edge_event_hl");
        s_EdgeColors[1][MorphEdge]  = GetPreferenceColor("node_edge_morph_hl");

        s_TransitionColors[Transition_OnRequest    ] = GetPreferenceColor("tr_onrequest_color"); 
        s_TransitionColors[Transition_OnLoop       ] = GetPreferenceColor("tr_onloop_color");    
        s_TransitionColors[Transition_OnSubLoop    ] = GetPreferenceColor("tr_onsubloop_color"); 
        s_TransitionColors[Transition_LastResort   ] = GetPreferenceColor("tr_lastresort_color");
        s_TransitionColors[Transition_OnConditional] = GetPreferenceColor("tr_onconditional_color");

        s_ConnectionHandle = UIGetBitmapHandle("connection");
        UIGetBitmapWidthAndHeight(s_ConnectionHandle, s_ConnectionW, s_ConnectionH);

        s_StateNodeGrid = UIGet9GridHandle("node");
    }
}
END_GRANNY_NAMESPACE;


void GRANNY
DrawNodeDropShadow(LuaRect const& Rect, bool Selected)
{
    // offset 3/6 pixels down/left.  We're not going to register this as part of the drawn
    // rect, it's ok if the scroll area computation excludes this.
    LuaRect DropRect = Rect;

    if (Selected)
    {
        DropRect.x += 6;
        DropRect.y += 6;
    }
    else
    {
        DropRect.x += 3;
        DropRect.y += 3;
    }

    UIRender9GridModulated(s_StateNodeGrid, DropRect, s_DropShadowColor);
}

void GRANNY
DrawNode(LuaRect const& Rect, LuaColor const& Color)
{
    UIRender9GridModulated(s_StateNodeGrid, Rect, Color);
}


static bool
EditableNodeType(node* Node)
{
    if (GSTATE_DYNCAST(Node, state_machine) != 0 ||
        GSTATE_DYNCAST(Node, blend_graph) != 0)
    {
        // These are editable only if their parent is a blend-graph, not a state machine
        Assert(Node->GetParent());
        if (GSTATE_DYNCAST(Node->GetParent(), state_machine))
            return false;
    }

    if (GSTATE_SLOW_TYPE_CHECK(Node, event_mixer) != 0)
        return false;

    return true;
}

void GRANNY
RegisterNodeMove(lua_State* L, node* Node,
                 LuaRect const& NodeRect,
                 LuaRect const& IconRect,
                 int32x Border,
                 bool IsReplaceableState)
{
    LuaRect wBorder = NodeRect;
    wBorder.x += Border;
    wBorder.y += Border;
    wBorder.w -= Border*2;
    wBorder.h -= Border*2;

    LuaPoint nodePt;
    Node->GetPosition(nodePt.x, nodePt.y);

    // Handle the node moving interaction
    //  Note: registering here causes the connection circles to move off the rect...
    {
        lua_getglobal(L, "RegisterNodeMove");
        lua_pushinteger(L, edit::TokenizedToID(Node));
        PushRect(L, wBorder);

        if (EditableNodeType(Node))
            PushRect(L, IconRect);
        else
            lua_pushnil(L);

        PushPoint(L, nodePt);
        lua_pushboolean(L, IsReplaceableState);
        lua_pcall(L, 5, 2, 0);

        LuaPoint newPoint;
        ExtractPoint(L, -2, newPoint);
        bool HasChanged = !!lua_toboolean(L, -1);
        lua_pop(L, 2);

        if (HasChanged)
        {
            PushUndoPos("node move", false, eNodeMove); // todo: too much!

            // Need to alter the whole selection, so compute a delta here...
            int32x DeltaX = newPoint.x - nodePt.x;
            int32x DeltaY = newPoint.y - nodePt.y;

            vector<tokenized*> Selection;
            edit::GetSelection(Selection);
            {for (size_t Idx = 0; Idx < Selection.size(); ++Idx)
            {
                node* ThisNode = GSTATE_DYNCAST(Selection[Idx], node);
                if (ThisNode == 0)
                    continue;

                if (DeltaX || DeltaY)
                {
                    LuaPoint thisPt;
                    ThisNode->GetPosition(thisPt.x, thisPt.y);
                    ThisNode->SetPosition(thisPt.x + DeltaX,
                                          thisPt.y + DeltaY);
                }
            }}
        }
    }
}

void GRANNY
RegisterStateInteractions(lua_State* L, node* Node,
                          LuaRect const& NodeRect,
                          LuaRect const& StartStateRect,
                          LuaRect const& CurrStateRect)
{
    lua_getglobal(L, "RegisterStateOnlyInteractions");
    lua_pushinteger(L, edit::TokenizedToID(Node));
    PushRect(L, NodeRect);
    PushRect(L, StartStateRect);
    PushRect(L, CurrStateRect);
    lua_pcall(L, 4, 0, 0);
}


void GRANNY
DoRenameableTitle(lua_State* L,
                  node* Node,
                  int32x TitleBarFont,
                  int32x X,
                  int32x Y,
                  int32x W,
                  LuaColor const& FrontColor,
                  LuaColor const& DropColor)
{
    vector<tokenized*> Selection;
    edit::GetSelection(Selection);

    // Only the first selected node is f2-able
    bool F2Renamable = !Selection.empty() && Selection[0] == Node;

    // Renamable edit field
    lua_getglobal(L, "HandleNodeName");
    lua_pushstring(L, "random_id_string");
    lua_pushinteger(L, X);
    lua_pushinteger(L, Y);
    lua_pushinteger(L, W);
    lua_pushinteger(L, edit::TokenizedToID(Node));
    lua_pushboolean(L, F2Renamable);
    lua_pcall(L, 6, 0, 0);
}

void GRANNY
NodeControls(lua_State* L, node* Node, LuaRect const& NodeRect)
{
    // Register the close button (always present)
    LuaRect closeRect;
    {
        lua_State* L = UILuaState();

        // todo: bitmap for the button
        int w,h;
        if (UIGetTextDimension(s_TitleBarFont, "x", 0, w, h))
        {
            int closeWidth = (w + 2*s_SidePadding);
            closeRect = LuaRect(NodeRect.x + NodeRect.w - closeWidth,
                                NodeRect.y + s_TopPadding,
                                closeWidth, h);

            lua_getglobal(L, "RegisterNodeClose");
            lua_pushinteger(L, edit::TokenizedToID(Node));
            PushRect(L, closeRect);
            lua_pcall(L, 2, 0, 0);

            RenderDropText(s_TitleBarFont, "x", 0,
                           closeRect.x, s_TopPadding + s_TitleBarFontBaseline,
                           s_NodeTitleText, s_NodeTitleTextDrop);
        }
        else
        {
            // todo: very bad
            return;
        }
    }
}

LuaRect GRANNY
ComputeNodeRect(GSTATE node* Node)
{
    Assert(Node);
    Assert(Node->GetParent());

    container* Parent = Node->GetParent();
    if (GSTATE_DYNCAST(Parent, blend_graph))
    {
        return ComputeBlendGraphNodeRect(Node);
    }
    else if (GSTATE_DYNCAST(Parent, state_machine))
    {
        return ComputeStateNodeRect(Node);
    }
    else
    {
        Assert(false);
        return LuaRect();
    }
}


void GRANNY
DoDragSelect(lua_State* L,
             container* Container)
{
    Assert(L && Container);
    lua_pushinteger(L, edit::TokenizedToID(Container));
    PushTableFunction(L, "DragSelect", "Do");

    // Push an id.  Note that on entry to this function, -1 was the parent id, which is
    // now at index -3.
    MakeLocalControlID(L, -3, -1);

    // Push the table of all nodes/id pairs
    {
        lua_createtable(L, (int)Container->GetNumChildren(), 0);

        {for(int32x Idx = 0; Idx < Container->GetNumChildren(); ++Idx)
        {
            node* Child = Container->GetChild(Idx);

            lua_pushinteger(L, Idx + 1);

            lua_createtable(L, 0, 2);
            {
                LuaRect nodeRect = ComputeNodeRect(Child);
                Child->GetPosition(nodeRect.x, nodeRect.y);

                // Node rect
                lua_pushstring(L, "rect");
                PushRect(L, nodeRect);
                lua_settable(L, -3);

                // Node id
                lua_pushstring(L, "id");
                lua_pushinteger(L, edit::TokenizedToID(Child));
                lua_settable(L, -3);
            }

            lua_settable(L, -3);
        }}
    }

    // Call that function!
    lua_pcall(L, 2, 0, 0);
    lua_pop(L, 1);  // control id
}

void GRANNY
DoComments(lua_State* L)
{
    PushTableFunction(L, "_G", "RenderComments");

    // Push an id.  Note that on entry to this function, -1 was the parent id, which is
    // now at index -3.
    MakeLocalControlID(L, -2, -1);

    // Call that function!
    lua_pcall(L, 1, 0, 0);
    lua_pop(L, 1);  // control id
}


static int
l_EditStateRender(lua_State* L)
{
    StTMZone("Edit State Render");

    state_machine* Machine    = GSTATE_DYNCAST(edit::GetCurrentContainer(), state_machine);
    blend_graph*   BlendGraph = GSTATE_DYNCAST(edit::GetCurrentContainer(), blend_graph);

    DoDragSelect(L, edit::GetCurrentContainer());
    DoComments(L);

    if (Machine)
    {
        return StateMachineRender(L, Machine);
    }
    else if (BlendGraph)
    {
        return BlendGraphRender(L, BlendGraph);
    }
    else
    {
        // todo: log error, no container?
        lua_pushnil(L);
        lua_pushboolean(L, false);
        return 2;
    }
}

static int
l_AlignSelectionFromKey(lua_State* State)
{
    int Key = lua_tointeger(State, -1);

    vector<tokenized*> TokSelection;
    edit::GetSelection(TokSelection);

    vector<node*> Selection;
    {for (size_t Idx = 0; Idx < TokSelection.size(); ++Idx)
    {
        if (GSTATE_DYNCAST(TokSelection[Idx], node))
            Selection.push_back(static_cast<node*>(TokSelection[Idx]));
    }}

    if (Selection.size() <= 1)
        return 0;

    PushUndoPos("Align nodes");

    vector<LuaRect> SelectionRects;
    {for (size_t Idx = 0; Idx < Selection.size(); ++Idx)
    {
        SelectionRects.push_back(ComputeNodeRect(Selection[Idx]));
        Selection[Idx]->GetPosition(SelectionRects.back().x,
                                    SelectionRects.back().y);
    }}

    switch (Key)
    {
        case Direction_KeyUp:
        {
            {for (size_t Idx = 1; Idx < Selection.size(); ++Idx)
            {
                Selection[Idx]->SetPosition(SelectionRects[Idx].x, SelectionRects[0].y);
            }}
        } break;

        case Direction_KeyLeft:
        {
            {for (size_t Idx = 1; Idx < Selection.size(); ++Idx)
            {
                Selection[Idx]->SetPosition(SelectionRects[0].x, SelectionRects[Idx].y);
            }}
        } break;

        case Direction_KeyDown:
        {
            int const AlignY = (SelectionRects[0].y + SelectionRects[0].h);
            {for (size_t Idx = 1; Idx < Selection.size(); ++Idx)
            {
                Selection[Idx]->SetPosition(SelectionRects[Idx].x,
                                            AlignY - SelectionRects[Idx].h);
            }}
        } break;

        case Direction_KeyRight:
        {
            int const AlignX = (SelectionRects[0].x + SelectionRects[0].w);
            {for (size_t Idx = 1; Idx < Selection.size(); ++Idx)
            {
                Selection[Idx]->SetPosition(AlignX - SelectionRects[Idx].w,
                                            SelectionRects[Idx].y);
            }}
        } break;
    }

    return 0;
}

int GRANNY
EditBoxThisNode(lua_State* State)
{
    int32x PosX   = (int32x)lua_tointeger(State, lua_upvalueindex(1));
    int32x RightX = (int32x)lua_tointeger(State, lua_upvalueindex(2));
    int32x PosY   = (int32x)lua_tointeger(State, lua_upvalueindex(3));
    int32x NodeID = (int32x)lua_tointeger(State, lua_upvalueindex(4));
    node* Node = edit::IDToNode(NodeID);

    edit::SetSelection(Node);

    PushTableFunction(State, "IPadDialog", "DoHorizontal");
    lua_pushinteger(State, PosX);
    lua_pushinteger(State, RightX);
    lua_pushinteger(State, PosY);
    lua_pushinteger(State, 400);
    lua_pushinteger(State, 500);
    lua_pcall(State, 5, 1, 0);

    return 0;
}

// For the event_source triggering at the node level...
int GRANNY
TriggerEventPoke(lua_State* L)
{
    int32x EventIdx = (int32x)lua_tointeger(L, lua_upvalueindex(1));
    int32x NodeID   = (int32x)lua_tointeger(L, lua_upvalueindex(2));

    event_source* EventSource = GSTATE_DYNCAST(edit::IDToNode(NodeID), event_source);
    if (EventSource == 0)
        return LuaBoolean(L, false);

    Assert(EventIdx >= 0 && EventIdx < EventSource->NumPossibleEvents());
    return LuaBoolean(L, EventSource->PokeEventByIdx(EventIdx));
}




bool GRANNY
UINodeDrawing_Register(lua_State* State)
{
    lua_register(State, "EditStateRender", l_EditStateRender);

    lua_register(State, "AlignSelectionFromKey",  l_AlignSelectionFromKey);
    return true;
}

void GRANNY
UINodeDrawing_Shutdown()
{
    //
}


