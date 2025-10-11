// *****************************************************************************
// NB: This has NO idempotency guards, since we use it to clobber inappropriate
// definitions from external files.
// *****************************************************************************

// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_exp_interface.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================

// Clear out anything inappropriate
#undef EXPAPI
#undef EXPCALLBACK
#undef EXPTYPE
#undef EXPTYPE_EPHEMERAL
#undef EXPMACRO
#undef EXPCONST
#undef EXPGROUP

#undef EXPIN
#undef EXPOUT
#undef EXPINOUT
#undef EXPFREE

#undef EXPTAG

#undef GS_SAFE
#undef GS_READ
#undef GS_PARAM
#undef GS_MODIFY
#undef GS_SPECIAL


// Refresh the definitions
#define EXPAPI
#define EXPCALLBACK
#define EXPTYPE
#define EXPTYPE_EPHEMERAL
#define EXPMACRO
#define EXPCONST
#define EXPGROUP(GroupName)

#define EXPIN
#define EXPOUT
#define EXPINOUT
#define EXPFREE

#define EXPTAG(x)

#define GS_SAFE
#define GS_READ
#define GS_PARAM
#define GS_MODIFY
#define GS_SPECIAL
