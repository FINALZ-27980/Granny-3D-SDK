require "color"
require "ui_font"

local BaseBevel = MakeColor(1, 1, 1)
local BaseField = MakeColor(0.85, 0.8, 0.75)

local BaseLabelText     = MakeColor(0.329, 0.317, 0.345)
local BaseLabelTextDrop = MakeColor(1, 1, 1)

local DarkLabelText         = MakeColor(1, 1, 1)
local DarkLabelDisabledText = MakeColor(0.7, 0.7, 0.7)
local DarkLabelTextDrop     = MakeColor(0, 0, 0)

PreferenceColors = {
   [ "background" ] = MakeColor(0.2, 0.2, 0.25),

   [ "comment_text_low" ]  = MakeColor(0.15, 0.5, 0.15),
   [ "comment_text" ]      = MakeColor(0.25, 0.75, 0.25),
   [ "comment_box" ]       = MakeColorA(0.75, 0.75, 0.15, 0.5),
   [ "comment_box_alpha" ] = MakeColorA(0.75, 0.75, 0.15, 0.15),

   -- Label
   [ "label_text" ]      = DarkLabelText,
   [ "label_text_drop" ] = DarkLabelTextDrop,

   -- Buttons
   [ "button_text" ]       = DarkLabelText,
   [ "button_text_drop" ]  = DarkLabelTextDrop,

   [ "button_disabled_text" ]       = BaseLabelText,
   [ "button_disabled_text_drop" ]  = DarkLabelTextDrop,

   [ "button_bevel" ] = BaseBevel,
   [ "button" ]       = BaseField,

   [ "bmbutton_outline" ] = MakeColor(0, 0, 1),
   [ "bmbutton_alpha" ]   = MakeColorA(0, 0, 1, 0.5),

   [ "lbmbutton_text_below" ] = MakeColor(1, 1, 1),
   [ "lbmbutton_text_side" ]  = MakeColor(0, 0, 0),

   -- Combobox
   [ "combo_highlighttext" ] = MakeColor(1, 1, 1),
   [ "combo_highlight" ]     = MakeColor(15/255, 54/255, 150/255),

   [ "combo_text" ]             = DarkLabelText,
   [ "combo_text_placeholder" ] = DarkLabelDisabledText,
   [ "combo_text_drop" ]        = DarkLabelTextDrop,

   -- Scrollbar
   [ "scroll_stipple0" ] = MakeColor(0.55, 0.5, 0.5),
   [ "scroll_stipple1" ] = MakeColor(0.95, 0.9, 0.85),
   [ "scroll_bevel" ]    = BaseBevel,
   [ "scroll_shuttle" ]  = BaseField,


   -- Node
   [ "blend_node_base" ]   = MakeColor(0.8, 0.8, 0.8),
   [ "blend_node_drop" ]   = MakeColorA(0, 0, 0, 0.4),
   [ "blend_node_select" ] = MakeColor(1, 1, 1),

   [ "state_text" ]       = MakeColor(0, 0, 0),
   [ "state_text_drop" ]  = MakeColor(0.9, 0.9, 0.9),
   [ "state_node_base" ]     = MakeColor(0.4, 0.60, 0.75),
   [ "state_node_selected" ] = MakeColor(1.3 * 0.4, 1.3 * 0.60, 1.3 * 0.75),

   -- [ "node_title_text" ]       = MakeColor(0.15, 0.15, 0.15),
   [ "node_title_text" ]       = MakeColor(0, 0, 0),
   [ "node_title_text_drop" ]  = MakeColor(0.9, 0.9, 0.9),
   [ "node_edge_text" ]        = MakeColor(0.85, 0.85, 0.85),
   [ "node_edge_text_drop" ]   = MakeColor(0.2, 0.2, 0.2),

   -- Connection colors
   [ "node_edge_pose" ]   = MakeColor(0.18, 0.607, 0.18),
   [ "node_edge_scalar" ] = MakeColor(0.51, 0.51, 0.77),
   [ "node_edge_mask" ]   = MakeColor(0.615, 0.282, 0.294),
   [ "node_edge_event" ]  = MakeColor(0.808, 0.624, 0.1),
   [ "node_edge_morph" ]   = MakeColor(0.25, 0.525, 0.53),
   [ "node_edge_pose_hl" ]   = MakeColor(0.18  * 1.3, 0.607 * 1.3, 0.18  * 1.3),
   [ "node_edge_scalar_hl" ] = MakeColor(0.51  * 1.3, 0.51  * 1.3, 0.77  * 1.3),
   [ "node_edge_mask_hl" ]   = MakeColor(0.615 * 1.3, 0.282 * 1.3, 0.294 * 1.3),
   [ "node_edge_event_hl" ]  = MakeColor(1          , 0.624 * 1.3, 0.1   * 1.3),
   [ "node_edge_morph_hl" ]  = MakeColor(0.25*1.3, 0.525*1.3, 0.53*1.3),

   [ "tr_onrequest_color" ]      = MakeColor(0.85, 0.85, 0.85),
   [ "tr_onloop_color" ]         = MakeColor(0.50, 0.85, 0.85),
   [ "tr_onsubloop_color" ]      = MakeColor(0.85, 0.50, 0.85),
   [ "tr_lastresort_color" ]     = MakeColor(0.85, 0.85, 0.50),
   [ "tr_onconditional_color" ]  = MakeColor(0.85, 0.50, 0.50),

   -- Backgrounds
   [ "render_bg" ]  = MakeColor(0.2, 0.2, 0.23),
   [ "control_bg" ] = MakeColor(0.26, 0.26, 0.26),
   [ "toolbar_bg" ] = MakeColor(0.2, 0.2, 0.2),

   [ "section_header" ] = MakeColor(0.3, 0.3, 0.3),

   [ "tabview_bg" ] = MakeColor(0.2, 0.2, 0.2),
   [ "tabview_border" ] = MakeColor(0.098, 0.098, 0.098),
   [ "tabview_text" ]      = DarkLabelText,
   [ "tabview_text_drop" ] = DarkLabelTextDrop,

   [ "border" ] = MakeColor(0.098, 0.098, 0.098),

   -- Editbox
   ["editbox_text"]          = MakeColor(1, 1, 1),
   ["editbox_text_disabled"] = MakeColor(0.5, 0.5, 0.5),
   ["editbox_hl_text"]       = MakeColor(0.66, 0.66, 0.66),
   ["editbox_hl_text_drop"]  = MakeColor(0, 0, 0),

   -- Helper
   ["inaction_indicator0"] = MakeColorA(0, 0, 0, 0.25),
   ["inaction_indicator1"] = MakeColorA(0, 0, 0, 0.75),

   -- Helper
   ["inaction_indicator_strong0"] = MakeColorA(0, 0, 0, 0.5),
   ["inaction_indicator_strong1"] = MakeColorA(0, 0, 0, 0.75),

   -- Motion controls
   ["motion_control_text_closed"] = MakeColor(0.5, 0.5, 0.5),
   ["motion_control_text_open"]   = MakeColor(1, 1, 1),

   [ "slot_header_text" ]      = DarkLabelText,
   [ "slot_header_text_drop" ] = DarkLabelTextDrop,
}


local SansSerifName = "Verdana"
local UIFontName    = "Trebuchet"
local FixedName     = "Consolas"

PreferenceFonts = {
   -- UIFont.Get(FontName, Points, Bold, Italic)

   [ "label_text" ]           = UIFont.Get(UIFontName,    9,  true,  false),
   [ "small_label_text" ]     = UIFont.Get(UIFontName,    8,  true, false),
   [ "comment_text" ]         = UIFont.Get(FixedName,     9,  true,  false),

   [ "node_title_text" ]      = UIFont.Get(SansSerifName, 18, true,  false),

   [ "button" ]               = UIFont.Get(UIFontName,    11, true,  false),
   [ "small_button" ]         = UIFont.Get(UIFontName,    8,  true,  false),

   [ "lbmbutton_text_below" ] = UIFont.Get(SansSerifName, 10, false, false),
   [ "lbmbutton_text_side" ]  = UIFont.Get(SansSerifName, 10, false, false),
   [ "tabview_tab" ]          = UIFont.Get(UIFontName,    9,  true,  false),
   [ "combobox" ]             = UIFont.Get(UIFontName,    11, true,  false),
   [ "combobox_small" ]       = UIFont.Get(UIFontName,    8,  true,  false),

   [ "node_title" ]           = UIFont.Get(SansSerifName, 9,  false, false),
   [ "node_edge" ]            = UIFont.Get(UIFontName,    8,  true,  false),

   [ "editbox" ]              = UIFont.Get(SansSerifName, 11, false, false),

   [ "tooltip" ]              = UIFont.Get(SansSerifName, 10, false, false),

   [ "table" ]                = UIFont.Get(FixedName,     10, false, false),
   [ "console" ]              = UIFont.Get(FixedName,     10, false, false),
   [ "log_strings" ]          = UIFont.Get(FixedName,     12, false, false),

   [ "anim_list_file" ]       = UIFont.Get(FixedName,     12, false, false),
   [ "anim_list_anim" ]       = UIFont.Get(FixedName,     10, false, false),

   [ "slot_list_header" ]     = UIFont.Get(UIFontName,    14, false, false),
   [ "slot_list_entry" ]      = UIFont.Get(UIFontName,    11, false, false),
}


PreferenceBitmaps = {
   [ "icon_blend_graph" ]          = UIBitmap_GetHandle("blendGraph"),
   [ "icon_blend_graph_small" ]    = UIBitmap_GetHandle("blendGraph_small"),

   [ "icon_comment" ]         = UIBitmap_GetHandle("comment"),

   [ "icon_anim_source"  ]         = UIBitmap_GetHandle("animation"),
   [ "icon_anim_source_small"  ]   = UIBitmap_GetHandle("animation_small"),

   [ "icon_sequence"  ]            = UIBitmap_GetHandle("animation"),
   [ "icon_sequence_small"  ]      = UIBitmap_GetHandle("animation_small"),

   [ "icon_blend"  ]         = UIBitmap_GetHandle("blend"),
   [ "icon_blend_disabled" ] = UIBitmap_GetHandle("blend_disabled"),
   [ "icon_blend_small"  ]   = UIBitmap_GetHandle("blend_small"),

   [ "icon_sequencer"  ]         = UIBitmap_GetHandle("sequencer_48"),
   [ "icon_sequencer_disabled" ] = UIBitmap_GetHandle("sequencer_48D"),
   [ "icon_sequencer_small"  ]   = UIBitmap_GetHandle("sequencer_24"),

   [ "icon_additive_blend"  ]         = UIBitmap_GetHandle("additive_blend_48"),
   [ "icon_additive_blend_disabled" ] = UIBitmap_GetHandle("additive_blend_48_disabled"),
   [ "icon_additive_blend_small"  ]   = UIBitmap_GetHandle("additive_blend_24"),

   [ "icon_masked_combine"  ]         = UIBitmap_GetHandle("masked_combine_48"),
   [ "icon_masked_combine_disabled" ] = UIBitmap_GetHandle("masked_combine_disabled_48"),
   [ "icon_masked_combine_small"  ]   = UIBitmap_GetHandle("masked_combine_24"),

   [ "icon_mask_source"  ]          = UIBitmap_GetHandle("mask_source_48"),
   [ "icon_mask_source_disabled"  ] = UIBitmap_GetHandle("mask_source_disabled_48"),
   [ "icon_mask_source_small"  ]    = UIBitmap_GetHandle("mask_source_24"),

   [ "icon_mask_invert"  ]         = UIBitmap_GetHandle("mask_invert"),
   [ "icon_mask_invert_disabled" ] = UIBitmap_GetHandle("mask_invert_disabled"),
   [ "icon_mask_invert_small"  ]   = UIBitmap_GetHandle("mask_invert_small"),

   [ "icon_mask_union"  ]         = UIBitmap_GetHandle("mask_union"),
   [ "icon_mask_union_disabled" ] = UIBitmap_GetHandle("mask_union_disabled"),
   [ "icon_mask_union_small"  ]   = UIBitmap_GetHandle("mask_union_small"),

   [ "icon_masked_blend"  ]       = UIBitmap_GetHandle("masked_blend"),
   [ "icon_masked_blend_small"  ] = UIBitmap_GetHandle("masked_blend_small"),

   [ "icon_nway_blend" ]          = UIBitmap_GetHandle("nway_blend"),
   [ "icon_nway_blend_disabled" ] = UIBitmap_GetHandle("nway_blend_disabled"),
   [ "icon_nway_blend_small" ]    = UIBitmap_GetHandle("nway_blend_small"),

   [ "icon_parameters"  ]         = UIBitmap_GetHandle("parameters"),
   [ "icon_parameters_disabled" ] = UIBitmap_GetHandle("parameters_disabled"),
   [ "icon_parameters_small"  ]   = UIBitmap_GetHandle("parameters_small"),

   [ "icon_event_source"  ]         = UIBitmap_GetHandle("event_source"),
   [ "icon_event_source_small"  ]   = UIBitmap_GetHandle("event_source_24"),

   [ "icon_timeshift"  ]         = UIBitmap_GetHandle("timeshift_48"),
   [ "icon_timeshift_disabled" ] = UIBitmap_GetHandle("timeshift_disabled_48"),
   [ "icon_timeshift_small"  ]   = UIBitmap_GetHandle("timeshift_24"),

   [ "icon_timeselect"  ]         = UIBitmap_GetHandle("timeselect_48"),
   [ "icon_timeselect_disabled" ] = UIBitmap_GetHandle("timeselect_disabled_48"),
   [ "icon_timeselect_small"  ]   = UIBitmap_GetHandle("timeselect_24"),

   [ "icon_selection"  ]         = UIBitmap_GetHandle("selectionD_48"),
   [ "icon_selection_disabled" ] = UIBitmap_GetHandle("selectionD_48_disabled"),
   [ "icon_selection_small"  ]   = UIBitmap_GetHandle("selectionD_24"),

   [ "icon_pose_storage"  ]         = UIBitmap_GetHandle("icon_pose_storage"),
   [ "icon_pose_storage_disabled" ] = UIBitmap_GetHandle("icon_pose_storage"),
   [ "icon_pose_storage_small"  ]   = UIBitmap_GetHandle("icon_pose_storage_24"),

   [ "icon_rest_pose"  ]         = UIBitmap_GetHandle("icon_rest_pose"),
   [ "icon_rest_pose_disabled" ] = UIBitmap_GetHandle("icon_rest_pose"),
   [ "icon_rest_pose_small"  ]   = UIBitmap_GetHandle("icon_rest_pose_24"),

   [ "icon_event_mixer"  ]         = UIBitmap_GetHandle("eventMixer_48"),
   [ "icon_event_mixer_disabled" ] = UIBitmap_GetHandle("eventMixer_48D"),
   [ "icon_event_mixer_small"  ]   = UIBitmap_GetHandle("eventMixer_24"),

   [ "icon_event_renamer"  ]         = UIBitmap_GetHandle("eventRenamer_48"),
   [ "icon_event_renamer_disabled" ] = UIBitmap_GetHandle("eventRenamer_48D"),
   [ "icon_event_renamer_small"  ]   = UIBitmap_GetHandle("eventRenamer_24"),

   [ "icon_state_machine" ]       = UIBitmap_GetHandle("state_machine"),
   [ "icon_state_machine_small" ] = UIBitmap_GetHandle("state_machine_small"),

   [ "save_success" ]             = UIBitmap_GetHandle("save_success"),
   [ "save_failure" ]             = UIBitmap_GetHandle("save_failure"),
   [ "play_pause" ]               = UIBitmap_GetHandle("play_pause"),
   [ "play_rewind" ]              = UIBitmap_GetHandle("rewind"),
   [ "play_forward" ]             = UIBitmap_GetHandle("forward"),
}

UIPref = {
   GetColor  = function(ColorName)  return PreferenceColors[ColorName]   end,
   GetFont   = function(FontName)   return PreferenceFonts[FontName]     end,
   GetBitmap = function(BitmapName) return PreferenceBitmaps[BitmapName] end,
}
