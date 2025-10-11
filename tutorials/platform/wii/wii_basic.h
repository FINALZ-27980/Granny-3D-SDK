#if !defined(WII_BASIC_H)
// ========================================================================
// $File: //jeffr/granny_29/tutorial/platform/wii/wii_basic.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

struct model;
struct texture;
struct granny_texture;

// Prototypes to silence Codewarrior warnings.
bool InitGranny();
bool InitScene();
bool
CreateRGBATexture(granny_texture *GrannyTexture,
                  int ImageIndex,
                  texture* Dest);
void Update(granny_real32 const CurrentTime,
            granny_real32 const DeltaTime);
void RenderModel(model *Model);
void Render();


#define WII_BASIC_H
#endif
