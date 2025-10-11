require "id_item_cache"
require "ui_preferences"
require "ui_itunesbox"
require "ui_recent_file_button"
require "ui_button"
require "ui_autoplacer"
require "rect"

function UserMacros(ID)
   -- Look for the UserMacrosTable global
   if (not UserMacrosTable) then
      return
   end

   local UIRect = UIArea.Get()
   local Placement = AutoPlacement.New(AutoPlacement.Horizontal,
                                       MakeRect(5, 5, UIRect.w - 10, 1000000),  -- Basically infinite height
                                       5)

   for i,um in ipairs(UserMacrosTable) do
      local w,h = Button.Dim(um.Name)

      local Rect = Placement:Next(w,h)
      Button.Do(IDItem.CreateSub(ID, i), Rect, um.Name, false,
                function()
                   local Marker = Edit_PushUndoCollapseMarker(um.Name)
                   um.Fn()
                   Edit_UndoCollapseToMarker(Marker)
                end)
   end
end
