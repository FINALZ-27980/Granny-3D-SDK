// ========================================================================
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_back_compat.h"

#include "granny_animation.h"
#include "granny_assert.h"
#include "granny_bspline_buffers.h"
#include "granny_data_type_definition.h"
#include "granny_file_info.h"
#include "granny_limits.h"
#include "granny_memory_arena.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"
#include "granny_texture.h"
#include "granny_track_group.h"

// Should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary
USING_GRANNY_NAMESPACE;

data_type_definition* GRANNY
GetFileInfoType()
{
    return FileInfoType;
}


static bool
DecompressTexture(texture* Texture, memory_arena* Arena)
{
    // Already decompressed?
    if (Texture->Encoding == RawTextureEncoding)
        return true;
    
    pixel_layout Layout;
    if (GetRecommendedPixelLayout(*Texture, Layout))
    {
        {for (int ImageIdx = 0; ImageIdx < Texture->ImageCount; ++ImageIdx)
        {
            texture_image& Image = Texture->Images[ImageIdx];

            {for (int MIPIdx = 0; MIPIdx < Image.MIPLevelCount; ++MIPIdx)
            {
                texture_mip_level& MIPLevel = Image.MIPLevels[MIPIdx];

                int32x MipWidth  = (Texture->Width  >> MIPIdx) != 0 ? (Texture->Width  >> MIPIdx) : 1;
                int32x MipHeight = (Texture->Height >> MIPIdx) != 0 ? (Texture->Height >> MIPIdx) : 1;
                int32x MipSize   = GetRawImageSize(Layout, MipWidth*Layout.BytesPerPixel,MipWidth,MipHeight);

                uint8* NewMip = ArenaPushArray(*Arena, MipSize, uint8);
                if (!CopyTextureImage(*Texture, ImageIdx, MIPIdx, Layout,
                                      MipWidth, MipHeight, MipWidth*Layout.BytesPerPixel,
                                      NewMip))
                {
                    Log3(ErrorLogMessage, TextureLogMessage,
                         "Unable to copy Image: %d Mip: %d from Texture: %s\n",
                         ImageIdx, MIPIdx, Texture->FromFileName);
                    return false;
                }

                MIPLevel.Stride = MipWidth * Layout.BytesPerPixel;
                MIPLevel.PixelByteCount = MipSize;
                MIPLevel.PixelBytes = NewMip;
            }}
        }}

        // Markup the texture...
        Texture->Encoding = RawTextureEncoding;
        Texture->SubFormat = 0;
        Copy(sizeof(Layout), &Layout, &Texture->Layout);

        return true;
    }
    else
    {
        Log1(ErrorLogMessage, TextureLogMessage,
             "Unable to obtain texture format for texture: %s\n",
             Texture->FromFileName);
        return false;
    }
}

static bool
DecompressTrackGroup(track_group* TrackGroup, real32 Duration, memory_arena* Arena)
{
    // Replace all curves with DaK32f, including constant and identity curves...
#define ReplaceCurve(curve_var, curvedim, norm, idvec)                                              \
    if (CurveGetKnotCount(curve_var) > 1)                                                           \
    {                                                                                               \
        int32x CurveSize   = GetResultingDaK32fC32fCurveSize(curve_var);                            \
        void*  CurveMemory = ArenaPushSize(*Arena, CurveSize);                                      \
        curve2* NewCurve = CurveConvertToDaK32fC32fInPlace(curve_var, CurveMemory, idvec);          \
        curve_var = *NewCurve;                                                                      \
    } else {                                                                                        \
        curve_builder* Builder = BeginCurve(CurveDataDaK32fC32fType, 0, curvedim, 1);               \
        real32 Knot = 0.0f;                                                                         \
        real32 Control[MaximumBSplineDimension];                                                    \
        EvaluateCurveAtT(curvedim, norm, false, curve_var, false, Duration, 0, &Control[0], idvec); \
        PushCurveKnotArray(Builder, &Knot);                                                         \
        PushCurveControlArray(Builder, &Control[0]);                                                \
        int CurveSize = GetResultingCurveSize(Builder);                                             \
        void*  CurveMemory = ArenaPushSize(*Arena, CurveSize);                                      \
        curve2* NewCurve = EndCurveInPlace(Builder, CurveMemory);                                   \
        curve_var = *NewCurve;                                                                      \
    } typedef int __require_semi
    
    static real32 VecIdentity[16] = { 0 };
    for (int Idx = 0; Idx < TrackGroup->VectorTrackCount; ++Idx)
    {
        vector_track& Track = TrackGroup->VectorTracks[Idx];
        ReplaceCurve(Track.ValueCurve, Track.Dimension, false, VecIdentity);
    }

    for (int Idx = 0; Idx < TrackGroup->TransformTrackCount; ++Idx)
    {
        transform_track& Track = TrackGroup->TransformTracks[Idx];
        
        ReplaceCurve(Track.PositionCurve, 3, false, CurveIdentityPosition);
        ReplaceCurve(Track.OrientationCurve, 4, true, CurveIdentityOrientation);
        ReplaceCurve(Track.ScaleShearCurve, 9, false, CurveIdentityScaleShear);
    }

    return true;
}


bool GRANNY
ClearOldStructures(file_info* Info, memory_arena* Arena)
{
    // Decompress *all* textures into raw form...
    for (int TexIdx = 0; TexIdx < Info->TextureCount; ++TexIdx)
    {
        if (!Info->Textures[TexIdx])
            continue;
                
        if (DecompressTexture(Info->Textures[TexIdx], Arena) == false)
            return false;
    }

    // Turn all animated curves into F32.  Note that this assumes that there aren't track
    // groups hiding in the root level array that aren't represented here...
    for (int AnimIdx = 0; AnimIdx < Info->AnimationCount; ++AnimIdx)
    {
        animation* Animation = Info->Animations[AnimIdx];
        if (!Animation)
            continue;

        for (int TGIdx = 0; TGIdx < Animation->TrackGroupCount; ++TGIdx)
        {
            track_group* TrackGroup = Animation->TrackGroups[TGIdx];
            if (!TrackGroup)
                continue;

            if (!DecompressTrackGroup(TrackGroup, Animation->Duration, Arena))
                return false;
        }
    }

    return true;
}

