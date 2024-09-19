//-------------------------------------------------------------------------------------
// DirectXTexSwizzle.cpp
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

    const uint8_t* sptr = srcImage.pixels;
    if (!sptr)
        return E_POINTER;

    uint8_t* dptr = result.GetImages()[0].pixels;
    if (!dptr)
        return E_POINTER;

    size_t bytesPerPixel = BitsPerPixel(srcImage.format) / 8;

    uint32_t xBytesMask = 0b1010101010101010;
    uint32_t yBytesMask = 0b0101010101010101;

    size_t height = IsCompressed(srcImage.format) ? (srcImage.height + 3) / 4 : srcImage.height;
    size_t width  = IsCompressed(srcImage.format) ? (srcImage.width  + 3) / 4 : srcImage.width;

    for (size_t y = 0; y < height; y++)
    {
        for (size_t x = 0; x < width; x++)
        {
            uint32_t swizzleIndex = deposit_bits(x, xBytesMask) + deposit_bits(y, yBytesMask);
            size_t swizzleOffset = swizzleIndex * bytesPerPixel;

            size_t rowMajorOffset = y * srcImage.rowPitch + x * bytesPerPixel;

            size_t sourceOffset = toSwizzle ? rowMajorOffset : swizzleOffset;
            size_t destOffset   = toSwizzle ? swizzleOffset  : rowMajorOffset;

            const uint8_t* sourcePixelPointer = sptr + sourceOffset;
            uint8_t* destPixelPointer = dptr + destOffset;
            memcpy(destPixelPointer, sourcePixelPointer, bytesPerPixel);
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT DirectX::StandardSwizzle(const Image* srcImages, size_t nimages, const TexMetadata& metadata, bool toSwizzle, ScratchImage& result) noexcept
{
    HRESULT hr = result.Initialize2D(srcImages[0].format, srcImages[0].width, srcImages[0].height, nimages, 1);

    if (!srcImages || !nimages || !IsValid(metadata.format) || nimages > metadata.mipLevels || !result.GetImages())
        return E_INVALIDARG;

    if (metadata.IsVolumemap() || IsTypeless(metadata.format) || IsPlanar(metadata.format) || IsPalettized(metadata.format))
        return HRESULT_E_NOT_SUPPORTED;

    if (srcImages[0].format != metadata.format || srcImages[0].width != metadata.width || srcImages[0].height != metadata.height)
    {
        // Base image must be the same format, width, and height
        return E_FAIL;
    }

    size_t height = IsCompressed(metadata.format) ? (metadata.height + 3) / 4 : metadata.height;
    size_t width  = IsCompressed(metadata.format) ? (metadata.width  + 3) / 4 : metadata.width;

    for (size_t imageIndex = 0; imageIndex < nimages; imageIndex++)
    {
        const uint8_t* sptr = srcImages[imageIndex].pixels;
        if (!sptr)
            return E_POINTER;

        uint8_t* dptr = result.GetImages()[imageIndex].pixels;
        if (!dptr)
            return E_POINTER;

        size_t bytesPerPixel = BitsPerPixel(srcImages[imageIndex].format) / 8;

        uint32_t xBytesMask = 0b1010101010101010;
        uint32_t yBytesMask = 0b0101010101010101;

        for (size_t y = 0; y <height; y++)
        {
            for (size_t x = 0; x < width; x++)
            {
                uint32_t swizzleIndex = deposit_bits(x, xBytesMask) + deposit_bits(y, yBytesMask);
                size_t swizzleOffset = swizzleIndex * bytesPerPixel;

                size_t rowMajorOffset = y * srcImages[0].rowPitch + x * bytesPerPixel;

                size_t sourceOffset = toSwizzle ? rowMajorOffset : swizzleOffset;
                size_t destOffset = toSwizzle ? swizzleOffset : rowMajorOffset;

                const uint8_t* sourcePixelPointer = sptr + sourceOffset;
                uint8_t* destPixelPointer = dptr + destOffset;
                memcpy(destPixelPointer, sourcePixelPointer, bytesPerPixel);
            }
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT DirectX::StandardSwizzle3D(const Image* srcImages, size_t depth, const TexMetadata& metadata, bool toSwizzle, ScratchImage& result) noexcept
{
    HRESULT hr = result.Initialize3D(srcImages[0].format, srcImages[0].width, srcImages[0].height, depth, 1);

    if (!srcImages || !depth || !IsValid(metadata.format) || depth > metadata.depth || !result.GetImages())
        return E_INVALIDARG;

    if (metadata.IsVolumemap() || IsTypeless(metadata.format) || IsPlanar(metadata.format) || IsPalettized(metadata.format))
        return HRESULT_E_NOT_SUPPORTED;

    if (srcImages[0].format != metadata.format || srcImages[0].width != metadata.width || srcImages[0].height != metadata.height)
    {
        // Base image must be the same format, width, and height
        return E_FAIL;
    }

    size_t height = IsCompressed(metadata.format) ? (metadata.height + 3) / 4 : metadata.height;
    size_t width  = IsCompressed(metadata.format) ? (metadata.width  + 3) / 4 : metadata.width;

    size_t bytesPerPixel = BitsPerPixel(srcImages[0].format) / 8;
    uint32_t xBytesMask = 0b1001001001001001;
    uint32_t yBytesMask = 0b0100100100100100;
    uint32_t zBytesMask = 0b0010010010010010;

    for (size_t z = 0; z < depth; z++)
    {
        for (size_t y = 0; y < height; y++)
        {
            for (size_t x = 0; x < width; x++)
            {
                uint32_t swizzle3Dindex = deposit_bits(x, xBytesMask) + deposit_bits(y, yBytesMask) + deposit_bits(z, zBytesMask);
                uint32_t swizzle2Dindex = swizzle3Dindex % (metadata.width * metadata.height);
                uint32_t swizzleSlice   = swizzle3Dindex / (metadata.width * metadata.height);
                size_t swizzleOffset = swizzle2Dindex * bytesPerPixel;

                size_t rowMajorOffset = y * srcImages[0].rowPitch + x * bytesPerPixel;

                size_t sourceOffset = toSwizzle ? rowMajorOffset : swizzleOffset;
                uint32_t sourceSlice = toSwizzle ? z : swizzleSlice;

                size_t destOffset = toSwizzle ? swizzleOffset : rowMajorOffset;
                uint32_t destSlice = toSwizzle ? swizzleSlice : z;

                const uint8_t* sptr = srcImages[sourceSlice].pixels;
                if (!sptr)
                    return E_POINTER;
                uint8_t* dptr = result.GetImages()[destSlice].pixels;
                if (!dptr)
                    return E_POINTER;

                const uint8_t* sourcePixelPointer = sptr + sourceOffset;
                uint8_t* destPixelPointer = dptr + destOffset;
                memcpy(destPixelPointer, sourcePixelPointer, bytesPerPixel);
            }
        }
    }

    return S_OK;
}
