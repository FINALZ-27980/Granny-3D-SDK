-- todo: drawarea clip replacement

-- Interaction routines
require "event"
require "stack"
require "ui_tooltips"

local ActiveEntry = nil
local IgnoreRegister = false

-- It's a bit clunky to keep two copies...
local AllInteractions     = Stack.New()
local OrderedInteractions = Stack.New()

local Contexts = { }
local ActiveContext = nil


local function MakeEntry(Interaction, AreaCapture, ClipRect)
   return {
      AreaCapture = AreaCapture,
      ClipRect    = ClipRect,
      Interaction = Interaction
   }
end

local function EventInValidRegion(Event, AreaCapture, ClipRect)
   -- Not a point localized event or a global registration?
   if ((not Event.Point) or (not AreaCapture)) then
      return true
   end

   local NewPoint, Clipped = UIArea.TransformToCapture(AreaCapture, Event.Point)
   if (Clipped) then
      return false
   end

   if (ClipRect) then
      -- Might be clipped by the sub rect...
      return RectContainsPoint(ClipRect, NewPoint)
   else
      return true
   end
end

local function Clear()
   ActiveEntry = nil
   AllInteractions = Stack.New()
   OrderedInteractions = Stack.New()

   if (ActiveContext ~= nil and Contexts[ActiveContext] ~= nil) then
      Contexts[ActiveContext].ActiveEntry_ = ActiveEntry
      Contexts[ActiveContext].AllInteractions_ = AllInteractions
      Contexts[ActiveContext].OrderedInteractions_ = OrderedInteractions
   end
end

local function ClearInactive()
   AllInteractions = Stack.New()
   OrderedInteractions = Stack.New()

   if (ActiveContext ~= nil and Contexts[ActiveContext] ~= nil) then
      Contexts[ActiveContext].AllInteractions_ = AllInteractions
      Contexts[ActiveContext].OrderedInteractions_ = OrderedInteractions
   end
end

local function Activate(Interaction)
   -- Note that whenever we activate an interaction, we kill the tooltip
   ToolTip.KillTip()

   ActiveEntry = Interaction
   if (ActiveContext ~= nil and Contexts[ActiveContext] ~= nil) then
      Contexts[ActiveContext].ActiveEntry_ = ActiveEntry
   end

   Stack.Iterate(AllInteractions,
                 function (Entry)
                    if (Entry ~= ActiveEntry and Entry.Interaction.Abort) then
                       Entry.Interaction.Abort()
                    end
                 end)
   AllInteractions = nil
   OrderedInteractions = nil
end

local function RegisterClipped(Interaction, ClipRect)
   if (IgnoreRegister) then
      return Interaction
   end

   if (ActiveEntry) then
      error("Shouldn't be registering while Interactions.Active() is true")
   end

   local AreaCapture, NonEmpty = UIArea.Capture()

   if (NonEmpty) then
      NewInteraction = MakeEntry(Interaction, AreaCapture, ClipRect)
      Stack.Push(AllInteractions, NewInteraction)
      Stack.Push(OrderedInteractions, NewInteraction)
   end

   return Interaction
end

local function RegisterGlobal(Interaction)
   if (IgnoreRegister) then
      return Interaction
   end

   if (ActiveEntry) then
      error("Shouldn't be registering while Interactions.Active() is true")
   end

   local NewInteraction = MakeEntry(Interaction, nil, nil)

   Stack.Push(AllInteractions, NewInteraction)
   Stack.Push(OrderedInteractions, NewInteraction)

   return Interaction
end

local function PumpEvents()
   IterateEvents(
      function (NextEvent)
         if (ActiveEntry) then
            normal, abort, captured =
               coroutine.resume(ActiveEntry.Interaction.Action,
                                MakeLocalEvent(NextEvent, ActiveEntry.AreaCapture))

            if (abort or coroutine.status(ActiveEntry.Interaction.Action) == "dead") then
               if (abort and ActiveEntry.Interaction.Abort) then
                  ActiveEntry.Interaction.Abort()
               end

               Clear()
               return true
            end
         else
            Stack.Iterate(OrderedInteractions,
                          function (Current)
                             if ((not Current.ClipRect) or EventInValidRegion(NextEvent, Current.AreaCapture, Current.ClipRect)) then
                                normal, abort, captured =
                                   coroutine.resume(Current.Interaction.Action,
                                                    MakeLocalEvent(NextEvent, Current.AreaCapture))

                                if (captured and not abort) then
                                   Activate(Current)
                                   return true
                                end
                             end
                          end)
         end
      end)

   if (AllInteractions) then
      return AllInteractions.Count
   else
      return 0
   end
end

local function Continue(CaptureState)
   return coroutine.yield(false, CaptureState)
end

local function Abort(CaptureState)
   return coroutine.yield(true)
end

local function Active()
   return ActiveEntry ~= nil
end

local function SetIgnoreRegister(val)
   IgnoreRegister = val
end


local function EnterContext(Name)
   local Entry = Contexts[Name]
   if (not Entry) then
      Entry = {
         ActiveEntry_ = nil,
         IgnoreRegister = false,
         AllInteractions_ = Stack.New(),
         OrderedInteractions_ = Stack.New()
      }
      Contexts[Name] = Entry
   end

   ActiveEntry         = Entry.ActiveEntry_
   IgnoreRegister      = Entry.IgnoreRegister_
   AllInteractions     = Entry.AllInteractions_
   OrderedInteractions = Entry.OrderedInteractions_

   ActiveContext = Name
end

local function DestroyContext(Name)
   Contexts[Name] = nil
   if (ActiveContext == Name) then
      ActiveContext = nil
   end
end

Interactions = {
   RegisterDefClipped = function(Interaction) return RegisterClipped(Interaction, UIArea.Get()) end,
   RegisterClipped = RegisterClipped,
   RegisterGlobal  = RegisterGlobal,
   PumpEvents      = PumpEvents,

   Continue = Continue,
   Abort    = Abort,

   Active = Active,
   ClearInactive = ClearInactive,

   -- Experts only!
   Activate = Activate,
   MakeEntry = MakeEntry,

   EnterContext   = EnterContext,
   DestroyContext = DestroyContext,

   SetIgnoreRegister = SetIgnoreRegister
}
