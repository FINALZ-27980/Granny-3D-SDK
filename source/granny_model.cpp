// ========================================================================
// $File: //jeffr/granny_29/rt/granny_model.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_model.h"
#include "granny_mesh.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

data_type_definition ModelMeshBindingType[] =
{
    {ReferenceMember, "Mesh", MeshType},
    {EndMember},
};

data_type_definition ModelType[] =
{
    {StringMember, "Name"},
    {ReferenceMember, "Skeleton", SkeletonType},

    {TransformMember, "InitialPlacement"},

    {ReferenceToArrayMember, "MeshBindings", ModelMeshBindingType},
    {VariantReferenceMember, "ExtendedData"},

    {EndMember},
};

END_GRANNY_NAMESPACE;

void GRANNY
GetModelInitialPlacement4x4(model const &Model,
                            real32 *Placement4x4)
{
    BuildCompositeTransform4x4(Model.InitialPlacement, Placement4x4);
}
