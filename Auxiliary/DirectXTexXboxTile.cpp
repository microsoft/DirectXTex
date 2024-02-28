//--------------------------------------------------------------------------------------
// File: DirectXTexXboxTile.cpp
//
// DirectXTex Auxilary functions for converting from linear to Xbox tiling
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "DirectXTexP.h"
#include "DirectXTexXbox.h"

//#define VERBOSE

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::Internal;
using namespace Xbox;

namespace
{
    //----------------------------------------------------------------------------------
    inline HRESULT TileByElement1D(
        _In_reads_(nimages) const Image* const * images,
        size_t nimages,
        uint32_t level,
        _In_ XGTextureAddressComputer* computer,
        _In_ const XG_RESOURCE_LAYOUT& layout,
        const XboxImage& xbox,
        size_t bpp,
        size_t w,
        bool packed)
    {
        uint8_t* dptr = xbox.GetPointer();
        const uint8_t* endPtr = dptr + layout.SizeBytes;

        for (uint32_t item = 0; item < nimages; ++item)
        {
            const Image* img = images[item];

            if (!img || !img->pixels)
                return E_POINTER;

            assert(img->width == images[0]->width);
            assert(img->height == images[0]->height);
            assert(img->rowPitch == images[0]->rowPitch);
            assert(img->format == images[0]->format);

            const uint8_t* sptr = img->pixels;

            for (size_t x = 0; x < w; ++x)
            {
            #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
                UINT64 element = (packed) ? (x >> 1) : x;
                size_t offset = computer->GetTexelElementOffsetBytes(0, level, element, 0, item, 0, nullptr);
            #else
                size_t offset = computer->GetTexelElementOffsetBytes(0, level, x, 0, item, 0);
            #endif
                if (offset == size_t(-1))
                    return E_FAIL;

                uint8_t* dest = dptr + offset;

                if ((dest + bpp) > endPtr)
                    return E_FAIL;

                memcpy(dest, sptr, bpp);
                sptr += bpp;

                if (packed)
                    ++x;
            }
        }

        return S_OK;
    }

    //----------------------------------------------------------------------------------
#ifdef VERBOSE
    void DebugPrintDesc(const XG_TEXTURE1D_DESC& desc)
    {
        wchar_t buff[2048] = {};
        swprintf_s(buff, L"XG_TEXTURE1D_DESC = { %u, %u, %u, %u, %u, %u, %u, %u, %u, %u }\n",
            desc.Width, desc.MipLevels, desc.ArraySize, desc.Format, desc.Usage, desc.BindFlags, desc.CPUAccessFlags, desc.MiscFlags,
        #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
            desc.SwizzleMode,
        #else
            desc.TileMode,
        #endif
            desc.Pitch);
        OutputDebugStringW(buff);
    }

    void DebugPrintDesc(const XG_TEXTURE2D_DESC& desc)
    {
        wchar_t buff[2048] = {};
        swprintf_s(buff, L"XG_TEXTURE2D_DESC = { %u, %u, %u, %u, %u, { %u, %u }, %u, %u, %u, %u, %u, %u }\n",
            desc.Width, desc.Height, desc.MipLevels, desc.ArraySize, desc.Format, desc.SampleDesc.Count, desc.SampleDesc.Quality, desc.Usage, desc.BindFlags, desc.CPUAccessFlags, desc.MiscFlags,
        #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
            desc.SwizzleMode,
        #else
            desc.TileMode,
        #endif
            desc.Pitch);
        OutputDebugStringW(buff);
    }

    void DebugPrintDesc(const XG_TEXTURE3D_DESC& desc)
    {
        wchar_t buff[2048] = {};
        swprintf_s(buff, L"XG_TEXTURE3D_DESC = { %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u }\n",
            desc.Width, desc.Height, desc.Depth, desc.MipLevels, desc.Format, desc.Usage, desc.BindFlags, desc.CPUAccessFlags, desc.MiscFlags,
        #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
            desc.SwizzleMode,
        #else
            desc.TileMode,
        #endif
            desc.Pitch);
        OutputDebugStringW(buff);
    }

    void DebugPrintLayout(const XG_RESOURCE_LAYOUT& layout)
    {
        wchar_t buff[2048] = {};

        swprintf_s(buff, L"Layout %u planes, %uD, %u mips, %llu size, %llu alignment\n", layout.Planes, layout.Dimension - 1, layout.MipLevels, layout.SizeBytes, layout.BaseAlignmentBytes);
        OutputDebugStringW(buff);

        for (size_t p = 0; p < layout.Planes; ++p)
        {
            auto& plane = layout.Plane[p];

            swprintf_s(buff, L"Plane %zu: %u bpe, %llu size, %llu offset, %llu alignment\n", p, plane.BytesPerElement, plane.SizeBytes, plane.BaseOffsetBytes, plane.BaseAlignmentBytes);
            OutputDebugStringW(buff);

            for (size_t level = 0; level < layout.MipLevels; ++level)
            {
                auto& mip = plane.MipLayout[level];

                swprintf_s(buff, L"\tLevel %zu: %llu size, %llu slice2D, %llu offset, %u alignment\n", level, mip.SizeBytes, mip.Slice2DSizeBytes, mip.OffsetBytes, mip.AlignmentBytes);
                OutputDebugStringW(buff);

                swprintf_s(buff, L"\t\t%u x %u x %u (padded %u x %u x %u)\n", mip.WidthElements, mip.HeightElements, mip.DepthOrArraySize,
                    mip.PaddedWidthElements, mip.PaddedHeightElements, mip.PaddedDepthOrArraySize);
                OutputDebugStringW(buff);

                swprintf_s(buff, L"\t\tpitch %u pixels (%u bytes)\n", mip.PitchPixels, mip.PitchBytes);
                OutputDebugStringW(buff);

            #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
                swprintf_s(buff, L"\t\t\t%u samples, %u swizzlemode\n", mip.SampleCount, mip.SwizzleMode);
            #else
                swprintf_s(buff, L"\t\t\t%u samples, %u tilemode\n", mip.SampleCount, mip.TileMode);
            #endif
                OutputDebugStringW(buff);
            }
        }
    }
#endif

    //-------------------------------------------------------------------------------------
    // 1D Tiling
    //-------------------------------------------------------------------------------------
    HRESULT Tile1D(
        _In_reads_(nimages) const Image** images,
        size_t nimages,
        uint32_t level,
        _In_ XGTextureAddressComputer* computer,
        const XG_RESOURCE_LAYOUT& layout,
        const XboxImage& xbox)
    {
        if (!nimages)
            return E_INVALIDARG;

        if (!images || !images[0] || !computer || !xbox.GetPointer())
            return E_POINTER;

        assert(layout.Planes == 1);

        const DXGI_FORMAT format = images[0]->format;

        assert(format == xbox.GetMetadata().format);

        assert(!IsCompressed(format));

        bool byelement = IsTypeless(format);
    #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
        if (nimages > 1)
            byelement = true;
    #endif

        if (IsPacked(format))
        {
            const size_t bpp = (BitsPerPixel(format) + 7) / 8;

            // XG (XboxOne) incorrectly returns 2 instead of 4 here for layout.Plane[0].BytesPerElement

            const size_t w = images[0]->width;
            assert(((w + 1) / 2) == layout.Plane[0].MipLayout[level].WidthElements);

            return TileByElement1D(images, nimages, level, computer, layout, xbox, bpp, w, true);
        }
        else if (byelement)
        {
            //--- Typeless is done with per-element copy ----------------------------------
            const size_t bpp = (BitsPerPixel(format) + 7) / 8;
            assert(bpp == layout.Plane[0].BytesPerElement);

            const size_t w = images[0]->width;
            assert(w == layout.Plane[0].MipLayout[level].WidthElements);

            return TileByElement1D(images, nimages, level, computer, layout, xbox, bpp, w, false);
        }
        else
        {
            //--- Standard format handling ------------------------------------------------
            auto& mip = layout.Plane[0].MipLayout[level];

            const UINT32 tiledPixels = mip.PitchPixels * mip.PaddedDepthOrArraySize;

            auto scanline = make_AlignedArrayXMVECTOR(images[0]->width + tiledPixels);

            XMVECTOR* row = scanline.get();
            XMVECTOR* tiled = row + images[0]->width;

        #ifdef _DEBUG
            memset(row, 0xCD, sizeof(XMVECTOR) * images[0]->width);
        #endif

            memset(tiled, 0, sizeof(XMVECTOR) * tiledPixels);

            // Perform tiling
            for (uint32_t item = 0; item < nimages; ++item)
            {
                const Image* img = images[item];

                if (!img || !img->pixels)
                    return E_POINTER;

                assert(img->width == images[0]->width);
                assert(img->height == images[0]->height);
                assert(img->rowPitch == images[0]->rowPitch);
                assert(img->format == images[0]->format);

                if (!LoadScanline(row, img->width, img->pixels, img->rowPitch, img->format))
                    return E_FAIL;

                for (size_t x = 0; x < img->width; ++x)
                {
                #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
                    size_t offset = computer->GetTexelElementOffsetBytes(0, level, x, 0, item, 0, nullptr);
                #else
                    size_t offset = computer->GetTexelElementOffsetBytes(0, level, x, 0, item, 0);
                #endif
                    if (offset == size_t(-1))
                        return E_FAIL;

                    assert(offset >= mip.OffsetBytes);
                    assert(offset < mip.OffsetBytes + mip.SizeBytes);

                    offset = (offset - mip.OffsetBytes) / layout.Plane[0].BytesPerElement;
                    assert(offset < tiledPixels);

                    tiled[offset] = row[x];
                }
            }

            // Store tiled texture
            assert(mip.OffsetBytes + mip.SizeBytes <= layout.SizeBytes);
            if (!StoreScanline(xbox.GetPointer() + mip.OffsetBytes, mip.SizeBytes, xbox.GetMetadata().format, tiled, tiledPixels))
                return E_FAIL;
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // 2D Tiling
    //-------------------------------------------------------------------------------------
    HRESULT Tile2D(
        _In_reads_(nimages) const Image** images,
        size_t nimages,
        uint32_t level,
        _In_ XGTextureAddressComputer* computer,
        const XboxImage& xbox)
    {
        if (!nimages)
            return E_INVALIDARG;

        if (!images || !images[0] || !computer || !xbox.GetPointer())
            return E_POINTER;

        uint8_t* baseAddr = xbox.GetPointer();
        const auto& metadata = xbox.GetMetadata();

        for (uint32_t item = 0; item < nimages; ++item)
        {
            const Image* img = images[item];

            if (!img || !img->pixels)
                return E_POINTER;

            assert(img->width == images[0]->width);
            assert(img->height == images[0]->height);
            assert(img->rowPitch == images[0]->rowPitch);
            assert(img->format == images[0]->format);

            HRESULT hr = computer->CopyIntoSubresource(
                baseAddr,
                0u,
                metadata.CalculateSubresource(level, item),
                img->pixels,
                static_cast<UINT32>(img->rowPitch),
                0u);
            if (FAILED(hr))
            {
                return hr;
            }
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // 3D Tiling
    //-------------------------------------------------------------------------------------
    HRESULT Tile3D(
        const Image& image,
        uint32_t level,
        _In_ XGTextureAddressComputer* computer,
        const XboxImage& xbox)
    {
        if (!image.pixels || !computer || !xbox.GetPointer())
            return E_POINTER;

        uint8_t* baseAddr = xbox.GetPointer();
        const auto& metadata = xbox.GetMetadata();

        return computer->CopyIntoSubresource(
            baseAddr,
            0u,
            metadata.CalculateSubresource(level, 0),
            image.pixels,
            static_cast<UINT32>(image.rowPitch),
            static_cast<UINT32>(image.slicePitch));
    }
}


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Tile image
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Xbox::Tile(
    const DirectX::Image& srcImage,
    XboxImage& xbox,
    XboxTileMode mode)
{
    if (!srcImage.pixels
        || srcImage.width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
        || srcImage.height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return E_INVALIDARG;

    xbox.Release();

    if (srcImage.format == DXGI_FORMAT_R1_UNORM
        || IsVideo(srcImage.format))
    {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    if (mode == c_XboxTileModeInvalid)
    {
        // If no specific tile mode is given, assume the optimal default
    #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
        mode = XGComputeOptimalSwizzleMode(XG_RESOURCE_DIMENSION_TEXTURE2D, static_cast<XG_FORMAT>(srcImage.format),
            static_cast<UINT>(srcImage.width), static_cast<UINT>(srcImage.height),
            1, 1, XG_BIND_SHADER_RESOURCE);
    #else
        mode = XGComputeOptimalTileMode(XG_RESOURCE_DIMENSION_TEXTURE2D, static_cast<XG_FORMAT>(srcImage.format),
            static_cast<UINT>(srcImage.width), static_cast<UINT>(srcImage.height),
            1, 1, XG_BIND_SHADER_RESOURCE);
    #endif
    }

    XG_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(srcImage.width);
    desc.Height = static_cast<UINT>(srcImage.height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = static_cast<XG_FORMAT>(srcImage.format);
    desc.SampleDesc.Count = 1;
    desc.Usage = XG_USAGE_DEFAULT;
    desc.BindFlags = XG_BIND_SHADER_RESOURCE;
#if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
    desc.SwizzleMode = mode;
#else
    desc.TileMode = mode;
#endif

    ComPtr<XGTextureAddressComputer> computer;
    HRESULT hr = XGCreateTexture2DComputer(&desc, computer.GetAddressOf());
    if (FAILED(hr))
        return hr;

    XG_RESOURCE_LAYOUT layout;
    hr = computer->GetResourceLayout(&layout);
    if (FAILED(hr))
        return hr;

    if (layout.Planes != 1)
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    hr = xbox.Initialize(desc, layout);
    if (FAILED(hr))
        return hr;

    const Image* images = &srcImage;
    hr = Tile2D(&images, 1, 0, computer.Get(), xbox);
    if (FAILED(hr))
    {
        xbox.Release();
        return hr;
    }

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Tile image (complex)
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Xbox::Tile(
    const DirectX::Image* srcImages,
    size_t nimages,
    const DirectX::TexMetadata& metadata,
    XboxImage& xbox,
    XboxTileMode mode)
{
    if (!srcImages
        || !nimages
        || metadata.width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
        || metadata.height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
        || metadata.depth > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION
        || metadata.arraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION
        || metadata.mipLevels > D3D11_REQ_MIP_LEVELS)
        return E_INVALIDARG;

    xbox.Release();

    if (metadata.format == DXGI_FORMAT_R1_UNORM
        || IsVideo(metadata.format))
    {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    switch (metadata.format)
    {
    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    default:
        break;
    }

    if (mode == c_XboxTileModeInvalid)
    {
    #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
        mode = XGComputeOptimalSwizzleMode(static_cast<XG_RESOURCE_DIMENSION>(metadata.dimension), static_cast<XG_FORMAT>(metadata.format),
            static_cast<UINT>(metadata.width), static_cast<UINT>(metadata.height),
            static_cast<UINT>((metadata.dimension == TEX_DIMENSION_TEXTURE3D) ? metadata.depth : metadata.arraySize),
            1, XG_BIND_SHADER_RESOURCE);
    #else
            // If no specific tile mode is given, assume the optimal default
        mode = XGComputeOptimalTileMode(static_cast<XG_RESOURCE_DIMENSION>(metadata.dimension), static_cast<XG_FORMAT>(metadata.format),
            static_cast<UINT>(metadata.width), static_cast<UINT>(metadata.height),
            static_cast<UINT>((metadata.dimension == TEX_DIMENSION_TEXTURE3D) ? metadata.depth : metadata.arraySize),
            1, XG_BIND_SHADER_RESOURCE);
    #endif
    }

    XG_RESOURCE_LAYOUT layout = {};

    switch (metadata.dimension)
    {
    case TEX_DIMENSION_TEXTURE1D:
        {
            XG_TEXTURE1D_DESC desc = {};
            desc.Width = static_cast<UINT>(metadata.width);
            desc.MipLevels = static_cast<UINT>(metadata.mipLevels);
            desc.ArraySize = static_cast<UINT>(metadata.arraySize);
            desc.Format = static_cast<XG_FORMAT>(metadata.format);
            desc.Usage = XG_USAGE_DEFAULT;
            desc.BindFlags = XG_BIND_SHADER_RESOURCE;
            desc.MiscFlags = (metadata.IsCubemap()) ? XG_RESOURCE_MISC_TEXTURECUBE : 0;
        #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
            desc.SwizzleMode = mode;
        #else
            desc.TileMode = mode;
        #endif

        #ifdef VERBOSE
            DebugPrintDesc(desc);
        #endif

            ComPtr<XGTextureAddressComputer> computer;
            HRESULT hr = XGCreateTexture1DComputer(&desc, computer.GetAddressOf());
            if (FAILED(hr))
                return hr;

            hr = computer->GetResourceLayout(&layout);
            if (FAILED(hr))
                return hr;

        #ifdef VERBOSE
            DebugPrintLayout(layout);
        #endif

            if (layout.Planes != 1)
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

            hr = xbox.Initialize(desc, layout, metadata.miscFlags2);
            if (FAILED(hr))
                return hr;

            for (uint32_t level = 0; level < metadata.mipLevels; ++level)
            {
                if (metadata.arraySize > 1)
                {
                    std::vector<const Image*> images;
                    images.reserve(metadata.arraySize);
                    for (uint32_t item = 0; item < metadata.arraySize; ++item)
                    {
                        const size_t index = metadata.ComputeIndex(level, item, 0);
                        if (index >= nimages)
                        {
                            xbox.Release();
                            return E_FAIL;
                        }

                        images.push_back(&srcImages[index]);
                    }

                    hr = Tile1D(&images[0], images.size(), level, computer.Get(), layout, xbox);
                }
                else
                {
                    const size_t index = metadata.ComputeIndex(level, 0, 0);
                    if (index >= nimages)
                    {
                        xbox.Release();
                        return E_FAIL;
                    }

                    const Image* images = &srcImages[index];
                    hr = Tile1D(&images, 1, level, computer.Get(), layout, xbox);
                }

                if (FAILED(hr))
                {
                    xbox.Release();
                    return hr;
                }
            }
        }
        break;

    case TEX_DIMENSION_TEXTURE2D:
        {
            XG_TEXTURE2D_DESC desc = {};
            desc.Width = static_cast<UINT>(metadata.width);
            desc.Height = static_cast<UINT>(metadata.height);
            desc.MipLevels = static_cast<UINT>(metadata.mipLevels);
            desc.ArraySize = static_cast<UINT>(metadata.arraySize);
            desc.Format = static_cast<XG_FORMAT>(metadata.format);
            desc.SampleDesc.Count = 1;
            desc.Usage = XG_USAGE_DEFAULT;
            desc.BindFlags = XG_BIND_SHADER_RESOURCE;
            desc.MiscFlags = (metadata.miscFlags & TEX_MISC_TEXTURECUBE) ? XG_RESOURCE_MISC_TEXTURECUBE : 0;
        #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
            desc.SwizzleMode = mode;
        #else
            desc.TileMode = mode;
        #endif

        #ifdef VERBOSE
            DebugPrintDesc(desc);
        #endif

            ComPtr<XGTextureAddressComputer> computer;
            HRESULT hr = XGCreateTexture2DComputer(&desc, computer.GetAddressOf());
            if (FAILED(hr))
                return hr;

            hr = computer->GetResourceLayout(&layout);
            if (FAILED(hr))
                return hr;

        #ifdef VERBOSE
            DebugPrintLayout(layout);
        #endif

            if (layout.Planes != 1)
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

            hr = xbox.Initialize(desc, layout, metadata.miscFlags2);
            if (FAILED(hr))
                return hr;

            for (uint32_t level = 0; level < metadata.mipLevels; ++level)
            {
                if (metadata.arraySize > 1)
                {
                    std::vector<const Image*> images;
                    images.reserve(metadata.arraySize);
                    for (uint32_t item = 0; item < metadata.arraySize; ++item)
                    {
                        const size_t index = metadata.ComputeIndex(level, item, 0);
                        if (index >= nimages)
                        {
                            xbox.Release();
                            return E_FAIL;
                        }

                        images.push_back(&srcImages[index]);
                    }

                    hr = Tile2D(&images[0], images.size(), level, computer.Get(), xbox);
                }
                else
                {
                    const size_t index = metadata.ComputeIndex(level, 0, 0);
                    if (index >= nimages)
                    {
                        xbox.Release();
                        return E_FAIL;
                    }

                    const Image* images = &srcImages[index];
                    hr = Tile2D(&images, 1, level, computer.Get(), xbox);
                }

                if (FAILED(hr))
                {
                    xbox.Release();
                    return hr;
                }
            }
        }
        break;

    case TEX_DIMENSION_TEXTURE3D:
        {
            XG_TEXTURE3D_DESC desc = {};
            desc.Width = static_cast<UINT>(metadata.width);
            desc.Height = static_cast<UINT>(metadata.height);
            desc.Depth = static_cast<UINT>(metadata.depth);
            desc.MipLevels = static_cast<UINT>(metadata.mipLevels);
            desc.Format = static_cast<XG_FORMAT>(metadata.format);
            desc.Usage = XG_USAGE_DEFAULT;
            desc.BindFlags = XG_BIND_SHADER_RESOURCE;
        #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
            desc.SwizzleMode = mode;
        #else
            desc.TileMode = mode;
        #endif

        #ifdef VERBOSE
            DebugPrintDesc(desc);
        #endif

            ComPtr<XGTextureAddressComputer> computer;
            HRESULT hr = XGCreateTexture3DComputer(&desc, computer.GetAddressOf());
            if (FAILED(hr))
                return hr;

            hr = computer->GetResourceLayout(&layout);
            if (FAILED(hr))
                return hr;

        #ifdef VERBOSE
            DebugPrintLayout(layout);
        #endif

            if (layout.Planes != 1)
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

            hr = xbox.Initialize(desc, layout, metadata.miscFlags2);
            if (FAILED(hr))
                return hr;

            uint32_t d = static_cast<uint32_t>(metadata.depth);

            size_t index = 0;
            for (uint32_t level = 0; level < metadata.mipLevels; ++level)
            {
                if ((index + d) > nimages)
                {
                    xbox.Release();
                    return E_FAIL;
                }

                // Relies on the fact that slices are contiguous
                hr = Tile3D(srcImages[index], level, computer.Get(), xbox);
                if (FAILED(hr))
                {
                    xbox.Release();
                    return hr;
                }

                index += d;

                if (d > 1)
                    d >>= 1;
            }
        }
        break;

    default:
        return E_FAIL;
    }

    return S_OK;
}
