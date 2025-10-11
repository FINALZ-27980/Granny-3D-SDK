require "id_item_cache"
require "rect"
require "ui_button"
require "ui_itunesbox"
require "ui_hotkeys"
require "ui_preferences"
require "ipad_dialog"

local FontHandle    = UIPref.GetFont("button")
local FontH, FontBL = GetFontHeightAndBaseline(FontHandle)
local VPadding = 2
local HPadding = 8

BreadCrumbsHeight = FontH + 2*VPadding

local function MakeClick(Item, Index, PopLevels, ClickRect, Capture)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)
      Event = WaitForEvent(Event, LButtonUp, true)

      if (RectContainsPoint(ClickRect, Event.Point)) then
         if (PopLevels ~= 0) then
            Item.ActiveEditNode = nil
            Edit_PopContainerLevels(PopLevels)
         else
            local Pt = UIArea.TransformFromCapture(Capture, MakePoint(ClickRect.x, ClickRect.y))

            if (Item.ActiveEditNode) then
               Item.ActiveEditNode = nil
            else
               Item.ActiveEditNode = Edit_GetCurrentContainerID()
               Item.ActiveEditRect = ClickRect
            end
         end
      end

      Item.ClickActions[Index] = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.ClickActions[Index] = nil
                     end }
end


local LastLevels = nil
local ForwardLevels = nil

local function LevelsMatch(this, last)
   if (not this or not last) then
      return false
   elseif (#this ~= #last) then
      return false
   else
      for i,v in ipairs(this) do
         if (v ~= last[i]) then
            return false
         end
      end

      return true
   end
end


local function Back()
   local Levels = Edit_GetContainerStack()
   if (Levels == nil or #Levels == 1) then
      return
   end

   ForwardLevels = { NodeID = Edit_GetCurrentContainerID(), Next = ForwardLevels }
   Edit_PopContainerLevels(1)
   table.remove(Levels, #Levels)
   LastLevels = Levels
end

local function Forward()
   if (ForwardLevels == nil) then
      return
   end

   local This = ForwardLevels
   ForwardLevels = This.Next

   if (not Edit_DescendNode(This.NodeID)) then
      ForwardLevels = nil
   else
      LastLevels = Edit_GetContainerStack()
   end
end

local BreadCrumbHotkeys = nil
local function BCHKTable()
   if (BreadCrumbHotkeys == nil) then
      BreadCrumbHotkeys = {
         { KeyString   = "Alt-Left or Back",
           Description = "Back one level",
           EventSpecs   = { { Type = BackButtonDown },
                            { Type = CtrlKeyDown, Key = Control_KeyBackward },
                            { Type = DirKeyDown, Key = Direction_KeyLeft, Alt = true } }, 
           Fn          = Back },

         { KeyString   = "Alt-Right or Forward",
           Description = "Forward one level",
           EventSpecs   = { { Type = ForwardButtonDown },
                            { Type = CtrlKeyDown, Key = Control_KeyForward },
                            { Type = DirKeyDown, Key = Direction_KeyRight, Alt = true } }, 
           Fn          = Forward }
      }
   end

   return BreadCrumbHotkeys
end

function BreadCrumbs(ID)
   local Item, IsNew = IDItem.Get(ID)

   if (not Item.ClickActions) then
      Item.ClickActions = { }
   end
   Item.Capture = UIArea.Capture()

   -- Clear to the bg color
   DrawRect(UIArea.Get(), UIPref.GetColor("tabview_bg"))

   local Levels = Edit_GetContainerStack()
   if (not LevelsMatch(Levels, LastLevels)) then
      ForwardLevels = nil
   end
   LastLevels = Levels

   local CurrX = 15
   local Y     = VPadding + FontBL
   local Cap   = UIArea.Capture()

   for i,lvl in ipairs(Levels) do
      if (i ~= 1) then
         local interw = GetSimpleTextDimension(FontHandle, "::")
         RenderDropText(FontHandle, "::", 0,
                        MakePoint(CurrX, Y),
                        UIPref.GetColor("label_text"),
                        UIPref.GetColor("label_text_drop"))
         CurrX = CurrX + interw + HPadding
      end

      local w = GetSimpleTextDimension(FontHandle, lvl)
      if (not Interactions.Active()) then
         local ClickRect = MakeRect(CurrX, VPadding, w, FontH)

         Item.ClickActions[i] =
            Interactions.RegisterClipped(Item.ClickActions[i] or MakeClick(Item, i, (#Levels - i), ClickRect, Cap),
                                         ClickRect)
      end

      RenderDropText(FontHandle, lvl, 0,
                     MakePoint(CurrX, Y),
                     UIPref.GetColor("label_text"),
                     UIPref.GetColor("label_text_drop"))
      CurrX = CurrX + w + HPadding

   end

   -- see if we need to close and ignore...
   if (Item.ActiveEditNode and ShouldCloseEdits()) then
      Item.ActiveEditNode = false
   end

   if (Item.ActiveEditNode) then
      DeferredRender.Register(
         function ()
            local UL, ClippedUL =
               UIAreaTransformFromCapture(Item.Capture, MakePoint(Item.ActiveEditRect.x, Item.ActiveEditRect.y))
            local LL, ClippedLL =
               UIAreaTransformFromCapture(Item.Capture, MakePoint(Item.ActiveEditRect.x, Item.ActiveEditRect.y + Item.ActiveEditRect.h))

            if (not (ClippedUL or ClippedLL)) then
               Item.EditDesiredRect = IPadDialog.DoTop(IDItem.CreateSub(ID, Item.ActiveEditNode .. "editbox"),
                                                       Item.ActiveEditNode,
                                                       UL.x + math.floor(Item.ActiveEditRect.w/2), UL.y + Item.ActiveEditRect.h,
                                                       475, 600,
                                                       Item.EditDesiredRect)
            end
         end)
   else
      Item.EditDesiredRect = nil
   end

   Hotkeys.Register(IDItem.MakeID("BreadCrumbs", "Hotkeys"),
                    "Hierarchy", BCHKTable())
end
