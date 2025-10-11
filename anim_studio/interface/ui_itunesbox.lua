require "deferred_rendering"
require "id_item_cache"
require "ui_area"
require "ui_preferences"
require "ui_scrollbar"

local LargeFontHandle         = UIPref.GetFont("combobox")
local LargeFontH, LargeFontBL = GetFontHeightAndBaseline(LargeFontHandle)

local LargeTriBitmap_Pressed  = UIBitmap_GetHandle("combobox_arrow_pressed_dark")
local LargeTriBitmap_Rest     = UIBitmap_GetHandle("combobox_arrow_rest_dark")
local LargeTriBitmap_Disabled = UIBitmap_GetHandle("combobox_arrow_dark_disabled")
local LargeTriBitmapW, LargeTriBitmapH = UIBitmap_Dimensions(LargeTriBitmap_Pressed)

ComboLargeParams = {
   ComboMenu9G        = NineGrid_GetHandle("combobox_menu"),
   NonEdit9G          = NineGrid_GetHandle("combobox_nonedit"),
   ComboNoneditable9G = NineGrid_GetHandle("combobox_menu"),

   TriBitmap_Pressed  = LargeTriBitmap_Pressed,
   TriBitmap_Rest     = LargeTriBitmap_Rest,
   TriBitmap_Disabled = LargeTriBitmap_Disabled,
   TriBitmapW = LargeTriBitmapW,
   TriBitmapH = LargeTriBitmapH,

   FontHandle = LargeFontHandle,
   FontH      = LargeFontH,
   FontBL     = LargeFontBL,

   FontY         = LargeFontBL + 5,
   FontX         = 10,
}

local SmallFontHandle         = UIPref.GetFont("combobox_small")
local SmallFontH, SmallFontBL = GetFontHeightAndBaseline(SmallFontHandle)

local SmallTriBitmap_Pressed      = UIBitmap_GetHandle("comboboxsmall_arrow_rest_dark")
local SmallTriBitmap_Rest         = UIBitmap_GetHandle("comboboxsmall_arrow_rest_dark")
local SmallTriBitmap_Disabled     = UIBitmap_GetHandle("comboboxsmall_arrow_rest_dark")
local SmallTriBitmapW, SmallTriBitmapH = UIBitmap_Dimensions(SmallTriBitmap_Pressed)

ComboSmallParams = {
   ComboMenu9G        = NineGrid_GetHandle("combobox_menu"),
   NonEdit9G          = NineGrid_GetHandle("combobox_nonedit"),
   ComboNoneditable9G = NineGrid_GetHandle("combobox_menu"),

   TriBitmap_Pressed  = SmallTriBitmap_Pressed,
   TriBitmap_Rest     = SmallTriBitmap_Rest,
   TriBitmap_Disabled = SmallTriBitmap_Disabled,
   TriBitmapW         = SmallTriBitmapW,
   TriBitmapH         = SmallTriBitmapH,

   FontHandle = SmallFontHandle,
   FontH      = SmallFontH,
   FontBL     = SmallFontBL,

   FontY = SmallFontBL + 3,
   FontX = 10,
}


HighColor = UIPref.GetColor("combo_highlight")

function DoesRectFit(AreaCapture, TestRect)
   if (UIAreaCaptureRectOnScreen(AreaCapture, TestRect)) then
      return TestRect
   else
      return nil
   end
end

local function EntriesRect(AreaCapture, FullArea, Entries, Params, AllowScroll)
   local EntriesHeight = #Entries * Params.FontH

   local Width = FullArea.w
   for i,str in ipairs(Entries) do
      local w = GetSimpleTextDimension(Params.FontHandle, str) + 2*Params.FontX
      Width = math.max(w, Width)
   end

   -- Default to right/down
   local DefaultRect = MakeRect(FullArea.x,
                                FullArea.y + FullArea.h,
                                Width, EntriesHeight)

   local Fits         = DoesRectFit(AreaCapture, DefaultRect)
   local ScrollParams = nil

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

   -- We've tried all the basic rects, let's slap a scrollbar on it, and go that way
   if (AllowScroll and (not Fits)) then
      ScrollParams = { Offset = 0,
                       FullHeight = DefaultRect.h }
      
      local DrawRect   = UIArea.Get()
      local CapturedPt = UIAreaTransformFromCapture(AreaCapture, MakePoint(DefaultRect.x, DefaultRect.y))
      
      -- Top/Bottom
      local DrawTop = false
      if (CapturedPt.y > (DrawRect.h / 2)) then
         DrawTop = true
      end
      
      local DrawRight = true
      if (CapturedPt.x > (DrawRect.w / 2)) then
         DrawRight = false
      end
      
      if (DrawTop) then
         DefaultRect.y = FullArea.y
         DefaultRect.h = UIAreaTransformFromCapture(AreaCapture, MakePoint(FullArea.x, FullArea.y)).y - FullArea.h
         DefaultRect.y = DefaultRect.y - DefaultRect.h
      else
         DefaultRect.h = DrawRect.h - (CapturedPt.y + FullArea.h)
      end

     if (DrawRight) then
        DefaultRect.x = (FullArea.x + FullArea.w) - DefaultRect.w - Scrollbar.VerticalWidth
     end

     local ScrollRect = CopyRect(DefaultRect)
     ScrollRect.x = DefaultRect.x + DefaultRect.w
     ScrollRect.w = Scrollbar.VerticalWidth
     ScrollParams.Rect = ScrollRect
     
     ScrollParams.ShuttleHeight = math.floor(ScrollParams.Rect.h * (ScrollParams.Rect.h / ScrollParams.FullHeight))
     ScrollParams.MaxOffset = ScrollParams.Rect.h - ScrollParams.ShuttleHeight
  end
  return (Fits or DefaultRect), ScrollParams
end


local function FilterEntriesRect(InitialX, InitialY, InitialW, InitialH, FilterEntries, Params)
   local Width = InitialW
   for i,v in ipairs(FilterEntries) do
      local str = v[1]
      local w = GetSimpleTextDimension(Params.FontHandle, str) + 2 * Params.FontX
      Width = math.max(w, Width)
   end

   -- Needs to adjust to screen position, but always left for now...
   InitialX = InitialX - (Width - InitialW)
   return MakeRect(InitialX, InitialY, Width, InitialH)
end

-- We have to run a little state machine here:
--  0: EventMove: update highlight
--     EventLeftDown: select entry OR
--                    pagedown     OR
--                    pageup       OR
--                    startscrolltrack (goto state 1)
--     EscPressed     abort selection
--  1: EventMove:   update scroll pos
--     EventLeftUp: end scroll track (goto 0)
--
-- We always begin in state 0.  Since state 1 is fairly simple, it's contained in a
-- separate loop inside the detection branch
local function ScrollEventLoop(Event, Item, ChangeHandler, Params)
   while (true) do
      Event = Interactions.Continue(YieldVal)

      local YOffset = math.floor((Item.ScrollParams.FullHeight - Item.EntriesRect.h) *
                              (Item.ScrollParams.Offset / Item.ScrollParams.MaxOffset))

      if (Event.Type == LButtonDown) then
         Item.Dragging = false
         if (RectContainsPoint(Item.ScrollParams.Rect, Event.Point)) then
            local ScrollY = Event.Point.y - Item.ScrollParams.Rect.y

            if (ScrollY < Item.ScrollParams.Offset) then
               Item.ScrollParams.Offset = math.max((Item.ScrollParams.Offset -
                                                    Item.ScrollParams.ShuttleHeight),
                                                   0)
            elseif (ScrollY > (Item.ScrollParams.Offset + Item.ScrollParams.ShuttleHeight)) then
               Item.ScrollParams.Offset = math.min((Item.ScrollParams.Offset +
                                                    Item.ScrollParams.ShuttleHeight),
                                                Item.ScrollParams.MaxOffset)
            else
               local StartY     = Event.Point.y
               local StartOffset = Item.ScrollParams.Offset
               Event = WaitForEvent(
                  Event, LButtonUp, true,
                  function (Event)
                     if (Event.Point) then
                        local NewOffset = StartOffset + (Event.Point.y - StartY)
                        Item.ScrollParams.Offset =
                           math.max(math.min(NewOffset, Item.ScrollParams.MaxOffset), 0)
                     end
                  end)
            end
         else
            return Event
         end
      elseif (Event.Type == MouseWheel) then
         if (Event.Delta > 0) then
            Item.ScrollParams.Offset =
               math.max((Item.ScrollParams.Offset - Params.FontH*3), 0)
         else
            Item.ScrollParams.Offset =
               math.min((Item.ScrollParams.Offset + Params.FontH*3), Item.ScrollParams.MaxOffset)
         end
      elseif (Event.Type == MouseMove) then
         -- todo: Handle scroll offset...
         Item.Dragging = RectContainsPoint(Item.EntriesRect, Event.Point)

         local OldHighlight = Item.HighlightEntry
         Item.HighlightEntry = math.floor((Event.Point.y - Item.EntriesRect.y + YOffset) / Params.FontH) + 1
         if (ChangeHandler and OldHighlight ~= Item.HighlightEntry) then
            ChangeHandler(Item)
         end
      elseif (Event.Type == CtrlKeyDown and Event.Key == Control_KeyEscape) then
         return Event
      end
   end

   return Event
end   

function ITunesBoxDropHandler(Event, Item, ChangeHander, Params)
   Item.HighlightEntry = Item.CurrEntry
   Item.Dragging = false
   Item.Dropped = true

   -- todo: compute rect above for list exceeding screensize
   -- todo: prevent clipping
   local EntriesHeight = #Item.Entries * Params.FontH

   Item.EntriesRect, Item.ScrollParams =
      EntriesRect(Item.AreaCapture, Item.FullArea, Item.Entries, Params, true)

   if (Item.ScrollParams) then
      -- Always wait for the release if we're going to scroll...
      Event = WaitForEvent(Event, LButtonUp, true)
   else
      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 if (RectContainsPoint(Item.EntriesRect, Event.Point)) then
                                    Item.Dragging = true
                                    local OldHighlight = Item.HighlightEntry
                                    Item.HighlightEntry = math.floor((Event.Point.y - Item.EntriesRect.y) / Params.FontH) + 1
                                    if (ChangeHandler and OldHighlight ~= Item.HighlightEntry) then
                                       ChangeHandler(Item)
                                    end
                                 end
                              end
                           end)
   end

   if (not Item.Dragging) then
      -- Potentially, we have to handle the scroll bar here...
      if (not Item.ScrollParams) then
         Event = WaitForEvent(Event, LButtonDown, true,
                              function (Event)
                                 if (Event.Point) then
                                    Item.Dragging = false
                                    if (RectContainsPoint(Item.EntriesRect, Event.Point)) then
                                       Item.Dragging = true
                                       Item.HighlightEntry = math.floor((Event.Point.y - Item.EntriesRect.y) / Params.FontH) + 1
                                    end
                                 end
                              end)
      else
         Event = ScrollEventLoop(Event, Item, ChangeHandler, Params)
      end
   end

   if (Event.Point) then
      if (RectContainsPoint(Item.EntriesRect, Event.Point)) then
         Item.CurrEntry = Item.HighlightEntry
         Item.IndexChanged = true
      end
   end

   Item.Dragging = false
   Item.InScroll = false
   Item.Dropped = false
end

local function MatchText(test_str, comp_strs)
   for i,v in ipairs(comp_strs) do
      if (string.find(string.lower(test_str), v, 1, true) == nil) then
         return false
      end
   end

   return true
end

local function GetFilters(Item)
   -- Parse out the comp strs
   local comp_strs = {}

   if (Item.STBEdit) then
      local Text, SelStart, SelEnd, Cursor = STB_GetTextParams(Item.STBEdit)
      for v in string.gmatch(Text, "([^ \t]+)") do
         table.insert(comp_strs, string.lower(v))
      end
   end

   return comp_strs
end

function ITunesBoxDrawEntryList(Item, Params)
   if (Item.Dropped or Item.ITunesDropped) then
      -- todo: drop shadow?

      local Capture = UIArea.Capture()
      DeferredRender.Register(
         function ()
            local UseRect
            if (Item.ITunesDropped) then
               UseRect    = Item.FilteredRect
               UseEntries = Item.FilteredEntries
            else
               UseRect    = Item.EntriesRect
               UseEntries = Item.Entries
            end

            local UL, Clipped = UIAreaTransformFromCapture(Capture, MakePoint(UseRect.x, UseRect.y))
            local Rect = MakeRect(UL.x, UL.y, UseRect.w, UseRect.h)

            local YOffset = 0
            if (Item.ScrollParams) then
               local ULS, ClippedS = UIAreaTransformFromCapture(Capture,
                                                                MakePoint(Item.ScrollParams.Rect.x,
                                                                          Item.ScrollParams.Rect.y))
               local RectS = MakeRect(ULS.x, ULS.y,
                                      Item.ScrollParams.Rect.w,
                                      Item.ScrollParams.Rect.h)
               -- Base
               -- DrawRect(RectS, MakeColor(0, 1, 0))

               -- Shuttle
               local RectShuttle = MakeRect(ULS.x + 1, ULS.y + Item.ScrollParams.Offset,
                                            Item.ScrollParams.Rect.w - 2, Item.ScrollParams.ShuttleHeight)
               -- DrawRect(RectShuttle, MakeColor(1, 0, 0))

               local GridBackground  = NineGrid_GetHandle("scrollbar_bg")
               local GridZone        = NineGrid_GetHandle("scrollbar_area")
               local GridThumb       = NineGrid_GetHandle("scrollbar_thumb")

               NineGrid_DrawLeft(GridZone,  RectS)
               NineGrid_DrawLeft(GridThumb, RectShuttle)
               
               -- Compute the Y Offset for the entries
               --  At 0         -> YOffset = 0
               --  At MaxOffset -> YOffset = FullHeight - Rect.h
               YOffset = math.floor((Item.ScrollParams.FullHeight - Rect.h) *
                                    (Item.ScrollParams.Offset / Item.ScrollParams.MaxOffset))
            end

            NineGrid_Draw(NineGrid_GetHandle("combobox_menu"), Rect)

            -- weird, but lines up correctly
            Rect.x = Rect.x + Params.FontX
            Rect.w = Rect.w - Params.FontX - 4

            UIArea.ProtectedSimplePush(
               Rect,
               function()
                  local CurrY = -YOffset

                  -- todo: last highlight rect missing a pixel at the bottom
                  for i,item in ipairs(UseEntries) do
                     local str = item
                     if (type(item) ~= "string") then
                        str = item[1]
                     end

                     local TextRect = MakeRect(0, CurrY, Rect.w, Params.FontH)

                     local RectColor = nil
                     local TextColor = UIPref.GetColor("combo_text")

                     if (i == Item.HighlightEntry and Item.Dragging) then
                        RectColor = HighColor
                        TextColor = UIPref.GetColor("combo_highlighttext")
                     elseif (Item.FilteredHighlightEntry and i == Item.FilteredHighlightEntry and Item.ITunesDropped) then
                        RectColor = HighColor
                        TextColor = UIPref.GetColor("combo_highlighttext")
                     end

                     if (RectColor) then
                        local HighRect = MakeRect(TextRect.x-3, TextRect.y, TextRect.w+5, Params.FontH)
                        DrawRect(HighRect, HighColor)
                     end

                     RenderText(Params.FontHandle, str, 0, MakePoint(0, CurrY + Params.FontBL), TextColor)

                     CurrY = CurrY + Params.FontH
                  end
               end)
         end)
   end
end


local function MakeClick(Item, Params)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      ITunesBoxDropHandler(Event, Item, nil, Params)

      Item.ClickAction = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.Dragging = false
                        Item.Dropped = false
                        Item.ButtonDown = false
                     end }
end


local function RenderTextWhileEditing(Item, Point, Params)
   local Text, SelStart, SelEnd, Cursor = STB_GetTextParams(Item.STBEdit)

   -- Actual text
   RenderDropText(Params.FontHandle, Text, 0, MakePoint(0, Params.FontY),
                  UIPref.GetColor("combo_text"),
                  UIPref.GetColor("combo_text_drop"))

   -- highlight
   if (SelEnd > SelStart) then
      UIArea.ProtectedSimplePush(
         MakeRect(SelStart, 0, SelEnd - SelStart, UIAreaGet().h),
         function()
            DrawRect(UIAreaGet(), MakeColor(0, 0, 1))
            RenderDropText(Params.FontHandle, Text, 0, MakePoint(-SelStart, Params.FontY),
                           UIPref.GetColor("combo_text"),
                           UIPref.GetColor("combo_text_drop"))
         end)
   end

   -- Cursor
   DrawRect(MakeRect(Cursor-1, 0, 2, Params.FontH), MakeColor(1, 1, 1))
end

local function FiltersDifferent(one, two)
   if (#one ~= #two) then
      return true
   end
   for i,str in ipairs(one) do
      if (str ~= two[i]) then
         return true
      end
   end

   return false
end

local function FilterEntries(Item, Params)
   local comp_strs = GetFilters(Item)

   Item.FilteredEntries = { }
   for i,str in ipairs(Item.Entries) do
      if (MatchText(str, comp_strs)) then
         table.insert(Item.FilteredEntries, { str, i })
      end
   end

   local EntriesHeight = #Item.FilteredEntries * Params.FontH
   Item.FilteredRect = FilterEntriesRect(Item.FullArea.x, Item.FullArea.y + Item.FullArea.h,
                                         Item.FullArea.w, EntriesHeight, Item.FilteredEntries,
                                         Params)

   -- Check to see if this has changed
   if (not Item.FilterApplied or FiltersDifferent(comp_strs, Item.FilterApplied)) then
      Item.FilteredHighlightEntry = nil
      Item.FilterApplied = comp_strs
   end
end

local function ITunesStart(Item)
   local EntriesHeight = #Item.Entries * Item.Params.FontH

   Item.EntriesRect, Item.ScrollParams = EntriesRect(Item.AreaCapture, Item.FullArea, Item.Entries,
                                                     Item.Params, false)

   Item.ITunesDropped = true
   FilterEntries(Item, Item.Params)

   STB_SelectAllPosEnd(Item.STBEdit)
end

local function ITunesEnd(Success, Item)
   Item.ITunesDropped = false

   if (not Success) then
      -- Unchanged
      -- Item.IndexChanged = NewEntry ~= Item.CurrEntry
      -- Item.CurrEntry = NewEntry
      -- Item.Canonical = Item.Entries[NewEntry]

      -- Clear
      Item.FilteredEntries = nil
      Item.FilteredHighlightEntry = nil
      Item.FilteredHighlightEntrySetByMouse = nil
      Item.FilterApplied = nil
      return
   end

   if (Item.FilteredHighlightEntry ~= nil) then
      local NewEntry = Item.FilteredEntries[Item.FilteredHighlightEntry][2]

      Item.IndexChanged = NewEntry ~= Item.CurrEntry
      Item.CurrEntry = NewEntry
      Item.Canonical = Item.Entries[NewEntry]

      Item.FilteredEntries = nil
      Item.FilteredHighlightEntry = nil
      Item.FilteredHighlightEntrySetByMouse = nil
      Item.FilterApplied = nil
   else
      Item.FilteredEntries = nil
      Item.FilteredHighlightEntry = nil
      Item.FilteredHighlightEntrySetByMouse = nil
      Item.FilterApplied = nil

      -- Pick the first item that matches the filter, unless there aren't any
      local comp_strs = GetFilters(Item)
      if (#comp_strs == 0) then
         return
      end

      for i,str in ipairs(Item.Entries) do
         if (MatchText(str, comp_strs)) then
            Item.IndexChanged = i ~= Item.CurrEntry
            Item.CurrEntry = i
            Item.Canonical = str
            return
         end
      end
   end
end

local function ITunesEvent(Item, Event)
   -- return value is (CancelEdit, IgnoreEvent)
   FilterEntries(Item, Item.Params)

   -- If this is a mouse event, set the highlight entry...
   if (Event.Point) then
      if (RectContainsPoint(Item.FilteredRect, Event.Point)) then
         Item.FilteredHighlightEntry = math.floor((Event.Point.y - Item.FilteredRect.y) / Item.Params.FontH) + 1
         Item.FilteredHighlightEntrySetByMouse = true
      else
         -- Yeah?  Classy!
         if (Item.FilteredHighlightEntrySetByMouse) then
            Item.FilteredHighlightEntry = nil
            Item.FilteredHighlightEntrySetByMouse = false
         end
      end

      if (Event.Type == LButtonDown) then
         return true, RectContainsPoint(Item.FilteredRect, Event.Point)
      end
   end

   if (Event.Type == DirKeyUp and (Event.Key == Direction_KeyUp or Event.Key == Direction_KeyDown)) then
      return false, true
   end

   if (Event.Type ~= DirKeyDown) then
      return false, false
   end

   local Inc = nil
   if (Event.Key == Direction_KeyUp) then
      Inc = -1
   elseif (Event.Key == Direction_KeyDown) then
      Inc = 1
   end

   if (not Inc or #Item.FilteredEntries == 0) then
      return false, false
   end

   if (Item.FilteredHighlightEntry) then
      Item.FilteredHighlightEntry = Item.FilteredHighlightEntry + Inc
   else
      Item.FilteredHighlightEntry = 1
   end

   if (Item.FilteredHighlightEntry <= 0) then
      Item.FilteredHighlightEntry = 1
   elseif (Item.FilteredHighlightEntry > #Item.FilteredEntries) then
      Item.FilteredHighlightEntry = #Item.FilteredEntries
   end

   -- Something touched it here...
   Item.FilteredHighlightEntrySetByMouse = false

   STB_SelectAllPosEnd(Item.STBEdit)
   return false, true
end

local function DoITunesBased(ID, Entries, CurrEntry, AllowFiltering, EditCallback, Params)
   local Item, IsNew = IDItem.Get(ID)
   if (IsNew) then
      Item.Dropped = false
      Item.IndexChanged = false
      Item.Changed = nil
      Item.Params = Params

      Item.Editing = false
      Item.FontHandle = Params.FontHandle
      Item.FontHeight, Item.FontBaseline = GetFontHeightAndBaseline(Params.FontHandle)

      Item.OffsetY = 0
   end

   if (not Item.Dropped and not Item.IndexChanged) then
      Item.CurrEntry = CurrEntry
   end

   Item.AreaCapture = UIArea.Capture()
   Item.FullArea = UIArea.Get()
   Item.Entries = Entries

   local ButtonArea  = MakeRect(Item.FullArea.x + Item.FullArea.w - Params.TriBitmapW, 0,
                                Params.TriBitmapW, Params.TriBitmapH)
   local DisplayArea = MakeRect(0, 0, Item.FullArea.w - Params.TriBitmapW, Item.FullArea.h)
   local TextArea    = MakeRect(Params.FontX, 0, Item.FullArea.w - Params.TriBitmapW - Params.FontX, Item.FullArea.h)

   -- Register the interaction, recreating if necessary
   if (not Interactions.Active() and #Entries ~= 0) then
      Item.ClickAction = Item.ClickAction or MakeClick(Item, Params)

      if (AllowFiltering) then
         UIArea.ProtectedSimplePush(
            TextArea,
            function()
               local Area = UIArea.Get()
               Item.Canonical = Entries[Item.CurrEntry]

               -- Register the interaction
               if (not Interactions.Active()) then
                  Item.EditAction =
                     Interactions.RegisterDefClipped(Item.EditAction or
                                                     MakeEdit{Item = Item,
                                                              Rect = Area,
                                                              IgnoreFirstClick = true,
                                                              StartCallback = ITunesStart,
                                                              EndCallback   = ITunesEnd,
                                                              EventCallback = ITunesEvent})
               end
            end)

         Interactions.RegisterClipped(Item.ClickAction, ButtonArea)
      elseif (EditCallback ~= nil) then
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

      -- Triangle drop...
   end

   -- Draw the interface...
   NineGrid_Draw(NineGrid_GetHandle("itunesbox"), DisplayArea)

   local TriHandle
   if (Item.Dropped) then
      TriHandle = Params.TriBitmap_Pressed
   else
      TriHandle = Params.TriBitmap_Rest
   end
   UIBitmap_Draw(TriHandle, MakePoint(ButtonArea.x, ButtonArea.y))

   if (not Item.Editing) then
      if (Item.CurrEntry >= 1 and Item.CurrEntry <= #Item.Entries) then
         UIArea.ProtectedSimplePush(
            TextArea,
            function()
               RenderDropText(Params.FontHandle, Entries[Item.CurrEntry], 0,
                              MakePoint(0, Params.FontY),
                              UIPref.GetColor("combo_text"),
                              UIPref.GetColor("combo_text_drop"))
            end)
      end
   else
      UIArea.ProtectedSimplePush(
         TextArea,
         function ()
            RenderTextWhileEditing(Item, MakePoint(0, Item.FontBaseline), Params)
         end)
   end

   ITunesBoxDrawEntryList(Item, Params)

   if (Item.IndexChanged) then
      local Changed = Item.IndexChanged
      Item.IndexChanged = false
      return Item.CurrEntry, Changed
   elseif (EditCallback and Item.Changed) then
      EditCallback(Item.CurrEntry, Item.Changed)
      Item.Changed = nil
      return Item.CurrEntry, false
   else
      return Item.CurrEntry, false
   end
end


local function DrawPlaceholder(Rect, String, Params)
   local ButtonArea  = MakeRect(Rect.x + Rect.w - Params.TriBitmapW, Rect.y,
                                Params.TriBitmapW, Params.TriBitmapH)
   local DisplayArea = MakeRect(Rect.x, Rect.y, Rect.w - Params.TriBitmapW, Rect.h)
   local TextArea    = MakeRect(Rect.x + Params.FontX, Rect.y, Rect.w - Params.TriBitmapW - Params.FontX, Rect.h)

   -- Draw the interface...
   NineGrid_Draw(Params.NonEdit9G, DisplayArea)
   UIBitmap_Draw(Params.TriBitmap_Disabled, MakePoint(ButtonArea.x, ButtonArea.y))

   if (String) then
      UIArea.ProtectedSimplePush(
         TextArea,
         function()
            RenderDropText(Params.FontHandle, String, 0, MakePoint(0, Params.FontY),
                           UIPref.GetColor("combo_text_placeholder"),
                           UIPref.GetColor("combo_text_drop"))
         end)
   end
end


ITunesBox = {
   Do = function (ID, Rect, Entries, CurrEntry)
           local RetVal,Changed = CurrEntry,false
           UIArea.ProtectedSimplePush(
              Rect,
              function()
                 RetVal,Changed = DoITunesBased(ID, Entries, CurrEntry, true, nil, ComboLargeParams)
              end)
           return RetVal,Changed
        end,
   RecommendHeight = function() return ComboLargeParams.TriBitmapH end,
   FontOffset      = function() return ComboLargeParams.FontY end,
   DrawPlaceholder = function(Rect, String) DrawPlaceholder(Rect, String, ComboLargeParams) end,
}

Combobox = {
   Do = function (ID, Rect, Entries, CurrEntry)
           local RetVal,Changed = CurrEntry,false
           UIArea.ProtectedSimplePush(
              Rect,
              function()
                 RetVal,Changed = DoITunesBased(ID, Entries, CurrEntry, false, nil, ComboLargeParams)
              end)
           return RetVal,Changed
        end,
   DoEditable = function (ID, Rect, Entries, CurrEntry, EditCallback)
                   local RetVal,Changed = CurrEntry,false
                   UIArea.ProtectedSimplePush(
                      Rect,
                      function()
                         RetVal,Changed = DoITunesBased(ID, Entries, CurrEntry, false, EditCallback, ComboLargeParams)
                      end)
                   return RetVal,Changed
                end,
   RecommendHeight = function() return ComboLargeParams.TriBitmapH end,
   FontOffset      = function() return ComboLargeParams.FontY end,
   DrawPlaceholder = function(Rect, String) DrawPlaceholder(Rect, String, ComboLargeParams) end,


   DoSmall = function (ID, Rect, Entries, CurrEntry)
           local RetVal,Changed = CurrEntry,false
           UIArea.ProtectedSimplePush(
              Rect,
              function()
                 RetVal,Changed = DoITunesBased(ID, Entries, CurrEntry, false, nil, ComboSmallParams)
              end)
           return RetVal,Changed
        end,
   DoEditableSmall = function (ID, Rect, Entries, CurrEntry, EditCallback)
                   local RetVal,Changed = CurrEntry,false
                   UIArea.ProtectedSimplePush(
                      Rect,
                      function()
                         RetVal,Changed = DoITunesBased(ID, Entries, CurrEntry, false, EditCallback, ComboSmallParams)
                      end)
                   return RetVal,Changed
                end,

                RecommendHeightSmall = function() return ComboSmallParams.TriBitmapH end,
   FontOffsetSmall      = function() return ComboSmallParams.FontY end,
   DrawPlaceholderSmall = function(Rect, String) DrawPlaceholder(Rect, String, ComboSmallParams) end,
}

