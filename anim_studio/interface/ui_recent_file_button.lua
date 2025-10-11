-- Button plus default interaction
require "event"
require "point"
require "id_item"
require "interactions"
require "deferred_rendering"
require "drag_context"
require "rect"
require "ui_pref"
require "ui_itunesbox"

local GridPressed = NineGrid_GetHandle("combobox_nonedit")
local GridRest    = NineGrid_GetHandle("combobox_nonedit")

local CBBitmapW, CBBitmapH   = UIBitmap_Dimensions(UIBitmap_GetHandle("combobox"))

local TriBitmapW, TriBitmapH = UIBitmap_Dimensions(UIBitmap_GetHandle("combobox_arrow_pressed"))

local TriHandleBitmap_Disabled = UIBitmap_GetHandle("combobox_arrow_dark_disabled")
local TriHandleBitmap_Pressed  = UIBitmap_GetHandle("combobox_arrow_pressed_dark")
local TriHandleBitmap_Rest     = UIBitmap_GetHandle("combobox_arrow_rest_dark")

local FontHandle    = UIPref.GetFont("combobox")
local FontH, FontBL = GetFontHeightAndBaseline(FontHandle)
local FontY         = FontBL + 5
local FontX         = 10
local HighColor     = UIPref.GetColor("combo_highlight")

local function MakeClick(Item, Action, Area)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.Active     = true
      Item.ButtonDown = true
      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 Item.ButtonDown = RectContainsPoint(Area, Event.Point)
                              end
                           end)

      if (Item.ButtonDown and Action) then
         Action()
      end

      Item.Active        = false
      Item.ButtonClicked = Item.ButtonDown
      Item.ClickAction   = nil
      Item.ButtonDown    = false
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.ButtonClicked = false
                        Item.ButtonDown = false
                        Item.ClickAction = nil
                     end }
end

local function FadeRecent(Item)
   if (Item.HighlightEntry > 0 and Item.HighlightEntry < #Item.RecentList) then
      PushFaderString(Item.RecentList[Item.HighlightEntry][2], MakeColor(0.5, 1, 1), 1.5)
   end
end

local function MakeRecentClick(Item, RecentAction)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.Entries = { }
      for i,v in ipairs(Item.RecentList) do
         Item.Entries[i] = v[1]
      end
      Item.CurrentEntry = nil

      ITunesBoxDropHandler(Event, Item, FadeRecent, ComboLargeParams)

      if (Item.IndexChanged and RecentAction) then
         local Entry = Item.RecentList[Item.CurrEntry]
         RecentAction(Entry[2], Entry[1])
         Item.RecentList = nil
         Item.IndexChanged = false
      end

      Item.RecentAction  = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.Dragging = false
                        Item.Dropped = false
                        Item.RecentAction = nil
                     end }
end

local function Do(ID, Text, FileListFn, Action, RecentAction)
   -- Obtain the cached state for this button
   local Item,IsNew = IDItem.Get(ID)

   Item.RecentList = FileListFn()

   Item.AreaCapture = UIArea.Capture()
   Item.FullArea    = UIArea.Get()
   Item.RecentArea  = MakeRect(Item.FullArea.x + Item.FullArea.w - TriBitmapW, 0,
                               TriBitmapW, TriBitmapH)
   Item.Area = MakeRect(0, 0, Item.FullArea.w - TriBitmapW, Item.FullArea.h)

   -- Register the interaction, recreating if necessary
   if (not Interactions.Active()) then
      if (#Item.RecentList ~= 0) then
         Item.ClickAction =
            Interactions.RegisterClipped(Item.ClickAction or MakeClick(Item, Action, Item.Area), Item.Area)
         Item.RecentAction =
            Interactions.RegisterClipped(Item.RecentAction or MakeRecentClick(Item, RecentAction), Item.RecentArea)
      else
         Item.ClickAction =
            Interactions.RegisterDefClipped(Item.ClickAction or MakeClick(Item, Action, Item.FullArea))
         Item.RecentAction = nil;
      end
   end

   -- Draw the button
   local Grid = GridRest
   if (Item.ButtonDown) then
      Grid = GridPressed
   end
   NineGrid_Draw(Grid, Item.Area)

   local TriHandle
   if (#Item.RecentList == 0) then
      TriHandle = TriHandleBitmap_Disabled
   elseif (Item.Dropped) then
      TriHandle = TriHandleBitmap_Pressed
   else
      TriHandle = TriHandleBitmap_Rest
   end
   UIBitmap_Draw(TriHandle, MakePoint(Item.RecentArea.x, Item.RecentArea.y))

   UIArea.ProtectedSimplePush(
      MakeRect(Item.Area.x + 2, Item.Area.y + 2, Item.Area.w - 4, Item.Area.h - 4),
      function()
         local w = GetSimpleTextDimension(FontHandle, Text)
         local AtX = math.floor((UIArea.Get().w/2) - (w/2))
         local AtY = (UIArea.Get().h - (FontH - FontBL) - 1)

         RenderDropText(FontHandle, Text, 0,
                        MakePoint(AtX, AtY),
                        UIPref.GetColor("button_text"),
                        UIPref.GetColor("button_text_drop"))
      end)

   ITunesBoxDrawEntryList(Item, ComboLargeParams)

   -- Return the clicked variable and clear it
   local RetVal
   RetVal, Item.ButtonClicked = Item.ButtonClicked, nil
   return RetVal
end

local function Dim(Text)
   local w = GetSimpleTextDimension(FontHandle, Text)

   -- we want internal padding in the field of 2 pixels, plus 2 pixels for the beveled
   -- border
   return w + TriBitmapW + 28, 26
end


RecentFileButton = {
   Do = function(ID, Rect, Text, FileListFn, Action, RecentAction)
           local RetVal = nil
           UIArea.ProtectedSimplePush(Rect, function()
                                               RetVal = Do(ID, Text, FileListFn, Action, RecentAction)
                                            end)
           return RetVal
        end,
   Dim = Dim
}
