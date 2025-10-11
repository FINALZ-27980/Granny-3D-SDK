// ========================================================================
// $File: //jeffr/granny_29/statement/ui_bitmap.cpp $
// $DateTime: 2012/03/14 17:48:04 $
// $Change: 36772 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "ui_bitmap.h"
#include "statement.h"

#include "luautils.h"

#include <map>

#include <windows.h>
#include <gl/gl.h>

#define STBI_HEADER_FILE_ONLY
#include "stb_image.c"

#include "granny_file_reader.h"

// Should always be the last header included
#include "granny_cpp_settings.h"

#define SubsystemCode ApplicationLogMessage
USING_GRANNY_NAMESPACE;
using namespace std;

#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1
#include "grid_list.h"
#include "images_list.h"
#else
  #include "grid_disk.h"
  #include "images_disk.h"

  struct FileEntry
  {
      char const* VarName;
      char*       VarPointer;
      int         Length;
  };
  typedef FileEntry ImageFileEntry;
  typedef FileEntry GridFileEntry;

  // Oh, boy howdy this sucks.  All of the images are looked up *sans* file extension by
  // UIGetBitmapHandle, but the on-disk file list has the extensions, of course.  We need
  // storage for the altered strings from ImageFileList, since we can't touch those
  // strings.
  vector<char*> FileEntryVarNames;

  static char*
  VarNameWithoutPoint(char const* VarName)
  {
      char* Modified;
      GStateCloneString(Modified, VarName);
      FileEntryVarNames.push_back(Modified);
      // Shear off the last '.'
      {
          char* Point = (char*)Modified + strlen(Modified);
          while (Point >= Modified)
          {
              if (*Point == '.')
              {
                  *Point = 0;
                  break;
              }
              --Point;
          }
      }

      return Modified;
  }

  static bool
  LoadFileFromDisk(char const* Filename,
                   FileEntry*  Entry,
                   bool NullTerminate)
  {
      file_reader* Reader = OpenFileReader(Filename);
      bool Success = false;
      if (Reader)
      {
          int32x NumBytes;
          if (GetReaderSize(Reader, &NumBytes))
          {
              char const* VarName = Filename + strlen(Filename);
              while (VarName > Filename)
              {
                  if (VarName[-1] == '/' || VarName[-1] == '\\')
                      break;
                  --VarName;
              }
  
              Entry->VarName = VarNameWithoutPoint(VarName);
              Entry->Length = NumBytes;
              if (NullTerminate)
                  ++Entry->Length;
  
              Entry->VarPointer = GStateAllocArray(char, Entry->Length);
              if (Entry->VarPointer && ReadExactly(Reader, 0, NumBytes, Entry->VarPointer))
              {
                  if (NullTerminate)
                      Entry->VarPointer[NumBytes] = 0;
  
                  Success = true;
              }
          }
  
          CloseFileReader(Reader);
      }
  
      return Success;
  }
  
  static void
  ReleaseDiskFile(FileEntry& Entry)
  {
      GStateDeallocate(Entry.VarPointer);
  }

#endif


struct SCmp : public binary_function<string, string, bool>
{
    bool operator()(char const* One, char const* Two) const
    {
        return strcmp(One, Two) < 0;
    }
};

// Todo: very simple, robustimacate
struct TextureEntry
{
    int    Width;
    int    Height;
    int    OrigWidth;
    int    OrigHeight;
    real32 U;
    real32 V;
    GLuint TexName;
};

struct NineGridEntry
{
    int  Widths[3];
    int  Heights[3];

    int32x        BitmapWidth;
    int32x        BitmapHeight;

    bool   Initialized;

    GLuint  TexName;
    bool    Present[3][3];
    GLfloat UMin[3][3];
    GLfloat UMax[3][3];
    GLfloat VMin[3][3];
    GLfloat VMax[3][3];
};

typedef map<char const*, int32x, SCmp> texture_map;
static texture_map          TextureMap;
static vector<TextureEntry> TextureEntries;

static texture_map           NineGridMap;
static vector<NineGridEntry> NineGridEntries;


static void
RenderGLHandleStretchedModulate(GLuint          Name,
                                LuaRect const&  Rect,
                                float UMin, float UMax,
                                float VMin, float VMax,
                                LuaColor const& Color)
{
    if (Name == 0xFFFFFFFF)
        return;

    glBindTexture(GL_TEXTURE_2D, Name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4fv(&Color.r);
    glBegin(GL_POLYGON);
    {
        glTexCoord2f(UMin, VMin); glVertex2i(Rect.x,          Rect.y);
        glTexCoord2f(UMax, VMin); glVertex2i(Rect.x + Rect.w, Rect.y);
        glTexCoord2f(UMax, VMax); glVertex2i(Rect.x + Rect.w, Rect.y + Rect.h);
        glTexCoord2f(UMin, VMax); glVertex2i(Rect.x,          Rect.y + Rect.h);
    }
    glEnd();
}


static void
EnsureTextureMapInit()
{
#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1
    if (!TextureMap.empty() || ArrayLength(ImageFileEntries) == 0)
        return;
    StTMZone(__FUNCTION__);

    TextureEntries.resize(ArrayLength(ImageFileEntries));
    {for(int32x Idx = 0; Idx < ArrayLength(ImageFileEntries); ++Idx)
    {
        TextureMap[ImageFileEntries[Idx].VarName] = Idx;
        TextureEntries[Idx].Width      = 0;
        TextureEntries[Idx].Height     = 0;
        TextureEntries[Idx].OrigWidth  = 0;
        TextureEntries[Idx].OrigHeight = 0;
        TextureEntries[Idx].U          = 1;
        TextureEntries[Idx].V          = 1;
        TextureEntries[Idx].TexName    = 0xFFFFFFFF;
    }}
#else
    if (!TextureMap.empty() || ArrayLength(ImageFileList) == 0)
        return;
    StTMZone(__FUNCTION__);

    // Initialize from the disk list...
    TextureEntries.resize(ArrayLength(ImageFileList));
    {for(int32x Idx = 0; Idx < ArrayLength(ImageFileList); ++Idx)
    {
        char const* VarName = ImageFileList[Idx] + strlen(ImageFileList[Idx]);
        while (VarName > ImageFileList[Idx])
        {
            if (VarName[-1] == '/' || VarName[-1] == '\\')
                break;
            --VarName;
        }
        VarName = VarNameWithoutPoint(VarName);
        TextureMap[VarName] = Idx;
        TextureEntries[Idx].Width      = 0;
        TextureEntries[Idx].Height     = 0;
        TextureEntries[Idx].OrigWidth  = 0;
        TextureEntries[Idx].OrigHeight = 0;
        TextureEntries[Idx].U          = 1;
        TextureEntries[Idx].V          = 1;
        TextureEntries[Idx].TexName    = 0xFFFFFFFF;
    }}
#endif
}


int NextPow2(int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

static void
Ensure9GridMapInit()
{
    EnsureTextureMapInit();

#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1
    int const NumGrids = ArrayLength(GridFileEntries);
#else
    int const NumGrids = ArrayLength(GridFileList);
#endif
    if (!NineGridMap.empty() || NumGrids == 0)
        return;


#if !(defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1)
    GridFileEntry* GridFileEntries = GStateAllocArray(GridFileEntry, NumGrids);
    for (int Idx = 0; Idx < NumGrids; ++Idx)
    {
        // todo: handle error
        if (LoadFileFromDisk(GridFileList[Idx], &GridFileEntries[Idx], true) == false)
            return;
    }
#endif

    StTMZone(__FUNCTION__);

    // Gridfile entries contains both the PNG and the description file


    {for(int32x Idx = 0; Idx < NumGrids; ++Idx)
    {
        char BitmapName[256];
        NineGridEntry NewEntry = { 0 };
        NewEntry.Initialized = false;

        sscanf(GridFileEntries[Idx].VarPointer,
               "%d %d\n"
               "%d %d\n"
               "%s",
               &NewEntry.Widths[0], &NewEntry.Widths[2],
               &NewEntry.Heights[0], &NewEntry.Heights[2],
               BitmapName);

        int32x BitmapHandle = UIGetBitmapHandle(BitmapName);
        if (BitmapHandle < 0)
            continue;

        int x = 0, y = 0, comp = 0;
#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1
        stbi_uc* Image = stbi_load_from_memory((const stbi_uc*)ImageFileEntries[BitmapHandle].VarPointer,
                                                       ImageFileEntries[BitmapHandle].Length,
                                                       &x, &y, &comp, 4);
#else
        stbi_uc* Image = stbi_load(ImageFileList[BitmapHandle], &x, &y, &comp, 4);
#endif

        if (Image)
        {
            NewEntry.BitmapWidth  = NextPow2(x);
            NewEntry.BitmapHeight = NextPow2(y);

            // We can now compute the middle width/height
            NewEntry.Widths[1]  = x - (NewEntry.Widths[0] + NewEntry.Widths[2]);
            NewEntry.Heights[1] = y - (NewEntry.Heights[0] + NewEntry.Heights[2]);
            if (NewEntry.Widths[1] < 0 || NewEntry.Heights < 0)
            {
                stbi_image_free(Image);
                continue;
            }

            glGenTextures(1, &NewEntry.TexName);
            glBindTexture(GL_TEXTURE_2D, NewEntry.TexName);
            {
                // Create the subsection, since there's no stride option to glTexImage2D
                vector<char> SubData(NewEntry.BitmapWidth * NewEntry.BitmapHeight * 4, 0);

                for (int cy = 0; cy < y; ++cy)
                {
                    memcpy(&SubData[cy * NewEntry.BitmapWidth * 4],
                           &Image[cy * x * 4],
                           4 * x);
                }

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                             NewEntry.BitmapWidth, NewEntry.BitmapHeight,
                             0, GL_RGBA, GL_UNSIGNED_BYTE, &SubData[0]);
            }

            // Ok, let's chop up that original bitmap into the 9 pieces that we need.
            int OffsetY = 0;
            for (int i = 0; i < 3; i++) // height
            {
                int OffsetX = 0;
                for (int j = 0; j < 3; j++) // width
                {
                    if (NewEntry.Heights[i] == 0 || NewEntry.Widths[j] == 0)
                    {
                        NewEntry.Present[i][j] = false;
                        continue;
                    }
                    else
                    {
                        NewEntry.Present[i][j] = true;
                    }

                    // Create the subsection, since there's no stride option to glTexImage2D
                    int DimX = NewEntry.Widths[j];
                    int DimY = NewEntry.Heights[i];

                    // @@todo: Compute the UMin/UMax
                    NewEntry.UMin[i][j] = OffsetX / float(NewEntry.BitmapWidth);
                    NewEntry.UMax[i][j] = (OffsetX + DimX) / float(NewEntry.BitmapWidth);

                    // @@todo: Compute the VMin/VMax
                    NewEntry.VMin[i][j] = OffsetY / float(NewEntry.BitmapHeight);
                    NewEntry.VMax[i][j] = (OffsetY + DimY) / float(NewEntry.BitmapHeight);

                    OffsetX += DimX;
                }

                OffsetY += NewEntry.Heights[i];
            }

            stbi_image_free(Image);
        }
        else
        {
            continue;
        }

        NineGridMap[GridFileEntries[Idx].VarName] = int(NineGridEntries.size());
        NineGridEntries.push_back(NewEntry);
    }}

#if !(defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1)
    for (int Idx = 0; Idx < NumGrids; ++Idx)
    {
        ReleaseDiskFile(GridFileEntries[Idx]);
    }
    GStateDeallocate(GridFileEntries);
#endif
}


int32x GRANNY
UIGetBitmapHandle(char const* Filename)
{
    EnsureTextureMapInit();

    texture_map::iterator Itr = TextureMap.find(Filename);
    if (Itr == TextureMap.end())
        return -1;

    int32x  Handle      = Itr->second;
    GLuint& TextureName = TextureEntries[Handle].TexName;
#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1
    Assert(_stricmp(Filename, ImageFileEntries[Handle].VarName) == 0);
#endif

    if (TextureName == 0xFFFFFFFF)
    {
        int x = 0, y = 0, comp = 0;
#if defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1
        stbi_uc* Image = stbi_load_from_memory((const stbi_uc*)ImageFileEntries[Handle].VarPointer,
                                               ImageFileEntries[Handle].Length,
                                               &x, &y, &comp, 4);
#else
        stbi_uc* Image = stbi_load(ImageFileList[Handle], &x, &y, &comp, 4);
#endif
        if (Image)
        {
            TextureEntries[Handle].OrigWidth  = x;
            TextureEntries[Handle].OrigHeight = y;
            TextureEntries[Handle].Width      = NextPow2(x);
            TextureEntries[Handle].Height     = NextPow2(y);

            TextureEntries[Handle].U = x / real32(TextureEntries[Handle].Width);
            TextureEntries[Handle].V = y / real32(TextureEntries[Handle].Height);

            glGenTextures(1, &TextureName);
            glBindTexture(GL_TEXTURE_2D, TextureName);

            vector<char> SubData(4 * (TextureEntries[Handle].Width * TextureEntries[Handle].Height), 0);
            for (int cy = 0; cy < y; ++cy)
            {
                memcpy(&SubData[cy * TextureEntries[Handle].Width * 4],
                       &Image[cy * x * 4],
                       4 * x);
            }

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         TextureEntries[Handle].Width,
                         TextureEntries[Handle].Height,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, &SubData[0]);

            stbi_image_free(Image);
        }
        else
        {
            // No point in trying this again, mark it so the handle gets ignored in the
            // outer if()
            TextureName = 0;
        }
    }

    return Handle;
}

int32x GRANNY
UIGet9GridHandle(char const* GridKey)
{
    Ensure9GridMapInit();

    texture_map::iterator Itr = NineGridMap.find(GridKey);
    if (Itr == NineGridMap.end())
        return -1;

    return Itr->second;
}

bool GRANNY
UIRender9GridModulated(int32x GridHandle, LuaRect const& RenderRect, LuaColor const& Color)
{
    StTMZone("NineGridRender");

    if (GridHandle < 0 || GridHandle >= int(NineGridEntries.size()))
        return false;

    NineGridEntry const& Entry = NineGridEntries[GridHandle];

    int32x const CenterWidth  = RenderRect.w - (Entry.Widths[0] + Entry.Widths[2]);
    int32x const CenterHeight = RenderRect.h - (Entry.Heights[0] + Entry.Heights[2]);

    int32x const UseWidths[3]  = { Entry.Widths[0],  CenterWidth,  Entry.Widths[2]  };
    int32x const UseHeights[3] = { Entry.Heights[0], CenterHeight, Entry.Heights[2] };

    int32x const OffsetX[3]    = { 0, Entry.Widths[0],  RenderRect.w - Entry.Widths[2] };
    int32x const OffsetY[3]    = { 0, Entry.Heights[0], RenderRect.h - Entry.Heights[2] };

    glBindTexture(GL_TEXTURE_2D, Entry.TexName);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4fv(&Color.r);

    glBegin(GL_TRIANGLES);
    {for (int y = 0; y < 3; ++y)
    {
        {for (int x = 0; x < 3; ++x)
        {
            if (Entry.Present[y][x] == false)
                continue;

            GLfloat UMin = Entry.UMin[y][x];
            GLfloat UMax = Entry.UMax[y][x];
            GLfloat VMin = Entry.VMin[y][x];
            GLfloat VMax = Entry.VMax[y][x];
            {
                /* 0 */ glTexCoord2f(UMin, VMin); glVertex2i(RenderRect.x + OffsetX[x],                RenderRect.y + OffsetY[y]);
                /* 1 */ glTexCoord2f(UMax, VMin); glVertex2i(RenderRect.x + OffsetX[x] + UseWidths[x], RenderRect.y + OffsetY[y]);
                /* 2 */ glTexCoord2f(UMax, VMax); glVertex2i(RenderRect.x + OffsetX[x] + UseWidths[x], RenderRect.y + OffsetY[y] + UseHeights[y]);
                /* 0 */ glTexCoord2f(UMin, VMin); glVertex2i(RenderRect.x + OffsetX[x],                RenderRect.y + OffsetY[y]);
                /* 2 */ glTexCoord2f(UMax, VMax); glVertex2i(RenderRect.x + OffsetX[x] + UseWidths[x], RenderRect.y + OffsetY[y] + UseHeights[y]);
                /* 3 */ glTexCoord2f(UMin, VMax); glVertex2i(RenderRect.x + OffsetX[x],                RenderRect.y + OffsetY[y] + UseHeights[y]);
            }
        }}
    }}
    glEnd();

    return true;
}

bool GRANNY
UIRender9Grid(int32x GridHandle, LuaRect const& RenderRect)
{
    return UIRender9GridModulated(GridHandle, RenderRect, LuaColor(1, 1, 1, 1));
}

bool GRANNY
UIRender9GridRotatedLeft(int32x GridHandle, LuaRect const& Rect)
{
    NineGridEntry const& RotEntry = NineGridEntries[GridHandle];

    int RotWidths[3]  = { RotEntry.Heights[0], RotEntry.Heights[1], RotEntry.Heights[2] };
    int RotHeights[3] = { RotEntry.Widths[2],  RotEntry.Widths[1],  RotEntry.Widths[2] };

    int32x const CenterWidth  = Rect.w - (RotWidths[0] + RotWidths[2]);
    int32x const CenterHeight = Rect.h - (RotHeights[0] + RotHeights[2]);

    int32x const UseWidths[3]  = { RotWidths[0],  CenterWidth,  RotWidths[2]  };
    int32x const UseHeights[3] = { RotHeights[0], CenterHeight, RotHeights[2] };

    int32x const OffsetX[3]    = { 0, RotWidths[0],  Rect.w - RotWidths[2] };
    int32x const OffsetY[3]    = { 0, RotHeights[0], Rect.h - RotHeights[2] };

    {
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindTexture(GL_TEXTURE_2D, RotEntry.TexName);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

        {for (int y = 0; y < 3; ++y)
        {
            {for (int x = 0; x < 3; ++x)
            {
                if (RotEntry.Present[x][2 - y] == false)
                    continue;

                GLfloat UMin = RotEntry.UMin[x][2 - y];
                GLfloat UMax = RotEntry.UMax[x][2 - y];
                GLfloat VMin = RotEntry.VMin[x][2 - y];
                GLfloat VMax = RotEntry.VMax[x][2 - y];

                LuaRect GridRect(Rect.x + OffsetX[x],
                                 Rect.y + OffsetY[y],
                                 UseWidths[x], UseHeights[y]);

                glBegin(GL_POLYGON);
                {
                    glTexCoord2f(UMax, VMin); glVertex2i(GridRect.x,              GridRect.y);
                    glTexCoord2f(UMax, VMax); glVertex2i(GridRect.x + GridRect.w, GridRect.y);
                    glTexCoord2f(UMin, VMax); glVertex2i(GridRect.x + GridRect.w, GridRect.y + GridRect.h);
                    glTexCoord2f(UMin, VMin); glVertex2i(GridRect.x,              GridRect.y + GridRect.h);
                }
                glEnd();
            }}
        }}
    }

    return true;
}


uint32x GRANNY
UIGetBitmapGLName(int32x BitmapHandle)
{
    if (BitmapHandle == -1)
        return 0;

    return TextureEntries[BitmapHandle].TexName;
}


bool GRANNY
UIGetBitmapWidthAndHeight(int32x BitmapHandle, int32x& w, int32x& h)
{
    if (BitmapHandle == -1)
        return false;

    w = TextureEntries[BitmapHandle].OrigWidth;
    h = TextureEntries[BitmapHandle].OrigHeight;
    return true;
}

bool GRANNY
UIRenderBitmapModulated(int32x BitmapHandle,
                        int32x AtX, int32x AtY,
                        LuaColor const& Color)
{
    if (BitmapHandle < 0)
        return false;
    Assert(BitmapHandle < int(TextureEntries.size()));

    TextureEntry& Entry = TextureEntries[BitmapHandle];
    RenderGLHandleStretchedModulate(Entry.TexName,
                                    LuaRect(AtX, AtY,
                                            Entry.OrigWidth, Entry.OrigHeight),
                                    0, Entry.U, 0, Entry.V,
                                    Color);

    return true;
}

bool GRANNY
UIRenderBitmap(int32x BitmapHandle,
               int32x AtX, int32x AtY)
{
    return UIRenderBitmapModulated(BitmapHandle, AtX, AtY, LuaColor(1, 1, 1, 1));
}

bool GRANNY
UIRenderBitmapFlipHoriz(int32x BitmapHandle, int AtX, int AtY)
{
    if (BitmapHandle < 0)
        return false;
    Assert(BitmapHandle < int(TextureEntries.size()));

    TextureEntry& Entry = TextureEntries[BitmapHandle];
    RenderGLHandleStretchedModulate(Entry.TexName,
                                    LuaRect(AtX + Entry.OrigWidth,
                                            AtY,
                                            -Entry.OrigWidth,
                                            Entry.OrigHeight),
                                    0, Entry.U, 0, Entry.V,
                                    LuaColor(1, 1, 1, 1));

    return true;
}

bool GRANNY
UIRenderBitmapStretchedModulate(int32x BitmapHandle,
                                LuaRect const& Rect,
                                LuaColor const& Color)
{
    if (BitmapHandle < 0)
        return false;
    Assert(BitmapHandle < int(TextureEntries.size()));

    TextureEntry& Entry = TextureEntries[BitmapHandle];

    RenderGLHandleStretchedModulate(Entry.TexName, Rect, 0, Entry.U, 0, Entry.V, Color);

    return true;
}


bool GRANNY
UIRenderBitmapStretched(int32x BitmapHandle, LuaRect const& Rect)
{
    return UIRenderBitmapStretchedModulate(BitmapHandle, Rect, LuaColor(1, 1, 1, 1));
}


// =============================================================================
// Lua gateway
static int
l_UIBitmapGetHandle(lua_State* L)
{
    char const* BitmapName = lua_tostring(L, -1);
    lua_pushinteger(L, UIGetBitmapHandle(BitmapName));
    return 1;
}

static int
l_UIBitmapDraw(lua_State* L)
{
    int BitmapHandle = (int)lua_tointeger(L, -2);
    LuaPoint atPt;
    if (!ExtractPoint(L, -1, atPt))
    {
        // todo error
        return 0;
    }

    UIRenderBitmap(BitmapHandle, atPt.x, atPt.y);
    return 0;
}

static int
l_UIBitmapDrawFaded(lua_State* L)
{
    int BitmapHandle = (int)lua_tointeger(L, -3);
    LuaPoint atPt;
    if (!ExtractPoint(L, -2, atPt))
    {
        // todo error
        return 0;
    }

    float Fade = (float)lua_tonumber(L, -1);
    if (Fade < 0) Fade = 0;
    if (Fade > 1) Fade = 1;

    UIRenderBitmapModulated(BitmapHandle, atPt.x, atPt.y, LuaColor(1, 1, 1, Fade));
    return 0;
}

static int
l_UIBitmapDrawStretched(lua_State* L)
{
    int BitmapHandle = (int)lua_tointeger(L, -2);
    LuaRect drawRect;
    if (!ExtractRect(L, -1, drawRect))
    {
        // todo error
        return 0;
    }

    UIRenderBitmapStretched(BitmapHandle, drawRect);
    return 0;
}

static int
l_UIBitmapDimensions(lua_State* L)
{
    int BitmapHandle = (int)lua_tointeger(L, -1);

    int32x w,h;
    if (UIGetBitmapWidthAndHeight(BitmapHandle, w, h))
    {
        lua_pushinteger(L, w);
        lua_pushinteger(L, h);
    }
    else
    {
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
    }

    return 2;
}

static int
l_UIBitmapFill(lua_State* L)
{
    // todo: robustimicate

    int BitmapHandle = (int)lua_tointeger(L, -2);
    if (BitmapHandle < 0)
        return 0;

    LuaRect Rect;
    if (!ExtractRect(L, -1, Rect))
        return 0;

    int32x w,h;
    if (!UIGetBitmapWidthAndHeight(BitmapHandle, w, h))
        return 0;

    if ((w & (w-1)) != 0 ||
        (h & (h-1)) != 0)
    {
        // Only works with pow2 bitmaps
        // @@ log
        return 0;
    }

    real32 u0 = Rect.w / real32(w);
    real32 v0 = Rect.h / real32(h);
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, UIGetBitmapGLName(BitmapHandle));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        glEnable(GL_TEXTURE_2D);
        glBegin(GL_POLYGON);
        {
            glColor3f(1, 1, 1);
            glTexCoord2f(0,  0);  glVertex2i(Rect.x,          Rect.y);
            glTexCoord2f(u0, 0);  glVertex2i(Rect.x + Rect.w, Rect.y);
            glTexCoord2f(u0, v0); glVertex2i(Rect.x + Rect.w, Rect.y + Rect.h);
            glTexCoord2f(0,  v0); glVertex2i(Rect.x,          Rect.y + Rect.h);
        }
        glEnd();
    }
    return 0;
}



static int
l_UINineGridGetHandle(lua_State* L)
{
    char const* GridKey = lua_tostring(L, -1);

    lua_pushinteger(L, UIGet9GridHandle(GridKey));
    return 1;
}


typedef bool GridRenderFn(int32x GridHandle, LuaRect const& RenderRect);

static int
l_UINineGridDrawCommon(lua_State* L, GridRenderFn fn)
{
    int GridHandle = (int)lua_tointeger(L, -2);
    LuaRect drawRect;
    if (!ExtractRect(L, -1, drawRect))
    {
        // todo error
        return 0;
    }

    fn(GridHandle, drawRect);
    return 0;
}

static int
l_UINineGridDraw(lua_State* L)
{
    return l_UINineGridDrawCommon(L, UIRender9Grid);
}

static int
l_UINineGridDrawLeft(lua_State* L)
{
    return l_UINineGridDrawCommon(L, UIRender9GridRotatedLeft);
}

static int
l_UINineGridDrawModulated(lua_State* L)
{
    int GridHandle = (int)lua_tointeger(L, -3);
    LuaRect drawRect;
    if (!ExtractRect(L, -2, drawRect))
    {
        // todo error
        return 0;
    }

    LuaColor drawColor;
    if (!ExtractColor(L, -1, drawColor))
    {
        // todo error
        return 0;
    }

    UIRender9GridModulated(GridHandle, drawRect, drawColor);
    return 0;
}



bool GRANNY
UIBitmap_Register(lua_State* State)
{
    lua_register(State, "UIBitmap_GetHandle",     l_UIBitmapGetHandle);
    lua_register(State, "UIBitmap_Draw",          l_UIBitmapDraw);
    lua_register(State, "UIBitmap_DrawFaded",     l_UIBitmapDrawFaded);
    lua_register(State, "UIBitmap_DrawStretched", l_UIBitmapDrawStretched);
    lua_register(State, "UIBitmap_Dimensions",    l_UIBitmapDimensions);
    lua_register(State, "UIBitmap_Fill",          l_UIBitmapFill);

    lua_register(State, "NineGrid_GetHandle", l_UINineGridGetHandle);
    lua_register(State, "NineGrid_Draw",      l_UINineGridDraw);
    lua_register(State, "NineGrid_DrawLeft",  l_UINineGridDrawLeft);

    lua_register(State, "NineGrid_DrawModulated",      l_UINineGridDrawModulated);

    return true;
}

void GRANNY
UIBitmap_Shutdown()
{
    {for (size_t Idx = 0; Idx < TextureEntries.size(); ++Idx)
    {
        if (TextureEntries[Idx].TexName != 0xFFFFFFFF && TextureEntries[Idx].TexName != 0)
            glDeleteTextures(1, &TextureEntries[Idx].TexName);
    }}
    TextureEntries.clear();
    TextureMap.clear();

#if !(defined(STATEMENT_OFFICIAL) && STATEMENT_OFFICIAL == 1)
    // Lose all of the modified strings...
    for (size_t Idx = 0; Idx < FileEntryVarNames.size(); ++Idx)
    {
        GStateDeallocate(FileEntryVarNames[Idx]);
    }
#endif

}
