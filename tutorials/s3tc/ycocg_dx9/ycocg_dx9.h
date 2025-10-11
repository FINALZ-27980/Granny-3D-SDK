#if !defined(S3TC_DX9_H)
// ========================================================================
// $File: //jeffr/granny_29/tutorial/s3tc/ycocg_dx9/ycocg_dx9.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "granny.h"
#include <d3d9.h>
#include <d3dx9.h>
#include <vector>

// ---------------------------------------------------------
// Global D3D Objects
extern IDirect3D9* g_pD3D;
extern IDirect3DDevice9* g_pD3DDevice;
extern IDirect3DTexture9* g_pTexture;
extern IDirect3DPixelShader9* g_pPixelShader;
extern D3DXHANDLE g_hDiffuseTexture;

// ---------------------------------------------------------
// Global Win32 objects
extern const char* MainWindowTitle;
extern HWND g_hwnd;
extern bool GlobalRunning;

// ---------------------------------------------------------
// Handy functions.
bool InitializeD3D();
void CleanupD3D();
void Render();

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message,
                                    WPARAM WParam, LPARAM LParam);


#define S3TC_DX9_H
#endif
