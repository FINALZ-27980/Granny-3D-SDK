// ========================================================================
// $File: //jeffr/granny_29/statement/ui_transition_edit.cpp $
// $DateTime: 2012/09/27 11:38:42 $
// $Change: 39585 $
// $Revision: #7 $
//
// $Notice: $
// ========================================================================
#include "ui_transition_edit.h"
#include "statement_undostack.h"
#include "statement_editstate.h"

#include "granny_math.h"
#include "gstate_character_instance.h"
#include "gstate_conditional.h"
#include "gstate_node_header_list.h"
#include "gstate_node_visitor.h"

#include "gstate_transition.h"
#include "gstate_transition_onloop.h"
#include "gstate_transition_onsubloop.h"

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

static bool
SelectSharedEvent(synchronize_spec* Spec,
                  node* RefNode,
                  node* SyncedNode,
                  LuaRect const& EventRect,
                  ControlIDGen& IDGen)
{
    if (RefNode    == 0 ||
        SyncedNode == 0 ||
        RefNode->GetBoundCharacter() == 0 ||
        Spec->ReferenceEventEdge == -1 ||
        Spec->SyncedEventEdge == -1)
    {
        ComboboxPlaceholder(Spec->EventName, EventRect, IDGen);
        return false;
    }

    // Get the shared events for each edge...
    // O(mn) this way, of course...
    const int LargeNum = 64;
    granny_text_track_entry RefBuffer[LargeNum];
    granny_text_track_entry SyncBuffer[LargeNum];  // This is really large, just to try to
                                                   // make sure that in normal
                                                   // circumstances it won't be exceeded

    int32x NumRefEvents;
    if (!RefNode->GetAllEvents(Spec->ReferenceEventEdge, RefBuffer, LargeNum, &NumRefEvents))
    {
        NumRefEvents = 0;
    }

    int32x NumSyncEvents = 0;
    if (NumRefEvents != 0)
        SyncedNode->GetAllEvents(Spec->SyncedEventEdge, SyncBuffer, LargeNum, &NumSyncEvents);

    int32x CurrentSelection = -1;
    vector<char const*> ValidEvents;
    {for (int Idx = 0; Idx < NumSyncEvents; ++Idx)
    {
        char const* SyncText = SyncBuffer[Idx].Text;
        {for (int RefIdx = 0; RefIdx < NumRefEvents; ++RefIdx)
        {
            char const* RefText = RefBuffer[RefIdx].Text;
            if (_stricmp(RefText, SyncText) == 0)
            {
                if (Spec->EventName && _stricmp(Spec->EventName, SyncText) == 0)
                    CurrentSelection = ValidEvents.size();

                ValidEvents.push_back(SyncBuffer[Idx].Text);
                break;
            }
        }}
    }}

    bool const Changed = ITunesComboboxOn(ValidEvents, CurrentSelection, EventRect, IDGen);
    if (CurrentSelection != -1)
        Spec->EventName = (char*)ValidEvents[CurrentSelection];
    else
        Spec->EventName = 0;

    return Changed;
}

static bool
SelectSyncNode(node* BaseNode, node*& Current,
               LuaRect const& ControlRect,
               ControlIDGen& IDGen)
{
    container* AsContainer = GSTATE_DYNCAST(BaseNode, container);

    // First, let's collect the valid entries.  This is the base node, plus any of its
    // children.
    vector<char const*> NodeNames;
    vector<node*> Nodes;

    // One special case name.  The base node needs a marker so it's clear that it owns
    // what follows
    string TopLevelName = "() ";
    TopLevelName += BaseNode->GetName();

    int32x CurrentSelection = -1;

    NodeNames.push_back(TopLevelName.c_str());
    Nodes.push_back(BaseNode);
    if (Current == BaseNode)
        CurrentSelection = 0;

    if (AsContainer)
    {
        {for (int Idx = 0; Idx < AsContainer->GetNumChildren(); ++Idx)
        {
            node* Child = AsContainer->GetChild(Idx);

            NodeNames.push_back(Child->GetName());
            Nodes.push_back(Child);

            if (Child == Current)
                CurrentSelection = Idx+1;
        }}
    }

    if (ITunesComboboxOn(NodeNames, CurrentSelection, ControlRect, IDGen))
    {
        if (CurrentSelection != -1)
            Current = Nodes[CurrentSelection];
        else
            Current = 0;

        return true;
    }

    return false;
}

static bool
SelectEventEdge(node*          Node,
                int16*         CurrentEdge,
                LuaRect const& ControlRect,
                ControlIDGen&  IDGen)
{
    vector<char const*> EdgeNames;
    vector<int32x>      EdgeIndices;

    int32x CurrentSelection = -1;
    if (Node)
    {
        {for (int Idx = 0; Idx < Node->GetNumOutputs(); ++Idx)
        {
            if (Node->IsOutputInternal(Idx))
                continue;

            if (Node->GetOutputType(Idx) != EventEdge)
                continue;

            if (*CurrentEdge == Idx)
                CurrentSelection = EdgeNames.size();

            EdgeNames.push_back(Node->GetOutputName(Idx));
            EdgeIndices.push_back(Idx);
        }}
    }

    bool Changed = ITunesComboboxOn(EdgeNames, CurrentSelection, ControlRect, IDGen);

    if (CurrentSelection != -1)
        *CurrentEdge = (int16)EdgeIndices[CurrentSelection];
    else
        *CurrentEdge = -1;

    return Changed;
}



// =============================================================================
// This one is singular for now, so we can just directly edit it...
int GRANNY
Edit_TransitionEdit(lua_State* LuaState)
{
    LuaRect drawArea = UIAreaGet();

    transition* Transition = edit::IDToTransition(lua_tointeger(LuaState, -2));
    ControlIDGen IDGen("transition_edit_box_%x_%d_%d", edit::TokenizedToID(Transition),
                       Transition->GetNumConditionals(),
                       Transition->GetNumSyncs());

    int32x LabelX, ControlX;
    LabeledControlsAtWidth(LabelWidth("Probability:"), &LabelX, &ControlX);

    int32x CurrY = lua_tointeger(LuaState, -1);
    AdvanceY(CurrY);

    // What is the type of this transition?
    {
        LabelAt("Type:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

        node* StartNode = Transition->GetStartNode();
        node* EndNode   = Transition->GetEndNode();
        int CurrentEntry = -1;

        vector<char const*>     Options;
        vector<transition_type> Types;
        for (int i = 0; i < NumTransitionTypes; ++i)
        {
            if (TransitionTypeValidForNodes((transition_type)i, StartNode, EndNode))
            {
                if (i == Transition->GetTransitionType())
                    CurrentEntry = Options.size();

                Options.push_back(NameForTransitionType(transition_type(i)));
                Types.push_back(transition_type(i));
            }
        }
        Assert(CurrentEntry != -1);

        LuaRect ControlRect(ControlX, CurrY, 200, 26);
        if (ComboboxOn(Options, CurrentEntry, ControlRect, IDGen))
        {
            PushUndoPos("Transition type");

            transition* NewTransition = TransferTransitionToType(Types[CurrentEntry], Transition);
            if (NewTransition)
            {
                // This probably always hits, of course...
                if (edit::IsSelected(Transition))
                {
                    edit::RemoveFromSelection(Transition);
                    edit::AddToSelection(NewTransition);
                }

                // If we're editing the old transition, we should be editing the new one...
                {
                    PushTableFunction(LuaState, "_G", "CopyTransitionEditState");
                    lua_pushinteger(LuaState, edit::TokenizedToID(Transition));
                    lua_pushinteger(LuaState, edit::TokenizedToID(NewTransition));
                    lua_pcall(LuaState, 2, 0, 0);
                }

                StartNode->DeleteTransition(Transition); // does the delete for us...
                StartNode->AddTransition(NewTransition);
                Transition = NewTransition;
            }
        }
        AdvanceYRect(CurrY, ControlRect);
        AdvanceY(CurrY);
    }
    AdvanceY(CurrY);

    // Duration
    {
        LabelAt("Duration:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

        real32 Duration = Transition->GetDuration();
        LuaRect ControlRect(ControlX, CurrY, 200, 26);
        if (SliderOn(0, 2, Duration, false, ControlRect, IDGen))
        {
            PushUndoPos("Transition duration", false, eSliderMotion, Transition->GetUID());

            Transition->SetDuration(Duration);
        }
        AdvanceYRect(CurrY, ControlRect);

        char buffer[32];
        sprintf(buffer, "%.2f", Duration);
        std::string DurationStr = buffer;
        ControlRect = LuaRect(ControlX, CurrY, 75, 24);
        if (EditLabelOn(DurationStr, ControlRect, IDGen))
        {
            PushUndoPos("Transition duration");

            float NewDuration = 1.0f;
            sscanf(DurationStr.c_str(), "%f", &NewDuration);
            Transition->SetDuration(NewDuration);
        }
        AdvanceYRect(CurrY, ControlRect);
    }

    // Probability
    if (Transition->GetTransitionType() == Transition_OnLoop)
    {
        tr_onloop* OnLoop = GSTATE_SLOW_TYPE_CHECK(Transition, tr_onloop);

        LabelAt("Probability:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

        real32 Probability = OnLoop->GetProbability();
        LuaRect ControlRect(ControlX, CurrY, 200, 26);
        if (SliderOn(0, 1, Probability, false, ControlRect, IDGen))
        {
            PushUndoPos("Transition probability", false, eSliderMotion, Transition->GetUID());

            OnLoop->SetProbability(Probability);
        }
        AdvanceYRect(CurrY, ControlRect);

        char buffer[32];
        sprintf(buffer, "%.2f", Probability);
        std::string ProbabilityStr = buffer;
        ControlRect = LuaRect(ControlX, CurrY, 75, 24);
        if (EditLabelOn(ProbabilityStr, ControlRect, IDGen))
        {
            PushUndoPos("Transition probability");

            float NewProbability = 1.0f;
            sscanf(ProbabilityStr.c_str(), "%f", &NewProbability);
            OnLoop->SetProbability(NewProbability);
        }
        AdvanceYRect(CurrY, ControlRect);
    }
    else if (Transition->GetTransitionType() == Transition_OnSubLoop)
    {
        tr_onsubloop* SubLoop        = static_cast<tr_onsubloop*>(Transition);
        container*    StartContainer = GSTATE_DYNCAST(Transition->GetStartNode(), container);
        Assert(StartContainer);

        // Get the subnode
        {
            LabelAt("Subnode:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

            int CurrentEntry = -1;

            vector<char const*> Names;
            for (int i = 0; i < StartContainer->GetNumChildren(); ++i)
            {
                node* Child = StartContainer->GetChild(i);
                if (Child == SubLoop->GetSubnode())
                    CurrentEntry = i;

                Names.push_back(Child->GetName());
            }

            // CurrentEntry may be -1 here, and that's OK.
            LuaRect ControlRect(ControlX, CurrY, 200, 26);
            if (ComboboxOn(Names, CurrentEntry, ControlRect, IDGen))
            {
                PushUndoPos("Subnode changed");

                if (CurrentEntry != -1)
                {
                    SubLoop->SetSubnode(StartContainer->GetChild(CurrentEntry));
                }
                else
                {
                    SubLoop->SetSubnode(0);
                }
            }
            AdvanceYRect(CurrY, ControlRect);
        }

        // Get the output
        {
            LabelAt("Output:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

            int CurrentEntry = -1;

            vector<char const*> Names;
            vector<int32>       OutputIndex;

            node* Subnode = SubLoop->GetSubnode();
            if (Subnode)
            {
                for (int i = 0; i < Subnode->GetNumOutputs(); ++i)
                {
                    if (Subnode->IsOutputExternal(i) == false)
                        continue;

                    Names.push_back(Subnode->GetOutputName(i));
                    OutputIndex.push_back(i);

                    if (i == SubLoop->GetSubnodeOutput())
                        CurrentEntry = i;
                }
            }

            // CurrentEntry may be -1 here, and that's OK.
            LuaRect ControlRect(ControlX, CurrY, 200, 26);
            if (ComboboxOn(Names, CurrentEntry, ControlRect, IDGen))
            {
                PushUndoPos("Subnode output changed");

                if (CurrentEntry >= 0 && CurrentEntry < int(OutputIndex.size()))
                {
                    SubLoop->SetSubnodeOutput(OutputIndex[CurrentEntry]);
                }
                else
                {
                    SubLoop->SetSubnodeOutput(0);
                }
            }
            AdvanceYRect(CurrY, ControlRect);
        }
    }

    SectionHeaderAt("Conditionals", CurrY, drawArea);
    AdvanceYLastControl(CurrY);
    {
        if (Transition->GetNumConditionals() > 1)
        {
            LuaRect boxDim = SmallCheckboxDim("All must be true");
            AdvanceX(boxDim.x);
            boxDim.y = CurrY;
            bool And = Transition->GetAndConditions();
            if (SmallCheckboxOn(And, "All must be true", boxDim, IDGen))
            {
                PushUndoPos("Set and conditions");
                Transition->SetAndConditions(And);
            }
            AdvanceYRect(CurrY, boxDim);
        }

        // Compute these out here, we'll use them repeatedly below...
        int32x RemoveButtonWidth = SmallButtonWidth("(remove)");
        LuaRect ControlRect = FillRectToRight(ControlX, drawArea.w - RemoveButtonWidth - HPadding());
        ControlRect.h = 26; // @magic
        LuaPoint RemoveButtonPt(ControlRect.x, 0);
        AdvanceX(RemoveButtonPt.x, ControlRect.w);

        int32x DeleteConditionIdx = -1;
        {for (int Idx = 0; Idx < Transition->GetNumConditionals(); ++Idx)
        {
            LabelAt("Condition:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

            // Gather the conditionals from the state machine at the current level.  Look on
            // startNode->Parent, which must be a state_machine.  Make sure to make "none" an
            // option, of course...
            vector<char const*> ConditionNames;
            vector<conditional*> Conditionals;
            ConditionNames.push_back("<none>");
            Conditionals.push_back(0);

            // May be null
            conditional* CurrentCondition = Transition->GetConditional(Idx);

            node* StartNode = Transition->GetStartNode();
            Assert(StartNode);
            node* ParentNode = StartNode->GetParent();
            Assert(ParentNode);
            state_machine* ParentMachine = GSTATE_DYNCAST(ParentNode, state_machine);

            int CurrentEntry = 0; // none by default...
            {for (int Idx = 0; Idx < ParentMachine->GetNumConditionals(); ++Idx)
            {
                conditional* Condition = ParentMachine->GetConditional(Idx);

                ConditionNames.push_back(Condition->GetName());
                Conditionals.push_back(Condition);

                // Account for the <none> entry...
                if (Condition == CurrentCondition)
                    CurrentEntry = Idx+1;
            }}

            ControlRect.y = CurrY;
            if (ComboboxOn(ConditionNames, CurrentEntry, ControlRect, IDGen))
            {
                PushUndoPos("Conditional");
                Transition->SetConditional(Idx, Conditionals[CurrentEntry]);
            }

            RemoveButtonPt.y = CurrY;
            if (SmallButtonAt("(remove)", RemoveButtonPt, true, IDGen))
            {
                DeleteConditionIdx = Idx;
            }
            AdvanceYRect(CurrY, ControlRect);
        }}

        if (SmallButtonAt("Add Conditional", HPadPtAtY(CurrY), true, IDGen))
        {
            PushUndoPos("transition add conditional");
            Transition->AddConditional();
        }
        AdvanceYLastControl(CurrY);

        if (DeleteConditionIdx != -1)
        {
            PushUndoPos("transition del conditional");
            Transition->DeleteConditional(DeleteConditionIdx);
        }
    }

    SectionHeaderAt("Synced Nodes", CurrY, drawArea);
    AdvanceYLastControl(CurrY);
    {
        LuaRect notionalArea = drawArea;
        notionalArea.x += 10;
        notionalArea.w -= 15;

        int DeleteSyncIndex = -1;
        {for (int Idx = 0; Idx < Transition->GetNumSyncs(); ++Idx)
        {
            int StartY = CurrY;
            AdvanceY(CurrY);

            node* RefNode;
            node* SyncedNode;
            synchronize_spec Spec;
            bool SpecChanged = false;
            Transition->GetSyncParameters(Idx, &Spec, &RefNode, &SyncedNode);

            LabelAt("Reference:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

            if (SelectSyncNode(Transition->GetStartNode(), RefNode,
                               FillRectToRight(ControlX, notionalArea.w, CurrY, 26), IDGen))
            {
                RefNode->GetTypeAndToken(&Spec.ReferenceNode);
                SpecChanged = true;
            }
            AdvanceYLastControl(CurrY);

            LabelAt("Synced:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

            if (SelectSyncNode(Transition->GetEndNode(), SyncedNode,
                               FillRectToRight(ControlX, notionalArea.w, CurrY, 26), IDGen))
            {
                SyncedNode->GetTypeAndToken(&Spec.SyncedNode);
                SpecChanged = true;
            }
            AdvanceYLastControl(CurrY);

            const int EdgeLabelX   = LabelX   + 30;
            const int EdgeControlX = ControlX + 30;

            // Right event
            {
                LabelAt("Ref. Edge:", LuaPoint(EdgeLabelX, ComboLabelAdjust(CurrY)), eRight);
                if (SelectEventEdge(RefNode, &Spec.ReferenceEventEdge,
                                    FillRectToRight(EdgeControlX, notionalArea.w, CurrY, 26), IDGen))
                {
                    SpecChanged = true;
                }
                AdvanceYLastControl(CurrY);
            }

            // Left event
            {
                LabelAt("Sync Edge:", LuaPoint(EdgeLabelX, ComboLabelAdjust(CurrY)), eRight);
                if (SelectEventEdge(SyncedNode, &Spec.SyncedEventEdge,
                                    FillRectToRight(EdgeControlX, notionalArea.w, CurrY, 26), IDGen))
                {
                    SpecChanged = true;
                }
                AdvanceYLastControl(CurrY);
            }

            // Event Name
            {
                LabelAt("On Event:", LuaPoint(EdgeLabelX+20, ComboLabelAdjust(CurrY)), eRight);

                if (SelectSharedEvent(&Spec, RefNode, SyncedNode,
                                      FillRectToRight(EdgeControlX+20, notionalArea.w, CurrY, 26), IDGen))
                {
                    SpecChanged = true;
                }
                AdvanceYLastControl(CurrY);
            }

            // Remove the conditional entirely...
            {
                char const* DeleteLabel = "Delete this sync pair";
                int Width = SmallButtonWidth(DeleteLabel);

                if (SmallButtonAt(DeleteLabel,
                                  LuaPoint(notionalArea.x + notionalArea.w - Width, CurrY),
                                  true, IDGen))
                {
                    DeleteSyncIndex = Idx;
                }
                AdvanceYLastControl(CurrY);
            }

            if (SpecChanged)
            {
                PushUndoPos("changing sync edge");

                Transition->SetSyncParameters(Idx, Spec);
            }

            // Outline the sync
            DrawRectOutline(LuaRect(notionalArea.x, StartY, notionalArea.w, CurrY - StartY), LuaColor(1, 1, 1));
            AdvanceY(CurrY, 1);
        }}

        if (SmallButtonAt("Add sync pair", HPadPtAtY(CurrY), true, IDGen))
        {
            PushUndoPos("transition add sync");
            Transition->AddSync();
        }
        AdvanceYLastControl(CurrY);

        if (DeleteSyncIndex != -1)
        {
            PushUndoPos("transition delete sync");
            Transition->DeleteSync(DeleteSyncIndex);
        }
    }

    // @@ fixme
    LuaRect used(0, 0, 1, CurrY);
    PushRect(LuaState, used);
    lua_pushboolean(LuaState, true);

    return 2;
}
