// ========================================================================
// $File: //jeffr/granny_29/statement/ui_bitmap.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(UI_BITMAP_H)


#ifndef GRANNY_TYPES_H
#include "granny_types.h"
#endif

struct lua_State;
BEGIN_GRANNY_NAMESPACE;

struct LuaRect;
struct LuaColor;

int32x  UIGetBitmapHandle(char const* Filename);

uint32x UIGetBitmapGLName(int32x BitmapHandle);
bool    UIGetBitmapWidthAndHeight(int32x BitmapHandle, int32x& w, int32x& h);


bool UIRenderBitmap(int32x BitmapHandle, int PosX, int PosY);
bool UIRenderBitmapModulated(int32x BitmapHandle, int PosX, int PosY, LuaColor const& Color);
bool UIRenderBitmapFlipHoriz(int32x BitmapHandle, int PosX, int PosY);
bool UIRenderBitmapStretched(int32x BitmapHandle, LuaRect const&);

bool UIRenderBitmapStretchedModulate(int32x BitmapHandle, LuaRect const& rect, LuaColor const& modulate);


int32x UIGet9GridHandle(char const* GridKey);

bool   UIRender9Grid(int32x GridHandle, LuaRect const&);
bool   UIRender9GridModulated(int32x GridHandle, LuaRect const&, LuaColor const&);
bool   UIRender9GridRotatedLeft(int32x GridHandle, LuaRect const&);



bool UIBitmap_Register(lua_State*);
void UIBitmap_Shutdown();

END_GRANNY_NAMESPACE;


#define UI_BITMAP_H
#endif /* UI_BITMAP_H */
