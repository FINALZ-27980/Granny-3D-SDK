require "id_item_cache"
require "ui_preferences"
require "ui_itunesbox"
require "ui_recent_file_button"
require "ui_button"
require "ui_autoplacer"
require "rect"

local ErrorFontHandle = UIFont.Get("Verdana", 16, true, false)
local ErrorFontHandleSM = UIFont.Get("Verdana", 10, true, false)
local ErrorFontH, ErrorFontBL = GetFontHeightAndBaseline(ErrorFontHandle)
local ErrorFontSMH, ErrorFontSMBL = GetFontHeightAndBaseline(ErrorFontHandleSM)

local ObservedHeight = 50

local PlusLabel = "+"
local MinusLabel = "-"
local PlusW, PlusH = Button.Dim(PlusLabel)

local PlusTip = ("Creates a new controller scheme.  If you hold\n" ..
                 "'Shift' while clicking this, the new scheme will\n" ..
                 "start as a copy of the currently selected scheme")
local MinusTip = "Delete the current controller scheme. (Undoable)"


local function EditCallback(Entry, ChangedText)
   -- Only the current scheme can be changed, so we don't need entry
   XInput_SetSchemeName(XInput_GetCurrentScheme(), ChangedText)
end

local function AddScheme(Modifiers)
   local Current = XInput_GetCurrentScheme()

   if (Modifiers.Shift and Current ~= 0) then
      XInput_AddScheme(Current)
   else
      XInput_AddScheme(nil)
   end
end


local function DeleteScheme(Modifiers)
   XInput_DeleteCurrentScheme()
end

function ControllerSchemes(ID)
   local CurrY = 10

   if (XInput_ControllerConnected() == false) then
      -- Display an error for now...
      RenderDropText(ErrorFontHandle, "Controller not connected", 0,
                     MakePoint(10, CurrY + ErrorFontBL),
                     MakeColor(1, 1, 1), MakeColor(0, 0, 0))
      CurrY = CurrY + ErrorFontH

      RenderDropText(ErrorFontHandleSM,
                     "  Ensure that XInput is installed and your 360 gamepad", 0,
                     MakePoint(10, CurrY + ErrorFontSMBL),
                     MakeColor(1, 1, 1), MakeColor(0, 0, 0))
      CurrY = CurrY + ErrorFontSMH

      RenderDropText(ErrorFontHandleSM,
                     "  is connected as Controller 0.", 0,
                     MakePoint(10, CurrY + ErrorFontSMBL),
                     MakeColor(1, 1, 1), MakeColor(0, 0, 0))
      CurrY = CurrY + ErrorFontSMH
   else
      local Schemes = XInput_GetSchemeNames()
      local Current = XInput_GetCurrentScheme()

      local SchemeRect = MakeRect(10, CurrY, 250, PlusH)
      local PlusRect   = MakeRect(265, CurrY, PlusW, PlusH)
      local MinusRect  = MakeRect(PlusRect.x + PlusRect.w + 5, CurrY, PlusW, PlusH)
      
      local Changed = false
      Current, Changed =
         Combobox.DoEditable(IDItem.CreateSub(ID, "Schemes"),
                             MakeRect(10, 10, 250, Combobox.RecommendHeight()),
                             Schemes,
                             XInput_GetCurrentScheme(),
                             EditCallback)

      Button.Do(IDItem.CreateSub(ID, "Plus"),  PlusRect,  PlusLabel,  false, AddScheme)
      ToolTip.Register(PlusTip, PlusRect)

      Button.Do(IDItem.CreateSub(ID, "Minus"), MinusRect, MinusLabel, false, DeleteScheme)
      ToolTip.Register(MinusTip, MinusRect)
      
      -- Advance
      CurrY = CurrY + PlusRect.h + 5

      -- Draw the real interface
      CurrY = XInput_EditCurrentScheme(CurrY)
   end

   ObservedHeight = math.max(CurrY + 10)
end

function ControllerSchemesHeight()
   return ObservedHeight
end

