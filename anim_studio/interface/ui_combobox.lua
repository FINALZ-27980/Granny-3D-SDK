require "deferred_rendering"
require "id_item_cache"
require "ui_area"
require "ui_preferences"

local CBBitmapW, CBBitmapH   = UIBitmap_Dimensions(UIBitmap_GetHandle("combobox"))
local TriBitmapW, TriBitmapH = UIBitmap_Dimensions(UIBitmap_GetHandle("combobox_arrow_pressed"))

local FontHandle    = UIPref.GetFont("combobox")
local FontH, FontBL = GetFontHeightAndBaseline(FontHandle)
local FontY         = FontBL + 5
local FontX         = 10
local HighColor     = UIPref.GetColor("combo_highlight")


function DoesRectFit(AreaCapture, TestRect)
   if (UIAreaCaptureRectOnScreen(AreaCapture, TestRect)) then
      return TestRect
   else
      return nil
   end
end


local function EntriesRect(AreaCapture, FullArea, Entries)
   local EntriesHeight = #Entries * FontH

   local Width = FullArea.w
   for i,str in ipairs(Entries) do
      local w = GetSimpleTextDimension(FontHandle, str) + 2*FontX
      Width = math.max(w, Width)
   end

   -- Default to right/down
   local DefaultRect = MakeRect(FullArea.x,
                                FullArea.y + FullArea.h,
                                Width, EntriesHeight)

   local Fits        = DoesRectFit(AreaCapture, DefaultRect)

   -- Left/down
   if (not Fits) then
      Fits = DoesRectFit(AreaCapture, MakeRect(FullArea.x - (Width - FullArea.w),
                                               FullArea.y + FullArea.h,
                                               Width, EntriesHeight))
   end

   -- right up
   if (not Fits) then
      Fits = DoesRectFit(AreaCapture, MakeRect(FullArea.x,
                                               FullArea.y - EntriesHeight,
                                               Width, EntriesHeight))
   end

   -- left/up
   if (not Fits) then
      Fits = DoesRectFit(AreaCapture, MakeRect(FullArea.x - (Width - FullArea.w),
                                               FullArea.y - EntriesHeight,
                                               Width, EntriesHeight))
   end

   return Fits or DefaultRect
end

function ComboBoxDropHandler(Event, Item, ChangeHandler)
   Item.HighlightEntry = Item.CurrEntry
   Item.Dragging = false
   Item.Dropped = true

   -- todo: compute rect above for list exceeding screensize
   Item.EntriesRect = EntriesRect(Item.AreaCapture, Item.FullArea, Item.Entries)

   Event = WaitForEvent(Event, LButtonUp, true,
                        function (Event)
                           if (Event.Point) then
                              if (RectContainsPoint(Item.EntriesRect, Event.Point)) then
                                 Item.Dragging = true
                                 Item.HighlightEntry = math.floor((Event.Point.y - Item.EntriesRect.y) / FontH) + 1
                              end
                           end
                        end)

   if (not Item.Dragging) then
      Event = WaitForEvent(Event, LButtonDown, true,
                           function (Event)
                              if (Event.Point) then
                                 if (RectContainsPoint(Item.EntriesRect, Event.Point)) then
                                    Item.Dragging = true

                                    local OldHighlight = Item.HighlightEntry
                                    Item.HighlightEntry = math.floor((Event.Point.y - Item.EntriesRect.y) / FontH) + 1
                                    if (ChangeHandler and OldHighlight ~= Item.HighlightEntry) then
                                       ChangeHandler(Item)
                                    end
                                 end
                              end
                           end)
   end

   if (Event.Point) then
      if (RectContainsPoint(Item.EntriesRect, Event.Point)) then
         Item.CurrEntry = Item.HighlightEntry
         Item.IndexChanged = true
      end
   end

   Item.Dragging = false
   Item.Dropped = false
end

function ComboBoxDrawEntryList(Item)
   if (Item.Dropped) then
      -- todo: drop shadow?

      local Capture = UIAreaCapture()
      DeferredRender.Register(
         function ()
            local UL, Clipped = UIAreaTransformFromCapture(Capture, MakePoint(Item.EntriesRect.x, Item.EntriesRect.y))
            local Rect = MakeRect(UL.x, UL.y,
                                  Item.EntriesRect.w, Item.EntriesRect.h + 5)

            NineGrid_Draw(NineGrid_GetHandle("combobox_menu"), Rect)

            -- weird, but lines up correctly
            Rect.x = Rect.x + FontX
            Rect.w = Rect.w - FontX - 4

            UIArea.ProtectedSimplePush(
               Rect,
               function()
                  local CurrY = 0

                  -- todo: last highlight rect missing a pixel at the bottom
                  for i,str in ipairs(Item.Entries) do
                     local TextRect = MakeRect(0, CurrY, Rect.w, FontH)

                     if (i == Item.HighlightEntry and Item.Dragging) then
                        local HighRect = MakeRect(TextRect.x-3, TextRect.y, TextRect.w+5, FontH)
                        DrawRect(HighRect, HighColor)
                        RenderText(FontHandle, str, 0, MakePoint(0, CurrY + FontBL), UIPref.GetColor("combo_highlighttext"))
                     else
                        RenderText(FontHandle, str, 0, MakePoint(0, CurrY + FontBL), UIPref.GetColor("combo_text"))
                     end

                     CurrY = CurrY + FontH
                  end
               end)
         end)
   end
end


local function MakeClick(Item)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      ComboBoxDropHandler(Event, Item, nil)

      Item.ClickAction = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.Dragging = false
                        Item.Dropped = false
                        Item.ButtonDown = false
                     end }
end


local function RenderTextWhileEditing(Item, Point)
   local Text, SelStart, SelEnd, Cursor = STB_GetTextParams(Item.STBEdit)

   -- Actual text
   RenderDropText(FontHandle, Text, 0, MakePoint(0, FontY),
                  UIPref.GetColor("combo_text"),
                  UIPref.GetColor("combo_text_drop"))

   -- highlight
   if (SelEnd > SelStart) then
      UIArea.ProtectedSimplePush(
         MakeRect(SelStart, 0, SelEnd - SelStart, UIAreaGet().h),
         function()
            DrawRect(UIAreaGet(), MakeColor(0, 0, 1))
            RenderDropText(Item.FontHandle, Text, 0, MakePoint(-SelStart, FontY),
                           UIPref.GetColor("combo_text"),
                           UIPref.GetColor("combo_text_drop"))
         end)
   end

   -- Cursor
   DrawRect(MakeRect(Cursor-1, 0, 2, Item.FontHeight), MakeColor(1, 1, 1))
end

local function DoAreaBased(ID, Entries, CurrEntry, EditCallback)
   local Item, IsNew = IDItem.Get(ID)

   if (IsNew) then
      Item.Dropped = false
      Item.IndexChanged = false
      Item.Changed = nil

      Item.Editing = false
      Item.FontHandle = FontHandle
      Item.FontHeight, Item.FontBaseline = GetFontHeightAndBaseline(FontHandle)
   end

   if (not Item.Dropped and not Item.IndexChanged) then
      Item.CurrEntry = CurrEntry
   end

   Item.AreaCapture = UIArea.Capture()
   Item.FullArea = UIArea.Get()
   Item.Entries = Entries

   local ButtonArea  = MakeRect(Item.FullArea.x + Item.FullArea.w - TriBitmapW, 0,
                                TriBitmapW, TriBitmapH)
   local DisplayArea = MakeRect(0, 0, Item.FullArea.w - TriBitmapW, Item.FullArea.h)
   local TextArea    = MakeRect(FontX, 0, Item.FullArea.w - TriBitmapW - FontX, Item.FullArea.h)

   -- Register the interaction, recreating if necessary
   if (not Interactions.Active() and #Entries ~= 0) then
      Item.ClickAction = Item.ClickAction or MakeClick(Item)

      if (EditCallback ~= nil) then
         UIArea.ProtectedSimplePush(
            TextArea,
            function()
               local Area = UIArea.Get()
               Item.Canonical = Entries[Item.CurrEntry]

               -- Register the interaction
               if (not Interactions.Active()) then
                  Item.EditAction =
                     Interactions.RegisterDefClipped(Item.EditAction or MakeEdit{Item = Item, Rect = Area})
               end

            end)

         Interactions.RegisterClipped(Item.ClickAction, ButtonArea)
      else
         Interactions.RegisterClipped(Item.ClickAction, Item.FullArea)
      end
   end

   -- Draw the interface...
   NineGrid_Draw(NineGrid_GetHandle("combobox_nonedit"), DisplayArea)

   local TriHandle
   if (Item.Dropped) then
      TriHandle = UIBitmap_GetHandle("combobox_arrow_pressed_dark")
   else
      TriHandle = UIBitmap_GetHandle("combobox_arrow_rest_dark")
   end
   UIBitmap_Draw(TriHandle, MakePoint(ButtonArea.x, ButtonArea.y))

   if (Item.CurrEntry >= 1 and Item.CurrEntry <= #Item.Entries) then
      UIArea.ProtectedSimplePush(
         TextArea,
         function()
            if (not Item.Editing) then
               local TextToShow = Entries[Item.CurrEntry]

               -- Handle a special case here.  If the text in the combobox was edited, and
               -- that edit is complete, we need to show the changed string until we
               -- return the changed value below.
               if (Item.Changed) then
                  TextToShow = Item.Changed
               end
               
               RenderDropText(FontHandle, TextToShow, 0, MakePoint(0, FontY),
                              UIPref.GetColor("combo_text"),
                              UIPref.GetColor("combo_text_drop"))
            else
               RenderTextWhileEditing(Item, MakePoint(0, Item.FontBaseline))
            end
         end)
   end

   ComboBoxDrawEntryList(Item)

   if (Item.IndexChanged) then
      local Changed = Item.IndexChanged
      Item.IndexChanged = false
      return Item.CurrEntry, Changed
   elseif (Item.Changed) then
      EditCallback(Item.CurrEntry, Item.Changed)
      Item.Changed = nil
      return Item.CurrEntry, false
   else
      return Item.CurrEntry, false
   end
end

local function Do(ID, Rect, Entries, CurrEntry)
   local RetVal,Changed = CurrEntry,false
   UIArea.ProtectedSimplePush(Rect, function()
                                       RetVal,Changed = DoAreaBased(ID, Entries, CurrEntry, nil)
                                    end)
   return RetVal,Changed
end

local function DoEditable(ID, Rect, Entries, CurrEntry, EditCallback)
   local RetVal,Changed = CurrEntry,false
   UIArea.ProtectedSimplePush(Rect, function()
                                       RetVal,Changed = DoAreaBased(ID, Entries, CurrEntry, EditCallback)
                                    end)
   return RetVal,Changed
end

Combobox = {
   Do = Do,
   DoEditable = DoEditable,
   RecommendHeight = function() return CBBitmapH end,
   FontOffset      = function() return FontY end,
}
