// ========================================================================
// $File: //jeffr/granny_29/statement/ui_character_render.h $
// $DateTime: 2012/07/06 13:27:18 $
// $Change: 38171 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#if !defined(UI_CHARACTER_RENDER_H)

#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

struct lua_State;
BEGIN_GRANNY_NAMESPACE;

enum ECameraUp
{
    eUseFileSetting = 1,
    eUseYUp         = 2,
    eUseZUp         = 3
};

struct file_info;
struct camera;

bool UICharacterDrawing_Register(lua_State*);
void UICharacterDrawing_Shutdown();

void UpdateCameraForFile(file_info*);
void ZoomToBoundingBox();

void ResetCharacterXForm();

// For undo/redo
void GetCharacterXForm(real32* XForm);
void SetCharacterXForm(real32 const* XForm);
void GetSceneCamera(camera* Camera, bool* Tracks);
void SetSceneCamera(camera const& Camera, bool Tracks);

ECameraUp GetCameraUpSetting();
void      SetCameraUpSetting(ECameraUp);

real32 GetGridSpacing();


END_GRANNY_NAMESPACE;

#define UI_CHARACTER_RENDER_H
#endif /* UI_CHARACTER_RENDER_H */
