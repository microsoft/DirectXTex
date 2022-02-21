//-------------------------------------------------------------------------------------
// DirectXTexMipMaps.cpp
//
// DirectX Texture Library - Mip-map generation
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "DirectXTexP.h"

#include "filters.h"

using namespace DirectX;
using namespace DirectX::Internal;
using Microsoft::WRL::ComPtr;

namespace
{
    constexpr bool ispow2(_In_ size_t x) noexcept
    {
        return ((x != 0) && !(x & (x - 1)));
    }


    size_t CountMips(_In_ size_t width, _In_ size_t height) noexcept
    {
        size_t mipLevels = 1;

        while (height > 1 || width > 1)
        {
            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;

            ++mipLevels;
        }

        return mipLevels;
    }


    size_t CountMips3D(_In_ size_t width, _In_ size_t height, _In_ size_t depth) noexcept
    {
        size_t mipLevels = 1;

        while (height > 1 || width > 1 || depth > 1)
        {
            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;

            if (depth > 1)
                depth >>= 1;

            ++mipLevels;
        }

        return mipLevels;
    }

#ifdef WIN32
    HRESULT EnsureWicBitmapPixelFormat(
        _In_ IWICImagingFactory* pWIC,
        _In_ IWICBitmap* src,
        _In_ TEX_FILTER_FLAGS filter,
        _In_ const WICPixelFormatGUID& desiredPixelFormat,
        _Deref_out_ IWICBitmap** dest) noexcept
    {
        if (!pWIC || !src || !dest)
            return E_POINTER;

        *dest = nullptr;

        WICPixelFormatGUID actualPixelFormat;
        HRESULT hr = src->GetPixelFormat(&actualPixelFormat);

        if (SUCCEEDED(hr))
        {
            if (memcmp(&actualPixelFormat, &desiredPixelFormat, sizeof(WICPixelFormatGUID)) == 0)
            {
                src->AddRef();
                *dest = src;
            }
            else
            {
                ComPtr<IWICFormatConverter> converter;
                hr = pWIC->CreateFormatConverter(converter.GetAddressOf());

                if (SUCCEEDED(hr))
                {
                    BOOL canConvert = FALSE;
                    hr = converter->CanConvert(actualPixelFormat, desiredPixelFormat, &canConvert);
                    if (FAILED(hr) || !canConvert)
                    {
                        return E_UNEXPECTED;
                    }
                }

                if (SUCCEEDED(hr))
                {
                    hr = converter->Initialize(src, desiredPixelFormat, GetWICDither(filter), nullptr,
                        0, WICBitmapPaletteTypeMedianCut);
                }

                if (SUCCEEDED(hr))
                {
                    hr = pWIC->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnDemand, dest);
                }
            }
        }

        return hr;
    }
#endif // WIN32


#if DIRECTX_MATH_VERSION >= 310
#define VectorSum XMVectorSum
#else
    inline XMVECTOR XM_CALLCONV VectorSum
    (
        FXMVECTOR V
    )
    {
        XMVECTOR vTemp = XMVectorSwizzle<2, 3, 0, 1>(V);
        XMVECTOR vTemp2 = XMVectorAdd(V, vTemp);
        vTemp = XMVectorSwizzle<1, 0, 3, 2>(vTemp2);
        return XMVectorAdd(vTemp, vTemp2);
    }
#endif


    HRESULT ScaleAlpha(
        const Image& srcImage,
        float alphaScale,
        const Image& destImage) noexcept
    {
        assert(srcImage.width == destImage.width);
        assert(srcImage.height == destImage.height);

        auto scanline = make_AlignedArrayXMVECTOR(srcImage.width);
        if (!scanline)
        {
            return E_OUTOFMEMORY;
        }

        const uint8_t* pSrc = srcImage.pixels;
        uint8_t* pDest = destImage.pixels;
        if (!pSrc || !pDest)
        {
            return E_POINTER;
        }

        const XMVECTOR vscale = XMVectorReplicate(alphaScale);

        for (size_t h = 0; h < srcImage.height; ++h)
        {
            if (!LoadScanline(scanline.get(), srcImage.width, pSrc, srcImage.rowPitch, srcImage.format))
            {
                return E_FAIL;
            }

            XMVECTOR* ptr = scanline.get();
            for (size_t w = 0; w < srcImage.width; ++w)
            {
                const XMVECTOR v = *ptr;
                const XMVECTOR alpha = XMVectorMultiply(XMVectorSplatW(v), vscale);
                *(ptr++) = XMVectorSelect(alpha, v, g_XMSelect1110);
            }

            if (!StoreScanline(pDest, destImage.rowPitch, destImage.format, scanline.get(), srcImage.width))
            {
                return E_FAIL;
            }

            pSrc += srcImage.rowPitch;
            pDest += destImage.rowPitch;
        }

        return S_OK;
    }


    void GenerateAlphaCoverageConvolutionVectors(
        _In_ size_t N,
        _Out_writes_(N*N) XMVECTOR* vectors) noexcept
    {
        for (size_t sy = 0; sy < N; ++sy)
        {
            const float fy = (float(sy) + 0.5f) / float(N);
            const float ify = 1.0f - fy;

            for (size_t sx = 0; sx < N; ++sx)
            {
                const float fx = (float(sx) + 0.5f) / float(N);
                const float ifx = 1.0f - fx;

                // [0]=(x+0, y+0), [1]=(x+0, y+1), [2]=(x+1, y+0), [3]=(x+1, y+1)
                vectors[sy * N + sx] = XMVectorSet(ifx * ify, ifx * fy, fx * ify, fx * fy);
            }
        }
    }


    HRESULT CalculateAlphaCoverage(
        const Image& srcImage,
        float alphaReference,
        float alphaScale,
        float& coverage) noexcept
    {
        coverage = 0.0f;

        auto row0 = make_AlignedArrayXMVECTOR(srcImage.width);
        if (!row0)
        {
            return E_OUTOFMEMORY;
        }

        auto row1 = make_AlignedArrayXMVECTOR(srcImage.width);
        if (!row1)
        {
            return E_OUTOFMEMORY;
        }

        const XMVECTOR scale = XMVectorReplicate(alphaScale);

        const uint8_t *pSrcRow0 = srcImage.pixels;
        if (!pSrcRow0)
        {
            return E_POINTER;
        }

        constexpr size_t N = 8;
        XMVECTOR convolution[N * N];
        GenerateAlphaCoverageConvolutionVectors(N, convolution);

        size_t coverageCount = 0;
        for (size_t y = 0; y < srcImage.height - 1; ++y)
        {
            if (!LoadScanlineLinear(row0.get(), srcImage.width, pSrcRow0, srcImage.rowPitch, srcImage.format, TEX_FILTER_DEFAULT))
            {
                return E_FAIL;
            }

            const uint8_t *pSrcRow1 = pSrcRow0 + srcImage.rowPitch;
            if (!LoadScanlineLinear(row1.get(), srcImage.width, pSrcRow1, srcImage.rowPitch, srcImage.format, TEX_FILTER_DEFAULT))
            {
                return E_FAIL;
            }

            const XMVECTOR* pRow0 = row0.get();
            const XMVECTOR* pRow1 = row1.get();
            for (size_t x = 0; x < srcImage.width - 1; ++x)
            {
                // [0]=(x+0, y+0), [1]=(x+0, y+1), [2]=(x+1, y+0), [3]=(x+1, y+1)
                XMVECTOR v1 = XMVectorSaturate(XMVectorMultiply(XMVectorSplatW(*pRow0), scale));
                const XMVECTOR v2 = XMVectorSaturate(XMVectorMultiply(XMVectorSplatW(*pRow1), scale));
                XMVECTOR v3 = XMVectorSaturate(XMVectorMultiply(XMVectorSplatW(*(pRow0++)), scale));
                const XMVECTOR v4 = XMVectorSaturate(XMVectorMultiply(XMVectorSplatW(*(pRow1++)), scale));

                v1 = XMVectorMergeXY(v1, v2); // [v1.x v2.x --- ---]
                v3 = XMVectorMergeXY(v3, v4); // [v3.x v4.x --- ---]

                XMVECTOR v = XMVectorPermute<0, 1, 4, 5>(v1, v3); // [v1.x v2.x v3.x v4.x]

                for (size_t sy = 0; sy < N; ++sy)
                {
                    const size_t ry = sy * N;
                    for (size_t sx = 0; sx < N; ++sx)
                    {
                        v = VectorSum(XMVectorMultiply(v, convolution[ry + sx]));
                        if (XMVectorGetX(v) > alphaReference)
                        {
                            ++coverageCount;
                        }
                    }
                }
            }

            pSrcRow0 = pSrcRow1;
        }

        float cscale = static_cast<float>((srcImage.width - 1) * (srcImage.height - 1) * N * N);
        if (cscale > 0.f)
        {
            coverage = static_cast<float>(coverageCount) / cscale;
        }

        return S_OK;
    }


    HRESULT EstimateAlphaScaleForCoverage(
            const Image& srcImage,
            float alphaReference,
            float targetCoverage,
            float& alphaScale) noexcept
    {
        float minAlphaScale = 0.0f;
        float maxAlphaScale = 4.0f;
        float bestError = FLT_MAX;

        // Determine desired scale using a binary search. Hardcoded to 10 steps max.
        alphaScale = 1.0f;
        constexpr size_t N = 10;
        for (size_t i = 0; i < N; ++i)
        {
            float currentCoverage = 0.0f;
            HRESULT hr = CalculateAlphaCoverage(srcImage, alphaReference, alphaScale, currentCoverage);
            if (FAILED(hr))
            {
                return hr;
            }

            const float error = fabsf(currentCoverage - targetCoverage);
            if (error < bestError)
            {
                bestError = error;
            }

            if (currentCoverage < targetCoverage)
            {
                minAlphaScale = alphaScale;
            }
            else if (currentCoverage > targetCoverage)
            {
                maxAlphaScale = alphaScale;
            }
            else
            {
                break;
            }

            alphaScale = (minAlphaScale + maxAlphaScale) * 0.5f;
        }

        return S_OK;
    }
}

_Use_decl_annotations_
bool DirectX::Internal::CalculateMipLevels(
    size_t width,
    size_t height,
    size_t& mipLevels) noexcept
{
    if (mipLevels > 1)
    {
        const size_t maxMips = CountMips(width, height);
        if (mipLevels > maxMips)
            return false;
    }
    else if (mipLevels == 0)
    {
        mipLevels = CountMips(width, height);
    }
    else
    {
        mipLevels = 1;
    }
    return true;
}

_Use_decl_annotations_
bool DirectX::Internal::CalculateMipLevels3D(
    size_t width,
    size_t height,
    size_t depth,
    size_t& mipLevels) noexcept
{
    if (mipLevels > 1)
    {
        const size_t maxMips = CountMips3D(width, height, depth);
        if (mipLevels > maxMips)
            return false;
    }
    else if (mipLevels == 0)
    {
        mipLevels = CountMips3D(width, height, depth);
    }
    else
    {
        mipLevels = 1;
    }
    return true;
}

#ifdef WIN32
//--- Resizing color and alpha channels separately using WIC ---
_Use_decl_annotations_
HRESULT DirectX::Internal::ResizeSeparateColorAndAlpha(
    IWICImagingFactory* pWIC,
    bool iswic2,
    IWICBitmap* original,
    size_t newWidth,
    size_t newHeight,
    TEX_FILTER_FLAGS filter,
    const Image* img) noexcept
{
    if (!pWIC || !original || !img)
        return E_POINTER;

    const WICBitmapInterpolationMode interpolationMode = GetWICInterp(filter);

    WICPixelFormatGUID desiredPixelFormat = GUID_WICPixelFormatUndefined;
    HRESULT hr = original->GetPixelFormat(&desiredPixelFormat);

    size_t colorBytesInPixel = 0;
    size_t colorBytesPerPixel = 0;
    size_t colorWithAlphaBytesPerPixel = 0;
    WICPixelFormatGUID colorPixelFormat = GUID_WICPixelFormatUndefined;
    WICPixelFormatGUID colorWithAlphaPixelFormat = GUID_WICPixelFormatUndefined;

    if (SUCCEEDED(hr))
    {
        ComPtr<IWICComponentInfo> componentInfo;
        hr = pWIC->CreateComponentInfo(desiredPixelFormat, componentInfo.GetAddressOf());

        ComPtr<IWICPixelFormatInfo> pixelFormatInfo;
        if (SUCCEEDED(hr))
        {
            hr = componentInfo.As(&pixelFormatInfo);
        }

        UINT bitsPerPixel = 0;
        if (SUCCEEDED(hr))
        {
            hr = pixelFormatInfo->GetBitsPerPixel(&bitsPerPixel);
        }

        if (SUCCEEDED(hr))
        {
            if (bitsPerPixel <= 32)
            {
                colorBytesInPixel = colorBytesPerPixel = 3;
                colorPixelFormat = GUID_WICPixelFormat24bppBGR;

                colorWithAlphaBytesPerPixel = 4;
                colorWithAlphaPixelFormat = GUID_WICPixelFormat32bppBGRA;
            }
            else
            {
#if(_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
                if (iswic2)
                {
                    colorBytesInPixel = colorBytesPerPixel = 12;
                    colorPixelFormat = GUID_WICPixelFormat96bppRGBFloat;
                }
                else
#else
                UNREFERENCED_PARAMETER(iswic2);
#endif
                {
                    colorBytesInPixel = 12;
                    colorBytesPerPixel = 16;
                    colorPixelFormat = GUID_WICPixelFormat128bppRGBFloat;
                }

                colorWithAlphaBytesPerPixel = 16;
                colorWithAlphaPixelFormat = GUID_WICPixelFormat128bppRGBAFloat;
            }
        }
    }

    // Resize color only image (no alpha channel)
    ComPtr<IWICBitmap> resizedColor;
    if (SUCCEEDED(hr))
    {
        ComPtr<IWICBitmapScaler> colorScaler;
        hr = pWIC->CreateBitmapScaler(colorScaler.GetAddressOf());
        if (SUCCEEDED(hr))
        {
            ComPtr<IWICBitmap> converted;
            hr = EnsureWicBitmapPixelFormat(pWIC, original, filter, colorPixelFormat, converted.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                hr = colorScaler->Initialize(converted.Get(), static_cast<UINT>(newWidth), static_cast<UINT>(newHeight), interpolationMode);
            }
        }

        if (SUCCEEDED(hr))
        {
            ComPtr<IWICBitmap> resized;
            hr = pWIC->CreateBitmapFromSource(colorScaler.Get(), WICBitmapCacheOnDemand, resized.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                hr = EnsureWicBitmapPixelFormat(pWIC, resized.Get(), filter, colorPixelFormat, resizedColor.GetAddressOf());
            }
        }
    }

    // Resize color+alpha image
    ComPtr<IWICBitmap> resizedColorWithAlpha;
    if (SUCCEEDED(hr))
    {
        ComPtr<IWICBitmapScaler> colorWithAlphaScaler;
        hr = pWIC->CreateBitmapScaler(colorWithAlphaScaler.GetAddressOf());
        if (SUCCEEDED(hr))
        {
            ComPtr<IWICBitmap> converted;
            hr = EnsureWicBitmapPixelFormat(pWIC, original, filter, colorWithAlphaPixelFormat, converted.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                hr = colorWithAlphaScaler->Initialize(converted.Get(), static_cast<UINT>(newWidth), static_cast<UINT>(newHeight), interpolationMode);
            }
        }

        if (SUCCEEDED(hr))
        {
            ComPtr<IWICBitmap> resized;
            hr = pWIC->CreateBitmapFromSource(colorWithAlphaScaler.Get(), WICBitmapCacheOnDemand, resized.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                hr = EnsureWicBitmapPixelFormat(pWIC, resized.Get(), filter, colorWithAlphaPixelFormat, resizedColorWithAlpha.GetAddressOf());
            }
        }
    }

    // Merge pixels (copying color channels from color only image to color+alpha image)
    if (SUCCEEDED(hr))
    {
        ComPtr<IWICBitmapLock> colorLock;
        ComPtr<IWICBitmapLock> colorWithAlphaLock;
        hr = resizedColor->Lock(nullptr, WICBitmapLockRead, colorLock.GetAddressOf());
        if (SUCCEEDED(hr))
        {
            hr = resizedColorWithAlpha->Lock(nullptr, WICBitmapLockWrite, colorWithAlphaLock.GetAddressOf());
        }

        if (SUCCEEDED(hr))
        {
            WICInProcPointer colorWithAlphaData = nullptr;
            UINT colorWithAlphaSizeInBytes = 0;
            UINT colorWithAlphaStride = 0;

            hr = colorWithAlphaLock->GetDataPointer(&colorWithAlphaSizeInBytes, &colorWithAlphaData);
            if (SUCCEEDED(hr))
            {
                if (!colorWithAlphaData)
                {
                    hr = E_POINTER;
                }
                else
                {
                    hr = colorWithAlphaLock->GetStride(&colorWithAlphaStride);
                }
            }

            WICInProcPointer colorData = nullptr;
            UINT colorSizeInBytes = 0;
            UINT colorStride = 0;
            if (SUCCEEDED(hr))
            {
                hr = colorLock->GetDataPointer(&colorSizeInBytes, &colorData);
                if (SUCCEEDED(hr))
                {
                    if (!colorData)
                    {
                        hr = E_POINTER;
                    }
                    else
                    {
                        hr = colorLock->GetStride(&colorStride);
                    }
                }
            }

            for (size_t j = 0; SUCCEEDED(hr) && j < newHeight; j++)
            {
                for (size_t i = 0; SUCCEEDED(hr) && i < newWidth; i++)
                {
                    size_t colorWithAlphaIndex = (j * colorWithAlphaStride) + (i * colorWithAlphaBytesPerPixel);
                    const size_t colorIndex = (j * colorStride) + (i * colorBytesPerPixel);

                    if (((colorWithAlphaIndex + colorBytesInPixel) > colorWithAlphaSizeInBytes)
                        || ((colorIndex + colorBytesPerPixel) > colorSizeInBytes))
                    {
                        hr = E_INVALIDARG;
                    }
                    else
                    {
#pragma warning( suppress : 26014 6386 ) // No overflow possible here
                        memcpy_s(colorWithAlphaData + colorWithAlphaIndex, colorWithAlphaBytesPerPixel, colorData + colorIndex, colorBytesInPixel);
                    }
                }
            }
        }
    }

    if (SUCCEEDED(hr))
    {
        if (img->rowPitch > UINT32_MAX || img->slicePitch > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        ComPtr<IWICBitmap> wicBitmap;
        hr = EnsureWicBitmapPixelFormat(pWIC, resizedColorWithAlpha.Get(), filter, desiredPixelFormat, wicBitmap.GetAddressOf());
        if (SUCCEEDED(hr))
        {
            hr = wicBitmap->CopyPixels(nullptr, static_cast<UINT>(img->rowPitch), static_cast<UINT>(img->slicePitch), img->pixels);
        }
    }

    return hr;
}
#endif // WIN32

namespace
{
#ifdef WIN32
    //--- determine when to use WIC vs. non-WIC paths ---
    bool UseWICFiltering(_In_ DXGI_FORMAT format, _In_ TEX_FILTER_FLAGS filter) noexcept
    {
        if (filter & TEX_FILTER_FORCE_NON_WIC)
        {
            // Explicit flag indicates use of non-WIC code paths
            return false;
        }

        if (filter & TEX_FILTER_FORCE_WIC)
        {
            // Explicit flag to use WIC code paths, skips all the case checks below
            return true;
        }

        if (IsSRGB(format) || (filter & TEX_FILTER_SRGB))
        {
            // Use non-WIC code paths for sRGB correct filtering
            return false;
        }

#if (defined(_XBOX_ONE) && defined(_TITLE)) || defined(_GAMING_XBOX)
        if (format == DXGI_FORMAT_R16G16B16A16_FLOAT
            || format == DXGI_FORMAT_R16_FLOAT)
        {
            // Use non-WIC code paths as these conversions are not supported by Xbox version of WIC
            return false;
        }
#endif

        static_assert(TEX_FILTER_POINT == 0x100000, "TEX_FILTER_ flag values don't match TEX_FILTER_MODE_MASK");

        switch (filter & TEX_FILTER_MODE_MASK)
        {
        case TEX_FILTER_LINEAR:
            if (filter & TEX_FILTER_WRAP)
            {
                // WIC only supports 'clamp' semantics (MIRROR is equivalent to clamp for linear)
                return false;
            }

            if (BitsPerColor(format) > 8)
            {
                // Avoid the WIC bitmap scaler when doing Linear filtering of XR/HDR formats
                return false;
            }
            break;

        case TEX_FILTER_CUBIC:
            if (filter & (TEX_FILTER_WRAP | TEX_FILTER_MIRROR))
            {
                // WIC only supports 'clamp' semantics
                return false;
            }

            if (BitsPerColor(format) > 8)
            {
                // Avoid the WIC bitmap scaler when doing Cubic filtering of XR/HDR formats
                return false;
            }
            break;

        case TEX_FILTER_TRIANGLE:
            // WIC does not implement this filter
            return false;
        }

        return true;
    }


    //--- mipmap (1D/2D) generation using WIC image scalar ---
    HRESULT GenerateMipMapsUsingWIC(
        _In_ const Image& baseImage,
        _In_ TEX_FILTER_FLAGS filter,
        _In_ size_t levels,
        _In_ const WICPixelFormatGUID& pfGUID,
        _In_ const ScratchImage& mipChain,
        _In_ size_t item) noexcept
    {
        assert(levels > 1);

        if (!baseImage.pixels || !mipChain.GetPixels())
            return E_POINTER;

        bool iswic2 = false;
        auto pWIC = GetWICFactory(iswic2);
        if (!pWIC)
            return E_NOINTERFACE;

        size_t width = baseImage.width;
        size_t height = baseImage.height;

        if (baseImage.rowPitch > UINT32_MAX || baseImage.slicePitch > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        ComPtr<IWICBitmap> source;
        HRESULT hr = pWIC->CreateBitmapFromMemory(static_cast<UINT>(width), static_cast<UINT>(height), pfGUID,
            static_cast<UINT>(baseImage.rowPitch), static_cast<UINT>(baseImage.slicePitch),
            baseImage.pixels, source.GetAddressOf());
        if (FAILED(hr))
            return hr;

        // Copy base image to top miplevel
        const Image *img0 = mipChain.GetImage(0, item, 0);
        if (!img0)
            return E_POINTER;

        uint8_t* pDest = img0->pixels;
        if (!pDest)
            return E_POINTER;

        const uint8_t *pSrc = baseImage.pixels;
        for (size_t h = 0; h < height; ++h)
        {
            const size_t msize = std::min<size_t>(img0->rowPitch, baseImage.rowPitch);
            memcpy_s(pDest, img0->rowPitch, pSrc, msize);
            pSrc += baseImage.rowPitch;
            pDest += img0->rowPitch;
        }

        ComPtr<IWICComponentInfo> componentInfo;
        hr = pWIC->CreateComponentInfo(pfGUID, componentInfo.GetAddressOf());
        if (FAILED(hr))
            return hr;

        ComPtr<IWICPixelFormatInfo2> pixelFormatInfo;
        hr = componentInfo.As(&pixelFormatInfo);
        if (FAILED(hr))
            return hr;

        BOOL supportsTransparency = FALSE;
        hr = pixelFormatInfo->SupportsTransparency(&supportsTransparency);
        if (FAILED(hr))
            return hr;

        // Resize base image to each target mip level
        for (size_t level = 1; level < levels; ++level)
        {
            const Image *img = mipChain.GetImage(level, item, 0);
            if (!img)
                return E_POINTER;

            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;

            assert(img->width == width && img->height == height && img->format == baseImage.format);

            if ((filter & TEX_FILTER_SEPARATE_ALPHA) && supportsTransparency)
            {
                hr = ResizeSeparateColorAndAlpha(pWIC, iswic2, source.Get(), width, height, filter, img);
                if (FAILED(hr))
                    return hr;
            }
            else
            {
                ComPtr<IWICBitmapScaler> scaler;
                hr = pWIC->CreateBitmapScaler(scaler.GetAddressOf());
                if (FAILED(hr))
                    return hr;

                if (img->rowPitch > UINT32_MAX || img->slicePitch > UINT32_MAX)
                    return HRESULT_E_ARITHMETIC_OVERFLOW;

                hr = scaler->Initialize(source.Get(),
                    static_cast<UINT>(width), static_cast<UINT>(height),
                    GetWICInterp(filter));
                if (FAILED(hr))
                    return hr;

                WICPixelFormatGUID pfScaler;
                hr = scaler->GetPixelFormat(&pfScaler);
                if (FAILED(hr))
                    return hr;

                if (memcmp(&pfScaler, &pfGUID, sizeof(WICPixelFormatGUID)) == 0)
                {
                    hr = scaler->CopyPixels(nullptr, static_cast<UINT>(img->rowPitch), static_cast<UINT>(img->slicePitch), img->pixels);
                    if (FAILED(hr))
                        return hr;
                }
                else
                {
                    // The WIC bitmap scaler is free to return a different pixel format than the source image, so here we
                    // convert it back
                    ComPtr<IWICFormatConverter> FC;
                    hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
                    if (FAILED(hr))
                        return hr;

                    BOOL canConvert = FALSE;
                    hr = FC->CanConvert(pfScaler, pfGUID, &canConvert);
                    if (FAILED(hr) || !canConvert)
                    {
                        return E_UNEXPECTED;
                    }

                    hr = FC->Initialize(scaler.Get(), pfGUID, GetWICDither(filter), nullptr,
                        0, WICBitmapPaletteTypeMedianCut);
                    if (FAILED(hr))
                        return hr;

                    hr = FC->CopyPixels(nullptr, static_cast<UINT>(img->rowPitch), static_cast<UINT>(img->slicePitch), img->pixels);
                    if (FAILED(hr))
                        return hr;
                }
            }
        }

        return S_OK;
    }
#endif // WIN32


    //-------------------------------------------------------------------------------------
    // Generate (1D/2D) mip-map helpers (custom filtering)
    //-------------------------------------------------------------------------------------
    HRESULT Setup2DMips(
        _In_reads_(nimages) const Image* baseImages,
        _In_ size_t nimages,
        _In_ const TexMetadata& mdata,
        _Out_ ScratchImage& mipChain) noexcept
    {
        if (!baseImages || !nimages)
            return E_INVALIDARG;

        assert(mdata.mipLevels > 1);
        assert(mdata.arraySize == nimages);
        assert(mdata.depth == 1 && mdata.dimension != TEX_DIMENSION_TEXTURE3D);
        assert(mdata.width == baseImages[0].width);
        assert(mdata.height == baseImages[0].height);
        assert(mdata.format == baseImages[0].format);

        HRESULT hr = mipChain.Initialize(mdata);
        if (FAILED(hr))
            return hr;

        // Copy base image(s) to top of mip chain
        for (size_t item = 0; item < nimages; ++item)
        {
            const Image& src = baseImages[item];

            const Image *dest = mipChain.GetImage(0, item, 0);
            if (!dest)
            {
                mipChain.Release();
                return E_POINTER;
            }

            assert(src.format == dest->format);

            uint8_t* pDest = dest->pixels;
            if (!pDest)
            {
                mipChain.Release();
                return E_POINTER;
            }

            const uint8_t *pSrc = src.pixels;
            const size_t rowPitch = src.rowPitch;
            for (size_t h = 0; h < mdata.height; ++h)
            {
                const size_t msize = std::min<size_t>(dest->rowPitch, rowPitch);
                memcpy(pDest, pSrc, msize);
                pSrc += rowPitch;
                pDest += dest->rowPitch;
            }
        }

        return S_OK;
    }

    //--- 2D Point Filter ---
    HRESULT Generate2DMipsPointFilter(size_t levels, const ScratchImage& mipChain, size_t item) noexcept
    {
        if (!mipChain.GetImages())
            return E_INVALIDARG;

        // This assumes that the base image is already placed into the mipChain at the top level... (see _Setup2DMips)

        assert(levels > 1);

        size_t width = mipChain.GetMetadata().width;
        size_t height = mipChain.GetMetadata().height;

        // Allocate temporary space (2 scanlines)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(width) * 2);
        if (!scanline)
            return E_OUTOFMEMORY;

        XMVECTOR* target = scanline.get();

        XMVECTOR* row = target + width;

        // Resize base image to each target mip level
        for (size_t level = 1; level < levels; ++level)
        {
#ifdef _DEBUG
            memset(row, 0xCD, sizeof(XMVECTOR)*width);
#endif

            // 2D point filter
            const Image* src = mipChain.GetImage(level - 1, item, 0);
            const Image* dest = mipChain.GetImage(level, item, 0);

            if (!src || !dest)
                return E_POINTER;

            const uint8_t* pSrc = src->pixels;
            uint8_t* pDest = dest->pixels;

            const size_t rowPitch = src->rowPitch;

            const size_t nwidth = (width > 1) ? (width >> 1) : 1;
            const size_t nheight = (height > 1) ? (height >> 1) : 1;

            const size_t xinc = (width << 16) / nwidth;
            const size_t yinc = (height << 16) / nheight;

            size_t lasty = size_t(-1);

            size_t sy = 0;
            for (size_t y = 0; y < nheight; ++y)
            {
                if ((lasty ^ sy) >> 16)
                {
                    if (!LoadScanline(row, width, pSrc + (rowPitch * (sy >> 16)), rowPitch, src->format))
                        return E_FAIL;
                    lasty = sy;
                }

                size_t sx = 0;
                for (size_t x = 0; x < nwidth; ++x)
                {
                    target[x] = row[sx >> 16];
                    sx += xinc;
                }

                if (!StoreScanline(pDest, dest->rowPitch, dest->format, target, nwidth))
                    return E_FAIL;
                pDest += dest->rowPitch;

                sy += yinc;
            }

            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;
        }

        return S_OK;
    }


    //--- 2D Box Filter ---
    HRESULT Generate2DMipsBoxFilter(size_t levels, TEX_FILTER_FLAGS filter, const ScratchImage& mipChain, size_t item) noexcept
    {
        using namespace DirectX::Filters;

        if (!mipChain.GetImages())
            return E_INVALIDARG;

        // This assumes that the base image is already placed into the mipChain at the top level... (see _Setup2DMips)

        assert(levels > 1);

        size_t width = mipChain.GetMetadata().width;
        size_t height = mipChain.GetMetadata().height;

        if (!ispow2(width) || !ispow2(height))
            return E_FAIL;

        // Allocate temporary space (3 scanlines)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(width) * 3);
        if (!scanline)
            return E_OUTOFMEMORY;

        XMVECTOR* target = scanline.get();

        XMVECTOR* urow0 = target + width;
        XMVECTOR* urow1 = target + width * 2;

        const XMVECTOR* urow2 = urow0 + 1;
        const XMVECTOR* urow3 = urow1 + 1;

        // Resize base image to each target mip level
        for (size_t level = 1; level < levels; ++level)
        {
            if (height <= 1)
            {
                urow1 = urow0;
            }

            if (width <= 1)
            {
                urow2 = urow0;
                urow3 = urow1;
            }

            // 2D box filter
            const Image* src = mipChain.GetImage(level - 1, item, 0);
            const Image* dest = mipChain.GetImage(level, item, 0);

            if (!src || !dest)
                return E_POINTER;

            const uint8_t* pSrc = src->pixels;
            uint8_t* pDest = dest->pixels;

            const size_t rowPitch = src->rowPitch;

            const size_t nwidth = (width > 1) ? (width >> 1) : 1;
            const size_t nheight = (height > 1) ? (height >> 1) : 1;

            for (size_t y = 0; y < nheight; ++y)
            {
                if (!LoadScanlineLinear(urow0, width, pSrc, rowPitch, src->format, filter))
                    return E_FAIL;
                pSrc += rowPitch;

                if (urow0 != urow1)
                {
                    if (!LoadScanlineLinear(urow1, width, pSrc, rowPitch, src->format, filter))
                        return E_FAIL;
                    pSrc += rowPitch;
                }

                for (size_t x = 0; x < nwidth; ++x)
                {
                    const size_t x2 = x << 1;

                    AVERAGE4(target[x], urow0[x2], urow1[x2], urow2[x2], urow3[x2])
                }

                if (!StoreScanlineLinear(pDest, dest->rowPitch, dest->format, target, nwidth, filter))
                    return E_FAIL;
                pDest += dest->rowPitch;
            }

            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;
        }

        return S_OK;
    }


    //--- 2D Linear Filter ---
    HRESULT Generate2DMipsLinearFilter(size_t levels, TEX_FILTER_FLAGS filter, const ScratchImage& mipChain, size_t item) noexcept
    {
        using namespace DirectX::Filters;

        if (!mipChain.GetImages())
            return E_INVALIDARG;

        // This assumes that the base image is already placed into the mipChain at the top level... (see _Setup2DMips)

        assert(levels > 1);

        size_t width = mipChain.GetMetadata().width;
        size_t height = mipChain.GetMetadata().height;

        // Allocate temporary space (3 scanlines, plus X and Y filters)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(width) * 3);
        if (!scanline)
            return E_OUTOFMEMORY;

        std::unique_ptr<LinearFilter[]> lf(new (std::nothrow) LinearFilter[width + height]);
        if (!lf)
            return E_OUTOFMEMORY;

        LinearFilter* lfX = lf.get();
        LinearFilter* lfY = lf.get() + width;

        XMVECTOR* target = scanline.get();

        XMVECTOR* row0 = target + width;
        XMVECTOR* row1 = target + width * 2;

        // Resize base image to each target mip level
        for (size_t level = 1; level < levels; ++level)
        {
            // 2D linear filter
            const Image* src = mipChain.GetImage(level - 1, item, 0);
            const Image* dest = mipChain.GetImage(level, item, 0);

            if (!src || !dest)
                return E_POINTER;

            const uint8_t* pSrc = src->pixels;
            uint8_t* pDest = dest->pixels;

            const size_t rowPitch = src->rowPitch;

            const size_t nwidth = (width > 1) ? (width >> 1) : 1;
            CreateLinearFilter(width, nwidth, (filter & TEX_FILTER_WRAP_U) != 0, lfX);

            const size_t nheight = (height > 1) ? (height >> 1) : 1;
            CreateLinearFilter(height, nheight, (filter & TEX_FILTER_WRAP_V) != 0, lfY);

#ifdef _DEBUG
            memset(row0, 0xCD, sizeof(XMVECTOR)*width);
            memset(row1, 0xDD, sizeof(XMVECTOR)*width);
#endif

            size_t u0 = size_t(-1);
            size_t u1 = size_t(-1);

            for (size_t y = 0; y < nheight; ++y)
            {
                auto const& toY = lfY[y];

                if (toY.u0 != u0)
                {
                    if (toY.u0 != u1)
                    {
                        u0 = toY.u0;

                        if (!LoadScanlineLinear(row0, width, pSrc + (rowPitch * u0), rowPitch, src->format, filter))
                            return E_FAIL;
                    }
                    else
                    {
                        u0 = u1;
                        u1 = size_t(-1);

                        std::swap(row0, row1);
                    }
                }

                if (toY.u1 != u1)
                {
                    u1 = toY.u1;

                    if (!LoadScanlineLinear(row1, width, pSrc + (rowPitch * u1), rowPitch, src->format, filter))
                        return E_FAIL;
                }

                for (size_t x = 0; x < nwidth; ++x)
                {
                    auto const& toX = lfX[x];

                    BILINEAR_INTERPOLATE(target[x], toX, toY, row0, row1)
                }

                if (!StoreScanlineLinear(pDest, dest->rowPitch, dest->format, target, nwidth, filter))
                    return E_FAIL;
                pDest += dest->rowPitch;
            }

            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;
        }

        return S_OK;
    }

    //--- 2D Cubic Filter ---
    HRESULT Generate2DMipsCubicFilter(size_t levels, TEX_FILTER_FLAGS filter, const ScratchImage& mipChain, size_t item) noexcept
    {
        using namespace DirectX::Filters;

        if (!mipChain.GetImages())
            return E_INVALIDARG;

        // This assumes that the base image is already placed into the mipChain at the top level... (see _Setup2DMips)

        assert(levels > 1);

        size_t width = mipChain.GetMetadata().width;
        size_t height = mipChain.GetMetadata().height;

        // Allocate temporary space (5 scanlines, plus X and Y filters)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(width) * 5);
        if (!scanline)
            return E_OUTOFMEMORY;

        std::unique_ptr<CubicFilter[]> cf(new (std::nothrow) CubicFilter[width + height]);
        if (!cf)
            return E_OUTOFMEMORY;

        CubicFilter* cfX = cf.get();
        CubicFilter* cfY = cf.get() + width;

        XMVECTOR* target = scanline.get();

        XMVECTOR* row0 = target + width;
        XMVECTOR* row1 = target + width * 2;
        XMVECTOR* row2 = target + width * 3;
        XMVECTOR* row3 = target + width * 4;

        // Resize base image to each target mip level
        for (size_t level = 1; level < levels; ++level)
        {
            // 2D cubic filter
            const Image* src = mipChain.GetImage(level - 1, item, 0);
            const Image* dest = mipChain.GetImage(level, item, 0);

            if (!src || !dest)
                return E_POINTER;

            const uint8_t* pSrc = src->pixels;
            uint8_t* pDest = dest->pixels;

            const size_t rowPitch = src->rowPitch;

            const size_t nwidth = (width > 1) ? (width >> 1) : 1;
            CreateCubicFilter(width, nwidth, (filter & TEX_FILTER_WRAP_U) != 0, (filter & TEX_FILTER_MIRROR_U) != 0, cfX);

            const size_t nheight = (height > 1) ? (height >> 1) : 1;
            CreateCubicFilter(height, nheight, (filter & TEX_FILTER_WRAP_V) != 0, (filter & TEX_FILTER_MIRROR_V) != 0, cfY);

#ifdef _DEBUG
            memset(row0, 0xCD, sizeof(XMVECTOR)*width);
            memset(row1, 0xDD, sizeof(XMVECTOR)*width);
            memset(row2, 0xED, sizeof(XMVECTOR)*width);
            memset(row3, 0xFD, sizeof(XMVECTOR)*width);
#endif

            size_t u0 = size_t(-1);
            size_t u1 = size_t(-1);
            size_t u2 = size_t(-1);
            size_t u3 = size_t(-1);

            for (size_t y = 0; y < nheight; ++y)
            {
                auto const& toY = cfY[y];

                // Scanline 1
                if (toY.u0 != u0)
                {
                    if (toY.u0 != u1 && toY.u0 != u2 && toY.u0 != u3)
                    {
                        u0 = toY.u0;

                        if (!LoadScanlineLinear(row0, width, pSrc + (rowPitch * u0), rowPitch, src->format, filter))
                            return E_FAIL;
                    }
                    else if (toY.u0 == u1)
                    {
                        u0 = u1;
                        u1 = size_t(-1);

                        std::swap(row0, row1);
                    }
                    else if (toY.u0 == u2)
                    {
                        u0 = u2;
                        u2 = size_t(-1);

                        std::swap(row0, row2);
                    }
                    else if (toY.u0 == u3)
                    {
                        u0 = u3;
                        u3 = size_t(-1);

                        std::swap(row0, row3);
                    }
                }

                // Scanline 2
                if (toY.u1 != u1)
                {
                    if (toY.u1 != u2 && toY.u1 != u3)
                    {
                        u1 = toY.u1;

                        if (!LoadScanlineLinear(row1, width, pSrc + (rowPitch * u1), rowPitch, src->format, filter))
                            return E_FAIL;
                    }
                    else if (toY.u1 == u2)
                    {
                        u1 = u2;
                        u2 = size_t(-1);

                        std::swap(row1, row2);
                    }
                    else if (toY.u1 == u3)
                    {
                        u1 = u3;
                        u3 = size_t(-1);

                        std::swap(row1, row3);
                    }
                }

                // Scanline 3
                if (toY.u2 != u2)
                {
                    if (toY.u2 != u3)
                    {
                        u2 = toY.u2;

                        if (!LoadScanlineLinear(row2, width, pSrc + (rowPitch * u2), rowPitch, src->format, filter))
                            return E_FAIL;
                    }
                    else
                    {
                        u2 = u3;
                        u3 = size_t(-1);

                        std::swap(row2, row3);
                    }
                }

                // Scanline 4
                if (toY.u3 != u3)
                {
                    u3 = toY.u3;

                    if (!LoadScanlineLinear(row3, width, pSrc + (rowPitch * u3), rowPitch, src->format, filter))
                        return E_FAIL;
                }

                for (size_t x = 0; x < nwidth; ++x)
                {
                    auto const& toX = cfX[x];

                    XMVECTOR C0, C1, C2, C3;

                    CUBIC_INTERPOLATE(C0, toX.x, row0[toX.u0], row0[toX.u1], row0[toX.u2], row0[toX.u3])
                    CUBIC_INTERPOLATE(C1, toX.x, row1[toX.u0], row1[toX.u1], row1[toX.u2], row1[toX.u3])
                    CUBIC_INTERPOLATE(C2, toX.x, row2[toX.u0], row2[toX.u1], row2[toX.u2], row2[toX.u3])
                    CUBIC_INTERPOLATE(C3, toX.x, row3[toX.u0], row3[toX.u1], row3[toX.u2], row3[toX.u3])

                    CUBIC_INTERPOLATE(target[x], toY.x, C0, C1, C2, C3)
                }

                if (!StoreScanlineLinear(pDest, dest->rowPitch, dest->format, target, nwidth, filter))
                    return E_FAIL;
                pDest += dest->rowPitch;
            }

            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;
        }

        return S_OK;
    }


    //--- 2D Triangle Filter ---
    HRESULT Generate2DMipsTriangleFilter(size_t levels, TEX_FILTER_FLAGS filter, const ScratchImage& mipChain, size_t item) noexcept
    {
        using namespace DirectX::Filters;

        if (!mipChain.GetImages())
            return E_INVALIDARG;

        // This assumes that the base image is already placed into the mipChain at the top level... (see _Setup2DMips)

        assert(levels > 1);

        size_t width = mipChain.GetMetadata().width;
        size_t height = mipChain.GetMetadata().height;

        // Allocate initial temporary space (1 scanline, accumulation rows, plus X and Y filters)
        auto scanline = make_AlignedArrayXMVECTOR(width);
        if (!scanline)
            return E_OUTOFMEMORY;

        std::unique_ptr<TriangleRow[]> rowActive(new (std::nothrow) TriangleRow[height]);
        if (!rowActive)
            return E_OUTOFMEMORY;

        TriangleRow * rowFree = nullptr;

        std::unique_ptr<Filter> tfX, tfY;

        XMVECTOR* row = scanline.get();

        // Resize base image to each target mip level
        for (size_t level = 1; level < levels; ++level)
        {
            // 2D triangle filter
            const Image* src = mipChain.GetImage(level - 1, item, 0);
            const Image* dest = mipChain.GetImage(level, item, 0);

            if (!src || !dest)
                return E_POINTER;

            const uint8_t* pSrc = src->pixels;
            const size_t rowPitch = src->rowPitch;
            const uint8_t* pEndSrc = pSrc + rowPitch * height;

            uint8_t* pDest = dest->pixels;

            const size_t nwidth = (width > 1) ? (width >> 1) : 1;
            HRESULT hr = CreateTriangleFilter(width, nwidth, (filter & TEX_FILTER_WRAP_U) != 0, tfX);
            if (FAILED(hr))
                return hr;

            const size_t nheight = (height > 1) ? (height >> 1) : 1;
            hr = CreateTriangleFilter(height, nheight, (filter & TEX_FILTER_WRAP_V) != 0, tfY);
            if (FAILED(hr))
                return hr;

#ifdef _DEBUG
            memset(row, 0xCD, sizeof(XMVECTOR)*width);
#endif

            auto xFromEnd = reinterpret_cast<const FilterFrom*>(reinterpret_cast<const uint8_t*>(tfX.get()) + tfX->sizeInBytes);
            auto yFromEnd = reinterpret_cast<const FilterFrom*>(reinterpret_cast<const uint8_t*>(tfY.get()) + tfY->sizeInBytes);

            // Count times rows get written (and clear out any leftover accumulation rows from last miplevel)
            for (FilterFrom* yFrom = tfY->from; yFrom < yFromEnd; )
            {
                for (size_t j = 0; j < yFrom->count; ++j)
                {
                    const size_t v = yFrom->to[j].u;
                    assert(v < nheight);
                    TriangleRow* rowAcc = &rowActive[v];

                    ++rowAcc->remaining;

                    if (rowAcc->scanline)
                    {
                        memset(rowAcc->scanline.get(), 0, sizeof(XMVECTOR) * nwidth);
                    }
                }

                yFrom = reinterpret_cast<FilterFrom*>(reinterpret_cast<uint8_t*>(yFrom) + yFrom->sizeInBytes);
            }

            // Filter image
            for (FilterFrom* yFrom = tfY->from; yFrom < yFromEnd; )
            {
                // Create accumulation rows as needed
                for (size_t j = 0; j < yFrom->count; ++j)
                {
                    const size_t v = yFrom->to[j].u;
                    assert(v < nheight);
                    TriangleRow* rowAcc = &rowActive[v];

                    if (!rowAcc->scanline)
                    {
                        if (rowFree)
                        {
                            // Steal and reuse scanline from 'free row' list
                            // (it will always be at least as wide as nwidth due to loop decending order)
                            assert(rowFree->scanline != nullptr);
                            rowAcc->scanline.reset(rowFree->scanline.release());
                            rowFree = rowFree->next;
                        }
                        else
                        {
                            auto nscanline = make_AlignedArrayXMVECTOR(nwidth);
                            if (!nscanline)
                                return E_OUTOFMEMORY;
                            rowAcc->scanline.swap(nscanline);
                        }

                        memset(rowAcc->scanline.get(), 0, sizeof(XMVECTOR) * nwidth);
                    }
                }

                // Load source scanline
                if ((pSrc + rowPitch) > pEndSrc)
                    return E_FAIL;

                if (!LoadScanlineLinear(row, width, pSrc, rowPitch, src->format, filter))
                    return E_FAIL;

                pSrc += rowPitch;

                // Process row
                size_t x = 0;
                for (FilterFrom* xFrom = tfX->from; xFrom < xFromEnd; ++x)
                {
                    for (size_t j = 0; j < yFrom->count; ++j)
                    {
                        const size_t v = yFrom->to[j].u;
                        assert(v < nheight);
                        const float yweight = yFrom->to[j].weight;

                        XMVECTOR* accPtr = rowActive[v].scanline.get();
                        if (!accPtr)
                            return E_POINTER;

                        for (size_t k = 0; k < xFrom->count; ++k)
                        {
                            size_t u = xFrom->to[k].u;
                            assert(u < nwidth);

                            const XMVECTOR weight = XMVectorReplicate(yweight * xFrom->to[k].weight);

                            assert(x < width);
                            accPtr[u] = XMVectorMultiplyAdd(row[x], weight, accPtr[u]);
                        }
                    }

                    xFrom = reinterpret_cast<FilterFrom*>(reinterpret_cast<uint8_t*>(xFrom) + xFrom->sizeInBytes);
                }

                // Write completed accumulation rows
                for (size_t j = 0; j < yFrom->count; ++j)
                {
                    size_t v = yFrom->to[j].u;
                    assert(v < nheight);
                    TriangleRow* rowAcc = &rowActive[v];

                    assert(rowAcc->remaining > 0);
                    --rowAcc->remaining;

                    if (!rowAcc->remaining)
                    {
                        XMVECTOR* pAccSrc = rowAcc->scanline.get();
                        if (!pAccSrc)
                            return E_POINTER;

                        switch (dest->format)
                        {
                        case DXGI_FORMAT_R10G10B10A2_UNORM:
                        case DXGI_FORMAT_R10G10B10A2_UINT:
                        {
                            // Need to slightly bias results for floating-point error accumulation which can
                            // be visible with harshly quantized values
                            static const XMVECTORF32 Bias = { { { 0.f, 0.f, 0.f, 0.1f } } };

                            XMVECTOR* ptr = pAccSrc;
                            for (size_t i = 0; i < dest->width; ++i, ++ptr)
                            {
                                *ptr = XMVectorAdd(*ptr, Bias);
                            }
                        }
                        break;

                        default:
                            break;
                        }

                        // This performs any required clamping
                        if (!StoreScanlineLinear(pDest + (dest->rowPitch * v), dest->rowPitch, dest->format, pAccSrc, dest->width, filter))
                            return E_FAIL;

                        // Put row on freelist to reuse it's allocated scanline
                        rowAcc->next = rowFree;
                        rowFree = rowAcc;
                    }
                }

                yFrom = reinterpret_cast<FilterFrom*>(reinterpret_cast<uint8_t*>(yFrom) + yFrom->sizeInBytes);
            }

            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Generate volume mip-map helpers
    //-------------------------------------------------------------------------------------
    HRESULT Setup3DMips(
        _In_reads_(depth) const Image* baseImages,
        size_t depth,
        size_t levels,
        _Out_ ScratchImage& mipChain) noexcept
    {
        if (!baseImages || !depth)
            return E_INVALIDARG;

        assert(levels > 1);

        const size_t width = baseImages[0].width;
        const size_t height = baseImages[0].height;

        HRESULT hr = mipChain.Initialize3D(baseImages[0].format, width, height, depth, levels);
        if (FAILED(hr))
            return hr;

        // Copy base images to top slice
        for (size_t slice = 0; slice < depth; ++slice)
        {
            const Image& src = baseImages[slice];

            const Image *dest = mipChain.GetImage(0, 0, slice);
            if (!dest)
            {
                mipChain.Release();
                return E_POINTER;
            }

            assert(src.format == dest->format);

            uint8_t* pDest = dest->pixels;
            if (!pDest)
            {
                mipChain.Release();
                return E_POINTER;
            }

            const uint8_t *pSrc = src.pixels;
            const size_t rowPitch = src.rowPitch;
            for (size_t h = 0; h < height; ++h)
            {
                const size_t msize = std::min<size_t>(dest->rowPitch, rowPitch);
                memcpy(pDest, pSrc, msize);
                pSrc += rowPitch;
                pDest += dest->rowPitch;
            }
        }

        return S_OK;
    }


    //--- 3D Point Filter ---
    HRESULT Generate3DMipsPointFilter(size_t depth, size_t levels, const ScratchImage& mipChain) noexcept
    {
        if (!depth || !mipChain.GetImages())
            return E_INVALIDARG;

        // This assumes that the base images are already placed into the mipChain at the top level... (see _Setup3DMips)

        assert(levels > 1);

        size_t width = mipChain.GetMetadata().width;
        size_t height = mipChain.GetMetadata().height;

        // Allocate temporary space (2 scanlines)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(width) * 2);
        if (!scanline)
            return E_OUTOFMEMORY;

        XMVECTOR* target = scanline.get();

        XMVECTOR* row = target + width;

        // Resize base image to each target mip level
        for (size_t level = 1; level < levels; ++level)
        {
#ifdef _DEBUG
            memset(row, 0xCD, sizeof(XMVECTOR)*width);
#endif

            if (depth > 1)
            {
                // 3D point filter
                const size_t ndepth = depth >> 1;

                const size_t zinc = (depth << 16) / ndepth;

                size_t sz = 0;
                for (size_t slice = 0; slice < ndepth; ++slice)
                {
                    const Image* src = mipChain.GetImage(level - 1, 0, (sz >> 16));
                    const Image* dest = mipChain.GetImage(level, 0, slice);

                    if (!src || !dest)
                        return E_POINTER;

                    const uint8_t* pSrc = src->pixels;
                    uint8_t* pDest = dest->pixels;

                    const size_t rowPitch = src->rowPitch;

                    const size_t nwidth = (width > 1) ? (width >> 1) : 1;
                    const size_t nheight = (height > 1) ? (height >> 1) : 1;

                    const size_t xinc = (width << 16) / nwidth;
                    const size_t yinc = (height << 16) / nheight;

                    size_t lasty = size_t(-1);

                    size_t sy = 0;
                    for (size_t y = 0; y < nheight; ++y)
                    {
                        if ((lasty ^ sy) >> 16)
                        {
                            if (!LoadScanline(row, width, pSrc + (rowPitch * (sy >> 16)), rowPitch, src->format))
                                return E_FAIL;
                            lasty = sy;
                        }

                        size_t sx = 0;
                        for (size_t x = 0; x < nwidth; ++x)
                        {
                            target[x] = row[sx >> 16];
                            sx += xinc;
                        }

                        if (!StoreScanline(pDest, dest->rowPitch, dest->format, target, nwidth))
                            return E_FAIL;
                        pDest += dest->rowPitch;

                        sy += yinc;
                    }

                    sz += zinc;
                }
            }
            else
            {
                // 2D point filter
                const Image* src = mipChain.GetImage(level - 1, 0, 0);
                const Image* dest = mipChain.GetImage(level, 0, 0);

                if (!src || !dest)
                    return E_POINTER;

                const uint8_t* pSrc = src->pixels;
                uint8_t* pDest = dest->pixels;

                const size_t rowPitch = src->rowPitch;

                const size_t nwidth = (width > 1) ? (width >> 1) : 1;
                const size_t nheight = (height > 1) ? (height >> 1) : 1;

                const size_t xinc = (width << 16) / nwidth;
                const size_t yinc = (height << 16) / nheight;

                size_t lasty = size_t(-1);

                size_t sy = 0;
                for (size_t y = 0; y < nheight; ++y)
                {
                    if ((lasty ^ sy) >> 16)
                    {
                        if (!LoadScanline(row, width, pSrc + (rowPitch * (sy >> 16)), rowPitch, src->format))
                            return E_FAIL;
                        lasty = sy;
                    }

                    size_t sx = 0;
                    for (size_t x = 0; x < nwidth; ++x)
                    {
                        target[x] = row[sx >> 16];
                        sx += xinc;
                    }

                    if (!StoreScanline(pDest, dest->rowPitch, dest->format, target, nwidth))
                        return E_FAIL;
                    pDest += dest->rowPitch;

                    sy += yinc;
                }
            }

            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;

            if (depth > 1)
                depth >>= 1;
        }

        return S_OK;
    }


    //--- 3D Box Filter ---
    HRESULT Generate3DMipsBoxFilter(size_t depth, size_t levels, TEX_FILTER_FLAGS filter, const ScratchImage& mipChain) noexcept
    {
        using namespace DirectX::Filters;

        if (!depth || !mipChain.GetImages())
            return E_INVALIDARG;

        // This assumes that the base images are already placed into the mipChain at the top level... (see _Setup3DMips)

        assert(levels > 1);

        size_t width = mipChain.GetMetadata().width;
        size_t height = mipChain.GetMetadata().height;

        if (!ispow2(width) || !ispow2(height) || !ispow2(depth))
            return E_FAIL;

        // Allocate temporary space (5 scanlines)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(width) * 5);
        if (!scanline)
            return E_OUTOFMEMORY;

        XMVECTOR* target = scanline.get();

        XMVECTOR* urow0 = target + width;
        XMVECTOR* urow1 = target + width * 2;
        XMVECTOR* vrow0 = target + width * 3;
        XMVECTOR* vrow1 = target + width * 4;

        const XMVECTOR* urow2 = urow0 + 1;
        const XMVECTOR* urow3 = urow1 + 1;
        const XMVECTOR* vrow2 = vrow0 + 1;
        const XMVECTOR* vrow3 = vrow1 + 1;

        // Resize base image to each target mip level
        for (size_t level = 1; level < levels; ++level)
        {
            if (height <= 1)
            {
                urow1 = urow0;
                vrow1 = vrow0;
            }

            if (width <= 1)
            {
                urow2 = urow0;
                urow3 = urow1;
                vrow2 = vrow0;
                vrow3 = vrow1;
            }

            if (depth > 1)
            {
                // 3D box filter
                const size_t ndepth = depth >> 1;

                for (size_t slice = 0; slice < ndepth; ++slice)
                {
                    const size_t slicea = std::min<size_t>(slice * 2, depth - 1);
                    const size_t sliceb = std::min<size_t>(slicea + 1, depth - 1);

                    const Image* srca = mipChain.GetImage(level - 1, 0, slicea);
                    const Image* srcb = mipChain.GetImage(level - 1, 0, sliceb);
                    const Image* dest = mipChain.GetImage(level, 0, slice);

                    if (!srca || !srcb || !dest)
                        return E_POINTER;

                    const uint8_t* pSrc1 = srca->pixels;
                    const uint8_t* pSrc2 = srcb->pixels;
                    uint8_t* pDest = dest->pixels;

                    const size_t aRowPitch = srca->rowPitch;
                    const size_t bRowPitch = srcb->rowPitch;

                    const size_t nwidth = (width > 1) ? (width >> 1) : 1;
                    const size_t nheight = (height > 1) ? (height >> 1) : 1;

                    for (size_t y = 0; y < nheight; ++y)
                    {
                        if (!LoadScanlineLinear(urow0, width, pSrc1, aRowPitch, srca->format, filter))
                            return E_FAIL;
                        pSrc1 += aRowPitch;

                        if (urow0 != urow1)
                        {
                            if (!LoadScanlineLinear(urow1, width, pSrc1, aRowPitch, srca->format, filter))
                                return E_FAIL;
                            pSrc1 += aRowPitch;
                        }

                        if (!LoadScanlineLinear(vrow0, width, pSrc2, bRowPitch, srcb->format, filter))
                            return E_FAIL;
                        pSrc2 += bRowPitch;

                        if (vrow0 != vrow1)
                        {
                            if (!LoadScanlineLinear(vrow1, width, pSrc2, bRowPitch, srcb->format, filter))
                                return E_FAIL;
                            pSrc2 += bRowPitch;
                        }

                        for (size_t x = 0; x < nwidth; ++x)
                        {
                            const size_t x2 = x << 1;

                            AVERAGE8(target[x], urow0[x2], urow1[x2], urow2[x2], urow3[x2],
                                vrow0[x2], vrow1[x2], vrow2[x2], vrow3[x2])
                        }

                        if (!StoreScanlineLinear(pDest, dest->rowPitch, dest->format, target, nwidth, filter))
                            return E_FAIL;
                        pDest += dest->rowPitch;
                    }
                }
            }
            else
            {
                // 2D box filter
                const Image* src = mipChain.GetImage(level - 1, 0, 0);
                const Image* dest = mipChain.GetImage(level, 0, 0);

                if (!src || !dest)
                    return E_POINTER;

                const uint8_t* pSrc = src->pixels;
                uint8_t* pDest = dest->pixels;

                const size_t rowPitch = src->rowPitch;

                const size_t nwidth = (width > 1) ? (width >> 1) : 1;
                const size_t nheight = (height > 1) ? (height >> 1) : 1;

                for (size_t y = 0; y < nheight; ++y)
                {
                    if (!LoadScanlineLinear(urow0, width, pSrc, rowPitch, src->format, filter))
                        return E_FAIL;
                    pSrc += rowPitch;

                    if (urow0 != urow1)
                    {
                        if (!LoadScanlineLinear(urow1, width, pSrc, rowPitch, src->format, filter))
                            return E_FAIL;
                        pSrc += rowPitch;
                    }

                    for (size_t x = 0; x < nwidth; ++x)
                    {
                        const size_t x2 = x << 1;

                        AVERAGE4(target[x], urow0[x2], urow1[x2], urow2[x2], urow3[x2])
                    }

                    if (!StoreScanlineLinear(pDest, dest->rowPitch, dest->format, target, nwidth, filter))
                        return E_FAIL;
                    pDest += dest->rowPitch;
                }
            }

            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;

            if (depth > 1)
                depth >>= 1;
        }

        return S_OK;
    }


    //--- 3D Linear Filter ---
    HRESULT Generate3DMipsLinearFilter(size_t depth, size_t levels, TEX_FILTER_FLAGS filter, const ScratchImage& mipChain) noexcept
    {
        using namespace DirectX::Filters;

        if (!depth || !mipChain.GetImages())
            return E_INVALIDARG;

        // This assumes that the base images are already placed into the mipChain at the top level... (see _Setup3DMips)

        assert(levels > 1);

        size_t width = mipChain.GetMetadata().width;
        size_t height = mipChain.GetMetadata().height;

        // Allocate temporary space (5 scanlines, plus X/Y/Z filters)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(width) * 5);
        if (!scanline)
            return E_OUTOFMEMORY;

        std::unique_ptr<LinearFilter[]> lf(new (std::nothrow) LinearFilter[width + height + depth]);
        if (!lf)
            return E_OUTOFMEMORY;

        LinearFilter* lfX = lf.get();
        LinearFilter* lfY = lf.get() + width;
        LinearFilter* lfZ = lf.get() + width + height;

        XMVECTOR* target = scanline.get();

        XMVECTOR* urow0 = target + width;
        XMVECTOR* urow1 = target + width * 2;
        XMVECTOR* vrow0 = target + width * 3;
        XMVECTOR* vrow1 = target + width * 4;

        // Resize base image to each target mip level
        for (size_t level = 1; level < levels; ++level)
        {
            const size_t nwidth = (width > 1) ? (width >> 1) : 1;
            CreateLinearFilter(width, nwidth, (filter & TEX_FILTER_WRAP_U) != 0, lfX);

            const size_t nheight = (height > 1) ? (height >> 1) : 1;
            CreateLinearFilter(height, nheight, (filter & TEX_FILTER_WRAP_V) != 0, lfY);

#ifdef _DEBUG
            memset(urow0, 0xCD, sizeof(XMVECTOR)*width);
            memset(urow1, 0xDD, sizeof(XMVECTOR)*width);
            memset(vrow0, 0xED, sizeof(XMVECTOR)*width);
            memset(vrow1, 0xFD, sizeof(XMVECTOR)*width);
#endif

            if (depth > 1)
            {
                // 3D linear filter
                const size_t ndepth = depth >> 1;
                CreateLinearFilter(depth, ndepth, (filter & TEX_FILTER_WRAP_W) != 0, lfZ);

                for (size_t slice = 0; slice < ndepth; ++slice)
                {
                    auto const& toZ = lfZ[slice];

                    const Image* srca = mipChain.GetImage(level - 1, 0, toZ.u0);
                    const Image* srcb = mipChain.GetImage(level - 1, 0, toZ.u1);
                    if (!srca || !srcb)
                        return E_POINTER;

                    size_t u0 = size_t(-1);
                    size_t u1 = size_t(-1);

                    const Image* dest = mipChain.GetImage(level, 0, slice);
                    if (!dest)
                        return E_POINTER;

                    uint8_t* pDest = dest->pixels;

                    for (size_t y = 0; y < nheight; ++y)
                    {
                        auto const& toY = lfY[y];

                        if (toY.u0 != u0)
                        {
                            if (toY.u0 != u1)
                            {
                                u0 = toY.u0;

                                if (!LoadScanlineLinear(urow0, width, srca->pixels + (srca->rowPitch * u0), srca->rowPitch, srca->format, filter)
                                    || !LoadScanlineLinear(vrow0, width, srcb->pixels + (srcb->rowPitch * u0), srcb->rowPitch, srcb->format, filter))
                                    return E_FAIL;
                            }
                            else
                            {
                                u0 = u1;
                                u1 = size_t(-1);

                                std::swap(urow0, urow1);
                                std::swap(vrow0, vrow1);
                            }
                        }

                        if (toY.u1 != u1)
                        {
                            u1 = toY.u1;

                            if (!LoadScanlineLinear(urow1, width, srca->pixels + (srca->rowPitch * u1), srca->rowPitch, srca->format, filter)
                                || !LoadScanlineLinear(vrow1, width, srcb->pixels + (srcb->rowPitch * u1), srcb->rowPitch, srcb->format, filter))
                                return E_FAIL;
                        }

                        for (size_t x = 0; x < nwidth; ++x)
                        {
                            auto const& toX = lfX[x];

                            TRILINEAR_INTERPOLATE(target[x], toX, toY, toZ, urow0, urow1, vrow0, vrow1)
                        }

                        if (!StoreScanlineLinear(pDest, dest->rowPitch, dest->format, target, nwidth, filter))
                            return E_FAIL;
                        pDest += dest->rowPitch;
                    }
                }
            }
            else
            {
                // 2D linear filter
                const Image* src = mipChain.GetImage(level - 1, 0, 0);
                const Image* dest = mipChain.GetImage(level, 0, 0);

                if (!src || !dest)
                    return E_POINTER;

                const uint8_t* pSrc = src->pixels;
                uint8_t* pDest = dest->pixels;

                const size_t rowPitch = src->rowPitch;

                size_t u0 = size_t(-1);
                size_t u1 = size_t(-1);

                for (size_t y = 0; y < nheight; ++y)
                {
                    auto const& toY = lfY[y];

                    if (toY.u0 != u0)
                    {
                        if (toY.u0 != u1)
                        {
                            u0 = toY.u0;

                            if (!LoadScanlineLinear(urow0, width, pSrc + (rowPitch * u0), rowPitch, src->format, filter))
                                return E_FAIL;
                        }
                        else
                        {
                            u0 = u1;
                            u1 = size_t(-1);

                            std::swap(urow0, urow1);
                        }
                    }

                    if (toY.u1 != u1)
                    {
                        u1 = toY.u1;

                        if (!LoadScanlineLinear(urow1, width, pSrc + (rowPitch * u1), rowPitch, src->format, filter))
                            return E_FAIL;
                    }

                    for (size_t x = 0; x < nwidth; ++x)
                    {
                        auto const& toX = lfX[x];

                        BILINEAR_INTERPOLATE(target[x], toX, toY, urow0, urow1)
                    }

                    if (!StoreScanlineLinear(pDest, dest->rowPitch, dest->format, target, nwidth, filter))
                        return E_FAIL;
                    pDest += dest->rowPitch;
                }
            }

            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;

            if (depth > 1)
                depth >>= 1;
        }

        return S_OK;
    }


    //--- 3D Cubic Filter ---
    HRESULT Generate3DMipsCubicFilter(size_t depth, size_t levels, TEX_FILTER_FLAGS filter, const ScratchImage& mipChain) noexcept
    {
        using namespace DirectX::Filters;

        if (!depth || !mipChain.GetImages())
            return E_INVALIDARG;

        // This assumes that the base images are already placed into the mipChain at the top level... (see _Setup3DMips)

        assert(levels > 1);

        size_t width = mipChain.GetMetadata().width;
        size_t height = mipChain.GetMetadata().height;

        // Allocate temporary space (17 scanlines, plus X/Y/Z filters)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(width) * 17);
        if (!scanline)
            return E_OUTOFMEMORY;

        std::unique_ptr<CubicFilter[]> cf(new (std::nothrow) CubicFilter[width + height + depth]);
        if (!cf)
            return E_OUTOFMEMORY;

        CubicFilter* cfX = cf.get();
        CubicFilter* cfY = cf.get() + width;
        CubicFilter* cfZ = cf.get() + width + height;

        XMVECTOR* target = scanline.get();

        XMVECTOR* urow[4];
        XMVECTOR* vrow[4];
        XMVECTOR* srow[4];
        XMVECTOR* trow[4];

        XMVECTOR *ptr = scanline.get() + width;
        for (size_t j = 0; j < 4; ++j)
        {
            urow[j] = ptr;  ptr += width;
            vrow[j] = ptr;  ptr += width;
            srow[j] = ptr;  ptr += width;
            trow[j] = ptr;  ptr += width;
        }

        // Resize base image to each target mip level
        for (size_t level = 1; level < levels; ++level)
        {
            const size_t nwidth = (width > 1) ? (width >> 1) : 1;
            CreateCubicFilter(width, nwidth, (filter & TEX_FILTER_WRAP_U) != 0, (filter & TEX_FILTER_MIRROR_U) != 0, cfX);

            const size_t nheight = (height > 1) ? (height >> 1) : 1;
            CreateCubicFilter(height, nheight, (filter & TEX_FILTER_WRAP_V) != 0, (filter & TEX_FILTER_MIRROR_V) != 0, cfY);

#ifdef _DEBUG
            for (size_t j = 0; j < 4; ++j)
            {
                memset(urow[j], 0xCD, sizeof(XMVECTOR)*width);
                memset(vrow[j], 0xDD, sizeof(XMVECTOR)*width);
                memset(srow[j], 0xED, sizeof(XMVECTOR)*width);
                memset(trow[j], 0xFD, sizeof(XMVECTOR)*width);
            }
#endif

            if (depth > 1)
            {
                // 3D cubic filter
                const size_t ndepth = depth >> 1;
                CreateCubicFilter(depth, ndepth, (filter & TEX_FILTER_WRAP_W) != 0, (filter & TEX_FILTER_MIRROR_W) != 0, cfZ);

                for (size_t slice = 0; slice < ndepth; ++slice)
                {
                    auto const& toZ = cfZ[slice];

                    const Image* srca = mipChain.GetImage(level - 1, 0, toZ.u0);
                    const Image* srcb = mipChain.GetImage(level - 1, 0, toZ.u1);
                    const Image* srcc = mipChain.GetImage(level - 1, 0, toZ.u2);
                    const Image* srcd = mipChain.GetImage(level - 1, 0, toZ.u3);
                    if (!srca || !srcb || !srcc || !srcd)
                        return E_POINTER;

                    size_t u0 = size_t(-1);
                    size_t u1 = size_t(-1);
                    size_t u2 = size_t(-1);
                    size_t u3 = size_t(-1);

                    const Image* dest = mipChain.GetImage(level, 0, slice);
                    if (!dest)
                        return E_POINTER;

                    uint8_t* pDest = dest->pixels;

                    for (size_t y = 0; y < nheight; ++y)
                    {
                        auto const& toY = cfY[y];

                        // Scanline 1
                        if (toY.u0 != u0)
                        {
                            if (toY.u0 != u1 && toY.u0 != u2 && toY.u0 != u3)
                            {
                                u0 = toY.u0;

                                if (!LoadScanlineLinear(urow[0], width, srca->pixels + (srca->rowPitch * u0), srca->rowPitch, srca->format, filter)
                                    || !LoadScanlineLinear(urow[1], width, srcb->pixels + (srcb->rowPitch * u0), srcb->rowPitch, srcb->format, filter)
                                    || !LoadScanlineLinear(urow[2], width, srcc->pixels + (srcc->rowPitch * u0), srcc->rowPitch, srcc->format, filter)
                                    || !LoadScanlineLinear(urow[3], width, srcd->pixels + (srcd->rowPitch * u0), srcd->rowPitch, srcd->format, filter))
                                    return E_FAIL;
                            }
                            else if (toY.u0 == u1)
                            {
                                u0 = u1;
                                u1 = size_t(-1);

                                std::swap(urow[0], vrow[0]);
                                std::swap(urow[1], vrow[1]);
                                std::swap(urow[2], vrow[2]);
                                std::swap(urow[3], vrow[3]);
                            }
                            else if (toY.u0 == u2)
                            {
                                u0 = u2;
                                u2 = size_t(-1);

                                std::swap(urow[0], srow[0]);
                                std::swap(urow[1], srow[1]);
                                std::swap(urow[2], srow[2]);
                                std::swap(urow[3], srow[3]);
                            }
                            else if (toY.u0 == u3)
                            {
                                u0 = u3;
                                u3 = size_t(-1);

                                std::swap(urow[0], trow[0]);
                                std::swap(urow[1], trow[1]);
                                std::swap(urow[2], trow[2]);
                                std::swap(urow[3], trow[3]);
                            }
                        }

                        // Scanline 2
                        if (toY.u1 != u1)
                        {
                            if (toY.u1 != u2 && toY.u1 != u3)
                            {
                                u1 = toY.u1;

                                if (!LoadScanlineLinear(vrow[0], width, srca->pixels + (srca->rowPitch * u1), srca->rowPitch, srca->format, filter)
                                    || !LoadScanlineLinear(vrow[1], width, srcb->pixels + (srcb->rowPitch * u1), srcb->rowPitch, srcb->format, filter)
                                    || !LoadScanlineLinear(vrow[2], width, srcc->pixels + (srcc->rowPitch * u1), srcc->rowPitch, srcc->format, filter)
                                    || !LoadScanlineLinear(vrow[3], width, srcd->pixels + (srcd->rowPitch * u1), srcd->rowPitch, srcd->format, filter))
                                    return E_FAIL;
                            }
                            else if (toY.u1 == u2)
                            {
                                u1 = u2;
                                u2 = size_t(-1);

                                std::swap(vrow[0], srow[0]);
                                std::swap(vrow[1], srow[1]);
                                std::swap(vrow[2], srow[2]);
                                std::swap(vrow[3], srow[3]);
                            }
                            else if (toY.u1 == u3)
                            {
                                u1 = u3;
                                u3 = size_t(-1);

                                std::swap(vrow[0], trow[0]);
                                std::swap(vrow[1], trow[1]);
                                std::swap(vrow[2], trow[2]);
                                std::swap(vrow[3], trow[3]);
                            }
                        }

                        // Scanline 3
                        if (toY.u2 != u2)
                        {
                            if (toY.u2 != u3)
                            {
                                u2 = toY.u2;

                                if (!LoadScanlineLinear(srow[0], width, srca->pixels + (srca->rowPitch * u2), srca->rowPitch, srca->format, filter)
                                    || !LoadScanlineLinear(srow[1], width, srcb->pixels + (srcb->rowPitch * u2), srcb->rowPitch, srcb->format, filter)
                                    || !LoadScanlineLinear(srow[2], width, srcc->pixels + (srcc->rowPitch * u2), srcc->rowPitch, srcc->format, filter)
                                    || !LoadScanlineLinear(srow[3], width, srcd->pixels + (srcd->rowPitch * u2), srcd->rowPitch, srcd->format, filter))
                                    return E_FAIL;
                            }
                            else
                            {
                                u2 = u3;
                                u3 = size_t(-1);

                                std::swap(srow[0], trow[0]);
                                std::swap(srow[1], trow[1]);
                                std::swap(srow[2], trow[2]);
                                std::swap(srow[3], trow[3]);
                            }
                        }

                        // Scanline 4
                        if (toY.u3 != u3)
                        {
                            u3 = toY.u3;

                            if (!LoadScanlineLinear(trow[0], width, srca->pixels + (srca->rowPitch * u3), srca->rowPitch, srca->format, filter)
                                || !LoadScanlineLinear(trow[1], width, srcb->pixels + (srcb->rowPitch * u3), srcb->rowPitch, srcb->format, filter)
                                || !LoadScanlineLinear(trow[2], width, srcc->pixels + (srcc->rowPitch * u3), srcc->rowPitch, srcc->format, filter)
                                || !LoadScanlineLinear(trow[3], width, srcd->pixels + (srcd->rowPitch * u3), srcd->rowPitch, srcd->format, filter))
                                return E_FAIL;
                        }

                        for (size_t x = 0; x < nwidth; ++x)
                        {
                            auto const& toX = cfX[x];

                            XMVECTOR D[4];

                            for (size_t j = 0; j < 4; ++j)
                            {
                                XMVECTOR C0, C1, C2, C3;
                                CUBIC_INTERPOLATE(C0, toX.x, urow[j][toX.u0], urow[j][toX.u1], urow[j][toX.u2], urow[j][toX.u3])
                                CUBIC_INTERPOLATE(C1, toX.x, vrow[j][toX.u0], vrow[j][toX.u1], vrow[j][toX.u2], vrow[j][toX.u3])
                                CUBIC_INTERPOLATE(C2, toX.x, srow[j][toX.u0], srow[j][toX.u1], srow[j][toX.u2], srow[j][toX.u3])
                                CUBIC_INTERPOLATE(C3, toX.x, trow[j][toX.u0], trow[j][toX.u1], trow[j][toX.u2], trow[j][toX.u3])

                                CUBIC_INTERPOLATE(D[j], toY.x, C0, C1, C2, C3)
                            }

                            CUBIC_INTERPOLATE(target[x], toZ.x, D[0], D[1], D[2], D[3])
                        }

                        if (!StoreScanlineLinear(pDest, dest->rowPitch, dest->format, target, nwidth, filter))
                            return E_FAIL;
                        pDest += dest->rowPitch;
                    }
                }
            }
            else
            {
                // 2D cubic filter
                const Image* src = mipChain.GetImage(level - 1, 0, 0);
                const Image* dest = mipChain.GetImage(level, 0, 0);

                if (!src || !dest)
                    return E_POINTER;

                const uint8_t* pSrc = src->pixels;
                uint8_t* pDest = dest->pixels;

                const size_t rowPitch = src->rowPitch;

                size_t u0 = size_t(-1);
                size_t u1 = size_t(-1);
                size_t u2 = size_t(-1);
                size_t u3 = size_t(-1);

                for (size_t y = 0; y < nheight; ++y)
                {
                    auto const& toY = cfY[y];

                    // Scanline 1
                    if (toY.u0 != u0)
                    {
                        if (toY.u0 != u1 && toY.u0 != u2 && toY.u0 != u3)
                        {
                            u0 = toY.u0;

                            if (!LoadScanlineLinear(urow[0], width, pSrc + (rowPitch * u0), rowPitch, src->format, filter))
                                return E_FAIL;
                        }
                        else if (toY.u0 == u1)
                        {
                            u0 = u1;
                            u1 = size_t(-1);

                            std::swap(urow[0], vrow[0]);
                        }
                        else if (toY.u0 == u2)
                        {
                            u0 = u2;
                            u2 = size_t(-1);

                            std::swap(urow[0], srow[0]);
                        }
                        else if (toY.u0 == u3)
                        {
                            u0 = u3;
                            u3 = size_t(-1);

                            std::swap(urow[0], trow[0]);
                        }
                    }

                    // Scanline 2
                    if (toY.u1 != u1)
                    {
                        if (toY.u1 != u2 && toY.u1 != u3)
                        {
                            u1 = toY.u1;

                            if (!LoadScanlineLinear(vrow[0], width, pSrc + (rowPitch * u1), rowPitch, src->format, filter))
                                return E_FAIL;
                        }
                        else if (toY.u1 == u2)
                        {
                            u1 = u2;
                            u2 = size_t(-1);

                            std::swap(vrow[0], srow[0]);
                        }
                        else if (toY.u1 == u3)
                        {
                            u1 = u3;
                            u3 = size_t(-1);

                            std::swap(vrow[0], trow[0]);
                        }
                    }

                    // Scanline 3
                    if (toY.u2 != u2)
                    {
                        if (toY.u2 != u3)
                        {
                            u2 = toY.u2;

                            if (!LoadScanlineLinear(srow[0], width, pSrc + (rowPitch * u2), rowPitch, src->format, filter))
                                return E_FAIL;
                        }
                        else
                        {
                            u2 = u3;
                            u3 = size_t(-1);

                            std::swap(srow[0], trow[0]);
                        }
                    }

                    // Scanline 4
                    if (toY.u3 != u3)
                    {
                        u3 = toY.u3;

                        if (!LoadScanlineLinear(trow[0], width, pSrc + (rowPitch * u3), rowPitch, src->format, filter))
                            return E_FAIL;
                    }

                    for (size_t x = 0; x < nwidth; ++x)
                    {
                        auto const& toX = cfX[x];

                        XMVECTOR C0, C1, C2, C3;
                        CUBIC_INTERPOLATE(C0, toX.x, urow[0][toX.u0], urow[0][toX.u1], urow[0][toX.u2], urow[0][toX.u3])
                        CUBIC_INTERPOLATE(C1, toX.x, vrow[0][toX.u0], vrow[0][toX.u1], vrow[0][toX.u2], vrow[0][toX.u3])
                        CUBIC_INTERPOLATE(C2, toX.x, srow[0][toX.u0], srow[0][toX.u1], srow[0][toX.u2], srow[0][toX.u3])
                        CUBIC_INTERPOLATE(C3, toX.x, trow[0][toX.u0], trow[0][toX.u1], trow[0][toX.u2], trow[0][toX.u3])

                        CUBIC_INTERPOLATE(target[x], toY.x, C0, C1, C2, C3)
                    }

                    if (!StoreScanlineLinear(pDest, dest->rowPitch, dest->format, target, nwidth, filter))
                        return E_FAIL;
                    pDest += dest->rowPitch;
                }
            }

            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;

            if (depth > 1)
                depth >>= 1;
        }

        return S_OK;
    }


    //--- 3D Triangle Filter ---
    HRESULT Generate3DMipsTriangleFilter(size_t depth, size_t levels, TEX_FILTER_FLAGS filter, const ScratchImage& mipChain) noexcept
    {
        using namespace DirectX::Filters;

        if (!depth || !mipChain.GetImages())
            return E_INVALIDARG;

        // This assumes that the base images are already placed into the mipChain at the top level... (see _Setup3DMips)

        assert(levels > 1);

        size_t width = mipChain.GetMetadata().width;
        size_t height = mipChain.GetMetadata().height;

        // Allocate initial temporary space (1 scanline, accumulation rows, plus X/Y/Z filters)
        auto scanline = make_AlignedArrayXMVECTOR(width);
        if (!scanline)
            return E_OUTOFMEMORY;

        std::unique_ptr<TriangleRow[]> sliceActive(new (std::nothrow) TriangleRow[depth]);
        if (!sliceActive)
            return E_OUTOFMEMORY;

        TriangleRow * sliceFree = nullptr;

        std::unique_ptr<Filter> tfX, tfY, tfZ;

        XMVECTOR* row = scanline.get();

        // Resize base image to each target mip level
        for (size_t level = 1; level < levels; ++level)
        {
            const size_t nwidth = (width > 1) ? (width >> 1) : 1;
            HRESULT hr = CreateTriangleFilter(width, nwidth, (filter & TEX_FILTER_WRAP_U) != 0, tfX);
            if (FAILED(hr))
                return hr;

            const size_t nheight = (height > 1) ? (height >> 1) : 1;
            hr = CreateTriangleFilter(height, nheight, (filter & TEX_FILTER_WRAP_V) != 0, tfY);
            if (FAILED(hr))
                return hr;

            const size_t ndepth = (depth > 1) ? (depth >> 1) : 1;
            hr = CreateTriangleFilter(depth, ndepth, (filter & TEX_FILTER_WRAP_W) != 0, tfZ);
            if (FAILED(hr))
                return hr;

#ifdef _DEBUG
            memset(row, 0xCD, sizeof(XMVECTOR)*width);
#endif

            auto xFromEnd = reinterpret_cast<const FilterFrom*>(reinterpret_cast<const uint8_t*>(tfX.get()) + tfX->sizeInBytes);
            auto yFromEnd = reinterpret_cast<const FilterFrom*>(reinterpret_cast<const uint8_t*>(tfY.get()) + tfY->sizeInBytes);
            auto zFromEnd = reinterpret_cast<const FilterFrom*>(reinterpret_cast<const uint8_t*>(tfZ.get()) + tfZ->sizeInBytes);

            // Count times slices get written (and clear out any leftover accumulation slices from last miplevel)
            for (FilterFrom* zFrom = tfZ->from; zFrom < zFromEnd; )
            {
                for (size_t j = 0; j < zFrom->count; ++j)
                {
                    const size_t w = zFrom->to[j].u;
                    assert(w < ndepth);
                    TriangleRow* sliceAcc = &sliceActive[w];

                    ++sliceAcc->remaining;

                    if (sliceAcc->scanline)
                    {
                        memset(sliceAcc->scanline.get(), 0, sizeof(XMVECTOR) * nwidth * nheight);
                    }
                }

                zFrom = reinterpret_cast<FilterFrom*>(reinterpret_cast<uint8_t*>(zFrom) + zFrom->sizeInBytes);
            }

            // Filter image
            size_t z = 0;
            for (FilterFrom* zFrom = tfZ->from; zFrom < zFromEnd; ++z)
            {
                // Create accumulation slices as needed
                for (size_t j = 0; j < zFrom->count; ++j)
                {
                    const size_t w = zFrom->to[j].u;
                    assert(w < ndepth);
                    TriangleRow* sliceAcc = &sliceActive[w];

                    if (!sliceAcc->scanline)
                    {
                        if (sliceFree)
                        {
                            // Steal and reuse scanline from 'free slice' list
                            // (it will always be at least as large as nwidth*nheight due to loop decending order)
                            assert(sliceFree->scanline != nullptr);
                            sliceAcc->scanline.reset(sliceFree->scanline.release());
                            sliceFree = sliceFree->next;
                        }
                        else
                        {
                            auto nscanline = make_AlignedArrayXMVECTOR(uint64_t(nwidth) * uint64_t(nheight));
                            if (!nscanline)
                                return E_OUTOFMEMORY;
                            sliceAcc->scanline.swap(nscanline);
                        }

                        memset(sliceAcc->scanline.get(), 0, sizeof(XMVECTOR) * nwidth * nheight);
                    }
                }

                assert(z < depth);
                const Image* src = mipChain.GetImage(level - 1, 0, z);
                if (!src)
                    return E_POINTER;

                const uint8_t* pSrc = src->pixels;
                const size_t rowPitch = src->rowPitch;
                const uint8_t* pEndSrc = pSrc + rowPitch * height;

                for (FilterFrom* yFrom = tfY->from; yFrom < yFromEnd; )
                {
                    // Load source scanline
                    if ((pSrc + rowPitch) > pEndSrc)
                        return E_FAIL;

                    if (!LoadScanlineLinear(row, width, pSrc, rowPitch, src->format, filter))
                        return E_FAIL;

                    pSrc += rowPitch;

                    // Process row
                    size_t x = 0;
                    for (FilterFrom* xFrom = tfX->from; xFrom < xFromEnd; ++x)
                    {
                        for (size_t j = 0; j < zFrom->count; ++j)
                        {
                            const size_t w = zFrom->to[j].u;
                            assert(w < ndepth);
                            const float zweight = zFrom->to[j].weight;

                            XMVECTOR* accSlice = sliceActive[w].scanline.get();
                            if (!accSlice)
                                return E_POINTER;

                            for (size_t k = 0; k < yFrom->count; ++k)
                            {
                                size_t v = yFrom->to[k].u;
                                assert(v < nheight);
                                const float yweight = yFrom->to[k].weight;

                                XMVECTOR * accPtr = accSlice + v * nwidth;

                                for (size_t l = 0; l < xFrom->count; ++l)
                                {
                                    size_t u = xFrom->to[l].u;
                                    assert(u < nwidth);

                                    const XMVECTOR weight = XMVectorReplicate(zweight * yweight * xFrom->to[l].weight);

                                    assert(x < width);
                                    accPtr[u] = XMVectorMultiplyAdd(row[x], weight, accPtr[u]);
                                }
                            }
                        }

                        xFrom = reinterpret_cast<FilterFrom*>(reinterpret_cast<uint8_t*>(xFrom) + xFrom->sizeInBytes);
                    }

                    yFrom = reinterpret_cast<FilterFrom*>(reinterpret_cast<uint8_t*>(yFrom) + yFrom->sizeInBytes);
                }

                // Write completed accumulation slices
                for (size_t j = 0; j < zFrom->count; ++j)
                {
                    const size_t w = zFrom->to[j].u;
                    assert(w < ndepth);
                    TriangleRow* sliceAcc = &sliceActive[w];

                    assert(sliceAcc->remaining > 0);
                    --sliceAcc->remaining;

                    if (!sliceAcc->remaining)
                    {
                        const Image* dest = mipChain.GetImage(level, 0, w);
                        XMVECTOR* pAccSrc = sliceAcc->scanline.get();
                        if (!dest || !pAccSrc)
                            return E_POINTER;

                        uint8_t* pDest = dest->pixels;

                        for (size_t h = 0; h < nheight; ++h)
                        {
                            switch (dest->format)
                            {
                            case DXGI_FORMAT_R10G10B10A2_UNORM:
                            case DXGI_FORMAT_R10G10B10A2_UINT:
                            {
                                // Need to slightly bias results for floating-point error accumulation which can
                                // be visible with harshly quantized values
                                static const XMVECTORF32 Bias = { { { 0.f, 0.f, 0.f, 0.1f } } };

                                XMVECTOR* ptr = pAccSrc;
                                for (size_t i = 0; i < dest->width; ++i, ++ptr)
                                {
                                    *ptr = XMVectorAdd(*ptr, Bias);
                                }
                            }
                            break;

                            default:
                                break;
                            }

                            // This performs any required clamping
                            if (!StoreScanlineLinear(pDest, dest->rowPitch, dest->format, pAccSrc, dest->width, filter))
                                return E_FAIL;

                            pDest += dest->rowPitch;
                            pAccSrc += nwidth;
                        }

                        // Put slice on freelist to reuse it's allocated scanline
                        sliceAcc->next = sliceFree;
                        sliceFree = sliceAcc;
                    }
                }

                zFrom = reinterpret_cast<FilterFrom*>(reinterpret_cast<uint8_t*>(zFrom) + zFrom->sizeInBytes);
            }

            if (height > 1)
                height >>= 1;

            if (width > 1)
                width >>= 1;

            if (depth > 1)
                depth >>= 1;
        }

        return S_OK;
    }
}


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Generate mipmap chain
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::GenerateMipMaps(
    const Image& baseImage,
    TEX_FILTER_FLAGS filter,
    size_t levels,
    ScratchImage& mipChain,
    bool allow1D) noexcept
{
    if (!IsValid(baseImage.format))
        return E_INVALIDARG;

    if (!baseImage.pixels)
        return E_POINTER;

    if (!CalculateMipLevels(baseImage.width, baseImage.height, levels))
        return E_INVALIDARG;

    if (levels <= 1)
        return E_INVALIDARG;

    if (IsCompressed(baseImage.format) || IsTypeless(baseImage.format) || IsPlanar(baseImage.format) || IsPalettized(baseImage.format))
    {
        return HRESULT_E_NOT_SUPPORTED;
    }

    HRESULT hr = E_UNEXPECTED;

    static_assert(TEX_FILTER_POINT == 0x100000, "TEX_FILTER_ flag values don't match TEX_FILTER_MODE_MASK");

#ifdef WIN32
    bool usewic = UseWICFiltering(baseImage.format, filter);

    WICPixelFormatGUID pfGUID = {};
    const bool wicpf = (usewic) ? DXGIToWIC(baseImage.format, pfGUID, true) : false;

    if (usewic && !wicpf)
    {
        // Check to see if the source and/or result size is too big for WIC
        const uint64_t expandedSize = uint64_t(std::max<size_t>(1, baseImage.width >> 1)) * uint64_t(std::max<size_t>(1, baseImage.height >> 1)) * sizeof(float) * 4;
        const uint64_t expandedSize2 = uint64_t(baseImage.width) * uint64_t(baseImage.height) * sizeof(float) * 4;
        if (expandedSize > UINT32_MAX || expandedSize2 > UINT32_MAX)
        {
            if (filter & TEX_FILTER_FORCE_WIC)
                return HRESULT_E_ARITHMETIC_OVERFLOW;

            usewic = false;
        }
    }

    if (usewic)
    {
        //--- Use WIC filtering to generate mipmaps -----------------------------------
        switch (filter & TEX_FILTER_MODE_MASK)
        {
        case 0:
        case TEX_FILTER_POINT:
        case TEX_FILTER_FANT: // Equivalent to Box filter
        case TEX_FILTER_LINEAR:
        case TEX_FILTER_CUBIC:
        {
            static_assert(TEX_FILTER_FANT == TEX_FILTER_BOX, "TEX_FILTER_ flag alias mismatch");

            if (wicpf)
            {
                // Case 1: Base image format is supported by Windows Imaging Component
                hr = (baseImage.height > 1 || !allow1D)
                    ? mipChain.Initialize2D(baseImage.format, baseImage.width, baseImage.height, 1, levels)
                    : mipChain.Initialize1D(baseImage.format, baseImage.width, 1, levels);
                if (FAILED(hr))
                    return hr;

                return GenerateMipMapsUsingWIC(baseImage, filter, levels, pfGUID, mipChain, 0);
            }
            else
            {
                // Case 2: Base image format is not supported by WIC, so we have to convert, generate, and convert back
                assert(baseImage.format != DXGI_FORMAT_R32G32B32A32_FLOAT);
                ScratchImage temp;
                hr = ConvertToR32G32B32A32(baseImage, temp);
                if (FAILED(hr))
                    return hr;

                const Image *timg = temp.GetImage(0, 0, 0);
                if (!timg)
                    return E_POINTER;

                ScratchImage tMipChain;
                hr = (baseImage.height > 1 || !allow1D)
                    ? tMipChain.Initialize2D(DXGI_FORMAT_R32G32B32A32_FLOAT, baseImage.width, baseImage.height, 1, levels)
                    : tMipChain.Initialize1D(DXGI_FORMAT_R32G32B32A32_FLOAT, baseImage.width, 1, levels);
                if (FAILED(hr))
                    return hr;

                hr = GenerateMipMapsUsingWIC(*timg, filter, levels, GUID_WICPixelFormat128bppRGBAFloat, tMipChain, 0);
                if (FAILED(hr))
                    return hr;

                temp.Release();

                return ConvertFromR32G32B32A32(tMipChain.GetImages(), tMipChain.GetImageCount(), tMipChain.GetMetadata(), baseImage.format, mipChain);
            }
        }

        default:
            return HRESULT_E_NOT_SUPPORTED;
        }
    }
    else
#endif // WIN32
    {
        //--- Use custom filters to generate mipmaps ----------------------------------
        TexMetadata mdata = {};
        mdata.width = baseImage.width;
        if (baseImage.height > 1 || !allow1D)
        {
            mdata.height = baseImage.height;
            mdata.dimension = TEX_DIMENSION_TEXTURE2D;
        }
        else
        {
            mdata.height = 1;
            mdata.dimension = TEX_DIMENSION_TEXTURE1D;
        }
        mdata.depth = mdata.arraySize = 1;
        mdata.mipLevels = levels;
        mdata.format = baseImage.format;

        unsigned long filter_select = (filter & TEX_FILTER_MODE_MASK);
        if (!filter_select)
        {
            // Default filter choice
            filter_select = (ispow2(baseImage.width) && ispow2(baseImage.height)) ? TEX_FILTER_BOX : TEX_FILTER_LINEAR;
        }

        switch (filter_select)
        {
        case TEX_FILTER_BOX:
            hr = Setup2DMips(&baseImage, 1, mdata, mipChain);
            if (FAILED(hr))
                return hr;

            hr = Generate2DMipsBoxFilter(levels, filter, mipChain, 0);
            if (FAILED(hr))
                mipChain.Release();
            return hr;

        case TEX_FILTER_POINT:
            hr = Setup2DMips(&baseImage, 1, mdata, mipChain);
            if (FAILED(hr))
                return hr;

            hr = Generate2DMipsPointFilter(levels, mipChain, 0);
            if (FAILED(hr))
                mipChain.Release();
            return hr;

        case TEX_FILTER_LINEAR:
            hr = Setup2DMips(&baseImage, 1, mdata, mipChain);
            if (FAILED(hr))
                return hr;

            hr = Generate2DMipsLinearFilter(levels, filter, mipChain, 0);
            if (FAILED(hr))
                mipChain.Release();
            return hr;

        case TEX_FILTER_CUBIC:
            hr = Setup2DMips(&baseImage, 1, mdata, mipChain);
            if (FAILED(hr))
                return hr;

            hr = Generate2DMipsCubicFilter(levels, filter, mipChain, 0);
            if (FAILED(hr))
                mipChain.Release();
            return hr;

        case TEX_FILTER_TRIANGLE:
            hr = Setup2DMips(&baseImage, 1, mdata, mipChain);
            if (FAILED(hr))
                return hr;

            hr = Generate2DMipsTriangleFilter(levels, filter, mipChain, 0);
            if (FAILED(hr))
                mipChain.Release();
            return hr;

        default:
            return HRESULT_E_NOT_SUPPORTED;
        }
    }
}

_Use_decl_annotations_
HRESULT DirectX::GenerateMipMaps(
    const Image* srcImages,
    size_t nimages,
    const TexMetadata& metadata,
    TEX_FILTER_FLAGS filter,
    size_t levels,
    ScratchImage& mipChain)
{
    if (!srcImages || !nimages || !IsValid(metadata.format))
        return E_INVALIDARG;

    if (metadata.IsVolumemap()
        || IsCompressed(metadata.format) || IsTypeless(metadata.format) || IsPlanar(metadata.format) || IsPalettized(metadata.format))
        return HRESULT_E_NOT_SUPPORTED;

    if (!CalculateMipLevels(metadata.width, metadata.height, levels))
        return E_INVALIDARG;

    if (levels <= 1)
        return E_INVALIDARG;

    std::vector<Image> baseImages;
    baseImages.reserve(metadata.arraySize);
    for (size_t item = 0; item < metadata.arraySize; ++item)
    {
        const size_t index = metadata.ComputeIndex(0, item, 0);
        if (index >= nimages)
            return E_FAIL;

        const Image& src = srcImages[index];
        if (!src.pixels)
            return E_POINTER;

        if (src.format != metadata.format || src.width != metadata.width || src.height != metadata.height)
        {
            // All base images must be the same format, width, and height
            return E_FAIL;
        }

        baseImages.push_back(src);
    }

    assert(baseImages.size() == metadata.arraySize);

    HRESULT hr = E_UNEXPECTED;

    if (baseImages.empty())
        return hr;

    static_assert(TEX_FILTER_POINT == 0x100000, "TEX_FILTER_ flag values don't match TEX_FILTER_MODE_MASK");

#ifdef WIN32
    bool usewic = !metadata.IsPMAlpha() && UseWICFiltering(metadata.format, filter);

    WICPixelFormatGUID pfGUID = {};
    const bool wicpf = (usewic) ? DXGIToWIC(metadata.format, pfGUID, true) : false;

    if (usewic && !wicpf)
    {
        // Check to see if the source and/or result size is too big for WIC
        const uint64_t expandedSize = uint64_t(std::max<size_t>(1, metadata.width >> 1)) * uint64_t(std::max<size_t>(1, metadata.height >> 1)) * sizeof(float) * 4;
        const uint64_t expandedSize2 = uint64_t(metadata.width) * uint64_t(metadata.height) * sizeof(float) * 4;
        if (expandedSize > UINT32_MAX || expandedSize2 > UINT32_MAX)
        {
            if (filter & TEX_FILTER_FORCE_WIC)
                return HRESULT_E_ARITHMETIC_OVERFLOW;

            usewic = false;
        }
    }

    if (usewic)
    {
        //--- Use WIC filtering to generate mipmaps -----------------------------------
        switch (filter & TEX_FILTER_MODE_MASK)
        {
        case 0:
        case TEX_FILTER_POINT:
        case TEX_FILTER_FANT: // Equivalent to Box filter
        case TEX_FILTER_LINEAR:
        case TEX_FILTER_CUBIC:
        {
            static_assert(TEX_FILTER_FANT == TEX_FILTER_BOX, "TEX_FILTER_ flag alias mismatch");

            if (wicpf)
            {
                // Case 1: Base image format is supported by Windows Imaging Component
                TexMetadata mdata2 = metadata;
                mdata2.mipLevels = levels;
                hr = mipChain.Initialize(mdata2);
                if (FAILED(hr))
                    return hr;

                for (size_t item = 0; item < metadata.arraySize; ++item)
                {
                    hr = GenerateMipMapsUsingWIC(baseImages[item], filter, levels, pfGUID, mipChain, item);
                    if (FAILED(hr))
                    {
                        mipChain.Release();
                        return hr;
                    }
                }

                return S_OK;
            }
            else
            {
                // Case 2: Base image format is not supported by WIC, so we have to convert, generate, and convert back
                assert(metadata.format != DXGI_FORMAT_R32G32B32A32_FLOAT);

                TexMetadata mdata2 = metadata;
                mdata2.mipLevels = levels;
                mdata2.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                ScratchImage tMipChain;
                hr = tMipChain.Initialize(mdata2);
                if (FAILED(hr))
                    return hr;

                for (size_t item = 0; item < metadata.arraySize; ++item)
                {
                    ScratchImage temp;
                    hr = ConvertToR32G32B32A32(baseImages[item], temp);
                    if (FAILED(hr))
                        return hr;

                    const Image *timg = temp.GetImage(0, 0, 0);
                    if (!timg)
                        return E_POINTER;

                    hr = GenerateMipMapsUsingWIC(*timg, filter, levels, GUID_WICPixelFormat128bppRGBAFloat, tMipChain, item);
                    if (FAILED(hr))
                        return hr;
                }

                return ConvertFromR32G32B32A32(tMipChain.GetImages(), tMipChain.GetImageCount(), tMipChain.GetMetadata(), metadata.format, mipChain);
            }
        }

        default:
            return HRESULT_E_NOT_SUPPORTED;
        }
    }
    else
#endif // WIN32
    {
        //--- Use custom filters to generate mipmaps ----------------------------------
        TexMetadata mdata2 = metadata;
        mdata2.mipLevels = levels;

        unsigned long filter_select = (filter & TEX_FILTER_MODE_MASK);
        if (!filter_select)
        {
            // Default filter choice
            filter_select = (ispow2(metadata.width) && ispow2(metadata.height)) ? TEX_FILTER_BOX : TEX_FILTER_LINEAR;
        }

        switch (filter_select)
        {
        case TEX_FILTER_BOX:
            hr = Setup2DMips(&baseImages[0], metadata.arraySize, mdata2, mipChain);
            if (FAILED(hr))
                return hr;

            for (size_t item = 0; item < metadata.arraySize; ++item)
            {
                hr = Generate2DMipsBoxFilter(levels, filter, mipChain, item);
                if (FAILED(hr))
                    mipChain.Release();
            }
            return hr;

        case TEX_FILTER_POINT:
            hr = Setup2DMips(&baseImages[0], metadata.arraySize, mdata2, mipChain);
            if (FAILED(hr))
                return hr;

            for (size_t item = 0; item < metadata.arraySize; ++item)
            {
                hr = Generate2DMipsPointFilter(levels, mipChain, item);
                if (FAILED(hr))
                    mipChain.Release();
            }
            return hr;

        case TEX_FILTER_LINEAR:
            hr = Setup2DMips(&baseImages[0], metadata.arraySize, mdata2, mipChain);
            if (FAILED(hr))
                return hr;

            for (size_t item = 0; item < metadata.arraySize; ++item)
            {
                hr = Generate2DMipsLinearFilter(levels, filter, mipChain, item);
                if (FAILED(hr))
                    mipChain.Release();
            }
            return hr;

        case TEX_FILTER_CUBIC:
            hr = Setup2DMips(&baseImages[0], metadata.arraySize, mdata2, mipChain);
            if (FAILED(hr))
                return hr;

            for (size_t item = 0; item < metadata.arraySize; ++item)
            {
                hr = Generate2DMipsCubicFilter(levels, filter, mipChain, item);
                if (FAILED(hr))
                    mipChain.Release();
            }
            return hr;

        case TEX_FILTER_TRIANGLE:
            hr = Setup2DMips(&baseImages[0], metadata.arraySize, mdata2, mipChain);
            if (FAILED(hr))
                return hr;

            for (size_t item = 0; item < metadata.arraySize; ++item)
            {
                hr = Generate2DMipsTriangleFilter(levels, filter, mipChain, item);
                if (FAILED(hr))
                    mipChain.Release();
            }
            return hr;

        default:
            return HRESULT_E_NOT_SUPPORTED;
        }
    }
}


//-------------------------------------------------------------------------------------
// Generate mipmap chain for volume texture
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::GenerateMipMaps3D(
    const Image* baseImages,
    size_t depth,
    TEX_FILTER_FLAGS filter,
    size_t levels,
    ScratchImage& mipChain) noexcept
{
    if (!baseImages || !depth)
        return E_INVALIDARG;

    if (filter & TEX_FILTER_FORCE_WIC)
        return HRESULT_E_NOT_SUPPORTED;

    const DXGI_FORMAT format = baseImages[0].format;
    const size_t width = baseImages[0].width;
    const size_t height = baseImages[0].height;

    if (!CalculateMipLevels3D(width, height, depth, levels))
        return E_INVALIDARG;

    if (levels <= 1)
        return E_INVALIDARG;

    for (size_t slice = 0; slice < depth; ++slice)
    {
        if (!baseImages[slice].pixels)
            return E_POINTER;

        if (baseImages[slice].format != format || baseImages[slice].width != width || baseImages[slice].height != height)
        {
            // All base images must be the same format, width, and height
            return E_FAIL;
        }
    }

    if (IsCompressed(format) || IsTypeless(format) || IsPlanar(format) || IsPalettized(format))
        return HRESULT_E_NOT_SUPPORTED;

    static_assert(TEX_FILTER_POINT == 0x100000, "TEX_FILTER_ flag values don't match TEX_FILTER_MODE_MASK");

    HRESULT hr = E_UNEXPECTED;

    unsigned long filter_select = (filter & TEX_FILTER_MODE_MASK);
    if (!filter_select)
    {
        // Default filter choice
        filter_select = (ispow2(width) && ispow2(height) && ispow2(depth)) ? TEX_FILTER_BOX : TEX_FILTER_TRIANGLE;
    }

    switch (filter_select)
    {
    case TEX_FILTER_BOX:
        hr = Setup3DMips(baseImages, depth, levels, mipChain);
        if (FAILED(hr))
            return hr;

        hr = Generate3DMipsBoxFilter(depth, levels, filter, mipChain);
        if (FAILED(hr))
            mipChain.Release();
        return hr;

    case TEX_FILTER_POINT:
        hr = Setup3DMips(baseImages, depth, levels, mipChain);
        if (FAILED(hr))
            return hr;

        hr = Generate3DMipsPointFilter(depth, levels, mipChain);
        if (FAILED(hr))
            mipChain.Release();
        return hr;

    case TEX_FILTER_LINEAR:
        hr = Setup3DMips(baseImages, depth, levels, mipChain);
        if (FAILED(hr))
            return hr;

        hr = Generate3DMipsLinearFilter(depth, levels, filter, mipChain);
        if (FAILED(hr))
            mipChain.Release();
        return hr;

    case TEX_FILTER_CUBIC:
        hr = Setup3DMips(baseImages, depth, levels, mipChain);
        if (FAILED(hr))
            return hr;

        hr = Generate3DMipsCubicFilter(depth, levels, filter, mipChain);
        if (FAILED(hr))
            mipChain.Release();
        return hr;

    case TEX_FILTER_TRIANGLE:
        hr = Setup3DMips(baseImages, depth, levels, mipChain);
        if (FAILED(hr))
            return hr;

        hr = Generate3DMipsTriangleFilter(depth, levels, filter, mipChain);
        if (FAILED(hr))
            mipChain.Release();
        return hr;

    default:
        return HRESULT_E_NOT_SUPPORTED;
    }
}

_Use_decl_annotations_
HRESULT DirectX::GenerateMipMaps3D(
    const Image* srcImages,
    size_t nimages,
    const TexMetadata& metadata,
    TEX_FILTER_FLAGS filter,
    size_t levels,
    ScratchImage& mipChain)
{
    if (!srcImages || !nimages || !IsValid(metadata.format))
        return E_INVALIDARG;

    if (filter & TEX_FILTER_FORCE_WIC)
        return HRESULT_E_NOT_SUPPORTED;

    if (!metadata.IsVolumemap()
        || IsCompressed(metadata.format) || IsTypeless(metadata.format) || IsPlanar(metadata.format) || IsPalettized(metadata.format))
        return HRESULT_E_NOT_SUPPORTED;

    if (!CalculateMipLevels3D(metadata.width, metadata.height, metadata.depth, levels))
        return E_INVALIDARG;

    if (levels <= 1)
        return E_INVALIDARG;

    std::vector<Image> baseImages;
    baseImages.reserve(metadata.depth);
    for (size_t slice = 0; slice < metadata.depth; ++slice)
    {
        const size_t index = metadata.ComputeIndex(0, 0, slice);
        if (index >= nimages)
            return E_FAIL;

        const Image& src = srcImages[index];
        if (!src.pixels)
            return E_POINTER;

        if (src.format != metadata.format || src.width != metadata.width || src.height != metadata.height)
        {
            // All base images must be the same format, width, and height
            return E_FAIL;
        }

        baseImages.push_back(src);
    }

    assert(baseImages.size() == metadata.depth);

    HRESULT hr = E_UNEXPECTED;

    static_assert(TEX_FILTER_POINT == 0x100000, "TEX_FILTER_ flag values don't match TEX_FILTER_MODE_MASK");

    unsigned long filter_select = (filter & TEX_FILTER_MODE_MASK);
    if (!filter_select)
    {
        // Default filter choice
        filter_select = (ispow2(metadata.width) && ispow2(metadata.height) && ispow2(metadata.depth)) ? TEX_FILTER_BOX : TEX_FILTER_TRIANGLE;
    }

    switch (filter_select)
    {
    case TEX_FILTER_BOX:
        hr = Setup3DMips(&baseImages[0], metadata.depth, levels, mipChain);
        if (FAILED(hr))
            return hr;

        hr = Generate3DMipsBoxFilter(metadata.depth, levels, filter, mipChain);
        if (FAILED(hr))
            mipChain.Release();
        return hr;

    case TEX_FILTER_POINT:
        hr = Setup3DMips(&baseImages[0], metadata.depth, levels, mipChain);
        if (FAILED(hr))
            return hr;

        hr = Generate3DMipsPointFilter(metadata.depth, levels, mipChain);
        if (FAILED(hr))
            mipChain.Release();
        return hr;

    case TEX_FILTER_LINEAR:
        hr = Setup3DMips(&baseImages[0], metadata.depth, levels, mipChain);
        if (FAILED(hr))
            return hr;

        hr = Generate3DMipsLinearFilter(metadata.depth, levels, filter, mipChain);
        if (FAILED(hr))
            mipChain.Release();
        return hr;

    case TEX_FILTER_CUBIC:
        hr = Setup3DMips(&baseImages[0], metadata.depth, levels, mipChain);
        if (FAILED(hr))
            return hr;

        hr = Generate3DMipsCubicFilter(metadata.depth, levels, filter, mipChain);
        if (FAILED(hr))
            mipChain.Release();
        return hr;

    case TEX_FILTER_TRIANGLE:
        hr = Setup3DMips(&baseImages[0], metadata.depth, levels, mipChain);
        if (FAILED(hr))
            return hr;

        hr = Generate3DMipsTriangleFilter(metadata.depth, levels, filter, mipChain);
        if (FAILED(hr))
            mipChain.Release();
        return hr;

    default:
        return HRESULT_E_NOT_SUPPORTED;
    }
}

_Use_decl_annotations_
HRESULT DirectX::ScaleMipMapsAlphaForCoverage(
    const Image* srcImages,
    size_t nimages,
    const TexMetadata& metadata,
    size_t item,
    float alphaReference,
    ScratchImage& mipChain) noexcept
{
    if (!srcImages || !nimages || !IsValid(metadata.format) || nimages > metadata.mipLevels || !mipChain.GetImages())
        return E_INVALIDARG;

    if (metadata.IsVolumemap()
        || IsCompressed(metadata.format) || IsTypeless(metadata.format) || IsPlanar(metadata.format) || IsPalettized(metadata.format))
        return HRESULT_E_NOT_SUPPORTED;

    if (srcImages[0].format != metadata.format || srcImages[0].width != metadata.width || srcImages[0].height != metadata.height)
    {
        // Base image must be the same format, width, and height
        return E_FAIL;
    }

    float targetCoverage = 0.0f;
    HRESULT hr = CalculateAlphaCoverage(srcImages[0], alphaReference, 1.0f, targetCoverage);
    if (FAILED(hr))
        return hr;

    // Copy base image
    {
        const Image& src = srcImages[0];

        const Image *dest = mipChain.GetImage(0, item, 0);
        if (!dest)
            return E_POINTER;

        uint8_t* pDest = dest->pixels;
        if (!pDest)
            return E_POINTER;

        const uint8_t *pSrc = src.pixels;
        const size_t rowPitch = src.rowPitch;
        for (size_t h = 0; h < metadata.height; ++h)
        {
            const size_t msize = std::min<size_t>(dest->rowPitch, rowPitch);
            memcpy(pDest, pSrc, msize);
            pSrc += rowPitch;
            pDest += dest->rowPitch;
        }
    }

    for (size_t level = 1; level < metadata.mipLevels; ++level)
    {
        if (level >= nimages)
            return E_FAIL;

        float alphaScale = 0.0f;
        hr = EstimateAlphaScaleForCoverage(srcImages[level], alphaReference, targetCoverage, alphaScale);
        if (FAILED(hr))
            return hr;

        const Image* mipImage = mipChain.GetImage(level, item, 0);
        if (!mipImage)
            return E_POINTER;

        hr = ScaleAlpha(srcImages[level], alphaScale, *mipImage);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}
