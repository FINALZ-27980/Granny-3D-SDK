require "event"
require "rect"
require "stack"
require "ui_area"
require "ui_preferences"

local Tips = Stack.New()

local TipDelay         = 0.5
local TipActive        = false

local LastPoint        = nil
local LastPointChanged = nil

local function Begin()
   Tips = Stack.New()
end

local function KillTip()
   TipActive = false
   LastPoint = nil
   LastPointChanged = nil
end

local function ProcessMouse(MousePt)
   if (LastPoint == nil or Interactions.Active()) then
      TipActive = false
      LastPoint = CopyPoint(MousePt)
      LastPointChanged = Edit_GetAbsoluteTime()
   else
      if (math.abs(LastPoint.x - MousePt.x) < 4 and math.abs(LastPoint.y - MousePt.y) < 4) then
         if (Edit_GetAbsoluteTime() - LastPointChanged > TipDelay) then
            TipActive = true
         end
      else
         -- different point
         TipActive = false
         LastPoint = CopyPoint(MousePt)
         LastPointChanged = Edit_GetAbsoluteTime()
      end
   end

   if (not TipActive) then
      return
   end

   Stack.Iterate(
      Tips,
      function(Entry)
         local UL, Clipped = UIArea.TransformToCapture(Entry.Area, MousePt)
         if (not Clipped) then
            DrawToolTip(Entry.Text, LastPoint)
            return true
         end
      end)
end

local function Register(Text, Rect)
   UIArea.ProtectedSimplePush(
      Rect,
      function()
         Stack.Push(Tips, { Area = UIArea.Capture(), Text = Text } )
      end)
end

ToolTip = {
   Register = Register,

   Begin = Begin,
   ProcessMouse = ProcessMouse,
   KillTip = KillTip,
}