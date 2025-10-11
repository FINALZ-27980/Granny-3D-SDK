require "ui_base"
require "ui_preferences"
require "math"
require "event"
require "ui_hotkeys"

local FontHandle = UIPref.GetFont("tabview_tab")
local FontHeight, FontBaseline = GetFontHeightAndBaseline(FontHandle)

local VertPadding  = 4
local HorizPadding = 10
local PixelsPerSecond = 250

local ScrubButWidth  = 80
local ScrubButHeight = FontHeight + 2*VertPadding
local ScrubBarHeight = 30


local TextColor     = UIPref.GetColor("tabview_text")
local TextDropColor = UIPref.GetColor("tabview_text_drop")

local function MakeSetScrub(Item, Rect, SetVal)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.ButtonDown = true
      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 Item.ButtonDown = RectContainsPoint(Rect, Event.Point)
                              end
                           end)

      if (Item.ButtonDown) then
         Edit_SetScrubMode(SetVal)
      end

      Item.ButtonClick = nil
      Item.ButtonDown = false
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.ButtonClick = nil
                        Item.ButtonDown = false
                     end }
end

local function MakeScrubberClick(Item)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.ScrubActive = true
      Item.DropTime    = Item.Curr
      Item.DroppedAt   = CopyPoint(Event.Point)

      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              Item.ScrubActive = true  -- Ok, I am mystified as to why this is necessary

                              if (Event.Point) then
                                 local Diff = Item.DroppedAt.x - Event.Point.x
                                 Item.Curr = Item.DropTime + Diff / PixelsPerSecond

                                 if (Item.Curr > 0) then
                                    Item.Curr = 0
                                 elseif (Item.Curr < Item.Least) then
                                    Item.Curr = Item.Least
                                 end
                              end
                           end)

      Item.ScrubClick    = nil
      Item.ScrubberDown  = false
      Item.ScrubActive   = false
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.ScrubClick   = nil
                        Item.ScrubberDown = false
                        Item.ScrubActive  = false
                     end }
end


local function SpaceToggle()
   if (Edit_InScrubMode()) then
      if (Edit_GetScrubTimeMotion() ~= 0.0) then
         Edit_SetScrubTimeMotion(0)
      else
         Edit_SetScrubTimeMotion(1)
      end
   else
      Edit_SetPause(not Edit_GetPause())
   end

   PushFader("play_pause", 0.5)
end

local function ArrowDown(Event)
   if (not Edit_InScrubMode()) then
      return
   end
   
   if (Event.Key == Direction_KeyLeft) then
      PushFader("play_rewind", 0.5)
      Edit_SetScrubTimeMotion(-1)
   elseif (Event.Key == Direction_KeyRight) then
      PushFader("play_forward", 0.5)
      Edit_SetScrubTimeMotion(1)
   end
end

local function ArrowUp(Event)
   if (not Edit_InScrubMode()) then
      return
   end
   
   Edit_SetScrubTimeMotion(0)
end

local ScrubberHotkeys = {
   { KeyString = "Space or Play/Pause",
     Description = "Toggle playback in the preview window",
     EventSpecs = { { Type = KeyDown, Key = KeyForChar(" ") },
                    { Type = CtrlKeyDown, Key = Control_KeyPlayPause } },
     Fn = SpaceToggle },

   { KeyString = "Left",
     Description = "Run time backward (When scrubbing)",
     EventSpecs = { { Type = DirKeyDown, Key = Direction_KeyLeft, Alt = false } },
     FnDown = ArrowDown,
     FnUp   = ArrowUp },

   { KeyString = "Right",
     Description = "Run time forward (When scrubbing)",
     EventSpecs = { { Type = DirKeyDown, Key = Direction_KeyRight, Alt = false } },
     FnDown = ArrowDown,
     FnUp   = ArrowUp },

   { KeyString = "<Esc>",
     Description = "Dismiss time scrubber",
     EventSpecs  = { { Type = CtrlKeyDown, Key = Control_KeyEscape } },
     Fn          = function() Edit_SetScrubMode(false) end }
}


local function Do(ID)
   local Item, IsNew = IDItem.Get(ID)
   if (IsNew) then
      Item.Curr = 0
   end

   Item.BaseArea         = UIArea.Get()
   Item.HiddenButtonRect = MakeRect(Item.BaseArea.x + Item.BaseArea.w/2 - ScrubButWidth/2,
                                    Item.BaseArea.y + Item.BaseArea.h - ScrubButHeight,
                                    ScrubButWidth, ScrubButHeight)
   Item.ShownButtonRect = MakeRect(Item.BaseArea.x + Item.BaseArea.w/2 - ScrubButWidth/2,
                                   Item.BaseArea.y + Item.BaseArea.h - ScrubButHeight - ScrubBarHeight,
                                   ScrubButWidth, ScrubButHeight)
   Item.ScrubberRect = MakeRect(Item.BaseArea.x,
                                Item.BaseArea.y + Item.BaseArea.h - ScrubBarHeight,
                                Item.BaseArea.w, ScrubBarHeight)

   local Curr    = Item.Curr
   local Changed = false

   if (Edit_InScrubMode()) then
      local MinRange, MaxRange = Edit_ScrubRange()
      Item.Least = MinRange - MaxRange

      if (Item.ScrubActive) then
         Changed = true
         Curr = Item.Curr
      else
         Item.Curr = Edit_GetGlobalClock() - MaxRange
      end

      if (Changed) then
         Edit_ScrubTo(MaxRange + Item.Curr)
      end

      if (not Interactions.Active()) then
         Item.ButtonClick =
            Interactions.RegisterClipped(Item.ButtonClick or MakeSetScrub(Item, Item.ShownButtonRect, false),
                                         Item.ShownButtonRect)
         Item.ScrubClick =
            Interactions.RegisterClipped(Item.ScrubClick or MakeScrubberClick(Item),
                                         Item.ScrubberRect)
      end

      
      NineGrid_Draw(NineGrid_GetHandle("tabview_top_active"), Item.ShownButtonRect)
      RenderDropText(FontHandle, "hide scrub", 0,
                     MakePoint(Item.ShownButtonRect.x + HorizPadding,
                               Item.ShownButtonRect.y + FontBaseline + 3),
                     TextColor, TextDropColor)

      DrawTimeCompass(Item.ScrubberRect, Item.Curr, PixelsPerSecond, Item.Least)
   else
      Item.Curr = 0

      if (not Interactions.Active()) then
         Item.ScrubClick  = nil
         Item.ButtonClick =
            Interactions.RegisterClipped(Item.ButtonClick or MakeSetScrub(Item, Item.HiddenButtonRect, true),
                                         Item.HiddenButtonRect)
      end

      NineGrid_Draw(NineGrid_GetHandle("tabview_top_active"), Item.HiddenButtonRect)
      RenderDropText(FontHandle, "time scrub", 0,
                     MakePoint(Item.HiddenButtonRect.x + HorizPadding,
                               Item.HiddenButtonRect.y + FontBaseline + 3),
                     TextColor, TextDropColor)
   end

   Hotkeys.Register(IDItem.MakeID("Scrubber", "Hotkeys"),
                    "Time Control",
                    ScrubberHotkeys)

   return Changed, Curr
end

TimeScrubber = {
   Do = Do
}
