require "ui_base"


function BogusNodeButtons(ID, Bitmaps)
   local RootIsStateMachine = not Edit_CurrentContainerIsBlendgraph()

   DrawRect(UIArea.Get(), UIPref.GetColor("control_bg"))
   local CurrX = 4
   for i,bm in ipairs(Bitmaps) do
      local bitmap   = bm.bm
      local disabled = false
      if (RootIsStateMachine and not bm.valid_in_statemachine) then
         bitmap = bm.bm_disabled
         disabled = true
      end

      if (Edit_InScrubMode()) then
         disabled = true
      end

      local DragType = CreateNodeDragType
      if (bm.valid_as_state) then
         DragType = CreateStateDragType
      end

      local bw, bh = Button.LabeledBitmapDim(bitmap, bm.label, 48, true)
      Button.DoLabeledBitmapDragger(IDItem.CreateSub(ID, bm.label),
                                    MakeRect(CurrX, 4, bw, bh),
                                    bitmap, bm.label, true, disabled, bm.createfn,
                                    bm.dragcreatefn, DragType)
      CurrX = CurrX + 8 + bw
   end
end

local function MakeBogusButton(ibm, ibm_disabled, ilabel, inode_type, idefault_name, ivalid_in_statemachine, ivalid_as_state)
   return { bm = ibm,
            bm_disabled = ibm_disabled,
            label = ilabel,
            createfn =
               function()
                  Edit_AddNode(inode_type, idefault_name)
               end,
            dragcreatefn =
               function(Val, Pt)
                  if (Val == 0) then
                     Edit_AddNodeAtPt(inode_type, idefault_name, Pt)
                  else
                     -- only replace with a new type...
                     if (Edit_GetNodeType(Val) ~= inode_type) then
                        Edit_ReplaceNodeWith(Val, inode_type)
                     end
                  end
               end,
            valid_in_statemachine = ivalid_in_statemachine,
            valid_as_state = ivalid_as_state }
end

local function MakeCommentButton(ibm, ilabel)
   return { bm = ibm,
            bm_disabled = nil,
            label = ilabel,
            createfn = function() Edit_AddComment("Comment", nil) end,
            dragcreatefn =
               function(Val, Pt)
                  Edit_AddComment("Comment", Pt)
               end,
            valid_in_statemachine = true,
            valid_as_state = false }
end

local function MakeStateOKButton(ibm, ilabel, inode_type, idefault_name, valid_state)
   return MakeBogusButton(ibm, nil, ilabel, inode_type, idefault_name, true, valid_state)
end

local function MakeBlendOnlyButton(ibm, ibm_disabled, ilabel, inode_type, idefault_name)
   return MakeBogusButton(ibm, ibm_disabled, ilabel, inode_type, idefault_name, false, false)
end

local function AddButton(Table, Button)
   table.insert(Table, Button)
end

local TopLevelButtons  = { }
local BlendButtons     = { }
local TrackMaskButtons = { }
local ModifierButtons  = { }
local EventButtons     = { }
local CustomButtons    = { }

-- =============================================================================
-- =============================================================================
-- STATES
AddButton(TopLevelButtons,
          MakeStateOKButton(UIPref.GetBitmap("icon_state_machine"),
                            "State Machine", "state_machine", "State Machine", true))
AddButton(TopLevelButtons,
          MakeStateOKButton(UIPref.GetBitmap("icon_blend_graph"),
                            "Blend Graph", "blend_graph", "Blend Graph", true))
AddButton(TopLevelButtons,
          MakeStateOKButton(UIPref.GetBitmap("icon_anim_source"),
                            "Animation", "anim_source", "Animation", true))
AddButton(TopLevelButtons,
          MakeStateOKButton(UIPref.GetBitmap("icon_parameters"),
                            "Parameters", "parameters", "Parameters", false))
AddButton(TopLevelButtons,
          MakeStateOKButton(UIPref.GetBitmap("icon_event_source"),
                            "Event Source", "event_source", "Event Source", false))
AddButton(TopLevelButtons,
          MakeCommentButton(UIPref.GetBitmap("icon_comment"), "Add Comment", false))

-- =============================================================================
-- =============================================================================
-- BLENDS
AddButton(BlendButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_blend"),
                                            UIPref.GetBitmap("icon_blend_disabled"),
                                            "Blend", "blend", "Blend"))
AddButton(BlendButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_nway_blend"),
                                            UIPref.GetBitmap("icon_nway_blend_disabled"),
                                            "NWay Blend", "nway_blend", "NWay Blend"))
AddButton(BlendButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_additive_blend"),
                                            UIPref.GetBitmap("icon_additive_blend_disabled"),
                                            "Add. Blend", "additive_blend", "Additive blend"))
AddButton(BlendButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_masked_combine"),
                                            UIPref.GetBitmap("icon_masked_combine_disabled"),
                                            "Masked Combine", "masked_combine", "Masked Combine"))


-- =============================================================================
-- =============================================================================
-- TRACK MASKS
AddButton(TrackMaskButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_mask_source"),
                                                UIPref.GetBitmap("icon_mask_source_disabled"),
                                                "Mask source", "mask_source", "Mask Source"))
AddButton(TrackMaskButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_mask_invert"),
                                                UIPref.GetBitmap("icon_mask_invert_disabled"),
                                                "Mask invert", "mask_invert", "Mask Invert"))
AddButton(TrackMaskButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_mask_union"),
                                                UIPref.GetBitmap("icon_mask_union_disabled"),
                                                "Mask union", "mask_union", "Mask Union"))

-- =============================================================================
-- =============================================================================
-- Modifiers
AddButton(ModifierButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_selection"),
                                               UIPref.GetBitmap("icon_selection_disabled"),
                                               "Constant Velocity", "constant_velocity", "Constant Velocity"))
AddButton(ModifierButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_selection"),
                                               UIPref.GetBitmap("icon_selection_disabled"),
                                               "Mirror", "mirror", "Mirror"))
AddButton(ModifierButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_timeshift"),
                                               UIPref.GetBitmap("icon_timeshift_disabled"),
                                               "Timeshift", "timeshift", "Timeshift"))
AddButton(ModifierButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_timeselect"),
                                               UIPref.GetBitmap("icon_timeselect_disabled"),
                                               "Timeselect", "timeselect", "Timeselect"))
AddButton(ModifierButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_selection"),
                                               UIPref.GetBitmap("icon_selection_disabled"),
                                               "LOD Protect", "lod_protect", "LOD Protect"))

-- =============================================================================
-- =============================================================================
-- Event Buttons
-- AddButton(EventButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_selection"),
--                                             UIPref.GetBitmap("icon_selection_disabled"),
--                                             "Event Adder", "event_adder", "Event Adder"))
AddButton(EventButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_event_mixer"),
                                            UIPref.GetBitmap("icon_event_mixer_disabled"),
                                            "Event Mixer", "event_mixer", "Event Mixer"))
AddButton(EventButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_event_renamer"),
                                            UIPref.GetBitmap("icon_event_renamer_disabled"),
                                            "Event Renamer", "event_renamer", "Event Renamer"))

-- =============================================================================
-- =============================================================================
-- CUSTOM BUTTONS
AddButton(CustomButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_selection"),
                                             UIPref.GetBitmap("icon_selection_disabled"),
                                             "2-Bone IK", "twobone_ik", "TwoBone IK"))
AddButton(CustomButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_selection"),
                                             UIPref.GetBitmap("icon_selection_disabled"),
                                             "AimAt IK", "aimat_ik", "AimAt IK"))
AddButton(CustomButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_selection"),
                                             UIPref.GetBitmap("icon_selection_disabled"),
                                             "Selection", "selection", "Selection"))
AddButton(CustomButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_pose_storage"),
                                             UIPref.GetBitmap("icon_pose_storage"),
                                             "Pose Storage", "pose_storage", "Pose Storage"))
AddButton(CustomButtons, MakeBlendOnlyButton(UIPref.GetBitmap("icon_rest_pose"),
                                             UIPref.GetBitmap("icon_rest_pose"),
                                             "Rest Pose", "rest_pose", "Rest Pose"))


local UITabs = {
   { Label = "Top level",       DrawFn = function (ID) BogusNodeButtons(ID, TopLevelButtons)  end },
   { Label = "Blends",          DrawFn = function (ID) BogusNodeButtons(ID, BlendButtons)     end },
   { Label = "Masking",         DrawFn = function (ID) BogusNodeButtons(ID, TrackMaskButtons) end },
   { Label = "Modifiers",       DrawFn = function (ID) BogusNodeButtons(ID, ModifierButtons)  end },
   { Label = "Events",          DrawFn = function (ID) BogusNodeButtons(ID, EventButtons)     end },
   { Label = "Misc and Custom", DrawFn = function (ID) BogusNodeButtons(ID, CustomButtons)    end },
}
CurrentToolTab = 1

function NodeCreateBar(ID)
   CurrentToolTab = Tabview.Do(IDItem.CreateSub(ID, "tab"), UITabs, CurrentToolTab, false)
end

