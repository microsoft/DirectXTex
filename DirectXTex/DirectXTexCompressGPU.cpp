//-------------------------------------------------------------------------------------
// DirectXTexCompressGPU.cpp
//
// DirectX Texture Library - DirectCompute-based texture compression
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "DirectXTexP.h"

#include "BCDirectCompute.h"

using namespace DirectX;
using namespace DirectX::Internal;

namespace
{
    constexpr TEX_FILTER_FLAGS GetSRGBFlags(_In_ TEX_COMPRESS_FLAGS compress) noexcept
    {
        static_assert(TEX_FILTER_SRGB_IN == 0x1000000, "TEX_FILTER_SRGB flag values don't match TEX_FILTER_SRGB_MASK");
        static_assert(static_cast<int>(TEX_COMPRESS_SRGB_IN) == static_cast<int>(TEX_FILTER_SRGB_IN), "TEX_COMPRESS_SRGB* should match TEX_FILTER_SRGB*");
        static_assert(static_cast<int>(TEX_COMPRESS_SRGB_OUT) == static_cast<int>(TEX_FILTER_SRGB_OUT), "TEX_COMPRESS_SRGB* should match TEX_FILTER_SRGB*");
        static_assert(static_cast<int>(TEX_COMPRESS_SRGB) == static_cast<int>(TEX_FILTER_SRGB), "TEX_COMPRESS_SRGB* should match TEX_FILTER_SRGB*");
        return static_cast<TEX_FILTER_FLAGS>(compress & TEX_FILTER_SRGB_MASK);
    }


    //-------------------------------------------------------------------------------------
    // Converts to R8G8B8A8_UNORM or R8G8B8A8_UNORM_SRGB doing any conversion logic needed
    //-------------------------------------------------------------------------------------
    HRESULT ConvertToRGBA32(
        const Image& srcImage,
        ScratchImage& image,
        bool srgb,
        TEX_FILTER_FLAGS filter) noexcept
    {
        if (!srcImage.pixels)
            return E_POINTER;

        const DXGI_FORMAT format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

        HRESULT hr = image.Initialize2D(format, srcImage.width, srcImage.height, 1, 1);
        if (FAILED(hr))
            return hr;

        const Image *img = image.GetImage(0, 0, 0);
        if (!img)
        {
            image.Release();
            return E_POINTER;
        }

        uint8_t* pDest = img->pixels;
        if (!pDest)
        {
            image.Release();
            return E_POINTER;
        }

        auto scanline = make_AlignedArrayXMVECTOR(srcImage.width);
        if (!scanline)
        {
            image.Release();
            return E_OUTOFMEMORY;
        }

        const uint8_t *pSrc = srcImage.pixels;
        for (size_t h = 0; h < srcImage.height; ++h)
        {
            if (!LoadScanline(scanline.get(), srcImage.width, pSrc, srcImage.rowPitch, srcImage.format))
            {
                image.Release();
                return E_FAIL;
            }

            ConvertScanline(scanline.get(), srcImage.width, format, srcImage.format, filter);

            if (!StoreScanline(pDest, img->rowPitch, format, scanline.get(), srcImage.width))
            {
                image.Release();
                return E_FAIL;
            }

            pSrc += srcImage.rowPitch;
            pDest += img->rowPitch;
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Converts to DXGI_FORMAT_R32G32B32A32_FLOAT doing any conversion logic needed
    //-------------------------------------------------------------------------------------
    HRESULT ConvertToRGBAF32(
        const Image& srcImage,
        ScratchImage& image,
        TEX_FILTER_FLAGS filter) noexcept
    {
        if (!srcImage.pixels)
            return E_POINTER;

        HRESULT hr = image.Initialize2D(DXGI_FORMAT_R32G32B32A32_FLOAT, srcImage.width, srcImage.height, 1, 1);
        if (FAILED(hr))
            return hr;

        const Image *img = image.GetImage(0, 0, 0);
        if (!img)
        {
            image.Release();
            return E_POINTER;
        }

        uint8_t* pDest = img->pixels;
        if (!pDest)
        {
            image.Release();
            return E_POINTER;
        }

        const uint8_t *pSrc = srcImage.pixels;
        for (size_t h = 0; h < srcImage.height; ++h)
        {
            if (!LoadScanline(reinterpret_cast<XMVECTOR*>(pDest), srcImage.width, pSrc, srcImage.rowPitch, srcImage.format))
            {
                image.Release();
                return E_FAIL;
            }

            ConvertScanline(reinterpret_cast<XMVECTOR*>(pDest), srcImage.width, DXGI_FORMAT_R32G32B32A32_FLOAT, srcImage.format, filter);

            pSrc += srcImage.rowPitch;
            pDest += img->rowPitch;
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Compress using GPU, converting to the proper input format for the shader if needed
    //-------------------------------------------------------------------------------------
    inline HRESULT GPUCompress(
        _In_ GPUCompressBC* gpubc,
        const Image& srcImage,
        const Image& destImage,
        TEX_COMPRESS_FLAGS compress)
    {
        if (!gpubc)
            return E_POINTER;

        assert(srcImage.pixels && destImage.pixels);

        DXGI_FORMAT tformat = gpubc->GetSourceFormat();
        if (compress & TEX_COMPRESS_SRGB_OUT)
        {
            tformat = MakeSRGB(tformat);
        }
        const DXGI_FORMAT sformat = (compress & TEX_COMPRESS_SRGB_IN) ? MakeSRGB(srcImage.format) : srcImage.format;

        if (sformat == tformat)
        {
            // Input is already in our required source format
            return gpubc->Compress(srcImage, destImage);
        }
        else
        {
            // Convert format and then use as the source image
            ScratchImage image;
            HRESULT hr = E_UNEXPECTED;

            auto const srgb = GetSRGBFlags(compress);

            switch (tformat)
            {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
                hr = ConvertToRGBA32(srcImage, image, false, srgb);
                break;

            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                hr = ConvertToRGBA32(srcImage, image, true, srgb);
                break;

            case DXGI_FORMAT_R32G32B32A32_FLOAT:
                hr = ConvertToRGBAF32(srcImage, image, srgb);
                break;

            default:
                break;
            }

            if (FAILED(hr))
                return hr;

            const Image *img = image.GetImage(0, 0, 0);
            if (!img)
                return E_POINTER;

            return gpubc->Compress(*img, destImage);
        }
    }
};

//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Compression
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::Compress(
    ID3D11Device* pDevice,
    const Image& srcImage,
    DXGI_FORMAT format,
    TEX_COMPRESS_FLAGS compress,
    float alphaWeight,
    ScratchImage& image) noexcept
{
    CompressOptions options = {};
    options.flags = compress;
    options.alphaWeight = alphaWeight;

    return CompressEx(pDevice, srcImage, format, options, image, nullptr);
}

_Use_decl_annotations_
HRESULT DirectX::Compress(
    ID3D11Device* pDevice,
    const Image* srcImages,
    size_t nimages,
    const TexMetadata& metadata,
    DXGI_FORMAT format,
    TEX_COMPRESS_FLAGS compress,
    float alphaWeight,
    ScratchImage& cImages) noexcept
{
    CompressOptions options = {};
    options.flags = compress;
    options.alphaWeight = alphaWeight;

    return CompressEx(pDevice, srcImages, nimages, metadata, format, options, cImages);
}

_Use_decl_annotations_
HRESULT DirectX::CompressEx(
    ID3D11Device* pDevice,
    const Image& srcImage,
    DXGI_FORMAT format,
    const CompressOptions& options,
    ScratchImage& image,
    std::function<bool __cdecl(size_t, size_t)> statusCallback)
{
    if (!pDevice || IsCompressed(srcImage.format) || !IsCompressed(format))
        return E_INVALIDARG;

    if (IsTypeless(format)
        || IsTypeless(srcImage.format) || IsPlanar(srcImage.format) || IsPalettized(srcImage.format))
        return HRESULT_E_NOT_SUPPORTED;

    // Setup GPU compressor
    std::unique_ptr<GPUCompressBC> gpubc(new (std::nothrow) GPUCompressBC);
    if (!gpubc)
        return E_OUTOFMEMORY;

    HRESULT hr = gpubc->Initialize(pDevice);
    if (FAILED(hr))
        return hr;

    hr = gpubc->Prepare(srcImage.width, srcImage.height, options.flags, format, options.alphaWeight);
    if (FAILED(hr))
        return hr;

    // Create workspace for result
    hr = image.Initialize2D(format, srcImage.width, srcImage.height, 1, 1);
    if (FAILED(hr))
        return hr;

    const Image *img = image.GetImage(0, 0, 0);
    if (!img)
    {
        image.Release();
        return E_POINTER;
    }

    if (statusCallback)
    {
        if (!statusCallback(0, 100))
        {
            image.Release();
            return E_ABORT;
        }
    }

    hr = GPUCompress(gpubc.get(), srcImage, *img, options.flags);

    if (FAILED(hr))
    {
        image.Release();
        return hr;
    }

    if (statusCallback)
    {
        if (!statusCallback(100, 100))
        {
            image.Release();
            return E_ABORT;
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT DirectX::CompressEx(
    ID3D11Device* pDevice,
    const Image* srcImages,
    size_t nimages,
    const TexMetadata& metadata,
    DXGI_FORMAT format,
    const CompressOptions& options,
    ScratchImage& cImages,
    std::function<bool __cdecl(size_t, size_t)> statusCallback)
{
    if (!pDevice || !srcImages || !nimages)
        return E_INVALIDARG;

    if (IsCompressed(metadata.format) || !IsCompressed(format))
        return E_INVALIDARG;

    if (IsTypeless(format)
        || IsTypeless(metadata.format) || IsPlanar(metadata.format) || IsPalettized(metadata.format))
        return HRESULT_E_NOT_SUPPORTED;

    cImages.Release();

    // Setup GPU compressor
    std::unique_ptr<GPUCompressBC> gpubc(new (std::nothrow) GPUCompressBC);
    if (!gpubc)
        return E_OUTOFMEMORY;

    HRESULT hr = gpubc->Initialize(pDevice);
    if (FAILED(hr))
        return hr;

    // Create workspace for result
    TexMetadata mdata2 = metadata;
    mdata2.format = format;
    hr = cImages.Initialize(mdata2);
    if (FAILED(hr))
        return hr;

    if (nimages != cImages.GetImageCount())
    {
        cImages.Release();
        return E_FAIL;
    }

    const Image* dest = cImages.GetImages();
    if (!dest)
    {
        cImages.Release();
        return E_POINTER;
    }

    if (statusCallback)
    {
        if (!statusCallback(0, nimages))
        {
            cImages.Release();
            return E_ABORT;
        }
    }

    // Process images (ordered by size)
    switch (metadata.dimension)
    {
    case TEX_DIMENSION_TEXTURE1D:
    case TEX_DIMENSION_TEXTURE2D:
        {
            size_t w = metadata.width;
            size_t h = metadata.height;
            size_t progress = 0;

            for (size_t level = 0; level < metadata.mipLevels; ++level)
            {
                hr = gpubc->Prepare(w, h, options.flags, format, options.alphaWeight);
                if (FAILED(hr))
                {
                    cImages.Release();
                    return hr;
                }

                for (size_t item = 0; item < metadata.arraySize; ++item)
                {
                    const size_t index = metadata.ComputeIndex(level, item, 0);
                    if (index >= nimages)
                    {
                        cImages.Release();
                        return E_FAIL;
                    }

                    assert(dest[index].format == format);

                    const Image& src = srcImages[index];

                    if (src.width != dest[index].width || src.height != dest[index].height)
                    {
                        cImages.Release();
                        return E_FAIL;
                    }

                    hr = GPUCompress(gpubc.get(), src, dest[index], options.flags);
                    if (FAILED(hr))
                    {
                        cImages.Release();
                        return hr;
                    }

                    if (statusCallback)
                    {
                        if (!statusCallback(progress++, nimages))
                        {
                            cImages.Release();
                            return E_ABORT;
                        }
                    }
                }

                if (h > 1)
                    h >>= 1;

                if (w > 1)
                    w >>= 1;
            }
        }
        break;

    case TEX_DIMENSION_TEXTURE3D:
        {
            size_t w = metadata.width;
            size_t h = metadata.height;
            size_t d = metadata.depth;
            size_t progress = 0;

            for (size_t level = 0; level < metadata.mipLevels; ++level)
            {
                hr = gpubc->Prepare(w, h, options.flags, format, options.alphaWeight);
                if (FAILED(hr))
                {
                    cImages.Release();
                    return hr;
                }

                for (size_t slice = 0; slice < d; ++slice)
                {
                    const size_t index = metadata.ComputeIndex(level, 0, slice);
                    if (index >= nimages)
                    {
                        cImages.Release();
                        return E_FAIL;
                    }

                    assert(dest[index].format == format);

                    const Image& src = srcImages[index];

                    if (src.width != dest[index].width || src.height != dest[index].height)
                    {
                        cImages.Release();
                        return E_FAIL;
                    }

                    hr = GPUCompress(gpubc.get(), src, dest[index], options.flags);
                    if (FAILED(hr))
                    {
                        cImages.Release();
                        return hr;
                    }

                    if (statusCallback)
                    {
                        if (!statusCallback(progress++, nimages))
                        {
                            cImages.Release();
                            return E_ABORT;
                        }
                    }
                }

                if (h > 1)
                    h >>= 1;

                if (w > 1)
                    w >>= 1;

                if (d > 1)
                    d >>= 1;
            }
        }
        break;

    default:
        return HRESULT_E_NOT_SUPPORTED;
    }

    if (statusCallback)
    {
        if (!statusCallback(nimages, nimages))
        {
            cImages.Release();
            return E_ABORT;
        }
    }

    return S_OK;
}
