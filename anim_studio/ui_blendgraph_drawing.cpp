// ========================================================================
// $File: //jeffr/granny_29/statement/ui_blendgraph_drawing.cpp $
// $DateTime: 2012/10/22 16:29:12 $
// $Change: 39907 $
// $Revision: #14 $
//
// $Notice: $
// ========================================================================
#include "ui_blendgraph_drawing.h"
#include "statement.h"
#include "statement_undostack.h"

#include "ui_area.h"
#include "ui_bitmap.h"
#include "ui_controls.h"
#include "ui_core.h"
#include "ui_drawing.h"
#include "ui_font.h"
#include "ui_nodedrawing.h"

#include "gstate_parameters.h"
#include "gstate_event_source.h"
#include "statement_editstate.h"
#include "luautils.h"
#include "granny_assert.h"
#include "granny_math.h"
#include <algorithm>

#include <windows.h>
#include <gl/gl.h>
#include <math.h>

USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;
using namespace node_drawing;
using namespace std;

// For the containing graph...
static LuaPoint
ComputeInternalInputLoc(node*          ContainerNode,
                        LuaRect const& ScreenRect,
                        int32x         InputIdx)
{
    InitNodeMetrics();
    Assert(ContainerNode->IsInputInternal(InputIdx));

    LuaPoint computed(0, ScreenRect.y);

    // Advance to the top of the edge, skipping externals
    {for (int Idx = 0; Idx < InputIdx; ++Idx)
    {
        if (ContainerNode->IsInputInternal(Idx))
            computed.y += s_EdgeHeight;
    }}

    // And then to the center of the correct one
    computed.y += s_EdgeHeight / 2;

    // Internal inputs are on the right edge, and are always "max width"
    computed.x = ScreenRect.x + ScreenRect.w - s_MaxEdgeWidth;

    return computed;
}

// For the containing graph...
static LuaPoint
ComputeInternalOutputLoc(node*          ContainerNode,
                         LuaRect const& ScreenRect,
                         int32x         OutputIdx)
{
    InitNodeMetrics();
    Assert(ContainerNode->IsOutputInternal(OutputIdx));

    LuaPoint computed(0, ScreenRect.y);

    // Advance to the top of the edge, skipping externals
    {for (int Idx = 0; Idx < OutputIdx; ++Idx)
    {
        if (ContainerNode->IsOutputInternal(Idx))
            computed.y += s_EdgeHeight;
    }}

    // And then to the center of the correct one
    computed.y += s_EdgeHeight / 2;

    // Internal inputs are on the left edge, and are always "max width"
    computed.x = ScreenRect.x + s_MaxEdgeWidth;

    return computed;
}

static LuaPoint
ComputeInputLoc(node*          Node,
                LuaRect const& NodeRect,
                int32x         InputIdx)
{
    InitNodeMetrics();
    Assert(Node->IsInputExternal(InputIdx));

    LuaPoint computed(0, 0);

    // Start with the title bits
    computed.y = NodeRect.y + (s_TopPadding +
                               s_TitleBarIconHeight +
                               s_BelowBarPadding);

    // Advance to the top of the edge, skipping internal
    {for (int Idx = 0; Idx < InputIdx; ++Idx)
    {
        if (Node->IsInputExternal(Idx))
            computed.y += (s_EdgeHeight-1); // remember the minus one to line up the outlines...
    }}

    // And then to the center of the correct one
    computed.y += s_EdgeHeight / 2;

    // Inputs are on the left edge
    computed.x = NodeRect.x;

    return computed;
}

// Duplicated, sadly
static LuaPoint
ComputeOutputLoc(node*          Node,
                 LuaRect const& NodeRect,
                 int32x         OutputIdx)
{
    InitNodeMetrics();
    Assert(Node->IsOutputExternal(OutputIdx));

    LuaPoint computed(0, 0);

    // Start with the title bits
    computed.y = NodeRect.y + (s_TopPadding +
                               s_TitleBarIconHeight +
                               s_BelowBarPadding);

    // Advance to the top of the edge, skipping internals
    {for (int Idx = 0; Idx < OutputIdx; ++Idx)
    {
        if (Node->IsOutputExternal(Idx))
            computed.y += (s_EdgeHeight-1);
    }}

    // And then to the center of the correct one
    computed.y += s_EdgeHeight / 2;

    // Outputs are on the right edge
    computed.x = NodeRect.x + NodeRect.w;

    return computed;
}


static LuaRect
BlendGraphNodeRectWithEdges(node* Node, int32x& InputEdgeWidth, int32x& OutputEdgeWidth)
{
    InitNodeMetrics();

    LuaRect NodeRect(0, 0, 0, 0);

    // To compute the edge width, we need to examine the widths of the external outputs.
    // We'll also count the number of external inputs and outputs here which will give us
    // the count we need to compute the height below...
    int NumExternalInputs = 0;
    int NumExternalOutputs = 0;
    int MaxInputWidth = s_MinEdgeWidth;
    int MaxOutputWidth = s_MinEdgeWidth;

    int EventButtonHeight = -1;
    {
        {for (int Idx = 0; Idx < Node->GetNumInputs(); ++Idx)
        {
            char const* EdgeName = Node->GetInputName(Idx);

            if (!Node->IsInputExternal(Idx))
                continue;

            ++NumExternalInputs;
            int32x w,h;
            if (UIGetTextDimension(s_EdgeFont, EdgeName, 0, w, h))
            {
                int32x FullWidth = w + 2 * s_EdgeSidePadding;
                MaxInputWidth = max(FullWidth, MaxInputWidth);
            }
        }}

        {for (int Idx = 0; Idx < Node->GetNumOutputs(); ++Idx)
        {
            char const* EdgeName = Node->GetOutputName(Idx);

            if (!Node->IsOutputExternal(Idx))
                continue;

            ++NumExternalOutputs;
            int32x w,h;
            if (UIGetTextDimension(s_EdgeFont, EdgeName, 0, w, h))
            {
                int32x FullWidth = w + 2 * s_EdgeSidePadding;
                MaxOutputWidth = max(FullWidth, MaxOutputWidth);
            }
        }}

        // If this is to be a parameters node, there will be no inputs, but leave room for
        // sliders over on the side...
        parameters*   Param  = GSTATE_DYNCAST(Node, parameters);
        event_source* Events = GSTATE_DYNCAST(Node, event_source);
        if (Param)
        {
            Assert(Node->GetNumInputs() == 0);
            MaxInputWidth = s_BGParamSliderWidth;
        }
        else if (Events)
        {
            Assert(Node->GetNumInputs() == 0);
            MaxInputWidth = s_BGParamSliderWidth;

            // Also we need to grow the
            EventButtonHeight = Events->NumPossibleEvents() * SmallButtonHeight();
        }

        // don't let them grow unbounded
        InputEdgeWidth  = min(MaxInputWidth,  s_MaxEdgeWidth);
        OutputEdgeWidth = min(MaxOutputWidth, s_MaxEdgeWidth);
    }

    // Compute the width of the title
    int TitleBarWidth = s_MinTitleBarWidth;
    {
        TitleBarWidth = 24 + 2*s_SidePadding;

        int32x w,h;
        if (UIGetTextDimension(s_TitleBarFont, Node->GetName(), 0, w, h))
        {
            TitleBarWidth += w;
        }

        // todo: bitmap for the buttons
        {
            if (UIGetTextDimension(s_TitleBarFont, "x", 0, w, h))
            {
                TitleBarWidth += w + 2*s_SidePadding;
            }

            if (GSTATE_DYNCAST(Node, container) != 0)
            {
                if (UIGetTextDimension(s_TitleBarFont, "d", 0, w, h))
                    TitleBarWidth += w + 2*s_SidePadding;
            }
        }

        TitleBarWidth += 20;
    }

    int32x TotalEdgeWidth = (InputEdgeWidth +
                             s_InternalEdgePadding * 2 +
                             OutputEdgeWidth);


    // Finalize the width
    NodeRect.w = (s_SidePadding +
                  max(TotalEdgeWidth, TitleBarWidth) +
                  s_SidePadding);

    // Now the height.
    // We have, top to bottom, the following elements:
    int const MaxEdges   = max(NumExternalInputs, NumExternalOutputs);
    int const EdgeHeight = MaxEdges * s_EdgeHeight;

    int const EdgeRegionHeight = max(EdgeHeight, EventButtonHeight);

    NodeRect.h += (s_TopPadding +
                   s_TitleBarIconHeight +
                   s_BelowBarPadding +
                   EdgeRegionHeight +
                   s_BottomPadding);

    return NodeRect;
}

static void
DrawConnection(LuaPoint const& InputLoc,
               LuaPoint const& OutputLoc,
               LuaColor const& ConnectionColor,
               bool Circles,
               bool Selected)
{
    // glDisable(GL_TEXTURE_2D);
    // glDisable(GL_BLEND);
    // glColor3fv(&ConnectionColor.r);

    // Draw connection between the input and output loci
    {
        float ex = (float)InputLoc.x - 3;
        float ey = (float)InputLoc.y;
        float sx = (float)OutputLoc.x + 3;
        float sy = (float)OutputLoc.y;

        static float f = 450;
        float deriv = float(2 * Clamp(0, fabs(ex-sx), f));

        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        SetLineWidth(2.5f);

        if (Selected)
        {
            float sey = ey + 2;
            float ssy = sy + 2;

            glColor4f(0, 0, 0, 0.25f);
            glBegin(GL_LINE_STRIP);
            for (int i = 0; i < 50; i++)
            {
                float t = i / 49.0f;
                float t2 = t*t;
                float t3 = t2*t;

                float x = (2*t3 - 3*t2 + 1) * sx + (t3 - 2*t2 + t) * deriv + (-2*t3 + 3*t2) * ex + (t3 - t2) * deriv;
                float y = (2*t3 - 3*t2 + 1) * ssy +                           (-2*t3 + 3*t2) * sey;

                glVertex2f(x, y);
            }
            glEnd();
        }


        glColor3fv(&ConnectionColor.r);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < 50; i++)
        {
            float t = i / 49.0f;
            float t2 = t*t;
            float t3 = t2*t;

            float x = (2*t3 - 3*t2 + 1) * sx + (t3 - 2*t2 + t) * deriv + (-2*t3 + 3*t2) * ex + (t3 - t2) * deriv;
            float y = (2*t3 - 3*t2 + 1) * sy +                           (-2*t3 + 3*t2) * ey;

            glVertex2f(x, y);
        }
        glEnd();
    }

    if (Circles)
    {
        // Draw half-circle at the input location (facing left, -x)
        UIRenderBitmapStretchedModulate(s_ConnectionHandle,
                                        LuaRect(InputLoc.x,
                                                InputLoc.y - (s_ConnectionH/2),
                                                -s_ConnectionW, s_ConnectionH),
                                        ConnectionColor);

        // Draw half-circle at the output location (facing right, +x)
        UIRenderBitmapStretchedModulate(s_ConnectionHandle,
                                        LuaRect(OutputLoc.x,
                                                OutputLoc.y - (s_ConnectionH/2),
                                                s_ConnectionW, s_ConnectionH),
                                        ConnectionColor);
    }
}

static bool
DoGraphConnections(node* Node, bool RenderSelected)
{
    Assert(Node);

    LuaRect ScreenRect = UIAreaGet();

    LuaRect NodeRect = ComputeBlendGraphNodeRect(Node);
    Node->GetPosition(NodeRect.x, NodeRect.y);

    // We only loop through the input connections, since outputs don't store the nodes on
    // the other side, 'natch.
    {for (int Idx = 0; Idx < Node->GetNumInputs(); ++Idx)
    {
        if (Node->IsInputInternal(Idx))
            continue;

        node* OtherNode = 0;
        int32x OutputIdx;
        Node->GetInputConnection(Idx, &OtherNode, &OutputIdx);

        // No connection, carry on.
        if (OtherNode == 0)
            continue;
        Assert(OutputIdx >= 0 && OutputIdx < OtherNode->GetNumOutputs());
        Assert(OtherNode->GetOutputType(OutputIdx) == Node->GetInputType(Idx));

        // Check to see if this is the correct phase...
        bool EitherSelected = (edit::IsSelected(OtherNode) || edit::IsSelected(Node));
        if (RenderSelected && !EitherSelected)
            continue;
        else if (!RenderSelected && EitherSelected)
            continue;

        LuaRect OtherRect = ComputeBlendGraphNodeRect(OtherNode);
        OtherNode->GetPosition(OtherRect.x, OtherRect.y);

        // Compute input locus on our node
        LuaPoint InputLoc = ComputeInputLoc(Node, NodeRect, Idx);

        // Compute output locus on other node
        LuaPoint OutputLoc;
        if (OtherNode->IsOutputExternal(OutputIdx))
        {
            OutputLoc = ComputeOutputLoc(OtherNode, OtherRect, OutputIdx);
        }
        else
        {
            Assert(OtherNode == Node->GetParent());
            OutputLoc = ComputeInternalOutputLoc(OtherNode, ScreenRect, OutputIdx);
        }

        // Get Data path color
        LuaColor ConnectionColor = s_EdgeColors[0][Node->GetInputType(Idx)];

        DrawConnection(InputLoc, OutputLoc, ConnectionColor, true, RenderSelected);
    }}

    return true;
}

static bool
DoInternalConnections(blend_graph* Graph)
{
    Assert(Graph);

    LuaRect ScreenRect = UIAreaGet();

    // We only loop through the input connections, since outputs don't store the nodes on
    // the other side, 'natch.
    {for (int Idx = 0; Idx < Graph->GetNumInputs(); ++Idx)
    {
        if (Graph->IsInputExternal(Idx))
            continue;

        node*  OtherNode;
        int32x OutputIdx;
        Graph->GetInputConnection(Idx, &OtherNode, &OutputIdx);

        // No connection, carry on.
        if (OtherNode == 0)
            continue;

        LuaPoint InputLoc, OutputLoc;
        LuaColor ConnectionColor;

        if (OtherNode != Graph)
        {
            Assert(OutputIdx >= 0 && OutputIdx < OtherNode->GetNumOutputs());
            Assert(OtherNode->GetOutputType(OutputIdx) == Graph->GetInputType(Idx));
            Assert(OtherNode->IsOutputExternal(OutputIdx));

            LuaRect OtherRect = ComputeBlendGraphNodeRect(OtherNode);
            OtherNode->GetPosition(OtherRect.x, OtherRect.y);

            InputLoc = ComputeInternalInputLoc(Graph, ScreenRect, Idx);
            OutputLoc = ComputeOutputLoc(OtherNode, OtherRect, OutputIdx);
            ConnectionColor = s_EdgeColors[0][Graph->GetInputType(Idx)];
        }
        else
        {
            Assert(OutputIdx >= 0 && OutputIdx < OtherNode->GetNumOutputs());
            Assert(OtherNode->GetOutputType(OutputIdx) == Graph->GetInputType(Idx));
            Assert(OtherNode->IsOutputInternal(OutputIdx));

            InputLoc  = ComputeInternalInputLoc(Graph, ScreenRect, Idx);
            OutputLoc = ComputeInternalOutputLoc(OtherNode, ScreenRect, OutputIdx);
            ConnectionColor = s_EdgeColors[0][Graph->GetInputType(Idx)];
        }

        DrawConnection(InputLoc, OutputLoc, ConnectionColor, true, false);
    }}

    return true;
}


static bool
RegisterNodeInput(node*          Node,
                  LuaRect const& Rect,
                  int32x         EdgeIndex,
                  node_edge_type Type)
{
    lua_State* L = UILuaState();

    lua_getglobal(L, "RegisterNodeInput");
    lua_pushinteger(L, edit::TokenizedToID(Node));
    PushRect(L, Rect);
    lua_pushinteger(L, EdgeIndex);
    lua_pushinteger(L, Type);
    lua_pcall(L, 4, 1, 0);

    bool ThisTypeActive = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    return ThisTypeActive;
}

static bool
RegisterNodeOutput(node*          Node,
                   LuaRect const& Rect,
                   int32x         EdgeIndex,
                   node_edge_type Type)
{
    lua_State* L = UILuaState();

    lua_getglobal(L, "RegisterNodeOutput");
    lua_pushinteger(L, edit::TokenizedToID(Node));
    PushRect(L, Rect);
    lua_pushinteger(L, EdgeIndex);
    lua_pushinteger(L, Type);
    lua_pcall(L, 4, 1, 0);

    bool ThisEdgeActive = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    return ThisEdgeActive;
}

static bool
DoBlendGraphNode(node* Node, bool Selected, LuaRect& DrawnRect)
{
    StTMZone(__FUNCTION__);

    InitNodeMetrics();
    lua_State* L = UILuaState();
    Assert(Node && L);

    // Only used for parameters...
    ControlIDGen IDGen("bg_slieve_controls_%x_%d", edit::TokenizedToID(Node), Node->GetNumOutputs());

    int32x InputEdgeWidth  = s_MinEdgeWidth;
    int32x OutputEdgeWidth = s_MinEdgeWidth;

    // First, let's compute the rect that we'll be working with.  Note that this rect is
    // at the origin, so move it out to where the node actually is
    LuaRect ChildRect = BlendGraphNodeRectWithEdges(Node, InputEdgeWidth, OutputEdgeWidth);
    LuaRect ParentRect = ChildRect;
    Node->GetPosition(ParentRect.x, ParentRect.y);

    // Compute the node icon rect for passing to the move function...
    int32x Small=-1;
    LuaRect IconRect;
    {
        int32x Large;
        edit::GetNodeIcons(Node,Large,Small);

        int32x w, h;
        UIGetBitmapWidthAndHeight(Small, w, h);
        IconRect = LuaRect(s_SidePadding,
                           s_TopPadding,
                           w, h);
    }

    // And now the node proper.  We'll push onto the area stack just to ensure that
    // everything is easy and properly clipped for the interactions
    if (UIAreaPush(ParentRect, ChildRect))
    {
        DrawNodeDropShadow(ChildRect, Selected);
        DrawNode(ChildRect, Selected ? s_NodeSelectColor : s_NodeColor);

        UIRenderBitmap(Small, s_SidePadding, s_TopPadding);

        RegisterNodeMove(L, Node, ChildRect, IconRect, 0, false);
        NodeControls(L, Node, ChildRect);

        // Draw Title
        int32x X = 24 + 2*s_SidePadding;
        int32x Y = s_TopPadding;
        int32x W = ChildRect.w - X - 15; // @@magic
        DoRenameableTitle(L, Node, s_TitleBarFont,
                          X, Y, W,
                          s_NodeTitleText, s_NodeTitleTextDrop);

        // Draw forwarded edges, if any.  We do this first to bury the lines *behind* the input/output boxes
        {
            int const NumOutputs = Node->GetNumOutputs();
            int InputX  = s_SidePadding;
            int OutputX = ChildRect.w - OutputEdgeWidth - s_SidePadding;
            int CurrY = (s_TopPadding + s_TitleBarIconHeight + s_BelowBarPadding);

            {for (int OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
            {
                if (Node->IsOutputInternal(OutputIdx))
                    continue;

                granny_int32x InputIdx = Node->GetOutputPassthrough(OutputIdx);
                if (InputIdx == -1)
                    continue;

                LuaRect OutputRect(OutputX, CurrY + OutputIdx * (s_EdgeHeight-1), OutputEdgeWidth, s_EdgeHeight);
                LuaRect InputRect(InputX,   CurrY + InputIdx  * (s_EdgeHeight-1), InputEdgeWidth,  s_EdgeHeight);

                LuaPoint StartPt(InputRect.x + InputRect.w - 2, InputRect.y + InputRect.h/2);
                LuaPoint EndPt(OutputRect.x + 2, OutputRect.y + OutputRect.h/2);
                {
                    glDisable(GL_TEXTURE_2D);
                    glEnable(GL_BLEND);
                    SetLineWidth(2);

                    glColor3fv(&s_EdgeColors[0][Node->GetOutputType(OutputIdx)].r);
                    glBegin(GL_LINES);
                    {
                        glVertex2i(StartPt.x, StartPt.y);
                        glVertex2i(EndPt.x,   EndPt.y);
                    }
                    glEnd();
                }
            }}
        }

        // for all inputs: DrawInputEdge
        int const NumInputs  = Node->GetNumInputs();
        int CurrX = s_SidePadding;
        int CurrY = (s_TopPadding + s_TitleBarIconHeight + s_BelowBarPadding);
        {for (int EdgeIdx = 0; EdgeIdx < NumInputs; ++EdgeIdx)
        {
            // Skip the internals
            if (Node->IsInputExternal(EdgeIdx) == false)
                continue;

            node_edge_type EdgeType = Node->GetInputType(EdgeIdx);
            char const*    EdgeName = Node->GetInputName(EdgeIdx);

            LuaRect InputChildRect(0, 0, InputEdgeWidth, s_EdgeHeight);
            LuaRect InputParentRect = InputChildRect;
            InputParentRect.x = CurrX;
            InputParentRect.y = CurrY;

            if (UIAreaPush(InputParentRect, InputChildRect))
            {
                // Register this as an input target
                bool Highlight = RegisterNodeInput(Node, InputChildRect, EdgeIdx, EdgeType);
                LuaColor const& EdgeColor = s_EdgeColors[Highlight?1:0][EdgeType];

                Assert(EdgeType >= PoseEdge && EdgeType < EdgeTypeCount);
                UIRenderBitmapStretchedModulate(UIGetBitmapHandle("button_rest_fill"),
                                                InputChildRect, EdgeColor);

                RenderDropText(s_EdgeFont, EdgeName, 0,
                               s_EdgeSidePadding,
                               s_EdgeTopPadding + s_EdgeFontBaseline,
                               s_EdgeText, s_EdgeTextDrop);


            }
            UIAreaPop();

            // You may be asking: why -1?  We want the dark lines for subsequent edges to overlap
            CurrY += (s_EdgeHeight - 1);
        }}

        parameters*   AsParam = GSTATE_DYNCAST(Node, parameters);
        event_source* AsEvent = GSTATE_DYNCAST(Node, event_source);

        // for all outputs: DrawOutputEdge (with slider if parameter node)
        // reset CurrX/Y
        int const NumOutputs = Node->GetNumOutputs();
        CurrX = ChildRect.w - OutputEdgeWidth - s_SidePadding;
        //CurrX = s_SidePadding + InputEdgeWidth + s_InternalEdgePadding;
        CurrY = (s_TopPadding + s_TitleBarIconHeight + s_BelowBarPadding);
        {for (int EdgeIdx = 0; EdgeIdx < NumOutputs; ++EdgeIdx)
        {
            // Skip the internals
            if (Node->IsOutputExternal(EdgeIdx) == false)
                continue;

            node_edge_type EdgeType = Node->GetOutputType(EdgeIdx);
            char const*    EdgeName = Node->GetOutputName(EdgeIdx);

            LuaRect OutputChildRect(0, 0, OutputEdgeWidth, s_EdgeHeight);
            LuaRect OutputParentRect = OutputChildRect;
            OutputParentRect.x = CurrX;
            OutputParentRect.y = CurrY;

            if (UIAreaPush(OutputParentRect, OutputChildRect))
            {
                // Register this as a drag source
                bool Highlight = RegisterNodeOutput(Node, OutputChildRect, EdgeIdx, EdgeType);
                LuaColor const& EdgeColor = s_EdgeColors[Highlight?1:0][EdgeType];

                Assert(EdgeType >= PoseEdge && EdgeType < EdgeTypeCount);
                UIRenderBitmapStretchedModulate(UIGetBitmapHandle("button_rest_fill"),
                                                OutputChildRect, EdgeColor);

                RenderDropText(s_EdgeFont, EdgeName, 0,
                               s_EdgeSidePadding,
                               s_EdgeTopPadding + s_EdgeFontBaseline,
                               s_EdgeText, s_EdgeTextDrop);
            }
            UIAreaPop();

            if (AsParam)
            {
                // Draw a slider on the input side...
                float Min, Max;
                float Value = AsParam->SampleScalarOutput(EdgeIdx, 0);
                AsParam->GetScalarOutputRange(EdgeIdx, &Min, &Max);

                LuaRect Rect(5, CurrY, 100, SmallSliderHeight());
                if (SmallSliderOn(Min, Max, Value, AsParam->GetClampSliderToInts(), Rect, IDGen))
                {
                    PushUndoPos("change parameter", false, eSliderMotion, AsParam->GetUID());
                    AsParam->SetParameter(EdgeIdx, Value);
                    AsParam->SetParameterDefault(EdgeIdx, Value);
                }
            }
            else if (AsEvent)
            {
                // Draw buttons on the input side.  Note that there should only be one output...
                Assert(EdgeIdx == 0);

                int ButtonHeight = SmallButtonHeight();
                for (int i = 0; i < AsEvent->NumPossibleEvents(); ++i)
                {
                    LuaRect Rect(5, CurrY + i * ButtonHeight, 100, ButtonHeight);
                    PushTokenizedIntClosure(AsEvent, TriggerEventPoke, i);
                    if (SmallButtonAtRectWithCallback(AsEvent->GetPossibleEvent(i), Rect, true, IDGen))
                    {
                        //
                    }
                }
            }

            // You may be asking: why -1?  We want the dark lines for subsequent edges to overlap
            CurrY += (s_EdgeHeight - 1);
        }}
    }
    else
    {
        RegisterNodeMove(L, Node, ChildRect, IconRect, 0, false);
    }
    UIAreaPop();

    DrawnRect = ParentRect;
    return true;
}


static bool
DoInternalInputsOutputs(blend_graph* Graph)
{
    InitNodeMetrics();
    lua_State* L = UILuaState();

    Assert(Graph && L);

    LuaRect ScreenRect = UIAreaGet();

    // for all inputs: DrawInputEdge
    int const NumInputs  = Graph->GetNumInputs();
    int CurrX = ScreenRect.x + ScreenRect.w - s_MaxEdgeWidth;
    int CurrY = ScreenRect.y;
    {for (int EdgeIdx = 0; EdgeIdx < NumInputs; ++EdgeIdx)
    {
        // Skip the externals
        if (Graph->IsInputExternal(EdgeIdx))
            continue;

        node_edge_type EdgeType = Graph->GetInputType(EdgeIdx);
        char const*    EdgeName = Graph->GetInputName(EdgeIdx);

        LuaRect InputChildRect(0, 0, s_MaxEdgeWidth, s_EdgeHeight);
        LuaRect InputParentRect = InputChildRect;
        InputParentRect.x = CurrX;
        InputParentRect.y = CurrY;

        if (UIAreaPush(InputParentRect, InputChildRect))
        {
            // Register this as an input target
            bool Highlight = RegisterNodeInput(Graph, InputChildRect, EdgeIdx, EdgeType);
            LuaColor const& EdgeColor = s_EdgeColors[Highlight?1:0][EdgeType];

            Assert(EdgeType >= PoseEdge && EdgeType < EdgeTypeCount);
            UIRenderBitmapStretchedModulate(UIGetBitmapHandle("button_rest_fill"),
                                            InputChildRect, EdgeColor);
            RenderDropText(s_EdgeFont, EdgeName, 0,
                           s_EdgeSidePadding,
                           s_EdgeTopPadding + s_EdgeFontBaseline,
                           s_EdgeText, s_EdgeTextDrop);
        }
        UIAreaPop();

        // You may be asking: why -1?  We want the dark lines for subsequent edges to overlap
        CurrY += (s_EdgeHeight - 1);
    }}

    // for all outputs: DrawOutputEdge
    // reset CurrX/Y
    int const NumOutputs = Graph->GetNumOutputs();
    CurrX = ScreenRect.x;
    CurrY = ScreenRect.y;
    {for (int EdgeIdx = 0; EdgeIdx < NumOutputs; ++EdgeIdx)
    {
        // Skip the externals
        if (Graph->IsOutputExternal(EdgeIdx))
            continue;

        node_edge_type EdgeType = Graph->GetOutputType(EdgeIdx);
        char const*    EdgeName = Graph->GetOutputName(EdgeIdx);

        LuaRect OutputChildRect(0, 0, s_MaxEdgeWidth, s_EdgeHeight);
        LuaRect OutputParentRect = OutputChildRect;
        OutputParentRect.x = CurrX;
        OutputParentRect.y = CurrY;

        if (UIAreaPush(OutputParentRect, OutputChildRect))
        {
            // Register this as a drag source
            bool Highlight = RegisterNodeOutput(Graph, OutputChildRect, EdgeIdx, EdgeType);
            LuaColor const& EdgeColor = s_EdgeColors[Highlight?1:0][EdgeType];

            Assert(EdgeType >= PoseEdge && EdgeType < EdgeTypeCount);
            UIRenderBitmapStretchedModulate(UIGetBitmapHandle("button_rest_fill"),
                                            OutputChildRect, EdgeColor);

            RenderDropText(s_EdgeFont, EdgeName, 0,
                           s_EdgeSidePadding,
                           s_EdgeTopPadding + s_EdgeFontBaseline,
                           s_EdgeText, s_EdgeTextDrop);
        }
        UIAreaPop();

        // You may be asking: why -1?  We want the dark lines for subsequent edges to overlap
        CurrY += (s_EdgeHeight - 1);
    }}

    return true;
}

static void
DrawConnectionDrag(lua_State* L)
{
    int32x EdgeTypes[EdgeTypeCount] = { PoseEdge, ScalarEdge, MaskEdge, EventEdge, MorphEdge };

    bool Active = false;
    LuaColor ConnectionColor;
    for (int i = 0; i < EdgeTypeCount; i++)
    {
        PushTableFunction(L, "DragContext", "IsActive");
        lua_pushinteger(L, EdgeTypes[i]);
        lua_pcall(L, 1, 1, 0);

        bool ThisTypeActive = !!lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (ThisTypeActive)
        {
            Active = true;
            ConnectionColor = s_EdgeColors[0][EdgeTypes[i]];
            break;
        }
    }

    if (!Active)
        return;

    PushTableFunction(L, "DragContext", "GetDragPoints");
    lua_pcall(L, 0, 2, 0);

    LuaPoint start, end;
    ExtractPoint(L, -2, start);
    ExtractPoint(L, -1, end);
    lua_pop(L, 2);

    ScreenToArea(start);
    ScreenToArea(end);

    DrawConnection(end, start, ConnectionColor, false, true);
}

// silly callback to shear off the edge feedback
LuaRect GRANNY
ComputeBlendGraphNodeRect(node* Node)
{
    int32x Ignore0, Ignore1;
    return BlendGraphNodeRectWithEdges(Node, Ignore0, Ignore1);
}


int GRANNY
BlendGraphRender(lua_State* L, blend_graph* BlendGraph)
{
    Assert(L && BlendGraph);

    // Stack: zoom, ID
    real32 const ZoomFactor = GetZoomFactor();

    // Alt-Mouse move interaction
    {
        lua_pushinteger(L, edit::TokenizedToID(BlendGraph));
        PushTableFunction(L, "MouseMoveEditRegion", "Do");

        // Push an id.  Note that on entry to this function, -1 was the parent id, which is
        // now at index -3.
        MakeLocalControlID(L, -3, -1);
        lua_pushnumber(L, ZoomFactor);

        // Call that function!
        lua_pcall(L, 2, 0, 0);
        lua_pop(L, 1);  // control id
    }

    lua_getglobal(L, "BlendGraphHotkeys");
    lua_pushvalue(L, -2);
    lua_pcall(L, 1, 0, 0);

    bool Drawn = false;
    LuaRect BaseRect;

    int32x NumNodes = BlendGraph->GetNumChildren();


    // Internal inputs and outputs must be rendered
    DoInternalInputsOutputs(BlendGraph);

    // Render anything not connected to a selected node
    {for (int Idx = 0; Idx < NumNodes; ++Idx)
    {
        node* Child = BlendGraph->GetChild(Idx);
        if (!Child)
            continue;
        DoGraphConnections(Child, false);
    }}
    DoInternalConnections(BlendGraph);

    {for (int Idx = NumNodes-1; Idx >= 0; --Idx)
    {
        node* Child = BlendGraph->GetChild(Idx);
        if (!Child)
            continue;

        LuaRect NodeRect;
        DoBlendGraphNode(Child, edit::IsSelected(Child), NodeRect);

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

    // And, render anything connected to a selected node
    {for (int Idx = 0; Idx < NumNodes; ++Idx)
    {
        node* Child = BlendGraph->GetChild(Idx);
        if (!Child)
            continue;
        DoGraphConnections(Child, true);
    }}
    DoInternalConnections(BlendGraph);

    DrawConnectionDrag(L);

    PushRect(L, BaseRect);
    lua_pushboolean(L, Drawn);
    return 2;
}

