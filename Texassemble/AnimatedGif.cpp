//--------------------------------------------------------------------------------------
// File: AnimatedGif.cpp
//
// Code for converting an animated GIF to a series of texture frames.
//
// References:
//   https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/Win7Samples/multimedia/wic/wicanimatedgif
//   http://www.imagemagick.org/Usage/anim_basics/#dispose
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//--------------------------------------------------------------------------------------

#ifdef  _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005)
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NODRAWTEXT
#define NOGDI
#define NOMCX
#define NOSERVICE
#define NOHELP
#ifdef  _MSC_VER
#pragma warning(pop)
#endif

#include <cstddef>
#include <iterator>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include <wrl/client.h>

#include <wincodec.h>

#ifdef  _MSC_VER
#pragma warning(disable : 4619 4616 26812)
#endif

#include "DirectXTex.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    enum
    {
        DM_UNDEFINED = 0,
        DM_NONE = 1,
        DM_BACKGROUND = 2,
        DM_PREVIOUS = 3
    };


    void FillRectangle(const Image& img, const RECT& destRect, uint32_t color)
    {
        RECT clipped =
        {
            (destRect.left < 0) ? 0 : destRect.left,
            (destRect.top < 0) ? 0 : destRect.top,
            (destRect.right > static_cast<long>(img.width)) ? static_cast<long>(img.width) : destRect.right,
            (destRect.bottom > static_cast<long>(img.height)) ? static_cast<long>(img.height) : destRect.bottom
        };

        auto ptr = reinterpret_cast<uint8_t*>(img.pixels + size_t(clipped.top) * img.rowPitch + size_t(clipped.left) * sizeof(uint32_t));

        for (long y = clipped.top; y < clipped.bottom; ++y)
        {
            auto pixelPtr = reinterpret_cast<uint32_t*>(ptr);
            for (long x = clipped.left; x < clipped.right; ++x)
            {
                *pixelPtr++ = color;
            }

            ptr += img.rowPitch;
        }
    }


    void BlendRectangle(const Image& composed, const Image& raw, const RECT& destRect, uint32_t transparent)
    {
        RECT clipped =
        {
            (destRect.left < 0) ? 0 : destRect.left,
            (destRect.top < 0) ? 0 : destRect.top,
            (destRect.right > static_cast<long>(composed.width)) ? static_cast<long>(composed.width) : destRect.right,
            (destRect.bottom > static_cast<long>(composed.height)) ? static_cast<long>(composed.height) : destRect.bottom
        };

        auto rawPtr = reinterpret_cast<uint8_t*>(raw.pixels);
        auto composedPtr = reinterpret_cast<uint8_t*>(
            composed.pixels
            + size_t(clipped.top) * composed.rowPitch
            + size_t(clipped.left) * sizeof(uint32_t));

        for (long y = clipped.top; y < clipped.bottom; ++y)
        {
            auto srcPtr = reinterpret_cast<uint32_t*>(rawPtr);
            auto destPtr = reinterpret_cast<uint32_t*>(composedPtr);
            for (long x = clipped.left; x < clipped.right; ++x, ++srcPtr, ++destPtr)
            {
                if (transparent == *srcPtr)
                    continue;

                *destPtr = *srcPtr;
            }

            rawPtr += raw.rowPitch;
            composedPtr += composed.rowPitch;
        }
    }
}

HRESULT LoadAnimatedGif(const wchar_t* szFile, std::vector<std::unique_ptr<ScratchImage>>& loadedImages, bool usebgcolor)
{
    bool iswic2;
    auto pWIC = GetWICFactory(iswic2);
    if (!pWIC)
        return E_NOINTERFACE;

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = pWIC->CreateDecoderFromFilename(szFile, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    {
        GUID containerFormat;
        hr = decoder->GetContainerFormat(&containerFormat);
        if (FAILED(hr))
            return hr;

        if (memcmp(&containerFormat, &GUID_ContainerFormatGif, sizeof(GUID)) != 0)
        {
            // This function only works for GIF
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }
    }

    ComPtr<IWICMetadataQueryReader> metareader;
    hr = decoder->GetMetadataQueryReader(metareader.GetAddressOf());
    if (FAILED(hr))
        return hr;

    PROPVARIANT propValue;
    PropVariantInit(&propValue);

    // Get palette
    WICColor rgbColors[256] = {};
    UINT actualColors = 0;
    {
        ComPtr<IWICPalette> palette;
        hr = pWIC->CreatePalette(palette.GetAddressOf());
        if (FAILED(hr))
            return hr;

        hr = decoder->CopyPalette(palette.Get());
        if (FAILED(hr))
            return hr;

        hr = palette->GetColors(static_cast<UINT>(std::size(rgbColors)), rgbColors, &actualColors);
        if (FAILED(hr))
            return hr;
    }

    // Get background color
    UINT bgColor = 0;
    if (usebgcolor)
    {
        // Most browsers just ignore the background color metadata and always use transparency
        hr = metareader->GetMetadataByName(L"/logscrdesc/GlobalColorTableFlag", &propValue);
        if (SUCCEEDED(hr))
        {
            const bool hasTable = (propValue.vt == VT_BOOL && propValue.boolVal);
            PropVariantClear(&propValue);

            if (hasTable)
            {
                hr = metareader->GetMetadataByName(L"/logscrdesc/BackgroundColorIndex", &propValue);
                if (SUCCEEDED(hr))
                {
                    if (propValue.vt == VT_UI1)
                    {
                        const uint8_t index = propValue.bVal;

                        if (index < actualColors)
                        {
                            bgColor = rgbColors[index];
                        }
                    }
                    PropVariantClear(&propValue);
                }
            }
        }
    }

    // Get global frame size
    UINT width = 0;
    UINT height = 0;

    hr = metareader->GetMetadataByName(L"/logscrdesc/Width", &propValue);
    if (FAILED(hr))
        return hr;

    if (propValue.vt != VT_UI2)
        return E_FAIL;

    width = propValue.uiVal;
    PropVariantClear(&propValue);

    hr = metareader->GetMetadataByName(L"/logscrdesc/Height", &propValue);
    if (FAILED(hr))
        return hr;

    if (propValue.vt != VT_UI2)
        return E_FAIL;

    height = propValue.uiVal;
    PropVariantClear(&propValue);

    UINT fcount;
    hr = decoder->GetFrameCount(&fcount);
    if (FAILED(hr))
        return hr;

    UINT disposal = DM_UNDEFINED;
    RECT rct = {};

    UINT previousFrame = 0;
    for (UINT iframe = 0; iframe < fcount; ++iframe)
    {
        int transparentIndex = -1;

        std::unique_ptr<ScratchImage> frameImage(new (std::nothrow) ScratchImage);
        if (!frameImage)
            return E_OUTOFMEMORY;

        if (disposal == DM_PREVIOUS)
        {
            hr = frameImage->InitializeFromImage(*loadedImages[previousFrame]->GetImage(0, 0, 0));
        }
        else if (iframe > 0)
        {
            hr = frameImage->InitializeFromImage(*loadedImages[iframe - 1]->GetImage(0, 0, 0));
        }
        else
        {
            hr = frameImage->Initialize2D(DXGI_FORMAT_B8G8R8A8_UNORM, width, height, 1, 1);
        }
        if (FAILED(hr))
            return hr;

        auto composedImage = frameImage->GetImage(0, 0, 0);

        if (!iframe)
        {
            RECT fullRct = { 0, 0, static_cast<long>(width), static_cast<long>(height) };
            FillRectangle(*composedImage, fullRct, bgColor);
        }
        else if (disposal == DM_BACKGROUND)
        {
            FillRectangle(*composedImage, rct, bgColor);
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(iframe, frame.GetAddressOf());
        if (FAILED(hr))
            return hr;

        WICPixelFormatGUID pixelFormat;
        hr = frame->GetPixelFormat(&pixelFormat);
        if (FAILED(hr))
            return hr;

        if (memcmp(&pixelFormat, &GUID_WICPixelFormat8bppIndexed, sizeof(GUID)) != 0)
        {
            // GIF is always loaded as this format
            return E_UNEXPECTED;
        }

        ComPtr<IWICMetadataQueryReader> frameMeta;
        hr = frame->GetMetadataQueryReader(frameMeta.GetAddressOf());
        if (SUCCEEDED(hr))
        {
            hr = frameMeta->GetMetadataByName(L"/imgdesc/Left", &propValue);
            if (SUCCEEDED(hr))
            {
                hr = (propValue.vt == VT_UI2 ? S_OK : E_FAIL);
                if (SUCCEEDED(hr))
                {
                    rct.left = static_cast<long>(propValue.uiVal);
                }
                PropVariantClear(&propValue);
            }

            hr = frameMeta->GetMetadataByName(L"/imgdesc/Top", &propValue);
            if (SUCCEEDED(hr))
            {
                hr = (propValue.vt == VT_UI2 ? S_OK : E_FAIL);
                if (SUCCEEDED(hr))
                {
                    rct.top = static_cast<long>(propValue.uiVal);
                }
                PropVariantClear(&propValue);
            }

            hr = frameMeta->GetMetadataByName(L"/imgdesc/Width", &propValue);
            if (SUCCEEDED(hr))
            {
                hr = (propValue.vt == VT_UI2 ? S_OK : E_FAIL);
                if (SUCCEEDED(hr))
                {
                    rct.right = static_cast<long>(propValue.uiVal) + rct.left;
                }
                PropVariantClear(&propValue);
            }

            hr = frameMeta->GetMetadataByName(L"/imgdesc/Height", &propValue);
            if (SUCCEEDED(hr))
            {
                hr = (propValue.vt == VT_UI2 ? S_OK : E_FAIL);
                if (SUCCEEDED(hr))
                {
                    rct.bottom = static_cast<long>(propValue.uiVal) + rct.top;
                }
                PropVariantClear(&propValue);
            }

            disposal = DM_UNDEFINED;
            hr = frameMeta->GetMetadataByName(L"/grctlext/Disposal", &propValue);
            if (SUCCEEDED(hr))
            {
                hr = (propValue.vt == VT_UI1 ? S_OK : E_FAIL);
                if (SUCCEEDED(hr))
                {
                    disposal = propValue.bVal;
                }
                PropVariantClear(&propValue);
            }

            hr = frameMeta->GetMetadataByName(L"/grctlext/TransparencyFlag", &propValue);
            if (SUCCEEDED(hr))
            {
                hr = (propValue.vt == VT_BOOL ? S_OK : E_FAIL);
                if (SUCCEEDED(hr) && propValue.boolVal)
                {
                    PropVariantClear(&propValue);
                    hr = frameMeta->GetMetadataByName(L"/grctlext/TransparentColorIndex", &propValue);
                    if (SUCCEEDED(hr))
                    {
                        hr = (propValue.vt == VT_UI1 ? S_OK : E_FAIL);
                        if (SUCCEEDED(hr) && propValue.uiVal < actualColors)
                        {
                            transparentIndex = static_cast<int>(propValue.uiVal);
                        }
                    }
                }
                PropVariantClear(&propValue);
            }

        }

        UINT w, h;
        hr = frame->GetSize(&w, &h);
        if (FAILED(hr))
            return hr;

        ScratchImage rawFrame;
        hr = rawFrame.Initialize2D(DXGI_FORMAT_B8G8R8A8_UNORM, w, h, 1, 1);
        if (FAILED(hr))
            return hr;

        ComPtr<IWICFormatConverter> FC;
        hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
        if (FAILED(hr))
            return hr;

        hr = FC->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut);
        if (FAILED(hr))
            return hr;

        auto img = rawFrame.GetImage(0, 0, 0);

        hr = FC->CopyPixels(nullptr, static_cast<UINT>(img->rowPitch), static_cast<UINT>(img->slicePitch), img->pixels);
        if (FAILED(hr))
            return hr;

        if (!iframe || transparentIndex == -1)
        {
            const Rect fullRect(0, 0, img->width, img->height);
            hr = CopyRectangle(*img, fullRect, *composedImage, TEX_FILTER_DEFAULT, size_t(rct.left), size_t(rct.top));
            if (FAILED(hr))
                return hr;
        }
        else
        {
            BlendRectangle(*composedImage, *img, rct, rgbColors[transparentIndex]);
        }

        if (disposal == DM_UNDEFINED || disposal == DM_NONE)
        {
            previousFrame = iframe;
        }

        loadedImages.emplace_back(std::move(frameImage));
    }

    PropVariantClear(&propValue);

    return S_OK;
}
