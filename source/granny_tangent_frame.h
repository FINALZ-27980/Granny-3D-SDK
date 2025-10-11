#if !defined(GRANNY_TANGENT_FRAME_H)
#include "header_preamble.h"
// ========================================================================
// $File: //jeffr/granny_29/rt/granny_tangent_frame.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================

#if !defined(GRANNY_TYPES_H)
#include "granny_types.h"
#endif

BEGIN_GRANNY_NAMESPACE EXPGROUP(MeshGroup);

struct vertex_data;
struct tri_topology;
struct data_type_definition;
struct mesh_builder;

EXPTYPE struct tangent_frame
{
    triple U;
    triple V;
    triple N;
};
EXPCONST EXPGROUP(gbx333_vertex) extern data_type_definition GBX333VertexType[];

void FrameSubtract3(tangent_frame &Result,
                    tangent_frame const &A, tangent_frame const &B);
void FrameScale3(tangent_frame &Result, real32 Scalar);
void FrameScale3(tangent_frame &Result,
                 real32 Scalar, tangent_frame const &A);
void ScaleFrameAdd3(tangent_frame &Result,
                    real32 Scalar, tangent_frame const &B);
void ScaleFrameAdd3(tangent_frame &Result,
                    tangent_frame const &A,
                    real32 Scalar, tangent_frame const &B);
void FrameAdd3(tangent_frame &Result, tangent_frame const &A);

EXPAPI GS_PARAM bool BuildTangentSpace(mesh_builder& Builder);


END_GRANNY_NAMESPACE;

#include "header_postfix.h"
#define GRANNY_TANGENT_FRAME_H
#endif
