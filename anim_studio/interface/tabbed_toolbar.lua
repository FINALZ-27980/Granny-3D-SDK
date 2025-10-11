require "ui_base"
require "ui_preferences"

local FontHandle = UIPref.GetFont("tabview_tab")
local FontHeight, FontBaseline = GetFontHeightAndBaseline(FontHandle)

local TextColor     = UIPref.GetColor("tabview_text")
local TextDropColor = UIPref.GetColor("tabview_text_drop")

local VertPadding  = 4
local HorizPadding = 10

local function MakeToggle(Item, Rect, SetVal)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      local ButtonDown = true
      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 Item.ButtonDown = RectContainsPoint(Rect, Event.Point)
                              end
                           end)

      if (ButtonDown) then
         Item.ToolbarActive = not Item.ToolbarActive
      end

      Item.ToggleAction = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.ToggleAction = nil
                     end }
end


local function GetToggleRect(Item, Rect, BindEdge, ToggleText, ToolbarRect, Active)
   local NewRect = nil
   if (BindEdge == TabbedToolbar.Top) then
      NewRect = MakeRect(math.floor((Rect.x + Rect.w - Item.ToggleWidth) / 2),
                         Rect.y,
                         Item.ToggleWidth + 2*HorizPadding, Item.ToggleHeight + 2*VertPadding)

      
      if (Active) then
         -- Shift down by toolbarrect height
         NewRect.y = NewRect.y + ToolbarRect.h
      end
   elseif (BindEdge == TabbedToolbar.Left) then
      error("not yet")
   elseif (BindEdge == TabbedToolbar.Bottom) then
      NewRect = MakeRect(math.floor((Rect.x + Rect.w - Item.ToggleWidth) / 2),
                         Rect.y + Rect.h - Item.ToggleHeight,
                         Item.ToggleWidth, Item.ToggleHeight)
      if (Active) then
         -- Shift up by toolbarrect height
         NewRect.y = NewRect.y - ToolbarRect.h
      end
   elseif (BindEdge == TabbedToolbar.Right) then
      error("not yet")
   end

   return NewRect
end

local function GetActiveChildRect(Rect, ToolbarRect, BindEdge)
   if (BindEdge == TabbedToolbar.Top) then
      return MakeRect(0, 0, Rect.w, ToolbarRect.h)
   elseif (BindEdge == TabbedToolbar.Left) then
      return MakeRect(0, 0, ToolbarRect.w, Rect.h)
   elseif (BindEdge == TabbedToolbar.Right) then
      return MakeRect((Rect.x + Rect.w) - ToolbarRect.w,
                      0,
                      ToolbarRect.w, ToolbarRect.h)
   elseif (BindEdge == TabbedToolbar.Bottom) then
      return MakeRect(0,
                      (Rect.y + Rect.h) - ToolbarRect.h,
                      ToolbarRect.w, ToolbarRect.h)
   else
      error("unknown bind edge")
   end
end


local function Do(ID, BindEdge, ToggleText, ToggleTip, ToolbarRect, ToolbarFn)
   local Item, IsNew = IDItem.Get(ID)
   if (IsNew) then
      Item.ToggleWidth, Item.ToggleHeight = GetSimpleTextDimension(FontHandle, ToggleText)
   end

   local Rect = UIArea.Get()

   local ToggleRect = GetToggleRect(Item, Rect, BindEdge, ToggleText, ToolbarRect, Item.ToolbarActive)
   if (Item.ToolbarActive) then
      local ChildRect  = GetActiveChildRect(Rect, ToolbarRect, BindEdge)

      UIArea.ProtectedSimplePush(
         ChildRect,
         function()
            DrawRect(UIArea.Get(), UIPref.GetColor("toolbar_bg"))
            if (ToolbarFn) then
               ToolbarFn(IDItem.CreateSub(ID, "bar"))
            end
         end)
   end

   if (not Interactions.Active()) then
      Item.ToggleAction =
         Interactions.RegisterClipped(Item.ToggleAction or MakeToggle(Item, ToggleRect, true),
                                      ToggleRect)
   end

   if (BindEdge == TabbedToolbar.Bottom) then
      NineGrid_Draw(NineGrid_GetHandle("tabview_top_active"), ToggleRect)
   elseif (BindEdge == TabbedToolbar.Top) then
      NineGrid_Draw(NineGrid_GetHandle("tabview_bottom_active"), ToggleRect)
   else
      error("nyi")
   end

   RenderDropText(FontHandle, ToggleText, 0,
                  MakePoint(ToggleRect.x + HorizPadding,
                            ToggleRect.y + FontBaseline + 3),
                  TextColor, TextDropColor)

   
   if (ToggleTip) then
      ToolTip.Register(ToggleTip, ToggleRect)
   end
end

TabbedToolbar = {
   Do = Do,

   Top    = 1,
   Bottom = 2,
   Right  = 3,
   Left   = 4
}

