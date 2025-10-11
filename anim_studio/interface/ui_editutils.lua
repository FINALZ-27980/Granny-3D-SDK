require "ui_base"

local function HandleMouseMove(Item, Event)
   if (Item.MouseDown) then
      STB_MouseDrag(Item.STBEdit, Event.Point)
   end
end

local function HandleMouseClick(Item, Event, Rect)
   if (Event.Type == LButtonDown) then
      if (RectContainsPoint(Rect, Event.Point)) then
         Item.MouseDown = true
         STB_MouseDown(Item.STBEdit, MakePoint(Event.Point.x - Rect.x,
                                               Event.Point.y - Rect.y))
      else
         return true
      end
   else
      if (Item.MouseDown) then
         STB_MouseDrag(Item.STBEdit, MakePoint(Event.Point.x - Rect.x,
                                               Event.Point.y - Rect.y))
         Item.MouseDown = false
      end
   end

   return false
end

local function HandleMoveKey(Item, Event)
   STB_DirectionKey(Item.STBEdit, Event.Key, Event.Shift, Event.Ctrl)
end

local function HandleKey(Item, Event)
   -- Look for ctrl-x|c|v
   if (Event.Ctrl and Event.Key == KeyForChar("c")) then
      -- Copy current selection to the clipboard
      local Text = STB_GetSelectedText(Item.STBEdit)
      if (Text) then
         App_SetClipboardString(Text)
      end
   elseif (Event.Ctrl and Event.Key == KeyForChar("x")) then
      -- Copy current selection to the clipboard
      local Text = STB_GetSelectedText(Item.STBEdit)
      if (Text) then
         App_SetClipboardString(Text)
      end

      -- And do the cut
      STB_Cut(Item.STBEdit)
   elseif (Event.Ctrl and Event.Key == KeyForChar("v")) then
      local Text = App_GetClipboardString()
      if (Text) then
         STB_Paste(Item.STBEdit, Text)
      end
   else
      -- For now, disallow all non-print characters...
      STB_EditKey(Item.STBEdit, Event.Key, Event.Shift, Event.Ctrl, false)
   end
end

local function HandleCtrlKey(Item, Event)
   if (Event.Key == Control_KeyEscape or Event.Key == Control_KeyEnter) then
      return true, (Event.Key == Control_KeyEscape)
   end

   STB_EditCtrlKey(Item.STBEdit, Event.Key, Event.Shift, Event.Ctrl)
   return false
end

function EditEventLoop(Item, Rect, StartDownAction, EventCallback)
   local Success = true
   while (true) do
      local Event = Interactions.Continue(true)

      -- Let the caller look at the events and cancel if necessary
      local IgnoreThis = false
      if (EventCallback) then
         local CancelLoop, IgnoreEvent = EventCallback(Item, Event)
         if (CancelLoop) then
            Success = IgnoreEvent
            break
         end

         IgnoreThis = IgnoreEvent
      end

      if (not IgnoreThis) then
         if (Event.Type == LButtonDown or Event.Type == LButtonUp)   then
            if (HandleMouseClick(Item, Event, Rect)) then
               Success = true
               break
            end
         elseif (Event.Type == MouseMove)   then HandleMouseMove(Item, Event)
         elseif (Event.Type == DirKeyDown)  then HandleMoveKey(Item, Event)
         elseif (Event.Type == KeyDown)     then HandleKey(Item, Event)
         elseif (Event.Type == CtrlKeyDown) then
            local BreakOut, Cancel = HandleCtrlKey(Item, Event)
            if (BreakOut) then
               Success = not Cancel
               break
            end
         elseif (Event.Type == RButtonUp) then
            if (StartDownAction == RButtonDown) then
               StartDownAction = nil
            else
               break
            end
         end
      end
   end

   return Success
end

function MakeEdit(args)
   local Item     = args.Item
   local Rect     = args.Rect
   if (not Item or not Rect) then
      error("missing requirements")
   end

   local F2Rename         = args.F2Rename         or false
   local IgnoreFirstClick = args.IgnoreFirstClick or false
   local StartCallback    = args.StartCallback    or false
   local EndCallback      = args.EndCallback      or false
   local EventCallback    = args.EventCallback    or false
   local StartActive      = args.StartActive      or false
   local StartDownAction  = args.StartAction      or LButtonDown
   
   local function EditCoroutine(Event)
      local Event = Event

      if (not StartActive) then
         if (not F2Rename) then
            Event = WaitForEvent(Event, StartDownAction, false)
         else
            while (Event.Type ~= StartDownAction and
                   (Event.Type ~= FnKeyDown or Event.Key ~= 2)) do
               Event = Interactions.Continue(false)
            end
         end
      end

      Item.Editing   = true

      -- Allocate an STB_textedit.  This registers a gc method so we don't have to worry
      -- about freeing it when it goes inactive
      Item.STBEdit = AllocSTBTextEdit(Item.Canonical, Item.FontHandle)

      if (not StartActive) then
         if (Event.Type == StartDownAction and not IgnoreFirstClick) then
            if (StartDownAction == LButtonDown) then
               HandleMouseClick(Item, Event, Rect)
            else
               -- specially handled, since we look for lbutton events directly in much of
               -- this code
               local Store = Event.Type
               Event.Type = LButtonDown
               HandleMouseClick(Item, Event, Rect)
               Event.Type = LButtonUp
               HandleMouseClick(Item, Event, Rect)
            end
         else
            STB_SelectAllPosEnd(Item.STBEdit)
         end
      end

      if (StartCallback) then
         StartCallback(Item)
      end

      local Success = EditEventLoop(Item, Rect, StartDownAction, EventCallback)

      if (EndCallback) then
         EndCallback(Success, Item)
      end

      local Text, SelStart, SelEnd, Cursor = STB_GetTextParams(Item.STBEdit)
      Item.Changed    = Text
      Item.Success    = Success
      Item.Editing    = false
      Item.EditAction = nil
      Item.STBEdit    = nil
   end

   if (StartActive) then
      PushBackEvent(MakeNullEvent())
   end

   return { Action = coroutine.create(EditCoroutine),
            Abort  = function ()
                        if (EndCallback) then
                           EndCallback(false, Item)
                        end
                        Item.STBEdit = nil
                        Item.Editing = false
                        Item.EditAction = nil
                     end }
end

