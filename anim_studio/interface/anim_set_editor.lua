require "rect"
require "event"
require "ui_splitter"
require "ui_button"
require "ui_scrollarea"
require "ui_editbox"
require "ui_hotkeys"

local DragTypeSource    = 0xcafe
local DragTypeAnimation = 0xbeef

-- Used by the handler to terminate the dialog
local EscPosted = false

AnimSetTempPadding = 5
AnimSetTempInternalPadding = 5
AnimationTempAnimIndent = 20

AnimSetSplitFactor = 0.4
AnimSetPreviewSize = 0.4

AnimSetTempSlotMidpoint = 0.4

local SourceScrollOffset = { x = 0, y = 0 }
local SlotScrollOffset = { x = 0, y = 0 }

-- Current list of animation sets
local AnimationSetNames = { }
local CurrAnimationSet = 0

-- For the current animation set, the list of source files
--  SourceList[idx] =
--  { Path     = FullName,           -- "c:/art/model/anim.gr2"
--    Relative = RelativePath,       -- "../model/anim.gr2"
--    Root     = RootName,           -- "anim.gr2"
--    ValidAnims = {
--      { Index, Duration, ShortName, FullName },
--      { Index, Duration, ShortName, FullName },
--      { Index, Duration, ShortName, FullName },
--    }

-- !!! or for invalid sources... !!!
--    ValidAnims = {
--      { ShortName },
--    }

--  }
local SourceList = { }

-- For the current animation set, the list of animation slots
-- AnimationSlotList[Idx] =
-- { Name   = SlotName,   -- "Run", "Walk", "EpicDeath" (editable)
--   ExpectedName = nil or Expected name of anim, mismatch = bad load!
--   SourceIdx = nil or SourceIdx -- Indexes SourceList
--   AnimIdx   = nil or AnimIdx   -- Indexes SourceList[SourceIdx].ValidAnims
-- }
local AnimationSlotList = { }

local BackColors = {
   MakeColor(0.25, 0.25, 0.25),
   MakeColor(0.3,  0.3,  0.3),

   MakeColor(0.25, 0.30, 0.25),
   MakeColor(0.3,  0.35,  0.3),

}
local function AlternateColor(idx, drag_active)
   local ColorIdx = (idx % 2) + 1
   if (drag_active) then
      ColorIdx = ColorIdx + 2
   end

   return BackColors[ColorIdx]
end


local AnimPreviewSource = nil
local AnimPreviewAnimation = nil
local function RenderPreview(ID)
   PreviewCharacterPane(IDItem.CreateSub(ID, "preview"),
                        AnimPreviewSource or 0,
                        AnimPreviewAnimation or 0)

   AnimPreviewSource = nil
   AnimPreviewAnimation = nil
end

-- "Add animation"
local AddLabel = "Add Animation..."
local AddW, AddH = Button.Dim(AddLabel)

-- "Add animation set"
local PlusTip = ("Creates a new animation set for the character.  If you\n" ..
                 "hold 'Shift' while clicking this, the animation set will\n" ..
                 "start as a copy of the currently selected set")
local MinusTip = "Delete the current animation set. (Undoable)"

local PlusLabel = "+"
local PlusW, PlusH = Button.Dim(PlusLabel)

-- "Delete this animation"
local FileDeleteLabel = "x"
local FileDeleteW, FileDeleteH = Button.DimSmall(FileDeleteLabel)
local SourceDeleteTip = "Delete this source file"

-- Drawing resources for the animation list
local FileFontHandle        = UIPref.GetFont("anim_list_file")
local FileFontH, FileFontBL = GetFontHeightAndBaseline(FileFontHandle)

local AnimFontHandle        = UIPref.GetFont("anim_list_anim")
local AnimFontH, AnimFontBL = GetFontHeightAndBaseline(AnimFontHandle)

local FileBackingGrid       = NineGrid_GetHandle("file_backing")

-- Drawing resources for the slot list
local HeaderFontHandle = UIPref.GetFont("slot_list_header")
local HeaderFontH, HeaderFontBL = GetFontHeightAndBaseline(HeaderFontHandle)
local HeaderText     = UIPref.GetColor("slot_header_text")
local HeaderTextDrop = UIPref.GetColor("slot_header_text_drop")

local SlotEntryFontHandle = UIPref.GetFont("slot_list_entry")
local SlotEntryFontH, SlotEntryFontBL = GetFontHeightAndBaseline(HeaderFontHandle)
local SlotEntryHeight = Editbox.RecommendedHeight(SlotEntryFontHandle)

local DragTargetLabel = "[[...Drag Target...]]"
local DragTargetLabelW = GetSimpleTextDimension(SlotEntryFontHandle, DragTargetLabel)

local DragSlotTip = ("Note that you can create new animation slots\n" ..
                     "by dragging a source or animation into this\n" ..
                     "empty space in the slot list")

local DeleteSlotTip = ("Deletes this animation slot")

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
local function RefreshState(ZeroScrolls)
   AnimationSetNames, CurrAnimationSet = Edit_GetAnimationSets()
   SourceList = Edit_GetSources()

   AnimationSlotList = Edit_GetAnimationSlots()

   if (ZeroScrolls) then
      SourceScrollOffset = { x=0, y=0 }
      SlotScrollOffset = { x=0, y=0 }
   end

   AnimPreviewSource = nil
   AnimPreviewAnimation = nil
end

local function EditCallback(Entry, ChangedText)
   if (Edit_SetAnimationSetName(Entry, ChangedText)) then
      AnimationSetNames[Entry] = ChangedText
   end
end

local function DeleteAnimationSet()
   Edit_DeleteCurrentAnimationSet();
   RefreshState(true)
end

local function AddAnimationSet(Modifiers)
   if (Modifiers.Shift) then
      Edit_CopyAnimationSet()
   else
      Edit_NewAnimationSet()
   end

   RefreshState(true)
end

local function RemoveAnimationFile(File)
   if (Edit_RemoveSource(File)) then
      RefreshState(false)
   end
end


local function AddAnimationFile(Modifiers)
   if (Edit_AddSource()) then
      RefreshState(false)
   end
end


local function MakeSourceDrag(Item, Type, Action)
   local function SourceAction(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.DraggingSource = true
      local StartPt = MakePoint(Event.Point.x,
                                Event.Point.y)
      DragContext.StartDrag(Type, UIArea.TransformFromCapture(Item.AreaCapture, Event.Point))

      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 local NewPoint = UIArea.TransformFromCapture(Item.AreaCapture, Event.Point)
                                 DragContext.CurrentPoint(NewPoint)
                              end
                           end)

      local Targets = DragContext.GetItems(Type)
      Stack.Iterate(Targets,
                    function (Target)
                       local TargetRelPt = DragContext.TargetContainsPoint(Target)
                       if (TargetRelPt) then
                          Action(Item, Target.Value)
                          return true
                       end
                    end)

      DragContext.EndDrag(Type)
      Item.DraggingSource = false
      Item.SourceDrag = nil
   end

   return { Action = coroutine.create(SourceAction),
            Abort = function ()
                       if (Item.DraggingSource) then
                          DragContext.EndDrag(Type)
                       end

                       Item.DraggingSource = false
                       Item.SourceDrag = nil
                    end }
end

-- local function MakeReplaceSource(Item, Rect, Index)
--    local function Action(Event)
--       local UIRect = CopyRect(Rect)
--       Event = WaitForEvent(Event, RButtonDown, false)
-- 
--       local Down = true
--       Event = WaitForEvent(Event, RButtonUp, true,
--                            function (Event)
--                               if (Event.Point) then
--                                  Down = RectContainsPoint(UIRect, Event.Point)
--                               end
--                            end)
--       if (Down) then
--          if (Edit_ReplaceSource(Index-1)) then
--             RefreshState(false);
--          end
--       end
-- 
--       Item.ReplaceAction = nil
--    end
-- 
--    return { Action = coroutine.create(Action),
--             Abort  = function ()
--                         Item.ReplaceAction = nil
--                      end }
-- end


local function AllAnimsAsSlots(idx)
   if (Edit_AddSlotsForSource(idx)) then
      RefreshState(false)
   end
end

local function AddAnimAsSlot(SourceIdx, AnimIdx)
   if (Edit_AddSlotForAnimation(SourceIdx, AnimIdx)) then
      RefreshState(false)
   end
end

local function SetAnimForSlot(SlotIdx, SourceIdx, AnimIdx)
   if (Edit_SetAnimationForSlot(SlotIdx, SourceIdx, AnimIdx)) then
      RefreshState(false)
   end
end

-- Layout is:
--
--  x Source   Anim0
--             Anim1
--             Anim2

local function RenderSourceFile(ID, Entry, Index)
   local UIRect = UIArea.Get()
   
   TextColor = MakeColor(1, 1, 1)
   TipString = ("Path: " .. Entry.Path .. "\n" .. "Relative Path: " .. Entry.Relative)
   if (not Entry.Loaded) then
      TipString =
         (TipString .. "\n" ..
          "\n" ..
          "  Note: This file was NOT correctly loaded\n" ..
          "  You should delete this source and reload.\n")
      TextColor = MakeColor(1, 0, 0)
   end

   RenderText(FileFontHandle, Entry.Root, 0, MakePoint(0, FileFontBL), TextColor)
   ToolTip.Register(TipString, UIRect)

   local Item, IsNew = IDItem.Get(IDItem.CreateSub(ID, #SourceList .. "source" .. Index))
   Item.AreaCapture = UIArea.Capture()

   if (not Interactions.Active()) then
      if (Entry.Loaded) then
         Item.SourceDrag =
            Interactions.RegisterDefClipped(Item.SourceDrag or
                                            MakeSourceDrag(Item, DragTypeSource,
                                                           function (Item, Value)
                                                              AllAnimsAsSlots(Index)
                                                           end))
      end
   end

   -- Draw the drag, if we need to...
   if (Item.DraggingSource and DragContext.IsActive(DragTypeSource)) then
      local St, Pt = DragContext.GetDragPoints()
      local TextPt = MakePoint(Pt.x + AnimSetTempInternalPadding,
                               Pt.y + FileFontBL + AnimSetTempInternalPadding)
      DeferredRender.Register(
         function ()
            local w,h = GetSimpleTextDimension(FileFontHandle, Entry.Root)

            local NumEntriesLabel = "(" .. #Entry.ValidAnims .. " animations)"
            local entriesW = GetSimpleTextDimension(AnimFontHandle, NumEntriesLabel)
            
            NineGrid_Draw(FileBackingGrid,
                          MakeRect(Pt.x,
                                   Pt.y,
                                   w + 2*AnimSetTempInternalPadding,
                                   h + 2*AnimSetTempInternalPadding))

            local SourceW = RenderText(FileFontHandle, Entry.Root, 0, TextPt, MakeColor(0, 0, 0))

            local NumAnimLines = 2 + math.min(#Entry.ValidAnims, 10)
            local AnimX = TextPt.x + SourceW + AnimSetTempInternalPadding
            local AnimY = TextPt.y - Pt.y

            local AnimBox = MakeRect(AnimX, Pt.y,
                                     entriesW + 2 * AnimSetTempInternalPadding,
                                     NumAnimLines * AnimFontH)
            NineGrid_Draw(FileBackingGrid, AnimBox)

            AnimBox.x = AnimBox.x + AnimSetTempInternalPadding
            AnimBox.w = AnimBox.w - 2*AnimSetTempInternalPadding
            AnimBox.h = AnimBox.h - AnimSetTempInternalPadding
            UIArea.ProtectedSimplePush(
               AnimBox,
               function()
                  RenderText(AnimFontHandle, "(" .. #Entry.ValidAnims .. " animations)", 0,
                             MakePoint(0, AnimY),
                             MakeColor(0, 0, 0))
                  AnimY = AnimY + AnimFontH
                  
                  for aidx,anim in ipairs(Entry.ValidAnims) do
                     RenderText(AnimFontHandle, anim.ShortName, 0,
                                MakePoint(0, AnimY),
                                MakeColor(0.1, 0.1, 0.1))
                     AnimY = AnimY + AnimFontH
                     -- Don't draw all of them...
                     if (aidx > 10) then
                        break
                     end
                  end
               end)
         end)
   end
end

local function RenderEntry(ID, Entry, Index, CurrY, ColumnSplitPos)
   local IndentX     = AnimSetTempPadding
   
   local w,TextH = GetSimpleTextDimension(FileFontHandle, Entry.Root)

   -- X for kill button
   local KillRect = MakeRect(AnimSetTempInternalPadding,
                             CurrY,
                             FileDeleteW, FileDeleteH)


   local LineH = math.max(TextH, KillRect.h)
   local TotalHeight = (LineH * #Entry.ValidAnims)
   local TotalRect   = MakeRect(0, CurrY, 2000, TotalHeight + 2*AnimSetTempInternalPadding)
   DrawRect(TotalRect, AlternateColor(Index, false))

   Button.DoSmall(IDItem.CreateSub(ID, "kill anim" .. Index),
                  KillRect, FileDeleteLabel, false,
                  function(Modifiers)
                     RemoveAnimationFile(Entry.Relative)
                  end)
   ToolTip.Register(SourceDeleteTip, KillRect)
   
   -- Render the source name
   --  register the tooltip for the full info
   --  register the drag rect
   --  if this item is currently being dragged, draw it
   local FileRect = MakeRect(KillRect.x + KillRect.w + AnimSetTempInternalPadding, CurrY, 0, LineH)
   FileRect.w = ColumnSplitPos - FileRect.x
   UIArea.ProtectedSimplePush(
      FileRect,
      function ()
         RenderSourceFile(ID, Entry, Index)
      end)

   -- Essentially inf width, but height tracking the number of valid animations
   if (#Entry.ValidAnims == 0) then
      CurrY = CurrY + LineH
      return CurrY
   end
   
   -- Draw the list of animations...
   local AnimationsRect = MakeRect(ColumnSplitPos, CurrY + (FileFontBL - AnimFontBL),
                                   2000, (LineH * #Entry.ValidAnims))

   for aidx,anim in ipairs(Entry.ValidAnims) do
      local AnimRect = MakeRect(AnimationsRect.x,
                                AnimationsRect.y + ((aidx-1) * LineH),
                                AnimationsRect.w,
                                LineH)
      DrawRect(AnimRect, AlternateColor(Index+aidx, false))

      UIArea.ProtectedSimplePush(
         AnimRect,
         function()
            local EntryItem, IsNew = IDItem.Get(IDItem.CreateSub(ID, #SourceList .. "source" .. Index .. "anim" .. aidx))
            if (IsNew) then
               if (anim.FullName) then
                  EntryItem.Tip = ("Full name: " .. anim.FullName .. "\n" ..
                                   "Duration: " .. string.format("%.4f", anim.Duration))
               else
                  EntryItem.Tip = ("This file is missing, or not a valid replacement\n" ..
                                   "for the original animation")
               end
            end

            RenderText(AnimFontHandle, anim.ShortName, 0, MakePoint(0, AnimFontBL), MakeColor(0, 1, 0))
            ToolTip.Register(EntryItem.Tip, UIArea.Get())

            -- Register drag of the animation
            EntryItem.AreaCapture = UIArea.Capture()
            if (not Interactions.Active() and anim.FullName) then
               EntryItem.SourceDrag =
                  Interactions.RegisterDefClipped(EntryItem.SourceDrag or
                                                  MakeSourceDrag(EntryItem, DragTypeAnimation,
                                                                 function (EntryItem, Value)
                                                                    if (Value.ID == "AddSlot") then
                                                                       AddAnimAsSlot(Index, aidx)
                                                                    else
                                                                       SetAnimForSlot(Value.SlotIdx, Index, aidx)
                                                                    end
                                                                 end))
            end

            -- Draw the drag, if we need to...
            if (EntryItem.DraggingSource and DragContext.IsActive(DragTypeAnimation)) then
               local St, Pt = DragContext.GetDragPoints()
               local TextPt = MakePoint(Pt.x, Pt.y + AnimFontBL)
               local w,h = GetSimpleTextDimension(FileFontHandle, anim.ShortName)
               
               AnimPreviewSource    = Index
               AnimPreviewAnimation = aidx
               DeferredRender.Register(
                  function ()
                     NineGrid_Draw(FileBackingGrid,
                                   MakeRect(Pt.x - AnimSetTempInternalPadding,
                                            Pt.y - AnimSetTempInternalPadding,
                                            w + 2*AnimSetTempInternalPadding,
                                            h + 2*AnimSetTempInternalPadding))
                     RenderText(AnimFontHandle, anim.ShortName, 0, TextPt, MakeColor(0, 0.35, 0))
                  end)
            end
         end)
   end

   CurrY = CurrY + TotalRect.h
   DrawRect(MakeRect(0, CurrY, 2000, 1), MakeColor(1, 1, 1))

   return CurrY
end


local function EditAnimSet(ID, ColumnSplitPos)
   local IndentX     = AnimSetTempPadding
   local AnimIndentX = AnimationTempAnimIndent
   local CurrY       = AnimSetTempPadding
   local MaxX        = 0
   local Drawn       = false

   for idx,entry in ipairs(SourceList) do
      Drawn = true

      CurrY = RenderEntry(ID, entry, idx, CurrY, ColumnSplitPos)
      CurrY = CurrY + AnimSetTempInternalPadding
   end

   return MakeRect(0, 0, MaxX, CurrY), Drawn
end

local function RenderSetInterface(ID)
   local Rect = CopyRect(UIArea.Get())
   Rect.y = Rect.y + AnimSetTempPadding
   Rect.h = Rect.h - AnimSetTempPadding

   local SetRect    = CopyRect(Rect)
   local ButtonRect = CopyRect(Rect)
   local SetDisplay = CopyRect(Rect)

   SetRect.w = SetRect.w - AddW - (2 * PlusW) - (3 * AnimSetTempPadding)
   SetRect.h = AddH

   local PlusButtonRect  = CopyRect(SetRect)
   PlusButtonRect.x = SetRect.x + SetRect.w + AnimSetTempPadding
   PlusButtonRect.w = PlusW
   
   local MinusButtonRect = CopyRect(PlusButtonRect)
   MinusButtonRect.x = PlusButtonRect.x + PlusButtonRect.w + AnimSetTempPadding
   
   ButtonRect.x = ButtonRect.w - AddW
   ButtonRect.w = AddW
   ButtonRect.h = AddH

   SetDisplay.h = SetDisplay.h - AddH - AnimSetTempPadding
   SetDisplay.y = SetDisplay.y + AddH + AnimSetTempPadding

   -- Make space for labels on the top to distinguish between sources and animations
   local ColumnSplit = SetDisplay.w / 2
   RenderText(HeaderFontHandle, "Source file", 0,
              MakePoint(SetDisplay.x + AnimSetTempPadding,
                        SetDisplay.y + HeaderFontBL),
              MakeColor(1, 1, 1))
   RenderText(HeaderFontHandle, "Animation", 0,
              MakePoint(SetDisplay.x + AnimSetTempPadding + ColumnSplit,
                        SetDisplay.y + HeaderFontBL),
              MakeColor(1, 1, 1))

   SetDisplay.h = SetDisplay.h - (HeaderFontH + AnimSetTempPadding)
   SetDisplay.y = SetDisplay.y + (HeaderFontH + AnimSetTempPadding)
   
   local Changed = false
   CurrAnimationSet, Changed =
      Combobox.DoEditable(IDItem.CreateSub(ID, "CurrentSet"), SetRect,
                          AnimationSetNames, CurrAnimationSet, EditCallback)
   if (Changed) then
      Edit_SetCurrentAnimationSet(CurrAnimationSet)

      RefreshState(true)
      SourceList = Edit_GetSources()
      AnimationSlotList = Edit_GetAnimationSlots()
   end

   local MinusDisabled = false
   if (#AnimationSetNames == 1) then
      MinusDisabled = true
   end
   
   Button.Do(IDItem.CreateSub(ID, "Plus"), PlusButtonRect, PlusLabel, false, AddAnimationSet)
   ToolTip.Register(PlusTip, PlusButtonRect)
   
   Button.DoArgs(IDItem.CreateSub(ID, "Minus"),   MinusButtonRect,
                 { Text = "-", Action = DeleteAnimationSet, Disabled = MinusDisabled })
   ToolTip.Register(MinusTip, MinusButtonRect)
   Button.Do(IDItem.CreateSub(ID, "AddAnim"), ButtonRect, AddLabel, false, AddAnimationFile)

   UIArea.ProtectedSimplePush(SetDisplay,
                              function ()
                                 Border.Do(IDItem.CreateSub(ID, "scrollborder"),
                                           1, 0, 1, 0,
                                           function (ID)
                                              SourceScrollOffset =
                                                 ScrollArea.Do(IDItem.CreateSub(ID, "Scroll" .. CurrAnimationSet),
                                                               SourceScrollOffset,
                                                               function (ID)
                                                                  return EditAnimSet(ID, ColumnSplit)
                                                               end)
                                           end)
                              end)
end

-- Fills the scroll list
local function SlotEntries(ID, MidPoint)
   local IndentX = AnimSetTempPadding

   local Area = UIArea.Get()

   local Colors = {
      MakeColor(0.40, 0.40, 0.40),
      MakeColor(0.40, 0.45, 0.40),
   }

   local CurrY = 0
   for i,v in ipairs(AnimationSlotList) do
      local WholeSlotRect = MakeRect(0, CurrY, Area.w, SlotEntryHeight)
      local SlotRect = MakeRect(0, CurrY, MidPoint, SlotEntryHeight)
      local NameRect = MakeRect(MidPoint, CurrY, Area.w - MidPoint, SlotEntryHeight)

      -- Only turn on the drag if something is happening...
      if (DragContext.IsActive(DragTypeAnimation)) then
         DragContext.MakeTargetEntry(DragTypeAnimation, NameRect, { ["SlotIdx"] = i } )
      end
      
      -- Draw rect, including target mod if dragging...
      DrawRect(WholeSlotRect, AlternateColor(i, false))
      DrawRect(NameRect, AlternateColor(i, DragContext.IsActive(DragTypeAnimation)))
      DrawRect(MakeRect(MidPoint, CurrY, 1, SlotEntryHeight), MakeColor(0, 0, 0))

      ToolTip.Register("Click to edit slot name",   SlotRect)
      ToolTip.Register("Drag animations to change\n", NameRect)

      -- Preview on hover, text color selection
      local TextColor = MakeColor(1, 1, 1)
      if (UIAreaRectContainsMouse(SlotRect)) then
         if (not AnimPreviewSource and not AnimPreviewAnimation) then
            AnimPreviewSource    = v.SourceIdx
            AnimPreviewAnimation = v.AnimIdx
         end

         if (not DragContext.IsDragging()) then
            TextColor = MakeColor(1, 0.75, 0.75)
         end
      end

      local DeleteWidth = 40
      local RebindWidth = 100
      local EditRect = MakeRect(IndentX, CurrY, MidPoint - IndentX - AnimSetTempPadding - DeleteWidth, SlotEntryHeight)
      local NewString, Changed, Success =
         Editbox.Do(IDItem.CreateSub(ID, "slotname" .. i),
                    EditRect,
                    v.Name,
                    SlotEntryFontHandle,
                    false)

      -- Button for delete
      local DeleteButtonRect = MakeRect(EditRect.x + EditRect.w,
                                        EditRect.y,
                                        DeleteWidth,
                                        EditRect.h);
      ToolTip.Register(DeleteSlotTip, DeleteButtonRect);
      Button.Do(IDItem.CreateSub(ID, "delete" .. i), DeleteButtonRect, "x", false,
                function ()
                   Edit_DeleteSlot(i)
                   RefreshState(false)
                end)


      local BindingX = MidPoint + AnimSetTempPadding
      if (v.SourceIdx) then
         if (not v.SourceString) then
            if (v.ExpectedName) then
               if (SourceList[v.SourceIdx].AllAnims[v.AnimIdx] ~= nil) then
                  v.SourceString = (SourceList[v.SourceIdx].AllAnims[v.AnimIdx].ShortName .. " in " .. SourceList[v.SourceIdx].Root)
                  v.Mismatch = (v.ExpectedName ~= SourceList[v.SourceIdx].AllAnims[v.AnimIdx].FullName)
               else
                  v.SourceString = (v.ExpectedName .. " in " .. SourceList[v.SourceIdx].Root)
                  v.Mismatch = true
               end
            else
               v.SourceString = (SourceList[v.SourceIdx].AllAnims[v.AnimIdx].ShortName ..
                                 " in " .. SourceList[v.SourceIdx].Root)
               v.Mismatch = false
            end
         end

         local Color = TextColor
         if (v.Mismatch == true) then
            Color = MakeColor(1, 0, 0)
            ToolTip.Register("This animation was not correctly loaded", NameRect)
         end
         
         RenderText(FileFontHandle, v.SourceString, 0,
                    MakePoint(BindingX, CurrY + FileFontBL),
                    Color)
      else
         if (v.Name) then
            local RebindRect = MakeRect(NameRect.x, NameRect.y, RebindWidth, NameRect.h);
            Button.Do(IDItem.CreateSub(ID, "rebind" .. i), RebindRect, "rebind", false,
                      function ()
                         for si,sv in ipairs(SourceList) do
                            for ai,av in ipairs(sv.ValidAnims) do
                               if (v.Name == av.FullName) then
                                  SetAnimForSlot(i, si, ai)
                               end
                            end
                         end
                      end)
         end
      end

      if (Changed and Success) then
         -- Might be changed for uniqueness
         local NewName = Edit_SetAnimationSlotName(i, NewString)
         if (NewName) then
            v.Name = NewName
         end
      end

      CurrY = CurrY + SlotEntryHeight
   end

   -- One more to act as a drag target...

   local DragTargetRect = MakeRect(0, CurrY, Area.w, 2 * SlotEntryHeight)
   if (DragContext.IsDragging()) then
      DragContext.MakeTargetEntry(DragTypeSource, DragTargetRect, { ["ID"] = "AddSlot" } )
      DragContext.MakeTargetEntry(DragTypeAnimation, DragTargetRect, { ["ID"] = "AddSlot" } )
   end
   
   ToolTip.Register(DragSlotTip, DragTargetRect)
   if (DragContext.IsDragging()) then
      DrawRect(DragTargetRect, Colors[2])
      RenderText(SlotEntryFontHandle, DragTargetLabel, 0,
                 MakePoint(MidPoint - math.floor(DragTargetLabelW / 2), CurrY + SlotEntryHeight/2 + SlotEntryFontBL),
                 MakeColor(0, 1, 0))
   else
      DrawRect(DragTargetRect, Colors[1])
   end
   CurrY = CurrY + DragTargetRect.h

   -- At the bottom, stick a button for adding a new slot
   CurrY = CurrY + AnimSetTempInternalPadding
   local w,h = Button.Dim("Add new animation slot")
   local Rect = MakeRect(IndentX, CurrY, w, h)
   Button.Do(IDItem.CreateSub(ID, "addslotbut"),
             Rect, "Add new animation slot", false,
             function (Modifiers)
                Edit_AddAnimationSlot("New Slot")
                
                RefreshState(false)
             end)
   CurrY = CurrY + h + AnimSetTempInternalPadding
   
   return MakeRect(0, 0, Area.w, CurrY), true
end

local function SlotEditor(ID)
   -- Draw Header
   -- Border for header
   -- ScrollArea
   --   For each slot
   --     register editable name
   --      tool tip to handle shorted entries
   --     show current binding
   --     drag target
   --     on hover, preview slot
   local IndentX = AnimSetTempPadding
   local CurrY   = AnimSetTempPadding

   local Area = UIArea.Get()
   local MidPoint = math.floor(Area.w * AnimSetTempSlotMidpoint)

   local HeaderArea = MakeRect(Area.x, Area.y, Area.w, HeaderFontH + 2*AnimSetTempPadding)
   UIArea.ProtectedSimplePush(
      HeaderArea,
      function()
         Border.Do(
            IDItem.CreateSub(ID, "border"), 0, 0, 0, 1,
            function (ID)
               local BorderArea = UIArea.Get()
               
               UIArea.ProtectedSimplePush(
                  MakeRect(AnimSetTempPadding, AnimSetTempPadding,
                           BorderArea.w - AnimSetTempPadding,
                           BorderArea.h - AnimSetTempPadding),
                  function()
                     RenderDropText(HeaderFontHandle, "Slot name", 0,
                                    MakePoint(0, HeaderFontBL),
                                    HeaderText, HeaderTextDrop)
                     RenderDropText(HeaderFontHandle, "Current binding", 0,
                                    MakePoint(MidPoint + IndentX, HeaderFontBL),
                                    HeaderText, HeaderTextDrop)
                  end)
               DrawRect(MakeRect(MidPoint, 0, 1, UIArea.Get().h), MakeColor(0, 0, 0))
            end)
      end)

   local ScrollRect = MakeRect(Area.x, HeaderArea.h, Area.w, Area.h - HeaderArea.h)
   UIArea.ProtectedSimplePush(
      ScrollRect,
      function ()
         SlotScrollOffset =
            ScrollArea.Do(IDItem.CreateSub(ID, "ScrollSlots" .. CurrAnimationSet),
                          SlotScrollOffset,
                          function (ID) return SlotEntries(ID, MidPoint) end)
      end)
end

local function DrawLeft(ID)
   local Rect = UIArea.Get()
   local Split = -math.floor(Rect.h * AnimSetPreviewSize)
   
   Splitter.DoHorizontal(IDItem.CreateSub(ID, "splith"),
                         Split, -1, false,
                         RenderSetInterface, RenderPreview)
end

local function DrawRight(ID)
   local Rect = CopyRect(UIArea.Get())
   Rect.w = Rect.w - 2*AnimSetTempPadding
   Rect.x = Rect.x + AnimSetTempPadding

   UIArea.ProtectedSimplePush(Rect,
                              function()
                                 Border.Do(IDItem.CreateSub(ID, "border"),
                                           2, 2, 2, 2, SlotEditor)
                              end)
end

local AnimSetHotkeys = {
   { KeyString   = "<Esc>",
     Description = "Close Set Editor",
     EventSpecs  = { { Type = CtrlKeyDown, Key = Control_KeyEscape },
                     { Type = KeyDown, Key = KeyForChar("a"), Ctrl = true } },
     Fn          = function() EscPosted = true end },

   { KeyString   = "Ctrl-Z",
     Description = "Undo",
     EventSpecs  = { { Type = KeyDown, Key = KeyForChar("z"), Ctrl = true } },
     Fn          = function()
                      Edit_PopUndo()
                      RefreshState(false) -- should we put the scrolls into the undo stack?
                   end }
}

local function RenderEditor(ID)
   local Item, IsNew = IDItem.Get(ID)

   -- Draw the title section for the animation sections
   local Rect = UIArea.Get()
   local Split = math.floor(Rect.w * AnimSetSplitFactor)

   Splitter.DoVertical(IDItem.CreateSub(ID, "splitv"),
                       Split, -1, false,
                       DrawLeft, DrawRight)

   Hotkeys.Register(IDItem.MakeID("AnimSetEditor", "Hotkeys"),
                    "Animation Set Editor",
                    AnimSetHotkeys)
end

AnimSetTempWLimit = 1400
AnimSetTempHLimit = 1000
local function DoYielding()
   -- Load up the current values...
   RefreshState(true)
   
   local function EventLoop(ID)
      while true do
         local UIRect = UIArea.Get()
         local DialogW = UIRect.w - 100
         local DialogH = UIRect.h - 100

         DialogW = math.min(DialogW, AnimSetTempWLimit)
         DialogH = math.min(DialogH, AnimSetTempHLimit)
         
         local DialogRect = MakeRect(math.floor((UIRect.w - DialogW) / 2),
                                     math.floor((UIRect.h - DialogH) / 2),
                                     DialogW, DialogH)
         local InternalRect = MakeRect((UIRect.w - DialogW) / 2 + AnimSetTempPadding,
                                       (UIRect.h - DialogH) / 2 + AnimSetTempPadding,
                                       DialogW - 2*AnimSetTempPadding, DialogH - 2*AnimSetTempPadding)

         -- Mask off the background
         DrawStippleRect(UIArea.Get(),
                         UIPref.GetColor("inaction_indicator_strong0"),
                         UIPref.GetColor("inaction_indicator_strong1"))

         -- Draw the dialog box
         NineGrid_Draw(NineGrid_GetHandle("dialog"), DialogRect)

         -- Render the editor itself
         UIArea.ProtectedSimplePush(InternalRect, function() RenderEditor(ID) end)

         if (EscPosted) then
            EscPosted = false
            coroutine.yield(true)
            return
         else
            coroutine.yield(false)
         end
      end
   end

   local EventCoro = coroutine.create(EventLoop)
   PushModal(EventCoro, "AnimSetEditor")
   Interactions.Continue(true)

   return GetModalResult()
end

AnimSetEditor = {
   DoYielding = DoYielding
}

