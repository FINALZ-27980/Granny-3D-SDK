// granny_callbacks.h : Some functions that adapt Granny to the 360.
//                      See .cpp file for detailed notes about these functions
//

#include "granny.h"

// Error callback
GRANNY_CALLBACK(void) GrannyError(granny_log_message_type   Type,
                                  granny_log_message_origin Origin,
                                  char const*  File,
                                  granny_int32x Line,
                                  char const *Error, void *UserData);

GRANNY_CALLBACK(void) IAmGrannysCloseFileReader(char const *SourceFileName,
                                                granny_int32x SourceLineNumber,
                                                granny_file_reader*
                                                ReaderInit);
GRANNY_CALLBACK(granny_int32x) IAmGrannysReadAtMost(char const*         SourceFileName,
                                                    granny_int32x       SourceLineNumber,
                                                    granny_file_reader* ReaderInit,
                                                    granny_int32x       FilePosition,
                                                    granny_int32x       UInt8Count,
                                                    void*               Buffer);
GRANNY_CALLBACK(granny_file_reader *) IAmGrannysOpenFileReader(char const *SourceFileName,
                                                               granny_int32x SourceLineNumber,
                                                               char const *FileNameToOpen);
GRANNY_CALLBACK(void *) IAmGrannysAllocate(char const *File,
                                           granny_int32x Line,
                                           granny_uintaddrx Alignment,
                                           granny_uintaddrx Size,
                                           granny_int32x    Intent);
GRANNY_CALLBACK(void) IAmGrannysDeallocate(char const *File,
                                           granny_int32x Line,
                                           void *Memory);



