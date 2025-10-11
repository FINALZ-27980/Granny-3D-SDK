-- General queue data type...
local function New()
   return { Count = 0 }
end

local function Push(Stk, Item)
   Stk.Count = Stk.Count + 1
   Stk[Stk.Count] = Item
end

local function Pop(Stk)
   if (Stk.Count == 0) then
      return nil
   end

   local value = Stk[Stk.Count]
   Stk[Stk.Count] = nil
   Stk.Count = Stk.Count - 1

   return value
end

local function Peek(Stk)
   return Stk[Stk.Count]
end

local function Empty(Stk)
   return (Stk == nil or Stk.Count == 0)
end

local function IsBackItem(Stack, Item)
   return ((not Empty(Stack)) and Item == Peek(Stack))
end

local function Iterate(Stk, Fn)
   for Idx = Stk.Count, 1, -1 do
      Current = Stk[Idx]
      if (Fn(Current)) then
         break
      end
   end
end

local function IterateFromBase(Stk, Fn)
   for Idx = 1, Stk.Count do
      Current = Stk[Idx]
      if (Fn(Current)) then
         break
      end
   end
end

local function IterateRem(Stk, Fn)
   Current = Pop(Stk)
   while (Current) do
      if (not not Fn(Current)) then break end
      Current = Pop(Stk)
   end
end


Stack = {
   New  = New,
   Empty = Empty,
   IsBackItem = IsBackItem,

   Push = Push,
   Pop  = Pop,
   Peek = Peek,

   Iterate = Iterate,
   IterateFromBase = IterateFromBase,
   IterateRem = IterateRem
}
