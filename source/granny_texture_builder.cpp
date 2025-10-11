// ========================================================================
// $File: //jeffr/granny_29/rt/granny_texture_builder.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// (C) Copyright 1999-2011 by RAD Game Tools, All Rights Reserved.
// ========================================================================
#include "granny_texture_builder.h"

#include "granny_aggr_alloc.h"
#include "granny_bink.h"
#include "granny_limits.h"
#include "granny_memory.h"
#include "granny_memory_ops.h"
#include "granny_parameter_checking.h"
#include "granny_texture.h"

// This should always be the last header included
#include "granny_cpp_settings.h"
// These lines ensure that all source code versions match.  If any of these asserts
// trigger, you have likely combined two difference versions of Granny.
#include "granny_version.h"
CompileAssert(ProductMajorVersion  == 2);
CompileAssert(ProductMinorVersion  == 9);
CompileAssert(ProductBuildNumber   == 12);
// CompileAssert(ProductCustomization == 0); customization not tested, it's valid for this to vary

#undef SubsystemCode
#define SubsystemCode TextureLogMessage

USING_GRANNY_NAMESPACE;

BEGIN_GRANNY_NAMESPACE;

int32x const BestMatchS3TC = 0xFFFF;

struct texture_image_builder
{
    int32x MIPLevelCount;
    texture_mip_level *MIPLevels;

    texture_image_builder *Next;
};

struct texture_builder
{
    int32x Type;
    int32x Encoding;

    int32x ImageCount;
    int32x HeaderByteCount;
    int32x ImageByteCount;
    texture_image_builder *FirstImage;
    texture_image_builder *LastImage;

    pixel_filter_type UpsamplingFilter;
    pixel_filter_type DownsamplingFilter;

    int32x Width;
    int32x Height;
    pixel_layout ResultingLayout;
    int32x SubFormat;

    int32x RawStrideAlignment;

    int32x BinkCompression;
    uint32x BinkFlags;
};

struct image_iterator
{
    texture_builder *Builder;
    int32x MipMapLevel;
    uint8 *DeletePixels;

    int32x OriginalWidth;
    int32x OriginalHeight;
    int32x OriginalStride;
    uint8 const *OriginalPixels;

    int32x Width;
    int32x Height;
    int32x Stride;
    uint8 const *Pixels;
};

END_GRANNY_NAMESPACE;

static texture_builder *
BeginTexture(int32x Width, int32x Height, int32x Encoding)
{
    texture_builder *Builder = Allocate(texture_builder, AllocationBuilder);
    if(Builder)
    {
        Builder->Encoding = Encoding;

        Builder->Type = ColorMapTextureType;
        Builder->ImageCount = 0;
        Builder->HeaderByteCount = 0;
        Builder->ImageByteCount = 0;
        Builder->FirstImage = 0;
        Builder->LastImage = 0;

        Builder->UpsamplingFilter = CubicPixelFilter;
        Builder->DownsamplingFilter = CubicPixelFilter;

        Builder->Width = Width;
        Builder->Height = Height;
    }

    return(Builder);
}

texture_builder *GRANNY
BeginRawTexture(int32x Width, int32x Height,
                pixel_layout const &ResultingLayout,
                int32x StrideAlignment)
{
    texture_builder *Builder = BeginTexture(Width, Height,
                                            RawTextureEncoding);
    if(Builder)
    {
        Builder->ResultingLayout = ResultingLayout;
        Builder->SubFormat = 0;
        Builder->RawStrideAlignment = StrideAlignment;
    }

    return(Builder);
}

texture_builder *GRANNY
BeginS3TCTexture(int32x Width, int32x Height, s3tc_texture_format Format)
{
    texture_builder *Builder = BeginTexture(Width, Height,
                                            S3TCTextureEncoding);
    if(Builder)
    {
        Builder->ResultingLayout = GetS3TCPixelLayout(Format);
        Builder->SubFormat = Format;
    }

    return(Builder);
}

texture_builder *GRANNY
BeginBestMatchS3TCTexture(int32x Width, int32x Height)
{
    texture_builder *Builder = BeginTexture(Width, Height, S3TCTextureEncoding);
    if(Builder)
    {
        // All of the best match formats are equivalent for the result layout
        Builder->ResultingLayout = GetS3TCPixelLayout(S3TCBGRA8888MappedAlpha);
        Builder->SubFormat = BestMatchS3TC;
    }

    return(Builder);
}

texture_builder *GRANNY
BeginBinkTexture(int32x Width, int32x Height,
                 int32x Compression, uint32x Flags)
{
    texture_builder *Builder = BeginTexture(Width, Height,
                                            BinkTextureEncoding);
    if(Builder)
    {
        // Always use Bink1 for compression now.
        CheckCondition((Flags & BinkUseBink1) != 0, Flags |= BinkUseBink1);

        Builder->ResultingLayout = GetBinkPixelLayout(Flags & BinkEncodeAlpha);
        Builder->SubFormat = (int32)Flags;
        Builder->BinkCompression = Compression;
        Builder->BinkFlags = Flags;
    }

    return(Builder);
}

texture_builder *GRANNY
BeginYCoCgTexture(int32x Width, int32x Height)
{
    texture_builder *Builder = BeginTexture(Width, Height, YCoCgTextureEncoding);
    if(Builder)
    {
        Builder->ResultingLayout = BGR888PixelFormat;
        Builder->SubFormat = 0;
    }

    return Builder;
}

texture *GRANNY
EndTexture(texture_builder *Builder)
{
    texture *Texture = 0;

    if(Builder)
    {
        Texture = EndTextureInPlace(Builder, AllocateSize(GetResultingTextureSize(*Builder), AllocationBuilder));
    }

    return(Texture);
}

static void
AggrTexture(aggr_allocator &Allocator,
            texture_builder const &Builder,
            texture *&Texture,
            void *&HeaderData,
            void *&ImageData)
{
    AggrAllocPtr(Allocator, Texture);
    AggrAllocOffsetArrayPtr(Allocator, Texture, Builder.ImageCount,
                            ImageCount, Images);
    AggrAllocSizePtr(Allocator, Builder.ImageByteCount, ImageData);
    AggrAllocSizePtr(Allocator, Builder.HeaderByteCount, HeaderData);
}

int32x GRANNY
GetResultingTextureSize(texture_builder const &Builder)
{
    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    texture *Texture;
    void *HeaderData;
    void *ImageData;
    AggrTexture(Allocator, Builder, Texture, HeaderData, ImageData);

    int ResultingSize;
    CheckConvertToInt32(ResultingSize, EndAggrSize(Allocator), return 0);
    return ResultingSize;
}

texture *GRANNY
EndTextureInPlace(texture_builder *Builder, void *Memory)
{
    texture *Texture = 0;

    if(Builder)
    {
        aggr_allocator Allocator;
        InitializeAggrAlloc(Allocator);

        void *HeaderData = 0;
        void *ImageData = 0;
        AggrTexture(Allocator, *Builder, Texture, HeaderData, ImageData);
        if(EndAggrPlacement(Allocator, Memory))
        {
            Texture->FromFileName = 0;
            Texture->TextureType = Builder->Type;
            Texture->Width = Builder->Width;
            Texture->Height = Builder->Height;
            Texture->Encoding = Builder->Encoding;
            Texture->SubFormat = Builder->SubFormat;
            Texture->Layout = Builder->ResultingLayout;
            Texture->ExtendedData.Type = 0;
            Texture->ExtendedData.Object = 0;

            texture_image *ToImage = Texture->Images;
            texture_mip_level *MIPPtr = (texture_mip_level *)HeaderData;
            uint8 *ImagePtr = (uint8 *)ImageData;
            {for(texture_image_builder *FromImage = Builder->FirstImage;
                 FromImage;
                 FromImage = FromImage->Next, ++ToImage)
            {
                ToImage->MIPLevelCount = FromImage->MIPLevelCount;
                ToImage->MIPLevels = MIPPtr;
                MIPPtr += ToImage->MIPLevelCount;

                {for(int32x MIPLevelIndex = 0;
                     MIPLevelIndex < ToImage->MIPLevelCount;
                     ++MIPLevelIndex)
                {
                    texture_mip_level &FromMIPLevel =
                        FromImage->MIPLevels[MIPLevelIndex];
                    texture_mip_level &ToMIPLevel =
                        ToImage->MIPLevels[MIPLevelIndex];

                    ToMIPLevel.Stride = FromMIPLevel.Stride;
                    ToMIPLevel.PixelByteCount = FromMIPLevel.PixelByteCount;
                    ToMIPLevel.PixelBytes = ImagePtr;
                    Copy(ToMIPLevel.PixelByteCount, FromMIPLevel.PixelBytes,
                         ToMIPLevel.PixelBytes);
                    ImagePtr += ToMIPLevel.PixelByteCount;
                }}
            }}

            Assert(((uint8 *)ImageData + Builder->ImageByteCount) == ImagePtr);
            Assert(((uint8 *)HeaderData + Builder->HeaderByteCount) == (uint8 *)MIPPtr);
        }

        texture_image_builder *WalkImage = Builder->FirstImage;
        while(WalkImage)
        {
            texture_image_builder *DeleteImage = WalkImage;
            WalkImage = WalkImage->Next;

            Deallocate(DeleteImage);
        }
        Deallocate(Builder);
    }

    return(Texture);
}

void GRANNY
SetImageScalingFilter(texture_builder &Builder,
                      pixel_filter_type UpsamplingFilter,
                      pixel_filter_type DownsamplingFilter)
{
    Builder.UpsamplingFilter = UpsamplingFilter;
    Builder.DownsamplingFilter = DownsamplingFilter;

    if(Builder.UpsamplingFilter != Builder.DownsamplingFilter)
    {
        LogNotImplemented("Image scaler does not currently support "
                          "different filters for upsampling and downsampling");
    }
}

static bool
IsValid(image_iterator const &Iterator)
{
    return(Iterator.MipMapLevel != 0);
}

static void
Build(image_iterator &Iterator)
{
    if(IsValid(Iterator) && Iterator.OriginalPixels)
    {
        if((Iterator.Width == Iterator.OriginalWidth) &&
           (Iterator.Height == Iterator.OriginalHeight))
        {
            Iterator.Pixels = Iterator.OriginalPixels;
            Iterator.DeletePixels = 0;
            Iterator.Stride = Iterator.OriginalStride;
        }
        else
        {
            Iterator.Stride = Iterator.Width * SizeOf(uint32);
            int32x Size = Iterator.Height * Iterator.Stride;
            Iterator.Pixels = Iterator.DeletePixels = (uint8 *)AllocateSize(Size, AllocationBuilder);
            if(Iterator.Pixels)
            {
                ScaleImage(Iterator.Builder->DownsamplingFilter,
                           Iterator.OriginalWidth, Iterator.OriginalHeight,
                           Iterator.OriginalStride, Iterator.OriginalPixels,
                           Iterator.Width, Iterator.Height,
                           Iterator.Stride, Iterator.DeletePixels);
            }
            else
            {
                // Invalidate the iterator
                Iterator.MipMapLevel = 0;
                Iterator.Width = 0;
                Iterator.Height = 0;

                Log0(ErrorLogMessage, TextureLogMessage,
                     "Out of memory in texture image builder");
            }
        }
    }
}

static image_iterator
BeginImageSeries(texture_builder &Builder,
                 int32x Width, int32x Height,
                 int32x Stride, int32x MIPCount,
                 uint8 const *Pixels)
{
    image_iterator Iterator;

    Iterator.Builder = &Builder;
    Iterator.MipMapLevel = MIPCount;
    Iterator.DeletePixels = 0;
    Iterator.OriginalWidth = Width;
    Iterator.OriginalHeight = Height;
    Iterator.OriginalStride = Stride;
    Iterator.OriginalPixels = Pixels;

    Iterator.Width = Builder.Width;
    Iterator.Height = Builder.Height;
    Iterator.Stride = Stride;
    Iterator.Pixels = Pixels;

    Build(Iterator);
    return Iterator;
}

static void
Advance(image_iterator &Iterator)
{
    Deallocate(Iterator.DeletePixels);
    Iterator.Pixels = 0;
    Iterator.DeletePixels = 0;

    --Iterator.MipMapLevel;
    Iterator.Width >>= 1;
    Iterator.Height >>= 1;

    if (Iterator.MipMapLevel)
    {
        Assert(Iterator.Width != 0 || Iterator.Height != 0);
        if (Iterator.Width  == 0) Iterator.Width  = 1;
        if (Iterator.Height == 0) Iterator.Height = 1;
    }

    Build(Iterator);
}

static texture_image_builder *
AddImage(texture_builder &Builder, int32x MIPLevelCount, int32x ImageSize,
         uint8 *&ImageMemory)
{
    aggr_allocator Allocator;
    InitializeAggrAlloc(Allocator);

    texture_image_builder *ImageBuilder;
    AggrAllocPtr(Allocator, ImageBuilder);
    AggrAllocOffsetArrayPtr(Allocator, ImageBuilder, MIPLevelCount,
                            MIPLevelCount, MIPLevels);
    AggrAllocSizePtr(Allocator, ImageSize, ImageMemory);
    if(EndAggrAlloc(Allocator, AllocationBuilder))
    {
        ImageBuilder->Next = 0;

        if(!Builder.FirstImage)
        {
            Builder.FirstImage = Builder.LastImage = ImageBuilder;
        }
        else
        {
            Builder.LastImage = Builder.LastImage->Next = ImageBuilder;
        }

        ++Builder.ImageCount;
        Builder.HeaderByteCount += MIPLevelCount*SizeOf(texture_mip_level);
        Builder.ImageByteCount += ImageSize;
    }

    return(ImageBuilder);
}

static int32x
GetRawStride(texture_builder &Builder, int32x Width)
{
    int32x AlignedStride;
    CheckConvertToInt32(AlignedStride,
                        AlignN(Width * Builder.ResultingLayout.BytesPerPixel,
                               Builder.RawStrideAlignment),
                        return 0);
    return AlignedStride;
}

static texture_mip_level
EncodeImageAsRaw(texture_builder &Builder,
                 image_iterator Iterator,
                 int32x ImageSize,
                 uint8 *ImageMemory)
{
    texture_mip_level MIPLevel;
    MIPLevel.Stride = GetRawStride(Builder, Iterator.Width);
    MIPLevel.PixelByteCount = ImageSize;
    MIPLevel.PixelBytes = ImageMemory;

    ConvertPixelFormat(Iterator.Width, Iterator.Height,
                       RGBA8888PixelFormat, Iterator.Stride,
                       Iterator.Pixels,
                       Builder.ResultingLayout, MIPLevel.Stride,
                       (uint8 *)MIPLevel.PixelBytes);

    return(MIPLevel);
}

static texture_mip_level
EncodeImageAsS3TC(texture_builder &Builder,
                  image_iterator Iterator,
                  int32x ImageSize,
                  uint8 *ImageMemory)
{
    texture_mip_level MIPLevel;
    MIPLevel.PixelByteCount = ImageSize;
    MIPLevel.PixelBytes = ImageMemory;

    int32x Error = 0;

    // Make sure the stride is correctly computed.  Each mip
    // level will be padded out to at least 4x4
    int32x const StrideWidth = Iterator.Width >= 4 ? Iterator.Width : 4;

    switch(Builder.SubFormat)
    {
        case S3TCBGR565:
        {
            MIPLevel.Stride = StrideWidth / 2;
            Error = to_S3TC1(MIPLevel.PixelBytes,
                             Iterator.Pixels,
                             Iterator.Stride,
                             Iterator.Width,
                             Iterator.Height);
        } break;

        case S3TCBGRA5551:
        {
            MIPLevel.Stride = StrideWidth / 2;
            Error = to_S3TC1a(MIPLevel.PixelBytes,
                              Iterator.Pixels,
                              Iterator.Stride,
                              Iterator.Width,
                              Iterator.Height);
        } break;

        case S3TCBGRA8888MappedAlpha:
        {
            MIPLevel.Stride = StrideWidth;
            Error = to_S3TC2or3(MIPLevel.PixelBytes,
                                Iterator.Pixels,
                                Iterator.Stride,
                                Iterator.Width,
                                Iterator.Height);
        } break;

        case S3TCBGRA8888InterpolatedAlpha:
        {
            MIPLevel.Stride = StrideWidth;
            Error = to_S3TC4or5(MIPLevel.PixelBytes,
                                Iterator.Pixels,
                                Iterator.Stride,
                                Iterator.Width,
                                Iterator.Height);
        } break;

        case BestMatchS3TC:
        {
            MIPLevel.Stride = StrideWidth;
            uint32x Type;
            Error = to_S3TC2or3vs4or5(MIPLevel.PixelBytes,
                                      Iterator.Pixels,
                                      Iterator.Stride,
                                      Iterator.Width,
                                      Iterator.Height,
                                      &Type);
            Builder.SubFormat = Type ? S3TCBGRA8888InterpolatedAlpha :
                S3TCBGRA8888MappedAlpha;
        } break;

        default:
        {
            Log1(ErrorLogMessage, TextureLogMessage,
                 "Unrecognized S3TC texture type (%d)", Builder.SubFormat);
        } break;
    }

    if (Error != 0)
    {
#if DEBUG
        Log1(WarningLogMessage, TextureLogMessage,
             "Encountered %d errors in converting to S3TC texture", Error);
#endif
    }

    return(MIPLevel);
}

static texture_mip_level
EncodeImageAsYCoCg(texture_builder& Builder,
                   image_iterator   Iterator,
                   int32x           ImageSize,
                   uint8*           ImageMemory)
{
    texture_mip_level MIPLevel;
    MIPLevel.PixelByteCount = ImageSize;
    MIPLevel.PixelBytes     = ImageMemory;

    // Make sure the stride is correctly computed.  Each mip
    // level will be padded out to at least 4x4
    int32x const StrideWidth = Iterator.Width >= 4 ? Iterator.Width : 4;


    /* RGB <-> YCoCg
       Y  = [ 1/4  1/2  1/4] [R]
       Co = [ 1/2    0 -1/2] [G]
       CG = [-1/4  1/2 -1/4] [B]

       R  = [   1    1   -1] [Y]
       G  = [   1    0    1] [Co]
       B  = [   1   -1   -1] [Cg] */

    // Convert Iterator.Pixels in place.  Note that we need to bias CoCg to 0..255.  The
    // matrix formulation above will yield CoCg values in [-128, 1/2]

    // int32x Width;
    // int32x Height;
    // int32x Stride;
    // uint8 const *Pixels;
    uint8* CurrRow = (uint8*)(Iterator.Pixels);
    {for (int32x Y = 0; Y < Iterator.Height; ++Y)
    {
        {for (int32x X = 0; X < Iterator.Width; ++X)
        {
            uint8* Pixel = CurrRow + (X * 4);

            const int32x r = Pixel[0];
            const int32x g = Pixel[1];
            const int32x b = Pixel[2];
            // const int32x a = Pixel[3];

            // Doing a scaled /4 rather than /2 for y/cg
            int32x const y  = (((r + (g<<1) + b) + 2) >> 2);
            int32x const co = ((((r<<1) - (b<<1)) + 2) >> 2);
            int32x const cg = (((-r + (g<<1) -  b) + 2) >> 2);
            Assert(y >= 0 && y <= 255);
            Assert(co >= -127 && co <= 128);
            Assert(cg >= -127 && cg <= 128);

            uint8 const StoreY  = uint8(y);
            uint8 const StoreCo = uint8(co + 127);
            uint8 const StoreCg = uint8(cg + 127);

            Pixel[0] = StoreCo;
            Pixel[1] = StoreCg;
            Pixel[2] = 0;
            Pixel[3] = StoreY;
        }}

        CurrRow += Iterator.Stride;
    }}

    MIPLevel.Stride = StrideWidth;
    int32x Error = to_S3TC4or5(MIPLevel.PixelBytes,
                               Iterator.Pixels,
                               Iterator.Stride,
                               Iterator.Width,
                               Iterator.Height);

    if (Error != 0)
    {
#if DEBUG
        Log1(WarningLogMessage, TextureLogMessage, "Encountered %d errors in converting to YCoCg texture", Error);
#endif
    }

    return MIPLevel;
}

static texture_mip_level
EncodeImageAsBink(texture_builder &Builder,
                  image_iterator Iterator,
                  int32x ImageSize,
                  uint8 *ImageMemory)
{
    texture_mip_level MIPLevel;
    MIPLevel.Stride = 0;
    MIPLevel.PixelBytes = ImageMemory;

    MIPLevel.PixelByteCount = BinkCompressTexture(
        Iterator.Width, Iterator.Height, Iterator.Stride,
        Iterator.Pixels,
        Builder.BinkFlags, Builder.BinkCompression,
        MIPLevel.PixelBytes);

    return(MIPLevel);
}

void GRANNY
EncodeImage(texture_builder &Builder,
            int32x SourceWidth, int32x SourceHeight, int32x SourceStride,
            int32x MIPLevelCount, void const *RGBAData)
{
    Assert(MIPLevelCount < MaximumMIPLevelCount);
    int32x ImageSizes[MaximumMIPLevelCount];

    int32x MaxImageSize = 0;
    int32x ImageIndex = 0;
    {for(image_iterator Iterator =
             BeginImageSeries(Builder, SourceWidth, SourceHeight,
                              SourceStride, MIPLevelCount, 0);
         IsValid(Iterator);
         Advance(Iterator), ++ImageIndex)
    {
        int32x ImageSize = 0;

        switch(Builder.Encoding)
        {
            case RawTextureEncoding:
            {
                ImageSize += GetRawImageSize(
                    Builder.ResultingLayout,
                    GetRawStride(Builder, Iterator.Width),
                    Iterator.Width, Iterator.Height);
            } break;

            case S3TCTextureEncoding:
            {
                ImageSize += GetS3TCImageSize(
                    (s3tc_texture_format)
                    ((Builder.SubFormat == BestMatchS3TC) ?
                     S3TCBGRA8888MappedAlpha : Builder.SubFormat),
                    Iterator.Width,
                    Iterator.Height);
            } break;

            case YCoCgTextureEncoding:
            {
                ImageSize += GetYCoCgImageSize(Iterator.Width, Iterator.Height);
            } break;

            case BinkTextureEncoding:
            {
                ImageSize += GetMaximumBinkImageSize(
                    Iterator.Width, Iterator.Height,
                    Builder.BinkFlags, Builder.BinkCompression);
            } break;

            default:
            {
                Log1(ErrorLogMessage, TextureLogMessage,
                     "Unrecognized texture encoding %d.", Builder.Encoding);
            } break;
        }

        MaxImageSize += ImageSizes[ImageIndex] = ImageSize;
    }}

    uint8 *ImageMemory;
    texture_image_builder *Image = AddImage(Builder, MIPLevelCount,
                                            MaxImageSize, ImageMemory);


    int32x ActualImageSize = 0;
    ImageIndex = 0;
    {for(image_iterator Iterator =
             BeginImageSeries(Builder, SourceWidth, SourceHeight,
                              SourceStride, MIPLevelCount,
                              (uint8 const *)RGBAData);
         IsValid(Iterator);
         Advance(Iterator), ++ImageIndex)
    {
        int32x ImageSize = ImageSizes[ImageIndex];

        texture_mip_level &MIPLevel = Image->MIPLevels[ImageIndex];
        MIPLevel.Stride = 0;
        MIPLevel.PixelByteCount = 0;
        MIPLevel.PixelBytes = 0;

        switch(Builder.Encoding)
        {
            case RawTextureEncoding:
            {
                MIPLevel = EncodeImageAsRaw(Builder, Iterator,
                                            ImageSize, ImageMemory);
            } break;

            case S3TCTextureEncoding:
            {
                MIPLevel = EncodeImageAsS3TC(Builder, Iterator,
                                             ImageSize, ImageMemory);
            } break;

            case YCoCgTextureEncoding:
            {
                MIPLevel = EncodeImageAsYCoCg(Builder, Iterator, ImageSize, ImageMemory);
            } break;

            case BinkTextureEncoding:
            {
                MIPLevel = EncodeImageAsBink(Builder, Iterator,
                                             ImageSize, ImageMemory);
            } break;

            default:
            {
                Log1(ErrorLogMessage, TextureLogMessage,
                     "Unrecognized texture encoding %d.", Builder.Encoding);
            } break;
        }

        ActualImageSize += MIPLevel.PixelByteCount;
        ImageMemory += MIPLevel.PixelByteCount;
    }}

    Assert(ActualImageSize <= MaxImageSize);
    Builder.ImageByteCount += ActualImageSize - MaxImageSize;
}
