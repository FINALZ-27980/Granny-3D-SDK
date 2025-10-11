#if !defined(GRANNY_CALLBACKS_H)
// ========================================================================
// A few utility functions that adapt Granny to the 360: see .cpp for more
// info
//
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================

#include "granny.h"

// Error callback
GRANNY_CALLBACK(void) GrannyError(granny_log_message_type Type,
                                  granny_log_message_origin Origin,
                                  char const* File, granny_int32x Line,
                                  char const *Error, void *UserData);


#define GRANNY_CALLBACKS_H
#endif
