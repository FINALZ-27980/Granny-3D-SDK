require "event"
require "rect"
require "id_item"

local TitleHandle    = UIPref.GetFont("button")
local TitleH, TitleBL = GetFontHeightAndBaseline(FontHandle)

local TPadding = 4
local VPadding = 8
local HPadding = 8

local function DoYield(Title, Choices)
   if (#Choices == 0) then
      error("No choices!")
   end

   local MaxW = 0
   local MaxH = 0
   for i,v in ipairs(Choices) do
      local ThisW, ThisH = Button.Dim(v)
      if (ThisW > MaxW) then
         MaxW = ThisW
      end
      if (ThisH > MaxH) then
         MaxH = ThisH
      end
   end

   local UIRect  = UIArea.Get()
   local DialogW = MaxW * #Choices + HPadding * (#Choices + 1)
   local DialogH = TitleH + 3 * VPadding + MaxH

   local DialogRect = MakeRect((UIRect.w - DialogW) / 2,
                               (UIRect.h - DialogH) / 2,
                               DialogW, DialogH)
   local TitleRect = MakeRect(DialogRect.x, DialogRect.y, DialogW, TitleH + VPadding)

   local function EventLoop(ID)
      while true do
         DrawStippleRect(UIArea.Get(),
                         UIPref.GetColor("inaction_indicator0"),
                         UIPref.GetColor("inaction_indicator1"))
      
         NineGrid_Draw(NineGrid_GetHandle("dialog"), DialogRect)
         NineGrid_Draw(NineGrid_GetHandle("dialog_title"), TitleRect)

         local Result = nil
         UIArea.ProtectedSimplePush(
            DialogRect,
            function ()
               RenderText(TitleHandle, Title, 0,
                          MakePoint(HPadding, TitleBL + TPadding), MakeColor(1, 1, 1))
         
               local CurrX = HPadding
               local CurrY = TitleH + 2 * TPadding + VPadding
               for i,v in ipairs(Choices) do
                  local ButID = IDItem.CreateSub(ID, i)
                  if (Button.Do(ButID, MakeRect(CurrX, CurrY, MaxW, MaxH), v, false, nil)) then
                     Result = v
                  end
                  CurrX = CurrX + HPadding + MaxW
               end
            end)

         if (Result ~= nil) then
            coroutine.yield(true, Result)
         else
            coroutine.yield(false, nil)
         end
      end
   end

   local EventCoro = coroutine.create(EventLoop)
   PushModal(EventCoro, "SimpleChoice")
   Interactions.Continue(true)

   return GetModalResult()
end

local function CallbackStub(Title, Choices, Callback)
   DoYield(Title, Choices)
   coroutine.yield(true)

   local Result = GetModalResult()
   if (Callback) then
      Callback(Result)
   end
end

local function DoCallback(Title, Choices, Callback)
   Coro = coroutine.create(CallbackStub)
   coroutine.resume(Coro, Title, Choices, Callback)
end

ModalChoice = {
   DoYield    = DoYield,
   DoCallback = DoCallback
}
