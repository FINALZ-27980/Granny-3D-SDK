require "rect"
require "ui_base"

local FontHandle    = UIPref.GetFont("table")
local FontH, FontBL = GetFontHeightAndBaseline(FontHandle)

local VPadding = 2
local HPadding = 2

local HeaderHeight = FontH + VPadding*2
local ItemHeight   = FontH + VPadding*2


local function MakeRightClick(Item, Callback, Entries, Width)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, RButtonDown, false)

      if (Event.Point.y > HeaderHeight) then
         -- Determine which line contains the event, so we can tell if we stayed in the
         -- correct region for the up event
         local ValidClick = true
         local Row = math.floor((Event.Point.y - HeaderHeight) / ItemHeight)

         if (Row < #Entries) then
            local ValidRect = MakeRect(0, HeaderHeight + Row*ItemHeight,
                                       Width, ItemHeight)

            Event = WaitForEvent(Event, RButtonUp, true,
                                 function (Event)
                                    if (Event.Point) then
                                       ValidClick = RectContainsPoint(ValidRect, Event.Point)
                                    end
                                 end)

            if (ValidClick) then
               Callback(Entries[Row+1])
            end
         end
      else
         -- this was a click on the header, which does nothing at the moment...
      end

      Item.Action = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.Action = nil
                     end }
end


local function Do(ID, Headers, Entries, ColorCallback, RClickCallback)
   local Item, IsNew = IDItem.Get(ID)

   if (IsNew) then
      Item.NumCols   = #Headers
      Item.ColWidths = {}

      Item.TotalWidth = 0
      for i = 1,#Headers do
         Item.ColWidths[i] = 100
         Item.TotalWidth = Item.TotalWidth + Item.ColWidths[i]
      end

      if (UIAreaGet().w > Item.TotalWidth) then
         Item.ColWidths[#Headers] = Item.ColWidths[#Headers] + (UIAreaGet().w - Item.TotalWidth)
         Item.TotalWidth = UIAreaGet().w
      end
   end

   DrawRect(UIArea.Get(), MakeColor(0, 0, 0))

   local CurrY = 0
   local CurrX = 0

   -- draw headers
   for i = 1,#Headers do
      UIArea.ProtectedSimplePush(MakeRect(CurrX, CurrY, Item.ColWidths[i], HeaderHeight),
                                 function ()
                                    RenderText(FontHandle, Headers[i], 0,
                                               MakePoint(HPadding, FontBL + VPadding),
                                               MakeColor(1, 1, 1))
                                 end)

      CurrX = CurrX + Item.ColWidths[i]
   end
   CurrY = CurrY + HeaderHeight

   -- draw items
   for i = 1,#Entries do
      local CurrX = 0

      local RowColor = MakeColor(1, 1, 1)
      if (ColorCallback ~= nil) then
         RowColor = ColorCallback(Entries[i])
      end

      for sub = 1,#Headers do
         local Str = Entries[i][sub]
         UIArea.ProtectedSimplePush(MakeRect(CurrX, CurrY, Item.ColWidths[sub], ItemHeight),
                                    function ()
                                       RenderText(FontHandle, Str, 0,
                                                  MakePoint(HPadding, FontBL + VPadding),
                                                  RowColor)
                                    end)
         CurrX = CurrX + Item.ColWidths[sub]
      end

      CurrY = CurrY + ItemHeight
   end

   if (not Interactions.Active()) then
      Item.Action = Interactions.RegisterDefClipped(Item.Action or MakeRightClick(Item, RClickCallback,
                                                                                  Entries, UIArea.Get().w))
   end

   -- Will almost always be contained in a scroll area, return for that case.
   return MakeRect(0, 0, Item.TotalWidth, CurrY), true
end


Table = {
   Do = Do
}
