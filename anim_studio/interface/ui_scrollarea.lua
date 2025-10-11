  require "rect"
require "ui_area"
require "ui_scrollbar"

local function ValidScrollArea(AreaRect)
   return ((AreaRect.w >= 2 * Scrollbar.VerticalWidth) and (AreaRect.h >= 2 * Scrollbar.VerticalWidth))
end

local function FullyContains(StartTest, LenTest, StartRegion, LenRegion)
   return false
   --- return ((StartTest >= StartRegion) and ((StartTest+LenTest) <= (StartRegion+LenRegion)))
end


local ScrollZoomFactors = {
   1.00,
   1.25,
   1.50,
   2.00,
   2.50,
   3.00,
   4.00,
   5.00,
   7.50
}

local function MakeZoomControl(Item)
   local function ZoomCoroutine(Event)
      local Event = WaitForEvent(Event, MouseWheel, false)

      local OldScale = ScrollZoomFactors[Item.ZoomClick]
      local PreservePoint = MakePoint(Event.Point.x / OldScale - Item.Offset.x,
                                      Event.Point.y / OldScale - Item.Offset.y)

      if (Event.Delta < 0) then
         Item.ZoomClick = math.min(#ScrollZoomFactors, Item.ZoomClick + 1)
      else
         Item.ZoomClick = math.max(1, Item.ZoomClick - 1)
      end

      local NewScale = ScrollZoomFactors[Item.ZoomClick]
      Item.Offset.x =  -(PreservePoint.x - ((PreservePoint.x + Item.Offset.x) * (OldScale / NewScale)))
      Item.Offset.y =  -(PreservePoint.y - ((PreservePoint.y + Item.Offset.y) * (OldScale / NewScale)))

      Item.ZoomAction = nil
   end

   return { Action = coroutine.create(ZoomCoroutine),
            Abort  = function ()
                        Item.ZoomAction = nil
                     end }
end

local function MakeWheelControl(Item)
   local function WheelCoroutine(Event)
      local Event = WaitForEvent(Event, MouseWheel, false)

      local Click = 20
      if (Event.Delta < 0) then
         Item.Offset.y = Item.Offset.y + Click
      else
         Item.Offset.y = Item.Offset.y - Click
      end

      if (Item.Offset.y < 0) then
         Item.Offset.y = 0
      elseif (Item.Offset.y + Item.RenderedArea.h > Item.ChildArea.h) then
         Item.Offset.y = math.max(Item.ChildArea.h - Item.RenderedArea.h, 0)
      end

      Item.ScrollwheelAction = nil
   end

   return { Action = coroutine.create(WheelCoroutine),
            Abort  = function ()
                        Item.ScrollwheelAction = nil
                     end }
end


-- ChildDraw should be a function that returns the used client rectangle.  This allows us
-- to compute whether or not we need scroll bars
local function Do(ID, Zoomable, Offset, ChildDraw, AllowHorizontal)
   local Item, IsNew = IDItem.Get(ID)

   if (not Item.ZoomClick) then
      Item.ZoomClick  = 1
   end

   if (not Item.Scrolling) then
      Item.Offset = Offset
   end

   -- If we've not come though before, then we need to setup our state variables
   if (IsNew) then
      Item.ChildArea = MakeRect(0, 0, 0, 0)
      Item.RenderedArea = CopyRect(UIArea.Get())
   end

   local BaseArea  = UIArea.Get()
   if (ValidScrollArea(BaseArea) == false) then
      print("ScrollArea width/height must be at least " .. 2*Scrollbar.VerticalWidth .. " pixels")
      return 0
   end

   -- If we *are* scrolling, hold everything constant from the start of that interaction.
   -- Otherwise, recompute
   if (not Item.ScrollActive) then
      Item.HorizActive = false
      Item.VertActive  = false

      Item.RenderedArea = CopyRect(UIArea.Get())
      Item.RenderedArea.x = Item.RenderedArea.x + Item.Offset.x
      Item.RenderedArea.y = Item.RenderedArea.y + Item.Offset.y

      -- Does the childarea fully contain the horizontal extent?
      if (AllowHorizontal and
          not FullyContains(Item.ChildArea.x, Item.ChildArea.w,
                            Item.RenderedArea.x, Item.RenderedArea.w)) then
         Item.HorizActive = true
         BaseArea.h  = BaseArea.h - Scrollbar.VerticalWidth
         Item.RenderedArea.h = Item.RenderedArea.h - Scrollbar.VerticalWidth
      end

      if (not FullyContains(Item.ChildArea.y, Item.ChildArea.h,
                            Item.RenderedArea.y, Item.RenderedArea.h)) then
         Item.VertActive = true
         BaseArea.w  = BaseArea.w - Scrollbar.VerticalWidth
         Item.RenderedArea.w = Item.RenderedArea.w - Scrollbar.VerticalWidth

         -- recheck this, just in case, since we've changed the variables...
         if (AllowHorizontal and
             (not Item.HorizActive and
              not FullyContains(Item.ChildArea.x, Item.ChildArea.w,
                                Item.RenderedArea.x, Item.RenderedArea.w))) then
            Item.HorizActive = true
            BaseArea.h  = BaseArea.h  - Scrollbar.VerticalWidth
            Item.RenderedArea.h = Item.RenderedArea.h - Scrollbar.VerticalWidth
         end
      end
   end

   if (Item.VertActive) then
      -- Draw vertical scroller
      Item.Offset.y, VertScrolling =
         Scrollbar.DoVertical(IDItem.CreateSub(ID,"vertbar"),
                              MakeRect(BaseArea.w, 0, Scrollbar.VerticalWidth, BaseArea.h),
                              Item.Offset.y,
                              math.min(Item.ChildArea.y,
                                       Item.RenderedArea.y),
                              math.max(Item.ChildArea.y + Item.ChildArea.h,
                                       Item.RenderedArea.y + Item.RenderedArea.h),
                              Item.RenderedArea.h,
                              function(IsScrolling) Item.Scrolling = IsScrolling end)
   end

   if (AllowHorizontal and Item.HorizActive) then
      -- Draw vertical scroller
      Item.Offset.x =
         Scrollbar.DoHorizontal(IDItem.CreateSub(ID,"horizbar"),
                                MakeRect(0, BaseArea.h, BaseArea.w, Scrollbar.VerticalWidth),
                                Item.Offset.x,
                                math.min(Item.ChildArea.x,
                                         Item.RenderedArea.x),
                                math.max(Item.ChildArea.x + Item.ChildArea.w,
                                         Item.RenderedArea.x + Item.RenderedArea.w),
                                Item.RenderedArea.w,
                                function(IsScrolling) Item.Scrolling = IsScrolling end)
   end

   -- Draw Child
   local ZoomFactor = ScrollZoomFactors[Item.ZoomClick]
   local DispChildArea = MakeRect(math.floor(ZoomFactor * Item.Offset.x),
                                  math.floor(ZoomFactor * Item.Offset.y),
                                  math.floor(ZoomFactor * BaseArea.w),
                                  math.floor(ZoomFactor * BaseArea.h))
   UIArea.ProtectedPush(BaseArea, DispChildArea,
                        function ()
                           UINoteZoom(ZoomFactor)
                           local NewRect, Drew = ChildDraw(IDItem.MakeID(ID,"client"))
                           UIRemoveZoom(ZoomFactor)
                           if (Drew and IsRect(NewRect)) then
                              Item.ChildArea =
                                 MakeRect(math.floor(NewRect.x / ZoomFactor),
                                          math.floor(NewRect.y / ZoomFactor),
                                          math.floor(NewRect.w / ZoomFactor),
                                          math.floor(NewRect.h / ZoomFactor))
                           else
                              Item.ChildArea = DispChildArea
                           end

                           if (not Interactions.Active()) then
                              if (Zoomable) then
                                 Item.ZoomAction =
                                    Interactions.RegisterDefClipped(Item.ZoomAction or MakeZoomControl(Item))
                              else
                                 Item.ScrollwheelAction =
                                    Interactions.RegisterDefClipped(Item.ScrollwheelAction or MakeWheelControl(Item))
                              end
                           end
                        end)

   -- Draw the shangle at the bottom
   -- if (Item.VertActive and Item.HorizActive) then
   if (AllowHorizontal) then
      DrawRect(MakeRect(UIAreaGet().w - Scrollbar.VerticalWidth,
                        UIAreaGet().h - Scrollbar.VerticalWidth,
                        Scrollbar.VerticalWidth, Scrollbar.VerticalWidth),
               UIPref.GetColor("toolbar_bg"))
   end

   return Item.Offset
end


ScrollArea = {
   Do = function(ID, Offset, ChildDraw)
           return Do(ID, false, Offset, ChildDraw, true)
        end,

   DoNoHorizontal = function(ID, Offset, ChildDraw)
                       return Do(ID, false, Offset, ChildDraw, false)
                    end,
   
   DoZoomable = function(ID, Offset, ChildDraw)
                   return Do(ID, true, Offset, ChildDraw, true)
                end
}
