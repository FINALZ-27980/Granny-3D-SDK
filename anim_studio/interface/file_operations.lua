require "id_item_cache"
require "ui_preferences"
require "ui_itunesbox"
require "ui_recent_file_button"
require "ui_button"
require "ui_autoplacer"
require "rect"

local VerticalPadding   = 5
local HorizontalPadding = 10
local InternalPadding   = 5

local FontHandle    = UIPref.GetFont("label_text")
local FontH, FontBL = GetFontHeightAndBaseline(FontHandle)
local FontColor     = UIPref.GetColor("label_text")
local FontDropColor = UIPref.GetColor("label_text_drop")

local NewFileButtonLabel   = "New Machine"
local NewFileButtonTip     = "Resets the tool for a new character"

local SaveButtonLabel   = "Save"
local SaveButtonTip     = "Saves the current state machine"

local SaveAsButtonLabel = "Save As..."
local SaveAsButtonTip   = "Prompts for a new filename to save"

local LoadButtonLabel   = "Load Machine..."
local LoadButtonTip     = ("Loads a previous created state machine.  Note that\n" ..
                           "the drop-down shows recently selected machines.")


local SetCharButtonLabel = "Set Model..."
local SetCharButtonTip   = ("Sets the gr2 that contains the model\n" ..
                            "that the state machine will drive")

local CurrModelLabel = "Model:"

local AddAnimationLabel = "Manage Anim Sets..."
local AddAnimationTip   = ("Add an animation or multiple animations to\n" ..
                           "the current machine's source set")

local CurrSetLabel = "Set:"
local SetAnimationSetTip = "Selects the current animation set to display"

local SelectionTip   = ("Selects the display model within the loaded gr2")
local SelAnimSetTip  = ("Selects the currently active animation set.")


-- These are the limiting items, check them first.
local NewButtonW,  NewButtonH = Button.Dim(NewFileButtonLabel)          -- for height
local DropButtonW, DropButtonH = RecentFileButton.Dim(LoadButtonLabel)  -- for width of the drop-downs

local Placement = AutoPlacement.New(AutoPlacement.Horizontal,
                                    MakeRect(InternalPadding, VerticalPadding, 100000, NewButtonH),  -- Basically infinite height
                                    InternalPadding)

local LoadButtonRect    = Placement:Next(DropButtonW, DropButtonH)
local SaveButtonRect    = Placement:Next(Button.Dim(SaveButtonLabel))
local SaveAsButtonRect  = Placement:Next(Button.Dim(SaveAsButtonLabel))
local NewFileButtonRect = Placement:Next(Button.Dim(NewFileButtonLabel))
Placement:CR()

local SetCharButtonRect = Placement:Next(DropButtonW, DropButtonH)
local CurrModelRect     = Placement:Next(SaveButtonRect.w, DropButtonH)  -- lines up under the "save" button correctly
local SelectionRect     = Placement:Next(1.5 * DropButtonW, DropButtonH)
Placement:CR()
local AddAnimationRect = Placement:Next(DropButtonW, DropButtonH)
local CurrSetRect      = Placement:Next(SaveButtonRect.w, DropButtonH)
local SetSelectionRect = Placement:Next(1.5 * DropButtonW, DropButtonH)
Placement:CR()

-- Export this so the main layout can refer to it
FileOperationsHeight = Placement.CurrY -- NewButtonH + 2 * VerticalPadding

local function RightLabel(rect, text)
   local w, h  = GetSimpleTextDimension(FontHandle, text)
   local shiftx = rect.w - w

   -- todo: need to baseline from the dropdown control so this can be calculated with the
   -- current font set.
   local shifty = FontBL + 7

   RenderDropText(FontHandle, text, 0,
                  MakePoint(rect.x + shiftx,
                            rect.y + shifty),
                  FontColor, FontDropColor)
end

function FileOperations(ID)
   -- First line: file operations
   -- 
   RecentFileButton.Do(IDItem.CreateSub(ID, "LoadChar"), LoadButtonRect, LoadButtonLabel,
                       Edit_GetRecentFiles,
                       function()
                          Edit_PromptForLoad()
                       end,
                       function(Entry, Name)
                          Edit_LoadFromFile(Entry)
                       end)
   ToolTip.Register(LoadButtonTip, LoadButtonRect)


   Button.Do(IDItem.CreateSub(ID, "SaveChar"), SaveButtonRect, SaveButtonLabel, false,
             function() Edit_SaveToFile(false) end)
   ToolTip.Register(SaveButtonTip, SaveButtonRect)

   Button.Do(IDItem.CreateSub(ID, "SaveAsChar"), SaveAsButtonRect, SaveAsButtonLabel, false,
             function() Edit_SaveToFile(true) end)
   ToolTip.Register(SaveAsButtonTip, SaveAsButtonRect)

   Button.Do(IDItem.CreateSub(ID, "NewFileChar"), NewFileButtonRect, NewFileButtonLabel, false,
             function() Edit_Reset() end)
   ToolTip.Register(NewFileButtonTip, NewFileButtonRect)

   -- Second line: character settings
   -- 
   RecentFileButton.Do(IDItem.CreateSub(ID, "NewChar"), SetCharButtonRect, SetCharButtonLabel,
                       Edit_GetRecentModels,
                       function() Edit_PromptForModel() end,
                       function(Entry, Text)
                          Edit_SetModelFromFilename(Entry)
                       end)
   ToolTip.Register(SetCharButtonTip, SetCharButtonRect)

   RightLabel(CurrModelRect, CurrModelLabel)

   SelectionRect.w = UIArea.Get().w - SelectionRect.x - HorizontalPadding
   if (SelectionRect.w < 100) then
      SelectionRect.w = 100
   end
   local NewCurrent, Changed = Combobox.Do(IDItem.CreateSub(ID, "ModelSelection"),
                                           SelectionRect,
                                           Edit_GetModelStrings(), Edit_GetModelIdx())
   if (Changed) then
      Edit_SetModelIdx(NewCurrent)
   end
   ToolTip.Register(SelectionTip, SelectionRect)

   Button.Do(IDItem.CreateSub(ID, "AddAnimation"), AddAnimationRect, AddAnimationLabel, false,
             function()
                AnimSetEditor.DoYielding()
             end)
   ToolTip.Register(AddAnimationTip, AddAnimationRect)

   RightLabel(CurrSetRect, CurrSetLabel)

   -- Third line: animation set operations
   -- 
   local SetNames, CurrSet = Edit_GetAnimationSets()
   SetSelectionRect.w = UIArea.Get().w - SetSelectionRect.x - HorizontalPadding
   if (SetSelectionRect.w < 100) then
      SetSelectionRect.w = 100
   end
   NewCurrent, Changed = Combobox.Do(IDItem.CreateSub(ID, "AnimSetSelection"),
                                     SetSelectionRect,
                                     SetNames, CurrSet)
   ToolTip.Register(SelAnimSetTip, SetSelectionRect)
   if (Changed) then
      Edit_SetCurrentAnimationSet(NewCurrent)
   end
   ToolTip.Register(SetAnimationSetTip, SetSelectionRect)
end
