require "event"
require "math"
require "ui_autoplacer"
require "ui_base"
require "ui_hotkeys"
require "ui_preferences"
require "ui_prefs"
require "ui_scrollarea"

local FontHandle = UIPref.GetFont("tabview_tab")
local FontHeight, FontBaseline = GetFontHeightAndBaseline(FontHandle)

local VertPadding  = 4
local HorizPadding = 10
local PixelsPerSecond = 250

local QuickSlotsButWidth  = 100
local QuickSlotsButHeight = FontHeight + 2*VertPadding

local TextColor     = UIPref.GetColor("tabview_text")

local SMBitmap = UIPref.GetBitmap("icon_state_machine")
local BGBitmap = UIPref.GetBitmap("icon_blend_graph")
local ASBitmap = UIPref.GetBitmap("icon_anim_source")

local MinHeight       = 100
local MinWidth        = 300
local FilterWidth     = 200
local MaxSavedFilters = 10

local function MakeResizeClick(Item, Rect)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      local StartY = Event.Point.y
      local StartHeight = Item.Height

      if (math.abs(Event.Point.y - Rect.h) < 5) then
         Event =
            WaitForEvent(
            Event, LButtonUp, true,
            function (Event)
               if (Event.Point) then
                  Item.Height = StartHeight + (Event.Point.y - StartY)
                  Item.Height = math.max(Item.Height, MinHeight)
               end
            end)
      else
         Event = WaitForEvent(Event, LButtonUp, true)
      end

      Item.ResizeClick = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.EatClick = nil
                     end }
end

local function MakeSetQuickSlots(Item, Rect, SetVal)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.ButtonDown = true
      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 Item.ButtonDown = RectContainsPoint(Rect, Event.Point)
                              end
                           end)

      if (Item.ButtonDown) then
         Item.ShowSlots = not Item.ShowSlots
      end

      Item.ButtonClick = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.ButtonClick = nil
                        Item.ButtonDown = false
                     end }
end

local function MakeSlotDrag(Item, SlotsDisplay, Type)
   local function SourceAction(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.DraggingSource = true
      Item.DraggingShift = Event.Shift
      Item.DraggingCtrl = Event.Ctrl

      Item.StoreShow = SlotsDisplay.ShowSlots
      SlotsDisplay.ShowSlots = false

      local StartPt = MakePoint(Event.Point.x, Event.Point.y)
      DragContext.StartDrag(Type, UIArea.TransformFromCapture(Item.AreaCapture, Event.Point))
      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 local NewPoint = UIArea.TransformFromCapture(Item.AreaCapture, Event.Point)
                                 DragContext.CurrentPoint(NewPoint)
                              end
                           end)

      local Targets = DragContext.GetItems(Type)
      Stack.Iterate(Targets,
                    function (Target)
                       local TargetRelPt = DragContext.TargetContainsPoint(Target)
                       if (TargetRelPt) then
                          Item.StoreShow = false

                          if (Event.Shift and not Event.Ctrl) then
                             local BG = Edit_AddNodeAtPt("blend_graph", Item.SlotName, TargetRelPt)
                             if (Edit_DescendNode(BG)) then
                                local Anim =  Edit_AddNodeAtPt("anim_source", Item.SlotName, MakePoint(100, 125))
                                SetAnimationSlotForNode(Anim, Item.SlotName)

                                -- Wire to the output
                                Edit_ConnectNodes(Anim, 0, BG, Edit_GetInternalInput(BG, 0))

                                Edit_PopContainerLevels(1)
                             end
                          elseif (Event.Ctrl) then
                             local SM = Edit_AddNodeAtPt("state_machine", Item.SlotName, TargetRelPt)
                             if (Edit_DescendNode(SM)) then
                                local Anim =  Edit_AddNodeAtPt("anim_source", Item.SlotName, MakePoint(100, 125))
                                SetAnimationSlotForNode(Anim, Item.SlotName)
                                Edit_PopContainerLevels(1)
                             end
                          else
                             local Node = Edit_AddNodeAtPt("anim_source", Item.SlotName, TargetRelPt)
                             SetAnimationSlotForNode(Node, Item.SlotName)
                          end
                          return true
                       end
                    end)

      SlotsDisplay.ShowSlots = Item.StoreShow
      DragContext.EndDrag(Type)
      Item.DraggingSource = false
      Item.SlotDrag = nil
   end

   return { Action = coroutine.create(SourceAction),
            Abort = function ()
                       if (Item.DraggingSource) then
                          DragContext.EndDrag(Type)
                          SlotsDisplay.ShowSlots = Item.StoreShow
                       end

                       Item.DraggingSource = false
                       Item.DraggingShift = false
                       Item.DraggingCtrl = false
                       Item.SlotDrag = nil
                    end }
end

local QPosted = false
local QuickSlotsHotkeys = {
   { KeyString   = "Q",
     Description = "Open/Close Quick Slots",
     EventSpecs  = { { Type = KeyDown, Key = KeyForChar("q") } },
     Fn          = function() QPosted = true end }
}

local function NameMatchesFilter(Filter, Name)
   if (Filter ~= nil and string.find(string.lower(Name), string.lower(Filter), 1, true) == nil) then
      return false
   end

   return true
end

local function DoSlotItems(ID, Item)
   local AreaCapture = UIArea.Capture()

   local MaxW = 0
   local Slots = Edit_GetAnimationSlots()
   for i,v in ipairs(Slots) do
      MaxW = math.max(MaxW, GetSimpleTextDimension(FontHandle, v.Name))
   end

   local A = UIArea.Get()
   local Placement = AutoPlacement.New(AutoPlacement.Horizontal, MakeRect(0, 0, A.w, A.h), 5)
   for i,v in ipairs(Slots) do
      if (NameMatchesFilter(Item.CurrFilter, v.Name)) then
         local SlotItem, IsNew = IDItem.Get(IDItem.CreateSub(ID, i .. " " .. v.Name))
         local Rect = Placement:Next(MaxW, FontHeight)

         SlotItem.SlotName = v.Name
         SlotItem.AreaCapture = AreaCapture
         if (not Interactions.Active()) then
            SlotItem.SlotDrag =
               Interactions.RegisterClipped(SlotItem.SlotDrag or
                                            MakeSlotDrag(SlotItem, Item, DragTypeSlot),
                                         Rect)
         end

         RenderText(FontHandle, v.Name, 0, MakePoint(Rect.x, Rect.y + FontBaseline), MakeColor(1, 1, 1))
      end
   end

   local x, y = Placement:CurrentDim()
   return MakeRect(0, 0, x, y), true
end

function split(str, pat)
   local t = {}  -- NOTE: use {n = 0} in Lua-5.0
   local fpat = "(.-)" .. pat
   local last_end = 1
   local s, e, cap = str:find(fpat, 1)
   while s do
      if s ~= 1 or cap ~= "" then
     table.insert(t,cap)
      end
      last_end = e+1
      s, e, cap = str:find(fpat, last_end)
   end
   if last_end <= #str then
      cap = str:sub(last_end)
      table.insert(t, cap)
   end
   return t
end

local function GetFilterStrings()
   local Joined = App_GetPersistentString("QuickSlotFilters", "")
   return split(Joined, ";")
end

local function StoreFilterStrings(StringArray)
   local StoreString = ""
   if (StringArray ~= nil) then
      StoreString = table.concat(StringArray, ";")
   end

   App_SetPersistentString("QuickSlotFilters", StoreString)
end

local function AddNewFilterString(NewString)
   local Filters = GetFilterStrings()

   -- If already the front, ignore the operation
   if (#Filters > 0 and (string.lower(Filters[1]) == string.lower(NewString))) then
      return
   end

   local FoundIdx = -1
   for i,v in ipairs(Filters) do
      if (string.lower(v) == string.lower(NewString)) then
         FoundIdx = i
      end
   end

   if (FoundIdx ~= -1) then
      table.remove(Filters, FoundIdx)
   end

   table.insert(Filters, 1, NewString)
   while (#Filters > MaxSavedFilters) do
      table.remove(Filters, MaxSavedFilters+1)
   end

   StoreFilterStrings(Filters)
end

local function Do(ID)
   local Item, IsNew = IDItem.Get(ID)
   if (IsNew) then
      Item.Curr = 0
      Item.Height = math.max(MinHeight, math.floor(UIArea.Get().h / 3))
      Item.ScrollOffset = MakePoint(0, 0)
      Item.CurrFilter = nil
   end

   if (not Interactions.Active() and not DragContext.IsActive(DragTypeSlot)) then
      if (QPosted) then
         Item.ShowSlots = not Item.ShowSlots
      end
   end
   QPosted = false

   Item.BaseArea = UIArea.Get()
   local Width   = math.max(math.floor(Item.BaseArea.w * 0.8), MinWidth);
   local XOffset = math.floor(Item.BaseArea.w * 0.1)

   Item.HiddenButtonRect = MakeRect(math.floor(Item.BaseArea.x + Item.BaseArea.w/2 - QuickSlotsButWidth/2),
                                    Item.BaseArea.y,
                                    QuickSlotsButWidth, QuickSlotsButHeight)
   Item.ShownButtonRect = MakeRect(Item.BaseArea.x + Item.BaseArea.w/2 - QuickSlotsButWidth/2,
                                   Item.BaseArea.y + Item.Height,
                                   QuickSlotsButWidth, QuickSlotsButHeight)
   Item.DrawerArea     = MakeRect(Item.BaseArea.x + XOffset, Item.BaseArea.y, Width, Item.Height)
   Item.FilterArea     = MakeRect(Item.BaseArea.x + XOffset, Item.BaseArea.y, FilterWidth, Item.Height)

   local bw, bh = Button.DimSmall("X");
   Item.ClearFilter    = MakeRect(FilterWidth - 2 - bw, 5, bw, bh)
   Item.EditRect       = MakeRect(2, 5, FilterWidth - 6 - bw, Editbox.RecommendedHeight(FontHandle))
   Item.QuickSlotsRect = MakeRect(Item.BaseArea.x + XOffset + FilterWidth + 2, Item.BaseArea.y,
                                  Width - FilterWidth, Item.Height)
   if (Item.ShowSlots) then
      if (not Interactions.Active()) then
         Item.ResizeClick =
            Interactions.RegisterClipped(Item.ResizeClick or MakeResizeClick(Item, Item.DrawerArea),
                                         Item.DrawerArea)
         Item.ButtonClick =
            Interactions.RegisterClipped(Item.ButtonClick or MakeSetQuickSlots(Item, Item.ShownButtonRect, false),
                                         Item.ShownButtonRect)
      end

      NineGrid_Draw(NineGrid_GetHandle("combobox_menu"), Item.DrawerArea)
      NineGrid_Draw(NineGrid_GetHandle("combobox_menu"), Item.FilterArea)

      local SubRect = CopyRect(Item.QuickSlotsRect)
      SubRect.x = Item.QuickSlotsRect.x + 4
      SubRect.y = Item.QuickSlotsRect.y + 4
      SubRect.w = Item.QuickSlotsRect.w - 8
      SubRect.h = Item.QuickSlotsRect.h -8

      UIArea.ProtectedSimplePush(
         Item.FilterArea,
         function ()
            Button.DoSmall(IDItem.CreateSub(ID, "clear_filter"),
                           Item.ClearFilter, "X", false,
                           function()
                              Item.CurrFilter = ""
                           end)
            ToolTip.Register("Clear the filter", Item.ClearFilter)

            local NewString, Changed, Success =
               Editbox.DoWithArgs(IDItem.CreateSub(ID, "filter"),
                                  Item.EditRect,
                                  Item.CurrFilter,
                                  FontHandle,
                                  true, { IgnoreFirstClick = true,
                                          EventCallback =
                                             function (EditItem, Event)
                                                local Text, SelStart, SelEnd, Cursor = STB_GetTextParams(EditItem.STBEdit)
                                                Item.CurrFilter = Text
                                             end })
            if (Changed or Success) then
               AddNewFilterString(NewString)
               Item.CurrFilter = NewString
            end
            ToolTip.Register("Filter the animation slots.\n[Enter] stores the filter for\nlater use.", Item.EditRect)

            local Filters = GetFilterStrings()
            local CurrY = Item.EditRect.y + Item.EditRect.h + 10
            for i,v in ipairs(Filters) do
               local ButtonRect = MakeRect(7, CurrY, Item.EditRect.w - 10, 22)
               Button.DoSmall(IDItem.CreateSub(ID, "filter" .. i .. v),
                              ButtonRect,
                              v, false,
                              function ()
                                 Item.CurrFilter = v
                              end)

               local DelButtonRect = MakeRect(Item.ClearFilter.x, CurrY, Item.ClearFilter.w, Item.ClearFilter.h)
               Button.DoSmall(IDItem.CreateSub(ID, "del_filter" .. i .. v),
                              DelButtonRect, "X", false,
                              function()
                                 -- note that this is called after the loop, so it's ok to
                                 -- modify filters here.
                                 table.remove(Filters, i)
                                 StoreFilterStrings(Filters)
                              end)
               ToolTip.Register("Remove this stored filter", DelButtonRect)

               CurrY = CurrY + 22 + 5 -- magic number
               if (CurrY > (Item.FilterArea.h - 22)) then
                  break
               end
            end
         end)

      UIArea.ProtectedSimplePush(
         Item.QuickSlotsRect,
         function()
            Item.ScrollOffset =
               ScrollArea.DoNoHorizontal(
               IDItem.CreateSub(ID, "scroll"),
               Item.ScrollOffset,
               function (LocalID)
                  -- use the outer id to match below!
                  return DoSlotItems(ID, Item)
               end)
         end)
      ToolTip.Register(("Drag to create animation source.\n"   ..
                        "Shift-drag to create a blend-graph\n" ..
                        "Ctrl-drag to create a state machine"), Item.QuickSlotsRect)

      NineGrid_Draw(NineGrid_GetHandle("tabview_bottom_active"), Item.ShownButtonRect)

      local TextRect = CopyRect(Item.ShownButtonRect)
      TextRect.y = TextRect.y + 3
      RenderTextClipped(FontHandle, "Hide slots", FontCentered, TextRect, TextColor)
   else
      if (not Interactions.Active()) then
         Item.ButtonClick =
            Interactions.RegisterClipped(Item.ButtonClick or MakeSetQuickSlots(Item, Item.HiddenButtonRect, true),
                                         Item.HiddenButtonRect)
      end

      local Slots = Edit_GetAnimationSlots()
      for i,v in ipairs(Slots) do
         local SlotItem, IsNew = IDItem.Get(IDItem.CreateSub(ID, i .. " " .. v.Name))
         if (SlotItem.DraggingSource) then
            local St, Pt = DragContext.GetDragPoints()
            DeferredRender.Register(
               function ()
                  local shift,ctrl,alt = App_GetShiftCtrlAlt()

                  local Handle = ASBitmap
                  if (shift and not (ctrl or alt)) then
                     Handle = BGBitmap
                  elseif (ctrl and not (shift or alt)) then
                     Handle = SMBitmap
                  end

                  RenderText(FontHandle, SlotItem.SlotName, 0,
                             MakePoint(Pt.x + FontBaseline, Pt.y),
                             MakeColor(1, 1, 1))
                  UIBitmap_Draw(Handle, MakePoint(Pt.x + FontHandle, Pt.y))
               end)
         end
      end


      NineGrid_Draw(NineGrid_GetHandle("tabview_bottom_active"), Item.HiddenButtonRect)

      local TextRect = CopyRect(Item.HiddenButtonRect)
      TextRect.y = TextRect.y + 3
      RenderTextClipped(FontHandle, "Quick slots", FontCentered, TextRect, TextColor)
   end

   -- register hotkeys...
   Hotkeys.Register(IDItem.MakeID("QuickSlots", "Hotkeys"),
                    "Quick Slots",
                    QuickSlotsHotkeys)
end

QuickSlots = {
   Do = Do
}
