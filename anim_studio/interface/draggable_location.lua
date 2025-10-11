require "rect"
require "event"
require "interaction"
require "id_item"

local ClickDim = 6

-- todo: axis constrained drag...
local function MakeDrag(Item)
   local function DragCoro(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.Dragging  = true
      Item.Changed   = false
      WaitForEvent(Event, LButtonUp, true,
                   function (Event)
                      if (Event.Type == MouseMove or Event.Type == LButtonUp) then
                         Item.CurrPos = CopyPoint(Event.Point)
                      end
                   end)

      Item.Dragging   = false
      Item.Changed    = true
      Item.DragAction = nil
   end

   return { Action = coroutine.create(DragCoro),
            Abort  = function ()
                        Item.Dragging = false
                        Item.CurrPos  = Item.OriginalPos
                        Item.DragAction = nil
                     end }
end

local function Do(ID, Position)
   local Item, IsNew = IDItem.Get(ID)

   if (IsNew) then
      Item.Dragging = false
      Item.Changed  = false
   end

   Item.OriginalPos = Position
   Item.ClickArea   = MakeRect(Position.x - ClickDim/2,
                               Position.y - ClickDim/2,
                               ClickDim, ClickDim)
   if (not Item.Dragging and not Item.Changed) then
      Item.CurrPos = Position
   end

   if (not Interactions.Active()) then
      Item.DragAction = Interactions.RegisterClipped(Item.DragAction or MakeDrag(Item), Item.ClickArea)
   end

   if (Item.Changed) then
      Item.Changed = false
      Item.OriginalPos = Item.CurrPos
      
      return Item.Dragging, true, Item.CurrPos, UIAreaRectContainsMouse(Item.ClickArea)
   else
      return Item.Dragging, false, Item.CurrPos, UIAreaRectContainsMouse(Item.ClickArea)
   end
end

DraggableLoc = {
   Do = Do
}