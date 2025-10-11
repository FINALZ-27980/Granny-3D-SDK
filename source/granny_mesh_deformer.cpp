// ========================================================================
// $File: //jeffr/granny_29/rt/granny_mesh_deformer.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_mesh_deformer.h"

#include "granny_memory.h"

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

mesh_deformer *GRANNY
NewMeshDeformer(data_type_definition const *InputVertexLayout,
                data_type_definition const *OutputVertexLayout,
                deformation_type DeformationType,
                deformer_tail_flags TailFlag)
{
    mesh_deformer *Deformer = 0;

    bone_deformer BoneDeformer;
    bone_deformer_parameters Parameters;
    if(FindBoneDeformerFor(InputVertexLayout, OutputVertexLayout,
                           DeformationType, TailFlag == AllowUncopiedTail,
                           BoneDeformer, Parameters))
    {
        Deformer = Allocate(mesh_deformer, AllocationInstance);
        if(Deformer)
        {
            Deformer->BoneDeformer = BoneDeformer;
            Deformer->Parameters = Parameters;
        }
    }

    return(Deformer);
}

void GRANNY
FreeMeshDeformer(mesh_deformer *Deformer)
{
    Deallocate(Deformer);
}

bool GRANNY
IsDeformerAccelerated(mesh_deformer const& Deformer)
{
    return Deformer.BoneDeformer.IsAccelerated != 0;
}

void GRANNY
DeformVertices(mesh_deformer const &Deformer,
               int32x const *TransformIndices,
               real32 const *TransformBuffer,
               int32x VertexCount,
               void const *SourceVertices,
               void *DestVertices)
{
    // NOTE: We punt early on 0 vertices, because some of the optimized
    // deformers do pre-reading which will fault if we don't.
    if(VertexCount)
    {
        if(TransformIndices)
        {
            Deformer.BoneDeformer.IndirectedBoneDeformer(
                VertexCount, SourceVertices, DestVertices,
                TransformIndices, (matrix_4x4 const *)TransformBuffer,
                Deformer.Parameters.TailCopy32Count,
                Deformer.Parameters.SourceVertexSize,
                Deformer.Parameters.DestVertexSize);
        }
        else
        {
            Deformer.BoneDeformer.DirectBoneDeformer(
                VertexCount, SourceVertices,
                DestVertices, (matrix_4x4 const *)TransformBuffer,
                Deformer.Parameters.TailCopy32Count,
                Deformer.Parameters.SourceVertexSize,
                Deformer.Parameters.DestVertexSize);
        }

        if (Deformer.BoneDeformer.PostDeformTransform)
        {
            Deformer.BoneDeformer.PostDeformTransform(VertexCount,
                                                      DestVertices,
                                                      Deformer.Parameters.DestVertexSize);
        }
    }
}
