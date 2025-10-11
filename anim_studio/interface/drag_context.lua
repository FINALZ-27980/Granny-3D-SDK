-- Functions for registering and iterating different drag contexts
require "stack"
require "ui_area"

local Current = {}
local ActiveType = nil

local function Push(ID, Name)
   -- Store off the points, we need those frame to frame
   if (Current) then
      Current = { StartPoint = Current.StartPoint,
                  CurrentPoint = Current.CurrentPoint }

   else
      Current = {}
   end
end

local function Pop()
end

local function MakeTargetEntry(Type, Rect, Value)
   if (not Current) then
      print("No drag context present 1")
      return
   end

   Current[Type] = Current[Type] or Stack.New()
   Stack.Push(Current[Type], { AreaCapture = UIArea.Capture(),
                               Rect        = Rect,
                               Value       = Value })
end

local function GetItems(Type)
   if (not Current) then
      print("No drag context present 2")
      return
   end

   return Current[Type] or Stack.New()
end

local function CurrentPoint(Point)
   if (ActiveType == nil or not Current) then
      error("drag must be activated")
   end

   Current.CurrentPoint = CopyPoint(Point)
end

local function StartDrag(Type, Point)
   if (not Current or ActiveType) then
      error("inconsistent drag operation")
   end

   ActiveType = Type
  Current.StartPoint   = CopyPoint(Point)
   Current.CurrentPoint = Current.StartPoint
end


local function EndDrag(Type)
   Current.StartPoint   = nil
   Current.CurrentPoint = nil

   if (ActiveType == nil or ActiveType ~= Type) then
      error("Type mismatch")
   else
      ActiveType = nil
   end
end

local function GetDragPoints()
   if (ActiveType == nil) then
      error("no drag in progress")
   end

   return Current.StartPoint, Current.CurrentPoint
end

local function TargetContainsPoint(Target)
   local Point = CopyPoint(Current.CurrentPoint)
   local TargetPt = UIArea.TransformToCapture(Target.AreaCapture, Point)

   if (RectContainsPoint(Target.Rect, TargetPt)) then
      return MakePoint(TargetPt.x - Target.Rect.x,
                       TargetPt.y - Target.Rect.y)
   else
      return nil
   end
end


DragContext = {
   Push = Push,
   Pop  = Pop,

   MakeTargetEntry = MakeTargetEntry,
   GetItems  = GetItems,

   StartDrag     = StartDrag,
   EndDrag       = EndDrag,

   CurrentPoint  = CurrentPoint,
   GetDragPoints = GetDragPoints,

   IsActive   = function(Type) return (ActiveType == Type) end,
   IsDragging = function() return (ActiveType ~= nil) end,

   TargetContainsPoint = TargetContainsPoint,

}
