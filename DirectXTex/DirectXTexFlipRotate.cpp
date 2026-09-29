//-------------------------------------------------------------------------------------
// DirectXTexFlipRotate.cpp
//
// DirectX Texture Library - Image flip/rotate operations
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// https://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "DirectXTexP.h"

using namespace DirectX;
using namespace DirectX::Internal;
#ifdef _WIN32
using Microsoft::WRL::ComPtr;
#endif

namespace
{
#ifdef _WIN32
    //-------------------------------------------------------------------------------------
    // Do flip/rotate operation using WIC
    //-------------------------------------------------------------------------------------
    HRESULT
    PerformFlipRotateUsingWIC(const Image& srcImage, TEX_FR_FLAGS flags, const WICPixelFormatGUID& pfGUID, const Image& destImage) noexcept
    {
        if (!srcImage.pixels || !destImage.pixels)
            return E_POINTER;

        assert(srcImage.format == destImage.format);

        bool iswic2 = false;
        auto pWIC   = GetWICFactory(iswic2);
        if (!pWIC)
            return E_NOINTERFACE;

        if (srcImage.rowPitch > UINT32_MAX || srcImage.slicePitch > UINT32_MAX || destImage.rowPitch > UINT32_MAX
            || destImage.slicePitch > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        ComPtr<IWICBitmap> source;
        HRESULT            hr = pWIC->CreateBitmapFromMemory(static_cast<UINT>(srcImage.width),
            static_cast<UINT>(srcImage.height),
            pfGUID,
            static_cast<UINT>(srcImage.rowPitch),
            static_cast<UINT>(srcImage.slicePitch),
            srcImage.pixels,
            source.GetAddressOf());
        if (FAILED(hr))
            return hr;

        ComPtr<IWICBitmapFlipRotator> FR;
        hr = pWIC->CreateBitmapFlipRotator(FR.GetAddressOf());
        if (FAILED(hr))
            return hr;

        hr = FR->Initialize(source.Get(), static_cast<WICBitmapTransformOptions>(flags));
        if (FAILED(hr))
            return hr;

        WICPixelFormatGUID pfFR;
        hr = FR->GetPixelFormat(&pfFR);
        if (FAILED(hr))
            return hr;

        if (memcmp(&pfFR, &pfGUID, sizeof(GUID)) != 0)
        {
            // Flip/rotate should return the same format as the source...
            return HRESULT_E_NOT_SUPPORTED;
        }

        UINT nwidth, nheight;
        hr = FR->GetSize(&nwidth, &nheight);
        if (FAILED(hr))
            return hr;

        if (destImage.width != nwidth || destImage.height != nheight)
            return E_FAIL;

        hr = FR->CopyPixels(nullptr, static_cast<UINT>(destImage.rowPitch), static_cast<UINT>(destImage.slicePitch), destImage.pixels);
        if (FAILED(hr))
            return hr;

        return S_OK;
    }

    //-------------------------------------------------------------------------------------
    // Do conversion, flip/rotate using WIC, conversion cycle
    //
    // For large images we have to use F16 instead of F32 to avoid exceeding the 32-bit
    // memory limitations of WIC.
    //-------------------------------------------------------------------------------------
    HRESULT PerformFlipRotateViaF16(const Image& srcImage, TEX_FR_FLAGS flags, const Image& destImage) noexcept
    {
        if (!srcImage.pixels || !destImage.pixels)
            return E_POINTER;

        assert(srcImage.format != DXGI_FORMAT_R16G16B16A16_FLOAT);
        assert(srcImage.format == destImage.format);

        ScratchImage temp;
        HRESULT      hr = ConvertToR16G16B16A16(srcImage, temp);
        if (FAILED(hr))
            return hr;

        const Image* tsrc = temp.GetImage(0, 0, 0);
        if (!tsrc)
            return E_POINTER;

        ScratchImage rtemp;
        hr = rtemp.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, destImage.width, destImage.height, 1, 1);
        if (FAILED(hr))
            return hr;

        const Image* tdest = rtemp.GetImage(0, 0, 0);
        if (!tdest)
            return E_POINTER;

        hr = PerformFlipRotateUsingWIC(*tsrc, flags, GUID_WICPixelFormat64bppRGBAHalf, *tdest);
        if (FAILED(hr))
            return hr;

        temp.Release();

        hr = ConvertFromR16G16B16A16(*tdest, destImage);
        if (FAILED(hr))
            return hr;

        return S_OK;
    }

    HRESULT PerformFlipRotateViaF32(const Image& srcImage, TEX_FR_FLAGS flags, const Image& destImage) noexcept
    {
        if (!srcImage.pixels || !destImage.pixels)
            return E_POINTER;

        assert(srcImage.format != DXGI_FORMAT_R32G32B32A32_FLOAT);
        assert(srcImage.format == destImage.format);

        ScratchImage temp;
        HRESULT      hr = ConvertToR32G32B32A32(srcImage, temp);
        if (FAILED(hr))
            return hr;

        const Image* tsrc = temp.GetImage(0, 0, 0);
        if (!tsrc)
            return E_POINTER;

        ScratchImage rtemp;
        hr = rtemp.Initialize2D(DXGI_FORMAT_R32G32B32A32_FLOAT, destImage.width, destImage.height, 1, 1);
        if (FAILED(hr))
            return hr;

        const Image* tdest = rtemp.GetImage(0, 0, 0);
        if (!tdest)
            return E_POINTER;

        hr = PerformFlipRotateUsingWIC(*tsrc, flags, GUID_WICPixelFormat128bppRGBAFloat, *tdest);
        if (FAILED(hr))
            return hr;

        temp.Release();

        hr = ConvertFromR32G32B32A32(*tdest, destImage);
        if (FAILED(hr))
            return hr;

        return S_OK;
    }
#else // !WIN32
    //-------------------------------------------------------------------------------------
    // Flip/rotate by moving whole pixels, which needs a fixed number of bytes per pixel
    //-------------------------------------------------------------------------------------
    bool IsSupportedForFlipRotate(DXGI_FORMAT format) noexcept
    {
        if (IsCompressed(format) || IsPacked(format) || IsPlanar(format) || IsPalettized(format))
            return false;

        const size_t bpp = BitsPerPixel(format);
        return (bpp >= 8) && ((bpp % 8) == 0);
    }

    HRESULT PerformFlipRotate(const Image& srcImage, TEX_FR_FLAGS flags, const Image& destImage) noexcept
    {
        if (!srcImage.pixels || !destImage.pixels)
            return E_POINTER;

        assert(srcImage.format == destImage.format);

        const size_t bytesPerPixel = BitsPerPixel(srcImage.format) / 8;
        if (!bytesPerPixel)
            return HRESULT_E_NOT_SUPPORTED;

        // Each destination pixel (x, y) is read from the source pixel (sx, sy), or (sy, sx) when the rotation
        // swaps the axes. The flip flags are applied to the source before the rotation, as WIC does.
        bool swapXY = false;
        bool flipX  = false;
        bool flipY  = false;

        switch (static_cast<int>(flags & (TEX_FR_ROTATE0 | TEX_FR_ROTATE90 | TEX_FR_ROTATE180 | TEX_FR_ROTATE270)))
        {
        case TEX_FR_ROTATE90:
            swapXY = flipX = true;
            break;

        case TEX_FR_ROTATE180:
            flipX = flipY = true;
            break;

        case TEX_FR_ROTATE270:
            swapXY = flipY = true;
            break;

        default:
            break;
        }

        if (flags & TEX_FR_FLIP_HORIZONTAL)
        {
            if (swapXY)
                flipY = !flipY;
            else
                flipX = !flipX;
        }

        if (flags & TEX_FR_FLIP_VERTICAL)
        {
            if (swapXY)
                flipX = !flipX;
            else
                flipY = !flipY;
        }

        const size_t width  = destImage.width;
        const size_t height = destImage.height;

        if (swapXY)
        {
            if (srcImage.width != height || srcImage.height != width)
                return E_FAIL;
        }
        else if (srcImage.width != width || srcImage.height != height)
        {
            return E_FAIL;
        }

        const size_t rowBytes = width * bytesPerPixel;

        for (size_t y = 0; y < height; ++y)
        {
            const size_t sy   = flipY ? (height - 1 - y) : y;
            uint8_t*     pDest = destImage.pixels + y * destImage.rowPitch;

            if (!swapXY && !flipX)
            {
                memcpy(pDest, srcImage.pixels + sy * srcImage.rowPitch, rowBytes);
                continue;
            }

            for (size_t x = 0; x < width; ++x)
            {
                const size_t sx = flipX ? (width - 1 - x) : x;
                const uint8_t* pSrc = swapXY
                    ? srcImage.pixels + sx * srcImage.rowPitch + sy * bytesPerPixel
                    : srcImage.pixels + sy * srcImage.rowPitch + sx * bytesPerPixel;
                memcpy(pDest + x * bytesPerPixel, pSrc, bytesPerPixel);
            }
        }

        return S_OK;
    }
#endif // WIN32
} // namespace

//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Flip/rotate image
//-------------------------------------------------------------------------------------
_Use_decl_annotations_ HRESULT DirectX::FlipRotate(const Image& srcImage, TEX_FR_FLAGS flags, ScratchImage& image) noexcept
{
    if (!srcImage.pixels)
        return E_POINTER;

    if (!flags)
        return E_INVALIDARG;

    if ((srcImage.width > UINT32_MAX) || (srcImage.height > UINT32_MAX))
        return E_INVALIDARG;

    if (IsCompressed(srcImage.format))
    {
        // We don't support flip/rotate operations on compressed images
        return HRESULT_E_NOT_SUPPORTED;
    }

#ifndef _WIN32
    if (!IsSupportedForFlipRotate(srcImage.format))
        return HRESULT_E_NOT_SUPPORTED;
#endif

#ifdef _WIN32
    static_assert(static_cast<int>(TEX_FR_ROTATE0) == static_cast<int>(WICBitmapTransformRotate0), "TEX_FR_ROTATE0 no longer matches WIC");
    static_assert(static_cast<int>(TEX_FR_ROTATE90) == static_cast<int>(WICBitmapTransformRotate90),
        "TEX_FR_ROTATE90 no longer matches WIC");
    static_assert(static_cast<int>(TEX_FR_ROTATE180) == static_cast<int>(WICBitmapTransformRotate180),
        "TEX_FR_ROTATE180 no longer matches WIC");
    static_assert(static_cast<int>(TEX_FR_ROTATE270) == static_cast<int>(WICBitmapTransformRotate270),
        "TEX_FR_ROTATE270 no longer matches WIC");
    static_assert(static_cast<int>(TEX_FR_FLIP_HORIZONTAL) == static_cast<int>(WICBitmapTransformFlipHorizontal),
        "TEX_FR_FLIP_HORIZONTAL no longer matches WIC");
    static_assert(static_cast<int>(TEX_FR_FLIP_VERTICAL) == static_cast<int>(WICBitmapTransformFlipVertical),
        "TEX_FR_FLIP_VERTICAL no longer matches WIC");
#endif

    // Only supports 90, 180, 270, or no rotation flags... not a combination of rotation flags
    const int rotateMode = static_cast<int>(flags & (TEX_FR_ROTATE0 | TEX_FR_ROTATE90 | TEX_FR_ROTATE180 | TEX_FR_ROTATE270));

    switch (rotateMode)
    {
    case 0:
    case TEX_FR_ROTATE90:
    case TEX_FR_ROTATE180:
    case TEX_FR_ROTATE270: break;

    default:               return E_INVALIDARG;
    }

    size_t nwidth  = srcImage.width;
    size_t nheight = srcImage.height;

    if ((rotateMode == TEX_FR_ROTATE90) || (rotateMode == TEX_FR_ROTATE270))
    {
        nwidth  = srcImage.height;
        nheight = srcImage.width;
    }

    HRESULT hr = image.Initialize2D(srcImage.format, nwidth, nheight, 1, 1);
    if (FAILED(hr))
        return hr;

    const Image* rimage = image.GetImage(0, 0, 0);
    if (!rimage)
    {
        image.Release();
        return E_POINTER;
    }

#ifdef _WIN32
    WICPixelFormatGUID pfGUID;
    if (DXGIToWIC(srcImage.format, pfGUID))
    {
        // Case 1: Source format is supported by Windows Imaging Component
        hr = PerformFlipRotateUsingWIC(srcImage, flags, pfGUID, *rimage);
    }
    else
    {
        // Case 2: Source format is not supported by WIC, so we have to convert, flip/rotate, and convert back
        const uint64_t expandedSize = uint64_t(srcImage.width) * uint64_t(srcImage.height) * sizeof(float) * 4;
        if (expandedSize > UINT32_MAX)
        {
            // Image is too large for float32, so have to use float16 instead
            hr = PerformFlipRotateViaF16(srcImage, flags, *rimage);
        }
        else
        {
            hr = PerformFlipRotateViaF32(srcImage, flags, *rimage);
        }
    }
#else
    hr = PerformFlipRotate(srcImage, flags, *rimage);
#endif

    if (FAILED(hr))
    {
        image.Release();
        return hr;
    }

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Flip/rotate image (complex)
//-------------------------------------------------------------------------------------
_Use_decl_annotations_ HRESULT
DirectX::FlipRotate(const Image* srcImages, size_t nimages, const TexMetadata& metadata, TEX_FR_FLAGS flags, ScratchImage& result) noexcept
{
    if (!srcImages || !nimages)
        return E_INVALIDARG;

    if (IsCompressed(metadata.format))
    {
        // We don't support flip/rotate operations on compressed images
        return HRESULT_E_NOT_SUPPORTED;
    }

#ifndef _WIN32
    if (!IsSupportedForFlipRotate(metadata.format))
        return HRESULT_E_NOT_SUPPORTED;
#endif

#ifdef _WIN32
    static_assert(static_cast<int>(TEX_FR_ROTATE0) == static_cast<int>(WICBitmapTransformRotate0), "TEX_FR_ROTATE0 no longer matches WIC");
    static_assert(static_cast<int>(TEX_FR_ROTATE90) == static_cast<int>(WICBitmapTransformRotate90),
        "TEX_FR_ROTATE90 no longer matches WIC");
    static_assert(static_cast<int>(TEX_FR_ROTATE180) == static_cast<int>(WICBitmapTransformRotate180),
        "TEX_FR_ROTATE180 no longer matches WIC");
    static_assert(static_cast<int>(TEX_FR_ROTATE270) == static_cast<int>(WICBitmapTransformRotate270),
        "TEX_FR_ROTATE270 no longer matches WIC");
    static_assert(static_cast<int>(TEX_FR_FLIP_HORIZONTAL) == static_cast<int>(WICBitmapTransformFlipHorizontal),
        "TEX_FR_FLIP_HORIZONTAL no longer matches WIC");
    static_assert(static_cast<int>(TEX_FR_FLIP_VERTICAL) == static_cast<int>(WICBitmapTransformFlipVertical),
        "TEX_FR_FLIP_VERTICAL no longer matches WIC");
#endif

    // Only supports 90, 180, 270, or no rotation flags... not a combination of rotation flags
    const int rotateMode = static_cast<int>(flags & (TEX_FR_ROTATE0 | TEX_FR_ROTATE90 | TEX_FR_ROTATE180 | TEX_FR_ROTATE270));

    switch (rotateMode)
    {
    case 0:
    case TEX_FR_ROTATE90:
    case TEX_FR_ROTATE180:
    case TEX_FR_ROTATE270: break;

    default:               return E_INVALIDARG;
    }

    TexMetadata mdata2 = metadata;

    bool flipwh = false;
    if ((rotateMode == TEX_FR_ROTATE90) || (rotateMode == TEX_FR_ROTATE270))
    {
        flipwh        = true;
        mdata2.width  = metadata.height;
        mdata2.height = metadata.width;
    }

    HRESULT hr = result.Initialize(mdata2);
    if (FAILED(hr))
        return hr;

    if (nimages != result.GetImageCount())
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

#ifdef _WIN32
    WICPixelFormatGUID pfGUID;
    const bool         wicpf = DXGIToWIC(metadata.format, pfGUID);
#endif

    for (size_t index = 0; index < nimages; ++index)
    {
        const Image& src = srcImages[index];
        if (src.format != metadata.format)
        {
            result.Release();
            return E_FAIL;
        }

        if ((src.width > UINT32_MAX) || (src.height > UINT32_MAX))
            return E_FAIL;

        const Image& dst = dest[index];
        assert(dst.format == metadata.format);

        if (flipwh)
        {
            if (src.width != dst.height || src.height != dst.width)
            {
                result.Release();
                return E_FAIL;
            }
        }
        else
        {
            if (src.width != dst.width || src.height != dst.height)
            {
                result.Release();
                return E_FAIL;
            }
        }

#ifdef _WIN32
        if (wicpf)
        {
            // Case 1: Source format is supported by Windows Imaging Component
            hr = PerformFlipRotateUsingWIC(src, flags, pfGUID, dst);
        }
        else
        {
            // Case 2: Source format is not supported by WIC, so we have to convert, flip/rotate, and convert back
            const uint64_t expandedSize = uint64_t(src.width) * uint64_t(src.height) * sizeof(float) * 4;
            if (expandedSize > UINT32_MAX)
            {
                // Image is too large for float32, so have to use float16 instead
                hr = PerformFlipRotateViaF16(src, flags, dst);
            }
            else
            {
                hr = PerformFlipRotateViaF32(src, flags, dst);
            }
        }
#else
        hr = PerformFlipRotate(src, flags, dst);
#endif

        if (FAILED(hr))
        {
            result.Release();
            return hr;
        }
    }

    return S_OK;
}
