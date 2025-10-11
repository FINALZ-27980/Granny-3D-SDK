-- Maps ids to backing tables.
--
-- Id/Table pairs that haven't been touched in between calls to FlushInactive are,
-- well, flushed.

local AllItems    = {}
local ActiveItems = {}

local function CreateSub(parent, child)
   return parent .. "/" .. child
end

local function MakeID(primary, secondary)
   return primary .. "|" .. secondary
end

local function FlushInactive()
   AllItems = ActiveItems
   ActiveItems = {}
end

local function Get(id)
   local Item = AllItems[id]
   local NewItem = false
   if (Item == nil) then
      Item = {}
      NewItem = true
      AllItems[id] = Item
   end

   ActiveItems[id] = Item
   return Item, NewItem
end

IDItem = {
   CreateSub     = CreateSub,
   MakeID        = MakeID,
   FlushInactive = FlushInactive,
   Get           = Get,
}
