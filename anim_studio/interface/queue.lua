-- General queue data type...
local function New()
   return { first = 0, last = -1 }
end

local function In(Q, Item)
   Q.last = Q.last + 1
   Q[Q.last] = Item
end

local function InFront(Q, Item)
   Q.first = Q.first - 1
   Q[Q.first] = Item
end

local function Out(Q)
   local first = Q.first
   if (first > Q.last) then
      return nil
   end

   local value = Q[first]
   Q[first] = nil
   Q.first = first + 1

   return value
end

local function Empty(Q)
   return (Q == nil or (Q.first >= Q.last))
end

local function Iterate(Q, Fn)
   for Idx = Q.first, Q.last, 1 do
      Current = Q[Idx]
      if (Fn(Current)) then
         break
      end
   end
end

local function IterateRem(Q, Fn)
   Current = Out(Q)
   while (Current) do
      if (Fn(Current)) then break end
      Current = Out(Q)
   end
end

Queue = {
   New = New,
   In  = In,
   InFront = InFront,
   Out = Out,
   Empty = Empty,

   Iterate = Iterate,
   IterateRem = IterateRem
}
