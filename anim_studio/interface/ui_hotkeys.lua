require "id_item"
require "event"

local ActiveKeyTables = { }

local function ItemForEvent(Table, Event)
   for i,val in pairs(Table) do
      for ek,ev in pairs(val.EventSpecs) do
         local Success = true

         for k,v in pairs(ev) do
            if (Event[k] ~= v) then
               Success = false
               break
            end
         end

         if (Success) then
            return val
         end
      end
   end

   return nil
end


local FontHandle    = UIPref.GetFont("button")
local FontH, FontBL = GetFontHeightAndBaseline(FontHandle)

local SectionPadding  = 10
local HeaderPadding   = 5
local InternalPadding = 5

local ColonStr   = "  :  "
local ColonWidth = GetSimpleTextDimension(FontHandle, ColonStr)

local EscPosted = false

local function MakeClose(Item)
   local function HKCoroutine(Event)
      Event = MultiWaitForEvent(Event, false,
                                {
                                   { Type = CtrlKeyDown, Key = Control_KeyEscape }
                                })
      if (Event) then
         local WaitEvent = UpEventFor(Event.Type)
         local HitKey = Event.Key
         
         if (SimpleWaitForEvent(Event, WaitEvent, true, { Key = HitKey })) then
            if (HitKey == Control_KeyEscape) then
               EscPosted = true
            end
         end
      end

      PushBackEvent(MakeNullEvent())
      Item.CloseAction = nil
   end
   
   return { Action = coroutine.create(HKCoroutine),
            Abort  = function ()
                        Item.CloseAction = nil
                     end }
end

local function RenderHotkeys(ID, key_tables, ColonMidpoint)
   local Item, IsNew = IDItem.Get(ID)

   local Midpoint = math.floor(UIArea.Get().w / 2)
   local CurrY = 0

   local ColonX = ColonMidpoint - math.floor(ColonWidth/2)

   for i,kt in ipairs(key_tables) do
      -- Header
      local HeaderW = GetSimpleTextDimension(FontHandle, kt.Name)
      local HeaderX = ColonMidpoint - math.floor(HeaderW/2)

      RenderText(FontHandle, kt.Name, 0,
                 MakePoint(HeaderX, CurrY + FontBL),
                 MakeColor(1, 1, 0))
      CurrY = CurrY + FontH + HeaderPadding

      -- Entries
      for j,kte in ipairs(kt.Table) do
         local KeyW = GetSimpleTextDimension(FontHandle, kte.KeyString)
         local KeyX = ColonX - KeyW

         local DescX = ColonX + ColonWidth
         
         RenderText(FontHandle, kte.KeyString, 0,
                    MakePoint(KeyX, CurrY + FontBL),
                    MakeColor(1, 1, 1))
         RenderText(FontHandle, ColonStr, 0,
                    MakePoint(ColonX, CurrY + FontBL),
                    MakeColor(1, 1, 1))
         RenderText(FontHandle, kte.Description, 0,
                    MakePoint(DescX, CurrY + FontBL),
                    MakeColor(1, 1, 1))
         CurrY = CurrY + FontH + InternalPadding
      end
      
      -- Advance section
      CurrY = CurrY + SectionPadding
   end

   if (not Interactions.Active()) then
      Item.CloseAction = Interactions.RegisterGlobal(Item.CloseAction or MakeClose(Item))
   end
end

local function DisplayHotkeys()
   -- Grab these so they don't go away...
   local DisplayKeyTables = ActiveKeyTables

   -- a fake table for our stuff...
   table.insert(DisplayKeyTables, 1, { Name = "Global",
                                       Table = { { KeyString = "?",
                                                   Description = "Show this help page" },
                                                 { KeyString = "<Esc>",
                                                   Description = "Dismiss help" } } })
   
   local RequiredWidthL = 0
   local RequiredWidthR = 0
   local RequiredHeight = 0
   for i,kt in ipairs(DisplayKeyTables) do
      RequiredHeight = RequiredHeight + FontH + HeaderPadding  -- title

      local HeaderW = GetSimpleTextDimension(FontHandle, kt.Name)
      RequiredWidthL = math.max(HeaderW, RequiredWidthL)
      RequiredWidthR = math.max(HeaderW, RequiredWidthR)

      for j,kte in ipairs(kt.Table) do
         RequiredHeight = RequiredHeight + FontH + InternalPadding  -- entry

         local keyw  = GetSimpleTextDimension(FontHandle, kte.KeyString)
         local descw = GetSimpleTextDimension(FontHandle, kte.Description)
         RequiredWidthL = math.max(keyw, RequiredWidthL)
         RequiredWidthR = math.max(descw, RequiredWidthR)
      end

      RequiredHeight = RequiredHeight + SectionPadding -- padding at the bottom
   end

   -- Account for dialog pad
   RequiredWidth  = RequiredWidthL + RequiredWidthR + ColonWidth + 2*SectionPadding
   RequiredHeight = RequiredHeight + 2*SectionPadding
   ColonPlacement = RequiredWidthL + math.floor(ColonWidth/2)

   local function EventLoop(ID)
      while true do
         local UIRect  = UIArea.Get()
         local DialogW = RequiredWidth
         local DialogH = RequiredHeight

         local DialogRect = MakeRect(math.floor((UIRect.w - DialogW) / 2),
                                     math.floor((UIRect.h - DialogH) / 2),
                                     DialogW, DialogH)
         local InternalRect = MakeRect(math.floor((UIRect.w - DialogW) / 2) + SectionPadding,
                                       math.floor((UIRect.h - DialogH) / 2) + SectionPadding,
                                       DialogW - 2*SectionPadding, DialogH - 2*SectionPadding)

         -- Mask off the background
         DrawStippleRect(UIArea.Get(),
                         UIPref.GetColor("inaction_indicator_strong0"),
                         UIPref.GetColor("inaction_indicator_strong1"))

         -- Draw the dialog box
         NineGrid_Draw(NineGrid_GetHandle("dialog"), DialogRect)

         -- Render the editor itself
         UIArea.ProtectedSimplePush(InternalRect,
                                    function()
                                       RenderHotkeys(IDItem.CreateSub(ID, "display"),
                                                     DisplayKeyTables,
                                                     ColonPlacement)
                                    end)

         if (EscPosted) then
            EscPosted = false
            PushBackEvent(MakeNullEvent())
            coroutine.yield(true)
            return
         else
            coroutine.yield(false)
         end
      end
   end

   local EventCoro = coroutine.create(EventLoop)
   PushModal(EventCoro, "HotkeyDisplay")
   Interactions.Continue(true)

   return GetModalResult()
end

local function MakeKeys(Item)
   local function HKCoroutine(Event)
      -- Slice out the event specs for the item's table
      local EventTable = { }
      for i,v in ipairs(Item.Keys) do
         for ek,ev in pairs(v.EventSpecs) do
            table.insert(EventTable, ev)
         end
      end

      -- Add our special event
      table.insert(EventTable, { Type = KeyDown, Key = KeyForChar("?") })

      Event = MultiWaitForEvent(Event, false, EventTable)
      if (Event) then
         local MatchedItem = ItemForEvent(Item.Keys, Event)

         if (not MatchedItem and Event.Key == KeyForChar("?")) then
            MatchedItem = { Fn = DisplayHotkeys }
         end

         if (MatchedItem) then
            if (MatchedItem.FnDown) then
               MatchedItem.FnDown(Event)
            end

            
            local WaitEvent = UpEventFor(Event.Type)
            local HitKey    = Event.Key
            
            Event = MultiWaitForEvent(Event, true, { { Type = WaitEvent, Key = HitKey } })
            if (Event and MatchedItem.Fn) then
               MatchedItem.Fn()
            end

            if (MatchedItem.FnUp and Event) then
               MatchedItem.FnUp(Event)
            end
         end
      end

      Item.Action = nil
   end
   
   return { Action = coroutine.create(HKCoroutine),
            Abort  = function ()
                        Item.Action = nil
                     end }
end

local function Register(ID, SectionName, KeysTable)
   local Item, IsNew = IDItem.Get(ID)

   Item.Keys = KeysTable
   table.insert(ActiveKeyTables, { Name = SectionName, Table = KeysTable, ItemID = ID })

   if (not Interactions.Active()) then
      Item.Action = Interactions.RegisterGlobal(Item.Action or MakeKeys(Item))
   end
end

local function Clear()
   ActiveKeyTables = { }
end

Hotkeys = {
   Clear = Clear,
   Register = Register
}

