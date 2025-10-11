// ========================================================================
// $File: //jeffr/granny_29/tutorial/platform/ps3_spu_speedcheck/granny_callbacks.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include <stdlib.h>
#include <stdio.h>
#include "granny_callbacks.h"

// Generic printf wrapper
static void
ErrorMessage(char const * const FormatString, ...)
{
    va_list ArgList;
    va_start(ArgList, FormatString);
    vprintf(FormatString, ArgList);
    va_end(ArgList);
}


// GrannyError() gets called whenever Granny encounters a problem or
// wants to warn us about a potential problem.  The Error string
// is an explanation of the problem, but the Type and Origin
// identifiers also allow a programmatic analysis of what
// happened.
GRANNY_CALLBACK(void) GrannyError(granny_log_message_type Type,
                                  granny_log_message_origin Origin,
                                  char const* File, granny_int32x Line,
                                  char const *Error, void *UserData)
{
    ErrorMessage("GRANNY: \"%s\"\n", Error);
}


struct i_am_grannys_file_reader
{
    granny_file_reader Base;
    FILE* MYFileHandle;
};

#define FILEHANDLE_VALID(h) (h != 0)

// Granny_Code
GRANNY_CALLBACK(void) IAmGrannysCloseFileReader(granny_file_reader* ReaderInit)
{
    i_am_grannys_file_reader* Reader = (i_am_grannys_file_reader *)ReaderInit;

    if(Reader)
    {
        if ( FILEHANDLE_VALID(Reader->MYFileHandle) )
        {
            fclose(Reader->MYFileHandle);
        }
        free(Reader);
    }
}

// Granny_Code
GRANNY_CALLBACK(granny_int32x) IAmGrannysReadAtMost(granny_file_reader* ReaderInit,
                                                    granny_int32x       FilePosition,
                                                    granny_int32x       UInt8Count,
                                                    void*               Buffer)
{
    i_am_grannys_file_reader *Reader = (i_am_grannys_file_reader *)ReaderInit;

    if ( !FILEHANDLE_VALID(Reader->MYFileHandle) )
    {
        return 0;
    }

    fseek(Reader->MYFileHandle, FilePosition, SEEK_SET);
    return fread(Buffer, 1, UInt8Count, Reader->MYFileHandle);
}

// Granny_Code
GRANNY_CALLBACK(bool) IAmGrannysGetReaderSize(granny_file_reader* ReaderInit,
                                              granny_int32x*      ResultSize)
{
    i_am_grannys_file_reader *Reader = (i_am_grannys_file_reader *)ReaderInit;

    if ( !FILEHANDLE_VALID(Reader->MYFileHandle) )
    {
        return 0;
    }

    size_t cur = ftell(Reader->MYFileHandle);
    fseek(Reader->MYFileHandle, 0, SEEK_END);
    *ResultSize = (granny_int32x)ftell(Reader->MYFileHandle);
    fseek(Reader->MYFileHandle, cur, SEEK_SET);

    return true;
}

// Granny_Code
GRANNY_CALLBACK(granny_file_reader *) IAmGrannysOpenFileReader(char const *SourceFileName,
                                                               granny_int32x SourceLineNumber,
                                                               char const *FileNameToOpen)
{
    i_am_grannys_file_reader *Reader = 0;

    FILE* MYFileHandle = fopen(FileNameToOpen, "rb");
    if(FILEHANDLE_VALID(MYFileHandle))
    {
        Reader = (i_am_grannys_file_reader *)malloc(sizeof(i_am_grannys_file_reader));
        if(Reader)
        {
            GrannyInitializeFileReader(IAmGrannysCloseFileReader,
                                       IAmGrannysReadAtMost,
                                       IAmGrannysGetReaderSize,
                                       &Reader->Base);
            Reader->MYFileHandle = MYFileHandle;
        }
        else
        {
            fclose(MYFileHandle);
        }
    }
    else
    {
        printf("fopen failed\n");
    }

    return((granny_file_reader *)Reader);
}


// Granny_Code
GRANNY_CALLBACK(void *) IAmGrannysAllocate(char const *File,
                                           granny_int32x Line,
                                           granny_uintaddrx Alignment,
                                           granny_uintaddrx Size,
                                           granny_int32x    Intent)
{
    void* ptr;
    if (Alignment <= 16)
        ptr = malloc(Size);
    else
        ptr = memalign(Alignment, Size);

    if ( ptr == NULL )
        printf ( "***Failed alloc of %i bytes from %s:%i, alignment %i\n", Size, File, Line, Alignment );

    return ptr;
}

// Granny_Code
GRANNY_CALLBACK(void) IAmGrannysDeallocate(char const *File,
                                           granny_int32x Line,
                                           void *Memory)
{
    free(Memory);
}


//-------------------------------------------------------------------------------------
// Name: InitGranny()
// Desc: Install any callback and subsystem overrides in the Granny API
//-------------------------------------------------------------------------------------
bool InitGrannyCallbacks()
{
    // Since this is a sample, I want Granny to call us back any time
    // there's an error or warning, so I can pop up a dialog box and
    // display it.
    granny_log_callback Callback;
    Callback.Function = GrannyError;
    Callback.UserData = 0;
    GrannySetLogCallback(&Callback);

    // // And set up Granny's memory manager.
    // GrannySetAllocator(IAmGrannysAllocate, IAmGrannysDeallocate);

    return true;
}


