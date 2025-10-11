// ========================================================================
// $File: //jeffr/granny_29/statement/ui_preferences.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(UI_PREFERENCES_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif


BEGIN_GRANNY_NAMESPACE;

struct LuaColor;

int32x   GetPreferenceFont(const char* Fontkey);
LuaColor GetPreferenceColor(const char* Colorkey);
int32x   GetPreferenceBitmap(const char* Filename);

END_GRANNY_NAMESPACE;

#define UI_PREFERENCES_H
#endif /* UI_PREFERENCES_H */
