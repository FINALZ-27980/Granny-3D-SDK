require "ui_base"
require "character_toolbar"
require "state_machine_switcher"
require "file_operations"
require "ui_controller_pane"
require "ui_usermacro_pane"
require "ui_hotkeys"
require "tabbed_toolbar"

local function HandleMouseWheel(Item, Event)
   if (Item.MouseDown) then
      STB_MouseDrag(Item.STBEdit, Event.Point)
   end
end

local function MakeSpinControl(Item)
   local function SpinCoroutine(Event)
      Event = SimpleWaitForEvent(Event, LButtonDown, false, { Alt = false })

      if (Event) then
         local LastX = Event.Point.x
         local LastY = Event.Point.y

         Event = WaitForEvent(Event, LButtonUp, true,
                              function (Event)
                                 if (Event.Point) then
                                    local DeltaX = LastX - Event.Point.x
                                    local DeltaY = LastY - Event.Point.y
                                    CameraEAR(0, DeltaX * 0.02, DeltaY * 0.02)

                                    LastX = Event.Point.x
                                    LastY = Event.Point.y
                                 end
                              end)
      end

      Item.SpinAction = nil
   end

   return { Action = coroutine.create(SpinCoroutine),
            Abort  = function ()
                        Item.SpinAction = nil
                     end }
end

local function MakePanControl(Item)
   local function PanCoroutine(Event)
      local Event = MultiWaitForEvent(Event, false,
                                      { { Type = MButtonDown },
                                        { Type = LButtonDown, Alt = true } })
      if (Event) then
         local WaitType = UpEventFor(Event.Type)
         local LastX = Event.Point.x
         local LastY = Event.Point.y

         Event = WaitForEvent(Event, WaitType, true,
                              function (Event)
                                 if (Event.Point) then
                                    local DeltaX = LastX - Event.Point.x
                                    local DeltaY = LastY - Event.Point.y
                                    CameraPan(DeltaX * 0.0025, DeltaY * -0.0025)

                                    LastX = Event.Point.x
                                    LastY = Event.Point.y
                                 end
                              end)
      end

      Item.PanAction = nil
   end

   return { Action = coroutine.create(PanCoroutine),
            Abort  = function ()
                        Item.PanAction = nil
                     end }
end

local function MakeZoomControl(Item)
   local function ZoomCoroutine(Event)
      local Event = WaitForEvent(Event, MouseWheel, false)

      if (Event.Delta < 0) then
         ZoomCamera(false)
      else
         ZoomCamera(true)
      end

      Item.ZoomAction = nil
   end

   return { Action = coroutine.create(ZoomCoroutine),
            Abort  = function ()
                        Item.ZoomAction = nil
                     end }
end


local FontHandle      = UIPref.GetFont("tabview_tab")
local FontH, FontBL = GetFontHeightAndBaseline(FontHandle)

local FontColorClosed = UIPref.GetColor("motion_control_text_closed")
local FontColorOpen   = UIPref.GetColor("motion_control_text_open")

local ExternalPadding  = 10
local InternalPaddingH = 3
local InternalPaddingW = 5

local CharacterFollows   = true

local FollowLabel = "Camera Track"
local FollowRect  = MakeRect(0, 0, Checkbox.DimSmall(FollowLabel) + 2*InternalPaddingW, FontH)
local FollowTip   = ("When enabled, the camera will track character through\n" ..
                     "its root motion. Note that if the character moves away\n" ..
                     "from the camera while this is enabled, it may indicate\n" ..
                     "that the root motion setting on export are incorrect.")

local SnapLabel    = "Snap Camera to Character"
local SnapW, SnapH = Button.DimSmall(SnapLabel)

local ResetLabel     = "Reset Character Position"
local ResetW, ResetH = Button.DimSmall(ResetLabel)

local ButtonWidth = math.max(SnapW, ResetW)
local MaxWidth    = math.max(FollowRect.w, Checkbox.BoxGap() + ButtonWidth) + 2*InternalPaddingW

local UpRect  = MakeRect(0, 0, 200, Combobox.RecommendHeightSmall())
local UpTip   = ("Allows you to override the Up vector encoded in\n" ..
                 "the model file, and use the setting below\n")
local UpEntries = {
   "Use File Setting",
   "Use Y Up",
   "Use Z Up"
}

local MotionControlsTotalHeight = FollowRect.h + ResetH + SnapH + UpRect.h + 6 * InternalPaddingH


function MotionControls(ID)
   -- Checkbox on follow char
   local CurrY = InternalPaddingH
   local ThisFollowRect= MakeRect(InternalPaddingW, CurrY, FollowRect.w, FollowRect.h)
   local Changed, Track = Checkbox.DoSmall(IDItem.CreateSub(ID, "Follow"), ThisFollowRect, FollowLabel, GetCameraTracks())
   ToolTip.Register(FollowTip, ThisFollowRect)
   if (Changed) then
      SetCameraTracks(Track)
   end

   CurrY = CurrY + FollowRect.h + InternalPaddingH
   Button.DoSmall(IDItem.CreateSub(ID, "Snap"),
                  MakeRect(InternalPaddingW + Checkbox.BoxGap(), CurrY, ButtonWidth, SnapH),
                  SnapLabel, false, SnapCameraToCharacter)

   CurrY = CurrY + ResetH + InternalPaddingH
   Button.DoSmall(IDItem.CreateSub(ID, "ResetCharacter"),
                  MakeRect(InternalPaddingW + Checkbox.BoxGap(), CurrY, ButtonWidth, ResetH),
                  ResetLabel, false, ResetCharacterPosition)

   CurrY = CurrY + ResetH + InternalPaddingH
   local ThisUpRect= MakeRect(InternalPaddingW, CurrY, UpRect.w, UpRect.h)
   local NewEntry, Changed = Combobox.DoSmall(IDItem.CreateSub(ID, "UpVector"),
                                         ThisUpRect, UpEntries, Edit_GetUpVectorSetting())
   if (Changed) then
      Edit_SetUpVectorSetting(NewEntry)
   end
end


local FileToolTip       = ("File operations, character specification\n" ..
                           "and animation set management")
local CameraToolTip     = "Controls for the preview camera and motion tracking"
local UserMacrosToolTip = "Macros from user_macros.lua"
local ControllerToolTip = "Mappings from 360 controller to Graph inputs"


local Tabs = {
   { Label = "File and Character",  Tip = FileToolTip,       DrawFn = FileOperations,    Height = FileOperationsHeight },
   { Label = "Camera Controls",     Tip = CameraToolTip,     DrawFn = MotionControls,    Height = MotionControlsTotalHeight },
   { Label = "User Macros",         Tip = UserMacrosToolTip, DrawFn = UserMacros,        Height = FileOperationsHeight },
   { Label = "Controller",          Tip = ControllerToolTip, DrawFn = ControllerSchemes, Height = nil, HeightFn = ControllerSchemesHeight },
}

local CharacterHotkeys = {
   { KeyString   = "F",
     Description = "Frame the character in the viewport",
     EventSpecs  = { { Type = KeyDown, Key = KeyForChar("f") } },
     Fn = SnapCameraToCharacter }
}

local CurrentTab = -1
function DrawCharacterPane(ID)
   local Item, IsNew = IDItem.Get(ID)

   -- Subtlety: register the interactions *before* rendering the character, since we want
   -- the active node interaction to override the zoom/pan/spin actions if it's present.
   if (not Interactions.Active()) then
      Item.ZoomAction = Interactions.RegisterDefClipped(Item.ZoomAction or MakeZoomControl(Item))
      Item.SpinAction = Interactions.RegisterDefClipped(Item.SpinAction or MakeSpinControl(Item))
      Item.PanAction  = Interactions.RegisterDefClipped(Item.PanAction  or MakePanControl(Item))
   end

   CharacterRender(ID)

   Hotkeys.Register(IDItem.MakeID("CharacterPane", "Hotkeys"),
                    "Character",
                    CharacterHotkeys)

   TimeScrubber.Do(IDItem.CreateSub(ID, "Scrubber"))

   local TabRect = MakeRect(0, 0, UIArea.Get().w, Tabview.TabHeight)
   if (CurrentTab ~= -1) then
      if (Tabs[CurrentTab].Height) then
         TabRect.h = TabRect.h + Tabs[CurrentTab].Height
      else
         TabRect.h = TabRect.h + Tabs[CurrentTab].HeightFn()
      end
   end
   UIArea.ProtectedSimplePush(
      TabRect,
      function ()
         CurrentTab = Tabview.Do(IDItem.CreateSub(ID, "CharacterOperations"),
                                 Tabs, CurrentTab, true)
      end
   )
end

function PreviewCharacterPane(ID, SourceIdx, AnimIdx)
   local Item, IsNew = IDItem.Get(ID)

   CharacterPreview(ID, SourceIdx, AnimIdx)

   if (not Interactions.Active()) then
      Item.ZoomAction = Interactions.RegisterDefClipped(Item.ZoomAction or MakeZoomControl(Item))
      Item.SpinAction = Interactions.RegisterDefClipped(Item.SpinAction or MakeSpinControl(Item))
      Item.PanAction  = Interactions.RegisterDefClipped(Item.PanAction  or MakePanControl(Item))
   end
end

