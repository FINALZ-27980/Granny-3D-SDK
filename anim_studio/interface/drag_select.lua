-- Draggable item with a drag handle at the top, and a a callback for internal gui items

require "rect"
require "event"
require "id_item_cache"
require "interactions"
require "drawarea"
require "ui_undostack"

local function MakeDrag(Item)
   local function DragCoroutine(Event)
      Event = SimpleWaitForEvent(Event, LButtonDown, false, { Alt = false })

      if (Event) then
         -- We handle the undo of selection changes to prevent multiple entries...
         local UndoPushed = false
         if (Edit_GetSelection() ~= nil) then
            Edit_PushUndoPos("Change Selection", UndoClass.NodeSelect)
            UndoPushed = true
         end

         if (Event.Ctrl ~= true and Event.Shift ~= true) then
            Edit_ClearSelection()
         end

         Item.DragActive = false
         Item.DropPoint = Event.Point
         Item.CurrPoint = Event.Point
         Event = WaitForEvent(Event, LButtonUp, true,
                              function (Event)
                                 if (Event.Point) then
                                    Item.CurrPoint = Event.Point
                                    if (Item.DragActive == false and
                                        (math.abs(Item.CurrPoint.x - Item.DropPoint.x) > 4 or
                                      math.abs(Item.CurrPoint.y - Item.DropPoint.y) > 4)) then
                                       Item.DragActive = true
                                    end
                                 end
                              end)


         if (Item.DragActive) then
            -- At this point, if the drag is active, bring in the rects
            local DragRect = RectFromPoints(Item.DropPoint, Item.CurrPoint)

            for k,v in ipairs(Item.Items) do
               if RectsOverlap(DragRect, v.rect) then
                  if (UndoPushed == false) then
                     Edit_PushUndoPos("Change selection", UndoClass.NodeSelect)
                     UndoPushed = true
                  end

                  Edit_SelectByID(v.id)
               end
            end
         else
            -- Leave the selection clear
         end
      end

      Item.DragActive = false
      Item.DragAction = nil
   end

   return { Action = coroutine.create(DragCoroutine),
            Abort  = function ()
                        Item.DragActive = false
                        Item.DragAction = nil
                     end }
end

local function Do(ID, Items)
   local Item, IsNew = IDItem.Get(ID)
   Item.Items = Items

   if (not Interactions.Active()) then
      Item.DragAction = Interactions.RegisterDefClipped(Item.DragAction or MakeDrag(Item))
   end

   if (Item.DragActive) then
      -- draw rect
      local DragRect = RectFromPoints(Item.DropPoint, Item.CurrPoint)

      local DragRect = RectFromPoints(Item.DropPoint, Item.CurrPoint)
      DrawRect(DragRect, MakeColorA(0.25, 0.25, 0.5, 0.5))
      DrawRectOutline(DragRect, MakeColor(0, 0, 0.5))
   end
end

DragSelect = {
   Do = Do,
}

