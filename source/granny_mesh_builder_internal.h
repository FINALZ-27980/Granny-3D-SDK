#if !defined(GRANNY_MESH_BUILDER_INTERNAL_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_mesh_builder_internal.h $
// $DateTime: 2012/01/31 16:51:41 $
// $Change: 36450 $
// $Revision: #2 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_MESH_BUILDER_H)
#include "granny_mesh_builder.h"
#endif

#if !defined(GRANNY_STACK_ALLOCATOR_H)
#include "granny_stack_allocator.h"
#endif

#if !defined(GRANNY_VERTEX_DATA_H)
#include "granny_vertex_data.h"
#endif

#if !defined(GRANNY_LIMITS_H)
#include "granny_limits.h"
#endif


BEGIN_GRANNY_NAMESPACE;

struct mesh_builder
{
    // Defaults to true.
    bool32 AllowTangentVertSplits;

    // Defaults to false
    bool32 PreserveEmptyMaterialGroups;

    // Tolerances
    real32 NormalTolerance;
    real32 TangentTolerance;
    real32 BinormalTolerance;
    real32 TangentBinormalCrossTolerance;
    real32 TangentMergingMinCosine;
    real32 ChannelTolerance[MaximumChannelCount];

    // Vertices
    data_type_definition const *VertexType;
    int32x VertexSize;
    int32x ChannelCount;

    int32x VertexCount;
    stack_allocator VertexStack;                       // Per vertex

    stack_allocator NextCoincidentVertStack;           // Per vertex
    stack_allocator FromTriangleStack;                 // Per vertex

    stack_allocator FromPolygonStack;       // Per vertex
    stack_allocator FromPolygonIndexStack;  // Per vertex

    triple Point;

    int32x BoneCount;
    real32 *BoneWeights;

    int32x WeightCount;
    vertex_weight_arrays WeightArrays;

    // Triangles
    stack_allocator VertexIndexStack;                  // 3 per triangle
    stack_allocator NextInMaterialStack;               // Per triangle

    int32x MaterialCount;
    int32x UsedMaterialCount;
    int32x *FirstMaterialTriangle;

    stack_allocator EdgeStack;
    int32x *EdgeHashHeads;

    int32x NextUnusedTriangle;

    int32x MaterialIndex;
    int32x VertexIndex[3];

    int32x Polygon;
    int32x PolygonIndex[3];

    data_type_definition *BufferedVertexType;
    data_type_definition *ChannelTypes;
    int32x BufferedPrefixSize;
    int32x BufferedChannelSize;
    int32x BufferedVertexSize;
    buffered_vertex_prefix *BufferedVertex[3];
    buffered_vertex_prefix *ComparisonVertex;

    // This is not BufferedVertexType, it is just VertexType
    void *TruncatedVertex;

    // Scratch space for the tangent-space generator, if employed
    stack_allocator TriangleTangentStack;              // Per triangle
    stack_allocator VertexTangentStack;                // Per vertex

    // The tool's names for the vertex components.
    // Copied straight into the generated vertex_data
    int32 VertexComponentNameCount;
    char **VertexComponentNames;

    // The size of this map is of course TriangleCount
    int32x* OrigTriIndexToFinalIndex;
};

END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_MESH_BUILDER_INTERNAL_H
#endif /* GRANNY_MESH_BUILDER_INTERNAL_H */
