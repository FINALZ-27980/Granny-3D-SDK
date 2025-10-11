-- Button plus default interaction
require "event"
require "point"
require "id_item"
require "interactions"
require "deferred_rendering"
require "drag_context"
require "rect"
require "ui_pref"

local GridPressed = NineGrid_GetHandle("button_pressed")
local GridRest    = NineGrid_GetHandle("button_rest")

local FontHandle    = UIPref.GetFont("button")
local FontH, FontBL = GetFontHeightAndBaseline(FontHandle)

local SmallFontHandle = UIPref.GetFont("small_button")
local SmallFontH, SmallFontBL = GetFontHeightAndBaseline(SmallFontHandle)

local LBMBFontHandle        = UIPref.GetFont("lbmbutton_text_below")
local LBMBFontH, LBMBFontBL = GetFontHeightAndBaseline(LBMBFontHandle)

local LBMSFontHandle        = UIPref.GetFont("lbmbutton_text_below")
local LBMSFontH, LBMSFontBL = GetFontHeightAndBaseline(LBMSFontHandle)


local function MakeClick(Item, Action)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)
      Item.Modifiers = { Shift = Event.Shift,
                         Ctrl  = Event.Ctrl,
                         Alt   = Event.Alt }
                         

      Item.Active     = true
      Item.ButtonDown = true
      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 Item.ButtonDown = RectContainsPoint(Item.Area, Event.Point)
                              end
                           end)

      -- We put this first in case the action kicks off a modal dialog.  That way the
      -- button pops up before entering the dialog...
      Item.Active = false
      if (Item.ButtonDown and Action) then
         Action(Item.Modifiers)
      end

      Item.ButtonClicked = Item.ButtonDown
      Item.ClickAction   = nil
      Item.ButtonDown    = false
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.ButtonClicked = false
                        Item.ButtonDown = false
                        Item.ClickAction = nil
                     end }
end

local function ButDo(ID, Rect, Text, Action, Disabled, FHandle, FHeight, FBaseLine)
   -- Obtain the cached state for this button
   local Item,IsNew = IDItem.Get(ID)
   Item.Area = Rect

   -- Register the interaction, recreating if necessary
   if (not Interactions.Active() and not Disabled) then
      Item.ClickAction = Interactions.RegisterClipped(Item.ClickAction or MakeClick(Item, Action), Item.Area)
   end

   -- Draw the button
   local Grid = GridRest
   if (Item.ButtonDown) then
      Grid = GridPressed
   end
   NineGrid_Draw(Grid, Rect)

   UIArea.ProtectedSimplePush(
      MakeRect(Rect.x + 2, Rect.y + 2, Rect.w - 4, Rect.h - 4),
      function()
         local w = GetSimpleTextDimension(FHandle, Text)
         local AtX = math.floor((UIArea.Get().w/2) - (w/2))
         local AtY = (UIArea.Get().h - (FHeight - FBaseLine) - 1)

         local c, cd
         
         if (not Disabled) then
            c  = UIPref.GetColor("button_text")
            cd = UIPref.GetColor("button_text_drop")
         else
            c  = UIPref.GetColor("button_disabled_text")
            cd = UIPref.GetColor("button_disabled_text_drop")
         end
         RenderDropText(FHandle, Text, 0,
                        MakePoint(AtX, AtY), c, cd)
      end)

   -- Return the clicked variable and clear it
   local RetVal
   RetVal, Item.ButtonClicked = Item.ButtonClicked, nil
   return RetVal
end

local function DoBitmap(ID, Rect, Handle, Action)
   -- Obtain the cached state for this button
   local Item,IsNew = IDItem.Get(ID)
   Item.Area = Rect
   Item.Bitmap = Handle

   -- Register the interaction, recreating if necessary
   if (not Interactions.Active()) then
      Item.ClickAction = Interactions.RegisterClipped(Item.ClickAction or MakeClick(Item, Action), Item.Area)
   end

   UIArea.ProtectedSimplePush(Rect,
                              function()
                                 local R = UIArea.Get()
                                 if (UIAreaRectContainsMouse(R)) then
                                    if ((not Interactions.Active()) or Item.Active) then
                                       DrawRect(R, UIPref.GetColor("bmbutton_alpha"))
                                       DrawRectOutline(R, UIPref.GetColor("bmbutton_outline"))
                                    end
                                 end

                                 if (Item.ButtonDown) then
                                    UIBitmap_Draw(Item.Bitmap, MakePoint(2, 2))
                                 else
                                    UIBitmap_Draw(Item.Bitmap, MakePoint(1, 1))
                                 end
                              end)

   -- Return the clicked variable and clear it
   local RetVal
   RetVal, Item.ButtonClicked = Item.ButtonClicked, nil
   return RetVal
end

local function DoLabeledBitmap(ID, Rect, BitmapHandle, Label, IsBelow, Disabled, Action)
   -- Obtain the cached state for this button
   local Item,IsNew = IDItem.Get(ID)
   Item.Area = Rect

   Item.Bitmap                = BitmapHandle
   Item.BitmapW, Item.BitmapH = UIBitmap_Dimensions(Item.Bitmap)
   Item.BitmapHOff = math.floor((Rect.h - LBMBFontH) - Item.BitmapH)

   -- Register the interaction, recreating if necessary
   if (not Disabled and not Interactions.Active()) then
      Item.ClickAction = Interactions.RegisterClipped(Item.ClickAction or MakeClick(Item, Action), Item.Area)
   end

   if (UIAreaRectContainsMouse(Rect)) then
      if (((not Interactions.Active()) or Item.Active) and not Disabled) then
         DrawRect(Rect, UIPref.GetColor("bmbutton_alpha"))
         DrawRectOutline(Rect, UIPref.GetColor("bmbutton_outline"))
      end
   end

   if (IsBelow) then
      UIArea.ProtectedSimplePush(Rect,
                                 function()
                                    local OffX = math.floor((Rect.w - Item.BitmapW) / 2)

                                    if (Item.ButtonDown) then
                                       UIBitmap_Draw(Item.Bitmap, MakePoint(OffX + 1, Item.BitmapHOff + 1))
                                    else
                                       UIBitmap_Draw(Item.Bitmap, MakePoint(OffX , Item.BitmapHOff))
                                    end

                                    local TextRect = MakeRect(1, Rect.h - LBMBFontH, Rect.w - 2, LBMBFontH)

                                    RenderTextClipped(LBMBFontHandle, Label, FontCentered,
                                                      TextRect, UIPref.GetColor("lbmbutton_text_below"))
                                 end)
   else
      UIArea.ProtectedSimplePush(Rect,
                                 function()
                                    local OffY = math.floor((Rect.h - Item.BitmapH) / 2)

                                    if (Item.ButtonDown) then
                                       UIBitmap_Draw(Item.Bitmap, MakePoint(2, OffY + 1))
                                    else
                                       UIBitmap_Draw(Item.Bitmap, MakePoint(1, OffY))
                                    end

                                    local w = GetSimpleTextDimension(LBMSFontHandle, Label)
                                    local AtX = Item.BitmapW + 1
                                    local AtY = math.floor((UIArea.Get().h - LBMSFontH + 2 * LBMSFontBL)/2)

                                    if (not Item.ButtonDown) then
                                       AtY = AtY - 1
                                    else
                                       AtX = AtX + 1
                                    end

                                    RenderText(LBMSFontHandle, Label, 0,
                                               MakePoint(AtX, AtY),
                                               UIPref.GetColor("lbmbutton_text_side"))
                                 end)
   end

   -- Return the clicked variable and clear it
   local RetVal
   RetVal, Item.ButtonClicked = Item.ButtonClicked, nil
   return RetVal
end



local function MakeDragClick(Item, Action, DragAction, DragType)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.Active     = true
      Item.ButtonDown = true
      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 if (Item.ButtonDown) then
                                    if (not RectContainsPoint(Item.Area, Event.Point)) then
                                       Item.ButtonDown = false
                                       DragContext.StartDrag(DragType,
                                                             UIArea.TransformFromCapture(Item.AreaCapture,
                                                                                         Event.Point))
                                    end
                                 else
                                    local NewPoint = UIArea.TransformFromCapture(Item.AreaCapture, Event.Point)
                                    DragContext.CurrentPoint(NewPoint)
                                 end
                              end
                           end)

      if (Item.ButtonDown) then
         if (Action) then
            Action()
         end
      else
         if (DragAction) then
            local Targets = DragContext.GetItems(DragType)
            Stack.Iterate(Targets,
                          function (Target)
                             local TargetRelPt = DragContext.TargetContainsPoint(Target)
                             if (TargetRelPt) then
                                DragAction(Target.Value, TargetRelPt)
                                return true
                             end
                          end)
         end
         DragContext.EndDrag(DragType)
      end

      Item.Active        = false
      Item.ButtonClicked = Item.ButtonDown
      Item.ClickAction   = nil
      Item.ButtonDown    = false
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.ButtonClicked = false
                        Item.ButtonDown = false
                        Item.ClickAction = nil
                     end }
end

local function DoLabeledBitmapDragger(ID, Rect, BitmapHandle, Label, IsBelow, Disabled, Action, DragAction, DragType)
   -- Obtain the cached state for this button
   local Item,IsNew = IDItem.Get(ID)
   Item.Area = Rect

   Item.Bitmap                = BitmapHandle
   Item.BitmapW, Item.BitmapH = UIBitmap_Dimensions(Item.Bitmap)
   Item.BitmapHOff = math.floor(((Rect.h - LBMBFontH) - Item.BitmapH)/2)
   Item.AreaCapture           = UIArea.Capture()

   -- Register the interaction, recreating if necessary
   if (not Disabled and not Interactions.Active()) then
      Item.ClickAction =
         Interactions.RegisterClipped(Item.ClickAction or MakeDragClick(Item, Action, DragAction, DragType), Item.Area)
   end

   if (UIAreaRectContainsMouse(Rect)) then
      if (((not Interactions.Active()) or Item.Active) and not Disabled) then
         DrawRect(Rect, UIPref.GetColor("bmbutton_alpha"))
         DrawRectOutline(Rect, UIPref.GetColor("bmbutton_outline"))
      end
   end

   if (IsBelow) then
      UIArea.ProtectedSimplePush(Rect,
                                 function()
                                   local OffX = math.floor((Rect.w - Item.BitmapW) / 2)

                                    if (Item.ButtonDown) then
                                       UIBitmap_Draw(Item.Bitmap, MakePoint(OffX + 1, Item.BitmapHOff + 1))
                                    else
                                       UIBitmap_Draw(Item.Bitmap, MakePoint(OffX , Item.BitmapHOff))
                                    end

                                    local TextRect = MakeRect(1, Rect.h - LBMBFontH, Rect.w - 2, LBMBFontH)

                                    RenderTextClipped(LBMBFontHandle, Label, FontCentered,
                                                      TextRect, UIPref.GetColor("lbmbutton_text_below"))
                                 end)
   else
      UIArea.ProtectedSimplePush(Rect,
                                 function()
                                    local OffY = math.floor((Rect.h - Item.BitmapH) / 2)

                                    if (Item.ButtonDown) then
                                       UIBitmap_Draw(Item.Bitmap, MakePoint(2, OffY + 1))
                                    else
                                       UIBitmap_Draw(Item.Bitmap, MakePoint(1, OffY))
                                    end

                                    local w = GetSimpleTextDimension(LBMSFontHandle, Label)
                                    local AtX = Item.BitmapW + 1
                                    local AtY = math.floor((UIArea.Get().h - LBMSFontH + 2 * LBMSFontBL)/2)

                                    if (not Item.ButtonDown) then
                                       AtY = AtY - 1
                                    else
                                       AtX = AtX + 1
                                    end

                                    RenderText(LBMSFontHandle, Label, 0,
                                               MakePoint(AtX, AtY),
                                               UIPref.GetColor("lbmbutton_text_side"))
                                 end)
   end

    if (Item.Active and DragContext.IsActive(DragType)) then
       local St, Pt = DragContext.GetDragPoints()
       DeferredRender.Register(
          function ()
             UIBitmap_Draw(Item.Bitmap, Pt)
          end)
    end

   -- Return the clicked variable and clear it
   local RetVal
   RetVal, Item.ButtonClicked = Item.ButtonClicked, nil
   return RetVal
end




local function Dim(Text)
   local w = GetSimpleTextDimension(FontHandle, Text)

   -- we want internal padding in the field of 2 pixels, plus 2 pixels for the beveled
   -- border
   return w + 28, 26
end

local function DimSmall(Text)
   local w = GetSimpleTextDimension(SmallFontHandle, Text)

   -- we want internal padding in the field of 2 pixels, plus 2 pixels for the beveled
   -- border
   return w + 20, 22
end

local function BitmapDim(BitmapHandle)
   local w,h  = UIBitmap_Dimensions(BitmapHandle)
   return w+4,h+4
end

local function LabeledBitmapDim(BitmapHandle, Label, MinHeight, IsBelow)
   -- todo: could cache a lot of this
   local bw,bh = UIBitmap_Dimensions(BitmapHandle)
   bh = math.max(MinHeight, bh)

   if (IsBelow) then
      local w = GetSimpleTextDimension(LBMBFontHandle, Label)

      -- We need a padding of 2 on the max of the width, and the sum of the heights
      local MaxW = w
      if (MaxW < bw) then
         MaxW = bw
      end

      return (MaxW + 4), ((bh + LBMBFontH) + 4)
   else
      local w = GetSimpleTextDimension(LBMSFontHandle, Label)

      -- We need a padding of 2 on the max of the height, and the sum of the widths
      local MaxH = bh
      if (MaxH < h) then
         MaxH = LBMSFontH
      end

      return ((bw + w) + 4), (MaxH + 4)
   end
end

Button = {
   Do = function(ID, Rect, Text, Disabled, Action)
           return ButDo(ID, Rect, Text, Action, Disabled,
                        FontHandle, FontH, FontBL)
        end,

   DoArgs = function(ID, Rect, args)
               local Text   = args.Text
               local Action = args.Action
               local Disabled = args.Disabled or false
               return ButDo(ID, Rect, Text, Action, Disabled,
                            FontHandle, FontH, FontBL)
            end,

   DoSmall = function(ID, Rect, Text, Disabled, Action)
                return ButDo(ID, Rect, Text, Action, Disabled,
                             SmallFontHandle, SmallFontH, SmallFontBL)
             end,

   DoSmallArgs = function(ID, Rect, args)
                    local Text   = args.Text
                    local Action = args.Action
                    local Disabled = args.Disabled or false
                    return ButDo(ID, Rect, Text, Action, Disabled,
                                 SmallFontHandle, SmallFontH, SmallFontBL)
                 end,

   Dim = Dim,
   DimSmall = DimSmall,
   DoBitmap  = DoBitmap,
   BitmapDim = BitmapDim,

   DoLabeledBitmap        = DoLabeledBitmap,
   DoLabeledBitmapDragger = DoLabeledBitmapDragger,
   LabeledBitmapDim       = LabeledBitmapDim,
}

