require "event"
require "id_item"
require "interactions"
require "rect"
require "ui_area"
require "ui_preferences"


local FontHandle               = UIPref.GetFont("tabview_tab")
local FontHeight, FontBaseline = GetFontHeightAndBaseline(FontHandle)

local VertPadding  = 4
local HorizPadding = 10

local TabHeight  = FontHeight + 2 * VertPadding

local TextColor     = UIPref.GetColor("tabview_text")
local TextDropColor = UIPref.GetColor("tabview_text_drop")



local function ComputeTabSizes(TabTable)
   local FontHandle = UIPref.GetFont("tabview_tab")

   local TabSizes = { }

   for i,n_fn in ipairs(TabTable) do
      local w,h = GetSimpleTextDimension(FontHandle, n_fn.Label)
      TabSizes[i] = w + 2 * HorizPadding
   end

   return TabSizes, Height, Baseline
end

local function ComputeTabRects(TabsRect, TabSizes, FontHeight)
   local TabRects = {}

   local CurrX = TabsRect.x
   for i,TabWidth in ipairs(TabSizes) do
      TabRects[i] = MakeRect(CurrX, TabsRect.y, TabWidth, FontHeight + 2*VertPadding)
      CurrX = CurrX + TabWidth
   end

   return TabRects
end


--      Item.TabAction = Item.TabAction or MakeTabClick(Item)
local function MakeTabClick(Item, HideWhenInactive)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      for i,r in ipairs(Item.TabRects) do
         if (RectContainsPoint(r, Event.Point)) then
            if (HideWhenInactive and Item.CurrentTab == i) then
               Item.CurrentTab = -1
            else
               Item.CurrentTab = i
            end
            break
         end
      end

      -- capture the action state, we'll just dump immediately (prevents the click from
      -- finding another control...)
      Interactions.Continue(true)
      Item.TabAction = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.TabAction = nil
                     end }
end


local function Do(ID, TabTable, CurrentTab, HideWhenInactive)
   local Item, IsNew = IDItem.Get(ID)
   if (IsNew) then
      Item.CurrentTab = CurrentTab
   end

   local BaseArea   = UIArea.Get()
   local TabArea    = MakeRect(0, BaseArea.h - TabHeight, BaseArea.w, TabHeight)
   local ClientArea = MakeRect(0, 0, BaseArea.w, BaseArea.h - TabArea.h)

   Item.TabTable = TabTable
   Item.TabSizes = ComputeTabSizes(TabTable)
   Item.TabRects = ComputeTabRects(TabArea, Item.TabSizes, FontHeight)


   -- Clear the area
   DrawRect(TabArea, UIPref.GetColor("tabview_bg"))
   DrawRect(MakeRect(TabArea.x, TabArea.y, TabArea.w, 1), UIPref.GetColor("tabview_border"))

   -- draw tabs
   local FontHandle = UIPref.GetFont("tabview_tab")

   if (not Interactions.Active()) then
      Item.TabAction = Interactions.RegisterClipped(Item.TabAction or MakeTabClick(Item, HideWhenInactive), TabArea)
   end

   local CurrX = 0
   for i,n_fn in ipairs(TabTable) do

      local TabWidth = Item.TabSizes[i]
      local TabRect  = Item.TabRects[i] -- MakeRect(CurrX, 2, TabWidth + 4, Item.FontHeight + 2)

      if (i == CurrentTab) then
         NineGrid_Draw(NineGrid_GetHandle("tabview_bottom_active"), TabRect)
      else
         NineGrid_Draw(NineGrid_GetHandle("tabview_bottom_inactive"), TabRect)
      end

      RenderDropText(FontHandle, n_fn.Label, 0, MakePoint(TabRect.x + HorizPadding, TabRect.y + FontBaseline + 3), TextColor, TextDropColor)
      if (n_fn.Tip) then
         ToolTip.Register(n_fn.Tip, TabRect)
      end
      
      CurrX = CurrX + TabWidth
   end

   -- Draw the current tab
   if (CurrentTab ~= -1) then
      UIArea.ProtectedSimplePush(ClientArea,
                                 function()
                                    DrawRect(UIArea.Get(), UIPref.GetColor("tabview_bg"))
                                    TabTable[CurrentTab].DrawFn(IDItem.CreateSub(ID, "client" .. CurrentTab))
                                 end)
   end

   return Item.CurrentTab
end

Tabview = {
   Do = Do,
   TabHeight = TabHeight
}
