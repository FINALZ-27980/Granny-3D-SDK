// ========================================================================
// $File: //jeffr/granny_29/rt/ansi/ansi_granny_matrix_operations.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_matrix_operations.h"

#include "granny_assert.h"
#include "granny_memory.h"
#include "granny_parameter_checking.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode Undefined_LogMessage
USING_GRANNY_NAMESPACE;


BEGIN_GRANNY_NAMESPACE;

OPTIMIZED_DISPATCH(ColumnMatrixMultiply4x3Impl)          = ColumnMatrixMultiply4x3Impl_Generic;
OPTIMIZED_DISPATCH(ColumnMatrixMultiply4x3TransposeImpl) = ColumnMatrixMultiply4x3TransposeImpl_Generic;
OPTIMIZED_DISPATCH(ColumnMatrixMultiply4x4Impl)          = ColumnMatrixMultiply4x4Impl_Generic;

END_GRANNY_NAMESPACE;
