require "rect"
require "event"
require "ui_editbox"

local FontHandle    = UIPref.GetFont("console")
local FontH, FontBL = GetFontHeightAndBaseline(FontHandle)

local function RenderEditor(ID)
   -- Draw the title section for the animation sections
   Splitter.DoVertical(IDItem.CreateSub(ID, "splitv"),
                       350, -1, false,
                       function (ID)
                          DrawRect(UIArea.Get(), MakeColor(1, 0, 0))
                       end,
                       function(ID)
                          DrawRect(UIArea.Get(), MakeColor(0, 1, 0))
                       end)
end

local ConsoleActive = false
ConsoleHeightFactor = 2

local CommandHistory = {  }
local CurrentHistoryEntry = -1

local function Activate()
   if (ConsoleActive) then
      return
   end

   local EditHeight = Editbox.RecommendedHeight(FontHandle)

   local function EventLoop(ID)
      local CurrentCommand = ""
      local Changed = false
      while true do
         local UIRect = UIArea.Get()

         local EditRect = CopyRect(UIRect)
         EditRect.x = EditRect.x + 5
         EditRect.y = EditRect.y + 5
         EditRect.h = EditHeight
         EditRect.w = EditRect.w - 10

         CurrentCommand, Changed, Success =
            Editbox.DoWithArgs(IDItem.CreateSub(ID, "console_edit"), EditRect,
                               CurrentCommand, FontHandle, true,
                               { StartActive = true,
                                 EventCallback = function (Item, Event)
                                                    if (#CommandHistory ~= 0) then
                                                       if (Event.Key == Direction_KeyUp) then
                                                          if (Event.Type == DirKeyDown) then
                                                             CurrentHistoryEntry = CurrentHistoryEntry + 1
                                                          else
                                                             return
                                                          end
                                                       elseif (Event.Key == Direction_KeyDown) then
                                                          if (Event.Type == DirKeyDown) then
                                                             CurrentHistoryEntry = CurrentHistoryEntry - 1
                                                          else
                                                             return
                                                          end
                                                       else
                                                          CurrentHistoryEntry = -1
                                                          return
                                                       end

                                                       if (CurrentHistoryEntry >= #CommandHistory) then
                                                          CurrentHistoryEntry = #CommandHistory - 1
                                                       end
                                                       if (CurrentHistoryEntry < 0) then
                                                          CurrentHistoryEntry = 0
                                                       end

                                                       STB_SetText(Item.STBEdit, CommandHistory[#CommandHistory - CurrentHistoryEntry])

                                                       -- dont_cancel, ignore
                                                       return false, true
                                                    end
                                                 end })

         if (Changed) then
            if (CurrentCommand == "" or not Success) then
               ConsoleActive = false
               break
            else
               table.insert(CommandHistory, CurrentCommand)
               if (not pcall(loadstring(CurrentCommand))) then
                  print("Error executing:", CurrentCommand)
               end
               CurrentCommand = ""
            end
         end

         coroutine.yield(false, nil)
      end

      coroutine.yield(true, nil)
   end

   local EventCoro = coroutine.create(EventLoop)
   PushModal(EventCoro, "Console")
end

Console = {
   Activate = Activate
}
