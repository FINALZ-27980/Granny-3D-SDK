require "rect"
require "id_item_cache"

local function MakeClick(Item)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.ButtonDown = true
      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 Item.ButtonDown = RectContainsPoint(Item.Area, Event.Point)
                              end
                           end)

      Item.StateChanged = Item.ButtonDown
      Item.ButtonDown  = false
      Item.ClickAction = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.StateChanged = false
                        Item.ButtonDown = false
                        Item.ClickAction = nil
                     end }
end

local function GetIndex(Down, State)
   if (Down) then
      if (State) then
         return 4
      else
         return 2
      end
   else
      if (State) then
         return 3
      else
         return 1
      end
   end
end


local function DoBase(ID, Rect, Label, State, FontHandle)
   local Item, IsNew = IDItem.Get(ID)
   if (IsNew) then
      Item.Area = Rect
      Item.Bitmaps = {
         UIBitmap_GetHandle("checkbox_rest"),
         UIBitmap_GetHandle("checkbox_pressed"),
         UIBitmap_GetHandle("checkbox_selected"),
         UIBitmap_GetHandle("checkbox_selected_pressed"),
      }
   end

   -- Register the interaction, recreating if necessary
   if (not Interactions.Active()) then
      Item.ClickAction = Interactions.RegisterClipped(Item.ClickAction or MakeClick(Item), Item.Area)
   end

   UIBitmap_Draw(Item.Bitmaps[GetIndex(Item.ButtonDown, State)], MakePoint(Rect.x, Rect.y))
   if (Label) then
      local fh, bl = GetFontHeightAndBaseline(FontHandle)
      local AtX = Rect.x + 20
      local AtY = Rect.y + bl
      RenderDropText(FontHandle, Label, 0,
                     MakePoint(AtX, AtY),
                     UIPref.GetColor("button_text"),
                     UIPref.GetColor("button_text_drop"))
   end

   if (Item.StateChanged) then
      Item.StateChanged = false
      return true, not State
   else
      return false, State
   end
end

local function Do(ID, Rect, Label, State)
   return DoBase(ID, Rect, Label, State, UIPref.GetFont("button"))
end

local function DoSmall(ID, Rect, Label, State)
   return DoBase(ID, Rect, Label, State, UIPref.GetFont("button_small"))
end

local function BoxGap()
   return 16 + 4
end

local function DimBase(Label, FontHandle)
   local w = BoxGap()
   local h = 16

   if (Label) then
      local fh, bl = GetFontHeightAndBaseline(FontHandle)
      w = w + GetSimpleTextDimension(FontHandle, Label)
      if (h < fh) then
         h = fh
      end
   end

   -- we want internal padding in the field of 2 pixels, plus 2 pixels for the beveled
   -- border
   return w, h
end

local function Dim(Label)
   return DimBase(Label, UIPref.GetFont("button"))
end

local function DimSmall(Label)
   return DimBase(Label, UIPref.GetFont("button_small"))
end


Checkbox = {
   Do       = Do,
   DoSmall  = DoSmall,
   Dim      = Dim,
   DimSmall = DimSmall,
   BoxGap   = BoxGap
}