require "event"
require "rect"
require "id_item"
require "ui_scrollarea"


local GridHandle  = NineGrid_GetHandle("itunes_dialog")
local ArrowHandle = UIBitmap_GetHandle("itunes_arrow_dark")
local ArrowWidth, ArrowHeight = UIBitmap_Dimensions(ArrowHandle)
local ArrowSetBack = ArrowHeight

local TopArrowHandle = UIBitmap_GetHandle("itunes_arrow_up_dark")
local TopArrowWidth, TopArrowHeight = UIBitmap_Dimensions(TopArrowHandle)
local TopArrowSetBack = ArrowWidth * 2


local function MakeDialogAndArrow(PosX, PosY, Width, MaxHeight, DesiredSubRect)
   local UIRect = CopyRect(UIArea.Get())
   
   local DialogRect = nil
   local ArrowRect  = nil
   local UseScroll  = false

   if (DesiredSubRect) then
      if (DesiredSubRect.h + 20 > MaxHeight) then
         DialogRect = MakeRect(0, 0, Width, MaxHeight)
         UseScroll = true
      else
         DialogRect = MakeRect(0, 0, Width, math.min(DesiredSubRect.h + 20))
      end
   else
      DialogRect = MakeRect(0, 0, Width, 100)
   end

   DialogRect  = MakeRect(PosX - Width - ArrowWidth,
                          PosY - ArrowSetBack,
                          DialogRect.w, DialogRect.h)

   local LowerDialog = DialogRect.y + DialogRect.h
   local LowerArea   = UIRect.y + UIRect.h
   if (LowerDialog > LowerArea) then
      DialogRect.y = DialogRect.y - (LowerDialog - LowerArea)
   end

   ArrowRect  = MakeRect(PosX - ArrowWidth, PosY - (ArrowHeight/2), ArrowWidth, ArrowHeight)

   return DialogRect, ArrowRect, UseScroll
end

local function MakeTopDialogAndArrow(PosX, PosY, Width, MaxHeight, DesiredSubRect)
   local UIRect = CopyRect(UIArea.Get())
   
   local DialogRect = nil
   local ArrowRect  = nil
   local UseScroll  = false

   if (DesiredSubRect) then
      if (DesiredSubRect.h + 20 > MaxHeight) then
         DialogRect = MakeRect(0, 0, Width, MaxHeight)
         UseScroll = true
      else
         DialogRect = MakeRect(0, 0, Width, math.min(DesiredSubRect.h + 20))
      end
   else
      DialogRect = MakeRect(0, 0, Width, 100)
   end

   DialogRect  = MakeRect(PosX - math.floor(Width/2),
                          PosY + TopArrowHeight,
                          DialogRect.w, DialogRect.h)

   local LowerDialog = DialogRect.y + DialogRect.h
   local LowerArea   = UIRect.y + UIRect.h
   if (LowerDialog > LowerArea) then
      DialogRect.y = DialogRect.y - (LowerDialog - LowerArea)
   end

   ArrowRect  = MakeRect(PosX - math.floor(TopArrowWidth/2), PosY, TopArrowWidth, TopArrowHeight)

   return DialogRect, ArrowRect, UseScroll
end

local function Do(ID, NodeID, PosX, PosY, Width, MaxHeight, DesiredRect, MakeRectsFn, ThisArrowHandle, ShowName)
   local DialogRect, ArrowRect, UseScroll = MakeRectsFn(PosX, PosY, Width, MaxHeight, DesiredRect)

   -- Draw the dialog box
   NineGrid_DrawModulated(GridHandle, DialogRect, MakeColor(0, 0, 0))

   local SubRect = CopyRect(DialogRect)
   SubRect.x = SubRect.x + 2
   SubRect.y = SubRect.y + 2
   SubRect.w = SubRect.w - 4
   SubRect.h = SubRect.h - 4
   NineGrid_DrawModulated(GridHandle, SubRect, MakeColor(0.2, 0.2, 0.2))
   UIBitmap_DrawStretched(ThisArrowHandle, ArrowRect)

   SubRect.y = SubRect.y + 5
   SubRect.h = SubRect.h - 10
   local ReturnedRect = nil

   local DialogFn = function()
                       if (not Edit_IsTransition(NodeID) and Edit_ParentIsBlendgraphOrNil(NodeID)) then
                          ReturnedRect = GraphNodeEditBox(IDItem.CreateSub(ID, "sub"), NodeID, ShowName)
                       else
                          ReturnedRect = StateNodeEditBox(IDItem.CreateSub(ID, "sub"), NodeID, ShowName)
                       end
                       return ReturnedRect, true
                    end

   if (UseScroll) then
      local Item, IsNew = IDItem.Get(ID)
      if (IsNew) then
         Item.ScrollOffset = { x = 0, y = 0 }
      end

      UIArea.ProtectedSimplePush(SubRect,
                                 function()
                                    Item.ScrollOffset =
                                       ScrollArea.Do(IDItem.CreateSub(ID, "Scroll" .. NodeID),
                                                     Item.ScrollOffset,
                                                     function (ID)
                                                        return DialogFn()
                                                     end)
                                 end)
   else
      UIArea.ProtectedSimplePush(SubRect, DialogFn)
   end

   return ReturnedRect
end

IPadDialog = {
   DoHorizontal = function(ID, NodeID, PosX, PosY, Width, MaxHeight, DesiredRect, ShowName)
                     return Do(ID, NodeID, PosX, PosY, Width, MaxHeight, DesiredRect, MakeDialogAndArrow, ArrowHandle, ShowName)
                  end,
   DoTop = function(ID, NodeID, PosX, PosY, Width, MaxHeight, DesiredRect)
              return Do(ID, NodeID, PosX, PosY, Width, MaxHeight, DesiredRect, MakeTopDialogAndArrow, TopArrowHandle, true)
           end
}

