//-------------------------------------------------------------------------------------
// DirectXTexResize.cpp
//
// DirectX Texture Library - Image resizing operations
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
#ifdef _WIN32
    //--- Do image resize using WIC ---
    HRESULT PerformResizeUsingWIC(
        const Image& srcImage,
        TEX_FILTER_FLAGS filter,
        const WICPixelFormatGUID& pfGUID,
        const Image& destImage) noexcept
    {
        if (!srcImage.pixels || !destImage.pixels)
            return E_POINTER;

        assert(srcImage.format == destImage.format);

        bool iswic2 = false;
        auto pWIC = GetWICFactory(iswic2);
        if (!pWIC)
            return E_NOINTERFACE;

        ComPtr<IWICComponentInfo> componentInfo;
        HRESULT hr = pWIC->CreateComponentInfo(pfGUID, componentInfo.GetAddressOf());
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

        if (srcImage.rowPitch > UINT32_MAX || srcImage.slicePitch > UINT32_MAX
            || destImage.rowPitch > UINT32_MAX || destImage.slicePitch > UINT32_MAX)
            return HRESULT_E_ARITHMETIC_OVERFLOW;

        ComPtr<IWICBitmap> source;
        hr = pWIC->CreateBitmapFromMemory(static_cast<UINT>(srcImage.width), static_cast<UINT>(srcImage.height), pfGUID,
            static_cast<UINT>(srcImage.rowPitch), static_cast<UINT>(srcImage.slicePitch),
            srcImage.pixels, source.GetAddressOf());
        if (FAILED(hr))
            return hr;

        if ((filter & TEX_FILTER_SEPARATE_ALPHA) && supportsTransparency)
        {
            hr = ResizeSeparateColorAndAlpha(pWIC, iswic2, source.Get(), destImage.width, destImage.height, filter, &destImage);
            if (FAILED(hr))
                return hr;
        }
        else
        {
            ComPtr<IWICBitmapScaler> scaler;
            hr = pWIC->CreateBitmapScaler(scaler.GetAddressOf());
            if (FAILED(hr))
                return hr;

            hr = scaler->Initialize(source.Get(),
                static_cast<UINT>(destImage.width), static_cast<UINT>(destImage.height),
                GetWICInterp(filter));
            if (FAILED(hr))
                return hr;

            WICPixelFormatGUID pfScaler;
            hr = scaler->GetPixelFormat(&pfScaler);
            if (FAILED(hr))
                return hr;

            if (memcmp(&pfScaler, &pfGUID, sizeof(WICPixelFormatGUID)) == 0)
            {
                hr = scaler->CopyPixels(nullptr, static_cast<UINT>(destImage.rowPitch), static_cast<UINT>(destImage.slicePitch), destImage.pixels);
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

                hr = FC->CopyPixels(nullptr, static_cast<UINT>(destImage.rowPitch), static_cast<UINT>(destImage.slicePitch), destImage.pixels);
                if (FAILED(hr))
                    return hr;
            }
        }

        return S_OK;
    }


    //--- Do conversion, resize using WIC, conversion cycle ---
    HRESULT PerformResizeViaF32(
        const Image& srcImage,
        TEX_FILTER_FLAGS filter,
        const Image& destImage) noexcept
    {
        if (!srcImage.pixels || !destImage.pixels)
            return E_POINTER;

        assert(srcImage.format != DXGI_FORMAT_R32G32B32A32_FLOAT);
        assert(srcImage.format == destImage.format);

        ScratchImage temp;
        HRESULT hr = ConvertToR32G32B32A32(srcImage, temp);
        if (FAILED(hr))
            return hr;

        const Image *tsrc = temp.GetImage(0, 0, 0);
        if (!tsrc)
            return E_POINTER;

        ScratchImage rtemp;
        hr = rtemp.Initialize2D(DXGI_FORMAT_R32G32B32A32_FLOAT, destImage.width, destImage.height, 1, 1);
        if (FAILED(hr))
            return hr;

        const Image *tdest = rtemp.GetImage(0, 0, 0);
        if (!tdest)
            return E_POINTER;

        hr = PerformResizeUsingWIC(*tsrc, filter, GUID_WICPixelFormat128bppRGBAFloat, *tdest);
        if (FAILED(hr))
            return hr;

        temp.Release();

        hr = ConvertFromR32G32B32A32(*tdest, destImage);
        if (FAILED(hr))
            return hr;

        return S_OK;
    }


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

        static_assert(TEX_FILTER_POINT == 0x100000, "TEX_FILTER_ flag values don't match TEX_FILTER_MASK");

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

        default:
            if (BitsPerColor(format) > 8)
            {
                // Avoid the WIC bitmap scaler when doing filtering of XR/HDR formats
                return false;
            }
            break;
        }

        return true;
    }
#endif // WIN32

    //-------------------------------------------------------------------------------------
    // Resize custom filters
    //-------------------------------------------------------------------------------------

    //--- Point Filter ---
    HRESULT ResizePointFilter(const Image& srcImage, const Image& destImage) noexcept
    {
        assert(srcImage.pixels && destImage.pixels);
        assert(srcImage.format == destImage.format);

        // Allocate temporary space (2 scanlines)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(srcImage.width) + destImage.width);
        if (!scanline)
            return E_OUTOFMEMORY;

        XMVECTOR* target = scanline.get();

        XMVECTOR* row = target + destImage.width;

    #ifdef _DEBUG
        memset(row, 0xCD, sizeof(XMVECTOR)*srcImage.width);
    #endif

        const uint8_t* pSrc = srcImage.pixels;
        uint8_t* pDest = destImage.pixels;

        const size_t rowPitch = srcImage.rowPitch;

        const size_t xinc = (srcImage.width << 16) / destImage.width;
        const size_t yinc = (srcImage.height << 16) / destImage.height;

        size_t lasty = size_t(-1);

        size_t sy = 0;
        for (size_t y = 0; y < destImage.height; ++y)
        {
            if ((lasty ^ sy) >> 16)
            {
                if (!LoadScanline(row, srcImage.width, pSrc + (rowPitch * (sy >> 16)), rowPitch, srcImage.format))
                    return E_FAIL;
                lasty = sy;
            }

            size_t sx = 0;
            for (size_t x = 0; x < destImage.width; ++x)
            {
                target[x] = row[sx >> 16];
                sx += xinc;
            }

            if (!StoreScanline(pDest, destImage.rowPitch, destImage.format, target, destImage.width))
                return E_FAIL;
            pDest += destImage.rowPitch;

            sy += yinc;
        }

        return S_OK;
    }


    //--- Box Filter ---
    HRESULT ResizeBoxFilter(const Image& srcImage, TEX_FILTER_FLAGS filter, const Image& destImage) noexcept
    {
        using namespace DirectX::Filters;

        assert(srcImage.pixels && destImage.pixels);
        assert(srcImage.format == destImage.format);

        if (((destImage.width << 1) != srcImage.width) || ((destImage.height << 1) != srcImage.height))
            return E_FAIL;

        // Allocate temporary space (3 scanlines)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(srcImage.width) * 2 + destImage.width);
        if (!scanline)
            return E_OUTOFMEMORY;

        XMVECTOR* target = scanline.get();

        XMVECTOR* urow0 = target + destImage.width;
        XMVECTOR* urow1 = urow0 + srcImage.width;

    #ifdef _DEBUG
        memset(urow0, 0xCD, sizeof(XMVECTOR)*srcImage.width);
        memset(urow1, 0xDD, sizeof(XMVECTOR)*srcImage.width);
    #endif

        const XMVECTOR* urow2 = urow0 + 1;
        const XMVECTOR* urow3 = urow1 + 1;

        const uint8_t* pSrc = srcImage.pixels;
        uint8_t* pDest = destImage.pixels;

        const size_t rowPitch = srcImage.rowPitch;

        for (size_t y = 0; y < destImage.height; ++y)
        {
            if (!LoadScanlineLinear(urow0, srcImage.width, pSrc, rowPitch, srcImage.format, filter))
                return E_FAIL;
            pSrc += rowPitch;

            if (urow0 != urow1)
            {
                if (!LoadScanlineLinear(urow1, srcImage.width, pSrc, rowPitch, srcImage.format, filter))
                    return E_FAIL;
                pSrc += rowPitch;
            }

            for (size_t x = 0; x < destImage.width; ++x)
            {
                const size_t x2 = x << 1;

                AVERAGE4(target[x], urow0[x2], urow1[x2], urow2[x2], urow3[x2])
            }

            if (!StoreScanlineLinear(pDest, destImage.rowPitch, destImage.format, target, destImage.width, filter))
                return E_FAIL;
            pDest += destImage.rowPitch;
        }

        return S_OK;
    }


    //--- Linear Filter ---
    HRESULT ResizeLinearFilter(const Image& srcImage, TEX_FILTER_FLAGS filter, const Image& destImage) noexcept
    {
        using namespace DirectX::Filters;

        assert(srcImage.pixels && destImage.pixels);
        assert(srcImage.format == destImage.format);

        // Allocate temporary space (3 scanlines, plus X and Y filters)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(srcImage.width) * 2 + destImage.width);
        if (!scanline)
            return E_OUTOFMEMORY;

        std::unique_ptr<LinearFilter[]> lf(new (std::nothrow) LinearFilter[destImage.width + destImage.height]);
        if (!lf)
            return E_OUTOFMEMORY;

        LinearFilter* lfX = lf.get();
        LinearFilter* lfY = lf.get() + destImage.width;

        CreateLinearFilter(srcImage.width, destImage.width, (filter & TEX_FILTER_WRAP_U) != 0, lfX);
        CreateLinearFilter(srcImage.height, destImage.height, (filter & TEX_FILTER_WRAP_V) != 0, lfY);

        XMVECTOR* target = scanline.get();

        XMVECTOR* row0 = target + destImage.width;
        XMVECTOR* row1 = row0 + srcImage.width;

    #ifdef _DEBUG
        memset(row0, 0xCD, sizeof(XMVECTOR)*srcImage.width);
        memset(row1, 0xDD, sizeof(XMVECTOR)*srcImage.width);
    #endif

        const uint8_t* pSrc = srcImage.pixels;
        uint8_t* pDest = destImage.pixels;

        const size_t rowPitch = srcImage.rowPitch;

        size_t u0 = size_t(-1);
        size_t u1 = size_t(-1);

        for (size_t y = 0; y < destImage.height; ++y)
        {
            auto const& toY = lfY[y];

            if (toY.u0 != u0)
            {
                if (toY.u0 != u1)
                {
                    u0 = toY.u0;

                    if (!LoadScanlineLinear(row0, srcImage.width, pSrc + (rowPitch * u0), rowPitch, srcImage.format, filter))
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

                if (!LoadScanlineLinear(row1, srcImage.width, pSrc + (rowPitch * u1), rowPitch, srcImage.format, filter))
                    return E_FAIL;
            }

            for (size_t x = 0; x < destImage.width; ++x)
            {
                auto const& toX = lfX[x];

                BILINEAR_INTERPOLATE(target[x], toX, toY, row0, row1)
            }

            if (!StoreScanlineLinear(pDest, destImage.rowPitch, destImage.format, target, destImage.width, filter))
                return E_FAIL;
            pDest += destImage.rowPitch;
        }

        return S_OK;
    }


    //--- Cubic Filter ---
#ifdef __clang__
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#endif

    HRESULT ResizeCubicFilter(const Image& srcImage, TEX_FILTER_FLAGS filter, const Image& destImage) noexcept
    {
        using namespace DirectX::Filters;

        assert(srcImage.pixels && destImage.pixels);
        assert(srcImage.format == destImage.format);

        // Allocate temporary space (5 scanlines, plus X and Y filters)
        auto scanline = make_AlignedArrayXMVECTOR(uint64_t(srcImage.width) * 4 + destImage.width);
        if (!scanline)
            return E_OUTOFMEMORY;

        std::unique_ptr<CubicFilter[]> cf(new (std::nothrow) CubicFilter[destImage.width + destImage.height]);
        if (!cf)
            return E_OUTOFMEMORY;

        CubicFilter* cfX = cf.get();
        CubicFilter* cfY = cf.get() + destImage.width;

        CreateCubicFilter(srcImage.width, destImage.width, (filter & TEX_FILTER_WRAP_U) != 0, (filter & TEX_FILTER_MIRROR_U) != 0, cfX);
        CreateCubicFilter(srcImage.height, destImage.height, (filter & TEX_FILTER_WRAP_V) != 0, (filter & TEX_FILTER_MIRROR_V) != 0, cfY);

        XMVECTOR* target = scanline.get();

        XMVECTOR* row0 = target + destImage.width;
        XMVECTOR* row1 = row0 + srcImage.width;
        XMVECTOR* row2 = row0 + srcImage.width * 2;
        XMVECTOR* row3 = row0 + srcImage.width * 3;

    #ifdef _DEBUG
        memset(row0, 0xCD, sizeof(XMVECTOR)*srcImage.width);
        memset(row1, 0xDD, sizeof(XMVECTOR)*srcImage.width);
        memset(row2, 0xED, sizeof(XMVECTOR)*srcImage.width);
        memset(row3, 0xFD, sizeof(XMVECTOR)*srcImage.width);
    #endif

        const uint8_t* pSrc = srcImage.pixels;
        uint8_t* pDest = destImage.pixels;

        const size_t rowPitch = srcImage.rowPitch;

        size_t u0 = size_t(-1);
        size_t u1 = size_t(-1);
        size_t u2 = size_t(-1);
        size_t u3 = size_t(-1);

        for (size_t y = 0; y < destImage.height; ++y)
        {
            auto const& toY = cfY[y];

            // Scanline 1
            if (toY.u0 != u0)
            {
                if (toY.u0 != u1 && toY.u0 != u2 && toY.u0 != u3)
                {
                    u0 = toY.u0;

                    if (!LoadScanlineLinear(row0, srcImage.width, pSrc + (rowPitch * u0), rowPitch, srcImage.format, filter))
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

                    if (!LoadScanlineLinear(row1, srcImage.width, pSrc + (rowPitch * u1), rowPitch, srcImage.format, filter))
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

                    if (!LoadScanlineLinear(row2, srcImage.width, pSrc + (rowPitch * u2), rowPitch, srcImage.format, filter))
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

                if (!LoadScanlineLinear(row3, srcImage.width, pSrc + (rowPitch * u3), rowPitch, srcImage.format, filter))
                    return E_FAIL;
            }

            for (size_t x = 0; x < destImage.width; ++x)
            {
                auto const& toX = cfX[x];

                XMVECTOR C0, C1, C2, C3;

                CUBIC_INTERPOLATE(C0, toX.x, row0[toX.u0], row0[toX.u1], row0[toX.u2], row0[toX.u3]);
                CUBIC_INTERPOLATE(C1, toX.x, row1[toX.u0], row1[toX.u1], row1[toX.u2], row1[toX.u3]);
                CUBIC_INTERPOLATE(C2, toX.x, row2[toX.u0], row2[toX.u1], row2[toX.u2], row2[toX.u3]);
                CUBIC_INTERPOLATE(C3, toX.x, row3[toX.u0], row3[toX.u1], row3[toX.u2], row3[toX.u3]);

                CUBIC_INTERPOLATE(target[x], toY.x, C0, C1, C2, C3);
            }

            if (!StoreScanlineLinear(pDest, destImage.rowPitch, destImage.format, target, destImage.width, filter))
                return E_FAIL;
            pDest += destImage.rowPitch;
        }

        return S_OK;
    }


    //--- Triangle Filter ---
    HRESULT ResizeTriangleFilter(const Image& srcImage, TEX_FILTER_FLAGS filter, const Image& destImage) noexcept
    {
        using namespace DirectX::Filters;

        assert(srcImage.pixels && destImage.pixels);
        assert(srcImage.format == destImage.format);

        // Allocate initial temporary space (1 scanline, accumulation rows, plus X and Y filters)
        auto scanline = make_AlignedArrayXMVECTOR(srcImage.width);
        if (!scanline)
            return E_OUTOFMEMORY;

        std::unique_ptr<TriangleRow[]> rowActive(new (std::nothrow) TriangleRow[destImage.height]);
        if (!rowActive)
            return E_OUTOFMEMORY;

        TriangleRow * rowFree = nullptr;

        std::unique_ptr<Filter> tfX;
        HRESULT hr = CreateTriangleFilter(srcImage.width, destImage.width, (filter & TEX_FILTER_WRAP_U) != 0, tfX);
        if (FAILED(hr))
            return hr;

        std::unique_ptr<Filter> tfY;
        hr = CreateTriangleFilter(srcImage.height, destImage.height, (filter & TEX_FILTER_WRAP_V) != 0, tfY);
        if (FAILED(hr))
            return hr;

        XMVECTOR* row = scanline.get();

    #ifdef _DEBUG
        memset(row, 0xCD, sizeof(XMVECTOR)*srcImage.width);
    #endif

        auto xFromEnd = reinterpret_cast<const FilterFrom*>(reinterpret_cast<const uint8_t*>(tfX.get()) + tfX->sizeInBytes);
        auto yFromEnd = reinterpret_cast<const FilterFrom*>(reinterpret_cast<const uint8_t*>(tfY.get()) + tfY->sizeInBytes);

        // Count times rows get written
        for (FilterFrom* yFrom = tfY->from; yFrom < yFromEnd; )
        {
            for (size_t j = 0; j < yFrom->count; ++j)
            {
                const size_t v = yFrom->to[j].u;
                assert(v < destImage.height);
                ++rowActive[v].remaining;
            }

            yFrom = reinterpret_cast<FilterFrom*>(reinterpret_cast<uint8_t*>(yFrom) + yFrom->sizeInBytes);
        }

        // Filter image
        const uint8_t* pSrc = srcImage.pixels;
        const size_t rowPitch = srcImage.rowPitch;
        const uint8_t* pEndSrc = pSrc + rowPitch * srcImage.height;

        uint8_t* pDest = destImage.pixels;

        for (FilterFrom* yFrom = tfY->from; yFrom < yFromEnd; )
        {
            // Create accumulation rows as needed
            for (size_t j = 0; j < yFrom->count; ++j)
            {
                const size_t v = yFrom->to[j].u;
                assert(v < destImage.height);
                TriangleRow* rowAcc = &rowActive[v];

                if (!rowAcc->scanline)
                {
                    if (rowFree)
                    {
                        // Steal and reuse scanline from 'free row' list
                        assert(rowFree->scanline != nullptr);
                        rowAcc->scanline.reset(rowFree->scanline.release());
                        rowFree = rowFree->next;
                    }
                    else
                    {
                        auto nscanline = make_AlignedArrayXMVECTOR(destImage.width);
                        if (!nscanline)
                            return E_OUTOFMEMORY;
                        rowAcc->scanline.swap(nscanline);
                    }

                    memset(rowAcc->scanline.get(), 0, sizeof(XMVECTOR) * destImage.width);
                }
            }

            // Load source scanline
            if ((pSrc + rowPitch) > pEndSrc)
                return E_FAIL;

            if (!LoadScanlineLinear(row, srcImage.width, pSrc, rowPitch, srcImage.format, filter))
                return E_FAIL;

            pSrc += rowPitch;

            // Process row
            size_t x = 0;
            for (FilterFrom* xFrom = tfX->from; xFrom < xFromEnd; ++x)
            {
                for (size_t j = 0; j < yFrom->count; ++j)
                {
                    const size_t v = yFrom->to[j].u;
                    assert(v < destImage.height);
                    const float yweight = yFrom->to[j].weight;

                    XMVECTOR* accPtr = rowActive[v].scanline.get();
                    if (!accPtr)
                        return E_POINTER;

                    for (size_t k = 0; k < xFrom->count; ++k)
                    {
                        size_t u = xFrom->to[k].u;
                        assert(u < destImage.width);

                        const XMVECTOR weight = XMVectorReplicate(yweight * xFrom->to[k].weight);

                        assert(x < srcImage.width);
                        accPtr[u] = XMVectorMultiplyAdd(row[x], weight, accPtr[u]);
                    }
                }

                xFrom = reinterpret_cast<FilterFrom*>(reinterpret_cast<uint8_t*>(xFrom) + xFrom->sizeInBytes);
            }

            // Write completed accumulation rows
            for (size_t j = 0; j < yFrom->count; ++j)
            {
                size_t v = yFrom->to[j].u;
                assert(v < destImage.height);
                TriangleRow* rowAcc = &rowActive[v];

                assert(rowAcc->remaining > 0);
                --rowAcc->remaining;

                if (!rowAcc->remaining)
                {
                    XMVECTOR* pAccSrc = rowAcc->scanline.get();
                    if (!pAccSrc)
                        return E_POINTER;

                    switch (destImage.format)
                    {
                    case DXGI_FORMAT_R10G10B10A2_UNORM:
                    case DXGI_FORMAT_R10G10B10A2_UINT:
                        {
                            // Need to slightly bias results for floating-point error accumulation which can
                            // be visible with harshly quantized values
                            static const XMVECTORF32 Bias = { { { 0.f, 0.f, 0.f, 0.1f } } };

                            XMVECTOR* ptr = pAccSrc;
                            for (size_t i = 0; i < destImage.width; ++i, ++ptr)
                            {
                                *ptr = XMVectorAdd(*ptr, Bias);
                            }
                        }
                        break;

                    default:
                        break;
                    }

                    // This performs any required clamping
                    if (!StoreScanlineLinear(pDest + (destImage.rowPitch * v), destImage.rowPitch, destImage.format, pAccSrc, destImage.width, filter))
                        return E_FAIL;

                    // Put row on freelist to reuse it's allocated scanline
                    rowAcc->next = rowFree;
                    rowFree = rowAcc;
                }
            }

            yFrom = reinterpret_cast<FilterFrom*>(reinterpret_cast<uint8_t*>(yFrom) + yFrom->sizeInBytes);
        }

        return S_OK;
    }


    //--- Custom filter resize ---
    HRESULT PerformResizeUsingCustomFilters(const Image& srcImage, TEX_FILTER_FLAGS filter, const Image& destImage) noexcept
    {
        if (!srcImage.pixels || !destImage.pixels)
            return E_POINTER;

        static_assert(TEX_FILTER_POINT == 0x100000, "TEX_FILTER_ flag values don't match TEX_FILTER_MASK");

        unsigned long filter_select = filter & TEX_FILTER_MODE_MASK;
        if (!filter_select)
        {
            // Default filter choice
            filter_select = (((destImage.width << 1) == srcImage.width) && ((destImage.height << 1) == srcImage.height))
                ? TEX_FILTER_BOX : TEX_FILTER_LINEAR;
        }

        switch (filter_select)
        {
        case TEX_FILTER_POINT:
            return ResizePointFilter(srcImage, destImage);

        case TEX_FILTER_BOX:
            return ResizeBoxFilter(srcImage, filter, destImage);

        case TEX_FILTER_LINEAR:
            return ResizeLinearFilter(srcImage, filter, destImage);

        case TEX_FILTER_CUBIC:
            return ResizeCubicFilter(srcImage, filter, destImage);

        case TEX_FILTER_TRIANGLE:
            return ResizeTriangleFilter(srcImage, filter, destImage);

        default:
            return HRESULT_E_NOT_SUPPORTED;
        }
    }
}


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Resize image
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::Resize(
    const Image& srcImage,
    size_t width,
    size_t height,
    TEX_FILTER_FLAGS filter,
    ScratchImage& image) noexcept
{
    if (width == 0 || height == 0)
        return E_INVALIDARG;

    if ((srcImage.width > UINT32_MAX) || (srcImage.height > UINT32_MAX))
        return E_INVALIDARG;

    if ((width > UINT32_MAX) || (height > UINT32_MAX))
        return E_INVALIDARG;

    if (!srcImage.pixels)
        return E_POINTER;

    if (IsCompressed(srcImage.format))
    {
        // We don't support resizing compressed images
        return HRESULT_E_NOT_SUPPORTED;
    }

#ifdef _WIN32
    bool usewic = UseWICFiltering(srcImage.format, filter);

    WICPixelFormatGUID pfGUID = {};
    const bool wicpf = (usewic) ? DXGIToWIC(srcImage.format, pfGUID, true) : false;

    if (usewic && !wicpf)
    {
        // Check to see if the source and/or result size is too big for WIC
        const uint64_t expandedSize = uint64_t(width) * uint64_t(height) * sizeof(float) * 4;
        const uint64_t expandedSize2 = uint64_t(srcImage.width) * uint64_t(srcImage.height) * sizeof(float) * 4;
        if (expandedSize > UINT32_MAX || expandedSize2 > UINT32_MAX)
        {
            if (filter & TEX_FILTER_FORCE_WIC)
                return HRESULT_E_ARITHMETIC_OVERFLOW;

            usewic = false;
        }
    }
#endif // WIN32

    HRESULT hr = image.Initialize2D(srcImage.format, width, height, 1, 1);
    if (FAILED(hr))
        return hr;

    const Image *rimage = image.GetImage(0, 0, 0);
    if (!rimage)
        return E_POINTER;

#ifdef _WIN32
    if (usewic)
    {
        if (wicpf)
        {
            // Case 1: Source format is supported by Windows Imaging Component
            hr = PerformResizeUsingWIC(srcImage, filter, pfGUID, *rimage);
        }
        else
        {
            // Case 2: Source format is not supported by WIC, so we have to convert, resize, and convert back
            hr = PerformResizeViaF32(srcImage, filter, *rimage);
        }
    }
    else
    #endif
    {
        // Case 3: not using WIC resizing
        hr = PerformResizeUsingCustomFilters(srcImage, filter, *rimage);
    }

    if (FAILED(hr))
    {
        image.Release();
        return hr;
    }

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Resize image (complex)
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::Resize(
    const Image* srcImages,
    size_t nimages,
    const TexMetadata& metadata,
    size_t width,
    size_t height,
    TEX_FILTER_FLAGS filter,
    ScratchImage& result) noexcept
{
    if (!srcImages || !nimages || width == 0 || height == 0)
        return E_INVALIDARG;

    if ((width > UINT32_MAX) || (height > UINT32_MAX))
        return E_INVALIDARG;

    TexMetadata mdata2 = metadata;
    mdata2.width = width;
    mdata2.height = height;
    mdata2.mipLevels = 1;
    HRESULT hr = result.Initialize(mdata2);
    if (FAILED(hr))
        return hr;

#ifdef _WIN32
    bool usewic = !metadata.IsPMAlpha() && UseWICFiltering(metadata.format, filter);

    WICPixelFormatGUID pfGUID = {};
    const bool wicpf = (usewic) ? DXGIToWIC(metadata.format, pfGUID, true) : false;

    if (usewic && !wicpf)
    {
        // Check to see if the source and/or result size is too big for WIC
        const uint64_t expandedSize = uint64_t(width) * uint64_t(height) * sizeof(float) * 4;
        const uint64_t expandedSize2 = uint64_t(metadata.width) * uint64_t(metadata.height) * sizeof(float) * 4;
        if (expandedSize > UINT32_MAX || expandedSize2 > UINT32_MAX)
        {
            if (filter & TEX_FILTER_FORCE_WIC)
                return HRESULT_E_ARITHMETIC_OVERFLOW;

            usewic = false;
        }
    }
#endif

    switch (metadata.dimension)
    {
    case TEX_DIMENSION_TEXTURE1D:
    case TEX_DIMENSION_TEXTURE2D:
        assert(metadata.depth == 1);

        for (size_t item = 0; item < metadata.arraySize; ++item)
        {
            const size_t srcIndex = metadata.ComputeIndex(0, item, 0);
            if (srcIndex >= nimages)
            {
                result.Release();
                return E_FAIL;
            }

            const Image* srcimg = &srcImages[srcIndex];
            const Image* destimg = result.GetImage(0, item, 0);
            if (!srcimg || !destimg)
            {
                result.Release();
                return E_POINTER;
            }

            if (srcimg->format != metadata.format)
            {
                result.Release();
                return E_FAIL;
            }

            if ((srcimg->width > UINT32_MAX) || (srcimg->height > UINT32_MAX))
            {
                result.Release();
                return E_FAIL;
            }

        #ifdef _WIN32
            if (usewic)
            {
                if (wicpf)
                {
                    // Case 1: Source format is supported by Windows Imaging Component
                    hr = PerformResizeUsingWIC(*srcimg, filter, pfGUID, *destimg);
                }
                else
                {
                    // Case 2: Source format is not supported by WIC, so we have to convert, resize, and convert back
                    hr = PerformResizeViaF32(*srcimg, filter, *destimg);
                }
            }
            else
            #endif
            {
                // Case 3: not using WIC resizing
                hr = PerformResizeUsingCustomFilters(*srcimg, filter, *destimg);
            }

            if (FAILED(hr))
            {
                result.Release();
                return hr;
            }
        }
        break;

    case TEX_DIMENSION_TEXTURE3D:
        assert(metadata.arraySize == 1);

        for (size_t slice = 0; slice < metadata.depth; ++slice)
        {
            const size_t srcIndex = metadata.ComputeIndex(0, 0, slice);
            if (srcIndex >= nimages)
            {
                result.Release();
                return E_FAIL;
            }

            const Image* srcimg = &srcImages[srcIndex];
            const Image* destimg = result.GetImage(0, 0, slice);
            if (!srcimg || !destimg)
            {
                result.Release();
                return E_POINTER;
            }

            if (srcimg->format != metadata.format)
            {
                result.Release();
                return E_FAIL;
            }

            if ((srcimg->width > UINT32_MAX) || (srcimg->height > UINT32_MAX))
            {
                result.Release();
                return E_FAIL;
            }

        #ifdef _WIN32
            if (usewic)
            {
                if (wicpf)
                {
                    // Case 1: Source format is supported by Windows Imaging Component
                    hr = PerformResizeUsingWIC(*srcimg, filter, pfGUID, *destimg);
                }
                else
                {
                    // Case 2: Source format is not supported by WIC, so we have to convert, resize, and convert back
                    hr = PerformResizeViaF32(*srcimg, filter, *destimg);
                }
            }
            else
            #endif
            {
                // Case 3: not using WIC resizing
                hr = PerformResizeUsingCustomFilters(*srcimg, filter, *destimg);
            }

            if (FAILED(hr))
            {
                result.Release();
                return hr;
            }
        }
        break;

    default:
        result.Release();
        return E_FAIL;
    }

    return S_OK;
}
