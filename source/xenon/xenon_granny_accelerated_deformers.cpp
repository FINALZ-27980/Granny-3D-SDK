// ========================================================================
// $File: //jeffr/granny_29/rt/xenon/xenon_granny_accelerated_deformers.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_deformers.h"
#include "granny_accelerated_deformers.h"
#include "xenon_granny_xtl.h"

#include "granny_assert.h"
#include "granny_parameter_checking.h"
#include "granny_prefetch.h"
#include "granny_vertex_data.h"

#include <xboxmath.h>

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#define SubsystemCode DeformerLogMessage
USING_GRANNY_NAMESPACE;

#define LOAD_PWNGBT_VERTEX(Num)                                                            \
    XMVECTOR Position ## Num;                                                       \
    XMVECTOR Normal ## Num;                                                         \
    XMVECTOR Tangent ## Num;                                                        \
    XMVECTOR Binormal ## Num;                                                       \
    XMVECTOR Texture ## Num;                                                        \
    XMVECTOR Weight ## Num;                                                         \
    {                                                                               \
        XMVECTOR V0 = XMLoadVector4A16(&Source[VertIdx + Num].Position[0]);         \
        XMVECTOR V1 = XMLoadVector4A16(&Source[VertIdx + Num].Position[0] + 4);     \
        XMVECTOR V2 = XMLoadVector4A16(&Source[VertIdx + Num].Position[0] + 8);     \
        XMVECTOR V3 = XMLoadVector4A16(&Source[VertIdx + Num].Position[0] + 12);    \
                                                                                    \
        Position ## Num = V0;                                                       \
        Normal ## Num   = __vsldoi(V1, V1, 1 << 2);                                 \
        Tangent ## Num  = V2;                                                       \
        Binormal ## Num = __vsldoi(V2, V3, 3 << 2);                                 \
        Texture ## Num  = __vsldoi(V3, V3, 2 << 2);;                                \
                                                                                    \
        /* Modified version of XMLoadUByteN4 */                                     \
        {                                                                           \
            Weight ## Num = __vupkd3d(Position ## Num, VPACK_D3DCOLOR);             \
            Weight ## Num = __vmaddcfp(UnpackScale, Weight ## Num, UnpackOffset);   \
            Weight ## Num = __vpermwi(Weight ## Num, 0xC6); /* 3, 0, 1, 2 */        \
        }                                                                           \
    } typedef int __require_semi


#define TRANSFORM_PNGB(Num)                                                     \
    XMVECTOR XPosition##Num = XMVector3Transform(Position##Num, Mat##Num);      \
    XMVECTOR XNormal##Num   = XMVector3TransformNormal(Normal##Num, Mat##Num);  \
    XMVECTOR XTangent##Num  = XMVector3TransformNormal(Tangent##Num, Mat##Num); \
    XMVECTOR XBinormal##Num = XMVector3TransformNormal(Binormal##Num, Mat##Num)

#define ADD_IN_MATRIX_DIRECT(Num, Idx)                                                              \
    {                                                                                               \
        XMMATRIX MatTemp = XMLoadFloat4x4A16(&Transforms[Source[VertIdx+Num].BoneIndices[Idx]]);    \
		Mat##Num.r[0] += __vspltw(Weight##Num, Idx) * MatTemp.r[0];                                 \
		Mat##Num.r[1] += __vspltw(Weight##Num, Idx) * MatTemp.r[1];                                 \
		Mat##Num.r[2] += __vspltw(Weight##Num, Idx) * MatTemp.r[2];                                 \
		Mat##Num.r[3] += __vspltw(Weight##Num, Idx) * MatTemp.r[3];                                 \
    } typedef int __require_semi

#define ADD_IN_MATRIX_INDIRECT(Num, Idx)                                                            \
    {                                                                                               \
        XMMATRIX MatTemp = XMLoadFloat4x4A16(&Transforms[TransformTable[Source[VertIdx+Num].BoneIndices[Idx]]]); \
        Mat##Num.r[0] += __vspltw(Weight##Num, Idx) * MatTemp.r[0];                                 \
        Mat##Num.r[1] += __vspltw(Weight##Num, Idx) * MatTemp.r[1];                                 \
        Mat##Num.r[2] += __vspltw(Weight##Num, Idx) * MatTemp.r[2];                                 \
        Mat##Num.r[3] += __vspltw(Weight##Num, Idx) * MatTemp.r[3];                                 \
    } typedef int __require_semi

static void
XenonDeformPWNGBT343332D(int32x             Count,
                         void const*        SourceInit,
                         void*              DestInit,
                         matrix_4x4 const*  TransformsInit,
                         int32x             TailCopy32Count,
                         int32x             SourceStride,
                         int32x             DestStride)
{
    if (Count == 0)
        return;

    CheckAlignment(SourceInit, 16, return);
    CheckAlignment(DestInit, 16, return);
    CheckAlignment(TransformsInit, 16, return);

    pwngbt343332_vertex const* NOALIAS Source = (pwngbt343332_vertex const* NOALIAS)SourceInit;
    pngbt33332_vertex* NOALIAS         Dest   = (pngbt33332_vertex* NOALIAS)DestInit;

    XMFLOAT4X4A16 const* NOALIAS Transforms = (XMFLOAT4X4A16 const* NOALIAS)TransformsInit;

    static CONST XMVECTOR    UnpackOffset = XM_UNPACK_UNSIGNEDN_OFFSET(8, 8, 8, 8);
    static CONST XMVECTOR    UnpackScale = XM_UNPACK_UNSIGNEDN_SCALE(8, 8, 8, 8);

    {for (int32x VertIdx = 0; VertIdx < Count-1; VertIdx += 2)
    {
        LOAD_PWNGBT_VERTEX(0);
        LOAD_PWNGBT_VERTEX(1);

        XMMATRIX Mat0(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        {
            ADD_IN_MATRIX_DIRECT(0, 0);
            ADD_IN_MATRIX_DIRECT(0, 1);
            ADD_IN_MATRIX_DIRECT(0, 2);
            ADD_IN_MATRIX_DIRECT(0, 3);
        }

        XMMATRIX Mat1(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        {
            ADD_IN_MATRIX_DIRECT(1, 0);
            ADD_IN_MATRIX_DIRECT(1, 1);
            ADD_IN_MATRIX_DIRECT(1, 2);
            ADD_IN_MATRIX_DIRECT(1, 3);
        }

        TRANSFORM_PNGB(0);
        TRANSFORM_PNGB(1);

        // Pack up the output 2 pngbt = 112 bytes which is 16 byte aligned
        {
            static const XMVECTORI32 g_XMSwizzle_xyzX = { XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_1X };
            static const XMVECTORI32 g_XMSwizzle_yzXY = { XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_1X, XM_PERMUTE_1Y };
            static const XMVECTORI32 g_XMSwizzle_zXYZ = { XM_PERMUTE_0Z, XM_PERMUTE_1X, XM_PERMUTE_1Y, XM_PERMUTE_1Z };
            static const XMVECTORI32 g_XMSwizzle_xyXY = { XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_1X, XM_PERMUTE_1Y };

            XMVECTOR Out0 = __vperm(XPosition0, XNormal0,   g_XMSwizzle_xyzX);  // P0x, P0y, P0z, N0x
            XMVECTOR Out1 = __vperm(XNormal0,   XTangent0,  g_XMSwizzle_yzXY);  // N0y, N0z, T0x, T0y
            XMVECTOR Out2 = __vperm(XTangent0,  XBinormal0, g_XMSwizzle_zXYZ);  // T0z, B0x, B0y, B0z
            XMVECTOR Out3 = __vperm(Texture0,   XPosition1, g_XMSwizzle_xyXY);  // U0x, U0y, P1x, P1y
            XMVECTOR Out4 = __vperm(XPosition1, XNormal1,   g_XMSwizzle_zXYZ);  // P1z, N1x, N1y, N1z
            XMVECTOR Out5 = __vperm(XTangent1,  XBinormal1, g_XMSwizzle_xyzX);  // T1x, T1y, T1z, B1x
            XMVECTOR Out6 = __vperm(XBinormal1, Texture1,   g_XMSwizzle_yzXY);  // B1y, B1z, U1x, U1y

            XMStoreVector4A16(&Dest[VertIdx].Position[0] +  0, Out0);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] +  4, Out1);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] +  8, Out2);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] + 12, Out3);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] + 16, Out4);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] + 20, Out5);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] + 24, Out6);
        }
    }}

    // Cleanup the straggler if the count is odd
    if (Count & 1)
    {
        int const VertIdx = Count-1;
        LOAD_PWNGBT_VERTEX(0);

        XMMATRIX Mat0(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        {
            ADD_IN_MATRIX_DIRECT(0, 0);
            ADD_IN_MATRIX_DIRECT(0, 1);
            ADD_IN_MATRIX_DIRECT(0, 2);
            ADD_IN_MATRIX_DIRECT(0, 3);
        }

        TRANSFORM_PNGB(0);

        XMStoreVector3(&Dest[VertIdx].Position[0], XPosition0);
        XMStoreVector3(&Dest[VertIdx].Normal[0],   XNormal0);
        XMStoreVector3(&Dest[VertIdx].Tangent[0],  XTangent0);
        XMStoreVector3(&Dest[VertIdx].Binormal[0], XBinormal0);
        XMStoreVector2(&Dest[VertIdx].UV[0],       Texture0);
    }
}


static void
XenonDeformPWNGBT343332I(int32x            Count,
                         void const*       SourceInit,
                         void*             DestInit,
                         int32x const*     TransformTable,
                         matrix_4x4 const* TransformsInit,
                         int32x            TailCopy32Count,
                         int32x            SourceStride,
                         int32x            DestStride)
{
    if (Count == 0)
        return;

    CheckAlignment(SourceInit, 16, return);
    CheckAlignment(DestInit, 16, return);
    CheckAlignment(TransformsInit, 16, return);

    pwngbt343332_vertex const* NOALIAS Source = (pwngbt343332_vertex const* NOALIAS)SourceInit;
    pngbt33332_vertex* NOALIAS         Dest   = (pngbt33332_vertex* NOALIAS)DestInit;

    XMFLOAT4X4A16 const* NOALIAS Transforms = (XMFLOAT4X4A16 const* NOALIAS)TransformsInit;

    static CONST XMVECTOR    UnpackOffset = XM_UNPACK_UNSIGNEDN_OFFSET(8, 8, 8, 8);
    static CONST XMVECTOR    UnpackScale = XM_UNPACK_UNSIGNEDN_SCALE(8, 8, 8, 8);

    {for (int32x VertIdx = 0; VertIdx < Count-1; VertIdx += 2)
    {
        LOAD_PWNGBT_VERTEX(0);
        LOAD_PWNGBT_VERTEX(1);

        XMMATRIX Mat0(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        {
            ADD_IN_MATRIX_INDIRECT(0, 0);
            ADD_IN_MATRIX_INDIRECT(0, 1);
            ADD_IN_MATRIX_INDIRECT(0, 2);
            ADD_IN_MATRIX_INDIRECT(0, 3);
        }

        XMMATRIX Mat1(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        {
            ADD_IN_MATRIX_INDIRECT(1, 0);
            ADD_IN_MATRIX_INDIRECT(1, 1);
            ADD_IN_MATRIX_INDIRECT(1, 2);
            ADD_IN_MATRIX_INDIRECT(1, 3);
        }

        TRANSFORM_PNGB(0);
        TRANSFORM_PNGB(1);

        // Pack up the output 2 pngbt = 112 bytes which is 16 byte aligned
        {
            static const XMVECTORI32 g_XMSwizzle_xyzX = { XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_1X };
            static const XMVECTORI32 g_XMSwizzle_yzXY = { XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_1X, XM_PERMUTE_1Y };
            static const XMVECTORI32 g_XMSwizzle_zXYZ = { XM_PERMUTE_0Z, XM_PERMUTE_1X, XM_PERMUTE_1Y, XM_PERMUTE_1Z };
            static const XMVECTORI32 g_XMSwizzle_xyXY = { XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_1X, XM_PERMUTE_1Y };

            XMVECTOR Out0 = __vperm(XPosition0, XNormal0,   g_XMSwizzle_xyzX);  // P0x, P0y, P0z, N0x
            XMVECTOR Out1 = __vperm(XNormal0,   XTangent0,  g_XMSwizzle_yzXY);  // N0y, N0z, T0x, T0y
            XMVECTOR Out2 = __vperm(XTangent0,  XBinormal0, g_XMSwizzle_zXYZ);  // T0z, B0x, B0y, B0z
            XMVECTOR Out3 = __vperm(Texture0,   XPosition1, g_XMSwizzle_xyXY);  // U0x, U0y, P1x, P1y
            XMVECTOR Out4 = __vperm(XPosition1, XNormal1,   g_XMSwizzle_zXYZ);  // P1z, N1x, N1y, N1z
            XMVECTOR Out5 = __vperm(XTangent1,  XBinormal1, g_XMSwizzle_xyzX);  // T1x, T1y, T1z, B1x
            XMVECTOR Out6 = __vperm(XBinormal1, Texture1,   g_XMSwizzle_yzXY);  // B1y, B1z, U1x, U1y

            XMStoreVector4A16(&Dest[VertIdx].Position[0] +  0, Out0);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] +  4, Out1);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] +  8, Out2);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] + 12, Out3);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] + 16, Out4);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] + 20, Out5);
            XMStoreVector4A16(&Dest[VertIdx].Position[0] + 24, Out6);
        }
    }}

    // Cleanup the straggler if the count is odd
    if (Count & 1)
    {
        int const VertIdx = Count-1;
        LOAD_PWNGBT_VERTEX(0);

        XMMATRIX Mat0(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        {
            ADD_IN_MATRIX_INDIRECT(0, 0);
            ADD_IN_MATRIX_INDIRECT(0, 1);
            ADD_IN_MATRIX_INDIRECT(0, 2);
            ADD_IN_MATRIX_INDIRECT(0, 3);
        }

        TRANSFORM_PNGB(0);

        XMStoreVector3(&Dest[VertIdx].Position[0], XPosition0);
        XMStoreVector3(&Dest[VertIdx].Normal[0],   XNormal0);
        XMStoreVector3(&Dest[VertIdx].Tangent[0],  XTangent0);
        XMStoreVector3(&Dest[VertIdx].Binormal[0], XBinormal0);
        XMStoreVector2(&Dest[VertIdx].UV[0],       Texture0);
    }
}


void GRANNY
AddAcceleratedDeformers(void)
{
    bone_deformer XenonDeformers[] = {
        { DeformPositionNormalTangentBinormal,
          XenonDeformPWNGBT343332D, XenonDeformPWNGBT343332I, NULL,
          PWNGBT343332VertexType, PNGBT33332VertexType,
          false, false, true },
    };

    AddBoneDeformerTable(ArrayLength(XenonDeformers), XenonDeformers);
}
