-- Draggable item with a drag handle at the top, and a a callback for internal gui items
require "rect"
require "point"
require "event"
require "id_item_cache"
require "interactions"

local function MakeDrag(Item)
   local function DragCoroutine(Event)
      local Event = MultiWaitForEvent(Event, false,
                                      { { Type = MButtonDown },
                                        { Type = LButtonDown, Alt = true } })
      if (Event) then
         local WaitType    = UpEventFor(Event.Type)
         local StartOffset = CopyPoint(ContainerEditingOffset)
         local DropPoint   = Event.Point

         Event = WaitForEvent(Event, WaitType, true,
                              function (Event)
                                 if (Event.Point) then
                                    ContainerEditingOffset.x = StartOffset.x + (DropPoint.x - Event.Point.x) / Item.ZoomFactor
                                    ContainerEditingOffset.y = StartOffset.y + (DropPoint.y - Event.Point.y) / Item.ZoomFactor
                                 end
                              end)
      end

      Item.MouseMoveAction = nil
   end

   return { Action = coroutine.create(DragCoroutine),
            Abort  = function ()
                        Item.MouseMoveAction = nil
                     end }
end

local function Do(ID, ZoomFactor)
   local Item, IsNew = IDItem.Get(ID)

   Item.ZoomFactor = ZoomFactor

   if (not Interactions.Active()) then
      Item.MouseMoveAction = Interactions.RegisterDefClipped(Item.MouseMoveAction or MakeDrag(Item))
   end
end

MouseMoveEditRegion = {
   Do = Do,
}

