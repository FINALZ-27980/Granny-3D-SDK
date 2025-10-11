// ========================================================================
// $File: //jeffr/granny_29/statement/ui_statemachine_drawing.cpp $
// $DateTime: 2012/05/17 11:49:45 $
// $Change: 37439 $
// $Revision: #17 $
//
// $Notice: $
// ========================================================================
#include "ui_statemachine_drawing.h"
#include "statement.h"
#include "ui_nodedrawing.h"
#include "statement_undostack.h"

#include "ui_area.h"
#include "ui_bitmap.h"
#include "ui_controls.h"
#include "ui_core.h"
#include "ui_drawing.h"
#include "ui_font.h"
#include "ui_preferences.h"

#include "gstate_transition.h"
#include "gstate_parameters.h"
#include "gstate_event_source.h"
#include "granny_math.h"

#include "statement_editstate.h"
#include "luautils.h"
#include <windows.h>
#include <gl/gl.h>


USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;
using namespace node_drawing;

namespace
{
    LuaColor s_StateBaseColor;
    LuaColor s_StateSelectedColor;

    LuaColor s_StateTextColor;
    LuaColor s_StateTextDropColor;
}


static void
InitStateDrawing()
{
    static bool StateInited = false;
    if (StateInited)
        return;
    StateInited = true;

    s_StateBaseColor = GetPreferenceColor("state_node_base");
    s_StateSelectedColor = GetPreferenceColor("state_node_selected");

    s_StateTextColor     = GetPreferenceColor("state_text");
    s_StateTextDropColor = GetPreferenceColor("state_text_drop");
}


LuaRect GRANNY
ComputeStateNodeRect(node* Node)
{
    int32x Height = (s_TopPadding +
                     s_TitleBarFontHeight +
                     s_BelowBarPadding);

    int32x LargeIcon = -1, SmallIcon = -1;
    edit::GetNodeIcons(Node, LargeIcon, SmallIcon);
    LuaRect Rect = BitmapButtonDim(LargeIcon);

    Height += Rect.h;
    Height += s_BottomPadding*2;

    parameters* Params = GSTATE_DYNCAST(Node, parameters);
    if (Params)
    {
        // Sliders
        Height += Params->GetNumOutputs() * SmallSliderHeight();
    }

    event_source* Source = GSTATE_DYNCAST(Node, event_source);
    if (Source)
    {
        // Buttons
        Height += VPadding() + Source->NumPossibleEvents() * SmallButtonHeight();
    }

    return LuaRect(0, 0, 150, Height);
}

static bool
DoStateNode(node* Node, bool Selected, bool IsStart, bool IsCurrent, LuaRect& DrawnRect)
{
    StTMZone(__FUNCTION__);

    InitNodeMetrics();
    InitStateDrawing();

    lua_State* L = UILuaState();
    Assert(Node);

    LuaRect ChildRect = ComputeStateNodeRect(Node);

    LuaRect ParentRect = ChildRect;
    Node->GetPosition(ParentRect.x, ParentRect.y);

    bool NodeIsValidState = state_machine::IsStateNode(Node);
    int  DragFringe       = NodeIsValidState ? 10 : 0;

    // Only used for parameters and event_source
    ControlIDGen IDGen("node_slieve_controls_%x_%d", edit::TokenizedToID(Node), Node->GetNumOutputs());
    
    LuaRect DragParentRect = ParentRect;
    if (NodeIsValidState)
    {
        DragParentRect.x += 10;
        DragParentRect.y += 10;
        DragParentRect.w -= 20;
        DragParentRect.h -= 20;
    }

    int TempPadding = 4;


    // Draw the icon for this node type
    int32x LargeIcon = -1;
    LuaRect IconRect;
    {
        int32x SmallIcon = -1;
        edit::GetNodeIcons(Node, LargeIcon, SmallIcon);

        UIGetBitmapWidthAndHeight(LargeIcon, IconRect.w, IconRect.h);

        int IconPosY = (s_TopPadding +
                        s_TitleBarFontHeight +
                        s_BelowBarPadding);
        IconRect.x = DragFringe + s_SidePadding;
        IconRect.y = IconPosY;
    }
    
    // And now the node proper.  We'll push onto the area stack just to ensure that
    // everything is easy and properly clipped for the interactions
    bool Clipped;
    if (UIAreaPush(ParentRect, ChildRect))
    {
        StTMZone("Unclipped");
        Clipped = false;

        // Drop shadow first
        DrawNodeDropShadow(ChildRect, Selected);

        // todo: cleanup
        if (NodeIsValidState)
        {
            StTMZone("State Node");
            DrawNode(ChildRect, Selected ? s_StateSelectedColor : s_StateBaseColor);
            DrawRect(LuaRect(ChildRect.x+10,
                             ChildRect.y+10,
                             ChildRect.w-20,
                             ChildRect.h-20), s_StateSelectedColor);
        }
        else
        {
            StTMZone("Non-state node");
            DrawNode(ChildRect, Selected ? s_NodeSelectColor : s_NodeColor);
        }

        // Rect for start state and curr stat
        LuaRect StartStateRect(10 + 48 + 20 - 1,
                               (s_TopPadding +
                                s_TitleBarFontHeight +
                                s_BelowBarPadding) + 8 - 1,
                               26, 26);
        LuaRect CurrStateRect(StartStateRect.x + 26 + TempPadding,
                              StartStateRect.y,
                              26, 26);
        
        // Lua interactions...
        {
            StTMZone("StateNode UIRegister");

            if (NodeIsValidState)
            {
                RegisterStateInteractions(L, Node, ChildRect, StartStateRect, CurrStateRect);
            }

            RegisterNodeMove(L, Node, ChildRect, IconRect, DragFringe, state_machine::IsStateNode(Node));
        }

        // Draw Title and register node controls...
        NodeControls(L, Node, ChildRect);

        int32x X = TempPadding;
        int32x Y = s_TopPadding;
        int32x W = ChildRect.w - X - 15; // @@magic
        DoRenameableTitle(L, Node, s_TitleBarFont,
                          X, Y, W,
                          s_StateTextColor, s_StateTextDropColor);

        UIRenderBitmap(LargeIcon, IconRect.x, IconRect.y);

        // Special icons
        if (state_machine::IsStateNode(Node))
        {
            StTMZone("Special cases");

            // TODO: Cleanup
            DrawRectOutline(StartStateRect, LuaColor(0.25, 0.25, 0.25));
            RegisterToolTip("Start state indicator\n\nRight click to make this\nthe start state", StartStateRect);
            DrawRectOutline(CurrStateRect, LuaColor(0.25, 0.25, 0.25));
            RegisterToolTip("Current state indicator\n\nRight click to make this\nthe current state", CurrStateRect);
            
            if (IsStart)
            {
                UIRenderBitmap(UIGetBitmapHandle("start_state"),
                               StartStateRect.x+1, StartStateRect.y+1);
            }

            if (IsCurrent)
            {
                UIRenderBitmap(UIGetBitmapHandle("current_state"),
                               CurrStateRect.x+1, CurrStateRect.y+1);
            }
        }
        else
        {
            parameters* Params = GSTATE_DYNCAST(Node, parameters);
            event_source* Events = GSTATE_DYNCAST(Node, event_source);

            if (Params)
            {
                int Height = SmallSliderHeight();
                for (int Idx = 0; Idx < Params->GetNumOutputs(); ++Idx)
                {
                    float Min, Max;
                    Params->GetScalarOutputRange(Idx, &Min, &Max);

                    float Value = Params->SampleScalarOutput(Idx, 0);

                    LuaRect Rect(10, IconRect.y + IconRect.h + Idx * Height, ChildRect.w - 20, Height);
                    if (SmallSliderOn(Min, Max, Value, Params->GetClampSliderToInts(), Rect, IDGen))
                    {
                        PushUndoPos("change parameter", false, eSliderMotion, Params->GetUID());
                        Params->SetParameter(Idx, Value);
                        Params->SetParameterDefault(Idx, Value);
                    }

                    RegisterToolTip(Params->GetOutputName(Idx), LastControlRect());
                }
            }
            else if (Events)
            {
                int Height = SmallButtonHeight();
                int BaseY  = IconRect.y + IconRect.h + VPadding();
                for (int Idx = 0; Idx < Events->NumPossibleEvents(); ++Idx)
                {
                    LuaRect Rect(10, BaseY + Idx * Height, ChildRect.w - 20, Height);

                    PushTokenizedIntClosure(Events, TriggerEventPoke, Idx);
                    if (SmallButtonAtRectWithCallback(Events->GetPossibleEvent(Idx),
                                                      Rect, true, IDGen))
                    {
                        
                    }
                }
            }
            else
            {
                InvalidCodePath("Really shouldn't be here.");
            }
        }
    }
    else
    {
        StTMZone("Clipped");
        Clipped = true;
        RegisterNodeMove(L, Node, ChildRect, IconRect, DragFringe, state_machine::IsStateNode(Node));
    }
    UIAreaPop();

    DrawnRect = ParentRect;
    return Clipped;
}

static void
RegisterTransition(lua_State* L,
                   transition* Transition,
                   LuaPoint const& start,
                   LuaPoint const& end)
{
    lua_getglobal(L, "RegisterTransition");
    lua_pushinteger(L, edit::TokenizedToID(Transition));
    PushPoint(L, start);
    PushPoint(L, end);
    lua_pcall(L, 3, 0, 0);
}

static void
DrawConnection(LuaPoint const& start,
               LuaPoint const& end,
               LuaColor const& color,
               bool Selected)
{

    // For rotated arrowheads...
    real32 vec[2] = {
        -real32(end.x - start.x),
        -real32(end.y - start.y)
    };
    real32 len = SquareRoot(vec[0]*vec[0] + vec[1]*vec[1]);
    if (len > 0.00001f)
    {
        vec[0] /= len;
        vec[1] /= len;
    }
    else
    {
        vec[0] = 0;
        vec[1] = 0;
    }

    {
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);

        if (Selected)
        {
            glColor3f(0, 1, 0);
            SetLineWidth(4.0f);
            glBegin(GL_LINES);
            {
                glVertex2iv(&start.x);
                glVertex2iv(&end.x);
            }
            glEnd();
        }

        glColor3fv(&color.r);
        SetLineWidth(1.5f);
        glBegin(GL_LINES);
        {
            glVertex2iv(&start.x);
            glVertex2iv(&end.x);

            glVertex2iv(&end.x);
            glVertex2f(end.x + 10 * (Cos(0.5f) * vec[0] - Sin(0.5f) * vec[1]),
                       end.y + 10 * (Sin(0.5f) * vec[0] + Cos(0.5f) * vec[1]));

            glVertex2iv(&end.x);
            glVertex2f(end.x + 10 * (Cos(-0.5f) * vec[0] - Sin(-0.5f) * vec[1]),
                       end.y + 10 * (Sin(-0.5f) * vec[0] + Cos(-0.5f) * vec[1]));
        }
        glEnd();
    }
}

static bool
ClipToRect(LuaRect const&  Rect,
           LuaPoint const& start,
           LuaPoint const& end,
           LuaPoint&       newStart)
{
    // We're going to advance start until it is on the border of the rect.  First, let's
    // make sure end is *outside* of rect.
    if (Rect.Contains(start) == false || Rect.Contains(end) == true)
    {
        newStart = start;
        return false;
    }

    int32x xBound = start.x;
    real32 xFrac = 1.0f;
    if (end.x < Rect.x)
    {
        Assert(end.x < start.x);

        // Move to the left
        xFrac = (start.x - Rect.x) / real32(start.x - end.x);
        xBound = Rect.x;
    }
    else if (end.x > Rect.x + Rect.w)
    {
        Assert(end.x > start.x);

        // Move to the right
        xFrac = ((Rect.x + Rect.w) - start.x) / real32(end.x - start.x);
        xBound = Rect.x + Rect.w;
    }

    int32x yBound = start.x;
    real32 yFrac = 1.0f;
    if (end.y < Rect.y)
    {
        Assert(end.y < start.y);

        // Move up
        yFrac = (start.y - Rect.y) / real32(start.y - end.y);
        yBound = Rect.y;
    }
    else if (end.y > Rect.y + Rect.h)
    {
        Assert(end.y > start.y);

        // Move to the right
        yFrac  = ((Rect.y + Rect.h) - start.y) / real32(end.y - start.y);
        yBound = Rect.y + Rect.h;
    }

    if (xFrac < yFrac)
    {
        newStart.x = xBound;
        newStart.y = int(xFrac * end.y + (1 - xFrac) * start.y);
    }
    else
    {
        newStart.x = int(yFrac * end.x + (1 - yFrac) * start.x);
        newStart.y = yBound;
    }

    return true;
}

static LuaColor const&
TransitionColor(transition* Transition)
{
    if (Transition->GetTransitionType() > ArrayLength(s_TransitionColors))
        return s_TransitionColors[0];

    return s_TransitionColors[Transition->GetTransitionType()];
}

static bool
DoTransitions(node* Node)
{
    Assert(Node);

    {for (int32x TransitionIdx = 0; TransitionIdx < Node->GetNumTransitions(); ++TransitionIdx)
    {
        transition* Transition = Node->GetTransition(TransitionIdx);
        Assert(Transition->GetStartNode() == Node);

        node* OtherNode = Transition->GetEndNode();

        LuaPoint start;
        LuaPoint end;
        {
            int32x tsx, tsy, tex, tey;
            Transition->GetDrawingHints(tsx, tsy, tex, tey);

            int sx,sy;
            Node->GetPosition(sx, sy);
            start = LuaPoint(tsx + sx, tsy + sy);

            int ex,ey;
            OtherNode->GetPosition(ex, ey);
            end = LuaPoint(tex + ex, tey + ey);
        }

        LuaRect MyRect    = ComputeStateNodeRect(Node);
        Node->GetPosition(MyRect.x, MyRect.y);
        LuaRect OtherRect = ComputeStateNodeRect(OtherNode);
        OtherNode->GetPosition(OtherRect.x, OtherRect.y);
        {
            // We know that "start" is contained in MyRect, while "end" is contained in
            // OtherRect.  Clip them to the boundaries...
            LuaPoint newStart, newEnd;

            if (ClipToRect(MyRect,    start, end, newStart) &&
                ClipToRect(OtherRect, end, start, newEnd))
            {
                start = newStart;
                end   = newEnd;
            }
        }

        RegisterTransition(UILuaState(), Transition, start, end);

        DrawConnection(start, end, TransitionColor(Transition), edit::IsSelected(Transition));
    }}

    return true;
}


static void
DrawTransitionDrag(lua_State* L)
{
    PushTableFunction(L, "DragContext", "IsActive");
    lua_getglobal(L, "TransitionDragType");
    lua_pcall(L, 1, 1, 0);

    bool ActiveDrag = !!lua_toboolean(L, -1);
    lua_pop(L, 1);
    if (ActiveDrag)
    {
        PushTableFunction(L, "DragContext", "GetDragPoints");
        lua_pcall(L, 0, 2, 0);


        LuaPoint start, end;
        ExtractPoint(L, -2, start);
        ExtractPoint(L, -1, end);
        lua_pop(L, 2);

        ScreenToArea(start);
        ScreenToArea(end);

        DrawConnection(start, end, LuaColor(1, 1, 1), false);
    }
}


// struct FloatPoint
// {
//     float x;
//     float y;
//     FloatPoint(float x_, float y_) : x(x_), y(y_) { }
//     FloatPoint() : x(0), y(0) { }
// };
// FloatPoint NormalizeFP(FloatPoint const& pt)
// {
//     float l = pt.x*pt.x + pt.y*pt.y;

//     FloatPoint Result(0, 0);
//     if (l != 0)
//     {
//         float n = sqrt(l);
//         Result.x = pt.x / n;
//         Result.y = pt.y / n;
//     }

//     return Result;
// }

// void RelaxTransitions(vector<transition*> Transitions)
// {
//     if (Transitions.empty())
//         return;

//     static float CentroidForce   = 1.0f;
//     static float RepelOtherForce = 0.0001f;
//     static float CrossOverForce  = 0.0001f;

//     int NumTransitions = int(Transitions.size());

//     vector<LuaRect> StartNodeRects(NumTransitions);
//     vector<LuaRect> EndNodeRects(NumTransitions);

//     vector<FloatPoint> StartPoints(NumTransitions);
//     vector<FloatPoint> EndPoints(NumTransitions);

//     vector<FloatPoint> StartPointForces(NumTransitions);
//     vector<FloatPoint> EndPointForces(NumTransitions);

//     // Compute the start and end points, we'll use those a lot...
//     {for (int Idx = 0; Idx < NumTransitions; ++Idx)
//     {
//         transition* Transition = Transitions[Idx];
//         node* Node = Transition->GetStartNode();
//         node* OtherNode = Transition->GetEndNode();

//         real32 tsx, tsy, tex, tey;
//         Transition->GetDrawingHints(tsx, tsy, tex, tey);

//         int sx,sy;
//         Node->GetPosition(sx, sy);
//         StartPoints[Idx] = FloatPoint(tsx + sx, tsy + sy);

//         int ex,ey;
//         OtherNode->GetPosition(ex, ey);
//         EndPoints[Idx] = FloatPoint(tex + ex, tey + ey);

//         StartNodeRects[Idx] = ComputeStateNodeRect(Node);
//         StartNodeRects[Idx].x = sx;
//         StartNodeRects[Idx].y = sy;

//         EndNodeRects[Idx] = ComputeStateNodeRect(OtherNode);
//         EndNodeRects[Idx].x = ex;
//         EndNodeRects[Idx].y = ey;
//     }}

//     {for (int Idx = 0; Idx < NumTransitions; ++Idx)
//     {
//         // Compute the forces on the start point...
//         FloatPoint centroidVector = FloatPoint((EndNodeRects[Idx].x + EndNodeRects[Idx].w/2) - StartPoints[Idx].x,
//                                                (EndNodeRects[Idx].y + EndNodeRects[Idx].h/2) - StartPoints[Idx].y);

//         StartPointForces[Idx] = NormalizeFP(centroidVector);
//         StartPointForces[Idx].x *= CentroidForce;
//         StartPointForces[Idx].y *= CentroidForce;
//     }}

//     {for (int Idx = 0; Idx < NumTransitions; ++Idx)
//     {
//         transition* Transition = Transitions[Idx];

//         real32 StartXPrev, EndXPrev;
//         real32 StartYPrev, EndYPrev;
//         Transition->GetVerletHints(StartXPrev, StartYPrev, EndXPrev, EndYPrev);
//         StartXPrev += StartNodeRects[Idx].x;
//         StartYPrev += StartNodeRects[Idx].y;

//         // verlet integration of the next position.  This is:
//         // xNext = 2 * x - xPrev + accelX * tSquared

//         // t == 1 for our purposes...
//         float xNext = 2 * StartPoints[Idx].x - StartXPrev + StartPointForces[Idx].x;
//         float yNext = 2 * StartPoints[Idx].y - StartYPrev + StartPointForces[Idx].y;

//         FloatPoint NextPos(xNext - StartNodeRects[Idx].x, yNext - StartNodeRects[Idx].y);

//         // Clamp
//         if (NextPos.x < 0)
//             NextPos.x = 0;
//         else if (NextPos.x > StartNodeRects[Idx].w)
//             NextPos.x = StartNodeRects[Idx].w;

//         if (NextPos.y < 0)
//             NextPos.y = 0;
//         else if (NextPos.y > StartNodeRects[Idx].h)
//             NextPos.y = StartNodeRects[Idx].h;

//         Transition->SetDrawingHints(NextPos.x, NextPos.y, EndPoints[Idx].x - EndNodeRects[Idx].x, EndPoints[Idx].y - EndNodeRects[Idx].y);
//     }}
// }


int GRANNY
StateMachineRender(lua_State* L, state_machine* Machine)
{
    Assert(L && Machine);

    // Stack: zoom, ID
    real32 const ZoomFactor = GetZoomFactor();

    // Alt-Mouse move interaction
    {
        lua_pushinteger(L, edit::TokenizedToID(Machine));
        PushTableFunction(L, "MouseMoveEditRegion", "Do");

        // Push an id.  Note that on entry to this function, -1 was the parent id, which is
        // now at index -3.
        MakeLocalControlID(L, -3, -1);
        lua_pushnumber(L, ZoomFactor);

        // Call that function!
        lua_pcall(L, 2, 0, 0);
        lua_pop(L, 1);  // control id
    }

    lua_getglobal(L, "StateMachineHotkeys");
    lua_pushvalue(L, -2);
    lua_pcall(L, 1, 0, 0);

    bool Drawn = false;
    LuaRect BaseRect;
    int32x NumNodes = Machine->GetNumChildren();

    // Sort lines *behind* the nodes...
    {for (int Idx = 0; Idx < NumNodes; ++Idx)
    {
        node* Child = Machine->GetChild(Idx);
        if (!Child)
            continue;
        DoTransitions(Child);
    }}

    int32x ClippedNodes   = 0;
    int32x UnclippedNodes = 0;
    {for (int Idx = NumNodes-1; Idx >= 0; --Idx)
    {
        node* Child = Machine->GetChild(Idx);
        if (!Child)
            continue;

        bool IsStartState = false;
        {
            int StartIdx = Machine->GetStartStateIdx();
            if (StartIdx != node::eInvalidChild)
            {
                IsStartState = (Child == Machine->GetChild(StartIdx));
            }
        }

        bool IsCurrentState =
            (Child == Machine->GetActiveElement());

        LuaRect NodeRect;
        if (DoStateNode(Child,
                        edit::IsSelected(Child),
                        IsStartState,
                        IsCurrentState,
                        NodeRect))
        {
            ++ClippedNodes;
        }
        else
        {
            ++UnclippedNodes;
        }

        if (Drawn)
        {
            BaseRect.Union(NodeRect);
        }
        else
        {
            BaseRect = NodeRect;
            Drawn = true;
        }
    }}
    StTMPlotInt(ClippedNodes,   "AnimStudio/UI/Nodes/Clipped(nodes)");
    StTMPlotInt(UnclippedNodes, "AnimStudio/UI/Nodes/Unclipped(nodes)");

    DrawTransitionDrag(L);

    PushRect(L, BaseRect);
    lua_pushboolean(L, Drawn);
    return 2;
}
