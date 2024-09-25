//-------------------------------------------------------------------------------------
// DirectXTexSwizzle.cpp
//
// DirectX Texture Library - Standard Swizzle (z-order curve)
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
#define extract_bits(v,m) _pext_u32(v,m)
#else
    // N3864 - A constexpr bitwise operations library for C++
    // https://github.com/fmatthew5876/stdcxx-bitops
    uint32_t deposit_bits(uint32_t val, int mask)
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

    uint32_t extract_bits(uint32_t val, int mask)
    {
        uint32_t res = 0;
        for (uint32_t bb = 1; mask !=0; bb += bb)
        {
            if (val & mask & -mask)
            {
                res |= bb;
            }
            mask &= (mask - 1);
        }
        return res;
    }
#endif

    constexpr size_t MAX_TEXTURE_DIMENSION = 16384u;

#if defined(_M_X64) || defined(_M_ARM64)
    constexpr size_t MAX_TEXTURE_SIZE = 16384u * 16384u * 16u;
#else
    constexpr size_t MAX_TEXTURE_SIZE = UINT32_MAX;
#endif
}


//-------------------------------------------------------------------------------------
// 2D z-order curve
//-------------------------------------------------------------------------------------
namespace
{
    constexpr uint16_t STANDARD_SWIZZLE_MASK_8   = 0b1010101000001111;
    constexpr uint16_t STANDARD_SWIZZLE_MASK_16  = 0b1010101010001111;
    constexpr uint16_t STANDARD_SWIZZLE_MASK_32  = 0b1010101010001111;
    constexpr uint16_t STANDARD_SWIZZLE_MASK_64  = 0b1010101011001111;
    constexpr uint16_t STANDARD_SWIZZLE_MASK_128 = 0b1010101011001111;

    //---------------------------------------------------------------------------------
    // row-major to z-order curve
    //---------------------------------------------------------------------------------
    template<int xBytesMask, size_t bytesPerPixel>
    HRESULT LinearToStandardSwizzle2D(
        const Image& srcImage,
        const Image& destImage,
        bool isCompressed) noexcept
    {
        assert((srcImage.format == destImage.format) || (srcImage.width == destImage.width) || (srcImage.height == destImage.height));

        const uint8_t* sptr = srcImage.pixels;
        if (!sptr)
            return E_POINTER;

        uint8_t* dptr = destImage.pixels;
        if (!dptr)
            return E_POINTER;

        if ((srcImage.rowPitch > UINT32_MAX) || (destImage.rowPitch > UINT32_MAX))
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        const size_t height = isCompressed ? (srcImage.height + 3) / 4 : srcImage.height;
        const size_t width  = isCompressed ? (srcImage.width  + 3) / 4 : srcImage.width;

        const size_t maxOffset = height * width * bytesPerPixel;
        const size_t tail = destImage.rowPitch * destImage.height;
        if (maxOffset > tail)
            return E_UNEXPECTED;

        const size_t rowPitch = srcImage.rowPitch;
        const uint8_t* endPtr = sptr + (rowPitch * height);
        for (size_t y = 0; y < height; ++y)
        {
            if (sptr >= endPtr)
                return E_FAIL;

            const uint8_t* sourcePixelPointer = sptr;
            for (size_t x = 0; x < width; ++x)
            {
                const uint32_t swizzleIndex = deposit_bits(static_cast<uint32_t>(x), xBytesMask) + deposit_bits(static_cast<uint32_t>(y), ~xBytesMask);
                const size_t swizzleOffset = swizzleIndex * bytesPerPixel;
                if (swizzleOffset >= maxOffset)
                    return E_UNEXPECTED;

                uint8_t* destPixelPointer = dptr + swizzleOffset;
                memcpy(destPixelPointer, sourcePixelPointer, bytesPerPixel);

                sourcePixelPointer += bytesPerPixel;
            }

            sptr += rowPitch;
        }

        if (tail > maxOffset)
        {
            if (isCompressed)
            {
                // TODO: Pad with copy of last block
            }
            else
            {
                // TODO: zero out tail space
            }
        }

        return S_OK;
    }

    //---------------------------------------------------------------------------------
    // z-order curve to row-major
    //---------------------------------------------------------------------------------
    template<int xBytesMask, size_t bytesPerPixel>
    HRESULT StandardSwizzleToLinear2D(
        const Image& srcImage,
        const Image& destImage,
        bool isCompressed) noexcept
    {
        assert((srcImage.format == destImage.format) || (srcImage.width == destImage.width) || (srcImage.height == destImage.height));

        const uint8_t* sptr = srcImage.pixels;
        if (!sptr)
            return E_POINTER;

        uint8_t* dptr = destImage.pixels;
        if (!dptr)
            return E_POINTER;

        if ((srcImage.rowPitch > UINT32_MAX) || (destImage.rowPitch > UINT32_MAX))
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        const size_t height = isCompressed ? (srcImage.height + 3) / 4 : srcImage.height;
        const size_t width  = isCompressed ? (srcImage.width  + 3) / 4 : srcImage.width;

        const size_t maxOffset = height * width * bytesPerPixel;
        const size_t rowPitch = destImage.rowPitch;

        const uint64_t totalPixels = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
        if (totalPixels > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        const uint64_t totalDataSize = totalPixels * static_cast<uint64_t>(bytesPerPixel);
        if (totalDataSize > MAX_TEXTURE_SIZE)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        const uint8_t* endPtr = sptr + static_cast<ptrdiff_t>(totalDataSize);
        for (size_t swizzleIndex = 0; swizzleIndex < static_cast<size_t>(totalPixels); ++swizzleIndex)
        {
            if (sptr >= endPtr)
                return E_FAIL;

            uint32_t destX = extract_bits(static_cast<uint32_t>(swizzleIndex), xBytesMask);
            uint32_t destY = extract_bits(static_cast<uint32_t>(swizzleIndex), ~xBytesMask);

            size_t rowMajorOffset = destY * rowPitch + destX * bytesPerPixel;
            if (rowMajorOffset >= maxOffset)
                return E_UNEXPECTED;

            uint8_t* destPixelPointer = dptr + rowMajorOffset;
            memcpy(destPixelPointer, sptr, bytesPerPixel);
            sptr += bytesPerPixel;
        }

        return S_OK;
    }
}

_Use_decl_annotations_
HRESULT DirectX::StandardSwizzle(
    const Image& srcImage,
    bool toSwizzle,
    ScratchImage& result) noexcept
{
    if (srcImage.height == 1
        || (srcImage.width > MAX_TEXTURE_DIMENSION) || (srcImage.height > MAX_TEXTURE_DIMENSION))
    {
        // Standard Swizzle is not defined for 1D textures or textures larger than 16k
        return E_INVALIDARG;
    }

    if (IsPlanar(srcImage.format) || IsPalettized(srcImage.format) || (srcImage.format == DXGI_FORMAT_R1_UNORM))
        return HRESULT_E_NOT_SUPPORTED;

    HRESULT hr = result.Initialize2D(srcImage.format, srcImage.width, srcImage.height, 1, 1);
    if (FAILED(hr))
        return hr;

    const bool isCompressed = IsCompressed(srcImage.format);
    const size_t bytesPerPixel = isCompressed ? BytesPerBlock(srcImage.format) : (BitsPerPixel(srcImage.format) / 8);
    if (!bytesPerPixel)
    {
        result.Release();
        return E_FAIL;
    }

    const auto dstImage = result.GetImage(0, 0, 0);
    if (!dstImage)
    {
        result.Release();
        return E_POINTER;
    }

    if (toSwizzle)
    {
        switch(bytesPerPixel)
        {
        case 1:
            hr = LinearToStandardSwizzle2D<STANDARD_SWIZZLE_MASK_8, 1>(srcImage, *dstImage, false);
            break;
        case 2:
            hr = LinearToStandardSwizzle2D<STANDARD_SWIZZLE_MASK_16, 2>(srcImage, *dstImage, false);
            break;
        case 8:
            hr = LinearToStandardSwizzle2D<STANDARD_SWIZZLE_MASK_64, 8>(srcImage, *dstImage, isCompressed);
            break;
        case 16:
            hr = LinearToStandardSwizzle2D<STANDARD_SWIZZLE_MASK_128, 16>(srcImage, *dstImage, isCompressed);
            break;
        default:
            hr = LinearToStandardSwizzle2D<STANDARD_SWIZZLE_MASK_32, 4>(srcImage, *dstImage, false);
            break;
        }
    }
    else
    {
        switch(bytesPerPixel)
        {
        case 1:
            hr = StandardSwizzleToLinear2D<STANDARD_SWIZZLE_MASK_8, 1>(srcImage, *dstImage, false);
            break;
        case 2:
            hr = StandardSwizzleToLinear2D<STANDARD_SWIZZLE_MASK_16, 2>(srcImage, *dstImage, false);
            break;
        case 8:
            hr = StandardSwizzleToLinear2D<STANDARD_SWIZZLE_MASK_64, 8>(srcImage, *dstImage, isCompressed);
            break;
        case 16:
            hr = StandardSwizzleToLinear2D<STANDARD_SWIZZLE_MASK_128, 16>(srcImage, *dstImage, isCompressed);
            break;
        default:
            hr = StandardSwizzleToLinear2D<STANDARD_SWIZZLE_MASK_32, 4>(srcImage, *dstImage, false);
            break;
        }
    }

    if (FAILED(hr))
    {
        result.Release();
        return hr;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT DirectX::StandardSwizzle(
    const Image* srcImages,
    size_t nimages,
    const TexMetadata& metadata,
    bool toSwizzle,
    ScratchImage& result) noexcept
{
    if (!srcImages || !nimages
        || (metadata.dimension != TEX_DIMENSION_TEXTURE2D)
        || (metadata.width > MAX_TEXTURE_DIMENSION) || (metadata.height > MAX_TEXTURE_DIMENSION))
        return E_INVALIDARG;

    if (IsPlanar(metadata.format) || IsPalettized(metadata.format) || (metadata.format == DXGI_FORMAT_R1_UNORM))
        return HRESULT_E_NOT_SUPPORTED;

    HRESULT hr = result.Initialize(metadata);
    if (FAILED(hr))
        return hr;

    if (nimages != result.GetImageCount())
    {
        result.Release();
        return E_FAIL;
    }

    const bool isCompressed = IsCompressed(metadata.format);
    const size_t bytesPerPixel = isCompressed ? BytesPerBlock(metadata.format) : (BitsPerPixel(metadata.format) / 8);
    if (!bytesPerPixel)
    {
        result.Release();
        return E_FAIL;
    }

    const Image* dest = result.GetImages();
    if (!dest)
    {
        result.Release();
        return E_POINTER;
    }

    for (size_t index = 0; index < nimages; ++index)
    {
        const Image& src = srcImages[index];
        if (src.format != metadata.format)
        {
            result.Release();
            return E_FAIL;
        }

        if ((src.width > MAX_TEXTURE_DIMENSION) || (src.height > MAX_TEXTURE_DIMENSION))
            return E_FAIL;

        const Image& dst = dest[index];
        assert(dst.format == metadata.format);

        if (src.width != dst.width || src.height != dst.height)
        {
            result.Release();
            return E_FAIL;
        }

        if (toSwizzle)
        {
            switch(bytesPerPixel)
            {
            case 1:
                hr = LinearToStandardSwizzle2D<STANDARD_SWIZZLE_MASK_8, 1>(src, dst, false);
                break;
            case 2:
                hr = LinearToStandardSwizzle2D<STANDARD_SWIZZLE_MASK_16, 2>(src, dst, false);
                break;
            case 8:
                hr = LinearToStandardSwizzle2D<STANDARD_SWIZZLE_MASK_64, 8>(src, dst, isCompressed);
                break;
            case 16:
                hr = LinearToStandardSwizzle2D<STANDARD_SWIZZLE_MASK_128, 16>(src, dst, isCompressed);
                break;
            default:
                hr = LinearToStandardSwizzle2D<STANDARD_SWIZZLE_MASK_32, 4>(src, dst, false);
                break;
            }
        }
        else
        {
            switch(bytesPerPixel)
            {
            case 1:
                hr = StandardSwizzleToLinear2D<STANDARD_SWIZZLE_MASK_8, 1>(src, dst, false);
                break;
            case 2:
                hr = StandardSwizzleToLinear2D<STANDARD_SWIZZLE_MASK_16, 2>(src, dst, false);
                break;
            case 8:
                hr = StandardSwizzleToLinear2D<STANDARD_SWIZZLE_MASK_64, 8>(src, dst, isCompressed);
                break;
            case 16:
                hr = StandardSwizzleToLinear2D<STANDARD_SWIZZLE_MASK_128, 16>(src, dst, isCompressed);
                break;
            default:
                hr = StandardSwizzleToLinear2D<STANDARD_SWIZZLE_MASK_32, 4>(src, dst, false);
                break;
            }
        }

        if (FAILED(hr))
        {
            result.Release();
            return hr;
        }
    }

    return S_OK;
}


//-------------------------------------------------------------------------------------
// 3D z-order curve
//-------------------------------------------------------------------------------------
namespace
{
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_X_8   = 0b1001000000001111;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_X_16  = 0b1001000000001111;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_X_32  = 0b1001001000001111;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_X_64  = 0b1001001100001111;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_X_128 = 0b1001001100001111;

    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_Y_8   = 0b0100101000110000;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_Y_16  = 0b0100101000110001;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_Y_32  = 0b0100100100110011;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_Y_64  = 0b0100100000110111;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_Y_128 = 0b0100100000111111;

    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_Z_8   = 0b0010010111000000;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_Z_16  = 0b0010010111000001;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_Z_32  = 0b0010010011000011;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_Z_64  = 0b0010010011000111;
    constexpr uint16_t VOLUME_STANDARD_SWIZZLE_Z_128 = 0b0010010011001111;

    inline int GetSwizzleMask3D_X(size_t bytesPerPixel) noexcept
    {
        switch(bytesPerPixel)
        {
        case 1: return VOLUME_STANDARD_SWIZZLE_X_8;
        case 2: return VOLUME_STANDARD_SWIZZLE_X_16;
        case 8: return VOLUME_STANDARD_SWIZZLE_X_64;
        case 16: return VOLUME_STANDARD_SWIZZLE_X_128;
        default: return VOLUME_STANDARD_SWIZZLE_X_32;
        }
    }

    inline int GetSwizzleMask3D_Y(size_t bytesPerPixel) noexcept
    {
        switch(bytesPerPixel)
        {
        case 1: return VOLUME_STANDARD_SWIZZLE_Y_8;
        case 2: return VOLUME_STANDARD_SWIZZLE_Y_16;
        case 8: return VOLUME_STANDARD_SWIZZLE_Y_64;
        case 16: return VOLUME_STANDARD_SWIZZLE_Y_128;
        default: return VOLUME_STANDARD_SWIZZLE_Y_32;
        }
    }

    inline int GetSwizzleMask3D_Z(size_t bytesPerPixel) noexcept
    {
        switch(bytesPerPixel)
        {
        case 1: return VOLUME_STANDARD_SWIZZLE_Z_8;
        case 2: return VOLUME_STANDARD_SWIZZLE_Z_16;
        case 8: return VOLUME_STANDARD_SWIZZLE_Z_64;
        case 16: return VOLUME_STANDARD_SWIZZLE_Z_128;
        default: return VOLUME_STANDARD_SWIZZLE_Z_32;
        }
    }
}

_Use_decl_annotations_
HRESULT DirectX::StandardSwizzle3D(
    const Image* srcImages,
    size_t depth,
    const TexMetadata& metadata,
    bool toSwizzle,
    ScratchImage& result) noexcept
{
    if (!srcImages || !depth || (metadata.dimension != TEX_DIMENSION_TEXTURE3D))
        return E_INVALIDARG;

    if (IsPlanar(metadata.format) || IsPalettized(metadata.format) || (metadata.format == DXGI_FORMAT_R1_UNORM))
        return HRESULT_E_NOT_SUPPORTED;

    HRESULT hr = result.Initialize(metadata);
    if (FAILED(hr))
        return hr;

    if ((depth * metadata.mipLevels) != result.GetImageCount())
    {
        result.Release();
        return E_FAIL;
    }

    const bool isCompressed = IsCompressed(metadata.format);
    const size_t bytesPerPixel = isCompressed ? BytesPerBlock(metadata.format) : (BitsPerPixel(metadata.format) / 8);
    if (!bytesPerPixel)
    {
        result.Release();
        return E_FAIL;
    }

    const int xBytesMask = GetSwizzleMask3D_X(metadata.format);
    const int yBytesMask = GetSwizzleMask3D_Y(metadata.format);
    const int zBytesMask = GetSwizzleMask3D_Z(metadata.format);

    if (toSwizzle)
    {
        // row-major to z-order curve
        const Image* destImages = result.GetImages();
        for (size_t z = 0; z < depth; z++)
        {
            size_t rowPitch = srcImages[z].rowPitch;
            const uint8_t* sptr = srcImages[z].pixels;
            if (!sptr)
                return E_POINTER;
            for (size_t y = 0; y < height; y++)
            {
                for (size_t x = 0; x < width; x++)
                {
                    uint32_t swizzle3Dindex = deposit_bits(static_cast<uint32_t>(x), xBytesMask) + deposit_bits(static_cast<uint32_t>(y), yBytesMask) + deposit_bits(static_cast<uint32_t>(z), zBytesMask);
                    uint32_t swizzle2Dindex = swizzle3Dindex % (metadata.width * metadata.height);
                    uint32_t swizzleSlice   = swizzle3Dindex / (metadata.width * metadata.height);
                    size_t swizzleOffset = swizzle2Dindex * bytesPerPixel;

                    size_t rowMajorOffset = y * rowPitch + x * bytesPerPixel;

                    uint8_t* dptr = destImages[swizzleSlice].pixels;
                    if (!dptr)
                        return E_POINTER;

                    const uint8_t* sourcePixelPointer = sptr + rowMajorOffset;
                    uint8_t* destPixelPointer = dptr + swizzleOffset;
                    memcpy(destPixelPointer, sourcePixelPointer, bytesPerPixel);
                }
            }
        }
    }
    else
    {
        // z-order curve to row-major
        const Image* destImages = result.GetImages();
        for (size_t z = 0; z < depth; z++)
        {
            const uint8_t* sptr = srcImages[z].pixels;
            if (!sptr)
                return E_POINTER;

            for (size_t swizzleIndex = 0; swizzleIndex < (width * height); swizzleIndex++)
            {
                size_t swizzleOffset = swizzleIndex * bytesPerPixel;
                const uint8_t* sourcePixelPointer = sptr + swizzleOffset;

                size_t index3D = z * width * height + swizzleIndex;
                uint32_t destX = extract_bits(index3D, xBytesMask);
                uint32_t destY = extract_bits(index3D, yBytesMask);
                uint32_t destZ = extract_bits(index3D, zBytesMask);
                size_t rowPitch = destImages[z].rowPitch;
                size_t rowMajorOffset = destY * rowPitch + destX * bytesPerPixel;

                uint8_t* dptr = destImages[destZ].pixels;
                if (!dptr)
                    return E_POINTER;
                uint8_t* destPixelPointer = dptr + rowMajorOffset;

                memcpy(destPixelPointer, sourcePixelPointer, bytesPerPixel);
            }
        }
    }

    return S_OK;
}
