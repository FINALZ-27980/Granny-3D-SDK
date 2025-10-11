// ========================================================================
// $File: //jeffr/granny_29/statement/statement_fileops.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "statement_fileops.h"
#include "statement.h"
#include "ui_core.h"

#include <stdarg.h>
#include <stdio.h>
#include <windows.h>
#include <vector>
#include <commdlg.h>
#include <assert.h>
#include <shellapi.h>

USING_GRANNY_NAMESPACE;
using namespace std;

static char FilterBuffer[256];
static char const *StarDotStarDescription = "All Files (*.*)";
static char *
PrepareWinXXFilterBuffer(char const *Description,
                         char const *ExtensionWithoutStar)
{
    char *Current = FilterBuffer;

    while(*Description)
    {
        *Current++ = *Description++;
    }
    *Current++ = '\0';

    *Current++ = '*';
    *Current++ = '.';
    while(*ExtensionWithoutStar)
    {
        *Current++ = *ExtensionWithoutStar++;
    }
    *Current++ = '\0';

    Description = StarDotStarDescription;
    while(*Description)
    {
        *Current++ = *Description++;
    }
    *Current++ = '\0';

    *Current++ = '*';
    *Current++ = '.';
    *Current++ = '*';
    *Current++ = '\0';
    *Current++ = '\0';

    return FilterBuffer;
}

static void
PrepareWinXXOFN(OPENFILENAME &PrepareOFN, char const *Title,
                int FilenameBufferSize, char *FilenameBuffer,
                char const *TypeDescription, char const *Extension,
                HWND Owner)
{
    // The fields that we care about
    PrepareOFN.lStructSize = sizeof(OPENFILENAME);
    PrepareOFN.hInstance = GetModuleHandle(0);
    PrepareOFN.lpstrFile = FilenameBuffer;
    PrepareOFN.nMaxFile = FilenameBufferSize;
    PrepareOFN.Flags = OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST;
    PrepareOFN.lpstrDefExt = Extension;
    PrepareOFN.lpstrTitle = Title;
    PrepareOFN.hwndOwner = NULL;

    // The fields that we don't
    PrepareOFN.lpstrFilter = 0;
    PrepareOFN.lpstrCustomFilter = 0;
    PrepareOFN.nMaxCustFilter = 0;
    PrepareOFN.nFilterIndex = 0;
    PrepareOFN.lpstrFileTitle = 0;
    PrepareOFN.nMaxFileTitle = 0;
    PrepareOFN.lpstrInitialDir = 0;
    PrepareOFN.nFileOffset = 0; PrepareOFN.nFileExtension = 0;
    PrepareOFN.lCustData = 0;
    PrepareOFN.lpfnHook = 0;
    PrepareOFN.lpTemplateName = 0;

    PrepareOFN.lpstrFilter = PrepareWinXXFilterBuffer(
        TypeDescription, Extension);
}

bool GRANNY
QueryForReadableFile(int BufferSize, char *Buffer,
                     bool AllowMultiSelect,
                     char const *Title,
                     char const *FileTypeDescription,
                     char const *FileTypeExtension)
{
    StTMIdle(__FUNCTION__);
    bool FileSelected = false;

    vector<char> QueryBufferVec(1 << 15);
    char* QueryBuffer = &QueryBufferVec[0];

    // Prepare an OPENFILENAME structure for the dialog box
    OPENFILENAME WindowsAPIsSuck;
    PrepareWinXXOFN(WindowsAPIsSuck, Title,
                    AllowMultiSelect ? QueryBufferVec.size() : BufferSize,
                    AllowMultiSelect ? QueryBuffer : Buffer,
                    FileTypeDescription, FileTypeExtension,
                    UIGetHWnd());
    WindowsAPIsSuck.Flags |= OFN_FILEMUSTEXIST;
    if(AllowMultiSelect)
    {
        WindowsAPIsSuck.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    }

    // Launch the common file dialog
    // Note: Make sure the buffer starts with \0.  The function can boot if not
    WindowsAPIsSuck.lpstrFile[0] = '\0';
    if(GetOpenFileName(&WindowsAPIsSuck))
    {
        // todo check buffer size...
        if(AllowMultiSelect)
        {
            char *FileName = &QueryBuffer[WindowsAPIsSuck.nFileOffset];
            while(*FileName)
            {
                // Copy the directory
                char const *Directory = QueryBuffer;
                while(*Directory &&
                      ((Directory - QueryBuffer) != WindowsAPIsSuck.nFileOffset))
                {
                    if(*Directory == '\\')
                    {
                        *Buffer++ = '/';
                    }
                    else
                    {
                        *Buffer++ = *Directory;
                    }
                    ++Directory;
                }

                // Ensure termination
                if (WindowsAPIsSuck.nFileOffset > 0 && *(Buffer-1) != '/')
                    *Buffer++ = '/';

                // Copy the filename
                while(*FileName)
                {
                    *Buffer++ = *FileName++;
                }
                ++FileName;
                *Buffer++ = '\0';
            }

            // Double null terminate so we know when to stop with the multiselection case.
            *Buffer++ = '\0';
        }

        FileSelected = true;
    }

    return(FileSelected);
}

bool GRANNY
QueryForWritableFile(int BufferSize, char *Buffer,
                     char const *Title,
                     char const *FileTypeDescription,
                     char const *FileTypeExtension)
{
    StTMIdle(__FUNCTION__);
    Buffer[0] = 0;

    // Prepare an OPENFILENAME structure for the dialog box
    OPENFILENAME WindowsAPIsSuck;
    PrepareWinXXOFN(WindowsAPIsSuck, Title,
                    BufferSize, Buffer,
                    FileTypeDescription, FileTypeExtension,
                    UIGetHWnd());
    WindowsAPIsSuck.Flags |= (OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT);

    // Launch the common file dialog
    // Note: Make sure the buffer starts with \0.  The function can boot if not
    WindowsAPIsSuck.lpstrFile[0] = '\0';
    return GetSaveFileName(&WindowsAPIsSuck) != FALSE;
}
