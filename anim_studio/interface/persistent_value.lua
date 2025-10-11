local Values = { }

local function GetInt(name, default)
   if (Values[name] == nil) then
      Values[name] = App_GetPersitentInt(name, default)
   end

   return Values[name]
end

local function SetInt(name, val)
   App_SetPersitentInt(name, val)
   Values[name] = val
end

Persistent = {
   GetInt = GetInt,
   SetInt = SetInt
}
