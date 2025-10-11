// *****************************************************************************
// NB: This has NO idempotency guards, since we use it to clobber inappropriate
// definitions from external files.
// *****************************************************************************

// ========================================================================
// $File: //jeffr/granny_29/rt/granny_clear_exp_interface.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================


// Clear out anything inappropriate
#undef EXPAPI
#undef EXPTYPE
#undef EXPTYPE_EPHEMERAL
#undef EXPMACRO
#undef EXPCONST
#undef EXPGROUP
#undef GS_SAFE
#undef GS_READ
#undef GS_PARAM
#undef GS_MODIFY
#undef GS_SPECIAL
