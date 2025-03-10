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


namespace
{
#ifdef __AVX2__
#define deposit_bits(v,m) _pdep_u32(v,m)
#define extract_bits(v,m) _pext_u32(v,m)
#else
    // For ARM64 we could use SVE2-BITPERM intrinsics BDEP/BEXT if supported by the platform/compiler.

    // N3864 - A constexpr bitwise operations library for C++
    // https://github.com/fmatthew5876/stdcxx-bitops
    uint32_t deposit_bits(uint32_t val, int mask) noexcept
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

    uint32_t extract_bits(uint32_t val, int mask) noexcept
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

#if defined(_M_X64) || defined(_M_ARM64) || __x86_64__ || __aarch64__
    constexpr uint64_t MAX_TEXTURE_SIZE = UINT64_C(16384) * UINT64_C(16384) * 16u;
#else
    constexpr uint64_t MAX_TEXTURE_SIZE = UINT32_MAX;
#endif

    // Standard Swizzle is not defined for these formats.
    bool IsExcludedFormat(DXGI_FORMAT fmt) noexcept
    {
        switch(static_cast<int>(fmt))
        {
        // 96bpp
        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:

        // Depth/stencil
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_D16_UNORM:
        case XBOX_DXGI_FORMAT_D16_UNORM_S8_UINT:
        case XBOX_DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
        case XBOX_DXGI_FORMAT_X16_TYPELESS_G8_UINT:

        // Monochrome
        case DXGI_FORMAT_R1_UNORM:

        // Packed
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_YUY2:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:

        // Planar
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_420_OPAQUE:
        case DXGI_FORMAT_NV11:
        case WIN10_DXGI_FORMAT_P208:
        case WIN10_DXGI_FORMAT_V208:
        case WIN10_DXGI_FORMAT_V408:

        // Palettized
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
        case DXGI_FORMAT_A8P8:
            return true;

        default:
            return false;
        }
    }

//-------------------------------------------------------------------------------------
// 2D z-order curve
//-------------------------------------------------------------------------------------
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

        if (srcImage.rowPitch > UINT32_MAX)
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

        if ((tail > maxOffset) && isCompressed)
        {
            // TODO: Pad with copy of last block instead of all zeroes
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

        if (srcImage.rowPitch > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        const size_t height = isCompressed ? (srcImage.height + 3) / 4 : srcImage.height;
        const size_t width  = isCompressed ? (srcImage.width  + 3) / 4 : srcImage.width;

        const size_t rowPitch = destImage.rowPitch;

        const uint64_t totalPixels = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
        if (totalPixels > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        const uint64_t totalDataSize = totalPixels * static_cast<uint64_t>(bytesPerPixel);
        if (totalDataSize > MAX_TEXTURE_SIZE)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        const size_t maxOffset = static_cast<size_t>(totalDataSize);
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


//-------------------------------------------------------------------------------------
// 3D z-order curve
//-------------------------------------------------------------------------------------
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

    //---------------------------------------------------------------------------------
    // row-major to z-order curve
    //---------------------------------------------------------------------------------
    template<int xBytesMask, int yBytesMask, int zBytesMask, size_t bytesPerPixel>
    HRESULT LinearToStandardSwizzle3D(
        _In_reads_(depth) const Image* srcImages,
        const Image& destImage,
        size_t depth,
        bool isCompressed) noexcept
    {
        if (!srcImages || !depth)
            return E_INVALIDARG;

        if (depth > UINT16_MAX)
            return E_INVALIDARG;

        // We rely on the fact that ScratchImage will put all slices in the same mip level
        // in continous memory. We do not assume that is true of the source images.
        uint8_t* dptr = destImage.pixels;
        if (!dptr)
            return E_POINTER;

        if (srcImages[0].rowPitch > UINT32_MAX
            || srcImages[0].slicePitch > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        const size_t height = isCompressed ? (srcImages[0].height + 3) / 4 : srcImages[0].height;
        const size_t width  = isCompressed ? (srcImages[0].width  + 3) / 4 : srcImages[0].width;

        const size_t maxOffset = height * width * depth * bytesPerPixel;
        const size_t tail = destImage.slicePitch * depth;
        if (maxOffset > tail)
            return E_UNEXPECTED;

        for(size_t z = 0; z < depth; ++z)
        {
            const uint8_t* sptr = srcImages[z].pixels;
            if (!sptr)
                return E_POINTER;

            const size_t rowPitch = srcImages[z].rowPitch;
            const uint8_t* endPtr = sptr + srcImages[z].slicePitch;
            for (size_t y = 0; y < height; ++y)
            {
                if (sptr >= endPtr)
                    return E_FAIL;

                const uint8_t* sourcePixelPointer = sptr;
                for (size_t x = 0; x < width; ++x)
                {
                    const uint32_t swizzleIndex = deposit_bits(static_cast<uint32_t>(x), xBytesMask)
                        + deposit_bits(static_cast<uint32_t>(y), yBytesMask)
                        + deposit_bits(static_cast<uint32_t>(z), zBytesMask);
                    const size_t swizzleOffset = swizzleIndex * bytesPerPixel;
                    if (swizzleOffset >= maxOffset)
                        return E_UNEXPECTED;

                    uint8_t* destPixelPointer = dptr + swizzleOffset;
                    memcpy(destPixelPointer, sourcePixelPointer, bytesPerPixel);

                    sourcePixelPointer += bytesPerPixel;
                }

                sptr += rowPitch;
            }
        }

        if ((tail > maxOffset) && isCompressed)
        {
            // TODO: Pad with copy of last block instead of all zeroes
        }

        return S_OK;
    }

    //---------------------------------------------------------------------------------
    // z-order curve to row-major
    //---------------------------------------------------------------------------------
    template<int xBytesMask, int yBytesMask, int zBytesMask, size_t bytesPerPixel>
    HRESULT StandardSwizzleToLinear3D(
        _In_reads_(depth) const Image* srcImages,
        const Image& destImage,
        size_t depth,
        bool isCompressed) noexcept
    {
        if (!srcImages || !depth)
            return E_INVALIDARG;

        if (depth > UINT16_MAX)
            return E_INVALIDARG;

        // We rely on the fact that ScratchImage will put all slices in the same mip level
        // in continous memory. We do not assume that is true of the source images.
        uint8_t* dptr = destImage.pixels;
        if (!dptr)
            return E_POINTER;

        if (srcImages[0].rowPitch > UINT32_MAX
            || srcImages[0].slicePitch > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        const size_t height = isCompressed ? (srcImages[0].height + 3) / 4 : srcImages[0].height;
        const size_t width  = isCompressed ? (srcImages[0].width  + 3) / 4 : srcImages[0].width;

        const uint64_t totalPixels = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
        if (totalPixels > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        const size_t maxOffset = height * width * depth * bytesPerPixel;

        const size_t rowPitch = destImage.rowPitch;
        const size_t slicePitch = destImage.slicePitch;

        size_t swizzleIndex = 0;
        for(size_t z = 0; z < depth; ++z)
        {
            const uint8_t* sptr = srcImages[z].pixels;
            if (!sptr)
                return E_POINTER;

            const uint8_t* endPtr = sptr + srcImages[z].slicePitch;

            for(size_t j = 0; j < totalPixels; ++j, ++swizzleIndex)
            {
                if (sptr >= endPtr)
                    return E_FAIL;

                uint32_t destX = extract_bits(static_cast<uint32_t>(swizzleIndex), xBytesMask);
                uint32_t destY = extract_bits(static_cast<uint32_t>(swizzleIndex), yBytesMask);
                uint32_t destZ = extract_bits(static_cast<uint32_t>(swizzleIndex), zBytesMask);

                size_t rowMajorOffset = destZ * slicePitch + destY * rowPitch + destX * bytesPerPixel;
                if (rowMajorOffset >= maxOffset)
                    return E_UNEXPECTED;

                uint8_t* destPixelPointer = dptr + rowMajorOffset;
                memcpy(destPixelPointer, sptr, bytesPerPixel);
                sptr += bytesPerPixel;
            }
        }

        return S_OK;
    }
}


//=====================================================================================
// Entry points
//=====================================================================================

_Use_decl_annotations_
HRESULT DirectX::StandardSwizzle(
    const Image& srcImage,
    bool toSwizzle,
    ScratchImage& result) noexcept
{
    if ((srcImage.height == 1)
        || (srcImage.width > MAX_TEXTURE_DIMENSION) || (srcImage.height > MAX_TEXTURE_DIMENSION))
    {
        // Standard Swizzle is not defined for 1D textures or textures larger than 16k
        return HRESULT_E_NOT_SUPPORTED;
    }

    if (IsExcludedFormat(srcImage.format))
        return HRESULT_E_NOT_SUPPORTED;

    if (!srcImage.pixels)
        return E_POINTER;

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
    if (!srcImages || !nimages)
        return E_INVALIDARG;

    if (((metadata.dimension != TEX_DIMENSION_TEXTURE2D) && (metadata.dimension != TEX_DIMENSION_TEXTURE3D))
        || (metadata.width > MAX_TEXTURE_DIMENSION) || (metadata.height > MAX_TEXTURE_DIMENSION))
    {
        // Standard Swizzle is not defined for 1D textures or textures larger than 16k
        return HRESULT_E_NOT_SUPPORTED;
    }

    if (IsExcludedFormat(metadata.format))
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

    if (metadata.dimension == TEX_DIMENSION_TEXTURE3D)
    {
        size_t index = 0;
        size_t depth = metadata.depth;
        for (size_t level = 0; level < metadata.mipLevels; ++level)
        {
            const Image* srcBase = &srcImages[index];
            const Image& destBase = dest[index];

            for(size_t slice = 0; slice < depth; ++slice, ++index)
            {
                // Validate source image array.
                if (index >= nimages)
                {
                    result.Release();
                    return E_UNEXPECTED;
                }

                const Image& src = srcImages[index];
                if (!src.pixels)
                {
                    result.Release();
                    return E_POINTER;
                }

                if (src.format != metadata.format)
                {
                    result.Release();
                    return E_FAIL;
                }

                if ((src.width > MAX_TEXTURE_DIMENSION) || (src.height > MAX_TEXTURE_DIMENSION))
                {
                    result.Release();
                    return E_FAIL;
                }

                const Image& dst = dest[index];
                assert(dst.format == metadata.format);

                if (src.width != dst.width || src.height != dst.height)
                {
                    result.Release();
                    return E_FAIL;
                }

                if (!src.rowPitch || !src.slicePitch)
                {
                    result.Release();
                    return E_FAIL;
                }

                assert(dst.rowPitch != 0 && dst.slicePitch != 0);

                uint64_t slicePitch = static_cast<uint64_t>(src.rowPitch) * static_cast<uint64_t>(src.height);
                if (static_cast<uint64_t>(slicePitch) > src.slicePitch)
                {
                    result.Release();
                    return E_FAIL;
                }
            }

            if (toSwizzle)
            {
                switch(bytesPerPixel)
                {
                case 1:
                    hr = LinearToStandardSwizzle3D<VOLUME_STANDARD_SWIZZLE_X_8, VOLUME_STANDARD_SWIZZLE_Y_8, VOLUME_STANDARD_SWIZZLE_Z_8, 1>(srcBase, destBase, depth, false);
                    break;
                case 2:
                    hr = LinearToStandardSwizzle3D<VOLUME_STANDARD_SWIZZLE_X_16, VOLUME_STANDARD_SWIZZLE_Y_16, VOLUME_STANDARD_SWIZZLE_Z_16, 2>(srcBase, destBase, depth, false);
                    break;
                case 8:
                    hr = LinearToStandardSwizzle3D<VOLUME_STANDARD_SWIZZLE_X_64, VOLUME_STANDARD_SWIZZLE_Y_64, VOLUME_STANDARD_SWIZZLE_Z_64, 8>(srcBase, destBase, depth, isCompressed);
                    break;
                case 16:
                    hr = LinearToStandardSwizzle3D<VOLUME_STANDARD_SWIZZLE_X_128, VOLUME_STANDARD_SWIZZLE_Y_128, VOLUME_STANDARD_SWIZZLE_Z_128, 16>(srcBase, destBase, depth, isCompressed);
                    break;
                default:
                    hr = LinearToStandardSwizzle3D<VOLUME_STANDARD_SWIZZLE_X_32, VOLUME_STANDARD_SWIZZLE_Y_32, VOLUME_STANDARD_SWIZZLE_Z_32, 4>(srcBase, destBase, depth, false);
                    break;
                }
            }
            else
            {
                switch(bytesPerPixel)
                {
                case 1:
                    hr = StandardSwizzleToLinear3D<VOLUME_STANDARD_SWIZZLE_X_8, VOLUME_STANDARD_SWIZZLE_Y_8, VOLUME_STANDARD_SWIZZLE_Z_8, 1>(srcBase, destBase, depth, false);
                    break;
                case 2:
                    hr = StandardSwizzleToLinear3D<VOLUME_STANDARD_SWIZZLE_X_16, VOLUME_STANDARD_SWIZZLE_Y_16, VOLUME_STANDARD_SWIZZLE_Z_16, 2>(srcBase, destBase, depth, false);
                    break;
                case 8:
                    hr = StandardSwizzleToLinear3D<VOLUME_STANDARD_SWIZZLE_X_64, VOLUME_STANDARD_SWIZZLE_Y_64, VOLUME_STANDARD_SWIZZLE_Z_64, 8>(srcBase, destBase, depth, isCompressed);
                    break;
                case 16:
                    hr = StandardSwizzleToLinear3D<VOLUME_STANDARD_SWIZZLE_X_128, VOLUME_STANDARD_SWIZZLE_Y_128, VOLUME_STANDARD_SWIZZLE_Z_128, 16>(srcBase, destBase, depth, isCompressed);
                    break;
                default:
                    hr = StandardSwizzleToLinear3D<VOLUME_STANDARD_SWIZZLE_X_32, VOLUME_STANDARD_SWIZZLE_Y_32, VOLUME_STANDARD_SWIZZLE_Z_32, 4>(srcBase, destBase, depth, false);
                    break;
                }
            }

            if (FAILED(hr))
            {
                result.Release();
                return hr;
            }

            if (depth > 1)
                depth >>= 1;
        }

        return S_OK;
    }
    else
    {
        // Handle the 2D case for TEX_DIMENSION_TEXTURE2D
        for (size_t index = 0; index < nimages; ++index)
        {
            const Image& src = srcImages[index];
            if (src.format != metadata.format)
            {
                result.Release();
                return E_FAIL;
            }

            if ((src.width > MAX_TEXTURE_DIMENSION) || (src.height > MAX_TEXTURE_DIMENSION))
            {
                result.Release();
                return E_FAIL;
            }

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
    }

    return S_OK;
}
