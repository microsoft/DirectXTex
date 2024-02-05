//-------------------------------------------------------------------------------------
// DirectXTexTGA.cpp
//
// DirectX Texture Library - Targa Truevision (TGA) file format reader/writer
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "DirectXTexP.h"

//
// The implementation here has the following limitations:
//      * Does not support files that contain color maps (these are rare in practice)
//      * Interleaved files are not supported (deprecated aspect of TGA format)
//      * Only supports 8-bit grayscale; 16-, 24-, and 32-bit truecolor images RLE or uncompressed
//        plus 24-bit color-mapped uncompressed images
//      * Always writes uncompressed files (i.e. can read RLE compression, but does not write it)
//

using namespace DirectX;
using namespace DirectX::Internal;

namespace
{
    constexpr float GAMMA_EPSILON = 0.01f;

    const char g_Signature[] = "TRUEVISION-XFILE.";
        // This is the official footer signature for the TGA 2.0 file format.

    enum TGAImageType
    {
        TGA_NO_IMAGE = 0,
        TGA_COLOR_MAPPED = 1,
        TGA_TRUECOLOR = 2,
        TGA_BLACK_AND_WHITE = 3,
        TGA_COLOR_MAPPED_RLE = 9,
        TGA_TRUECOLOR_RLE = 10,
        TGA_BLACK_AND_WHITE_RLE = 11,
    };

    enum TGADescriptorFlags
    {
        TGA_FLAGS_INVERTX = 0x10,
        TGA_FLAGS_INVERTY = 0x20,
        TGA_FLAGS_INTERLEAVED_2WAY = 0x40, // Deprecated
        TGA_FLAGS_INTERLEAVED_4WAY = 0x80, // Deprecated
    };

    enum TGAAttributesType : uint8_t
    {
        TGA_ATTRIBUTE_NONE = 0,             // 0: no alpha data included
        TGA_ATTRIBUTE_IGNORED = 1,          // 1: undefined data, can be ignored
        TGA_ATTRIBUTE_UNDEFINED = 2,        // 2: uedefined data, should be retained
        TGA_ATTRIBUTE_ALPHA = 3,            // 3: useful alpha channel data
        TGA_ATTRIBUTE_PREMULTIPLIED = 4,    // 4: pre-multiplied alpha
    };

#pragma pack(push,1)
    struct TGA_HEADER
    {
        uint8_t     bIDLength;
        uint8_t     bColorMapType;
        uint8_t     bImageType;
        uint16_t    wColorMapFirst;
        uint16_t    wColorMapLength;
        uint8_t     bColorMapSize;
        uint16_t    wXOrigin;
        uint16_t    wYOrigin;
        uint16_t    wWidth;
        uint16_t    wHeight;
        uint8_t     bBitsPerPixel;
        uint8_t     bDescriptor;
    };

    static_assert(sizeof(TGA_HEADER) == 18, "TGA 2.0 size mismatch");

    constexpr size_t TGA_HEADER_LEN = 18;

    struct TGA_FOOTER
    {
        uint32_t    dwExtensionOffset;
        uint32_t    dwDeveloperOffset;
        char        Signature[18];
    };

    static_assert(sizeof(TGA_FOOTER) == 26, "TGA 2.0 size mismatch");

    struct TGA_EXTENSION
    {
        uint16_t    wSize;
        char        szAuthorName[41];
        char        szAuthorComment[324];
        uint16_t    wStampMonth;
        uint16_t    wStampDay;
        uint16_t    wStampYear;
        uint16_t    wStampHour;
        uint16_t    wStampMinute;
        uint16_t    wStampSecond;
        char        szJobName[41];
        uint16_t    wJobHour;
        uint16_t    wJobMinute;
        uint16_t    wJobSecond;
        char        szSoftwareId[41];
        uint16_t    wVersionNumber;
        uint8_t     bVersionLetter;
        uint32_t    dwKeyColor;
        uint16_t    wPixelNumerator;
        uint16_t    wPixelDenominator;
        uint16_t    wGammaNumerator;
        uint16_t    wGammaDenominator;
        uint32_t    dwColorOffset;
        uint32_t    dwStampOffset;
        uint32_t    dwScanOffset;
        uint8_t     bAttributesType;
    };

    static_assert(sizeof(TGA_EXTENSION) == 495, "TGA 2.0 size mismatch");

#pragma pack(pop)

    enum CONVERSION_FLAGS
    {
        CONV_FLAGS_NONE = 0x0,
        CONV_FLAGS_EXPAND = 0x1,        // Conversion requires expanded pixel size
        CONV_FLAGS_INVERTX = 0x2,       // If set, scanlines are right-to-left
        CONV_FLAGS_INVERTY = 0x4,       // If set, scanlines are top-to-bottom
        CONV_FLAGS_RLE = 0x8,           // Source data is RLE compressed
        CONV_FLAGS_PALETTED = 0x10,     // Source data is paletted

        CONV_FLAGS_SWIZZLE = 0x10000,   // Swizzle BGR<->RGB data
        CONV_FLAGS_888 = 0x20000,       // 24bpp format
    };


    //-------------------------------------------------------------------------------------
    // Decodes TGA header
    //-------------------------------------------------------------------------------------
    HRESULT DecodeTGAHeader(
        _In_reads_bytes_(size) const void* pSource,
        size_t size,
        TGA_FLAGS flags,
        _Out_ TexMetadata& metadata,
        size_t& offset,
        _Inout_opt_ uint32_t* convFlags) noexcept
    {
        if (!pSource)
            return E_INVALIDARG;

        memset(&metadata, 0, sizeof(TexMetadata));

        if (size < TGA_HEADER_LEN)
        {
            return HRESULT_E_INVALID_DATA;
        }

        auto pHeader = static_cast<const TGA_HEADER*>(pSource);

        if (pHeader->bDescriptor & (TGA_FLAGS_INTERLEAVED_2WAY | TGA_FLAGS_INTERLEAVED_4WAY))
        {
            return HRESULT_E_NOT_SUPPORTED;
        }

        if (!pHeader->wWidth || !pHeader->wHeight)
        {
            // These are uint16 values so are already bounded by UINT16_MAX.
            return HRESULT_E_INVALID_DATA;
        }

        switch (pHeader->bImageType)
        {
        case TGA_NO_IMAGE:
        case TGA_COLOR_MAPPED_RLE:
            return HRESULT_E_NOT_SUPPORTED;

        case TGA_COLOR_MAPPED:
            if (pHeader->bColorMapType != 1
                || pHeader->wColorMapLength == 0
                || pHeader->bBitsPerPixel != 8)
            {
                return HRESULT_E_NOT_SUPPORTED;
            }

            switch (pHeader->bColorMapSize)
            {
            case 24:
                if (flags & TGA_FLAGS_BGR)
                {
                    metadata.format = DXGI_FORMAT_B8G8R8X8_UNORM;
                }
                else
                {
                    metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    metadata.SetAlphaMode(TEX_ALPHA_MODE_OPAQUE);
                }
                break;

            // Other possible values are 15, 16, and 32 which we do not support.

            default:
                return HRESULT_E_NOT_SUPPORTED;
            }

            if (convFlags)
            {
                *convFlags |= CONV_FLAGS_PALETTED;
            }
            break;

        case TGA_TRUECOLOR:
        case TGA_TRUECOLOR_RLE:
            if (pHeader->bColorMapType != 0
                || pHeader->wColorMapLength != 0)
            {
                return HRESULT_E_NOT_SUPPORTED;
            }

            switch (pHeader->bBitsPerPixel)
            {
            case 16:
                metadata.format = DXGI_FORMAT_B5G5R5A1_UNORM;
                break;

            case 24:
                if (flags & TGA_FLAGS_BGR)
                {
                    metadata.format = DXGI_FORMAT_B8G8R8X8_UNORM;
                }
                else
                {
                    metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    metadata.SetAlphaMode(TEX_ALPHA_MODE_OPAQUE);
                }

                if (convFlags)
                    *convFlags |= CONV_FLAGS_EXPAND;
                break;

            case 32:
                metadata.format = (flags & TGA_FLAGS_BGR) ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
                break;

            default:
                return HRESULT_E_NOT_SUPPORTED;
            }

            if (convFlags && (pHeader->bImageType == TGA_TRUECOLOR_RLE))
            {
                *convFlags |= CONV_FLAGS_RLE;
            }
            break;

        case TGA_BLACK_AND_WHITE:
        case TGA_BLACK_AND_WHITE_RLE:
            if (pHeader->bColorMapType != 0
                || pHeader->wColorMapLength != 0)
            {
                return HRESULT_E_NOT_SUPPORTED;
            }

            switch (pHeader->bBitsPerPixel)
            {
            case 8:
                metadata.format = DXGI_FORMAT_R8_UNORM;
                break;

            default:
                return HRESULT_E_NOT_SUPPORTED;
            }

            if (convFlags && (pHeader->bImageType == TGA_BLACK_AND_WHITE_RLE))
            {
                *convFlags |= CONV_FLAGS_RLE;
            }
            break;

        default:
            return HRESULT_E_INVALID_DATA;
        }

        uint64_t sizeBytes = uint64_t(pHeader->wWidth) * uint64_t(pHeader->wHeight) * uint64_t(pHeader->bBitsPerPixel) / 8;
        if (sizeBytes > UINT32_MAX)
        {
            return HRESULT_E_ARITHMETIC_OVERFLOW;
        }

        metadata.width = pHeader->wWidth;
        metadata.height = pHeader->wHeight;
        metadata.depth = metadata.arraySize = metadata.mipLevels = 1;
        metadata.dimension = TEX_DIMENSION_TEXTURE2D;

        if (convFlags)
        {
            if (pHeader->bDescriptor & TGA_FLAGS_INVERTX)
                *convFlags |= CONV_FLAGS_INVERTX;

            if (pHeader->bDescriptor & TGA_FLAGS_INVERTY)
                *convFlags |= CONV_FLAGS_INVERTY;
        }

        offset = TGA_HEADER_LEN;

        if (pHeader->bIDLength != 0)
        {
            offset += pHeader->bIDLength;
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Reads palette for color-mapped TGA formats
    //-------------------------------------------------------------------------------------
    HRESULT ReadPalette(
        _In_reads_(TGA_HEADER_LEN) const uint8_t* header,
        _In_reads_bytes_(size) const void* pSource,
        size_t size,
        TGA_FLAGS flags,
        uint8_t palette[256 * 4],
        size_t& colorMapSize) noexcept
    {
        assert(header && pSource);

        auto pHeader = reinterpret_cast<const TGA_HEADER*>(header);

        if (pHeader->bColorMapType != 1
            || pHeader->wColorMapLength == 0
            || pHeader->wColorMapLength > 256
            || pHeader->bColorMapSize != 24)
        {
            return HRESULT_E_NOT_SUPPORTED;
        }

        const size_t maxColorMap = size_t(pHeader->wColorMapFirst) + size_t(pHeader->wColorMapLength);
        if (maxColorMap > 256)
        {
            return HRESULT_E_NOT_SUPPORTED;
        }

        colorMapSize = size_t(pHeader->wColorMapLength) * ((size_t(pHeader->bColorMapSize) + 7) >> 3);
        if (colorMapSize > size)
        {
            return HRESULT_E_INVALID_DATA;
        }

        auto bytes = reinterpret_cast<const uint8_t*>(pSource);

        for (size_t i = pHeader->wColorMapFirst; i < maxColorMap; ++i)
        {
            if (flags & TGA_FLAGS_BGR)
            {
                palette[i * 4 + 0] = bytes[0];
                palette[i * 4 + 2] = bytes[2];
            }
            else
            {
                palette[i * 4 + 0] = bytes[2];
                palette[i * 4 + 2] = bytes[0];
            }
            palette[i * 4 + 1] = bytes[1];
            palette[i * 4 + 3] = 255;
            bytes += 3;
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Set alpha for images with all 0 alpha channel
    //-------------------------------------------------------------------------------------
    HRESULT SetAlphaChannelToOpaque(_In_ const Image* image) noexcept
    {
        assert(image);

        uint8_t* pPixels = image->pixels;
        if (!pPixels)
            return E_POINTER;

        for (size_t y = 0; y < image->height; ++y)
        {
            CopyScanline(pPixels, image->rowPitch, pPixels, image->rowPitch, image->format, TEXP_SCANLINE_SETALPHA);
            pPixels += image->rowPitch;
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Uncompress pixel data from a TGA into the target image
    //-------------------------------------------------------------------------------------
    HRESULT UncompressPixels(
        _In_reads_bytes_(size) const void* pSource,
        size_t size,
        TGA_FLAGS flags,
        _In_ const Image* image,
        _In_ uint32_t convFlags) noexcept
    {
        assert(pSource && size > 0);

        if (!image || !image->pixels)
            return E_POINTER;

        // Compute TGA image data pitch
        size_t rowPitch, slicePitch;
        HRESULT hr = ComputePitch(image->format, image->width, image->height, rowPitch, slicePitch,
            (convFlags & CONV_FLAGS_EXPAND) ? CP_FLAGS_24BPP : CP_FLAGS_NONE);
        if (FAILED(hr))
            return hr;

        auto sPtr = static_cast<const uint8_t*>(pSource);
        const uint8_t* endPtr = sPtr + size;

        bool opaquealpha = false;

        switch (image->format)
        {
        //--------------------------------------------------------------------------- 8-bit
        case DXGI_FORMAT_R8_UNORM:
            for (size_t y = 0; y < image->height; ++y)
            {
                size_t offset = ((convFlags & CONV_FLAGS_INVERTX) ? (image->width - 1) : 0);
                assert(offset < rowPitch);

                uint8_t* dPtr = image->pixels
                    + (image->rowPitch * ((convFlags & CONV_FLAGS_INVERTY) ? y : (image->height - y - 1)))
                    + offset;

                for (size_t x = 0; x < image->width; )
                {
                    if (sPtr >= endPtr)
                        return E_FAIL;

                    if (*sPtr & 0x80)
                    {
                        // Repeat
                        size_t j = size_t(*sPtr & 0x7F) + 1;
                        if (++sPtr >= endPtr)
                            return E_FAIL;

                        for (; j > 0; --j, ++x)
                        {
                            if (x >= image->width)
                                return E_FAIL;

                            *dPtr = *sPtr;

                            if (convFlags & CONV_FLAGS_INVERTX)
                                --dPtr;
                            else
                                ++dPtr;
                        }

                        ++sPtr;
                    }
                    else
                    {
                        // Literal
                        size_t j = size_t(*sPtr & 0x7F) + 1;
                        ++sPtr;

                        if (sPtr + j > endPtr)
                            return E_FAIL;

                        for (; j > 0; --j, ++x)
                        {
                            if (x >= image->width)
                                return E_FAIL;

                            *dPtr = *(sPtr++);

                            if (convFlags & CONV_FLAGS_INVERTX)
                                --dPtr;
                            else
                                ++dPtr;
                        }
                    }
                }
            }
            break;

        //-------------------------------------------------------------------------- 16-bit
        case DXGI_FORMAT_B5G5R5A1_UNORM:
            {
                uint32_t minalpha = 255;
                uint32_t maxalpha = 0;

                for (size_t y = 0; y < image->height; ++y)
                {
                    size_t offset = ((convFlags & CONV_FLAGS_INVERTX) ? (image->width - 1) : 0);
                    assert(offset * 2 < rowPitch);

                    auto dPtr = reinterpret_cast<uint16_t*>(image->pixels
                        + (image->rowPitch * ((convFlags & CONV_FLAGS_INVERTY) ? y : (image->height - y - 1))))
                        + offset;

                    for (size_t x = 0; x < image->width; )
                    {
                        if (sPtr >= endPtr)
                            return E_FAIL;

                        if (*sPtr & 0x80)
                        {
                            // Repeat
                            size_t j = size_t(*sPtr & 0x7F) + 1;
                            ++sPtr;

                            if (sPtr + 1 >= endPtr)
                                return E_FAIL;

                            auto t = static_cast<uint16_t>(uint32_t(*sPtr) | uint32_t(*(sPtr + 1u) << 8));

                            const uint32_t alpha = (t & 0x8000) ? 255 : 0;
                            minalpha = std::min(minalpha, alpha);
                            maxalpha = std::max(maxalpha, alpha);

                            sPtr += 2;

                            for (; j > 0; --j, ++x)
                            {
                                if (x >= image->width)
                                    return E_FAIL;

                                *dPtr = t;

                                if (convFlags & CONV_FLAGS_INVERTX)
                                    --dPtr;
                                else
                                    ++dPtr;
                            }
                        }
                        else
                        {
                            // Literal
                            size_t j = size_t(*sPtr & 0x7F) + 1;
                            ++sPtr;

                            if (sPtr + (j * 2) > endPtr)
                                return E_FAIL;

                            for (; j > 0; --j, ++x)
                            {
                                if (x >= image->width)
                                    return E_FAIL;

                                auto t = static_cast<uint16_t>(uint32_t(*sPtr) | uint32_t(*(sPtr + 1u) << 8));

                                const uint32_t alpha = (t & 0x8000) ? 255 : 0;
                                minalpha = std::min(minalpha, alpha);
                                maxalpha = std::max(maxalpha, alpha);

                                sPtr += 2;
                                *dPtr = t;

                                if (convFlags & CONV_FLAGS_INVERTX)
                                    --dPtr;
                                else
                                    ++dPtr;
                            }
                        }
                    }
                }

                // If there are no non-zero alpha channel entries, we'll assume alpha is not used and force it to opaque
                if (maxalpha == 0 && !(flags & TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA))
                {
                    opaquealpha = true;
                    hr = SetAlphaChannelToOpaque(image);
                    if (FAILED(hr))
                        return hr;
                }
                else if (minalpha == 255)
                {
                    opaquealpha = true;
                }
            }
            break;

            //------------------------------------------------------ 24/32-bit (with swizzling)
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            {
                uint32_t minalpha = 255;
                uint32_t maxalpha = 0;

                for (size_t y = 0; y < image->height; ++y)
                {
                    size_t offset = ((convFlags & CONV_FLAGS_INVERTX) ? (image->width - 1) : 0);

                    auto dPtr = reinterpret_cast<uint32_t*>(image->pixels
                        + (image->rowPitch * ((convFlags & CONV_FLAGS_INVERTY) ? y : (image->height - y - 1))))
                        + offset;

                    for (size_t x = 0; x < image->width; )
                    {
                        if (sPtr >= endPtr)
                            return E_FAIL;

                        if (*sPtr & 0x80)
                        {
                            // Repeat
                            size_t j = size_t(*sPtr & 0x7F) + 1;
                            ++sPtr;

                            uint32_t t;
                            if (convFlags & CONV_FLAGS_EXPAND)
                            {
                                assert(offset * 3 < rowPitch);

                                if (sPtr + 2 >= endPtr)
                                    return E_FAIL;

                                // BGR -> RGBA
                                t = uint32_t(*sPtr << 16) | uint32_t(*(sPtr + 1) << 8) | uint32_t(*(sPtr + 2)) | 0xFF000000;
                                sPtr += 3;

                                minalpha = maxalpha = 255;
                            }
                            else
                            {
                                assert(offset * 4 < rowPitch);

                                if (sPtr + 3 >= endPtr)
                                    return E_FAIL;

                                // BGRA -> RGBA
                                const uint32_t alpha = *(sPtr + 3);
                                t = uint32_t(*sPtr << 16) | uint32_t(*(sPtr + 1) << 8) | uint32_t(*(sPtr + 2)) | uint32_t(alpha << 24);

                                minalpha = std::min(minalpha, alpha);
                                maxalpha = std::max(maxalpha, alpha);

                                sPtr += 4;
                            }

                            for (; j > 0; --j, ++x)
                            {
                                if (x >= image->width)
                                    return E_FAIL;

                                *dPtr = t;

                                if (convFlags & CONV_FLAGS_INVERTX)
                                    --dPtr;
                                else
                                    ++dPtr;
                            }
                        }
                        else
                        {
                            // Literal
                            size_t j = size_t(*sPtr & 0x7F) + 1;
                            ++sPtr;

                            if (convFlags & CONV_FLAGS_EXPAND)
                            {
                                if (sPtr + (j * 3) > endPtr)
                                    return E_FAIL;
                            }
                            else
                            {
                                if (sPtr + (j * 4) > endPtr)
                                    return E_FAIL;
                            }

                            for (; j > 0; --j, ++x)
                            {
                                if (x >= image->width)
                                    return E_FAIL;

                                if (convFlags & CONV_FLAGS_EXPAND)
                                {
                                    assert(offset * 3 < rowPitch);

                                    if (sPtr + 2 >= endPtr)
                                        return E_FAIL;

                                    // BGR -> RGBA
                                    *dPtr = uint32_t(*sPtr << 16) | uint32_t(*(sPtr + 1) << 8) | uint32_t(*(sPtr + 2)) | 0xFF000000;
                                    sPtr += 3;

                                    minalpha = maxalpha = 255;
                                }
                                else
                                {
                                    assert(offset * 4 < rowPitch);

                                    if (sPtr + 3 >= endPtr)
                                        return E_FAIL;

                                    // BGRA -> RGBA
                                    uint32_t alpha = *(sPtr + 3);
                                    *dPtr = uint32_t(*sPtr << 16) | uint32_t(*(sPtr + 1) << 8) | uint32_t(*(sPtr + 2)) | uint32_t(alpha << 24);

                                    minalpha = std::min(minalpha, alpha);
                                    maxalpha = std::max(maxalpha, alpha);

                                    sPtr += 4;
                                }

                                if (convFlags & CONV_FLAGS_INVERTX)
                                    --dPtr;
                                else
                                    ++dPtr;
                            }
                        }
                    }
                }

                // If there are no non-zero alpha channel entries, we'll assume alpha is not used and force it to opaque
                if (maxalpha == 0 && !(flags & TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA))
                {
                    opaquealpha = true;
                    hr = SetAlphaChannelToOpaque(image);
                    if (FAILED(hr))
                        return hr;
                }
                else if (minalpha == 255)
                {
                    opaquealpha = true;
                }
            }
            break;

            //-------------------------------------------------------------------- 32-bit (BGR)
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            {
                assert((convFlags & CONV_FLAGS_EXPAND) == 0);

                uint32_t minalpha = 255;
                uint32_t maxalpha = 0;

                for (size_t y = 0; y < image->height; ++y)
                {
                    size_t offset = ((convFlags & CONV_FLAGS_INVERTX) ? (image->width - 1) : 0);

                    auto dPtr = reinterpret_cast<uint32_t*>(image->pixels
                        + (image->rowPitch * ((convFlags & CONV_FLAGS_INVERTY) ? y : (image->height - y - 1))))
                        + offset;

                    for (size_t x = 0; x < image->width; )
                    {
                        if (sPtr >= endPtr)
                            return E_FAIL;

                        if (*sPtr & 0x80)
                        {
                            // Repeat
                            size_t j = size_t(*sPtr & 0x7F) + 1;
                            ++sPtr;

                            assert(offset * 4 < rowPitch);

                            if (sPtr + 3 >= endPtr)
                                return E_FAIL;

                            const uint32_t alpha = *(sPtr + 3);

                            auto t = *reinterpret_cast<const uint32_t*>(sPtr);

                            minalpha = std::min(minalpha, alpha);
                            maxalpha = std::max(maxalpha, alpha);

                            sPtr += 4;

                            for (; j > 0; --j, ++x)
                            {
                                if (x >= image->width)
                                    return E_FAIL;

                                *dPtr = t;

                                if (convFlags & CONV_FLAGS_INVERTX)
                                    --dPtr;
                                else
                                    ++dPtr;
                            }
                        }
                        else
                        {
                            // Literal
                            size_t j = size_t(*sPtr & 0x7F) + 1;
                            ++sPtr;

                            if (sPtr + (j * 4) > endPtr)
                                return E_FAIL;

                            for (; j > 0; --j, ++x)
                            {
                                if (x >= image->width)
                                    return E_FAIL;

                                assert(offset * 4 < rowPitch);

                                if (sPtr + 3 >= endPtr)
                                    return E_FAIL;

                                const uint32_t alpha = *(sPtr + 3);
                                *dPtr = *reinterpret_cast<const uint32_t*>(sPtr);

                                minalpha = std::min(minalpha, alpha);
                                maxalpha = std::max(maxalpha, alpha);

                                sPtr += 4;

                                if (convFlags & CONV_FLAGS_INVERTX)
                                    --dPtr;
                                else
                                    ++dPtr;
                            }
                        }
                    }
                }

                // If there are no non-zero alpha channel entries, we'll assume alpha is not used and force it to opaque
                if (maxalpha == 0 && !(flags & TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA))
                {
                    opaquealpha = true;
                    hr = SetAlphaChannelToOpaque(image);
                    if (FAILED(hr))
                        return hr;
                }
                else if (minalpha == 255)
                {
                    opaquealpha = true;
                }
            }
            break;

            //-------------------------------------------------------------------- 24-bit (BGR)
        case DXGI_FORMAT_B8G8R8X8_UNORM:
            {
                assert((convFlags & CONV_FLAGS_EXPAND) != 0);

                for (size_t y = 0; y < image->height; ++y)
                {
                    size_t offset = ((convFlags & CONV_FLAGS_INVERTX) ? (image->width - 1) : 0);

                    auto dPtr = reinterpret_cast<uint32_t*>(image->pixels
                        + (image->rowPitch * ((convFlags & CONV_FLAGS_INVERTY) ? y : (image->height - y - 1))))
                        + offset;

                    for (size_t x = 0; x < image->width; )
                    {
                        if (sPtr >= endPtr)
                            return E_FAIL;

                        if (*sPtr & 0x80)
                        {
                            // Repeat
                            size_t j = size_t(*sPtr & 0x7F) + 1;
                            ++sPtr;

                            assert(offset * 3 < rowPitch);

                            if (sPtr + 2 >= endPtr)
                                return E_FAIL;

                            uint32_t t = uint32_t(*sPtr) | uint32_t(*(sPtr + 1) << 8) | uint32_t(*(sPtr + 2) << 16);
                            sPtr += 3;

                            for (; j > 0; --j, ++x)
                            {
                                if (x >= image->width)
                                    return E_FAIL;

                                *dPtr = t;

                                if (convFlags & CONV_FLAGS_INVERTX)
                                    --dPtr;
                                else
                                    ++dPtr;
                            }
                        }
                        else
                        {
                            // Literal
                            size_t j = size_t(*sPtr & 0x7F) + 1;
                            ++sPtr;

                            if (sPtr + (j * 3) > endPtr)
                                return E_FAIL;

                            for (; j > 0; --j, ++x)
                            {
                                if (x >= image->width)
                                    return E_FAIL;

                                assert(offset * 3 < rowPitch);

                                if (sPtr + 2 >= endPtr)
                                    return E_FAIL;

                                *dPtr = uint32_t(*sPtr) | uint32_t(*(sPtr + 1) << 8) | uint32_t(*(sPtr + 2) << 16);
                                sPtr += 3;

                                if (convFlags & CONV_FLAGS_INVERTX)
                                    --dPtr;
                                else
                                    ++dPtr;
                            }
                        }
                    }
                }
            }
            break;

            //---------------------------------------------------------------------------------
        default:
            return E_FAIL;
        }

        return opaquealpha ? S_FALSE : S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Copies pixel data from a TGA into the target image
    //-------------------------------------------------------------------------------------
    HRESULT CopyPixels(
        _In_reads_bytes_(size) const void* pSource,
        size_t size,
        TGA_FLAGS flags,
        _In_ const Image* image,
        _In_ uint32_t convFlags,
        _In_opt_ const uint8_t* palette) noexcept
    {
        assert(pSource && size > 0);

        if (!image || !image->pixels)
            return E_POINTER;

        // Compute TGA image data pitch
        size_t rowPitch, slicePitch;
        HRESULT hr = ComputePitch(image->format, image->width, image->height,
            rowPitch, slicePitch,
            (convFlags & CONV_FLAGS_EXPAND) ? CP_FLAGS_24BPP : CP_FLAGS_NONE);
        if (FAILED(hr))
            return hr;

        auto sPtr = static_cast<const uint8_t*>(pSource);
        const uint8_t* endPtr = sPtr + size;

        bool opaquealpha = false;

        if ((convFlags & CONV_FLAGS_PALETTED) != 0)
        {
            if (!palette)
                return E_UNEXPECTED;

            const auto table = reinterpret_cast<const uint32_t*>(palette);

            for (size_t y = 0; y < image->height; ++y)
            {
                size_t offset = ((convFlags & CONV_FLAGS_INVERTX) ? (image->width - 1) : 0);
                assert(offset < rowPitch);

                auto dPtr = reinterpret_cast<uint32_t*>(image->pixels
                    + (image->rowPitch * ((convFlags & CONV_FLAGS_INVERTY) ? y : (image->height - y - 1))))
                    + offset;

                for (size_t x = 0; x < image->width; ++x)
                {
                    if (sPtr >= endPtr)
                        return E_FAIL;

                    *dPtr = table[*(sPtr++)];

                    if (convFlags & CONV_FLAGS_INVERTX)
                        --dPtr;
                    else
                        ++dPtr;
                }
            }
        }
        else
        {
            switch (image->format)
            {
            //----------------------------------------------------------------------- 8-bit
            case DXGI_FORMAT_R8_UNORM:
                for (size_t y = 0; y < image->height; ++y)
                {
                    size_t offset = ((convFlags & CONV_FLAGS_INVERTX) ? (image->width - 1) : 0);
                    assert(offset < rowPitch);

                    uint8_t* dPtr = image->pixels
                        + (image->rowPitch * ((convFlags & CONV_FLAGS_INVERTY) ? y : (image->height - y - 1)))
                        + offset;

                    for (size_t x = 0; x < image->width; ++x)
                    {
                        if (sPtr >= endPtr)
                            return E_FAIL;

                        *dPtr = *(sPtr++);

                        if (convFlags & CONV_FLAGS_INVERTX)
                            --dPtr;
                        else
                            ++dPtr;
                    }
                }
                break;

            //---------------------------------------------------------------------- 16-bit
            case DXGI_FORMAT_B5G5R5A1_UNORM:
                {
                    uint32_t minalpha = 255;
                    uint32_t maxalpha = 0;

                    for (size_t y = 0; y < image->height; ++y)
                    {
                        size_t offset = ((convFlags & CONV_FLAGS_INVERTX) ? (image->width - 1) : 0);
                        assert(offset * 2 < rowPitch);

                        auto dPtr = reinterpret_cast<uint16_t*>(image->pixels
                            + (image->rowPitch * ((convFlags & CONV_FLAGS_INVERTY) ? y : (image->height - y - 1))))
                            + offset;

                        for (size_t x = 0; x < image->width; ++x)
                        {
                            if (sPtr + 1 >= endPtr)
                                return E_FAIL;

                            auto t = static_cast<uint16_t>(uint32_t(*sPtr) | uint32_t(*(sPtr + 1u) << 8));
                            sPtr += 2;
                            *dPtr = t;

                            const uint32_t alpha = (t & 0x8000) ? 255 : 0;
                            minalpha = std::min(minalpha, alpha);
                            maxalpha = std::max(maxalpha, alpha);

                            if (convFlags & CONV_FLAGS_INVERTX)
                                --dPtr;
                            else
                                ++dPtr;
                        }
                    }

                    // If there are no non-zero alpha channel entries, we'll assume alpha is not used and force it to opaque
                    if (maxalpha == 0 && !(flags & TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA))
                    {
                        opaquealpha = true;
                        hr = SetAlphaChannelToOpaque(image);
                        if (FAILED(hr))
                            return hr;
                    }
                    else if (minalpha == 255)
                    {
                        opaquealpha = true;
                    }
                }
                break;

            //-------------------------------------------------- 24/32-bit (with swizzling)
            case DXGI_FORMAT_R8G8B8A8_UNORM:
                {
                    uint32_t minalpha = 255;
                    uint32_t maxalpha = 0;

                    for (size_t y = 0; y < image->height; ++y)
                    {
                        size_t offset = ((convFlags & CONV_FLAGS_INVERTX) ? (image->width - 1) : 0);

                        auto dPtr = reinterpret_cast<uint32_t*>(image->pixels
                            + (image->rowPitch * ((convFlags & CONV_FLAGS_INVERTY) ? y : (image->height - y - 1))))
                            + offset;

                        for (size_t x = 0; x < image->width; ++x)
                        {
                            if (convFlags & CONV_FLAGS_EXPAND)
                            {
                                assert(offset * 3 < rowPitch);

                                if (sPtr + 2 >= endPtr)
                                    return E_FAIL;

                                // BGR -> RGBA
                                *dPtr = uint32_t(*sPtr << 16) | uint32_t(*(sPtr + 1) << 8) | uint32_t(*(sPtr + 2)) | 0xFF000000;
                                sPtr += 3;

                                minalpha = maxalpha = 255;
                            }
                            else
                            {
                                assert(offset * 4 < rowPitch);

                                if (sPtr + 3 >= endPtr)
                                    return E_FAIL;

                                // BGRA -> RGBA
                                uint32_t alpha = *(sPtr + 3);
                                *dPtr = uint32_t(*sPtr << 16) | uint32_t(*(sPtr + 1) << 8) | uint32_t(*(sPtr + 2)) | uint32_t(alpha << 24);

                                minalpha = std::min(minalpha, alpha);
                                maxalpha = std::max(maxalpha, alpha);

                                sPtr += 4;
                            }

                            if (convFlags & CONV_FLAGS_INVERTX)
                                --dPtr;
                            else
                                ++dPtr;
                        }
                    }

                    // If there are no non-zero alpha channel entries, we'll assume alpha is not used and force it to opaque
                    if (maxalpha == 0 && !(flags & TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA))
                    {
                        opaquealpha = true;
                        hr = SetAlphaChannelToOpaque(image);
                        if (FAILED(hr))
                            return hr;
                    }
                    else if (minalpha == 255)
                    {
                        opaquealpha = true;
                    }
                }
                break;

            //---------------------------------------------------------------- 32-bit (BGR)
            case DXGI_FORMAT_B8G8R8A8_UNORM:
                {
                    assert((convFlags & CONV_FLAGS_EXPAND) == 0);

                    uint32_t minalpha = 255;
                    uint32_t maxalpha = 0;

                    for (size_t y = 0; y < image->height; ++y)
                    {
                        size_t offset = ((convFlags & CONV_FLAGS_INVERTX) ? (image->width - 1) : 0);

                        auto dPtr = reinterpret_cast<uint32_t*>(image->pixels
                            + (image->rowPitch * ((convFlags & CONV_FLAGS_INVERTY) ? y : (image->height - y - 1))))
                            + offset;

                        for (size_t x = 0; x < image->width; ++x)
                        {
                            assert(offset * 4 < rowPitch);

                            if (sPtr + 3 >= endPtr)
                                return E_FAIL;

                            const uint32_t alpha = *(sPtr + 3);
                            *dPtr = *reinterpret_cast<const uint32_t*>(sPtr);

                            minalpha = std::min(minalpha, alpha);
                            maxalpha = std::max(maxalpha, alpha);

                            sPtr += 4;

                            if (convFlags & CONV_FLAGS_INVERTX)
                                --dPtr;
                            else
                                ++dPtr;
                        }
                    }

                    // If there are no non-zero alpha channel entries, we'll assume alpha is not used and force it to opaque
                    if (maxalpha == 0 && !(flags & TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA))
                    {
                        opaquealpha = true;
                        hr = SetAlphaChannelToOpaque(image);
                        if (FAILED(hr))
                            return hr;
                    }
                    else if (minalpha == 255)
                    {
                        opaquealpha = true;
                    }
                }
                break;

            //---------------------------------------------------------------- 24-bit (BGR)
            case DXGI_FORMAT_B8G8R8X8_UNORM:
                {
                    assert((convFlags & CONV_FLAGS_EXPAND) != 0);

                    for (size_t y = 0; y < image->height; ++y)
                    {
                        size_t offset = ((convFlags & CONV_FLAGS_INVERTX) ? (image->width - 1) : 0);

                        auto dPtr = reinterpret_cast<uint32_t*>(image->pixels
                            + (image->rowPitch * ((convFlags & CONV_FLAGS_INVERTY) ? y : (image->height - y - 1))))
                            + offset;

                        for (size_t x = 0; x < image->width; ++x)
                        {
                            assert(offset * 3 < rowPitch);

                            if (sPtr + 2 >= endPtr)
                                return E_FAIL;

                            *dPtr = uint32_t(*sPtr) | uint32_t(*(sPtr + 1) << 8) | uint32_t(*(sPtr + 2) << 16);
                            sPtr += 3;

                            if (convFlags & CONV_FLAGS_INVERTX)
                                --dPtr;
                            else
                                ++dPtr;
                        }
                    }
                }
                break;

            //-----------------------------------------------------------------------------
            default:
                return E_FAIL;
            }
        }

        return opaquealpha ? S_FALSE : S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Encodes TGA file header
    //-------------------------------------------------------------------------------------
    HRESULT EncodeTGAHeader(_In_ const Image& image, _Out_ TGA_HEADER& header, _Inout_ uint32_t& convFlags) noexcept
    {
        memset(&header, 0, TGA_HEADER_LEN);

        if ((image.width > UINT16_MAX)
            || (image.height > UINT16_MAX))
        {
            return HRESULT_E_NOT_SUPPORTED;
        }

        header.wWidth = static_cast<uint16_t>(image.width);
        header.wHeight = static_cast<uint16_t>(image.height);

        switch (image.format)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            header.bImageType = TGA_TRUECOLOR;
            header.bBitsPerPixel = 32;
            header.bDescriptor = TGA_FLAGS_INVERTY | 8;
            convFlags |= CONV_FLAGS_SWIZZLE;
            break;

        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            header.bImageType = TGA_TRUECOLOR;
            header.bBitsPerPixel = 32;
            header.bDescriptor = TGA_FLAGS_INVERTY | 8;
            break;

        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            header.bImageType = TGA_TRUECOLOR;
            header.bBitsPerPixel = 24;
            header.bDescriptor = TGA_FLAGS_INVERTY;
            convFlags |= CONV_FLAGS_888;
            break;

        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_A8_UNORM:
            header.bImageType = TGA_BLACK_AND_WHITE;
            header.bBitsPerPixel = 8;
            header.bDescriptor = TGA_FLAGS_INVERTY;
            break;

        case DXGI_FORMAT_B5G5R5A1_UNORM:
            header.bImageType = TGA_TRUECOLOR;
            header.bBitsPerPixel = 16;
            header.bDescriptor = TGA_FLAGS_INVERTY | 1;
            break;

        default:
            return HRESULT_E_NOT_SUPPORTED;
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Copies BGRX data to form BGR 24bpp data
    //-------------------------------------------------------------------------------------
#pragma warning(suppress: 6001 6101) // In the case where outSize is insufficient we do not write to pDestination
    void Copy24bppScanline(
        _Out_writes_bytes_(outSize) void* pDestination,
        _In_ size_t outSize,
        _In_reads_bytes_(inSize) const void* pSource,
        _In_ size_t inSize) noexcept
    {
        assert(pDestination && outSize > 0);
        assert(pSource && inSize > 0);

        assert(pDestination != pSource);

        const uint32_t * __restrict sPtr = static_cast<const uint32_t*>(pSource);
        uint8_t * __restrict dPtr = static_cast<uint8_t*>(pDestination);

        if (inSize >= 4 && outSize >= 3)
        {
            const uint8_t* endPtr = dPtr + outSize;

            for (size_t count = 0; count < (inSize - 3); count += 4)
            {
                uint32_t t = *(sPtr++);

                if (dPtr + 3 > endPtr)
                    return;

                *(dPtr++) = uint8_t(t & 0xFF);              // Blue
                *(dPtr++) = uint8_t((t & 0xFF00) >> 8);     // Green
                *(dPtr++) = uint8_t((t & 0xFF0000) >> 16);  // Red
            }
        }
    }

    //-------------------------------------------------------------------------------------
    // TGA 2.0 Extension helpers
    //-------------------------------------------------------------------------------------
    void SetExtension(_In_ TGA_EXTENSION *ext, TGA_FLAGS flags, const TexMetadata& metadata) noexcept
    {
        memset(ext, 0, sizeof(TGA_EXTENSION));

        ext->wSize = sizeof(TGA_EXTENSION);

        memcpy(ext->szSoftwareId, "DirectXTex", sizeof("DirectXTex"));
        ext->wVersionNumber = DIRECTX_TEX_VERSION;
        ext->bVersionLetter = ' ';

        const bool sRGB = ((flags & TGA_FLAGS_FORCE_LINEAR) == 0) && ((flags & TGA_FLAGS_FORCE_SRGB) != 0 || IsSRGB(metadata.format));
        if (sRGB)
        {
            ext->wGammaNumerator = 22;
            ext->wGammaDenominator = 10;
        }
        else if (flags & TGA_FLAGS_FORCE_LINEAR)
        {
            ext->wGammaNumerator = 1;
            ext->wGammaDenominator = 1;
        }

        switch (metadata.GetAlphaMode())
        {
        default:
        case TEX_ALPHA_MODE_UNKNOWN:
            ext->bAttributesType = HasAlpha(metadata.format) ? TGA_ATTRIBUTE_UNDEFINED : TGA_ATTRIBUTE_NONE;
            break;

        case TEX_ALPHA_MODE_STRAIGHT:
            ext->bAttributesType = TGA_ATTRIBUTE_ALPHA;
            break;

        case TEX_ALPHA_MODE_PREMULTIPLIED:
            ext->bAttributesType = TGA_ATTRIBUTE_PREMULTIPLIED;
            break;

        case TEX_ALPHA_MODE_OPAQUE:
            ext->bAttributesType = TGA_ATTRIBUTE_IGNORED;
            break;

        case TEX_ALPHA_MODE_CUSTOM:
            ext->bAttributesType = TGA_ATTRIBUTE_UNDEFINED;
            break;
        }

        // Set file time stamp
        {
            time_t now = {};
            time(&now);

        #ifdef _WIN32
            tm info;
            auto pinfo = &info;
            if (!gmtime_s(pinfo, &now))
            #else
            const tm* pinfo = gmtime(&now);
            if (pinfo)
            #endif
            {
                ext->wStampMonth = static_cast<uint16_t>(pinfo->tm_mon + 1);
                ext->wStampDay = static_cast<uint16_t>(pinfo->tm_mday);
                ext->wStampYear = static_cast<uint16_t>(pinfo->tm_year + 1900);
                ext->wStampHour = static_cast<uint16_t>(pinfo->tm_hour);
                ext->wStampMinute = static_cast<uint16_t>(pinfo->tm_min);
                ext->wStampSecond = static_cast<uint16_t>(pinfo->tm_sec);
            }
        }
    }

    TEX_ALPHA_MODE GetAlphaModeFromExtension(_In_opt_ const TGA_EXTENSION *ext) noexcept
    {
        if (ext && ext->wSize == sizeof(TGA_EXTENSION))
        {
            switch (ext->bAttributesType)
            {
            case TGA_ATTRIBUTE_IGNORED: return TEX_ALPHA_MODE_OPAQUE;
            case TGA_ATTRIBUTE_UNDEFINED: return TEX_ALPHA_MODE_CUSTOM;
            case TGA_ATTRIBUTE_ALPHA: return TEX_ALPHA_MODE_STRAIGHT;
            case TGA_ATTRIBUTE_PREMULTIPLIED: return TEX_ALPHA_MODE_PREMULTIPLIED;
            default: return TEX_ALPHA_MODE_UNKNOWN;
            }
        }

        return TEX_ALPHA_MODE_UNKNOWN;
    }

    DXGI_FORMAT GetSRGBFromExtension(_In_opt_ const TGA_EXTENSION* ext, DXGI_FORMAT format, TGA_FLAGS flags, _In_opt_ ScratchImage* image) noexcept
    {
        bool sRGB = false;

        if (ext && ext->wSize == sizeof(TGA_EXTENSION) && ext->wGammaDenominator != 0)
        {
            auto const gamma = static_cast<float>(ext->wGammaNumerator) / static_cast<float>(ext->wGammaDenominator);
            if (fabsf(gamma - 2.2f) < GAMMA_EPSILON || fabsf(gamma - 2.4f) < GAMMA_EPSILON)
            {
                sRGB = true;
            }
        }
        else
        {
            sRGB = (flags & TGA_FLAGS_DEFAULT_SRGB) != 0;
        }

        if (sRGB)
        {
            format = MakeSRGB(format);
            if (image)
            {
                image->OverrideFormat(format);
            }
        }

        return format;
    }
}


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Obtain metadata from TGA file in memory/on disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromTGAMemory(
    const void* pSource,
    size_t size,
    TGA_FLAGS flags,
    TexMetadata& metadata) noexcept
{
    if (!pSource || size == 0)
        return E_INVALIDARG;

    size_t offset;
    HRESULT hr = DecodeTGAHeader(pSource, size, flags, metadata, offset, nullptr);
    if (FAILED(hr))
        return hr;

    // Optional TGA 2.0 footer & extension area
    const TGA_EXTENSION* ext = nullptr;
    if (size >= sizeof(TGA_FOOTER))
    {
        auto footer = reinterpret_cast<const TGA_FOOTER*>(static_cast<const uint8_t*>(pSource) + size - sizeof(TGA_FOOTER));

        if (memcmp(footer->Signature, g_Signature, sizeof(g_Signature)) == 0)
        {
            if (footer->dwExtensionOffset != 0
                && ((footer->dwExtensionOffset + sizeof(TGA_EXTENSION)) <= size))
            {
                ext = reinterpret_cast<const TGA_EXTENSION*>(static_cast<const uint8_t*>(pSource) + footer->dwExtensionOffset);
                metadata.SetAlphaMode(GetAlphaModeFromExtension(ext));
            }
        }
    }

    if (!(flags & TGA_FLAGS_IGNORE_SRGB))
    {
        metadata.format = GetSRGBFromExtension(ext, metadata.format, flags, nullptr);
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromTGAFile(const wchar_t* szFile, TGA_FLAGS flags, TexMetadata& metadata) noexcept
{
    if (!szFile)
        return E_INVALIDARG;

#ifdef _WIN32
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(szFile, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, nullptr)));
#endif
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Get the file size
    FILE_STANDARD_INFO fileInfo;
    if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // File is too big for 32-bit allocation, so reject read (4 GB should be plenty large enough for a valid TGA file)
    if (fileInfo.EndOfFile.HighPart > 0)
    {
        return HRESULT_E_FILE_TOO_LARGE;
    }

    const size_t len = fileInfo.EndOfFile.LowPart;
#else // !WIN32
    std::ifstream inFile(std::filesystem::path(szFile), std::ios::in | std::ios::binary | std::ios::ate);
    if (!inFile)
        return E_FAIL;

    std::streampos fileLen = inFile.tellg();
    if (!inFile)
        return E_FAIL;

    if (fileLen > UINT32_MAX)
        return HRESULT_E_FILE_TOO_LARGE;

    inFile.seekg(0, std::ios::beg);
    if (!inFile)
        return E_FAIL;

    size_t len = fileLen;
#endif

    // Need at least enough data to fill the standard header to be a valid TGA
    if (len < TGA_HEADER_LEN)
    {
        return E_FAIL;
    }

    // Read the standard header (we don't need the file footer to parse the file)
    uint8_t header[TGA_HEADER_LEN] = {};

#ifdef _WIN32
    DWORD bytesRead = 0;
    if (!ReadFile(hFile.get(), header, TGA_HEADER_LEN, &bytesRead, nullptr))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    auto const headerLen = static_cast<size_t>(bytesRead);
#else
    inFile.read(reinterpret_cast<char*>(header), TGA_HEADER_LEN);
    if (!inFile)
        return E_FAIL;

    size_t headerLen = TGA_HEADER_LEN;
#endif

    size_t offset;
    HRESULT hr = DecodeTGAHeader(header, headerLen, flags, metadata, offset, nullptr);
    if (FAILED(hr))
        return hr;

    // Optional TGA 2.0 footer & extension area
    const TGA_EXTENSION* ext = nullptr;
    TGA_EXTENSION extData = {};
    {
        TGA_FOOTER footer = {};

    #ifdef _WIN32
        if (SetFilePointer(hFile.get(), -static_cast<int>(sizeof(TGA_FOOTER)), nullptr, FILE_END) != INVALID_SET_FILE_POINTER)
        {
            if (!ReadFile(hFile.get(), &footer, sizeof(TGA_FOOTER), &bytesRead, nullptr))
            {
                return HRESULT_FROM_WIN32(GetLastError());
            }

            if (bytesRead != sizeof(TGA_FOOTER))
            {
                return E_FAIL;
            }
        }
    #else
        inFile.seekg(-static_cast<int>(sizeof(TGA_FOOTER)), std::ios::end);
        if (inFile)
        {
            inFile.read(reinterpret_cast<char*>(&footer), sizeof(TGA_FOOTER));
            if (!inFile)
                return E_FAIL;
        }
    #endif

        if (memcmp(footer.Signature, g_Signature, sizeof(g_Signature)) == 0)
        {
            if (footer.dwExtensionOffset != 0
                && ((footer.dwExtensionOffset + sizeof(TGA_EXTENSION)) <= len))
            {
            #ifdef _WIN32
                const LARGE_INTEGER filePos = { { static_cast<DWORD>(footer.dwExtensionOffset), 0 } };
                if (SetFilePointerEx(hFile.get(), filePos, nullptr, FILE_BEGIN))
                {
                    if (ReadFile(hFile.get(), &extData, sizeof(TGA_EXTENSION), &bytesRead, nullptr)
                        && bytesRead == sizeof(TGA_EXTENSION))
                    {
                        ext = &extData;
                        metadata.SetAlphaMode(GetAlphaModeFromExtension(ext));
                    }
                }
            #else // !WIN32
                inFile.seekg(static_cast<std::streampos>(footer.dwExtensionOffset), std::ios::beg);
                if (inFile)
                {
                    inFile.read(reinterpret_cast<char*>(&extData), sizeof(TGA_EXTENSION));
                    if (inFile)
                    {
                        ext = &extData;
                        metadata.SetAlphaMode(GetAlphaModeFromExtension(ext));
                    }
                }
            #endif
            }
        }
    }

    if (!(flags & TGA_FLAGS_IGNORE_SRGB))
    {
        metadata.format = GetSRGBFromExtension(ext, metadata.format, flags, nullptr);
    }

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Load a TGA file in memory
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::LoadFromTGAMemory(
    const void* pSource,
    size_t size,
    TGA_FLAGS flags,
    TexMetadata* metadata,
    ScratchImage& image) noexcept
{
    if (!pSource || size == 0)
        return E_INVALIDARG;

    image.Release();

    size_t offset;
    uint32_t convFlags = 0;
    TexMetadata mdata;
    HRESULT hr = DecodeTGAHeader(pSource, size, flags, mdata, offset, &convFlags);
    if (FAILED(hr))
        return hr;

    if (offset > size)
        return HRESULT_E_INVALID_DATA;

    size_t paletteOffset = 0;
    uint8_t palette[256 * 4] = {};
    if (convFlags & CONV_FLAGS_PALETTED)
    {
        const size_t remaining = size - offset;
        if (remaining == 0)
            return E_FAIL;

        auto pColorMap = static_cast<const uint8_t*>(pSource) + offset;

        _Analysis_assume_(size > TGA_HEADER_LEN);
        hr = ReadPalette(static_cast<const uint8_t*>(pSource), pColorMap, remaining, flags,
            palette, paletteOffset);
        if (FAILED(hr))
            return hr;
    }

    const size_t remaining = size - offset - paletteOffset;
    if (remaining == 0)
        return E_FAIL;

    const void* pPixels = static_cast<const uint8_t*>(pSource) + offset + paletteOffset;

    hr = image.Initialize2D(mdata.format, mdata.width, mdata.height, 1, 1, CP_FLAGS_LIMIT_4GB);
    if (FAILED(hr))
        return hr;

    if (convFlags & CONV_FLAGS_RLE)
    {
        hr = UncompressPixels(pPixels, remaining, flags, image.GetImage(0, 0, 0), convFlags);
    }
    else
    {
        hr = CopyPixels(pPixels, remaining, flags, image.GetImage(0, 0, 0), convFlags, palette);
    }

    if (FAILED(hr))
    {
        image.Release();
        return hr;
    }

    // Optional TGA 2.0 footer & extension area
    const TGA_EXTENSION* ext = nullptr;
    if (size >= sizeof(TGA_FOOTER))
    {
        auto footer = reinterpret_cast<const TGA_FOOTER*>(static_cast<const uint8_t*>(pSource) + size - sizeof(TGA_FOOTER));

        if (memcmp(footer->Signature, g_Signature, sizeof(g_Signature)) == 0)
        {
            if (footer->dwExtensionOffset != 0
                && ((footer->dwExtensionOffset + sizeof(TGA_EXTENSION)) <= size))
            {
                ext = reinterpret_cast<const TGA_EXTENSION*>(static_cast<const uint8_t*>(pSource) + footer->dwExtensionOffset);
            }
        }
    }

    if (!(flags & TGA_FLAGS_IGNORE_SRGB))
    {
        mdata.format = GetSRGBFromExtension(ext, mdata.format, flags, &image);
    }

    if (metadata)
    {
        memcpy(metadata, &mdata, sizeof(TexMetadata));
        if (hr == S_FALSE)
        {
            metadata->SetAlphaMode(TEX_ALPHA_MODE_OPAQUE);
        }
        else if (ext)
        {
            metadata->SetAlphaMode(GetAlphaModeFromExtension(ext));
        }
    }

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Load a TGA file from disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::LoadFromTGAFile(
    const wchar_t* szFile,
    TGA_FLAGS flags,
    TexMetadata* metadata,
    ScratchImage& image) noexcept
{
    if (!szFile)
        return E_INVALIDARG;

    image.Release();

#ifdef _WIN32
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(szFile, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, nullptr)));
#endif
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Get the file size
    FILE_STANDARD_INFO fileInfo;
    if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // File is too big for 32-bit allocation, so reject read (4 GB should be plenty large enough for a valid TGA file)
    if (fileInfo.EndOfFile.HighPart > 0)
    {
        return HRESULT_E_FILE_TOO_LARGE;
    }

    const size_t len = fileInfo.EndOfFile.LowPart;
#else // !WIN32
    std::ifstream inFile(std::filesystem::path(szFile), std::ios::in | std::ios::binary | std::ios::ate);
    if (!inFile)
        return E_FAIL;

    std::streampos fileLen = inFile.tellg();
    if (!inFile)
        return E_FAIL;

    if (fileLen > UINT32_MAX)
        return HRESULT_E_FILE_TOO_LARGE;

    inFile.seekg(0, std::ios::beg);
    if (!inFile)
        return E_FAIL;

    size_t len = fileLen;
#endif

    // Need at least enough data to fill the header to be a valid TGA
    if (len < TGA_HEADER_LEN)
    {
        return E_FAIL;
    }

    // Read the header
    uint8_t header[TGA_HEADER_LEN] = {};

#ifdef _WIN32
    DWORD bytesRead = 0;
    if (!ReadFile(hFile.get(), header, TGA_HEADER_LEN, &bytesRead, nullptr))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    auto const headerLen = static_cast<size_t>(bytesRead);
#else
    inFile.read(reinterpret_cast<char*>(header), TGA_HEADER_LEN);
    if (!inFile)
        return E_FAIL;

    size_t headerLen = TGA_HEADER_LEN;
#endif

    size_t offset;
    uint32_t convFlags = 0;
    TexMetadata mdata;
    HRESULT hr = DecodeTGAHeader(header, headerLen, flags, mdata, offset, &convFlags);
    if (FAILED(hr))
        return hr;

    if (offset > len)
        return HRESULT_E_INVALID_DATA;

    // Read the pixels
    auto const remaining = len - offset;
    if (remaining == 0)
        return E_FAIL;

    if (offset > TGA_HEADER_LEN)
    {
    #ifdef _WIN32
            // Skip past the id string
        const LARGE_INTEGER filePos = { { static_cast<DWORD>(offset), 0 } };
        if (!SetFilePointerEx(hFile.get(), filePos, nullptr, FILE_BEGIN))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }
    #else
        inFile.seekg(offset, std::ios::beg);
        if (!inFile)
            return E_FAIL;
    #endif
    }

    hr = image.Initialize2D(mdata.format, mdata.width, mdata.height, 1, 1, CP_FLAGS_LIMIT_4GB);
    if (FAILED(hr))
        return hr;

    assert(image.GetPixels());

    bool opaquealpha = false;

    if (!(convFlags & (CONV_FLAGS_RLE | CONV_FLAGS_EXPAND | CONV_FLAGS_INVERTX | CONV_FLAGS_PALETTED)) && (convFlags & CONV_FLAGS_INVERTY))
    {
        // This case we can read directly into the image buffer in place
        if (remaining < image.GetPixelsSize())
        {
            image.Release();
            return HRESULT_E_HANDLE_EOF;
        }

        if (image.GetPixelsSize() > UINT32_MAX)
        {
            image.Release();
            return HRESULT_E_ARITHMETIC_OVERFLOW;
        }

    #ifdef _WIN32
        if (!ReadFile(hFile.get(), image.GetPixels(), static_cast<DWORD>(image.GetPixelsSize()), &bytesRead, nullptr))
        {
            image.Release();
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (bytesRead != image.GetPixelsSize())
        {
            image.Release();
            return E_FAIL;
        }
    #else
        inFile.read(reinterpret_cast<char*>(image.GetPixels()), image.GetPixelsSize());
        if (!inFile)
        {
            image.Release();
            return E_FAIL;
        }
    #endif

        switch (mdata.format)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            {
                // TGA stores 32-bit data in BGRA form, need to swizzle to RGBA
                assert(image.GetImageCount() == 1);
                const Image* img = image.GetImage(0, 0, 0);
                if (!img)
                {
                    image.Release();
                    return E_POINTER;
                }

                uint8_t *pPixels = img->pixels;
                if (!pPixels)
                {
                    image.Release();
                    return E_POINTER;
                }

                size_t rowPitch = img->rowPitch;

                // Scan for non-zero alpha channel
                uint32_t minalpha = 255;
                uint32_t maxalpha = 0;

                for (size_t h = 0; h < img->height; ++h)
                {
                    auto sPtr = reinterpret_cast<const uint32_t*>(pPixels);

                    for (size_t x = 0; x < img->width; ++x)
                    {
                        const uint32_t alpha = ((*sPtr & 0xFF000000) >> 24);

                        minalpha = std::min(minalpha, alpha);
                        maxalpha = std::max(maxalpha, alpha);

                        ++sPtr;
                    }

                    pPixels += rowPitch;
                }

                uint32_t tflags = TEXP_SCANLINE_NONE;
                if (maxalpha == 0 && !(flags & TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA))
                {
                    opaquealpha = true;
                    tflags = TEXP_SCANLINE_SETALPHA;
                }
                else if (minalpha == 255)
                {
                    opaquealpha = true;
                }

                // Swizzle scanlines
                pPixels = img->pixels;

                for (size_t h = 0; h < img->height; ++h)
                {
                    SwizzleScanline(pPixels, rowPitch, pPixels, rowPitch, mdata.format, tflags);
                    pPixels += rowPitch;
                }
            }
            break;

        case DXGI_FORMAT_B8G8R8A8_UNORM:
            {
                assert(image.GetImageCount() == 1);
                const Image* img = image.GetImage(0, 0, 0);
                if (!img)
                {
                    image.Release();
                    return E_POINTER;
                }

                // Scan for non-zero alpha channel
                uint32_t minalpha = 255;
                uint32_t maxalpha = 0;

                const uint8_t *pPixels = img->pixels;
                if (!pPixels)
                {
                    image.Release();
                    return E_POINTER;
                }

                const size_t rowPitch = img->rowPitch;

                for (size_t h = 0; h < img->height; ++h)
                {
                    auto sPtr = reinterpret_cast<const uint32_t*>(pPixels);

                    for (size_t x = 0; x < img->width; ++x)
                    {
                        const uint32_t alpha = ((*sPtr & 0xFF000000) >> 24);

                        minalpha = std::min(minalpha, alpha);
                        maxalpha = std::max(maxalpha, alpha);

                        ++sPtr;
                    }

                    pPixels += rowPitch;
                }

                // If there are no non-zero alpha channel entries, we'll assume alpha is not used and force it to opaque
                if (maxalpha == 0 && !(flags & TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA))
                {
                    opaquealpha = true;
                    hr = SetAlphaChannelToOpaque(img);
                    if (FAILED(hr))
                    {
                        image.Release();
                        return hr;
                    }
                }
                else if (minalpha == 255)
                {
                    opaquealpha = true;
                }
            }
            break;

        case DXGI_FORMAT_B5G5R5A1_UNORM:
            {
                assert(image.GetImageCount() == 1);
                const Image* img = image.GetImage(0, 0, 0);
                if (!img)
                {
                    image.Release();
                    return E_POINTER;
                }

                // Scan for non-zero alpha channel
                uint32_t minalpha = 255;
                uint32_t maxalpha = 0;

                const uint8_t *pPixels = img->pixels;
                if (!pPixels)
                {
                    image.Release();
                    return E_POINTER;
                }

                const size_t rowPitch = img->rowPitch;

                for (size_t h = 0; h < img->height; ++h)
                {
                    auto sPtr = reinterpret_cast<const uint16_t*>(pPixels);

                    for (size_t x = 0; x < img->width; ++x)
                    {
                        const uint32_t alpha = (*sPtr & 0x8000) ? 255 : 0;

                        minalpha = std::min(minalpha, alpha);
                        maxalpha = std::max(maxalpha, alpha);

                        ++sPtr;
                    }

                    pPixels += rowPitch;
                }

                // If there are no non-zero alpha channel entries, we'll assume alpha is not used and force it to opaque
                if (maxalpha == 0 && !(flags & TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA))
                {
                    opaquealpha = true;
                    hr = SetAlphaChannelToOpaque(img);
                    if (FAILED(hr))
                    {
                        image.Release();
                        return hr;
                    }
                }
                else if (minalpha == 255)
                {
                    opaquealpha = true;
                }
            }
            break;

        case DXGI_FORMAT_B8G8R8X8_UNORM:
            // Should never be trying to direct-read 24bpp
            return E_FAIL;

        default:
            break;
        }
    }
    else // RLE || EXPAND || INVERTX || PALETTED || !INVERTY
    {
        std::unique_ptr<uint8_t[]> temp(new (std::nothrow) uint8_t[remaining]);
        if (!temp)
        {
            image.Release();
            return E_OUTOFMEMORY;
        }

    #ifdef _WIN32
        if (!ReadFile(hFile.get(), temp.get(), static_cast<DWORD>(remaining), &bytesRead, nullptr))
        {
            image.Release();
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (bytesRead != remaining)
        {
            image.Release();
            return E_FAIL;
        }
    #else
        inFile.read(reinterpret_cast<char*>(temp.get()), remaining);
        if (!inFile)
        {
            image.Release();
            return E_FAIL;
        }
    #endif

        size_t paletteOffset = 0;
        uint8_t palette[256 * 4] = {};
        if (convFlags & CONV_FLAGS_PALETTED)
        {
            hr = ReadPalette(header, temp.get(), remaining, flags, palette, paletteOffset);
            if (FAILED(hr))
            {
                image.Release();
                return hr;
            }
        }

        if (convFlags & CONV_FLAGS_RLE)
        {
            hr = UncompressPixels(temp.get() + paletteOffset, remaining - paletteOffset,
                flags, image.GetImage(0, 0, 0), convFlags);
        }
        else
        {
            hr = CopyPixels(temp.get() + paletteOffset, remaining - paletteOffset,
                flags, image.GetImage(0, 0, 0), convFlags, palette);
        }

        if (FAILED(hr))
        {
            image.Release();
            return hr;
        }

        if (hr == S_FALSE)
            opaquealpha = true;
    }

    // Optional TGA 2.0 footer & extension area
    const TGA_EXTENSION* ext = nullptr;
    TGA_EXTENSION extData = {};
    {
        TGA_FOOTER footer = {};

    #ifdef _WIN32
        if (SetFilePointer(hFile.get(), -static_cast<int>(sizeof(TGA_FOOTER)), nullptr, FILE_END) != INVALID_SET_FILE_POINTER)
        {
            if (!ReadFile(hFile.get(), &footer, sizeof(TGA_FOOTER), &bytesRead, nullptr))
            {
                image.Release();
                return HRESULT_FROM_WIN32(GetLastError());
            }

            if (bytesRead != sizeof(TGA_FOOTER))
            {
                image.Release();
                return E_FAIL;
            }
        }
    #else // !WIN32
        inFile.seekg(-static_cast<int>(sizeof(TGA_FOOTER)), std::ios::end);
        if (inFile)
        {
            inFile.read(reinterpret_cast<char*>(&footer), sizeof(TGA_FOOTER));
            if (!inFile)
            {
                image.Release();
                return E_FAIL;
            }
        }
    #endif

        if (memcmp(footer.Signature, g_Signature, sizeof(g_Signature)) == 0)
        {
            if (footer.dwExtensionOffset != 0
                && ((footer.dwExtensionOffset + sizeof(TGA_EXTENSION)) <= len))
            {
            #ifdef _WIN32
                const LARGE_INTEGER filePos = { { static_cast<DWORD>(footer.dwExtensionOffset), 0 } };
                if (SetFilePointerEx(hFile.get(), filePos, nullptr, FILE_BEGIN))
                {
                    if (ReadFile(hFile.get(), &extData, sizeof(TGA_EXTENSION), &bytesRead, nullptr)
                        && bytesRead == sizeof(TGA_EXTENSION))
                    {
                        ext = &extData;
                    }
                }
            #else // !WIN32
                inFile.seekg(static_cast<std::streampos>(footer.dwExtensionOffset), std::ios::beg);
                if (inFile)
                {
                    inFile.read(reinterpret_cast<char*>(&extData), sizeof(TGA_EXTENSION));
                    if (inFile)
                    {
                        ext = &extData;
                    }
                }
            #endif
            }
        }
    }

    if (!(flags & TGA_FLAGS_IGNORE_SRGB))
    {
        mdata.format = GetSRGBFromExtension(ext, mdata.format, flags, &image);
    }

    if (metadata)
    {
        memcpy(metadata, &mdata, sizeof(TexMetadata));
        if (opaquealpha)
        {
            metadata->SetAlphaMode(TEX_ALPHA_MODE_OPAQUE);
        }
        else if (ext)
        {
            metadata->SetAlphaMode(GetAlphaModeFromExtension(ext));
        }
    }

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Save a TGA file to memory
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::SaveToTGAMemory(
    const Image& image,
    TGA_FLAGS flags,
    Blob& blob,
    const TexMetadata* metadata) noexcept
{
    if ((flags & (TGA_FLAGS_FORCE_LINEAR | TGA_FLAGS_FORCE_SRGB)) != 0 && !metadata)
        return E_INVALIDARG;

    if (!image.pixels)
        return E_POINTER;

    TGA_HEADER tga_header = {};
    uint32_t convFlags = 0;
    HRESULT hr = EncodeTGAHeader(image, tga_header, convFlags);
    if (FAILED(hr))
        return hr;

    blob.Release();

    // Determine memory required for image data
    size_t rowPitch, slicePitch;
    hr = ComputePitch(image.format, image.width, image.height, rowPitch, slicePitch,
        (convFlags & CONV_FLAGS_888) ? CP_FLAGS_24BPP : CP_FLAGS_NONE);
    if (FAILED(hr))
        return hr;

    hr = blob.Initialize(TGA_HEADER_LEN
        + slicePitch
        + (metadata ? sizeof(TGA_EXTENSION) : 0)
        + sizeof(TGA_FOOTER));
    if (FAILED(hr))
        return hr;

    // Copy header
    auto destPtr = static_cast<uint8_t*>(blob.GetBufferPointer());
    assert(destPtr  != nullptr);

    uint8_t* dPtr = destPtr;
    memcpy(dPtr, &tga_header, TGA_HEADER_LEN);
    dPtr += TGA_HEADER_LEN;

    const uint8_t* pPixels = image.pixels;
    assert(pPixels);

    for (size_t y = 0; y < image.height; ++y)
    {
        // Copy pixels
        if (convFlags & CONV_FLAGS_888)
        {
            Copy24bppScanline(dPtr, rowPitch, pPixels, image.rowPitch);
        }
        else if (convFlags & CONV_FLAGS_SWIZZLE)
        {
            SwizzleScanline(dPtr, rowPitch, pPixels, image.rowPitch, image.format, TEXP_SCANLINE_NONE);
        }
        else
        {
            CopyScanline(dPtr, rowPitch, pPixels, image.rowPitch, image.format, TEXP_SCANLINE_NONE);
        }

        dPtr += rowPitch;
        pPixels += image.rowPitch;
    }

    uint32_t extOffset = 0;
    if (metadata)
    {
        // metadata is only used for writing the TGA 2.0 extension header
        auto ext = reinterpret_cast<TGA_EXTENSION*>(dPtr);
        SetExtension(ext, flags, *metadata);

        extOffset = static_cast<uint32_t>(dPtr - destPtr);
        dPtr += sizeof(TGA_EXTENSION);
    }

    // Copy TGA 2.0 footer
    auto footer = reinterpret_cast<TGA_FOOTER*>(dPtr);
    footer->dwDeveloperOffset = 0;
    footer->dwExtensionOffset = extOffset;
    memcpy(footer->Signature, g_Signature, sizeof(g_Signature));

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Save a TGA file to disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::SaveToTGAFile(
    const Image& image,
    TGA_FLAGS flags,
    const wchar_t* szFile,
    const TexMetadata* metadata) noexcept
{
    if (!szFile)
        return E_INVALIDARG;

    if ((flags & (TGA_FLAGS_FORCE_LINEAR | TGA_FLAGS_FORCE_SRGB)) != 0 && !metadata)
        return E_INVALIDARG;

    if (!image.pixels)
        return E_POINTER;

    TGA_HEADER tga_header = {};
    uint32_t convFlags = 0;
    HRESULT hr = EncodeTGAHeader(image, tga_header, convFlags);
    if (FAILED(hr))
        return hr;

    // Create file and write header
#ifdef _WIN32
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(szFile, GENERIC_WRITE, 0,
        CREATE_ALWAYS, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(szFile,
        GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)));
#endif
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    auto_delete_file delonfail(hFile.get());
#else
    std::ofstream outFile(std::filesystem::path(szFile), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!outFile)
        return E_FAIL;
#endif

    // Determine size for TGA pixel data
    size_t rowPitch, slicePitch;
    hr = ComputePitch(image.format, image.width, image.height, rowPitch, slicePitch,
        (convFlags & CONV_FLAGS_888) ? CP_FLAGS_24BPP : CP_FLAGS_NONE);
    if (FAILED(hr))
        return hr;

    if (slicePitch < 65535)
    {
        // For small images, it is better to create an in-memory file and write it out
        Blob blob;

        hr = SaveToTGAMemory(image, flags, blob, metadata);
        if (FAILED(hr))
            return hr;

        // Write blob
    #ifdef _WIN32
        const DWORD bytesToWrite = static_cast<DWORD>(blob.GetBufferSize());
        DWORD bytesWritten;
        if (!WriteFile(hFile.get(), blob.GetBufferPointer(), bytesToWrite, &bytesWritten, nullptr))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (bytesWritten != bytesToWrite)
        {
            return E_FAIL;
        }
    #else
        outFile.write(reinterpret_cast<char*>(blob.GetBufferPointer()),
            static_cast<std::streamsize>(blob.GetBufferSize()));

        if (!outFile)
            return E_FAIL;
    #endif
    }
    else
    {
        // Otherwise, write the image one scanline at a time...
        std::unique_ptr<uint8_t[]> temp(new (std::nothrow) uint8_t[rowPitch]);
        if (!temp)
            return E_OUTOFMEMORY;

        // Write header
    #ifdef _WIN32
        DWORD bytesWritten;
        if (!WriteFile(hFile.get(), &tga_header, TGA_HEADER_LEN, &bytesWritten, nullptr))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (bytesWritten != TGA_HEADER_LEN)
            return E_FAIL;
    #else
        outFile.write(reinterpret_cast<char*>(&tga_header), TGA_HEADER_LEN);
        if (!outFile)
            return E_FAIL;
    #endif

        if (rowPitch > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        // Write pixels
        const uint8_t* pPixels = image.pixels;

        for (size_t y = 0; y < image.height; ++y)
        {
            // Copy pixels
            if (convFlags & CONV_FLAGS_888)
            {
                Copy24bppScanline(temp.get(), rowPitch, pPixels, image.rowPitch);
            }
            else if (convFlags & CONV_FLAGS_SWIZZLE)
            {
                SwizzleScanline(temp.get(), rowPitch, pPixels, image.rowPitch, image.format, TEXP_SCANLINE_NONE);
            }
            else
            {
                CopyScanline(temp.get(), rowPitch, pPixels, image.rowPitch, image.format, TEXP_SCANLINE_NONE);
            }

            pPixels += image.rowPitch;

        #ifdef _WIN32
            if (!WriteFile(hFile.get(), temp.get(), static_cast<DWORD>(rowPitch), &bytesWritten, nullptr))
            {
                return HRESULT_FROM_WIN32(GetLastError());
            }

            if (bytesWritten != rowPitch)
                return E_FAIL;
        #else
            outFile.write(reinterpret_cast<char*>(temp.get()), rowPitch);
            if (!outFile)
                return E_FAIL;
        #endif
        }

        uint32_t extOffset = 0;
        if (metadata)
        {
            // metadata is only used for writing the TGA 2.0 extension header
            TGA_EXTENSION ext = {};
            SetExtension(&ext, flags, *metadata);

        #ifdef _WIN32
            extOffset = SetFilePointer(hFile.get(), 0, nullptr, FILE_CURRENT);
            if (extOffset == INVALID_SET_FILE_POINTER)
            {
                return E_FAIL;
            }

            if (!WriteFile(hFile.get(), &ext, sizeof(TGA_EXTENSION), &bytesWritten, nullptr))
            {
                return HRESULT_FROM_WIN32(GetLastError());
            }

            if (bytesWritten != sizeof(TGA_EXTENSION))
                return E_FAIL;
        #else
            extOffset = static_cast<uint32_t>(outFile.tellp());
            if (!outFile)
                return E_FAIL;

            outFile.write(reinterpret_cast<char*>(&ext), sizeof(TGA_EXTENSION));
            if (!outFile)
                return E_FAIL;
        #endif
        }

        // Write TGA 2.0 footer
        TGA_FOOTER footer = {};
        footer.dwExtensionOffset = extOffset;
        memcpy(footer.Signature, g_Signature, sizeof(g_Signature));

    #ifdef _WIN32
        if (!WriteFile(hFile.get(), &footer, sizeof(TGA_FOOTER), &bytesWritten, nullptr))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (bytesWritten != sizeof(footer))
            return E_FAIL;
    #else
        outFile.write(reinterpret_cast<char*>(&footer), sizeof(TGA_FOOTER));
        if (!outFile)
            return E_FAIL;
    #endif
    }

#ifdef _WIN32
    delonfail.clear();
#endif

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Adapters for /Zc:wchar_t- clients

#if defined(_MSC_VER) && !defined(_NATIVE_WCHAR_T_DEFINED)

namespace DirectX
{
    HRESULT __cdecl GetMetadataFromTGAFile(
        _In_z_ const __wchar_t* szFile,
        _In_ TGA_FLAGS flags,
        _Out_ TexMetadata& metadata) noexcept
    {
        return GetMetadataFromTGAFile(reinterpret_cast<const unsigned short*>(szFile), flags, metadata);
    }

    HRESULT __cdecl LoadFromTGAFile(
        _In_z_ const __wchar_t* szFile,
        _In_ TGA_FLAGS flags,
        _Out_opt_ TexMetadata* metadata,
        _Out_ ScratchImage& image) noexcept
    {
        return LoadFromTGAFile(reinterpret_cast<const unsigned short*>(szFile), flags, metadata, image);
    }

    HRESULT __cdecl SaveToTGAFile(_In_ const Image& image,
        _In_ TGA_FLAGS flags,
        _In_z_ const __wchar_t* szFile,
        _In_opt_ const TexMetadata* metadata) noexcept
    {
        return SaveToTGAFile(image, flags, reinterpret_cast<const unsigned short*>(szFile), metadata);
    }
}

#endif // !_NATIVE_WCHAR_T_DEFINED
