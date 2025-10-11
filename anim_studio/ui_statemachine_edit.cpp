// ========================================================================
// $File: //jeffr/granny_29/statement/ui_statemachine_edit.cpp $
// $DateTime: 2012/10/22 16:29:12 $
// $Change: 39907 $
// $Revision: #12 $
//
// $Notice: $
// ========================================================================
#include "ui_statemachine_edit.h"
#include "ui_nodedrawing.h"
#include "statement.h"
#include "statement_undostack.h"
#include "statement_editstate.h"
#include "statement_xinput.h"

#include "granny_math.h"
#include "gstate_node_header_list.h"
#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"

#include "gstate_transition.h"
#include "gstate_transition_onloop.h"
#include "gstate_transition_onsubloop.h"

#include "gstate_scalar_compare.h"
#include "gstate_event_triggered.h"

#include "granny_model.h"
#include "granny_skeleton.h"

#include "luautils.h"
#include "statement_editstate.h"
#include "ui_controls.h"
#include "ui_font.h"
#include "ui_drawing.h"
#include "ui_area.h"

#include <vector>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;
using std::vector;
using std::string;
using namespace edit;

vector<char const*> s_ConditionalTypeStrings;
vector<char const*> s_ScalarOpStrings;

static void
InitTypeStrings()
{
    if (s_ConditionalTypeStrings.empty())
    {
        s_ConditionalTypeStrings.push_back("Scalar Compare");
        s_ConditionalTypeStrings.push_back("Event Triggered");
    }

    if (s_ScalarOpStrings.empty())
    {
        s_ScalarOpStrings.push_back("<");   // ScalarCompare_Less     = 0
        s_ScalarOpStrings.push_back(">");   // ScalarCompare_Greater  = 1
        s_ScalarOpStrings.push_back("<=");  // ScalarCompare_LEqual   = 2
        s_ScalarOpStrings.push_back(">=");  // ScalarCompare_GEqual   = 3
        s_ScalarOpStrings.push_back("==");  // ScalarCompare_Equal    = 4
        s_ScalarOpStrings.push_back("!=");  // ScalarCompare_NotEqual = 5
    }
}


static void
GatherValidEdges(state_machine*       Machine,
                 bool                 IncludeParameters,
                 bool                 IncludeEventSources,
                 vector<char const*>& EdgeNames,
                 vector<node*>&       EdgeNodes,
                 vector<int>&         EdgeIndices,
                 node_edge_type       Type)
{
    {for (int Idx = 0; Idx < Machine->GetNumOutputs(); ++Idx)
    {
        if (Machine->GetOutputType(Idx) == Type)
        {
            // Clunky, but keeps it in the system...
            char* NewString = 0;
            GStateCloneString(NewString, Machine->GetOutputName(Idx));
            EdgeNames.push_back(NewString);

            EdgeNodes.push_back(Machine);
            EdgeIndices.push_back(Idx);
        }
    }}

    {for (int Idx = 0; Idx < Machine->GetNumChildren(); ++Idx)
    {
        node* AsNode = Machine->GetChild(Idx);
        parameters*   AsParameters  = IncludeParameters   ? GSTATE_DYNCAST(AsNode, parameters)   : 0;
        event_source* AsEventSource = IncludeEventSources ? GSTATE_DYNCAST(AsNode, event_source) : 0;

        if (AsParameters == 0 && AsEventSource == 0)
            continue;

        {for (int Idx = 0; Idx < AsNode->GetNumOutputs(); ++Idx)
        {
            if (AsNode->GetOutputType(Idx) == Type)
            {
                // Setup the name as "edge name (on 'nodename')" for the parameters.
                // That's 8 extra characters for those playing around at home
                int const TotalLen = strlen(AsNode->GetName()) + 8 + strlen(AsNode->GetOutputName(Idx)) + 1;

                char* LabeledName = (char*)GStateAlloc(TotalLen);
                sprintf(LabeledName, "%s (on '%s')", 
                        AsNode->GetOutputName(Idx),
                        AsNode->GetName());

                EdgeNames.push_back(LabeledName);
                EdgeNodes.push_back(AsNode);
                EdgeIndices.push_back(Idx);
            }
        }}
    }}
}

static void
ReleaseEdgeVectors(vector<char const*>& EdgeNames,
                   vector<node*>&       EdgeNodes,
                   vector<int>&         EdgeIndices)
{
    {for (size_t Idx = 0; Idx < EdgeNames.size(); ++Idx)
    {
        char* AsChar = (char*)EdgeNames[Idx];
        GStateDeallocate(AsChar);
        EdgeNames[Idx] = 0;
    }}
    EdgeNames.clear();
    EdgeNodes.clear();
    EdgeIndices.clear();
}


static int
RemoveScalarCompare(lua_State* L)
{
    PushUndoPos("Remove comparison");

    int32x CompIdx = (int32x)lua_tointeger(L, lua_upvalueindex(1));
    int32x NodeID  = (int32x)lua_tointeger(L, lua_upvalueindex(2));

    tokenized* Tokenized = edit::IDToTokenized(NodeID);

    scalar_compare* SCompare = GSTATE_SLOW_TYPE_CHECK(Tokenized, scalar_compare);

    Assert(CompIdx >= 0 && CompIdx < SCompare->GetNumCompares());
    return LuaBoolean(L, SCompare->RemoveCompare(CompIdx));
}

static void
EditScalarCompare(scalar_compare*     ScalarCompare,
                  LuaRect const&      notionalArea,
                  int&                CurrY,
                  ControlIDGen const& ParentIDGen)
{
    ControlIDGen IDGen("%s|scalar_compare_%x_%d",
                       ParentIDGen.ParentID,
                       edit::TokenizedToID(ScalarCompare),
                       ScalarCompare->GetNumCompares());

    LuaRect boxDim = SmallCheckboxDim("All must be true");
    boxDim.x = notionalArea.x;
    boxDim.y = CurrY;
    bool And = ScalarCompare->IsAndCondition();
    if (SmallCheckboxOn(And, "All must be true", boxDim, IDGen))
    {
        PushUndoPos("Set compare operation");
        ScalarCompare->SetIsAndCondition(And);
    }
    AdvanceYRect(CurrY, boxDim);

    // Gather the valid scalar edges of the owner.  Note that we have to delete the
    // strings after this is complete.
    {
        vector<char const*> EdgeNames;
        vector<node*>       EdgeNodes;
        vector<int>         EdgeIndices;
        GatherValidEdges(ScalarCompare->GetOwner(), true, false, EdgeNames, EdgeNodes, EdgeIndices, ScalarEdge);

        // Edit each of the compares
        {for (int Idx = 0; Idx < ScalarCompare->GetNumCompares(); ++Idx)
        {
            node*             OutputNode = 0;
            granny_int32x     OutputIdx = -1;
            scalar_compare_op Op = ScalarCompare_Less;
            granny_real32     Ref = 0.0f;
            bool Changed = false;

            bool Success = ScalarCompare->GetCompareSpec(Idx, &OutputNode, &OutputIdx, &Op, &Ref);
            Assert(Success);

            // Look for the output index in the valid edges
            int32x CurrentEntry = -1;
            {for (int EntryIdx = 0; EntryIdx < SizeI(EdgeIndices); ++EntryIdx)
            {
                if (OutputNode == EdgeNodes[EntryIdx] && EdgeIndices[EntryIdx] == OutputIdx)
                {
                    CurrentEntry = EntryIdx;
                    break;
                }
            }}


            LuaRect cb(notionalArea.x, CurrY, 200, 26);
            {
                Changed = Changed || ComboboxOn(EdgeNames, CurrentEntry, cb, IDGen);
                if (CurrentEntry >= 0)
                {
                    OutputNode = EdgeNodes[CurrentEntry];
                    OutputIdx = EdgeIndices[CurrentEntry];
                }
                else
                {
                    OutputNode = 0;
                    OutputIdx  = -1;
                }
            }

            LuaRect op(cb.x, CurrY, 60, 26);
            AdvanceXRect(op.x, cb);
            {
                int32x CurrentOp = Op;
                Changed = Changed || ComboboxOn(s_ScalarOpStrings, CurrentOp, op, IDGen);
                Op = scalar_compare_op(CurrentOp);
            }

            LuaRect refRect(op.x, CurrY, 60, 26);
            AdvanceXRect(refRect.x, op);
            {
                char buffer[32];
                sprintf(buffer, "%.2f", Ref);
                std::string RefStr = buffer;
                if (EditLabelOn(RefStr, refRect, IDGen))
                {
                    Changed = true;
                    sscanf(RefStr.c_str(), "%f", &Ref);
                }
            }

            LuaPoint removePt(refRect.x, CurrY);
            AdvanceXRect(removePt.x, refRect);

            PushTokenizedIntClosure(ScalarCompare, RemoveScalarCompare, Idx);
            SmallButtonAtWithCallback("remove", removePt, true, IDGen);

            if (Changed)
            {
                PushUndoPos("Set compare spec");
                ScalarCompare->SetCompareSpec(Idx, OutputNode, OutputIdx, Op, Ref);
            }

            // C/R
            AdvanceYRect(CurrY, cb);
        }}

        // Nuke the strings...
        ReleaseEdgeVectors(EdgeNames, EdgeNodes, EdgeIndices);
    }

    // Add a new compare...
    if (SmallButtonAt("Add a comparison", LuaPoint(notionalArea.x, CurrY), true, IDGen))
    {
        PushUndoPos("Add Comparison");

        ScalarCompare->AddCompare();
    }
    AdvanceYLastControl(CurrY);
}


static void
EditEventTriggered(event_triggered*    EventTriggered,
                   LuaRect const&      notionalArea,
                   int                 ControlX,
                   int&                CurrY,
                   ControlIDGen const& ParentIDGen)
{
    ControlIDGen IDGen("%s|event_triggered_%x_%d",
                       ParentIDGen.ParentID,
                       edit::TokenizedToID(EventTriggered));

    // Gather the valid scalar edges of the owner.  Note that we have to delete the
    // strings after this is complete.
    {
        vector<char const*> EdgeNames;
        vector<node*>       EdgeNodes;
        vector<int>         EdgeIndices;
        GatherValidEdges(EventTriggered->GetOwner(), false, true, EdgeNames, EdgeNodes, EdgeIndices, EventEdge);

        bool Changed = false;

        node*       OutputNode  = 0;
        int32x      OutputIndex = -1;
        char const* EventString = 0;
        EventTriggered->GetTriggerEvent(&OutputNode, &OutputIndex, &EventString);

        node*       NewOutputNode  = OutputNode;
        int32x      NewOutputIndex = OutputIndex;
        char const* NewEventString = EventString;
        
        
        int32x CurrentEntry = -1;
        {for (int Idx = 0; Idx < SizeI(EdgeIndices); ++Idx)
        {
            if (EdgeNodes[Idx] == OutputNode && EdgeIndices[Idx] == OutputIndex)
            {
                CurrentEntry = Idx;
                break;
            }
        }}

        if (ComboboxOn(EdgeNames, CurrentEntry,
                       FillRectToRight(ControlX, notionalArea.w, CurrY, 26), IDGen))
        {
            Changed        = true;
            NewOutputNode  = EdgeNodes[CurrentEntry];
            NewOutputIndex = EdgeIndices[CurrentEntry];
            NewEventString = 0;
        }
        AdvanceYLastControl(CurrY);

        // Gather the event strings if we have an output
        granny_text_track_entry Buffer[128];
        int32x TotalEvents;

        LuaRect EventRect = FillRectToRight(ControlX, notionalArea.w, CurrY, 26);
        if (OutputIndex != -1 &&
            OutputNode->GetAllEvents(OutputIndex, Buffer, 128, &TotalEvents))
        {
            CurrentEntry = -1;
            
            vector<char const*> EventNames;
            {for (int Idx = 0; Idx < TotalEvents; ++Idx)
            {
                EventNames.push_back(Buffer[Idx].Text);
                if (EventString && strcmp(EventNames.back(), EventString) == 0)
                {
                    CurrentEntry = Idx;
                }
            }}

            if (ComboboxOn(EventNames, CurrentEntry, EventRect, IDGen))
            {
                Changed = true;
                NewEventString = EventNames[CurrentEntry];;
            }
        }
        else
        {
            // Placeholder
            ComboboxPlaceholder(0, EventRect, IDGen);
        }
        AdvanceYRect(CurrY, EventRect);

        if (Changed)
        {
            PushUndoPos("change event trigger");
            EventTriggered->SetTriggerEvent(NewOutputNode, NewOutputIndex, NewEventString);
        }

        // Nuke the strings...
        ReleaseEdgeVectors(EdgeNames, EdgeNodes, EdgeIndices);
    }
}


static bool
EditConditional(lua_State*     L,
                ControlIDGen const& ParentGen,
                LuaRect const& drawArea,
                conditional*   Conditional,
                int32x         ConditionalIdx,  // for replacement...
                int&           CurrY)
{
    InitTypeStrings();

    ControlIDGen IDGen("%s_%x_%d",
                       ParentGen.ParentID,
                       edit::TokenizedToID(Conditional),
                       ConditionalIdx);

    // Restrict ourselves a bit to set off the conditional.  We're going to draw a rect
    // around the conditional to delimit it.
    int StartY = CurrY;
    AdvanceY(CurrY);

    LuaRect notionalArea = drawArea;
    notionalArea.x += 25 + 2*HPadding();
    notionalArea.w -= 4*HPadding() + 25;

    // Draw the indicator for conditional status

    // @@todo: put into the character preview window...
    // extern real32 g_GlobalDelta;
    // extern real32 g_GlobalClock;
    // DrawRect(LuaRect(drawArea.x + HPadding + 5, CurrY + 5, 15, 15),
    //          (Conditional->IsTrue(edit::GetCharacterOrGlobalTime()) ?
    //           LuaColor(0, 1, 0) :
    //           LuaColor(1, 0, 0)));

    // Draw the combobox for the type selector
    int32x CurrentType = Conditional->GetType();
    Assert(CurrentType >= 0 && CurrentType < SizeI(s_ConditionalTypeStrings));

    char const* TypeLabel = "Condition type:";
    char const* NameLabel = "Condition name:";

    int32x LabelX, ControlX;
    LabeledControlsInArea(LabelWidth(NameLabel), notionalArea, &LabelX, &ControlX);

    LabelAt(TypeLabel, LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);
    {
        if (ComboboxOn(s_ConditionalTypeStrings, CurrentType,
                       FillRectToRight(ControlX, notionalArea.w, CurrY, 26), IDGen))
        {
            if (CurrentType != Conditional->GetType())
            {
                // We will detect this at the end of the function, see below.  This prevents
                // races with the edit name below...
            }
        }
        AdvanceYLastControl(CurrY);
    }

    // Editable name for the conditional
    LabelAt(NameLabel, LuaPoint(LabelX, EditLabelAdjust(CurrY)), eRight);
    {
        string CurrentName = Conditional->GetName();
        if (EditLabelOn(CurrentName, FillRectToRight(ControlX, notionalArea.w, CurrY, 26), IDGen))
        {
            PushUndoPos("Set Conditional name");
            Conditional->SetName(CurrentName.c_str());
        }
        AdvanceYLastControl(CurrY);
    }

    SeparatorAt(CurrY, notionalArea);
    AdvanceYLastControl(CurrY);
    // -------------------------------------------

    if (Conditional->GetType() == Conditional_ScalarCompare)
    {
        scalar_compare* ScalarCompare = GSTATE_SLOW_TYPE_CHECK(Conditional, scalar_compare);
        Assert(ScalarCompare); // deeply wrong if hits

        EditScalarCompare(ScalarCompare, notionalArea, CurrY, IDGen);
    }
    else
    {
        event_triggered* EventTrig = GSTATE_SLOW_TYPE_CHECK(Conditional, event_triggered);
        Assert(EventTrig); // prob a new conditional type, add it...

        EditEventTriggered(EventTrig, notionalArea, ControlX, CurrY, IDGen);
    }

    // Remove the conditional entirely...
    bool DeleteThisNode = false;
    {
        char const* DeleteLabel = "Delete this condition";
        int Width = SmallButtonWidth(DeleteLabel);
        
        if (SmallButtonAt(DeleteLabel,
                          LuaPoint(notionalArea.x + notionalArea.w - Width, CurrY),
                          true, IDGen))
        {
            DeleteThisNode = true;
        }
        AdvanceYLastControl(CurrY);
    }

    // Draw the bounding rect
    DrawRectOutline(LuaRect(HPadding(), StartY, drawArea.w - 2*HPadding(), CurrY - StartY), LuaColor(1, 1, 1));

    if (CurrentType != Conditional->GetType())
    {
        // Need to replace this conditional in its owner
        state_machine* Machine = Conditional->GetOwner();

        conditional* NewCondition = 0;
        switch (CurrentType)
        {
            case Conditional_ScalarCompare:
            {
                scalar_compare* NewScalar = scalar_compare::DefaultInstance();
                NewScalar->SetName(Conditional->GetName());
                NewScalar->AddCompare(); // always have one out of the box...
                NewCondition = NewScalar;
            } break;
            case Conditional_EventTriggered:
            {
                NewCondition = event_triggered::DefaultInstance();
                NewCondition->SetName(Conditional->GetName());
            } break;

            default:
                InvalidCodePath("Unknown conditional type");
                return false;
        }

        PushUndoPos("Replace conditional");

        Machine->ReplaceConditional(ConditionalIdx, NewCondition);
    }

    return DeleteThisNode;
}

static int
RemoveNodeOutput(lua_State* L)
{
    PushUndoPos("Remove node output");

    int32x OutputIdx = (int32x)lua_tointeger(L, lua_upvalueindex(1));
    int32x NodeID    = (int32x)lua_tointeger(L, lua_upvalueindex(2));

    node* Node = edit::IDToNode(NodeID);
    Assert(OutputIdx >= 0 && OutputIdx < Node->GetNumOutputs());

    XInput_NoteOutputDelete(Node, OutputIdx);
    return LuaBoolean(L, Node->DeleteOutput(OutputIdx));
}

static int
RemoveNodeInput(lua_State* L)
{
    PushUndoPos("Remove node output");

    int32x InputIdx = (int32x)lua_tointeger(L, lua_upvalueindex(1));
    int32x NodeID    = (int32x)lua_tointeger(L, lua_upvalueindex(2));

    node* Node = edit::IDToNode(NodeID);
    Assert(InputIdx >= 0 && InputIdx < Node->GetNumInputs());

    return LuaBoolean(L, Node->DeleteInput(InputIdx));
}

int GRANNY
EditStateMachineNode(lua_State* L, state_machine* Machine, int StartY)
{
    node_drawing::InitNodeMetrics(); // for the edge colors

    LuaRect drawArea = UIAreaGet();

    ControlIDGen IDGen("statemachine_edit_box_%x_%d_%d_%d",
                       edit::TokenizedToID(Machine),
                       Machine->GetNumInputs(),
                       Machine->GetNumOutputs(),
                       Machine->GetNumConditionals());

    bool AllowedToEditParams = Machine->GetParent() == NULL;

    // General interface layout:
    //   (if parent is blend node)
    //     [existing scalar and event inputs]
    //     [add new scalar input]
    //     [add new event input]
    //   Published Parameters
    //     [existing scalar and event publishes]
    //     [add new scalar publish]
    //     [add new event publish]
    //   Conditionals
    //     [existing conditionals]
    //     [add new conditional]
    //   Published Parameters
    // -----------------------------------------------------
    int CurrY = StartY + VPadding();

    if (GSTATE_DYNCAST(Machine->GetParent(), blend_graph))
    {
        // Also allowed to edit if the parent is a blend graph...
        AllowedToEditParams = true;

        int32x const centerX = CenterX(drawArea);
        int32x const offsetX = 30;

        // List the input and outputs...
        SectionHeaderAt("Remove inputs and outputs", CurrY, drawArea);
        AdvanceYLastControl(CurrY);
        {
            int EdgesStartY = CurrY;

            {for (int32x Idx = 0; Idx < Machine->GetNumInputs(); ++Idx)
            {
                if (Machine->IsInputInternal(Idx))
                    continue;

                if (AllowedToEditParams)
                {
                    LuaRect EditRect = FillRectToLeft(centerX, CurrY, 21);
                    string EdgeName = Machine->GetInputName(Idx);
                    if (EditLabelOn(EdgeName, EditRect, IDGen))
                    {
                        PushUndoPos("Change SM output name");
                        Machine->SetInputName(Idx, EdgeName.c_str());
                        GetRootMachine()->CheckConnections();
                    }
                    LastControlTip("Renames the output");
                    AdvanceYLastControl(CurrY);

                    PushTokenizedIntClosure(Machine, RemoveNodeInput, Idx);
                    SmallButtonAtWithCallback("Remove",
                                              LuaPoint(EditRect.x, CurrY),
                                              true, IDGen);
                    LastControlTip("Deletes this input");
                    AdvanceYLastControl(CurrY);
                }
                else
                {
                    int Width = SmallButtonWidth(Machine->GetInputName(Idx));

                    PushTokenizedIntClosure(Machine, RemoveNodeInput, Idx);
                    SmallButtonAtWithCallback(Machine->GetInputName(Idx),
                                              LuaPoint(centerX - Width - offsetX, CurrY),
                                              true, IDGen);

                    LastControlTip("Deletes this input");
                    AdvanceYLastControl(CurrY);
                }
            }}
            int EndY = CurrY;
            CurrY = EdgesStartY;

            // Never allow the 0 output to be removed...
            {for (int32x Idx = 1; Idx < Machine->GetNumOutputs(); ++Idx)
            {
                if (Machine->IsOutputInternal(Idx))
                    continue;

                if (AllowedToEditParams)
                {
                    LuaRect EditRect = FillRectToRight(centerX + offsetX, drawArea.w, CurrY, 21);
                    string EdgeName = Machine->GetOutputName(Idx);
                    if (EditLabelOn(EdgeName, EditRect, IDGen))
                    {
                        PushUndoPos("Change SM output name");
                        Machine->SetOutputName(Idx, EdgeName.c_str());
                        GetRootMachine()->CheckConnections();
                    }
                    LastControlTip("Renames the output");
                    AdvanceYLastControl(CurrY);

                    PushTokenizedIntClosure(Machine, RemoveNodeOutput, Idx);
                    SmallButtonAtWithCallback("Remove",
                                              LuaPoint(centerX + offsetX, CurrY),
                                              true, IDGen);
                    LastControlTip("Deletes this output");
                    AdvanceYLastControl(CurrY);
                }
                else
                {
                    PushTokenizedIntClosure(Machine, RemoveNodeOutput, Idx);
                    SmallButtonAtWithCallback(Machine->GetOutputName(Idx),
                                              LuaPoint(centerX + offsetX, CurrY),
                                              true, IDGen);
                    LastControlTip("Deletes this output");
                    AdvanceYLastControl(CurrY);
                }
            }}

            CurrY = max(CurrY, EndY);
            AdvanceY(CurrY);
        }

        SectionHeaderAt("Add inputs and outputs", CurrY, drawArea);
        AdvanceYLastControl(CurrY);

        // Input side
        int const StartY = CurrY;

        node_edge_type EdgeTypes[] = { ScalarEdge, EventEdge, MorphEdge };
        char const* EdgeNames[] = { "Scalar", "Event", "Morph" };
        int const TypeCount = ArrayLength(EdgeTypes);

        {for (int Idx = 0; Idx < TypeCount; ++Idx)
        {
            int Width = SmallButtonWidth(EdgeNames[Idx]);

            if (SmallButtonAt(EdgeNames[Idx], LuaPoint(CenterX(drawArea) - Width - 30, CurrY), true, IDGen))
            {
                PushUndoPos("Add Input");
                Machine->AddInput(EdgeTypes[Idx], EdgeNames[Idx]);
            }
            AdvanceYLastControl(CurrY);
        }}

        // Reset and move to the second column
        CurrY = StartY;
        {for (int Idx = 0; Idx < TypeCount; ++Idx)
        {
            if (SmallButtonAt(EdgeNames[Idx], LuaPoint(CenterX(drawArea) + 30, CurrY), true, IDGen))
            {
                PushUndoPos("Add Output");
                Machine->AddOutput(EdgeTypes[Idx], EdgeNames[Idx]);
            }
            AdvanceYLastControl(CurrY);
        }}
    }
    else
    {
        SectionHeaderAt("Published parameters", CurrY, drawArea);
        AdvanceYLastControl(CurrY);

        //     [existing scalar and event publishes]
        {
            int32x LabelX, ControlX;
            LabeledControlsAtWidth(max(LabelWidth("Scalar"), LabelWidth("Morph")),
                                   &LabelX, &ControlX);

            int PoseOutputIdx = Machine->GetNthExternalOutput(0);
            Assert(Machine->GetOutputType(PoseOutputIdx) == PoseEdge);

            {for (int Idx = 0; Idx < Machine->GetNumOutputs(); ++Idx)
            {
                // Skip internal outputs.  (There aren't any now, but there might be in the
                // future...)
                if (Machine->IsOutputInternal(Idx))
                    continue;

                // Skip the pose output...
                if (Idx == PoseOutputIdx)
                    continue;

                if (Machine->GetOutputType(Idx) == ScalarEdge)
                {
                    ColorLabelAt("Scalar:", LuaPoint(LabelX, EditLabelAdjust(CurrY)), node_drawing::s_EdgeColors[0][ScalarEdge], eRight);
                }
                else if (Machine->GetOutputType(Idx) == MorphEdge)
                {
                    ColorLabelAt("Morph:", LuaPoint(LabelX, EditLabelAdjust(CurrY)), node_drawing::s_EdgeColors[0][MorphEdge], eRight);
                }
                else if (Machine->GetOutputType(Idx) == EventEdge)
                {
                    ColorLabelAt("Event:", LuaPoint(LabelX, EditLabelAdjust(CurrY)), node_drawing::s_EdgeColors[0][EventEdge], eRight);
                }
                else
                {
                    // Don't know how to deal with these yet...
                    NotImplemented;
                }

                LuaRect EditRect = FillRectToRight(ControlX, drawArea.w, CurrY, 26);
                int DelWidth = SmallButtonWidth("Del");

                EditRect.w -= (DelWidth + 2*HPadding());

                string EdgeName = Machine->GetOutputName(Idx);
                if (AllowedToEditParams)
                {
                    if (EditLabelOn(EdgeName, EditRect, IDGen))
                    {
                        PushUndoPos("Change SM output name");
                        Machine->SetOutputName(Idx, EdgeName.c_str());
                        GetRootMachine()->CheckConnections();
                    }
                }
                else
                {
                    EditLabelPlaceholder(EdgeName, EditRect, IDGen);
                    RegisterToolTip("Not editable in this context", EditRect);
                }

                LuaPoint delPt(EditRect.x, CurrY);
                AdvanceXRect(delPt.x, EditRect);

                PushTokenizedIntClosure(Machine, RemoveNodeOutput, Idx);
                SmallButtonAtWithCallback("Del", delPt, true, IDGen);
                AdvanceYRect(CurrY, EditRect);
            }}
        }

        // a little extra space...
        AdvanceY(CurrY);

        //     [add new scalar publish]
        //     [add new event publish]
        //     [add new morph publish]
        {

            char const* AddScalarLabel = "Add Scalar Output";
            char const* AddEventLabel  = "Add Event  Output";
            char const* AddMorphLabel  = "Add Morph  Output";
            int const AddScalarWidth = SmallButtonWidth(AddScalarLabel);
            int const totalWidth = AddScalarWidth*3 + HPadding()*2;
        
            int CurrX = (drawArea.w - totalWidth) / 2;
            if (SmallButtonAt(AddScalarLabel, LuaPoint(CurrX, CurrY), true, IDGen))
            {
                PushUndoPos("New published scalar");
                Machine->AddOutput(ScalarEdge, "Scalar");
            }
            CurrX += AddScalarWidth + HPadding();

            if (SmallButtonAt(AddEventLabel, LuaPoint(CurrX, CurrY), true, IDGen))
            {
                PushUndoPos("New published event");
                Machine->AddOutput(EventEdge, "Event");
            }
            CurrX += AddScalarWidth + HPadding();

            if (SmallButtonAt(AddMorphLabel, LuaPoint(CurrX, CurrY), true, IDGen))
            {
                PushUndoPos("New published morph");
                Machine->AddOutput(MorphEdge, "Morph");
            }

            AdvanceYLastControl(CurrY);
        }
    }

    //   Conditionals
    AdvanceY(CurrY);
    SectionHeaderAt("Conditionals", CurrY, drawArea);
    AdvanceYLastControl(CurrY);
    {
        int32x DeleteConditional = -1;

        //     [existing conditionals]
        {for (int Idx = 0; Idx < Machine->GetNumConditionals(); ++Idx)
        {
            conditional* Conditional = Machine->GetConditional(Idx);
            GStateAssert(Conditional);

            // Updates CurrY, returns "DeleteMe"
            if (EditConditional(L, IDGen, drawArea, Conditional, Idx, CurrY))
            {
                DeleteConditional = Idx;
            }

            // Put the spacing in...
            AdvanceY(CurrY);
        }}

        //  [add new conditional]
        char const* ACLabel = "Add Conditional";
        int32x ACWidthD2 = SmallButtonWidth(ACLabel) / 2;
        if (SmallButtonAt(ACLabel, LuaPoint(CenterX(drawArea) - ACWidthD2, CurrY),  true, IDGen))
        {
            PushUndoPos("New conditional");

            scalar_compare* NewCondition = scalar_compare::DefaultInstance();
            NewCondition->SetName("Scalar");
            NewCondition->AddCompare();
            Machine->AddConditional(NewCondition);
        }
        AdvanceYLastControl(CurrY);

        if (DeleteConditional != -1)
        {
            Machine->DeleteConditional(DeleteConditional);
        }
    }

    return CurrY;
}


