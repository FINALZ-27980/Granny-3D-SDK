-- Auto placer for simple interfaces...
require "rect"

AutoPlacement = {
   Horizontal = 1,
   Vertical   = 2
}

function AutoPlacement.New(Mode, InRect, Spacing)
   local Obj = {
      Mode  = Mode,
      Spacing = Spacing or 0,
      CurrX = InRect.x,
      CurrY = InRect.y,
      Rect  = InRect,
      MaxWidthSeen  = 0,
      MaxHeightSeen = 0,
      StateStack = nil
   }
   setmetatable(Obj, { __index = AutoPlacement })
   return Obj
end

function AutoPlacement:NeedsCR(EndX, EndY)
   if (self.Mode == AutoPlacement.Horizontal and self.Rect.w > 0 and
       (EndX > self.Rect.x + self.Rect.w)) then
      -- horizontal cr
      return true
   elseif (self.Mode == AutoPlacement.Vertical and self.Rect.h > 0 and
           (EndY > self.Rect.y + self.Rect.h)) then
      -- vertical cr
      return true
   end

   return false
end

function AutoPlacement:CR()
   if (self.Mode == AutoPlacement.Horizontal) then
      self.CurrX = self.Rect.x
      self.CurrY = self.CurrY + self.MaxHeightSeen
      self.MaxWidthSeen  = 0
      self.MaxHeightSeen = 0
   elseif (self.Mode == AutoPlacement.Vertical) then
      self.CurrX = self.CurrX + self.MaxWidthSeen
      self.CurrY = self.Rect.y
      self.MaxWidthSeen  = 0
      self.MaxHeightSeen = 0
   end
end

function AutoPlacement:Next(w, h)
   local RealWidth  = w + self.Spacing
   local RealHeight = h + self.Spacing
   local EndX = self.CurrX + RealWidth
   local EndY = self.CurrY + RealHeight

   if (self:NeedsCR(EndX, EndY)) then
      self:CR()
      EndX = self.CurrX + RealWidth
      EndY = self.CurrY + RealHeight
   end

   if (RealWidth  > self.MaxWidthSeen)  then self.MaxWidthSeen  = RealWidth end
   if (RealHeight > self.MaxHeightSeen) then self.MaxHeightSeen = RealHeight end

   local NewRect = MakeRect(self.CurrX, self.CurrY, w, h)
   if (self.Mode == AutoPlacement.Horizontal) then
      self.CurrX = EndX
   else
      self.CurrY = EndY
   end

   return NewRect
end

function AutoPlacement:PushState()
   self.StateStack = {
      Mode  = self.Mode,
      Spacing = self.Spacing,
      CurrX = self.CurrX,
      CurrY = self.CurrY,
      Rect  = self.Rect,
      MaxWidthSeen  = self.MaxWidthSeen,
      MaxHeightSeen = self.MaxHeightSeen,
      Next = self.StateStack
   }
end

function AutoPlacement:PopState()
   if (self.StateStack) then
      self.Mode          = self.StateStack.Mode
      self.Spacing       = self.StateStack.Spacing
      self.CurrX         = self.StateStack.CurrX
      self.CurrY         = self.StateStack.CurrY
      self.Rect          = self.StateStack.Rect
      self.MaxWidthSeen  = self.StateStack.MaxWidthSeen
      self.MaxHeightSeen = self.StateStack.MaxHeightSeen
      self.StateStack    = self.StateStack.Next
   end
end

function AutoPlacement:DiscardState()
   if (self.StateStack) then
      self.StateStack = self.StateStack.Next
   end
end

-- Doesn't do any bounds or CR checking...
function AutoPlacement:GetNth(w, h, n)
   local RealWidth  = w + self.Spacing
   local RealHeight = h + self.Spacing

   if (self.Mode == AutoPlacement.Horizontal) then
      return MakeRect(self.CurrX + n * RealWidth, self.CurrY, w, h)
   else
      return MakeRect(self.CurrX, self.CurrY + n * RealHeight, w, h)
   end
end

function AutoPlacement:CurrentDim()
   return self.CurrX, self.CurrY
end
