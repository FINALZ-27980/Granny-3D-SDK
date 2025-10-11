require "events"
require "ui_base"

-- Increments on destructive action to force recreation of interactions...
local CommentUID = 0
local function GetCommentUIDString(Comments)
   return CommentUID .. "_" .. #Comments
end


local FontHandle    = UIPref.GetFont("comment_text")
local FontH, FontBL = GetFontHeightAndBaseline(FontHandle)

local CommentLowlighted  = UIPref.GetColor("comment_text_low")
local CommentHighlighted = UIPref.GetColor("comment_text")
local RectColor       = UIPref.GetColor("comment_box")
local BackgroundColor = UIPref.GetColor("comment_box_alpha")

local TipString =
   ("Left-click and drag to move.\n" ..
    "Right-click and drag to change indication rectangle.\n" ..
    "Right-click and release to edit comment.")

local CloseString = " x "
local CloseStringW, CloseStringH = GetSimpleTextDimension(FontHandle, CloseString)

local Padding = 3

local function MakeChangeCommentOrRect(Item)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, RButtonDown, false)

      local TextEditing = true
      Event = WaitForEvent(Event, RButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 if (not RectContainsPoint(Item.EditRect, Event.Point)) then
                                    Item.DraggingSize = true
                                    TextEditing = false
                                 end

                                 if (not TextEditing) then
                                    local Pos, Size = Edit_GetCommentPosition(Item.CommentIndex)
                                    if (Pos and Size) then
                                       Size.x = math.max(Event.Point.x - Item.TextRect.x, 0)
                                       Size.y = math.max(Event.Point.y - (Item.TextRect.y + Item.TextRect.h -1), 0)
                                       Edit_SetCommentPosition(Item.CommentIndex, Pos, Size)
                                    end
                                 end
                                 
                              end
                           end)

      -- We were dragging the size...
      if (not TextEditing) then
         Item.DraggingSize = false
         Item.TextAlterOrDragSize = nil
         return
      end

      -- Edit the text!
      Item.Editing = true
      Item.STBEdit = AllocSTBTextEdit(Edit_GetCommentText(Item.CommentIndex), FontHandle)
      STB_SelectAllPosEnd(Item.STBEdit)

      local Success = EditEventLoop(Item, Item.EditRect, nil, nil)
      if (Success) then
         local Text, SelStart, SelEnd, Cursor = STB_GetTextParams(Item.STBEdit)
         Edit_SetCommentText(Item.CommentIndex, Text)
      end

      Item.Editing = false
      Item.TextAlterOrDragSize = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.DraggingSize = false
                        Item.Editing = false
                        Item.TextAlterOrDragSize = nil
                     end }
end

local function MakeMoveComment(Item)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      local StartMousePt = CopyPoint(Event.Point)
      local StartPos, StartSize = Edit_GetCommentPosition(Item.CommentIndex)

      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 local p = MakePoint(Event.Point.x - StartMousePt.x,
                                                     Event.Point.y - StartMousePt.y),
                                 Edit_SetCommentPosition(Item.CommentIndex,
                                                         MakePoint(StartPos.x + (Event.Point.x - StartMousePt.x),
                                                                   StartPos.y + (Event.Point.y - StartMousePt.y)),
                                                         StartSize)
                              end
                           end)

      Item.MoveAction = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.MoveAction = nil
                     end }
end

local function MakeDeleteComment(Item)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)
      Event = WaitForEvent(Event, LButtonUp, true)

      if (RectContainsPoint(Item.CloseRect, Event.Point)) then
         CommentUID = CommentUID + 1

         Edit_DeleteComment(Item.CommentIndex)
      end

      Item.CloseAction = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.CloseAction = nil
                     end }
end

local function RenderWhileEditing(Item, Point, TextColor)
   local Text, SelStart, SelEnd, Cursor = STB_GetTextParams(Item.STBEdit)

   TextColor = TextColor or UIPref.GetColor("editbox_text")

   -- Actual text
   RenderText(FontHandle, Text, 0, Point, TextColor)

   -- highlight
   if (SelEnd > SelStart) then
      UIArea.ProtectedSimplePush(
         MakeRect(Point.x + SelStart, Point.y - FontBL, SelEnd - SelStart, FontH),
         function()
            DrawRect(UIAreaGet(), MakeColor(0, 0, 1))
            RenderText(FontHandle, Text, 0, MakePoint(-SelStart, FontBL), MakeColor(0, 0, 0))
         end)
   end

   -- Cursor
   DrawRect(MakeRect(Point.x + Cursor - 1, Point.y - FontBL, 2, FontH), TextColor)
end


local function DoComment(ID, Comment, Index)
   local Item, IsNew = IDItem.Get(ID)

   if (IsNew) then
      Item.Editing = false
      Item.DraggingSize = false
      Item.CommentIndex = Index
   end
   

   local SizeText = Comment.Text
   if (Item.Editing) then
      SizeText = STB_GetTextParams(Item.STBEdit)
   end

   local w,h = GetSimpleTextDimension(FontHandle, SizeText)

   Item.EditRect  = MakeRect(Comment.Pos.x, Comment.Pos.y, w, h)
   Item.TextRect  = MakeRect(Item.EditRect.x - Padding, Item.EditRect.y - Padding,
                             Item.EditRect.w + (Padding * 2),
                             Item.EditRect.h + (Padding * 2))
   Item.CloseRect = MakeRect(Item.TextRect.x + Item.TextRect.w,
                             Item.TextRect.y,
                             CloseStringW + Padding * 2, Item.TextRect.h)
   Item.HoverRect = MakeRect(Item.TextRect.x,
                             Item.TextRect.y,
                             Item.TextRect.w + Item.CloseRect.w,
                             Item.TextRect.h)

   if (not Interactions.Active()) then
      Item.TextAlterOrDragSize = Interactions.RegisterClipped(Item.TextAlterOrDragSize or MakeChangeCommentOrRect(Item),
                                                              Item.EditRect)
      Item.MoveAction   = Interactions.RegisterClipped(Item.MoveAction or MakeMoveComment(Item), Item.EditRect)
      Item.DeleteAction = Interactions.RegisterClipped(Item.DeleteAction or MakeDeleteComment(Item), Item.CloseRect)
   end

   -- Render the comment
   local TextColor    = CommentLowlighted
   local OutlineColor = BackgroundColor

   if (Item.Editing or Item.DraggingSize or UIAreaRectContainsMouse(Item.HoverRect)) then
      OutlineColor = RectColor
      TextColor    = CommentHighlighted

      DrawRect(Item.CloseRect, BackgroundColor)
      DrawRectOutline(Item.CloseRect, OutlineColor)
      RenderText(FontHandle, CloseString, 0, MakePoint(Comment.Pos.x + Item.TextRect.w, Comment.Pos.y + FontBL), TextColor)
   end

   DrawRect(Item.TextRect, BackgroundColor)
   DrawRectOutline(Item.TextRect, OutlineColor)
   
   if (not Item.Editing) then
      RenderText(FontHandle, Comment.Text, 0, MakePoint(Comment.Pos.x, Comment.Pos.y + FontBL), TextColor)
   else
      RenderWhileEditing(Item, MakePoint(Comment.Pos.x, Comment.Pos.y + FontBL), TextColor)
   end
   
   if (Comment.Size.x ~= 0 or Comment.Size.y ~= 0) then
      DrawRectOutline(MakeRect(Item.TextRect.x, Item.TextRect.y + Item.TextRect.h - 1,
                               Comment.Size.x, Comment.Size.y),
                      OutlineColor)
   end

   ToolTip.Register(TipString, Item.TextRect)
end

function RenderComments(ID)
   local Comments = Edit_GetComments()

   if (not Comments or #Comments == 0) then
      return
   end

   for idx,comment in ipairs(Comments) do
      DoComment(IDItem.CreateSub(ID, GetCommentUIDString(Comments) .. "_" .. idx), comment, idx)
   end
end
