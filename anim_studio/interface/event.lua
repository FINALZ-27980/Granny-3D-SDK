-- Handy event functions and variables...
require "queue"
require "ui_area"

MouseEventRange    = 0
KeyboardEventRange = 100
LossOfFocusEvent   = 1000
NullEvent          = 2000

LButtonDown = MouseEventRange + 0
LButtonUp   = MouseEventRange + 1
RButtonDown = MouseEventRange + 2
RButtonUp   = MouseEventRange + 3
MButtonDown = MouseEventRange + 4
MButtonUp   = MouseEventRange + 5
MouseMove   = MouseEventRange + 6
MouseWheel  = MouseEventRange + 7

BackButtonDown    = MouseEventRange + 8
BackButtonUp      = MouseEventRange + 9
ForwardButtonDown = MouseEventRange + 10
ForwardButtonUp   = MouseEventRange + 11


KeyDown     = KeyboardEventRange + 0
KeyUp       = KeyboardEventRange + 1
DirKeyDown  = KeyboardEventRange + 2
DirKeyUp    = KeyboardEventRange + 3
CtrlKeyDown = KeyboardEventRange + 4
CtrlKeyUp   = KeyboardEventRange + 5
FnKeyDown   = KeyboardEventRange + 6
FnKeyUp     = KeyboardEventRange + 7

Direction_KeyUp    = 0
Direction_KeyDown  = 1
Direction_KeyRight = 2
Direction_KeyLeft  = 3
Direction_KeyHome  = 4
Direction_KeyEnd   = 5

Control_KeyBackspace = 0
Control_KeyDelete    = 1
Control_KeyEnter     = 2
Control_KeyEscape    = 3
Control_KeyBackward  = 4
Control_KeyForward   = 5
Control_KeyStop      = 6
Control_KeyPlayPause = 7



EventQueue = Queue.New()

function ClearEventQueue()
   EventQueue = Queue.New()
end

function PushBackEvent(Event)
   Queue.In(EventQueue, Event)
end

function PopFrontEvent()
   return Queue.Out(EventQueue)
end

function IterateEvents(Fn)
   Queue.IterateRem(EventQueue, Fn)
end


function MakeLossOfFocusEvent()
   return { Type  = LossOfFocusEvent }
end

function MakeNullEvent()
   return { Type = NullEvent }
end

-- Mouse event functions
function MakeMouseEvent(Type, x, y, Delta, Shift, Ctrl, Alt)
   return { Type  = Type,
            Point = { x = x, y = y },
            Delta = Delta,
            Shift = Shift,
            Ctrl  = Ctrl,
            Alt = Alt }
end

-- Keyboard event functions
function MakeKeyEvent(Type, Key, Shift, Ctrl, Alt)
   return { Type  = Type,
            Key   = Key,
            Ctrl  = Ctrl,
            Shift = Shift,
            Alt   = Alt }
end


UpEvents = { }
UpEvents[LButtonDown] = LButtonUp
UpEvents[RButtonDown] = RButtonUp
UpEvents[MButtonDown] = MButtonUp
UpEvents[BackButtonDown] = BackButtonUp
UpEvents[ForwardButtonDown] = ForwardButtonUp
UpEvents[KeyDown]     = KeyUp
UpEvents[CtrlKeyDown] = CtrlKeyUp
UpEvents[DirKeyDown]  = DirKeyUp
UpEvents[FnKeyDown]   = FnKeyUp

function UpEventFor(Type)
   local UpType = UpEvents[Type]

   if (not UpType) then
      error("Unable to map event type:", Type)
   end

   return UpType
end


function MakeLocalEvent(Event, AreaCapture)
   local NewEvent = {}
   for key, val in pairs(Event) do
      NewEvent[key] = val
   end

   if (NewEvent.Point and AreaCapture) then
      local NewPoint, Clipped = UIArea.TransformToCapture(AreaCapture, NewEvent.Point)

      NewEvent.Point = NewPoint
   end

   return NewEvent
end

function MultiWaitForEvent(Event, YieldVal, EventParams)
   repeat
      local Found = nil
      for i,Match in ipairs(EventParams) do
         local Success = true
         for k,v in pairs(Match) do
            if (Event[k] ~= v) then
               Success = false
            end
         end

         if (Success) then
            Found = Match
            break
         end
      end

      if (Found) then
         return Event
      elseif (Event.Type == LossOfFocusEvent) then
         return nil
      end

      Event = Interactions.Continue(YieldVal)
   until nil
end


function SimpleWaitForEvent(Event, Type, YieldVal, Params)
   repeat
      if (Event.Type == Type) then
         local Success = true
         for k,v in pairs(Params) do
            if (Event[k] ~= v) then
               Success = false
            end
         end

         if (Success) then
            return Event
         end
      elseif (Event.Type == LossOfFocusEvent) then
         return nil
      end

      Event = Interactions.Continue(YieldVal)
   until nil
end

function WaitForEvent(Event, Type, YieldValue, Filter)
   -- This is a little lame because we come in with an active event that must be properly
   -- handled.  The event must be submitted to the filter function for consideration,
   -- before we yield in the repeat/until loop below
   if (Filter and (not not Filter(Event))) then
      Interactions.Abort()
   end

   while (Event.Type ~= Type) do
      Event = Interactions.Continue(YieldValue)

      if (Filter and (not not Filter(Event))) then
         Interactions.Abort()
      end
   end

   return Event
end

function KeyForChar(c)
   if (type(c) ~= "string") then
      error("should be a string!")
   end

   return string.byte(c, 1)
end
