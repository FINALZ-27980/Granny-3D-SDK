require "ui_preferences"
require "id_item"
require "rect"
require "table"

local FontHandle    = UIPref.GetFont("tree_view")
local FontH, FontBL = GetFontHeightAndBaseline(FontHandle)

local OpenStr = "+ "
local PlusDim = GetSimpleTextDimension(FontHandle, OpenStr)

local StateClear   = 0
local StatePartial = 1
local StateSet     = 2

local function dumpTree(Tree, Prefix)
   print(Prefix .. Tree.Name, Tree.Selected)
   for k,v in pairs(Tree.Children) do
      dumpTree(v, Prefix .. "  ")
   end
end

local function StillValid(Tree, NodeID)
   -- todo
   return true
end

local function FillInTree(NodeID)
   local temp_table = MaskSource_GetBones(NodeID)
   local refs = { }

   for k,v in ipairs(temp_table) do
      refs[k] = { Name = v.Name,
                  Idx = k,
                  Open = false,
                  Selected = v.Selected,
                  Children = { } }
      if (v.ParentIdx ~= 0) then
         table.insert(refs[v.ParentIdx].Children, refs[k])
      end
   end

   refs[1].Open = true
   refs[1].Children[1].Open = true

   return refs[1]
end

local function ChildrenSelected(Tree)
   local OrMask  = false
   local AndMask = true
   for k,v in pairs(Tree.Children) do
      OrMask  = OrMask  or  v.Selected
      AndMask = AndMask and v.Selected

      local COR, CAND = ChildrenSelected(v)
      OrMask  = OrMask  or COR
      AndMask = AndMask and CAND
   end

   return OrMask, AndMask
end

local function SetWithChildren(Tree, State)
   Tree.Selected = State
   for k,v in pairs(Tree.Children) do
      SetWithChildren(v, State)
   end
end

local function DrawTree(Tree, StartPt)
   local PlusRects   = { }
   local StringRects = { }

   if (#Tree.Children ~= 0) then
      RenderText(FontHandle, OpenStr, 0,
                 MakePoint(StartPt.x, StartPt.y + FontBL), MakeColor(1, 1, 1))

      table.insert(PlusRects,
                   { Ref  = Tree,
                     Rect = MakeRect(StartPt.x, StartPt.y, PlusDim, FontH) } )
   end

   local OrMask  = Tree.Selected
   local AndMask = Tree.Selected

   local DrawState

   local Pt = MakePoint(StartPt.x, StartPt.y + FontH)
   if (Tree.Open) then
      Pt.x = Pt.x + PlusDim + 5

      for k,v in pairs(Tree.Children) do
         local DrawPt, Rects, Strings, SetOr, SetAnd = DrawTree(v, Pt)
         Pt.y = DrawPt.y

         OrMask  = OrMask  or  SetOr
         AndMask = AndMask and SetAnd

         for k,v in pairs(Rects) do
            table.insert(PlusRects, v)
         end
         for k,v in pairs(Strings) do
            table.insert(StringRects, v)
         end
      end
   else
      SetOr, SetAnd = ChildrenSelected(Tree)

      OrMask  = OrMask  or  SetOr
      AndMask = AndMask and SetAnd
   end


   -- Handle selection states...
   local StringW = GetSimpleTextDimension(FontHandle, Tree.Name)
   local StringRect = MakeRect(StartPt.x + PlusDim, StartPt.y, StringW, FontH)

   if (Tree.Open) then
      if (Tree.Selected) then
         DrawRect(StringRect, MakeColor(0, 0, 1))
      end
   else
      if (Tree.Selected and AndMask) then
         DrawRect(StringRect, MakeColor(0, 0, 1))
      elseif (OrMask) then
         DrawRect(StringRect, MakeColor(0, 0, 0.5))
      end
   end

   table.insert(
      StringRects,
      { Ref  = Tree,
        Rect = StringRect }
   )

   RenderText(FontHandle, Tree.Name, 0,
              MakePoint(StartPt.x + PlusDim, StartPt.y + FontBL), MakeColor(1, 1, 1))

   return Pt, PlusRects, StringRects

end

local function MakeClick(Item)
   local function ClickCoroutine(Event)
      Event = WaitForEvent(Event, LButtonDown, false)

      local IsPlus   = false
      local BoneRef  = nil
      local BoneRect = nil
      for i,v in ipairs(Item.Pluses) do
         if (RectContainsPoint(v.Rect, Event.Point)) then
            IsPlus   = true
            BoneRef  = v.Ref
            BoneRect = v.Rect
            break
         end
      end
      for i,v in ipairs(Item.Strings) do
         if (RectContainsPoint(v.Rect, Event.Point)) then
            IsPlus   = false
            BoneRef  = v.Ref
            BoneRect = v.Rect
            break
         end
      end

      if (BoneRef ~= nil) then
         Event = WaitForEvent(Event, LButtonUp, true)
         if (RectContainsPoint(BoneRect, Event.Point)) then
            if (IsPlus) then
               BoneRef.Open = not BoneRef.Open
            else
               if (BoneRef.Open) then
                  BoneRef.Selected = not BoneRef.Selected
               else
                  local COR, CAND = ChildrenSelected(BoneRef)
                  if (BoneRef.Selected and CAND) then
                     SetWithChildren(BoneRef, false)
                  elseif ((not BoneRef.Selected) and (not COR)) then
                     SetWithChildren(BoneRef, true)
                  else
                     SetWithChildren(BoneRef, false)
                  end
               end
            end
         end
      end

      Item.ClickAction = nil
      Item.Changed = true
   end

   return { Action = coroutine.create(ClickCoroutine),
            Abort  = function ()
                        Item.ClickAction = nil
                     end }
end

local function FillInMask(Tree, Mask)
   Mask[Tree.Idx] = Tree.Selected
   for k,v in pairs(Tree.Children) do
      FillInMask(v, Mask)
   end
end



local function Do(ID, NodeID)
   local Item, IsNew = IDItem.Get(ID)

   if (IsNew or StillValid(Item.MaskTree, NodeID) == false) then
      Item.MaskTree = FillInTree(NodeID)
      Item.Offset = MakePoint(0, 0)
   end

   Pt, Item.Pluses, Item.Strings = DrawTree(Item.MaskTree, MakePoint(0, 0))
   if (not Interactions.Active()) then
      Item.ClickAction = Interactions.RegisterDefClipped(Item.ClickAction or MakeClick(Item))
   end

   if (Item.Changed) then
      Item.Changed = false
      local Mask = { }
      FillInMask(Item.MaskTree, Mask)
      return Mask, Pt
   else
      return false, Pt
   end
end

-- Expose for working with these from LUA
local function RecursiveSet(Tree, Bone, State)
   if (Tree.Name == Bone) then
      SetWithChildren(Tree, State)
   else
      for k,v in pairs(Tree.Children) do
         RecursiveSet(v, Bone, State)
      end
   end
end

local function NonRecursiveSet(Tree, Bone, State)
   if (Tree.Name == Bone) then
      Tree.Selected = State
   else
      for k,v in pairs(Tree.Children) do
         NonRecursiveSet(v, Bone, State)
      end
   end
end


local function SetSouthOfBone(NodeID, Bone, State)
   local MaskTree = FillInTree(NodeID)

   RecursiveSet(MaskTree, Bone, State)

   local Mask = { }
   FillInMask(MaskTree, Mask)

   MaskSource_Set(NodeID, Mask)
end

local function SetMaskBone(NodeID, Bone, State)
   local MaskTree = FillInTree(NodeID)

   NonRecursiveSet(MaskTree, Bone, State)

   local Mask = { }
   FillInMask(MaskTree, Mask)

   MaskSource_Set(NodeID, Mask)
end

MaskEdit = {
   Do = Do,

   SetSouthOfBone = SetSouthOfBone,
   SetMaskBone = SetMaskBone,
}
