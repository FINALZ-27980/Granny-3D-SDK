require "id_item_cache"
require "stack"
require "rect"
require "point"
require "drag_context"
require "interactions"
require "console"

local CloseAction = "CloseAction"
local DescendAction = "DescendAction"

local function MakeSetStartState(Item)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, RButtonDown, false)
      Event = WaitForEvent(Event, RButtonUp, true)

      if (Event.Point and RectContainsPoint(Item.StartStateRect, Event.Point)) then
         Edit_PushUndoPos("Set start state", UndoClass.NeverCollapse)
         Edit_SetDefaultState(Item.NodeID)
      end

      Item.SetStartStateAction = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.SetStartStateAction = nil
                     end }
end

local function MakeSetState(Item)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, RButtonDown, false)
      Event = WaitForEvent(Event, RButtonUp, true)

      if (Event.Point and RectContainsPoint(Item.CurrStateRect, Event.Point)) then
         Edit_PushUndoPos("Set state", UndoClass.NeverCollapse)
         Edit_SetActiveState(Item.NodeID)
      end

      Item.SetStateAction = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.SetStateAction = nil
                     end }
end

local function MakeNodeMoveOrEnter(Item)
   local function MoveCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      if (not (Event.Ctrl == true or Event.Shift == true)) then
         -- Only check double click if the ctrl/shift keys are off
         if (Edit_CanDescendNode(Item.NodeID)) then
            if (not Item.LastClick) then
               Item.LastClick = Edit_GetAbsoluteTime()
            else
               local CurrTime = Edit_GetAbsoluteTime()
               local Time = CurrTime - Item.LastClick
               if (Edit_IsDClick(Time)) then
                  Event = WaitForEvent(Event, LButtonUp, true)

                  SetCloseEdits()
                  Edit_DescendNode(Item.NodeID)
                  Item.MoveAction = nil
                  return
               else
                  Item.LastClick = CurrTime
               end
            end
         end

         if (Edit_IsSelectedByID(Item.NodeID) == false) then
            Edit_PushUndoPos("Move node", UndoClass.NodeSelect)
            Edit_ClearSelection()
            Edit_SelectByID(Item.NodeID)
            Edit_MoveToFrontByID(Item.NodeID)
         end
      else
         Item.LastClick = nil

         Edit_PushUndoPos("Move node", UndoClass.NodeSelect)
         Edit_ToggleSelectByID(Item.NodeID)
      end

      Item.DragPt      = Item.CurrPt
      Item.Dragging    = true
      Item.EventAnchor = CopyPoint(Event.Point)

      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Point) then
                                 Item.DragPt = MakePoint(Item.CurrPt.x + (Event.Point.x - Item.EventAnchor.x),
                                                         Item.CurrPt.y + (Event.Point.y - Item.EventAnchor.y))
                              end
                           end)

      Item.CurrPt     = Item.DragPt
      Item.Dragging   = false
      Item.MoveAction = nil
   end

   return { Action = coroutine.create(MoveCoroutine),
            Abort  = function ()
                        Item.Dragging = false
                        Item.MoveAction = nil
                     end }
end

local function MakeNodeClick(Item, ActionName, Rect, Action)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)
      Event = WaitForEvent(Event, LButtonUp, true)

      if (Event.Point and RectContainsPoint(Rect, Event.Point)) then
         Action()
      end

      Item[ActionName] = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item[ActionName] = nil
                     end }
end


local function MakeTransitionDrag(Item, Rect, Type)
   local function SourceAction(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      Item.DraggingTransition = true
      Item.CheckMultiple = Event.Ctrl
      
      local StartPt = MakePoint(Event.Point.x - Rect.x,
                                Event.Point.y - Rect.y)
      DragContext.StartDrag(Type, UIArea.TransformFromCapture(Item.Capture, Event.Point))

      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Type == KeyDown and Event.Key == 27) then
                                 Interaction.Abort()
                              elseif (Event.Point) then
                                 local NewPoint = UIArea.TransformFromCapture(Item.Capture, Event.Point)
                                 DragContext.CurrentPoint(NewPoint)
                              end
                           end)

      local Targets = DragContext.GetItems(Type)
      Stack.Iterate(Targets,
                    function (Target)
                       local TargetRelPt = DragContext.TargetContainsPoint(Target)
                       if (TargetRelPt) then
                          if (Item.NodeID ~= Target.Value.ID) then
                             Edit_AddTransition(Item.NodeID, StartPt,
                                                Target.Value.ID, TargetRelPt)
                          end

                          -- If we are part of a selection, zip through it and add
                          -- connections from all nodes selected
                          if (Item.CheckMultiple and Edit_IsSelectedByID(Item.NodeID)) then
                             local Sel = Edit_GetSelection()
                             for i,it in ipairs(Sel) do
                                if (it ~= Item.NodeID and it ~= Target.Value.ID) then
                                   Edit_AddTransition(it, StartPt, Target.Value.ID, TargetRelPt)
                                end
                             end
                          end
                          
                          return true
                       end
                    end)

      DragContext.EndDrag(Type)
      Item.DraggingTransition = false
      Item.TransitionDrag = nil
   end

   return { Action = coroutine.create(SourceAction),
            Abort = function ()
                       if (Item.DraggingTransition) then
                          DragContext.EndDrag(Type)
                       end

                       Item.DraggingTransition = false
                       Item.TransitionDrag = nil
                    end }
end


local function MakeTarget(Item, EdgeIndex, Rect)
   local function TargetAction(Event)
      Event = WaitForEvent(Event, RButtonDown, false)
      Item.ActiveTargetAction = EdgeIndex
      Event = WaitForEvent(Event, RButtonUp, true)

      if (Event.Point and RectContainsPoint(Rect, Event.Point)) then
         Edit_DisconnectEdge(Item.NodeID, EdgeIndex)
      end

      Item.ActiveTargetAction = nil
      Item.TargetActions[EdgeIndex] = nil
   end

   return { Action = coroutine.create(TargetAction),
            Abort = function ()
                       if (Item.ActiveTargetAction == EdgeIndex) then
                          Item.ActiveSourceAction = nil
                       end
                       Item.TargetActions[EdgeIndex] = nil
                    end }
end

local function MakeSource(Item, EdgeIndex, Type, ConnectionFn)
   local function SourceAction(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      -- Dragging an edge triggers the selection
      if (Edit_IsSolelySelectedByID(Item.NodeID) == false) then
         Edit_PushUndoPos("Change selection", UndoClass.NodeSelect)
      end

      Edit_ClearSelection()
      Edit_SelectByID(Item.NodeID)


      Item.ActiveSourceAction = EdgeIndex
      DragContext.StartDrag(Type, UIArea.TransformFromCapture(Item.AreaCapture[EdgeIndex], Event.Point))

      Event = WaitForEvent(Event, LButtonUp, true,
                           function (Event)
                              if (Event.Type == KeyDown and Event.Key == 27) then
                                 Interaction.Abort()
                              elseif (Event.Point) then
                                 local NewPoint = UIArea.TransformFromCapture(Item.AreaCapture[EdgeIndex], Event.Point)
                                 DragContext.CurrentPoint(NewPoint)
                              end
                           end)

      local Targets = DragContext.GetItems(Type)
      Stack.Iterate(Targets,
                    function (Target)
                       if (DragContext.TargetContainsPoint(Target)) then
                          if (ConnectionFn) then
                             ConnectionFn(Item.NodeID, EdgeIndex,
                                          Target.Value.ID, Target.Value.EdgeIndex)
                          end

                          return true
                       end
                    end)

      DragContext.EndDrag(Type)
      Item.ActiveSourceAction = nil
      Item.SourceActions[EdgeIndex] = nil
   end

   return { Action = coroutine.create(SourceAction),
            Abort = function ()
                       if (Item.ActiveSourceAction == EdgeIndex) then
                          DragContext.EndDrag(Type)
                          Item.ActiveSourceAction = nil
                       end

                       Item.SourceActions[EdgeIndex] = nil
                    end }
end



--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- The actual registration functions below...
--------------------------------------------------------------------------------
local function ItemStorageFor(NodeID)
   return IDItem.Get(IDItem.CreateSub(NodeID, "storage"))
end
   

local function MakeEditClick(Item)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, RButtonDown, false)
      Event = WaitForEvent(Event, RButtonUp, true)

      if (RectContainsPoint(Item.EditRect, Event.Point)) then
         Item.EditActive = not Item.EditActive
      end

      Edit_MoveToFrontByID(Item.NodeID)
      Item.EditClickAction   = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.EditClickAction = nil
                     end }
end


function RegisterNodeMove(NodeID, Rect, EditRect, BasePt, IsDragTarget)
   -- Must match above...
   local Item, IsNew = ItemStorageFor(NodeID)

   if (IsNew) then
      Item.NodeID = NodeID
      Item.EditActive = false
   end

   if (not Item.Dragging) then
      Item.CurrPt = CopyPoint(BasePt)
   end

   Item.Capture = UIArea.Capture()
   Item.EditRect = EditRect
   if (Item.EditRect and not Item.EditActive) then
      ToolTip.Register("Right click to edit node parameters", EditRect)
      DrawRectOutline(EditRect, MakeColor(0.25, 0.25, 0.25))
   end

   if (IsDragTarget) then
      if (DragContext.IsActive(CreateStateDragType)) then
         DragContext.MakeTargetEntry(CreateStateDragType, Rect, NodeID)
         
         local Val = 0.5 * (math.sin(Edit_GetAbsoluteTime() * 2 * math.pi) + 1)/2
         local RectColor = MakeColorA(1, 1, 1, Val)
         DrawRect(Rect, RectColor)
      end
   end
   
   if (not Interactions.Active()) then
      Item.MoveAction = Interactions.RegisterClipped(Item.MoveAction or MakeNodeMoveOrEnter(Item), Rect)

      if (Item.EditRect) then
         Item.EditClickAction = Interactions.RegisterClipped(Item.EditClickAction or MakeEditClick(Item), EditRect)
      end
   end

   if (Item.EditActive and ShouldCloseEdits()) then
      Item.EditActive = false
   end
      
   if (Item.EditActive) then
      DeferredRender.Register(
         function ()
            local UL, ClippedUL =
               UIAreaTransformFromCapture(Item.Capture, MakePoint(EditRect.x, EditRect.y))
            local LL, ClippedLL =
               UIAreaTransformFromCapture(Item.Capture, MakePoint(EditRect.x, EditRect.y + EditRect.h))

            if (not (ClippedUL or ClippedLL)) then
               Item.EditDesiredRect = IPadDialog.DoHorizontal(IDItem.CreateSub(NodeID, "editbox"),
                                                              NodeID,
                                                              UL.x, UL.y + math.floor(EditRect.h/2),
                                                              475, 600,
                                                              Item.EditDesiredRect, false)
            end
         end)
   end

   if (Item.Dragging) then
      return Item.DragPt, true
   else
      return Item.CurrPt, false
   end
end


function RegisterStateOnlyInteractions(NodeID, Rect, StartStateRect, CurrStateRect)
   -- Must match above...
   local Item, IsNew = ItemStorageFor(NodeID)

   if (IsNew) then
      Item.NodeID = NodeID
      Item.EditActive = false
   end

   Item.TransitionRect = Rect
   Item.StartStateRect = StartStateRect
   Item.CurrStateRect  = CurrStateRect

   if (not Interactions.Active()) then
      Item.TransitionDrag =
         Interactions.RegisterClipped(Item.TransitionDrag or
                                      MakeTransitionDrag(Item, Rect, TransitionDragType), Rect)

      Item.SetStartStateAction = Interactions.RegisterClipped(Item.SetStartStateAction or MakeSetStartState(Item), StartStateRect)
      Item.SetStateAction = Interactions.RegisterClipped(Item.SetStateAction or MakeSetState(Item), CurrStateRect)
   end

   -- Register this with the drag context.  Note that even if we are dragging, we register
   -- ourselves as a target.  This allows us to occlude nodes that are beneath us
   -- visually...
   DragContext.MakeTargetEntry(TransitionDragType, Rect, { ["ID"] = NodeID } )
end


local function DistanceToSeg(Point, Start, End)
   local v = SubPoint(End, Start)
   local w = SubPoint(Point, Start)

   local c1 = DotPoint(v,w)
   if (c1 <= 0) then
      return DistPoint(Point, Start)
   end

   local c2 = DotPoint(v,v)
   if (c2 <= c1) then
      return DistPoint(Point, End)
   end

   local b = c1 / c2

   local ClosePoint = MakePoint(Start.x + b * v.x,
                                Start.y + b * v.y)
   return DistPoint(Point, ClosePoint)
end



SelectTransitionDist = 4
local function MakeSelectTransition(Item, StartPt, EndPt)
   local function ClickCoroutine(Event)

      while (true) do
         if (Event.Type == LButtonDown or Event.Type == RButtonDown) then
            local Dist = DistanceToSeg(Event.Point, StartPt, EndPt)
            if (Dist < 4) then
               break
            end
         end
         Event = Interactions.Continue(false)
      end

      local WaitType = UpEventFor(Event.Type)

      if (Edit_IsSolelySelectedByID(Item.TransitionID) == false) then
         Edit_PushUndoPos("Change selection", UndoClass.NodeSelect)
      end

      Edit_ClearSelection()
      Edit_SelectByID(Item.TransitionID)
      if (Event.Type == RButtonDown) then
         Edit_TriggerTransition(Item.TransitionID)
      elseif (Event.Type == LButtonDown) then
         Item.EditActive = not Item.EditActive
      end
      Event = WaitForEvent(Event, WaitType, true)

      Item.SelectAction = nil
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.SelectAction = nil
                     end }
end

function CopyTransitionEditState(TransitionID, NewTransitionID)
   local Item, IsNew = ItemStorageFor(TransitionID)
   if (IsNew) then
      -- If the "old" transition is not in the cache, ignore the copy
      return
   end
   
   local NewItem, NewItemIsNew = ItemStorageFor(NewTransitionID)
   if (NewItemIsNew) then
      NewItem.TransitionID = NewTransitionID
   end

   NewItem.EditActive = Item.EditActive
end


function RegisterTransition(TransitionID, StartPt, EndPt)
   local Item, IsNew = ItemStorageFor(TransitionID)

   if (IsNew) then
      Item.TransitionID = TransitionID
      Item.EditActive = false
   end

   Item.Capture = UIArea.Capture()

   local MinX = math.min(StartPt.x, EndPt.x)
   local MaxX = math.max(StartPt.x, EndPt.x)
   local MinY = math.min(StartPt.y, EndPt.y)
   local MaxY = math.max(StartPt.y, EndPt.y)

   local Width  = MaxX - MinX
   local Height = MaxY - MinY

   if (Width < 10) then
      local Diff = 10 - Width
      MinX = MinX - math.ceil(Diff/2)
      Width = Width + Diff
   end

   if (Height < 10) then
      local Diff = 10 - Height
      MinY = MinY - math.ceil(Diff/2)
      Height = Height + Diff
   end

   local CheckRect = MakeRect(MinX, MinY, Width, Height)
   -- DrawRect(CheckRect, MakeColorA(1, 0, 0, 0.25))
   if (not Interactions.Active()) then
      Item.SelectAction = Interactions.RegisterClipped(Item.SelectAction or MakeSelectTransition(Item, StartPt, EndPt),
                                                       CheckRect)
   end

   if (Item.EditActive and ShouldCloseEdits()) then
      Item.EditActive = false
   end

   if (Item.EditActive) then
      DeferredRender.Register(
         function ()
            local Pt, Clipped =
               UIAreaTransformFromCapture(Item.Capture, MakePoint(math.floor((StartPt.x + EndPt.x)/2),
                                                                  math.floor((StartPt.y + EndPt.y)/2)))
            if (not Clipped) then
               Item.EditDesiredRect = IPadDialog.DoHorizontal(IDItem.CreateSub(TransitionID, "editbox"),
                                                              TransitionID,
                                                              Pt.x, Pt.y,
                                                              475, 600,
                                                              Item.EditDesiredRect, true)
            end
         end)
   end
end



function RegisterNodeClose(NodeID, Rect)
   local Item, IsNew = ItemStorageFor(NodeID)

   if (IsNew) then
      Item.NodeID = NodeID
   end

   if (not Interactions.Active()) then
      Item[CloseAction] =
         Item[CloseAction] or MakeNodeClick(Item, CloseAction, Rect,
                                            function()
                                               Edit_DeleteNode(NodeID)
                                            end)
      Interactions.RegisterClipped(Item[CloseAction], Rect)
   end
end


function RegisterNodeInput(NodeID, Rect, EdgeIndex, Type)
   local Item, IsNew = ItemStorageFor(NodeID)
   if (IsNew) then
      Item.NodeID = NodeID
   end

   Item.TargetActions = Item.TargetActions or { }

   -- Register this with the drag context
   DragContext.MakeTargetEntry(Type, Rect, { ["ID"] = NodeID, ["EdgeIndex"] = EdgeIndex } )

   if (not Interactions.Active()) then
      Item.TargetActions[EdgeIndex] =
         Interactions.RegisterClipped(Item.TargetActions[EdgeIndex] or MakeTarget(Item, EdgeIndex, Rect),
                                      Rect)
   end

   return DragContext.IsActive(Type)
end

function RegisterNodeOutput(NodeID, Rect, EdgeIndex, Type)
   local Item, IsNew = ItemStorageFor(NodeID)
   if (IsNew) then
      Item.NodeID = NodeID
   end

   Item.AreaCapture   = Item.AreaCapture or { }
   Item.SourceActions = Item.SourceActions or { }

   Item.AreaCapture[EdgeIndex] = UIArea.Capture()

   -- Register our drag action
   if (not Interactions.Active()) then
      Item.SourceActions[EdgeIndex] =
         Interactions.RegisterClipped(Item.SourceActions[EdgeIndex] or MakeSource(Item, EdgeIndex, Type, Edit_ConnectNodes),
                                      Rect)
   end

   return (Item.ActiveSourceAction == EdgeIndex)
end


local CloseEdits = false

local EditHotkeys = {
   { KeyString   = "<Left>",
     Description = "Align Selection Left",
     EventSpecs  = { { Type = DirKeyDown, Key = Direction_KeyLeft, Alt = false } },
     FnDown      = function() AlignSelectionFromKey(Direction_KeyLeft) end },
   { KeyString   = "<Right>",
     Description = "Align Selection Right",
     EventSpecs  = { { Type = DirKeyDown, Key = Direction_KeyRight, Alt = false } },
     FnDown      = function() AlignSelectionFromKey(Direction_KeyRight) end },
   { KeyString   = "<Up>",
     Description = "Align Selection Up",
     EventSpecs  = { { Type = DirKeyDown, Key = Direction_KeyUp, Alt = false } },
     FnDown      = function() AlignSelectionFromKey(Direction_KeyUp) end },
   { KeyString   = "<Down>",
     Description = "Align Selection Down",
     EventSpecs  = { { Type = DirKeyDown, Key = Direction_KeyDown, Alt = false } },
     FnDown      = function() AlignSelectionFromKey(Direction_KeyDown) end },

   { KeyString   = "Backspace or Delete",
     Description = "Delete selection",
     EventSpecs  = { { Type = CtrlKeyDown, Key = Control_KeyBackspace },
                     { Type = CtrlKeyDown, Key = Control_KeyDelete } },
     Fn          = function() Edit_DeleteSelection() end },
   { KeyString   = "<Esc>",
     Description = "Close all edit dialogs",
     EventSpecs  = { { Type = CtrlKeyDown, Key = Control_KeyEscape } },
     Fn          = function()
                      CloseEdits = true
                   end }
}

function SetCloseEdits()
   CloseEdits = true
end

function ClearCloseEdits()
   CloseEdits = false
end

function ShouldCloseEdits()
   return CloseEdits
end


function StateMachineHotkeys(ID)
   Hotkeys.Register(IDItem.CreateSub(ID, "SM_Hotkeys"),
                    "State Machine",
                    EditHotkeys)
end


function BlendGraphHotkeys(ID)
   Hotkeys.Register(IDItem.CreateSub(ID, "BG_Hotkeys"),
                    "Blend Graph",
                    EditHotkeys)
end

