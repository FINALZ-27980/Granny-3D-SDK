require "ui_base"
require "ui_editbox"
require "ui_preferences"

local FontHandle               = UIPref.GetFont("node_title_text")
local FontHeight, FontBaseline = GetFontHeightAndBaseline(FontHandle)

local IconHeight  = 48
local Padding     = 5
-- local TitleHeight = IconHeight + Padding
local TitleHeight = Editbox.RecommendedHeight(FontHandle) + Padding
local IconRect    = MakeRect(Padding, Padding, IconHeight, IconHeight)

local TextX       = Padding*2
local TextY       = TitleHeight - FontHeight - Padding + FontBaseline
local TextHeight  = TitleHeight - 2*Padding

local NodeFontHandle                   = UIPref.GetFont("node_title")
local NodeFontHeight, NodeFontBaseline = GetFontHeightAndBaseline(NodeFontHandle)
local NodeTitleHeight = Editbox.RecommendedHeight(NodeFontHandle)


local function HandleTitleBar(ID, Area, NodeID)
   local TextRect = MakeRect(TextX, TextY, Area.w - TextX, TitleHeight)

   if (NodeID) then
      local NodeName = Edit_NameForNode(NodeID)
      local EditHeight = Editbox.RecommendedHeight(FontHandle)

      -- Todo: should this be editable if the statemachine is the displayed name?
      local String, Changed, Success =
         Editbox.DoRenamable(IDItem.CreateSub(ID, NodeID .. "box"),
                             MakeRect(TextX, TitleHeight - EditHeight, Area.w - TextX, EditHeight),
                             NodeName, FontHandle)
      if (Changed and Success) then
         Edit_SetNameForNode(NodeID, String)
      end
   end

   return TextRect
end

-- Dupe for now, maybe factor out to get the icon/name set right once I'm sure this is
-- stable...
function HandleNodeName(ID, X, Y, W, NodeID, F2Active)
   Y = Y - 3
   local TextRect = MakeRect(X, Y, W, NodeTitleHeight)

   if (NodeID) then
      local NodeName = Edit_NameForNode(NodeID)

      -- Todo: should this be editable if the statemachine is the displayed name?
      local String, Changed, Success
      if (F2Active) then
         String, Changed, Success =
            Editbox.DoWithArgs(IDItem.CreateSub(ID, NodeID .. "box"),
                               TextRect, NodeName, NodeFontHandle, false,
                               { F2Rename = true,
                                 TextColor = UIPref.GetColor("node_title_text"),
                                 StartAction = RButtonDown })
      else
         String, Changed, Success =
            Editbox.DoWithArgs(IDItem.CreateSub(ID, NodeID .. "box"),
                               TextRect, NodeName, NodeFontHandle, false,
                               { TextColor = UIPref.GetColor("node_title_text"),
                                 StartAction = RButtonDown })
      end

      if (Changed and Success) then
         Edit_SetNameForNode(NodeID, String)
      end
   end

   return TextRect
end

-- Dupe for now, maybe factor out to get the icon/name set right once I'm sure this is
-- stable...
local TransitionX = Padding * 2
local function HandleTransitionTitleBar(ID, Area, TransitionID)
   local TextRect = MakeRect(TransitionX, TextY, Area.w - TransitionX, TitleHeight)

   if (TransitionID) then
      local TransitionName = Edit_NameForTokenized(TransitionID)
      local EditHeight = Editbox.RecommendedHeight(FontHandle)

      local String, Changed, Success =
         Editbox.Do(IDItem.CreateSub(ID, "box"),
                    MakeRect(TransitionX, TitleHeight - EditHeight, Area.w - TransitionX, EditHeight),
                    TransitionName, FontHandle)

      if (Changed and Success) then
         Edit_SetNameForTokenized(TransitionID, String)
      end
   end

   return TextRect
end


function StateNodeEditBox(ID, NodeID, ShowName)
   local Item, IsNew = IDItem.Get(IDItem.CreateSub(ID, NodeID))
   local Area = UIArea.Get()
   
   local ReportedSubRect = nil
   if (NodeID) then
      -- This might be a transition, not a node.  Handle that here...
      if (not Edit_IsTransition(NodeID)) then
         if (ShowName) then
            TextRect = HandleTitleBar(ID, Area, NodeID)
         else
            TextRect = MakeRect(0, 0, 0, 5)
         end

         ReportedSubRect = Edit_StateNodeEdit(NodeID, TextRect.h)
      else
         if (not IsHorizontal) then
            TextRect = HandleTransitionTitleBar(ID, Area, NodeID)
         end

         ReportedSubRect = Edit_TransitionEdit(NodeID, TextRect.h)
      end
   end

   return ReportedSubRect
end


function GraphNodeEditBox(ID, NodeID, ShowName)
   local Item,IsNew = IDItem.Get(ID)
   
   local Area = UIArea.Get()

   local TextRect = nil
   if (ShowName) then
      TextRect = HandleTitleBar(ID, Area, NodeID)
   else
      TextRect = MakeRect(0, 0, 0, 5)
   end
   
   return Edit_BlendNodeEdit(NodeID, TextRect.h)
end

