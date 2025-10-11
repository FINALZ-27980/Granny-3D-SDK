require "rect"

local function ProtectedPush(ParentRect, ChildRect, Fn)
   if (UIArea.Push(ParentRect, ChildRect)) then
      -- Ensure that pop happens even if exception is thrown
      pcall(Fn)
   end

   UIArea.Pop()
end

-- Just forward to C implementation
UIArea = {
   ProtectedPush = ProtectedPush,
   Push          = UIAreaPush,

   ProtectedSimplePush = function(Rect, Fn) ProtectedPush(Rect, MakeRect(0, 0, Rect.w, Rect.h), Fn) end,
   SimplePush          = function(Rect)     return UIAreaPush(Rect, MakeRect(0, 0, Rect.w, Rect.h)) end,

   Pop      = UIAreaPop,
   Get      = UIAreaGet,

   Capture            = UIAreaCapture,
   TransformToCapture = UIAreaTransformToCapture,
   TransformFromCapture = UIAreaTransformFromCapture,
}
