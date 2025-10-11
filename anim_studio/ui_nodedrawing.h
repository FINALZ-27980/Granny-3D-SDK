// ========================================================================
// $File: //jeffr/granny_29/statement/ui_nodedrawing.h $
// $DateTime: 2012/05/09 17:10:12 $
// $Change: 37347 $
// $Revision: #10 $
//
// $Notice: $
// ========================================================================
#if !defined(UI_NODEDRAWING_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

#if !defined(GSTATE_TRANSITION_H)
#include "gstate_transition.h"
#endif


struct lua_State;
BEGIN_GRANNY_NAMESPACE;

struct LuaRect;
struct LuaPoint;
struct LuaColor;

bool UINodeDrawing_Register(lua_State*);
void UINodeDrawing_Shutdown();


// Used in both state and blend graph drawing
namespace node_drawing
{
    extern int32x s_TitleBarFont;
    extern int32x s_TitleBarFontHeight;
    extern int32x s_TitleBarFontBaseline;

    extern int32x s_TitleBarIconHeight;

    extern int32x s_MinTitleBarWidth;

    extern LuaColor s_NodeTitleText;
    extern LuaColor s_NodeTitleTextDrop;

    extern int32x s_EdgeFont;
    extern int32x s_EdgeFontHeight;
    extern int32x s_EdgeFontBaseline;

    extern LuaColor s_EdgeText;
    extern LuaColor s_EdgeTextDrop;

    extern int s_TopPadding;
    extern int s_BelowBarPadding;
    extern int s_BottomPadding;

    extern int s_SidePadding;
    extern int s_InternalEdgePadding;

    extern int const s_MinEdgeWidth;
    extern int const s_MaxEdgeWidth;
    extern int const s_BGParamSliderWidth;

    extern int const s_EdgeTopPadding;
    extern int const s_EdgeBottomPadding;
    extern int const s_EdgeSidePadding;
    extern real32 const s_ConnectionRadius;
    extern int s_EdgeHeight;

    extern LuaColor s_NodeColor;
    extern LuaColor s_DropShadowColor;
    extern LuaColor s_NodeSelectColor;

    // 0 = normal, 1 = highlight
    extern LuaColor s_EdgeColors[2][GSTATE EdgeTypeCount];
    extern LuaColor s_TransitionColors[GSTATE NumTransitionTypes];

    extern int32x s_ConnectionHandle;
    extern int32x s_ConnectionW;
    extern int32x s_ConnectionH;

    extern int32x s_StateNodeGrid;

    void InitNodeMetrics();
}

void DrawNodeDropShadow(LuaRect const& GlobalRect, bool Selected);
void DrawNode(LuaRect const& LocalRect, LuaColor const& Color);

void RegisterNodeMove(lua_State* L,
                      GSTATE node* Node,
                      LuaRect const& NodeRect,
                      LuaRect const& IconRect,
                      int32x Border,
                      bool IsReplaceableState);

void RegisterStateInteractions(lua_State* L,
                               GSTATE node* Node,
                               LuaRect const& NodeRect,
                               LuaRect const& StartStateRect,
                               LuaRect const& CurrStateRect);

void DoRenameableTitle(lua_State* L,
                       GSTATE node* Node,
                       int32x TitleBarFont,
                       int32x X,
                       int32x Y,
                       int32x W,
                       LuaColor const& FrontColor,
                       LuaColor const& DropColor);

void NodeControls(lua_State* L,
                  GSTATE node* Node,
                  LuaRect const& NodeRect);

LuaRect ComputeNodeRect(GSTATE node* Node);

void DoDragSelect(lua_State* L, GSTATE container* Container);
void DoComments(lua_State* L);

int EditBoxThisNode(lua_State* State);

int TriggerEventPoke(lua_State* L);

END_GRANNY_NAMESPACE;

#define UI_NODEDRAWING_H
#endif /* UI_NODEDRAWING_H */
