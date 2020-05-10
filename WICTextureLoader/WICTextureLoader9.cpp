//--------------------------------------------------------------------------------------
// File: WICTextureLoader9.cpp
//
// Function for loading a WIC image and creating a Direct3D runtime texture for it
//
// Note: Assumes application has already called CoInitializeEx
//
// Note these functions are useful for images created as simple 2D textures. For
// more complex resources, DDSTextureLoader is an excellent light-weight runtime loader.
// For a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

// We could load multi-frame images (TIFF/GIF) into a texture array.
// For now, we just load the first frame (note: DirectXTex supports multi-frame images)

#include "WICTextureLoader9.h"

#include <d3d9types.h>

#include <assert.h>
#include <algorithm>
#include <memory>

#include <wincodec.h>

#include <wrl\client.h>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    //-------------------------------------------------------------------------------------
    // WIC Pixel Format Translation Data
    //-------------------------------------------------------------------------------------
    struct WICTranslate
    {
        GUID                wic;
        D3DFORMAT           format;
    };

    const WICTranslate g_WICFormats[] =
    {
        { GUID_WICPixelFormat128bppRGBAFloat,       D3DFMT_A32B32G32R32F },

        { GUID_WICPixelFormat64bppRGBAHalf,         D3DFMT_A16B16G16R16F },
        { GUID_WICPixelFormat64bppRGBA,             D3DFMT_A16B16G16R16 },

        { GUID_WICPixelFormat32bppBGRA,             D3DFMT_A8R8G8B8 },

        { GUID_WICPixelFormat32bppRGBA1010102,      D3DFMT_A2B10G10R10 },

        { GUID_WICPixelFormat16bppBGRA5551,         D3DFMT_A1R5G5B5 },
        { GUID_WICPixelFormat16bppBGR555,           D3DFMT_X1R5G5B5 },
        { GUID_WICPixelFormat16bppBGR565,           D3DFMT_R5G6B5 },

        { GUID_WICPixelFormat32bppGrayFloat,        D3DFMT_R32F },
        { GUID_WICPixelFormat16bppGrayHalf,         D3DFMT_R16F },
        { GUID_WICPixelFormat16bppGray,             D3DFMT_L16 },
        { GUID_WICPixelFormat8bppGray,              D3DFMT_L8 },

        { GUID_WICPixelFormat8bppAlpha,             D3DFMT_A8 },
    };

    //-------------------------------------------------------------------------------------
    // WIC Pixel Format nearest conversion table
    //-------------------------------------------------------------------------------------

    struct WICConvert
    {
        GUID        source;
        GUID        target;
    };

    const WICConvert g_WICConvert[] =
    {
        // Note target GUID in this conversion table must be one of those directly supported formats (above).

        { GUID_WICPixelFormatBlackWhite,            GUID_WICPixelFormat8bppGray }, // D3DFMT_L8

        { GUID_WICPixelFormat1bppIndexed,           GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8
        { GUID_WICPixelFormat2bppIndexed,           GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8
        { GUID_WICPixelFormat4bppIndexed,           GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8
        { GUID_WICPixelFormat8bppIndexed,           GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8

        { GUID_WICPixelFormat2bppGray,              GUID_WICPixelFormat8bppGray }, // D3DFMT_L8 
        { GUID_WICPixelFormat4bppGray,              GUID_WICPixelFormat8bppGray }, // D3DFMT_L8 

        { GUID_WICPixelFormat16bppGrayFixedPoint,   GUID_WICPixelFormat16bppGrayHalf }, // D3DFMT_R16F 
        { GUID_WICPixelFormat32bppGrayFixedPoint,   GUID_WICPixelFormat32bppGrayFloat }, // D3DFMT_R32F 

        { GUID_WICPixelFormat32bppBGR101010,        GUID_WICPixelFormat32bppRGBA1010102 }, // D3DFMT_A2B10G10R10

        { GUID_WICPixelFormat24bppBGR,              GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8
        { GUID_WICPixelFormat24bppRGB,              GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8
        { GUID_WICPixelFormat32bppBGR,              GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8
        { GUID_WICPixelFormat32bppRGBA,             GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8
        { GUID_WICPixelFormat32bppPBGRA,            GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8
        { GUID_WICPixelFormat32bppPRGBA,            GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8

        { GUID_WICPixelFormat48bppRGB,              GUID_WICPixelFormat64bppRGBA }, // D3DFMT_A16B16G16R16
        { GUID_WICPixelFormat48bppBGR,              GUID_WICPixelFormat64bppRGBA }, // D3DFMT_A16B16G16R16
        { GUID_WICPixelFormat64bppBGRA,             GUID_WICPixelFormat64bppRGBA }, // D3DFMT_A16B16G16R16
        { GUID_WICPixelFormat64bppPRGBA,            GUID_WICPixelFormat64bppRGBA }, // D3DFMT_A16B16G16R16
        { GUID_WICPixelFormat64bppPBGRA,            GUID_WICPixelFormat64bppRGBA }, // D3DFMT_A16B16G16R16

        { GUID_WICPixelFormat48bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // D3DFMT_A16B16G16R16F 
        { GUID_WICPixelFormat48bppBGRFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // D3DFMT_A16B16G16R16F 
        { GUID_WICPixelFormat64bppRGBAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // D3DFMT_A16B16G16R16F 
        { GUID_WICPixelFormat64bppBGRAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // D3DFMT_A16B16G16R16F 
        { GUID_WICPixelFormat64bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // D3DFMT_A16B16G16R16F 
        { GUID_WICPixelFormat64bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // D3DFMT_A16B16G16R16F 
        { GUID_WICPixelFormat48bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // D3DFMT_A16B16G16R16F 

        { GUID_WICPixelFormat128bppPRGBAFloat,      GUID_WICPixelFormat128bppRGBAFloat }, // D3DFMT_A32B32G32R32F 
        { GUID_WICPixelFormat128bppRGBFloat,        GUID_WICPixelFormat128bppRGBAFloat }, // D3DFMT_A32B32G32R32F 
        { GUID_WICPixelFormat128bppRGBAFixedPoint,  GUID_WICPixelFormat128bppRGBAFloat }, // D3DFMT_A32B32G32R32F 
        { GUID_WICPixelFormat128bppRGBFixedPoint,   GUID_WICPixelFormat128bppRGBAFloat }, // D3DFMT_A32B32G32R32F 
        { GUID_WICPixelFormat32bppRGBE,             GUID_WICPixelFormat128bppRGBAFloat }, // D3DFMT_A32B32G32R32F

        { GUID_WICPixelFormat32bppCMYK,             GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8
        { GUID_WICPixelFormat64bppCMYK,             GUID_WICPixelFormat64bppRGBA }, // D3DFMT_A16B16G16R16
        { GUID_WICPixelFormat40bppCMYKAlpha,        GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8
        { GUID_WICPixelFormat80bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // D3DFMT_A16B16G16R16

    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        { GUID_WICPixelFormat32bppRGB,              GUID_WICPixelFormat32bppBGRA }, // D3DFMT_A8R8G8B8
        { GUID_WICPixelFormat64bppRGB,              GUID_WICPixelFormat64bppRGBA }, // D3DFMT_A16B16G16R16
        { GUID_WICPixelFormat64bppPRGBAHalf,        GUID_WICPixelFormat64bppRGBAHalf }, // D3DFMT_A16B16G16R16F 
        { GUID_WICPixelFormat96bppRGBFloat,         GUID_WICPixelFormat128bppRGBAFloat }, // D3DFMT_A32B32G32R32F 
    #endif

        // We don't support n-channel formats
    };

    bool g_WIC2 = false;

    //--------------------------------------------------------------------------------------
    BOOL WINAPI InitializeWICFactory(PINIT_ONCE, PVOID, PVOID* ifactory) noexcept
    {
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory2,
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(IWICImagingFactory2),
            ifactory
        );

        if (SUCCEEDED(hr))
        {
            // WIC2 is available on Windows 10, Windows 8.x, and Windows 7 SP1 with KB 2670838 installed
            g_WIC2 = true;
            return TRUE;
        }
        else
        {
            hr = CoCreateInstance(
                CLSID_WICImagingFactory1,
                nullptr,
                CLSCTX_INPROC_SERVER,
                __uuidof(IWICImagingFactory),
                ifactory
            );
            return SUCCEEDED(hr) ? TRUE : FALSE;
        }
#else
        return SUCCEEDED(CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(IWICImagingFactory),
            ifactory)) ? TRUE : FALSE;
#endif
    }

    IWICImagingFactory* _GetWIC() noexcept
    {
        static INIT_ONCE s_initOnce = INIT_ONCE_STATIC_INIT;

        IWICImagingFactory* factory = nullptr;
        if (!InitOnceExecuteOnce(
            &s_initOnce,
            InitializeWICFactory,
            nullptr,
            reinterpret_cast<LPVOID*>(&factory)))
        {
            return nullptr;
        }

        return factory;
    }

    //---------------------------------------------------------------------------------
    D3DFORMAT _WICToD3D9(const GUID& guid) noexcept
    {
        for (size_t i = 0; i < _countof(g_WICFormats); ++i)
        {
            if (memcmp(&g_WICFormats[i].wic, &guid, sizeof(GUID)) == 0)
                return g_WICFormats[i].format;
        }

        return D3DFMT_UNKNOWN;
    }

    //---------------------------------------------------------------------------------
    void FitPowerOf2(UINT origx, UINT origy, UINT& targetx, UINT& targety, size_t maxsize)
    {
        float origAR = float(origx) / float(origy);

        if (origx > origy)
        {
            size_t x;
            for (x = maxsize; x > 1; x >>= 1) { if (x <= targetx) break; }
            targetx = UINT(x);

            float bestScore = FLT_MAX;
            for (size_t y = maxsize; y > 0; y >>= 1)
            {
                float score = fabsf((float(x) / float(y)) - origAR);
                if (score < bestScore)
                {
                    bestScore = score;
                    targety = UINT(y);
                }
            }
        }
        else
        {
            size_t y;
            for (y = maxsize; y > 1; y >>= 1) { if (y <= targety) break; }
            targety = UINT(y);

            float bestScore = FLT_MAX;
            for (size_t x = maxsize; x > 0; x >>= 1)
            {
                float score = fabsf((float(x) / float(y)) - origAR);
                if (score < bestScore)
                {
                    bestScore = score;
                    targetx = UINT(x);
                }
            }
        }
    }

    //---------------------------------------------------------------------------------
    HRESULT CreateTextureFromWIC(
        _In_ LPDIRECT3DDEVICE9 device,
        _In_ IWICBitmapFrameDecode* frame,
        _In_ size_t maxsize,
        _In_ unsigned int loadFlags,
        _Outptr_ LPDIRECT3DTEXTURE9* texture) noexcept
    {
        UINT width, height;
        HRESULT hr = frame->GetSize(&width, &height);
        if (FAILED(hr))
            return hr;

        if (maxsize > UINT32_MAX)
            return E_INVALIDARG;

        assert(width > 0 && height > 0);

        if (!maxsize)
        {
            maxsize = 4096u /*D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
        }

        assert(maxsize > 0);

        UINT twidth = width;
        UINT theight = height;
        if (loadFlags & WIC_LOADER_FIT_POW2)
        {
            FitPowerOf2(width, height, twidth, theight, maxsize);
        }
        else if (width > maxsize || height > maxsize)
        {
            float ar = static_cast<float>(height) / static_cast<float>(width);
            if (width > height)
            {
                twidth = static_cast<UINT>(maxsize);
                theight = std::max<UINT>(1, static_cast<UINT>(static_cast<float>(maxsize) * ar));
            }
            else
            {
                theight = static_cast<UINT>(maxsize);
                twidth = std::max<UINT>(1, static_cast<UINT>(static_cast<float>(maxsize) / ar));
            }
            assert(twidth <= maxsize && theight <= maxsize);
        }

        if (loadFlags & WIC_LOADER_MAKE_SQUARE)
        {
            twidth = std::max<UINT>(twidth, theight);
            theight = twidth;
        }

        // Determine format
        WICPixelFormatGUID pixelFormat;
        hr = frame->GetPixelFormat(&pixelFormat);
        if (FAILED(hr))
            return hr;

        WICPixelFormatGUID convertGUID;
        memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &pixelFormat, sizeof(GUID));

        D3DFORMAT format = _WICToD3D9(pixelFormat);
        if (format == D3DFMT_UNKNOWN)
        {
            for (size_t i = 0; i < _countof(g_WICConvert); ++i)
            {
                if (memcmp(&g_WICConvert[i].source, &pixelFormat, sizeof(WICPixelFormatGUID)) == 0)
                {
                    memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &g_WICConvert[i].target, sizeof(GUID));

                    format = _WICToD3D9(g_WICConvert[i].target);
                    assert(format != D3DFMT_UNKNOWN);
                    break;
                }
            }

            if (format == D3DFMT_UNKNOWN)
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        if (loadFlags & WIC_LOADER_FORCE_RGBA32)
        {
            memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat32bppBGRA, sizeof(GUID));
            format = D3DFMT_A8R8G8B8;
        }

        // Create texture
        ComPtr<IDirect3DTexture9> pTexture;
        hr = device->CreateTexture(twidth, theight, 1u,
            (loadFlags & WIC_LOADER_MIP_AUTOGEN) ? D3DUSAGE_AUTOGENMIPMAP : 0u,
            format, D3DPOOL_DEFAULT,
            pTexture.GetAddressOf(), nullptr);
        if (FAILED(hr))
            return hr;

        // Create staging texture memory
        ComPtr<IDirect3DTexture9> pStagingTexture;
        hr = device->CreateTexture(twidth, theight, 1u,
            0u, format, D3DPOOL_SYSTEMMEM,
            pStagingTexture.GetAddressOf(), nullptr);
        if (FAILED(hr))
            return hr;

        D3DLOCKED_RECT LockedRect = {};
        hr = pStagingTexture->LockRect(0, &LockedRect, nullptr, 0);
        if (FAILED(hr))
            return hr;

        uint64_t numBytes = uint64_t(LockedRect.Pitch) * uint64_t(theight);

        pStagingTexture->UnlockRect(0);

        if (numBytes > UINT32_MAX)
        {
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
        }

        // Load image data
        if (memcmp(&convertGUID, &pixelFormat, sizeof(GUID)) == 0
            && twidth == width
            && theight == height)
        {
            // No format conversion or resize needed
            hr = pStagingTexture->LockRect(0, &LockedRect, nullptr, 0);
            if (FAILED(hr))
                return hr;

            hr = frame->CopyPixels(nullptr, static_cast<UINT>(LockedRect.Pitch), static_cast<UINT>(numBytes),
                static_cast<BYTE*>(LockedRect.pBits));

            pStagingTexture->UnlockRect(0);

            if (FAILED(hr))
                return hr;
        }
        else if (twidth != width || theight != height)
        {
            // Resize
            auto pWIC = _GetWIC();
            if (!pWIC)
                return E_NOINTERFACE;

            ComPtr<IWICBitmapScaler> scaler;
            hr = pWIC->CreateBitmapScaler(scaler.GetAddressOf());
            if (FAILED(hr))
                return hr;

            hr = scaler->Initialize(frame, twidth, theight, WICBitmapInterpolationModeFant);
            if (FAILED(hr))
                return hr;

            WICPixelFormatGUID pfScaler;
            hr = scaler->GetPixelFormat(&pfScaler);
            if (FAILED(hr))
                return hr;

            if (memcmp(&convertGUID, &pfScaler, sizeof(GUID)) == 0)
            {
                // No format conversion needed
                hr = pStagingTexture->LockRect(0, &LockedRect, nullptr, 0);
                if (FAILED(hr))
                    return hr;

                hr = scaler->CopyPixels(nullptr, static_cast<UINT>(LockedRect.Pitch), static_cast<UINT>(numBytes),
                    static_cast<BYTE*>(LockedRect.pBits));

                pStagingTexture->UnlockRect(0);

                if (FAILED(hr))
                    return hr;
            }
            else
            {
                ComPtr<IWICFormatConverter> FC;
                hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
                if (FAILED(hr))
                    return hr;

                BOOL canConvert = FALSE;
                hr = FC->CanConvert(pfScaler, convertGUID, &canConvert);
                if (FAILED(hr) || !canConvert)
                {
                    return E_UNEXPECTED;
                }

                hr = FC->Initialize(scaler.Get(), convertGUID, WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeMedianCut);
                if (FAILED(hr))
                    return hr;

                hr = pStagingTexture->LockRect(0, &LockedRect, nullptr, 0);
                if (FAILED(hr))
                    return hr;

                hr = FC->CopyPixels(nullptr, static_cast<UINT>(LockedRect.Pitch), static_cast<UINT>(numBytes),
                    static_cast<BYTE*>(LockedRect.pBits));

                pStagingTexture->UnlockRect(0);

                if (FAILED(hr))
                    return hr;
            }
        }
        else
        {
            // Format conversion but no resize
            auto pWIC = _GetWIC();
            if (!pWIC)
                return E_NOINTERFACE;

            ComPtr<IWICFormatConverter> FC;
            hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
            if (FAILED(hr))
                return hr;

            BOOL canConvert = FALSE;
            hr = FC->CanConvert(pixelFormat, convertGUID, &canConvert);
            if (FAILED(hr) || !canConvert)
            {
                return E_UNEXPECTED;
            }

            hr = FC->Initialize(frame, convertGUID, WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr))
                return hr;

            hr = pStagingTexture->LockRect(0, &LockedRect, nullptr, 0);
            if (FAILED(hr))
                return hr;

            hr = FC->CopyPixels(nullptr, static_cast<UINT>(LockedRect.Pitch), static_cast<UINT>(numBytes),
                static_cast<BYTE*>(LockedRect.pBits));

            pStagingTexture->UnlockRect(0);

            if (FAILED(hr))
                return hr;
        }

        hr = device->UpdateTexture(pStagingTexture.Get(), pTexture.Get());
        if (FAILED(hr))
            return hr;

        *texture = pTexture.Detach();

        return S_OK;
    }
} // anonymous namespace

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateWICTextureFromMemory(
    LPDIRECT3DDEVICE9 d3dDevice,
    const uint8_t* wicData,
    size_t wicDataSize,
    LPDIRECT3DTEXTURE9* texture,
    size_t maxsize,
    unsigned int loadFlags) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !wicData || !wicDataSize || !texture)
        return E_INVALIDARG;

    if (!wicDataSize)
        return E_FAIL;

    if (wicDataSize > UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);

    auto pWIC = _GetWIC();
    if (!pWIC)
        return E_NOINTERFACE;

    // Create input stream for memory
    ComPtr<IWICStream> stream;
    HRESULT hr = pWIC->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromMemory(const_cast<uint8_t*>(wicData), static_cast<DWORD>(wicDataSize));
    if (FAILED(hr))
        return hr;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    hr = pWIC->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    return CreateTextureFromWIC(d3dDevice, frame.Get(), maxsize, loadFlags, texture);
}

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateWICTextureFromFile(
    LPDIRECT3DDEVICE9 d3dDevice,
    const wchar_t* fileName,
    LPDIRECT3DTEXTURE9* texture,
    size_t maxsize,
    unsigned int loadFlags) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !fileName || !texture)
        return E_INVALIDARG;

    auto pWIC = _GetWIC();
    if (!pWIC)
        return E_NOINTERFACE;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = pWIC->CreateDecoderFromFilename(fileName,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    return CreateTextureFromWIC(d3dDevice, frame.Get(), maxsize, loadFlags, texture);
}
