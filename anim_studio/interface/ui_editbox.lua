require "ui_base"
require "edit_utils"

local EditGridHandle = NineGrid_GetHandle("editbox")

-- local FontHandle = UIPref.GetFont("editbox")
-- local FontHeight, FontBaseline = GetFontHeightAndBaseline(FontHandle)

local SidePadding   = 8
local TopPadding    = 3
local BottomPadding = 4

local DefaultTextColor       = UIPref.GetColor("editbox_text")
local HLTextColor     = UIPref.GetColor("editbox_hl_text")
local HLTextDropColor = UIPref.GetColor("editbox_hl_text_drop")

local DisabledTextColor = UIPref.GetColor("editbox_text_disabled")


local function RenderWhileEditing(Item, Point, TextColor)
   local Text, SelStart, SelEnd, Cursor = STB_GetTextParams(Item.STBEdit)

   TextColor = TextColor or UIPref.GetColor("editbox_text")

   -- Actual text
   RenderText(Item.FontHandle, Text, 0, MakePoint(0, TopPadding + Item.FontBaseline), TextColor)

   -- highlight
   if (SelEnd > SelStart) then
      UIArea.ProtectedSimplePush(
         MakeRect(SelStart, 0, SelEnd - SelStart, UIAreaGet().h),
         function()
            DrawRect(UIAreaGet(), MakeColor(0, 0, 1))
            RenderText(Item.FontHandle, Text, 0, MakePoint(-SelStart, TopPadding + Item.FontBaseline), MakeColor(0, 0, 0))
         end)
   end

   -- Cursor
   DrawRect(MakeRect(Cursor-1, 0, 2, Item.FontHeight), TextColor)
end


local function DoImpl(ID, Rect, String, FontHandle, ShowBox, EditArgs)
   local Item, IsNew = IDItem.Get(ID)

   Item.Canonical = String

   if (IsNew) then
      Item.Editing = false

      Item.FontHandle = FontHandle
      Item.FontHeight, Item.FontBaseline = GetFontHeightAndBaseline(FontHandle)
   end

   UIArea.ProtectedSimplePush(
      Rect,
      function()
         local Area = UIArea.Get()

         -- Register the interaction
         if (not Interactions.Active()) then
            EditArgs.Item = Item
            EditArgs.Rect = Area

            Item.EditAction =
               Interactions.RegisterDefClipped(Item.EditAction or MakeEdit(EditArgs))
         end

         if (ShowBox) then
            NineGrid_Draw(EditGridHandle, Area)
         end

         if (not Item.Editing) then
            local TextToShow = String

            -- Handle a special case here.  If the text in the combobox was edited, and
            -- that edit is complete, we need to show the changed string until we
            -- return the changed value below.
            if (Item.Changed) then
               TextToShow = Item.Changed
            end

            RenderText(Item.FontHandle, TextToShow, 0, MakePoint(0, TopPadding + Item.FontBaseline), EditArgs.TextColor)
         else
            RenderWhileEditing(Item, MakePoint(0, Item.FontBaseline), EditArgs.TextColor)
         end
      end)

   if (Item.Changed) then
      local NewString = Item.Changed
      Item.Changed = nil

      return NewString, true, Item.Success
   else
      return String, false, false
   end
end

local function DrawPlaceholder(Rect, String, FontHandle)
   UIArea.ProtectedSimplePush(
      Rect,
      function()
         local Area = UIArea.Get()

         local h,bl = GetFontHeightAndBaseline(FontHandle)

         NineGrid_Draw(EditGridHandle, Area)
         RenderText(FontHandle, String, 0, MakePoint(0, TopPadding + bl), DisabledTextColor)
      end)
end


local function RecommendedHeight(FontHandle)
   local FontHeight, FontBaseline = GetFontHeightAndBaseline(FontHandle)
   return FontHeight + (BottomPadding + TopPadding)
end

Editbox = {
   Do  = function(ID, Rect, String, FontHandle, ShowBox, EditTextColor)
            return DoImpl(ID, Rect, String, FontHandle, ShowBox, { TextColor = EditTextColor or UIPref.GetColor("editbox_text") })
         end,
   DrawPlaceholder = DrawPlaceholder,

   DoRenamable  = function(ID, Rect, String, FontHandle, ShowBox, EditTextColor)
                     return DoImpl(ID, Rect, String, FontHandle, ShowBox,
                                   { F2Rename = true,
                                     TextColor = EditTextColor or UIPref.GetColor("editbox_text") })
                  end,
   DoWithArgs  = function(ID, Rect, String, FontHandle, ShowBox, Args)
                    if (Args.TextColor == nil) then
                       Args.TextColor = UIPref.GetColor("editbox_text")
                    end
                    return DoImpl(ID, Rect, String, FontHandle, ShowBox, Args)
                 end,
   RecommendedHeight = RecommendedHeight,
}
