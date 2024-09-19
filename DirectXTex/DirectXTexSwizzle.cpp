//-------------------------------------------------------------------------------------
// DirectXTexImage.cpp
//
// DirectX Texture Library - Image container
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "DirectXTex.h"
#include "DirectXTexP.h"

using namespace DirectX;
using namespace DirectX::Internal;

namespace
{
#ifdef __AVX2__
#define deposit_bits(v,m) _pdep_u32(v,m)
#else
    // N3864 - A constexpr bitwise operations library for C++
    // https://github.com/fmatthew5876/stdcxx-bitops
    uint32_t deposit_bits(uint32_t val, uint32_t mask)
    {
        uint32_t res = 0;
        for (uint32_t bb = 1; mask != 0; bb += bb)
        {
            if (val & bb)
            {
                res |= mask & (-mask);
            }
            mask &= (mask - 1);
        }
        return res;
    }
#endif
}


_Use_decl_annotations_
HRESULT DirectX::StandardSwizzle(const Image& srcImage, bool toSwizzle, ScratchImage& result) noexcept
{
    if (!IsValid(srcImage.format))
        return E_INVALIDARG;

    if (IsTypeless(srcImage.format) || IsPlanar(srcImage.format) || IsPalettized(srcImage.format))
        return HRESULT_E_NOT_SUPPORTED;

    HRESULT hr = result.Initialize2D(srcImage.format, srcImage.width, srcImage.height, 1, 1);
    if (FAILED(hr))
        return hr;

    Image uncompressedSource;
    Image uncompressedDestination;
    ScratchImage wide;
    ScratchImage dest;
    if (IsCompressed(srcImage.format))
    {
        wide = ScratchImage();
        HRESULT de = Decompress(srcImage, srcImage.format, wide);
        if (FAILED(de))
            return de;
        uncompressedSource = wide.GetImages()[0];

        dest = ScratchImage();
        HRESULT dcom = dest.Initialize2D(uncompressedSource.format, srcImage.width, srcImage.height, 1, 1);
        if (FAILED(dcom))
            return dcom;
        uncompressedDestination = dest.GetImages()[0];
    }
    else
    {
        uncompressedSource = srcImage;
        uncompressedDestination = result.GetImages()[0];
    }

    const uint8_t* sptr = uncompressedSource.pixels;
    if (!sptr)
        return E_POINTER;

    uint8_t* dptr = uncompressedDestination.pixels;
    if (!dptr)
        return E_POINTER;

    size_t bytesPerPixel = BitsPerPixel(uncompressedSource.format) / 8;

    uint32_t xBytesMask = 0b1010101010101010;
    uint32_t yBytesMask = 0b0101010101010101;

    for (size_t y = 0; y < uncompressedSource.height; y++)
    {
        for (size_t x = 0; x < uncompressedSource.width; x++)
        {
            uint32_t swizzleIndex = deposit_bits(x, xBytesMask) + deposit_bits(y, yBytesMask);
            size_t swizzleOffset = swizzleIndex * bytesPerPixel;

            size_t rowMajorOffset = y * uncompressedSource.rowPitch + x * bytesPerPixel;

            size_t sourceOffset = toSwizzle ? rowMajorOffset : swizzleOffset;
            size_t destOffset   = toSwizzle ? swizzleOffset  : rowMajorOffset;

            const uint8_t* sourcePixelPointer = sptr + sourceOffset;
            uint8_t* destPixelPointer = dptr + destOffset;
            memcpy(destPixelPointer, sourcePixelPointer, bytesPerPixel);
        }
    }

    if (IsCompressed(srcImage.format))
    {
        TEX_COMPRESS_FLAGS flags = TEX_COMPRESS_DEFAULT; // !!TODO!! actual value
        float threshold = -42; // !!TODO!! actual value

        Compress(uncompressedDestination, srcImage.format, flags, threshold, result);
        wide.Release();
        dest.Release();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT DirectX::StandardSwizzle(const Image* srcImages, size_t nimages, const TexMetadata& metadata, bool toSwizzle, ScratchImage& result) noexcept
{
    HRESULT hr = result.Initialize2D(srcImages[0].format, srcImages[0].width, srcImages[0].height, nimages, 1);

    if (!srcImages || !nimages || !IsValid(metadata.format) || nimages > metadata.mipLevels || !result.GetImages())
        return E_INVALIDARG;

    if (metadata.IsVolumemap()
        || IsCompressed(metadata.format) || IsTypeless(metadata.format) || IsPlanar(metadata.format) || IsPalettized(metadata.format))
        return HRESULT_E_NOT_SUPPORTED;

    if (srcImages[0].format != metadata.format || srcImages[0].width != metadata.width || srcImages[0].height != metadata.height)
    {
        // Base image must be the same format, width, and height
        return E_FAIL;
    }

    const Image* uncompressedSource;
    const Image* uncompressedDestination;
    ScratchImage wide;
    ScratchImage dest;
    if (IsCompressed(metadata.format))
    {
        wide = ScratchImage();
        HRESULT de = Decompress(srcImages, nimages, metadata, metadata.format, wide);
        if (FAILED(de))
            return de;
        uncompressedSource = wide.GetImages();

        dest = ScratchImage();
        HRESULT dcom = dest.Initialize2D(uncompressedSource[0].format, srcImages[0].width, srcImages[0].height, nimages, 1);
        if (FAILED(dcom))
            return dcom;
        uncompressedDestination = dest.GetImages();
    }
    else
    {
        uncompressedSource = srcImages;
        uncompressedDestination = result.GetImages();
    }

    for (size_t imageIndex = 0; imageIndex < nimages; imageIndex++)
    {
        const uint8_t* sptr = uncompressedSource[imageIndex].pixels;
        if (!sptr)
            return E_POINTER;

        uint8_t* dptr = uncompressedDestination[imageIndex].pixels;
        if (!dptr)
            return E_POINTER;

        size_t bytesPerPixel = BitsPerPixel(uncompressedSource[imageIndex].format) / 8;

        uint32_t xBytesMask = 0b1010101010101010;
        uint32_t yBytesMask = 0b0101010101010101;

        for (size_t y = 0; y < uncompressedSource[imageIndex].height; y++)
        {
            for (size_t x = 0; x < uncompressedSource[imageIndex].width; x++)
            {
                uint32_t swizzleIndex = deposit_bits(x, xBytesMask) + deposit_bits(y, yBytesMask);
                size_t swizzleOffset = swizzleIndex * bytesPerPixel;

                size_t rowMajorOffset = y * uncompressedSource[0].rowPitch + x * bytesPerPixel;

                size_t sourceOffset = toSwizzle ? rowMajorOffset : swizzleOffset;
                size_t destOffset = toSwizzle ? swizzleOffset : rowMajorOffset;

                const uint8_t* sourcePixelPointer = sptr + sourceOffset;
                uint8_t* destPixelPointer = dptr + destOffset;
                memcpy(destPixelPointer, sourcePixelPointer, bytesPerPixel);
            }
        }
    }

    if (IsCompressed(metadata.format))
    {
        TEX_COMPRESS_FLAGS flags = TEX_COMPRESS_DEFAULT; // !!TODO!! actual value
        float threshold = -42; // !!TODO!! actual value

        Compress(uncompressedDestination, nimages, metadata, metadata.format, flags, threshold, result);
        wide.Release();
        dest.Release();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT DirectX::StandardSwizzle3D(const Image* srcImages, size_t depth, const TexMetadata& metadata, bool toSwizzle, ScratchImage& result) noexcept
{
    HRESULT hr = result.Initialize3D(srcImages[0].format, srcImages[0].width, srcImages[0].height, depth, 1);

    if (!srcImages || !depth || !IsValid(metadata.format) || depth > metadata.depth || !result.GetImages())
        return E_INVALIDARG;

    if (metadata.IsVolumemap()
        || IsCompressed(metadata.format) || IsTypeless(metadata.format) || IsPlanar(metadata.format) || IsPalettized(metadata.format))
        return HRESULT_E_NOT_SUPPORTED;

    if (srcImages[0].format != metadata.format || srcImages[0].width != metadata.width || srcImages[0].height != metadata.height)
    {
        // Base image must be the same format, width, and height
        return E_FAIL;
    }

    const Image* uncompressedSource;
    const Image* uncompressedDestination;
    ScratchImage wide;
    ScratchImage dest;
    if (IsCompressed(metadata.format))
    {
        wide = ScratchImage();
        HRESULT de = Decompress(srcImages, depth, metadata, metadata.format, wide);
        if (FAILED(de))
            return de;
        uncompressedSource = wide.GetImages();

        dest = ScratchImage();
        HRESULT dcom = dest.Initialize3D(uncompressedSource[0].format, srcImages[0].width, srcImages[0].height, depth, 1);
        if (FAILED(dcom))
            return dcom;
        uncompressedDestination = dest.GetImages();
    }
    else
    {
        uncompressedSource = srcImages;
        uncompressedDestination = result.GetImages();
    }

    for (size_t slice = 0; slice < depth; slice++)
    {
        const uint8_t* sptr = uncompressedSource[slice].pixels;
        if (!sptr)
            return E_POINTER;

        uint8_t* dptr = uncompressedDestination[slice].pixels;
        if (!dptr)
            return E_POINTER;

        size_t bytesPerPixel = BitsPerPixel(uncompressedSource[slice].format) / 8;

        uint32_t xBytesMask = 0b1001001001001001;
        uint32_t yBytesMask = 0b0100100100100100;
        uint32_t zBytesMask = 0b0010010010010010;

        for (size_t y = 0; y < uncompressedSource[slice].height; y++)
        {
            for (size_t x = 0; x < uncompressedSource[slice].width; x++)
            {
                uint32_t swizzleIndex = deposit_bits(x, xBytesMask) + deposit_bits(y, yBytesMask) + deposit_bits(slice, zBytesMask);
                size_t swizzleOffset = swizzleIndex * bytesPerPixel;

                size_t rowMajorOffset = y * uncompressedSource[0].rowPitch + x * bytesPerPixel;

                size_t sourceOffset = toSwizzle ? rowMajorOffset : swizzleOffset;
                size_t destOffset = toSwizzle ? swizzleOffset : rowMajorOffset;

                const uint8_t* sourcePixelPointer = sptr + sourceOffset;
                uint8_t* destPixelPointer = dptr + destOffset;
                memcpy(destPixelPointer, sourcePixelPointer, bytesPerPixel);
            }
        }
    }

    if (IsCompressed(metadata.format))
    {
        TEX_COMPRESS_FLAGS flags = TEX_COMPRESS_DEFAULT; // !!TODO!! actual value
        float threshold = -42; // !!TODO!! actual value

        Compress(uncompressedDestination, depth, metadata, metadata.format, flags, threshold, result);
        wide.Release();
        dest.Release();
    }

    return S_OK;
}
