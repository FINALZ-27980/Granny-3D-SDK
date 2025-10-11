-- Little help to get context menus, dialogs, etc in the right place...
require "queue"

local RenderQueue = Queue.New()

local function Register(DrawFn)
   if (DrawFn) then
      Queue.In(RenderQueue, DrawFn)
   end
end

local function Flush()
   Queue.IterateRem(RenderQueue, function(Item) Item() end)
   RenderQueue = Queue.New()
end

DeferredRender = {
   Register = Register,
   Flush = Flush
}
