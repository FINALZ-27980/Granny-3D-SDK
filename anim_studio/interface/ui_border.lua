require "rect"
require "ui_preferences"
require "ui_area"
require "id_item_cache"

local function Do(ID, left, right, top, bottom, SubFn, Color)
   local BorderColor = Color or UIPref.GetColor("border")
   
   local Rect = UIArea.Get()
   local SubRect = MakeRect(Rect.x + left,
                            Rect.y + top,
                            Rect.w - (left + right),
                            Rect.h - (top + bottom))

   if (left ~= 0) then
      DrawRect(MakeRect(Rect.x, Rect.y, left, Rect.h), BorderColor)
   end
   if (right ~= 0) then
      DrawRect(MakeRect(Rect.x + Rect.w - right, Rect.y, right, Rect.h), BorderColor)
   end

   if (top ~= 0) then
      DrawRect(MakeRect(Rect.x, Rect.y, Rect.w, top), BorderColor)
   end
   if (bottom ~= 0) then
      DrawRect(MakeRect(Rect.x, Rect.y + Rect.h - bottom, Rect.w, bottom), BorderColor)
   end

   if (SubFn) then
      UIArea.ProtectedSimplePush(SubRect, function() SubFn(ID) end)
   end
end

Border = {
   Do = Do
}

