// ========================================================================
// $File: //jeffr/granny_29/statement/ui_blendnode_edit.cpp $
// $DateTime: 2012/11/02 11:18:48 $
// $Change: 40188 $
// $Revision: #19 $
//
// $Notice: $
// ========================================================================
#include "ui_blendnode_edit.h"
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

#include "granny_model.h"
#include "granny_skeleton.h"
#include "granny_animation.h"
#include "granny_track_group.h"

#include "luautils.h"
#include "statement_editstate.h"
#include "ui_area.h"
#include "ui_character_render.h"
#include "ui_controls.h"
#include "ui_drawing.h"
#include "ui_font.h"
#include "ui_nodedrawing.h"
#include "ui_statemachine_edit.h"

#include <vector>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

USING_GRANNY_NAMESPACE;
USING_GSTATE_NAMESPACE;
using namespace std;

BEGIN_GRANNY_NAMESPACE;
namespace edit
{
    extern model* Model;
}
END_GRANNY_NAMESPACE;
using namespace GRANNY edit;

// static int
// AddPoseInputToNode(lua_State* L)
// {
//     PushUndoPos("Add input");

//     int32x NodeID = (int32x)lua_tointeger(L, lua_upvalueindex(1));
//     node* Node = edit::IDToNode(NodeID);

//     Node->AddInput(PoseEdge, "Pose Input");
//     edit::MarkModified();

//     return 0;
// }

static int
AddPoseOutputToNode(lua_State* L)
{
    PushUndoPos("Add output");

    int32x NodeID = (int32x)lua_tointeger(L, lua_upvalueindex(1));
    node* Node = edit::IDToNode(NodeID);

    Node->AddOutput(PoseEdge, "Pose Output");

    return 0;
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
RemoveRenamer(lua_State* L)
{
    PushUndoPos("Remove renamer");

    int32x OutputIdx = (int32x)lua_tointeger(L, lua_upvalueindex(1));
    int32x NodeID    = (int32x)lua_tointeger(L, lua_upvalueindex(2));

    node* Node = edit::IDToNode(NodeID);
    Assert(Node);

    event_renamer* Renamer = GSTATE_SLOW_TYPE_CHECK(Node, event_renamer);
    Assert(Renamer);
    Assert(OutputIdx >= 0 && OutputIdx < Renamer->NumRenames());

    Renamer->DeleteRename(OutputIdx);
    return LuaBoolean(L, true);
}


static int
DeleteSwapPair(lua_State* L)
{
    PushUndoPos("Delete swap pair");

    int32x Idx    = (int32x)lua_tointeger(L, lua_upvalueindex(1));
    int32x NodeID = (int32x)lua_tointeger(L, lua_upvalueindex(2));

    node* Node = edit::IDToNode(NodeID);

    mirror* Mirror = GSTATE_SLOW_TYPE_CHECK(Node, mirror);
    Assert(Mirror);
    Assert(Idx >= 0 && Idx < Mirror->GetNameSwapCount());


    vector<char const*> Swaps(Mirror->GetNameSwapCount());
    {for (int Idx = 0; Idx < Mirror->GetNameSwapCount(); ++Idx)
    {
        Swaps[Idx] = Mirror->GetNameSwap(Idx);
    }}
    Swaps.erase(Swaps.begin() + Idx);
    Swaps.erase(Swaps.begin() + Idx);

    Mirror->SetNameSwaps(Swaps.size(), &Swaps[0]);
    return 0;
}


char const* s_AnimationSelectTip =
    "Selects the animation for this node. Note\n"
    "that the edit portion of this control can\n"
    "be used as a search/filter interface.";

static void
ComputeAnimationSelectRect(int AreaWidth, int ControlX, int CurrY, LuaRect* SelectRect)
{
    int ButtonW = 50;
    int32x const MaxWidth = 350;
    int32x ScreenWidth = AreaWidth - ControlX - ButtonW - 2*HPadding();
    *SelectRect = LuaRect(ControlX, CurrY, min(MaxWidth, ScreenWidth), 26);
}

static animation_slot*
AnimationSelect(lua_State* LuaState,
                vector<animation_slot*>& AnimationSlots,
                animation_slot* ThisSlot,
                LuaRect const& Rect,
                ControlIDGen& IDGen)
{
    int CurrentEntry = -1;

    // Edit button
    {
        bool Enabled = !((ThisSlot == 0) || edit::CanEditSlot(ThisSlot) == false);

        LuaPoint editPt(Rect.x, Rect.y);
        AdvanceXRect(editPt.x, Rect);
        if (SmallButtonAt("edit", editPt, Enabled, IDGen))
        {
            edit::EditSlot(ThisSlot);
        }
        LastControlTip("Attempts to open the animation in your DCC package.");
    }

    vector<char const*> AnimationNicknames(AnimationSlots.size(), 0);
    {for (size_t SlotIdx = 0; SlotIdx < AnimationSlots.size(); ++SlotIdx)
    {
        AnimationNicknames[SlotIdx] = AnimationSlots[SlotIdx]->Name;

        if (AnimationSlots[SlotIdx] == ThisSlot)
        {
            Assert(CurrentEntry == -1);
            CurrentEntry = int(SlotIdx);
        }
    }}

    // Register the tooltip for this control
    RegisterToolTip(s_AnimationSelectTip, Rect);
    if (ITunesComboboxOn(AnimationNicknames, CurrentEntry, Rect, IDGen))
        return AnimationSlots[CurrentEntry];
    else
        return ThisSlot;
}

static int32x
EditAnimSource(lua_State* LuaState,
               bool EditInBlendGraph,
               anim_source* Animation,
               int32x AreaWidth,
               int32x& CurrY,
               vector<animation_slot*>& AnimationSlots)
{
    // Layout is: for each edge,
    //   label
    //    combobox
    //    checkbox
    //    slider
    Assert(Animation->GetOutputType(0) == PoseEdge);

    ControlIDGen IDGen("animation_edit_box_%x_%d",
                       edit::TokenizedToID(Animation), Animation->GetNumOutputs());

    int32x LabelX, ControlX;
    LabeledControlsAtWidth(LabelWidth("OneShot:"), &LabelX, &ControlX);

    bool AnimationChanged = false;

    // Only allow editing the animation edge name in blend graph mode
    if (EditInBlendGraph)
    {
        LabelAt("Edge:", LuaPoint(LabelX, EditLabelAdjust(CurrY)), eRight);

        std::string Name = Animation->GetOutputName(0);
        if (EditLabelOn(Name, LuaRect(ControlX, CurrY, 200, 24), IDGen))
        {
            PushUndoPos("Change name");
            Animation->SetOutputName(0, Name.c_str());
        }
        AdvanceYLastControl(CurrY);
    }

    // Find the entry for the combobox...
    {
        LabelAt("Anim:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

        animation_slot* ThisSlot = Animation->GetAnimationSlot();

        LuaRect SelectRect;
        ComputeAnimationSelectRect(AreaWidth, ControlX, CurrY, &SelectRect);
        animation_slot* NewSlot  = AnimationSelect(LuaState, AnimationSlots,
                                                   ThisSlot, SelectRect, IDGen);
        if (NewSlot != ThisSlot)
        {
            PushUndoPos("Change animation");
            Animation->SetAnimationSlot(NewSlot);

            if (Animation->IsNameUserSpecified() == false)
            {
                // Set the name based on the animation selected...
                Animation->SetName(NewSlot->Name);
            }

            AnimationChanged = true;
        }

        AdvanceYLastControl(CurrY);
    }

    // One shot option
    {
        LabelAt("OneShot:", LuaPoint(LabelX, CurrY-1), eRight);

        LuaRect checkRect = CheckboxDim("");
        checkRect.x = ControlX;
        checkRect.y = CurrY;

        bool Clamped = Animation->GetOutputLoopClamping();
        if (CheckboxOn(Clamped, "", checkRect, IDGen))
        {
            PushUndoPos("Animation option");
            Animation->SetOutputLoopClamping(Clamped);
        }
        AdvanceYLastControl(CurrY);
    }

    // Speed
    {
        LabelAt("Speed:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

        real32 Speed = Animation->GetOutputSpeed();
        if (SliderOn(0, 10, Speed, false, LuaRect(ControlX, CurrY, 200, 26), IDGen))
        {
            PushUndoPos("Animation option", false, eSliderMotion, Animation->GetUID());
            Animation->SetOutputSpeed(Speed);
        }
        AdvanceYLastControl(CurrY);

        char buffer[32];
        sprintf(buffer, "%.2f", Speed);
        std::string SpeedStr = buffer;
        if (EditLabelOn(SpeedStr, LuaRect(ControlX, CurrY, 75, 24), IDGen))
        {
            PushUndoPos("Animation option");

            float NewSpeed = 1.0f;
            sscanf(SpeedStr.c_str(), "%f", &NewSpeed);
            Animation->SetOutputSpeed(NewSpeed);
        }
        AdvanceYLastControl(CurrY);
    }

    return AnimationChanged;
}

static int
SharedParametersEdit(lua_State* LuaState, parameters* Parameters)
{
    Assert(Parameters);

    LuaRect drawArea = UIAreaGet();

    // We want this to create all new items if the number of outputs change...
    ControlIDGen IDGen("parameters_edit_box_%x_%d",
                       edit::TokenizedToID(Parameters),
                       Parameters->GetNumOutputs());

    // @@magic number
    // Compute the control offset...
    int32x LabelX, ControlX;
    LabeledControlsAtWidth(200, &LabelX, &ControlX);

    // Compute the rects up here so we can use it to compute the value rects as well...
    // @@magic number
    LuaRect SliderRect = FillRectToRight(ControlX, drawArea.w, 0, 26);

    // @@magic number
    LuaRect NameRect   = FillRectToLeft(LabelX, 0, 24);

    int MinMaxValWidth = 50;
    int MinStart = ControlX;
    int MaxStart = ControlX + SliderRect.w - MinMaxValWidth;
    int ValStart = (MinStart + MaxStart) / 2;

    int CurrY = VPadding();

    // Integer lock
    {
        LabelAt("Integers only", LuaPoint(LabelX, CheckboxLabelAdjust(CurrY)), eRight);

        bool IsLocked = Parameters->GetClampSliderToInts();

        LuaRect boxRect = CheckboxDim("");
        boxRect.x = ControlX;
        boxRect.y = CurrY;
        if (CheckboxOn(IsLocked, "", boxRect, IDGen))
        {
            PushUndoPos("Parameter Integer Lock");

            Parameters->SetClampSliderToInts(IsLocked);
        }
        AdvanceYLastControl(CurrY);
    }

    {for (int Idx = 0; Idx < Parameters->GetNumOutputs(); ++Idx)
    {
        NameRect.y = CurrY;

        std::string Name = Parameters->GetOutputName(Idx);
        if (EditLabelOn(Name, NameRect, IDGen))
        {
            PushUndoPos("Change param name");
            Parameters->SetOutputName(Idx, Name.c_str());
            GetRootMachine()->CheckConnections();
        }
        LastControlTip("Changes the name of the parameter");

        SliderRect.y = CurrY;

        real32 Param = Parameters->SampleScalarOutput(Idx, 0);
        real32 MinVal, MaxVal;
        Parameters->GetScalarOutputRange(Idx, &MinVal, &MaxVal);
        if (SliderOn(MinVal, MaxVal, Param, Parameters->GetClampSliderToInts(), SliderRect, IDGen))
        {
            PushUndoPos("Change parameter", false, eSliderMotion, Parameters->GetUID());
            Parameters->SetParameter(Idx, Param);
            Parameters->SetParameterDefault(Idx, Param);
        }
        LastControlTip("Sets the value of the parameter. Note that this will\n"
                       "also change the default value stored in the file.");
        AdvanceYLastControl(CurrY);

        // Removal button
        {
            PushTokenizedIntClosure(Parameters, RemoveNodeOutput, Idx);
            SmallButtonAtWithCallback("(remove)", HPadPtAtY(CurrY), true, IDGen);

            LastControlTip("Deletes this edge");
        }

        // Min/Current/Max
        {
            // Min
            {
                char buffer[32];
                sprintf(buffer, "%.2f", MinVal);
                std::string MinStr = buffer;

                if (EditLabelOn(MinStr, LuaRect(MinStart, CurrY, MinMaxValWidth, 24), IDGen))
                {
                    PushUndoPos("Change parameter options");

                    sscanf(MinStr.c_str(), "%f", &MinVal);

                    if (MinVal > MaxVal)
                        MaxVal = MinVal;
                    Parameters->SetScalarOutputRange(Idx, MinVal, MaxVal);

                    // Clamp the value...
                    Param = Clamp(MinVal, Param, MaxVal);
                    Parameters->SetParameter(Idx, Param);
                    Parameters->SetParameterDefault(Idx, Param);
                }
                LastControlTip("Minimum value for this parameter");
            }

            // Current
            {
                char buffer[32];
                sprintf(buffer, "%.2f", Param);
                std::string ParamStr = buffer;

                if (EditLabelOn(ParamStr, LuaRect(ValStart, CurrY, MinMaxValWidth, 24), IDGen))
                {
                    PushUndoPos("Change parameter options");

                    sscanf(ParamStr.c_str(), "%f", &Param);

                    // Clamp the value...
                    Param = Clamp(MinVal, Param, MaxVal);
                    Parameters->SetParameter(Idx, Param);
                    Parameters->SetParameterDefault(Idx, Param);
                }
                LastControlTip("Current value for this parameter");
            }

            // Max
            {
                char buffer[32];
                sprintf(buffer, "%.2f", MaxVal);
                std::string MaxStr = buffer;
                if (EditLabelOn(MaxStr, LuaRect(MaxStart, CurrY, MinMaxValWidth, 24), IDGen))
                {
                    PushUndoPos("Change parameter options");

                    sscanf(MaxStr.c_str(), "%f", &MaxVal);

                    if (MaxVal < MinVal)
                        MinVal = MaxVal;
                    Parameters->SetScalarOutputRange(Idx, MinVal, MaxVal);

                    // Clamp the value...
                    Param = Clamp(MinVal, Param, MaxVal);
                    Parameters->SetParameter(Idx, Param);
                    Parameters->SetParameterDefault(Idx, Param);
                }
                LastControlTip("Maximum value for this parameter");
            }
        }
        AdvanceYLastControl(CurrY);
    }}

    AdvanceY(CurrY);
    if (ButtonAt("Add Parameter", HPadPtAtY(CurrY), true, IDGen))
    {
        PushUndoPos("Add parameter output");

        Parameters->AddOutput(ScalarEdge, "Parameter");
    }
    LastControlTip("Create a new output parameter");
    AdvanceYLastControl(CurrY);

    return CurrY;
}


static int
RemoveEventPoke(lua_State* L)
{
    PushUndoPos("RemoveEvent");

    int32x EventIdx = (int32x)lua_tointeger(L, lua_upvalueindex(1));
    int32x NodeID   = (int32x)lua_tointeger(L, lua_upvalueindex(2));

    node* Node = edit::IDToNode(NodeID);
    event_source* Source = GSTATE_SLOW_TYPE_CHECK(Node, event_source);
    Assert(Source && GS_InRange(EventIdx, Source->NumPossibleEvents()));

    XInput_NotePossibleEventDelete(Source, EventIdx);
    Source->DeletePossibleEvent(EventIdx);

    return LuaBoolean(L, true);
}


static int
SharedEventSourceEdit(event_source* Source)
{
    Assert(Source);
    LuaRect drawArea = UIAreaGet();

    // We want this to create all new items if the number of outputs change...
    ControlIDGen IDGen("event_source_edit_box_%x_%d",
                       edit::TokenizedToID(Source),
                       Source->NumPossibleEvents());

    // Compute the control offset...
    char const* TriggerText = "(trigger)";
    char const* DeleteText = "(del)";
    int ControlX = SmallButtonWidth(TriggerText) + 2 * HPadding();
    int CurrY = VPadding();

    {for (int Idx = 0; Idx < Source->NumPossibleEvents(); ++Idx)
    {
        // Removeable button
        {
            PushTokenizedIntClosure(Source, TriggerEventPoke, Idx);
            SmallButtonAtWithCallback(TriggerText, HPadPtAtY(CurrY), true, IDGen);
        }

        std::string Str = Source->GetPossibleEvent(Idx);
        LuaRect EditRect = FillRectToRight(ControlX, drawArea.w - 50, CurrY, 24);
        if (EditLabelOn(Str, EditRect, IDGen))
        {
            PushUndoPos("EventString change");

            Source->SetPossibleEvent(Idx, Str.c_str());
        }
        int ButtonY = CurrY;
        AdvanceYLastControl(CurrY);

        // Removeable button
        {
            PushTokenizedIntClosure(Source, RemoveEventPoke, Idx);
            SmallButtonAtWithCallback(DeleteText, LuaPoint(EditRect.x + EditRect.w + HPadding(), ButtonY), true, IDGen);
        }

        AdvanceY(CurrY);
    }}

    // Button to add a new pair...
    AdvanceY(CurrY);
    if (ButtonAt("Add Possible Event", HPadPtAtY(CurrY), true, IDGen))
    {
        PushUndoPos("Add possible event");
        Source->AddPossibleEvent("");
    }
    AdvanceYLastControl(CurrY);

    return CurrY;
}



static void
ExtractScalarTracks(track_group* TrackGroup,
                    model* CurrentModel,
                    vector<const char*>& FriendlyNames,
                    vector<const char*>& RealNames,
                    vector<uint32>& TrackKeys)
{
    skeleton* Skeleton = CurrentModel->Skeleton;

    {for (int TTIdx = 0; TTIdx < TrackGroup->VectorTrackCount; ++TTIdx)
    {
        // Only take d = 1 curves...
        if (TrackGroup->VectorTracks[TTIdx].Dimension != 1)
            continue;

        char const* TrackName = TrackGroup->VectorTracks[TTIdx].Name;
        uint32      TrackKey = TrackGroup->VectorTracks[TTIdx].TrackKey;

        // n^2 what?
        char const* BoneName = 0;
        {for (int Idx = 0; !BoneName && Idx < Skeleton->BoneCount; ++Idx)
        {
            if (VectorTrackKeyForBone(*Skeleton, Idx, TrackName) == TrackKey)
                BoneName = Skeleton->Bones[Idx].Name;
        }}


        char* Friendly;
        if (BoneName)
        {
            int Len = strlen(BoneName) + 2 + strlen(TrackName) + 1;
            Friendly = GStateAllocArray(char, Len);
            sprintf(Friendly, "%s: %s", BoneName, TrackName);
        }
        else
        {
            int Len = 7 + strlen(TrackName) + 1;
            Friendly = GStateAllocArray(char, Len);
            sprintf(Friendly, "<unk>: %s", TrackName);
        }

        FriendlyNames.push_back(Friendly);
        RealNames.push_back(TrackGroup->VectorTracks[TTIdx].Name);
        TrackKeys.push_back(TrackGroup->VectorTracks[TTIdx].TrackKey);
    }}
}


// Shared animation node editor
static int
VisitAnimationNode(lua_State*   LuaState,
                   anim_source* Animation,
                   bool         VisitorIsBlendGraph)
{
    Assert(Animation);

    LuaRect const& drawArea = UIAreaGet();

    ControlIDGen IDGen("animation_edit_box_%x_%d_events",
                       edit::TokenizedToID(Animation),
                       Animation->GetNumOutputs());

    // First we need to get a list of the animation slots. We'll need these as strings and
    // as an array so we can set the combobox entry properly.  Get this from the edit
    // state, as there may not be a character instance yet...
    vector<animation_slot*> AnimationSlots;
    edit::GetAnimationSlots(&AnimationSlots);

    vector<animation_spec> AnimationSpecs;
    edit::GetAnimationSpecs(&AnimationSpecs);


    // An animation source has exactly one pose output.  All the rest must be event
    // tracks or scalar tracks for now
    if (Animation->GetNumOutputs() < 1 || Animation->GetOutputType(0) != PoseEdge)
    {
        Assert(false);
        return -1;
    }
    else
    {
        {for (int Idx = 1; Idx < Animation->GetNumOutputs(); ++Idx)
        {
            if (Animation->GetOutputType(Idx) != EventEdge &&
                Animation->GetOutputType(Idx) != ScalarEdge &&
                Animation->GetOutputType(Idx) != MorphEdge)
            {
                Assert(false);
                return -1;
            }
        }}
    }

    int32x CurrY = VPadding();
    SectionHeaderAt("Animation track", CurrY, drawArea);
    AdvanceYLastControl(CurrY);

    if (EditAnimSource(LuaState, VisitorIsBlendGraph,
                       Animation,
                       drawArea.w,
                       CurrY,
                       AnimationSlots))
    {
        // If the edge changes, we need to clear out the event tracks, or at least do a match...
        // TODO ?
    }


    // Put the header up even if we'll ignore the tracks...
    if (VisitorIsBlendGraph || Animation->GetNumOutputs() > 1)
    {
        AdvanceY(CurrY);
        SectionHeaderAt("Scalar and Event Tracks", CurrY, drawArea);
        AdvanceYLastControl(CurrY);
    }

    if (Animation->GetNumOutputs() > 1)
    {
        char const* OneShotLabel = "OneShot:";
        char const* RemoveLabel  = "(remove)";
        int32x const MaxLeftW = max(LabelWidth(OneShotLabel), SmallButtonWidth(RemoveLabel));

        int32x LabelX, ControlX;
        LabeledControlsAtWidth(MaxLeftW, &LabelX, &ControlX);

        // Ugly?  Oh yeah.
        animation_slot*   ThisSlot   = Animation->GetAnimationSlot();
        animation_spec*   ThisSpec   = ThisSlot  ? &AnimationSpecs[ThisSlot->Index] : 0;
        source_file_ref*  SourceRef  = ThisSpec  ? ThisSpec->SourceReference : 0;
        granny_file_info* SourceInfo = SourceRef ? SourceRef->SourceInfo     : 0;

        GStateAssert(SourceInfo == 0 ||
                     (ThisSpec->AnimationIndex >= 0 &&
                      ThisSpec->AnimationIndex < SourceInfo->AnimationCount));
        animation* CurrentAnimation = SourceInfo ? (animation*)SourceInfo->Animations[ThisSpec->AnimationIndex] : 0;

        // We need a model so we can find the appropriate trackgroup.  If it's not
        // present, we'll just display the current setting, and disallow changes...
        model* CurrentModel = 0;
        track_group* TrackGroup = 0;
        do
        {
            // Check the precondition
            if (CurrentAnimation == 0)
                break;

            gstate_character_instance* Instance = edit::GetCharacterInstance();
            if (!Instance)
                break;

            CurrentModel = (model*)GetSourceModelForCharacter(Instance);
            if (!CurrentModel)
                break;

            int32x TGIdx;
            if (FindTrackGroupForModel(*CurrentAnimation, Model->Name, TGIdx))
                TrackGroup = CurrentAnimation->TrackGroups[TGIdx];
        } while(false);

        // Valid morph tracks belong to the same *animation*, but not necessarily the same
        // *track_group*
        vector<const char*> MorphTrackNames;
        if (CurrentAnimation)
        {
            for (int TGIdx = 0; TGIdx < CurrentAnimation->TrackGroupCount; ++TGIdx)
            {
                if (CurrentAnimation->TrackGroups[TGIdx]->Flags & TrackGroupIsMorphs)
                    MorphTrackNames.push_back(CurrentAnimation->TrackGroups[TGIdx]->Name);
            }
        }

        vector<bool>        EventIsAnimEvents;
        vector<const char*> EventTrackNames;

        // These are separate because the combo box needs the strings on their own
        vector<const char*>   FriendlyTrackNames;
        vector<const char*>   ScalarTrackNames;
        vector<granny_uint32> ScalarTrackKeys;

        EventIsAnimEvents.push_back(true);
        EventTrackNames.push_back("<Anim. events>");


        // todo: this fails to account for trackgroups that have the same text_track name...
        if (TrackGroup)
        {
            {for (int TTIdx = 0; TTIdx < TrackGroup->TextTrackCount; ++TTIdx)
            {
                EventIsAnimEvents.push_back(false);
                EventTrackNames.push_back(TrackGroup->TextTracks[TTIdx].Name);
            }}

            // Get the names, plus the friendly display with the bone ids...
            ExtractScalarTracks(TrackGroup, CurrentModel,
                                FriendlyTrackNames,
                                ScalarTrackNames, ScalarTrackKeys);
        }

        {for (int Idx = 1; Idx < Animation->GetNumOutputs(); ++Idx)
        {
            AdvanceY(CurrY);

            static char const* s_EdgeNames[] = { "Pose:", "Scalar:", "Mask:", "Event:" };

            node_edge_type EdgeType = Animation->GetOutputType(Idx);

            ColorLabelAt(s_EdgeNames[EdgeType], LuaPoint(LabelX, CurrY),
                         node_drawing::s_EdgeColors[0][EdgeType],
                         eRight);
            LabelAt(Animation->GetOutputName(Idx), LuaPoint(ControlX, CurrY));
            AdvanceYLastControl(CurrY);


            // @@magic number
            LuaRect Rect = FillRectToRight(ControlX, drawArea.w, CurrY, 26);
            if (Animation->GetOutputType(Idx) == EventEdge)
            {
                char const* CurrentTrack = Animation->GetEventTrackName(Idx);
                if (TrackGroup)
                {
                    int CurrentEntry = -1;
                    {for (int NameIdx = 0; CurrentTrack && NameIdx < int(EventTrackNames.size()); ++NameIdx)
                    {
                        if (_stricmp(CurrentTrack, EventTrackNames[NameIdx]) == 0)
                        {
                            CurrentEntry = NameIdx;
                            break;
                        }
                    }}

                    if (ITunesComboboxOn(EventTrackNames, CurrentEntry, Rect, IDGen))
                    {
                        PushUndoPos("Change event track");

                        if (CurrentEntry >= 0 && CurrentEntry < int(EventTrackNames.size()))
                        {
                            Animation->SetEventTrack(Idx, EventIsAnimEvents[CurrentEntry], EventTrackNames[CurrentEntry]);
                            if (VisitorIsBlendGraph)
                                Animation->SetOutputName(Idx, EventTrackNames[CurrentEntry]);
                        }
                        else
                        {
                            Animation->SetEventTrack(Idx, false, NULL);
                            if (VisitorIsBlendGraph)
                                Animation->SetOutputName(Idx, "Event");
                        }
                    }
                }
                else
                {
                    ComboboxPlaceholder(CurrentTrack, Rect, IDGen);
                }
            }
            else if (Animation->GetOutputType(Idx) == MorphEdge)
            {
                char const* CurrentTrackName = Animation->GetMorphTrackName(Idx);
                LuaRect Rect(ControlX, CurrY, 200, 26);
                if (CurrentAnimation)
                {
                    int CurrentEntry = -1;
                    {for (int NameIdx = 0; CurrentTrackName && NameIdx < int(MorphTrackNames.size()); ++NameIdx)
                    {
                        if (_stricmp(CurrentTrackName, MorphTrackNames[NameIdx]) == 0)
                        {
                            CurrentEntry = NameIdx;
                            break;
                        }
                    }}

                    if (ITunesComboboxOn(MorphTrackNames, CurrentEntry, Rect, IDGen))
                    {
                        PushUndoPos("Change morph track");

                        if (CurrentEntry >= 0 && CurrentEntry < int(MorphTrackNames.size()))
                        {
                            Animation->SetMorphTrackName(Idx, MorphTrackNames[CurrentEntry]);
                            if (VisitorIsBlendGraph)
                                Animation->SetOutputName(Idx, MorphTrackNames[CurrentEntry]);
                        }
                        else
                        {
                            Animation->SetOutputName(Idx, NULL);
                            if (VisitorIsBlendGraph)
                                Animation->SetOutputName(Idx, "Morph");
                        }
                    }
                }
                else
                {
                    ComboboxPlaceholder(CurrentTrackName, Rect, IDGen);
                }
            }
            else
            {
                // Only one left...
                Assert(Animation->GetOutputType(Idx) == ScalarEdge);

                char const* CurrentTrack;
                granny_uint32 CurrentTrackKey;
                Animation->GetScalarTrack(Idx, CurrentTrack, CurrentTrackKey);
                if (TrackGroup)
                {
                    int CurrentEntry = -1;
                    {for (int NameIdx = 0; CurrentTrack && NameIdx < int(ScalarTrackNames.size()); ++NameIdx)
                    {
                        if (CurrentTrackKey == ScalarTrackKeys[NameIdx] &&
                            _stricmp(CurrentTrack, ScalarTrackNames[NameIdx]) == 0)
                        {
                            CurrentEntry = NameIdx;
                            break;
                        }
                    }}

                    LuaRect Rect(ControlX, CurrY, 200, 26);
                    if (ITunesComboboxOn(FriendlyTrackNames, CurrentEntry, Rect, IDGen))
                    {
                        PushUndoPos("Change scalar track");

                        if (CurrentEntry >= 0 && CurrentEntry < int(ScalarTrackNames.size()))
                        {
                            Animation->SetScalarTrack(Idx, ScalarTrackNames[CurrentEntry], ScalarTrackKeys[CurrentEntry]);
                            if (VisitorIsBlendGraph)
                                Animation->SetOutputName(Idx, ScalarTrackNames[CurrentEntry]);
                        }
                        else
                        {
                            Animation->SetScalarTrack(Idx, NULL, 0);
                            if (VisitorIsBlendGraph)
                                Animation->SetOutputName(Idx, "Scalar");
                        }
                    }
                }
                else
                {
                    ComboboxPlaceholder(CurrentTrack, Rect, IDGen);
                }
            }
            RegisterToolTip("Selects the animation track for this edge. Note that\n"
                            "a model must be selected to change this property so\n"
                            "the tool can find the correct track_group.",
                            Rect);

            if (VisitorIsBlendGraph)
            {
                PushTokenizedIntClosure(Animation, RemoveNodeOutput, Idx);
                SmallButtonAtWithCallback(RemoveLabel, HPadPtAtY(CurrY), true, IDGen);
            }

            AdvanceYRect(CurrY, Rect);
        }}

        // We have to delete the memory for the friendly track names...
        {for (size_t Idx = 0; Idx < FriendlyTrackNames.size(); ++Idx)
        {
            GStateDeallocate((void*&)FriendlyTrackNames[Idx]);
        }}
    }

    // Note that state machine nodes are not allowed to add their own event tracks.  That
    // will be handled by the state machine itself...
    if (VisitorIsBlendGraph)
    {
        AdvanceY(CurrY, 5);

        char const* AddScalarLabel = "Add Scalar Output";
        char const* AddEventLabel  = "Add Event  Output";
        char const* AddMorphLabel  = "Add Morph  Output";
        int const AddScalarWidth = SmallButtonWidth(AddScalarLabel);
        int const totalWidth = AddScalarWidth*3 + HPadding()*2;

        int CurrX = (drawArea.w - totalWidth) / 2;
        if (SmallButtonAt(AddScalarLabel, LuaPoint(CurrX, CurrY), true, IDGen))
        {
            PushUndoPos("anim_source add scalar");
            Animation->AddOutput(ScalarEdge, "Scalar");
        }
        CurrX += AddScalarWidth + HPadding();

        if (SmallButtonAt(AddEventLabel, LuaPoint(CurrX, CurrY), true, IDGen))
        {
            PushUndoPos("anim_source add event");
            Animation->AddOutput(EventEdge, "Event");
        }
        CurrX += AddScalarWidth + HPadding();

        if (SmallButtonAt(AddMorphLabel, LuaPoint(CurrX, CurrY), true, IDGen))
        {
            PushUndoPos("anim_source add morph");
            Animation->AddOutput(MorphEdge, "Morph");
        }

        AdvanceYLastControl(CurrY);
        AdvanceY(CurrY); // Extra just for visual ease when scrolling
    }

    return CurrY;
}


// =============================================================================
// =============================================================================
// =============================================================================
// =============================================================================
class statenode_edit_visitor : public node_visitor
{
public:
    lua_State*  LuaState;
    int LastY;

    statenode_edit_visitor(lua_State*  L) : LuaState(L), LastY(-1) { }
    virtual void VisitNode(anim_source* Animation);
    virtual void VisitNode(parameters* VisitNode);
    virtual void VisitNode(event_source*  EventSource);
    virtual void VisitNode(state_machine* StateMachine);
};

// -----------------------------------------------------------------------------
// anim_source
void
statenode_edit_visitor::VisitNode(anim_source* Animation)
{
    LastY = VisitAnimationNode(LuaState, Animation, false);
}

// -----------------------------------------------------------------------------
// parameters
void
statenode_edit_visitor::VisitNode(parameters* Parameters)
{
    Assert(Parameters);

    LastY = SharedParametersEdit(LuaState, Parameters);
}


void
statenode_edit_visitor::VisitNode(event_source* Source)
{
    LastY = SharedEventSourceEdit(Source);
}


// -----------------------------------------------------------------------------
void
statenode_edit_visitor::VisitNode(state_machine* StateMachine)
{
    LastY = EditStateMachineNode(LuaState, StateMachine, LastY);
}



// =============================================================================
// =============================================================================
// =============================================================================
// =============================================================================
class blendnode_edit_visitor : public node_visitor
{
public:
    lua_State*  LuaState;
    int LastY;

    blendnode_edit_visitor(lua_State*  L) : LuaState(L), LastY(-1) { }

    virtual void VisitNode(aimat_ik*          AimAt);
    virtual void VisitNode(anim_source*       Animation);
    virtual void VisitNode(blend*             Blend);
    virtual void VisitNode(blend_graph*       BlendGraph);
    virtual void VisitNode(constant_velocity* ConstantVel);
    virtual void VisitNode(event_source*      EventSource);
    virtual void VisitNode(event_renamer*     EventRenamer);
    virtual void VisitNode(mask_source*       MaskSource);
    virtual void VisitNode(mirror*            Mirror);
    virtual void VisitNode(nway_blend*        NWayBlend);
    virtual void VisitNode(parameters*        Parameters);
    virtual void VisitNode(pose_storage*      PoseStorage);
    virtual void VisitNode(selection*         Selection);
    virtual void VisitNode(state_machine*     StateMachine);
    virtual void VisitNode(timeselect*             Blend);
    virtual void VisitNode(timeshift*         Timeshift);
    virtual void VisitNode(twobone_ik*        TwoBone);
};

// -----------------------------------------------------------------------------
// blend_graph
void
blendnode_edit_visitor::VisitNode(blend_graph* BlendGraph)
{
    LuaRect drawArea = UIAreaGet();

    int CurrY = LastY + VPadding();
    ControlIDGen IDGen("blend_node_edit_box_%x_%d",
                       edit::TokenizedToID(BlendGraph),
                       BlendGraph->GetNumOutputs());

    int32x const centerX = CenterX(drawArea);
    int32x const offsetX = 30;

    // List the input and outputs...
    SectionHeaderAt("Remove inputs and outputs", CurrY, drawArea);
    AdvanceYLastControl(CurrY);
    {
        int EdgesStartY = CurrY;

        {for (int32x Idx = 0; Idx < BlendGraph->GetNumInputs(); ++Idx)
        {
            if (BlendGraph->IsInputInternal(Idx))
                continue;

            int Width = SmallButtonWidth(BlendGraph->GetInputName(Idx));

            PushTokenizedIntClosure(BlendGraph, RemoveNodeInput, Idx);
            SmallButtonAtWithCallback(BlendGraph->GetInputName(Idx),
                                      LuaPoint(centerX - Width - offsetX, CurrY),
                                      true, IDGen);

            LastControlTip("Deletes this input");
            AdvanceYLastControl(CurrY);
        }}
        int EndY = CurrY;
        CurrY = EdgesStartY;

        {for (int32x Idx = 0; Idx < BlendGraph->GetNumOutputs(); ++Idx)
        {
            if (BlendGraph->IsOutputInternal(Idx))
                continue;

            PushTokenizedIntClosure(BlendGraph, RemoveNodeOutput, Idx);
            SmallButtonAtWithCallback(BlendGraph->GetOutputName(Idx),
                                      LuaPoint(centerX + offsetX, CurrY),
                                      true, IDGen);

            LastControlTip("Deletes this output");
            AdvanceYLastControl(CurrY);
        }}

        CurrY = max(CurrY, EndY);
        AdvanceY(CurrY);
    }

    SectionHeaderAt("Add inputs and outputs", CurrY, drawArea);
    AdvanceYLastControl(CurrY);

    // Input side
    int const StartY = CurrY;

    char const* EdgeNames[] = { "Pose", "Scalar", "Mask", "Event", "Morph" };
    {for (int Idx = 0; Idx < EdgeTypeCount; ++Idx)
    {
        int Width = SmallButtonWidth(EdgeNames[Idx]);

        if (SmallButtonAt(EdgeNames[Idx], LuaPoint(CenterX(drawArea) - Width - 30, CurrY), true, IDGen))
        {
            PushUndoPos("Add Input");
            BlendGraph->AddInput(node_edge_type(Idx), EdgeNames[Idx]);
        }
        AdvanceYLastControl(CurrY);
    }}

    // Reset and move to the second column
    CurrY = StartY;
    {for (int Idx = 0; Idx < EdgeTypeCount; ++Idx)
    {
        if (SmallButtonAt(EdgeNames[Idx], LuaPoint(CenterX(drawArea) + 30, CurrY), true, IDGen))
        {
            PushUndoPos("Add Output");
            BlendGraph->AddOutput(node_edge_type(Idx), EdgeNames[Idx]);
        }
        AdvanceYLastControl(CurrY);
    }}

    LastY = CurrY;
}

// -----------------------------------------------------------------------------
// mask_source
void
blendnode_edit_visitor::VisitNode(mask_source* MaskSource)
{
    Assert(MaskSource);

    ControlIDGen IDGen("mask_source_box_%x", edit::TokenizedToID(MaskSource));

    // @@ adjustable size
    LuaPoint pt;
    if (MaskEditOn(MaskSource, IDGen))
    {
        edit::MarkModified();
    }

    LastY = LastControlRect().h;
}

// -----------------------------------------------------------------------------
void
blendnode_edit_visitor::VisitNode(selection* Selection)
{
    Assert(Selection);

    ControlIDGen IDGen("selection_%x_%d", edit::TokenizedToID(Selection), Selection->GetNumInputs());

    int32x CurrY = VPadding();
    {for (int Idx = 1; Idx < Selection->GetNumInputs(); ++Idx)
    {
        // Add/Delete at the beginning
        char buttonString[256];
        sprintf(buttonString, "Delete %s", Selection->GetInputName(Idx));

        PushTokenizedIntClosure(Selection, RemoveNodeInput, Idx);
        ButtonAtWithCallback(buttonString, HPadPtAtY(CurrY),
                             Selection->GetNumInputs() > 3,
                             IDGen);

        AdvanceYLastControl(CurrY);
    }}

    if (ButtonAt("Add Input", HPadPtAtY(CurrY), true, IDGen))
    {
        PushUndoPos("Selection add animation");
        Selection->AddInput(PoseEdge, "Input");
    }
    AdvanceYLastControl(CurrY);

    LastY = CurrY;
}

// -----------------------------------------------------------------------------
void
blendnode_edit_visitor::VisitNode(state_machine* StateMachine)
{
    LastY = EditStateMachineNode(LuaState, StateMachine, LastY);
}


// -----------------------------------------------------------------------------
void
blendnode_edit_visitor::VisitNode(nway_blend* NWayBlend)
{
    LuaRect drawArea = UIAreaGet();

    char const* DeleteInputLabel = "Delete input:";

    int32x LabelX, ControlX;
    LabeledControlsAtWidth(LabelWidth(DeleteInputLabel), &LabelX, &ControlX);

    int CurrY = VPadding();
    ControlIDGen IDGen("nway_blend_node_edit_box_%x_%d",
                       edit::TokenizedToID(NWayBlend),
                       NWayBlend->GetNumInputs());

    // Delete buttons for the inputs...
    {
        bool Enabled = (NWayBlend->GetNumInputs() > 3);
        {for (int32x Idx = 0; Idx < NWayBlend->GetNumInputs(); ++Idx)
        {
            if (NWayBlend->GetInputType(Idx) != PoseEdge)
                continue;

            LabelAt("Delete input:", LuaPoint(LabelX, SmallButtonLabelAdjust(CurrY)), eRight);

            PushTokenizedIntClosure(NWayBlend, RemoveNodeInput, Idx);
            SmallButtonAtWithCallback(NWayBlend->GetInputName(Idx),
                                      LuaPoint(ControlX, CurrY),
                                      Enabled, IDGen);

            AdvanceYLastControl(CurrY);
        }}

        SeparatorAt(CurrY, drawArea);
        AdvanceYLastControl(CurrY);
    }

    // The add button
    {
        LabelAt("Add Input:", LuaPoint(LabelX, SmallButtonLabelAdjust(CurrY)), eRight);

        if (SmallButtonAt("Pose", LuaPoint(ControlX, CurrY), true, IDGen))
        {
            PushUndoPos("Add PoseInput (nway_blend)");
            NWayBlend->AddInput(PoseEdge, "Pose");
        }
        AdvanceYLastControl(CurrY);
    }

    LastY = CurrY;
}



// -----------------------------------------------------------------------------
// timeshift
void
blendnode_edit_visitor::VisitNode(timeshift* Timeshift)
{
    Assert(Timeshift);

    int32x CurrY = VPadding();

    ControlIDGen IDGen("timeshift_edit_box_%x_%d", edit::TokenizedToID(Timeshift), Timeshift->GetNumOutputs());

    {for (int Idx = 0; Idx < Timeshift->GetNumOutputs(); ++Idx)
    {
        PushTokenizedIntClosure(Timeshift, RemoveNodeOutput, Idx);
        ButtonAtWithCallback("Del Edge", HPadPtAtY(CurrY), true, IDGen);

        int32x LabelX, ControlX;
        LabeledControlsAtWidth(LastControlRect().w, &LabelX, &ControlX);

        std::string Name = Timeshift->GetOutputName(Idx);
        if (EditLabelOn(Name, FillRectToRight(ControlX, UIAreaGet().w, CurrY, 24), IDGen))
        {
            PushUndoPos("Change t/s edge name");
            Timeshift->SetOutputName(Idx, Name.c_str());
            Timeshift->SetInputName(Idx+2, Name.c_str());
        }

        AdvanceYLastControl(CurrY);
    }}

    AdvanceY(CurrY);
    AdvanceY(CurrY);

    PushTokenizedClosure(Timeshift, AddPoseOutputToNode, 0);
    ButtonAtWithCallback("Add Edge", HPadPtAtY(CurrY), true, IDGen);
    AdvanceYLastControl(CurrY);

    LastY = CurrY;
}

// -----------------------------------------------------------------------------
// timeselect
void
blendnode_edit_visitor::VisitNode(timeselect* Timeselect)
{
    Assert(Timeselect);

    ControlIDGen IDGen("timeselect_edit_box_%x", edit::TokenizedToID(Timeselect));

    const char* LockLabel = "Time is relative:";

    int32x LabelX, ControlX;
    LabeledControlsAtWidth(LabelWidth(LockLabel), &LabelX, &ControlX);

    int CurrY = VPadding();

    LabelAt(LockLabel, LuaPoint(LabelX, CheckboxLabelAdjust(CurrY)), eRight);

    bool IsRelative = Timeselect->GetTimeIsRelative();

    LuaRect boxRect = CheckboxDim("");
    boxRect.x = ControlX;
    boxRect.y = CurrY;
    if (CheckboxOn(IsRelative, "", boxRect, IDGen))
    {
        PushUndoPos("Timeselect relative change");

        Timeselect->SetTimeIsRelative(IsRelative);
    }
    AdvanceYLastControl(CurrY);

    {for (int Idx = 0; Idx < Timeselect->GetNumOutputs(); ++Idx)
    {
        PushTokenizedIntClosure(Timeselect, RemoveNodeOutput, Idx);
        ButtonAtWithCallback("Del Edge", HPadPtAtY(CurrY), true, IDGen);

        int32x LabelX, ControlX;
        LabeledControlsAtWidth(LastControlRect().w, &LabelX, &ControlX);

        std::string Name = Timeselect->GetOutputName(Idx);
        if (EditLabelOn(Name, FillRectToRight(ControlX, UIAreaGet().w, CurrY, 24), IDGen))
        {
            PushUndoPos("Change t/s edge name");
            Timeselect->SetOutputName(Idx, Name.c_str());
            Timeselect->SetInputName(Idx+1, Name.c_str());
        }

        AdvanceYLastControl(CurrY);
    }}

    AdvanceY(CurrY);
    AdvanceY(CurrY);

    PushTokenizedClosure(Timeselect, AddPoseOutputToNode, 0);
    ButtonAtWithCallback("Add Edge", HPadPtAtY(CurrY), true, IDGen);
    AdvanceYLastControl(CurrY);

    LastY = CurrY;
}

// -----------------------------------------------------------------------------
// blend
void
blendnode_edit_visitor::VisitNode(blend* Blend)
{
    Assert(Blend);

    ControlIDGen IDGen("blend_phase_lock_%x", edit::TokenizedToID(Blend));

    const char* LockLabel = "Phase locked:";
    const char* NeighborhoodLabel = "Use neighborhooded blend:";

    int32x LabelX, ControlX;
    LabeledControlsAtWidth(LabelWidth(NeighborhoodLabel), &LabelX, &ControlX);

    int CurrY = VPadding();
    LuaRect boxRect = CheckboxDim("");

    if (Blend->CanPhaseLock(true))
    {
        LabelAt(LockLabel, LuaPoint(LabelX, CheckboxLabelAdjust(CurrY)), eRight);

        bool IsLocked = Blend->GetPhaseLocked();

        boxRect.x = ControlX;
        boxRect.y = CurrY;
        if (CheckboxOn(IsLocked, "", boxRect, IDGen))
        {
            PushUndoPos("Blend lock change");

            Blend->SetPhaseLocked(IsLocked);

            // This is a guess, basically.  It could be wrong given the external parent?
            extern real32 g_GlobalClock;
            Blend->Activate(0, g_GlobalClock);
        }
        AdvanceYLastControl(CurrY);
    }

    LabelAt(NeighborhoodLabel, LuaPoint(LabelX, CheckboxLabelAdjust(CurrY)), eRight);
    bool UseN = Blend->GetNeighborhooded();

    boxRect.x = ControlX;
    boxRect.y = CurrY;
    if (CheckboxOn(UseN, "", boxRect, IDGen))
    {
        PushUndoPos("Blend neighborhood change");

        Blend->SetNeighborhooded(UseN);
    }
    AdvanceYLastControl(CurrY);

    LastY = CurrY;
}


// -----------------------------------------------------------------------------
// anim_source
void
blendnode_edit_visitor::VisitNode(anim_source* Animation)
{
    LastY = VisitAnimationNode(LuaState, Animation, true);
}

void
blendnode_edit_visitor::VisitNode(parameters* Parameters)
{
    Assert(Parameters);

    LastY = SharedParametersEdit(LuaState, Parameters);
}

void
blendnode_edit_visitor::VisitNode(pose_storage* PoseStorage)
{
    Assert(PoseStorage);

    // We want this to create all new items if the input changes, or the duration of the
    // input node is altered...
    bool InputConnected = false;
    float Duration = 10.0f;  // bogus default duration for nodes that don't report duration...
    {
        node* OtherNode;
        granny_int32x Output;
        PoseStorage->GetInputConnection(0, &OtherNode, &Output);

        if (OtherNode)
        {
            InputConnected = true;

            real32 ReportedDuration = OtherNode->GetDuration(Output);
            if (ReportedDuration > 0)
                Duration = ReportedDuration;
        }
    }

    ControlIDGen IDGen("pose_storage_%x_%d_%x",
                       edit::TokenizedToID(PoseStorage),
                       InputConnected ? 1 : 0,
                       *((uint32*)&Duration));

    // Compute the control offset...
    int32x LabelX, ControlX;
    LabeledControlsAtWidth(100, &LabelX, &ControlX);

    int CurrY = VPadding();

    // @@magic
    LuaRect SliderRect = FillRectToRight(ControlX, UIAreaGet().w, 0, 26);

    if (SmallButtonAt("Rest Pose", LuaPoint(ControlX, CurrY), true, IDGen))
    {
        PushUndoPos("rest pose");

        PoseStorage->ReturnToRestPose();
    }
    AdvanceYLastControl(CurrY);


    LabelAt("Capture time:", LuaPoint(LabelX, CurrY), eRight);
    SliderRect.y = CurrY;

    real32 Time = 0;
    if (SliderOn(0, Duration, Time, false, SliderRect, IDGen))
    {
        if (InputConnected)
        {
            PushUndoPos("Change Capture", false, eSliderMotion, PoseStorage->GetUID());

            PoseStorage->CaptureInput(Time);
        }
    }
    AdvanceYLastControl(CurrY);
    LastY = CurrY;
}


void
blendnode_edit_visitor::VisitNode(constant_velocity* ConstantVel)
{
    Assert(ConstantVel);

    // We want this to create all new items if the number of outputs change...
    ControlIDGen IDGen("constant_velocity_edit_box_%x_%d",
                       edit::TokenizedToID(ConstantVel),
                       ConstantVel->GetNumOutputs());

    // Compute the control offset...
    int32x LabelX, ControlX;
    LabeledControlsAtWidth(100, &LabelX, &ControlX);

    // @@magic
    LuaRect SliderRect = FillRectToRight(ControlX, UIAreaGet().w, 0, 26);

    int MinMaxValWidth = 50;
    int MinStart = ControlX;
    int MaxStart = ControlX + SliderRect.w - MinMaxValWidth;
    int ValStart = (MinStart + MaxStart) / 2;

    static char const* Names[3] = {
        "X Component",
        "Y Component",
        "Z Component"
    };
    real32 Axis[3];
    ConstantVel->GetAxis(Axis);

    int CurrY = VPadding();
    {for (int32x Idx = 0; Idx < 3; ++Idx)
    {
        LabelAt(Names[Idx], HPadPtAtY(CurrY));

        SliderRect.y = CurrY;
        if (SliderOn(-1, 1, Axis[Idx], false, SliderRect, IDGen))
        {
            PushUndoPos("Change axis", false, eSliderMotion, ConstantVel->GetUID());

            // Compute the correction to make this axis component correct
            real32 det = (1 - Axis[Idx]*Axis[Idx]);
            if (det < 1e-4)
            {
                Axis[(Idx+1)%3] = 0;
                Axis[(Idx+2)%3] = 0;
            }
            else
            {
                real32 AxisSum = (Axis[(Idx+1)%3]* Axis[(Idx+1)%3] +
                                  Axis[(Idx+2)%3]* Axis[(Idx+2)%3]);
                if (AxisSum < 1e-6)
                {
                    Axis[(Idx+1)%3] = 1;
                    Axis[(Idx+2)%3] = 1;
                    AxisSum = 2;
                }

                real32 fac = SquareRoot(AxisSum / det);
                Axis[(Idx+1)%3] /= fac;
                Axis[(Idx+2)%3] /= fac;
            }

            ConstantVel->SetAxis(Axis);
        }
        AdvanceYLastControl(CurrY);
    }}

    // Current
    {
        LabelAt("Speed", HPadPtAtY(CurrY));

        char buffer[32];
        sprintf(buffer, "%.2f", ConstantVel->GetSpeed());
        std::string ParamStr = buffer;
        if (EditLabelOn(ParamStr, LuaRect(ValStart, CurrY, MinMaxValWidth, 24), IDGen))
        {
            PushUndoPos("Change speed");

            real32 Param;
            sscanf(ParamStr.c_str(), "%f", &Param);
            ConstantVel->SetSpeed(Param);
        }
        AdvanceYLastControl(CurrY);
    }

    LastY = CurrY;
}

void
blendnode_edit_visitor::VisitNode(twobone_ik* TwoBone)
{
    Assert(TwoBone);

    LuaRect drawArea = UIAreaGet();

    ControlIDGen IDGen("twobone_%x_%d_%d_%d_%s",
                       edit::TokenizedToID(TwoBone),
                       TwoBone->GetBoundFootIndex(),
                       TwoBone->GetBoundKneeIndex(),
                       TwoBone->GetBoundHipIndex(),
                       (Model ? "yes" : "no"));

    char const* FootLabel      = "Foot Effector:";  // longest...
    char const* KneeLabel      = "Knee Joint:";
    char const* HipLabel       = "Hip Joint:";
    char const* KneePlaneLabel = "Knee plane:";

    int32 LabelX, ControlX;
    LabeledControlsAtWidth(LabelWidth(FootLabel), &LabelX, &ControlX);

    float FootPosition[3];
    TwoBone->GetDefaultFootPosition(FootPosition);

    int CurrY = VPadding();

    // @@magic: heights
    // @@todo: cleanup, seriously janky
    // Don't display the full interface unless the model exists
    if (Model == 0)
    {
        LabelAt(FootLabel, LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);
        ComboboxPlaceholder(TwoBone->GetFootName(),
                            FillRectToRight(ControlX, drawArea.w, CurrY, 26),
                            IDGen);
        AdvanceYLastControl(CurrY);

        LabelAt(KneeLabel, LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);
        ComboboxPlaceholder(TwoBone->GetKneeName(),
                            FillRectToRight(ControlX, drawArea.w, CurrY, 26),
                            IDGen);
        AdvanceYLastControl(CurrY);

        LabelAt(HipLabel, LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);
        ComboboxPlaceholder(TwoBone->GetHipName(),
                            FillRectToRight(ControlX, drawArea.w, CurrY, 26),
                            IDGen);
        AdvanceYLastControl(CurrY);

        int Axis = TwoBone->GetKneePlane();
        LabelAt(KneeLabel, LuaPoint(LabelX, CurrY), eRight);
        AxisSelector(Axis, FillRectToRight(ControlX, drawArea.w, CurrY, 26), false, IDGen);
        AdvanceYLastControl(CurrY);

        LastY = CurrY;
        return;
    }

    // Spacer
    AdvanceY(CurrY, 10);

    vector<char const*> BoneNames(Model->Skeleton->BoneCount);
    vector<int> BoneIndices(Model->Skeleton->BoneCount);
    {for (int i = 0; i < Model->Skeleton->BoneCount; ++i)
    {
        BoneNames[i] = Model->Skeleton->Bones[i].Name;
    }}

    // "Foot" Bone
    LabelAt(FootLabel, LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);
    {
        int CurrentEntry = TwoBone->GetBoundFootIndex();
        if (ITunesComboboxOn(BoneNames, CurrentEntry,
                             FillRectToRight(ControlX, drawArea.w, CurrY, 26),
                             IDGen))
        {
            PushUndoPos("Change hip joint");

            TwoBone->SetFootName(BoneNames[CurrentEntry]);

            // Todo: maybe scan to see if we *need* to clear these...
            TwoBone->SetKneeName(0);
            TwoBone->SetHipName(0);
        }
        AdvanceYLastControl(CurrY);
    }

    //
    // todo: factor out...
    //
    LabelAt(KneeLabel, LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);
    if (TwoBone->GetBoundFootIndex() != -1)
    {
        // Active link specs.  We need to reset the bone names array to only include
        // parents of the head bone...
        BoneNames.resize(0);
        BoneIndices.resize(0);

        int Walk = Model->Skeleton->Bones[TwoBone->GetBoundFootIndex()].ParentIndex;
        for (; Walk != GrannyNoParentBone; Walk = Model->Skeleton->Bones[Walk].ParentIndex)
        {
            BoneNames.insert(BoneNames.begin(), Model->Skeleton->Bones[Walk].Name);
            BoneIndices.insert(BoneIndices.begin(), Walk);
        }

        int CurrentEntry = -1;
        {for (size_t Idx = 0; Idx < BoneIndices.size(); ++Idx)
        {
            if (TwoBone->GetBoundKneeIndex() == BoneIndices[Idx])
            {
                CurrentEntry = Idx;
                break;
            }
        }}

        if (ITunesComboboxOn(BoneNames, CurrentEntry,
                             FillRectToRight(ControlX, drawArea.w, CurrY, 26),
                             IDGen))
        {
            PushUndoPos("Set knee joint");

            TwoBone->SetKneeName(BoneNames[CurrentEntry]);

            // Todo: maybe scan to see if we *need* to clear these...
            TwoBone->SetHipName(0);
        }
    }
    else
    {
        ComboboxPlaceholder(0, FillRectToRight(ControlX, drawArea.w, CurrY, 26), IDGen);
    }
    AdvanceYLastControl(CurrY);

    // Hip bone
    LabelAt(HipLabel, LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);
    if (TwoBone->GetBoundKneeIndex() != -1)
    {
        // Active link specs.  We need to reset the bone names array to only include
        // parents of the head bone...
        BoneNames.resize(0);
        BoneIndices.resize(0);

        int Walk = Model->Skeleton->Bones[TwoBone->GetBoundKneeIndex()].ParentIndex;
        for (; Walk != GrannyNoParentBone; Walk = Model->Skeleton->Bones[Walk].ParentIndex)
        {
            BoneNames.insert(BoneNames.begin(), Model->Skeleton->Bones[Walk].Name);
            BoneIndices.insert(BoneIndices.begin(), Walk);
        }

        int CurrentEntry = -1;
        {for (size_t Idx = 0; Idx < BoneIndices.size(); ++Idx)
        {
            if (TwoBone->GetBoundHipIndex() == BoneIndices[Idx])
            {
                CurrentEntry = Idx;
                break;
            }
        }}

        if (ITunesComboboxOn(BoneNames, CurrentEntry,
                             FillRectToRight(ControlX, drawArea.w, CurrY, 26),
                             IDGen))
        {
            PushUndoPos("Set hip joint");

            TwoBone->SetHipName(BoneNames[CurrentEntry]);
        }
    }
    else
    {
        ComboboxPlaceholder(0, FillRectToRight(ControlX, drawArea.w, CurrY, 26), IDGen);
    }
    AdvanceYLastControl(CurrY);

    // Ground Normal
    LabelAt(KneePlaneLabel, LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

    int Axis = TwoBone->GetKneePlane();
    if (AxisSelector(Axis, FillRectToRight(ControlX, drawArea.w, CurrY, 26), true, IDGen))
    {
        PushUndoPos("Change knee plane");
        TwoBone->SetKneePlane(EAimAxis(Axis));
    }
    AdvanceYLastControl(CurrY);

    LastY = CurrY;
}

void
blendnode_edit_visitor::VisitNode(aimat_ik* AimAt)
{
    Assert(AimAt);

    LuaRect drawArea = UIAreaGet();

    ControlIDGen IDGen("aimat_%x_%d_%d_%d",
                       edit::TokenizedToID(AimAt),
                       AimAt->GetBoundHeadIndex(),
                       AimAt->GetBoundLinkCount(),
                       AimAt->GetBoundInactiveLinkCount());

    int32x LabelX, ControlX;
    LabeledControlsAtWidth(LabelWidth("Ground Normal:"), &LabelX, &ControlX);

    // Allow the aim point to move one large grid cube around the character.
    real32 CubeDimension = GetGridSpacing() * 10;

    float AimPosition[3];
    AimAt->GetDefaultAimPosition(AimPosition);

    int CurrY = VPadding();
    char const* Labels[3] = { "X:", "Y:", "Z:" };

    {for (int Idx = 0; Idx < 3; ++Idx)
    {
        LabelAt(Labels[Idx], LuaPoint(LabelX, ComboLabelAdjust(CurrY)), eRight);

        if (SliderOn(-CubeDimension, CubeDimension, AimPosition[Idx], false,
                     FillRectToRight(ControlX, drawArea.w, CurrY, 26), IDGen))
        {
            PushUndoPos("aimat option", false, eSliderMotion, AimAt->GetUID());
            AimAt->SetDefaultAimPosition(AimPosition);
        }
        AdvanceYLastControl(CurrY);

        char buffer[32];
        sprintf(buffer, "%.2f", AimPosition[Idx]);
        std::string Str = buffer;
        if (EditLabelOn(Str, LuaRect(ControlX, CurrY, 75, 24), IDGen))
        {
            PushUndoPos("Animation option");

            sscanf(Str.c_str(), "%f", &AimPosition[Idx]);
            AimAt->SetDefaultAimPosition(AimPosition);
        }
        AdvanceYLastControl(CurrY);
    }}

    // Spacer
    AdvanceY(CurrY, 10);

    // Axis
    {
        // Aim axis
        ColorLabelAt("Aim Axis:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), LuaColor(1, 1, 0), eRight);

        int Axis = AimAt->GetAimAxis();
        if (AxisSelector(Axis, FillRectToRight(ControlX, drawArea.w, CurrY, 26), true, IDGen))
        {
            PushUndoPos("Change aim axis");
            AimAt->SetAimAxis(EAimAxis(Axis));
        }
        AdvanceYLastControl(CurrY);

        // Ear axis
        ColorLabelAt("Ear Axis:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), LuaColor(0, 1, 1), eRight);

        Axis = AimAt->GetEarAxis();
        if (AxisSelector(Axis, FillRectToRight(ControlX, drawArea.w, CurrY, 26), true, IDGen))
        {
            PushUndoPos("Change ear axis");
            AimAt->SetEarAxis(EAimAxis(Axis));
        }
        AdvanceYLastControl(CurrY);

        // Ground Normal
        ColorLabelAt("Ground Normal:", LuaPoint(LabelX, ComboLabelAdjust(CurrY)), LuaColor(1, 0, 1), eRight);

        Axis = AimAt->GetGroundNormal();
        if (AxisSelector(Axis, FillRectToRight(ControlX, drawArea.w, CurrY, 26), true, IDGen))
        {
            PushUndoPos("Change ground normal");
            AimAt->SetGroundNormal(EAimAxis(Axis));
        }
        AdvanceYLastControl(CurrY);
    }

    // Don't display this bit unless the model exists
    if (Model == 0)
    {
        LastY = CurrY;
        return;
    }

    // Spacer
    AdvanceY(CurrY, 10);

    vector<char const*> BoneNames(Model->Skeleton->BoneCount);
    {for (int i = 0; i < Model->Skeleton->BoneCount; ++i)
    {
        BoneNames[i] = Model->Skeleton->Bones[i].Name;
    }}

    // "Head" Bone
    {
        LabelAt("End Effector:", HPadPtAtY(ComboLabelAdjust(CurrY)));

        int CurrentEntry = AimAt->GetBoundHeadIndex();
        if (ITunesComboboxOn(BoneNames, CurrentEntry, FillRectToRight(ControlX, drawArea.w, CurrY, 26), IDGen))
        {
            PushUndoPos("Change aim ee");
            AimAt->SetHeadName(BoneNames[CurrentEntry]);
            AimAt->SetFirstActiveName(BoneNames[CurrentEntry]);
            AimAt->SetLastActiveName(BoneNames[0]);
        }
        AdvanceYLastControl(CurrY);
    }

    if (AimAt->GetBoundHeadIndex() != -1)
    {
        // Active link specs.  We need to reset the bone names array to only include
        // parents of the head bone...
        BoneNames.resize(0);
        for (int Walk = AimAt->GetBoundHeadIndex();
             Walk != GrannyNoParentBone;
             Walk = Model->Skeleton->Bones[Walk].ParentIndex)
        {
            BoneNames.insert(BoneNames.begin(), Model->Skeleton->Bones[Walk].Name);
        }

        // First active
        {
            LabelAt("First Active:", HPadPtAtY(ComboLabelAdjust(CurrY)));

            int CurrentEntry = AimAt->GetBoundInactiveLinkCount();
            if (CurrentEntry != -1)
                CurrentEntry = BoneNames.size() - CurrentEntry - 1;

            if (ITunesComboboxOn(BoneNames, CurrentEntry, FillRectToRight(ControlX, drawArea.w, CurrY, 26), IDGen))
            {
                PushUndoPos("Change inactive links");
                AimAt->SetFirstActiveName(BoneNames[CurrentEntry]);
                AimAt->SetLastActiveName(BoneNames[CurrentEntry]);
            }
            AdvanceYLastControl(CurrY);
        }

        // Last Active.  Note that we shave off the inactive links...
        {
            LabelAt("Last Active:", HPadPtAtY(ComboLabelAdjust(CurrY)));

            int CurrentEntry = AimAt->GetBoundLinkCount();
            if (CurrentEntry != -1)
                CurrentEntry = BoneNames.size() - CurrentEntry - 1;

            int Inactive = AimAt->GetBoundInactiveLinkCount();
            if (Inactive != -1)
            {
                BoneNames.resize(BoneNames.size() - Inactive);
                Assert(CurrentEntry == -1 || (CurrentEntry >= 0 && CurrentEntry < (int)BoneNames.size()));
            }

            if (ITunesComboboxOn(BoneNames, CurrentEntry, FillRectToRight(ControlX, drawArea.w, CurrY, 26), IDGen))
            {
                PushUndoPos("Change last active link");
                AimAt->SetLastActiveName(BoneNames[CurrentEntry]);
            }
            AdvanceYLastControl(CurrY);
        }
    }

    LastY = CurrY;
}

void
blendnode_edit_visitor::VisitNode(mirror* Mirror)
{
    Assert(Mirror);

    LuaRect drawArea = UIAreaGet();

    // We want this to create all new items if the number of outputs change...
    ControlIDGen IDGen("mirror_edit_box_%x_%d",
                       edit::TokenizedToID(Mirror),
                       Mirror->GetNameSwapCount());

    // Compute the control offset...
    int ControlX = 100;

    int CurrY = VPadding();

    // Select axis to mirror
    {
        const char* MirrorLabel = "Axis To Mirror:";
        LabelAt(MirrorLabel, HPadPtAtY(ComboLabelAdjust(CurrY)));

        int CurrentEntry = Mirror->GetMirrorAxis();
        Assert(CurrentEntry >= 0 && CurrentEntry < 3);

        vector<char const*> AxisNames(3);
        AxisNames[0] = "X Axis";
        AxisNames[1] = "Y Axis";
        AxisNames[2] = "Z Axis";
        if (ComboboxOn(AxisNames, CurrentEntry, FillRectToRight(ControlX, drawArea.w, CurrY, 26), IDGen))
        {
            Assert(CurrentEntry >= 0 && CurrentEntry < 3);

            PushUndoPos("Change mirror axis");
            Mirror->SetMirrorAxis((granny_mirror_axis)CurrentEntry);
        }
        AdvanceYLastControl(CurrY);
    }

    AdvanceY(CurrY);
    LabelAt("Name Swap Pairs", HPadPtAtY(ComboLabelAdjust(CurrY)));
    AdvanceYLastControl(CurrY);

    // Name pairs
    int SwapCount = Mirror->GetNameSwapCount();
    Assert((SwapCount & 1) == 0);
    {
        {for (int Idx = 0; Idx < SwapCount; Idx += 2)
        {
            // Removeable button
            {
                PushTokenizedIntClosure(Mirror, DeleteSwapPair, Idx);
                SmallButtonAtWithCallback("(remove)", HPadPtAtY(CurrY + 14), true, IDGen);
            }

            char const* LeftSwap = Mirror->GetNameSwap(Idx + 0);
            char const* RightSwap = Mirror->GetNameSwap(Idx + 1);

            std::string LeftStr = LeftSwap;
            if (EditLabelOn(LeftStr, FillRectToRight(ControlX, drawArea.w, CurrY, 24), IDGen))
            {
                PushUndoPos("Swap change");

                Mirror->SetNameSwap(Idx + 0, LeftStr.c_str());
            }
            AdvanceYLastControl(CurrY);

            std::string RightStr = RightSwap;
            if (EditLabelOn(RightStr, FillRectToRight(ControlX, drawArea.w, CurrY, 24), IDGen))
            {
                PushUndoPos("Swap change");

                Mirror->SetNameSwap(Idx + 1, RightStr.c_str());
            }
            AdvanceYLastControl(CurrY);
            AdvanceY(CurrY);
        }}
    }

    // Button to add a new pair...
    AdvanceY(CurrY);
    if (ButtonAt("Add Swap Pair", HPadPtAtY(CurrY), true, IDGen))
    {
        PushUndoPos("Add swap pair");

        vector<char const*> Swaps(Mirror->GetNameSwapCount() + 2);
        {for (int Idx = 0; Idx < Mirror->GetNameSwapCount(); ++Idx)
        {
            Swaps[Idx] = Mirror->GetNameSwap(Idx);
        }}
        Swaps[Mirror->GetNameSwapCount() + 0] = "";
        Swaps[Mirror->GetNameSwapCount() + 1] = "";

        Mirror->SetNameSwaps(Swaps.size(), &Swaps[0]);
    }
    AdvanceYLastControl(CurrY);

    LastY = CurrY;
}

void
blendnode_edit_visitor::VisitNode(event_source* Source)
{
    LastY = SharedEventSourceEdit(Source);
}

void
blendnode_edit_visitor::VisitNode(event_renamer* Renamer)
{
    Assert(Renamer);
    LuaRect drawArea = UIAreaGet();


    // We want this to create all new items if the number of outputs change...
    ControlIDGen IDGen("event_renamer_edit_box_%x_%d",
                       edit::TokenizedToID(Renamer),
                       Renamer->NumRenames());

    // Compute the control offset...
    int ControlX = 125;
    int CurrY    = VPadding();

    int32x NumEvents = 0;
    vector<granny_text_track_entry> Entries(128);

    // Note that we are *bypassing* this call on the renamer, since we want the raw
    // strings, not the renamed ones...
    {
        node* Connected;
        int32x Edge;
        Renamer->GetInputConnection(0, &Connected, &Edge);

        // todo: can fail, retry with larger array?
        if (Connected != 0)
            Connected->GetAllEvents(Edge, &Entries[0], Entries.size(), &NumEvents);
    }

    vector<char const*> EventNames;
    for (int Idx = 0; Idx < NumEvents; ++Idx)
        EventNames.push_back(Entries[Idx].Text);

    {for (int Idx = 0; Idx < Renamer->NumRenames(); ++Idx)
    {
        LuaRect Rect = FillRectToRight(HPadding(), ControlX, CurrY, 26);
        if (EventNames.empty())
        {
            ComboboxPlaceholder(0, Rect, IDGen);
        }
        else
        {
            char const* Input = Renamer->GetRenameInput(Idx);

            // Find the current entry in the list, or -1 if it's not present...
            int32x Selection = -1;
            if (Input)
            {
                for (size_t Entry = 0; Entry < EventNames.size(); ++Entry)
                {
                    if (strcmp(Input, EventNames[Entry]) == 0)
                    {
                        Selection = Entry;
                        break;
                    }
                }
            }

            if (ComboboxOn(EventNames, Selection, Rect, IDGen))
            {
                PushUndoPos("Rename input change");

                char const* Text = 0;
                if (Selection != -1)
                    Text = EventNames[Selection];

                Renamer->SetRename(Idx, Text, Renamer->GetRenameOutput(Idx));
            }
        }

        int DelWidth = SmallButtonWidth("(delete)");

        std::string Str = Renamer->GetRenameOutput(Idx);
        LuaRect EditRect = FillRectToRight(ControlX, drawArea.w - DelWidth - 2*HPadding(), CurrY, 24);

        PushTokenizedIntClosure(Renamer, RemoveRenamer, Idx);
        SmallButtonAtWithCallback("(delete)", LuaPoint(EditRect.x+EditRect.w+HPadding(),CurrY), true, IDGen);

        if (EditLabelOn(Str, EditRect, IDGen))
        {
            PushUndoPos("Rename output change");
            Renamer->SetRename(Idx, Renamer->GetRenameInput(Idx), Str.c_str());
        }

        AdvanceYLastControl(CurrY);
        AdvanceY(CurrY);
    }}

    // Button to add a new pair...
    AdvanceY(CurrY);
    if (ButtonAt("Add Possible Event", HPadPtAtY(CurrY), true, IDGen))
    {
        PushUndoPos("Add possible event");
        Renamer->AddRename(0, "");
    }
    AdvanceYLastControl(CurrY);

    LastY = CurrY;
}

// node
int GRANNY
Edit_BlendNodeEdit(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    node* Node = edit::IDToNode(NodeID);

    blendnode_edit_visitor Visitor(L);
    Visitor.LastY = lua_tointeger(L, -1);
    Node->AcceptNodeVisitor(&Visitor);

    // @@ fixme
    LuaRect used(0, 0, 1, 1);
    if (Visitor.LastY != -1)
    {
        used.h = Visitor.LastY;
    }

    PushRect(L, used);
    lua_pushboolean(L, true);

    return 2;
}

int GRANNY
Edit_StateNodeEdit(lua_State* L)
{
    int32x NodeID = (int32x)lua_tointeger(L, -2);
    node* Node = edit::IDToNode(NodeID);

    statenode_edit_visitor Visitor(L);
    Visitor.LastY = lua_tointeger(L, -1);
    Node->AcceptNodeVisitor(&Visitor);

    // @@ fixme
    LuaRect used(0, 0, 1, 1);
    if (Visitor.LastY != -1)
    {
        used.h = Visitor.LastY;
    }

    PushRect(L, used);
    lua_pushboolean(L, true);

    return 2;
}

