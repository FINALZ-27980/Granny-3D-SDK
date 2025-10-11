require "ui_base"

local ArrowZone = 21

local GridBackground  = NineGrid_GetHandle("scrollbar_bg")
local GridZone        = NineGrid_GetHandle("scrollbar_area")
local GridThumb       = NineGrid_GetHandle("scrollbar_thumb")

local LeftArrow = UIBitmap_GetHandle("arrow_horizontal_left_dark")
local RightArrow = UIBitmap_GetHandle("arrow_horizontal_right_dark")

local UpArrow   = UIBitmap_GetHandle("arrow_vertical_up_dark")
local DownArrow = UIBitmap_GetHandle("arrow_vertical_down_dark")



local function ComputeZone(Rect, IsVertical)
   if (IsVertical) then
      return MakeRect(0, ArrowZone, Rect.w, Rect.h - 2*ArrowZone)
   else
      return MakeRect(ArrowZone, 0, Rect.w - 2*ArrowZone, Rect.h)
   end
end

local function ComputeStartLength(Offset, Start, End, Size)
   local S  = (Offset - Start) / (End - Start)
   local L = Size/(End-Start)
   return S,L
end


local function ClampScroll(Item, IsVertical)
   local AllowedRange
   local CurrentPoint
   local ScrollSize
   if (IsVertical) then
      AllowedRange = Item.ShuttleZone.h
      CurrentPoint = Item.ShuttleRect.y - Item.ShuttleZone.y
      ScrollSize   = Item.ShuttleRect.h
      ClientSize   = Item.ClientSize
   else
      AllowedRange = Item.ShuttleZone.w
      CurrentPoint = Item.ShuttleRect.x - Item.ShuttleZone.x
      ScrollSize   = Item.ShuttleRect.w
   end

   if (CurrentPoint < 0) then
      CurrentPoint = 0
   elseif (CurrentPoint + ScrollSize > AllowedRange) then
      CurrentPoint = AllowedRange - ScrollSize
   end

   if (IsVertical) then
      Item.ShuttleRect.y = CurrentPoint + Item.ShuttleZone.y
   else
      Item.ShuttleRect.y = CurrentPoint + Item.ShuttleZone.x
   end

   Item.Offset = Item.Start + (CurrentPoint / AllowedRange) * (Item.End - Item.Start)
end

local function MakeShuttleDrag(Item, IsVertical, Callback)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.Scrolling = true
      if (Callback) then
         Callback(true)
      end

      -- Need to distinguish between shuttle grab and a pagedown
      if (RectContainsPoint(Item.ShuttleRect, Event.Point)) then
         -- Drag
         local AnchorPoint = CopyPoint(Event.Point)
         local AnchorOffset
         if (IsVertical) then
            AnchorOffset = Item.ShuttleRect.y
         else
            AnchorOffset = Item.ShuttleRect.x
         end

         Event = WaitForEvent(Event, LButtonUp, true,
                              function (Event)
                                 if (Event.Point) then
                                    local NewOffsetX = Event.Point.x - AnchorPoint.x
                                    local NewOffsetY = Event.Point.y - AnchorPoint.y

                                    if (IsVertical) then
                                       Item.ShuttleRect.y = (AnchorOffset + NewOffsetY)
                                       ClampScroll(Item, IsVertical)
                                    else
                                       Item.ShuttleRect.x = (AnchorOffset + NewOffsetX)
                                       ClampScroll(Item, IsVertical)
                                    end
                                 end
                              end)
      else
         -- page down TODO: timing for repeat?
         if (IsVertical) then
            if (Event.Point.y < Item.ShuttleRect.y) then
               Item.ShuttleRect.y = Item.ShuttleRect.y - Item.ShuttleRect.h * 0.9
            else
               Item.ShuttleRect.y = Item.ShuttleRect.y + Item.ShuttleRect.h * 0.9
            end
         else
            if (Event.Point.x < Item.ShuttleRect.x) then
               Item.ShuttleRect.x = Item.ShuttleRect.x - Item.ShuttleRect.w * 0.9
            else
               Item.ShuttleRect.x = Item.ShuttleRect.x + Item.ShuttleRect.w * 0.9
            end
         end

         ClampScroll(Item, IsVertical)
         Event = WaitForEvent(Event, LButtonUp, true)
      end

      Item.Scrolling = false
      if (Callback) then
         Callback(false)
      end
      Item.ShuttleDrag = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.Scrolling = false
                        if (Callback) then
                           Callback(false)
                        end
                        Item.ShuttleDrag = nil
                     end }
end

local function Do(ID, Rect, Offset, Start, End, TotalSize, IsVertical, Callback)
   local Item, IsNew = IDItem.Get(ID)

   -- Is this a disabled scrollbar?
   if (not Item.Scrolling and ((End - Start) <= TotalSize)) then
      -- todo: break out...
      local ShZone = ComputeZone(Rect, IsVertical)
      UIArea.ProtectedSimplePush(Rect,
                                 function ()
                                    if (IsVertical) then
                                       NineGrid_DrawLeft(GridBackground, UIArea.Get())
                                       NineGrid_DrawLeft(GridZone, ShZone)
                                    else
                                       NineGrid_Draw(GridBackground, UIArea.Get())
                                       NineGrid_Draw(GridZone,       ShZone)
                                    end
                                 end)
      return Offset
   end

   -- Not disabled.
   if (IsNew) then
      Item.Scrolling = false
   end

   if (not Item.Scrolling) then
      Item.Offset = Offset
      Item.Start  = Start
      Item.End    = End
      Item.Size   = TotalSize

      -- Compute the shuttle parameters.  These change per frame as the object is sized
      Item.ShuttleZone = ComputeZone(Rect, IsVertical)

   else
      -- A scroll is in progress, leave the parameters alone...
   end

   local ShuttleStart, ShuttleLength =
      ComputeStartLength(Item.Offset, Item.Start, Item.End, Item.Size)
   if (IsVertical) then
      Item.ShuttleRect = MakeRect(Item.ShuttleZone.x,
                                  Item.ShuttleZone.y + (Item.ShuttleZone.h * ShuttleStart),
                                  Item.ShuttleZone.w,
                                  Item.ShuttleZone.h * ShuttleLength)
   else
      Item.ShuttleRect = MakeRect(Item.ShuttleZone.x + (Item.ShuttleZone.w * ShuttleStart),
                                  Item.ShuttleZone.y,
                                  Item.ShuttleZone.w * ShuttleLength,
                                  Item.ShuttleZone.h)
   end

   -- Draw the Item
   UIArea.ProtectedSimplePush(Rect,
                              function ()
                                 -- Register the interactions
                                 if (not Interactions.Active()) then
                                    Item.ShuttleDrag = Item.ShuttleDrag or MakeShuttleDrag(Item, IsVertical, Callback)
                                    Interactions.RegisterClipped(Item.ShuttleDrag, Item.ShuttleZone)
                                 end

                                 if (IsVertical) then
                                    NineGrid_DrawLeft(GridBackground, UIArea.Get())
                                    NineGrid_DrawLeft(GridZone, Item.ShuttleZone)
                                    NineGrid_DrawLeft(GridThumb, Item.ShuttleRect)

                                    UIBitmap_Draw(UpArrow, MakePoint(5, 7))
                                    UIBitmap_Draw(DownArrow, MakePoint(5, UIArea.Get().h - 12))
                                 else
                                    NineGrid_Draw(GridBackground, UIArea.Get())
                                    NineGrid_Draw(GridZone, Item.ShuttleZone)
                                    NineGrid_Draw(GridThumb, Item.ShuttleRect)

                                    UIBitmap_Draw(LeftArrow, MakePoint(7, 5))
                                    UIBitmap_Draw(RightArrow, MakePoint(UIArea.Get().w - 12, 5))
                                 end


                              end)

   return Item.Offset
end

Scrollbar = {
   DoVertical   = function(ID, Rect, Offset, Start, End, SectionHeight, Callback)
                     return Do(ID, Rect, Offset, Start, End, SectionHeight, true, Callback)
                  end,
   VerticalWidth = 18,

   DoHorizontal = function(ID, Rect, Offset, Start, End, SectionWidth, Callback)
                     return Do(ID, Rect, Offset, Start, End, SectionWidth, false, Callback)
                  end,
   HorizontalHeight = 18
}
