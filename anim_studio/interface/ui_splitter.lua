-- Split window
require "id_item_cache"
require "ui_area"
require "interactions"
require "ui_bitmap"

local DraggableWidth   = 6
local UndraggableWidth = 0

local VertHandle  = UIBitmap_GetHandle("split_vert")
local HorizHandle = UIBitmap_GetHandle("split_horiz")

local function HandleDrag(Item, coord, MinCoord, Area, IsHorizontal)
   if (not Item.Dragging) then
      return coord
   end

   local PosCoord = coord
   local MaxCoord
   if (IsHorizontal) then
      MaxCoord = Area.h - MinCoord
      PosCoord = PosCoord + (Item.DeltaPoint.y - Item.StartPoint.y)
   else
      MaxCoord = Area.w - MinCoord
      PosCoord = PosCoord + (Item.DeltaPoint.x - Item.StartPoint.x)
   end

   if (MinCoord > 0) then
      if (PosCoord < MinCoord) then
         PosCoord = MinCoord
      elseif (PosCoord > MaxCoord) then
         PosCoord = MaxCoord
      end
   end

   -- Reset the last point to clamp this correctly
   Item.StartPoint.x = PosCoord
   Item.StartPoint.y = PosCoord
   
   return PosCoord
end

local function CreateRects(Rect, coord, Dimension, IsHorizontal)
   local UseCoord = coord
   local HalfDim = math.floor(Dimension / 2)

   if (IsHorizontal) then
      if (coord < 0) then
         UseCoord = Rect.h + coord
      end
      local TopRect    = MakeRect(Rect.x, Rect.y, Rect.w, UseCoord - HalfDim)
      local ItemRect   = MakeRect(Rect.x, TopRect.y + TopRect.h, Rect.w, Dimension)
      local BottomRect = MakeRect(Rect.x, ItemRect.y + ItemRect.h, Rect.w, Rect.h - (TopRect.h + ItemRect.h))
      return TopRect, ItemRect, BottomRect
   else
      if (coord < 0) then
         UseCoord = Rect.w + coord
      end

      local LeftRect  = MakeRect(Rect.x, Rect.y, UseCoord - HalfDim, Rect.h)
      local ItemRect  = MakeRect(LeftRect.x + LeftRect.w, Rect.y, Dimension, Rect.h)
      local RightRect = MakeRect(ItemRect.x + ItemRect.w, Rect.y, Rect.w - (LeftRect.w + ItemRect.w), Rect.h)

      return LeftRect, ItemRect, RightRect
   end
end


local function MakeSplitAction(Item)
   local function SplitCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.Dragging  = true
      Item.StartPoint = Event.Point
      Item.DeltaPoint = Event.Point

      WaitForEvent(Event, LButtonUp, true,
                   function (Event)
                      if (Event.Type == MouseMove or Event.Type == LButtonUp) then
                         Item.DeltaPoint = CopyPoint(Event.Point)
                      end
                   end)

      Item.Dragging = false
      Item.DragAction = nil
   end

   return { Action = coroutine.create(SplitCoroutine),
            Abort = function ()
                       Item.Dragging = false
                       Item.DragAction = nil
                    end }
end

local function Handle(ID, Coord, MinDimension, Draggable, FnBack, FnFront, IsHorizontal)
   local Item, IsNew = IDItem.Get(ID)

   local Area = UIArea.Get()

   -- Clamp to handle resizing
   if (MinDimension > 0) then
      if (not IsHorizontal and Coord > (Area.w - MinDimension)) then
         Coord = (Area.w - MinDimension)
      elseif (IsHorizontal and Coord > (Area.h - MinDimension)) then
         Coord = (Area.h - MinDimension)
      end

      if (Coord < MinDimension) then
         Coord = MinDimension
      end
   end

   -- Update the Coords if we're dragging
   Coord = HandleDrag(Item, Coord, MinDimension, Area, IsHorizontal)

   local Width = UndraggableWidth
   if (Draggable) then
      Width = DraggableWidth
   end

   local BackRect
   local FrontRect
   BackRect, Item.Area, FrontRect =
      CreateRects(Area, Coord, Width, IsHorizontal)

   if (Draggable and (not Interactions.Active())) then
      Item.DragAction = Interactions.RegisterClipped(Item.DragAction or MakeSplitAction(Item), Item.Area)
   end

   -- Root of all evil: don't create the SubID unless we have sub functions
   local LocalID = (FnBack or FnFront) and IDItem.CreateSub(ID, "Split")

   if (FnBack) then
      UIArea.ProtectedSimplePush(BackRect, function () FnBack(LocalID) end)
   end

   if (FnFront) then
      UIArea.ProtectedSimplePush(FrontRect, function () FnFront(LocalID) end)
   end

   local UseHandle = VertHandle
   if (IsHorizontal) then
      UseHandle = HorizHandle
   end

   if (Draggable) then
      UIBitmap_DrawStretched(UseHandle, Item.Area)
   end

   return Coord
end

Splitter = {
   DoHorizontal = function(ID, Coord, MinDimension, Draggable, FnBack, FnFront)
                     return Handle(ID, Coord, MinDimension, Draggable, FnBack, FnFront, true)
                  end,
   DoVertical = function(ID, Coord, MinDimension, Draggable, FnBack, FnFront)
                   return Handle(ID, Coord, MinDimension, Draggable, FnBack, FnFront, false)
                end
}

