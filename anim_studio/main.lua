-- The first bits here are system related, please skip to the SKIPHERE section below
function require(str)
end

package.path = ExePathName .. "?.lua;" .. ExePathName .. "interface/?.lua"

-- SKIPHERE: Ok, you can start paying attention
require "ui_base"
require "ui_hotkeys"
require "ui_statement"
require "ui_preferences"
require "animation_list"
require "bread_crumbs"
require "stack"
require "point"
require "persistent_value"
require "node_create_bar"
require "interactions"
require "character_pane"
require "quick_slots"
require "ui_comments"

-- Todo: find a better place for these

TransitionDragType  = 1337
CreateNodeDragType  = 50015
CreateStateDragType = 801135
DragTypeSlot        = 0x7005



function GetPersistentRatio(Name, DefaultNum, DefaultDenom)
   local  N = App_GetPersistentInt(Name .. "_Num",   DefaultNum)
   local  D = App_GetPersistentInt(Name .. "_Denom", DefaultDenom)

   return (N/D)
end

function SetPersistentRatio(Name, Num, Denom)
   App_SetPersistentInt(Name .. "_Num",   Num)
   App_SetPersistentInt(Name .. "_Denom", Denom)
end



local function NoActionIfScrub()
   Interactions.SetIgnoreRegister(Edit_InScrubMode())
end

local function ResetNoAction()
   Interactions.SetIgnoreRegister(false)
end

function CharacterRegion(ID)
   NewSplitI = Splitter.DoHorizontal(IDItem.CreateSub(ID, "char_splith"),
                                  -100, -100, false,
                               DrawCharacterPane,
                               function(ID)
                                  Border.Do(ID, 0, 0, 1, 0, NodeCreateBar)
                               end)
   if (NewSplitI ~= SplitI) then
      SetPersistentRatio("SplitPreview", NewSplitI, UIArea.Get().h)
   end
end


local CarbonFiber = UIBitmap_GetHandle("background")
ContainerEditingOffset = { x = 0, y = 0 }
function NodeEditArea(ID)
   UIBitmap_Fill(CarbonFiber, UIArea.Get())

   Border.Do(
      ID, 0, 0, 1, 1,
      function(ID)
         ContainerEditingOffset =
            ScrollArea.DoZoomable(IDItem.CreateSub(ID, "scroll"),
                                  ContainerEditingOffset,
                                  function (ID)
                                     DragContext.MakeTargetEntry(CreateNodeDragType, UIArea.Get(), 0)
                                     DragContext.MakeTargetEntry(CreateStateDragType, UIArea.Get(), 0)
                                     DragContext.MakeTargetEntry(DragTypeSlot, UIArea.Get(), 0)
                                     return EditStateRender(IDItem.CreateSub(ID, "editrender"))
                                  end)
      end)

   QuickSlots.Do(IDItem.CreateSub(ID, "quickslots"))
end

function NodeDisplay(ID)
   Splitter.DoHorizontal(IDItem.CreateSub(ID, "breadcrumbsplit"),
                         BreadCrumbsHeight+1, -1, false,
                         function(ID)
                            Border.Do(ID, 0, 0, 1, 0, BreadCrumbs)
                         end,
                         NodeEditArea)
end

function EditingRegion(ID)
   NoActionIfScrub()

   NodeDisplay(ID)
   if (Edit_InScrubMode()) then
      DrawStippleRect(UIArea.Get(),
                      UIPref.GetColor("inaction_indicator0"),
                      UIPref.GetColor("inaction_indicator1"))
   end
   ResetNoAction()
end


function DoMainInterface()
   Interactions.EnterContext("main")

   -- Clear to the background color
   ClearScreen(UIPref.GetColor("background"))

   local MainID = IDItem.MakeID("main","window")

   DragContext.Push(IDItem.CreateSub(MainID, "DragContext"), "MainInterface")


   -- We do this dance so we can store this without drift
   local SplitCoordF = GetPersistentRatio("MainSplit", 4, 10)

   local SplitCoordI = math.floor(SplitCoordF * UIArea.Get().w)
   local NewSplitI   = Splitter.DoVertical(IDItem.CreateSub(MainID, "splitv"),
                                           SplitCoordI, 415, true, CharacterRegion, EditingRegion)
   if (NewSplitI ~= SplitCoordI) then
      SetPersistentRatio("MainSplit", NewSplitI, UIArea.Get().w)
   end

   DragContext.Pop()


   -- Flush out the deferred rendering
   DeferredRender.Flush()
   ClearCloseEdits()
end


local ModalStack  = nil
local ModalResult = nil
local ModalUID    = 1

function GetModalResult()
   local Ret = ModalResult
   ModalResult = nil

   return Ret
end

function InitModalStack()
   if (not ModalStack) then
      ModalStack = Stack.New()
   end
end

function PushModal(coro, NameForID)
   InitModalStack()

   local NewModal = { Action = coro, Name = NameForID, UID = ModalUID }
   ModalUID = ModalUID + 1

   Stack.Push(ModalStack, NewModal)
   Interactions.Clear()
end

function DoModals()
   InitModalStack()

   if (Stack.Empty(ModalStack)) then
      return
   end

   Stack.IterateFromBase(
      ModalStack,
      function (Item)
         Interactions.EnterContext("modal" .. Item.UID)

         local ItemID = IDItem.MakeID("modal" .. Item.UID, Item.Name)

         local normal, endit, retval = coroutine.resume(Item.Action, ItemID)
         if (endit or coroutine.status(Item.Action) == "dead") then
            if (Stack.IsBackItem(ModalStack, Item) == false) then
               error("Out of order modal result!")
            end

            if (ModelResult ~= nil) then
               error("should be nil here!")
            end

            ModalResult = retval

            -- Pull the back off the stack
            Stack.Pop(ModalStack)
            Interactions.DestroyContext("modal" .. Item.UID)
         end

         -- Flush out the deferred rendering
         DeferredRender.Flush()
      end)
end

local FaderArray = { }
local FaderStrings = { }

function PushFader(BitmapName, FadeTime)
   local BitmapHandle = UIPref.GetBitmap(BitmapName)
   if (not BitmapHandle) then
      return
   end

   local BitmapW, BitmapH = UIBitmap_Dimensions(BitmapHandle)

   local NewFader = { Handle = BitmapHandle,
                      WidthDiv2  = math.floor(BitmapW / 2),
                      HeightDiv2 = math.floor(BitmapH / 2),
                      StartTime = Edit_GetAbsoluteTime(),
                      EndTime   = Edit_GetAbsoluteTime() + FadeTime }
   table.insert(FaderArray, NewFader)
end

function PushFaderString(String, Color, FadeTime)
   local NewFader = { DisplayString = String,
                      Color = Color,
                      StartTime = Edit_GetAbsoluteTime(),
                      EndTime   = Edit_GetAbsoluteTime() + FadeTime }
   table.insert(FaderStrings, 1, NewFader)
end

local FaderFontHandle         = nil
local FaderFontH, FaderFontBL = 0,0

function DoFaders()
   local AbsTime = Edit_GetAbsoluteTime()
   while (#FaderArray ~= 0 and FaderArray[1].EndTime < AbsTime) do
      table.remove(FaderArray, 1)
   end

   local Removed
   repeat
      Removed = false
      for i,Item in ipairs(FaderStrings) do
         if (Item.EndTime < AbsTime) then
            table.remove(FaderStrings, i)
            Removed = true
            break
         end
      end
   until (Removed == false)

   if (#FaderArray ~= 0) then
      local Area = UIArea.Get()
      local AreaCenterX = math.floor((Area.x + Area.w) / 2)
      local AreaCenterY = math.floor((Area.y + Area.h) / 2)

      for i,Item in ipairs(FaderArray) do
         local Param = 1.0 - ((AbsTime - Item.StartTime) / (Item.EndTime - Item.StartTime))

         if (Param > 0) then
            UIBitmap_DrawFaded(Item.Handle,
                               MakePoint(AreaCenterX - Item.WidthDiv2,
                                         AreaCenterY - Item.HeightDiv2),
                               Param)
         end
      end
   end

   if (#FaderStrings ~= 0) then
      if (FaderFontHandle == nil) then
         FaderFontHandle = UIPref.GetFont("log_strings")
         FaderFontH, FaderFontBL = GetFontHeightAndBaseline(FontHandle)
      end

      local Area  = UIArea.Get()
      local CurrY = Area.h - FaderFontH + FaderFontBL

      for i,Item in ipairs(FaderStrings) do
         local Param = 1.0 - ((AbsTime - Item.StartTime) / (Item.EndTime - Item.StartTime))

         if (not Item.Width) then
            Item.Width = GetSimpleTextDimension(FaderFontHandle, Item.DisplayString)
         end

         local x = math.max(0, Area.w - Item.Width - 5)
         RenderText(FaderFontHandle, Item.DisplayString, 0,
                    MakePoint(x, CurrY),
                    MakeColorA(Item.Color[1],
                               Item.Color[2],
                               Item.Color[3],
                               Param))

         CurrY = CurrY - FaderFontH
      end
   end
end


local HotKeyTable = nil
function ApplicationKeys()
   -- Must create after all of the other files have loaded...
   if (not HotKeyTable) then
      HotKeyTable = {
         { KeyString   = "Ctrl-N",
           Description = "New machine",
           EventSpecs  = { { Type = KeyDown, Key = KeyForChar("n"), Ctrl = true } },
           Fn          = function() Edit_Reset() end },
         { KeyString   = "Ctrl-O",
           Description = "Load",
           EventSpecs  = { { Type = KeyDown, Key = KeyForChar("o"), Ctrl = true } },
           Fn          = function() Edit_PromptForLoad() end },
         { KeyString   = "Ctrl-S",
           Description = "Save",
           EventSpecs  = { { Type = KeyDown, Key = KeyForChar("s"), Ctrl = true } },
           Fn          = function() Edit_SaveToFile(false) end },
         { KeyString   = "Ctrl-Z",
           Description = "Undo",
           EventSpecs  = { { Type = KeyDown, Key = KeyForChar("z"), Ctrl = true } },
           Fn          = function() Edit_PopUndo() end },
         { KeyString   = "Ctrl-A",
           Description = "Animation Set Editor",
           EventSpecs  = { { Type = KeyDown, Key = KeyForChar("a"), Ctrl = true } },
           Fn          = AnimSetEditor.DoYielding },
         { KeyString   = "`",
           Description = "Activate Console",
           EventSpecs  = { { Type = KeyDown, Key = KeyForChar("`") } },
           Fn          = Console.Activate }
      }
   end

   local HKID = IDItem.MakeID("Application", "Hotkeys")
   Hotkeys.Register(HKID, "Application", HotKeyTable)
end
