//--------------------------------------------------------------------------------------
// File: WICTextureLoader9.cpp
//
// Function for loading a WIC image and creating a Direct3D runtime texture for it
// (auto-generating mipmaps if possible)
//
// Note: Assumes application has already called CoInitializeEx
//
// Warning: CreateWICTexture* functions are not thread-safe if given a d3dContext instance for
//          auto-gen mipmap support.
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

        { GUID_WICPixelFormat24bppBGR,              D3DFMT_R8G8B8 },

        { GUID_WICPixelFormat32bppRGBA,             D3DFMT_A8B8G8R8 },
        { GUID_WICPixelFormat32bppBGRA,             D3DFMT_A8R8G8B8 },
        { GUID_WICPixelFormat32bppBGR,              D3DFMT_X8R8G8B8 },

        { GUID_WICPixelFormat32bppRGBA1010102,      D3DFMT_A2B10G10R10 },

        { GUID_WICPixelFormat16bppBGRA5551,         D3DFMT_A1R5G5B5 },
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

        { GUID_WICPixelFormat1bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // D3DFMT_A8B8G8R8 
        { GUID_WICPixelFormat2bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // D3DFMT_A8B8G8R8 
        { GUID_WICPixelFormat4bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // D3DFMT_A8B8G8R8 
        { GUID_WICPixelFormat8bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // D3DFMT_A8B8G8R8 

        { GUID_WICPixelFormat2bppGray,              GUID_WICPixelFormat8bppGray }, // D3DFMT_L8 
        { GUID_WICPixelFormat4bppGray,              GUID_WICPixelFormat8bppGray }, // D3DFMT_L8 

        { GUID_WICPixelFormat16bppGrayFixedPoint,   GUID_WICPixelFormat16bppGrayHalf }, // D3DFMT_R16F 
        { GUID_WICPixelFormat32bppGrayFixedPoint,   GUID_WICPixelFormat32bppGrayFloat }, // D3DFMT_R32F 

        { GUID_WICPixelFormat16bppBGR555,           GUID_WICPixelFormat16bppBGRA5551 }, // D3DFMT_A1R5G5B5

        { GUID_WICPixelFormat32bppBGR101010,        GUID_WICPixelFormat32bppRGBA1010102 }, // D3DFMT_A2B10G10R10

        { GUID_WICPixelFormat24bppRGB,              GUID_WICPixelFormat24bppBGR }, // D3DFMT_R8G8B8

        { GUID_WICPixelFormat32bppPBGRA,            GUID_WICPixelFormat32bppRGBA }, // D3DFMT_A8B8G8R8 
        { GUID_WICPixelFormat32bppPRGBA,            GUID_WICPixelFormat32bppRGBA }, // D3DFMT_A8B8G8R8 

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

        { GUID_WICPixelFormat32bppCMYK,             GUID_WICPixelFormat32bppRGBA }, // D3DFMT_A8B8G8R8
        { GUID_WICPixelFormat64bppCMYK,             GUID_WICPixelFormat64bppRGBA }, // D3DFMT_A16B16G16R16
        { GUID_WICPixelFormat40bppCMYKAlpha,        GUID_WICPixelFormat32bppRGBA }, // D3DFMT_A8B8G8R8
        { GUID_WICPixelFormat80bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // D3DFMT_A16B16G16R16

    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        { GUID_WICPixelFormat32bppRGB,              GUID_WICPixelFormat32bppRGBA }, // D3DFMT_A8B8G8R8
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
    size_t _WICBitsPerPixel(REFGUID targetGuid) noexcept
    {
        auto pWIC = _GetWIC();
        if (!pWIC)
            return 0;

        ComPtr<IWICComponentInfo> cinfo;
        if (FAILED(pWIC->CreateComponentInfo(targetGuid, cinfo.GetAddressOf())))
            return 0;

        WICComponentType type;
        if (FAILED(cinfo->GetComponentType(&type)))
            return 0;

        if (type != WICPixelFormat)
            return 0;

        ComPtr<IWICPixelFormatInfo> pfinfo;
        if (FAILED(cinfo.As(&pfinfo)))
            return 0;

        UINT bpp;
        if (FAILED(pfinfo->GetBitsPerPixel(&bpp)))
            return 0;

        return bpp;
    }

#if 0
    //---------------------------------------------------------------------------------
    HRESULT CreateTextureFromWIC(_In_ ID3D11Device* d3dDevice,
        _In_opt_ ID3D11DeviceContext* d3dContext,
        _In_ IWICBitmapFrameDecode *frame,
        _In_ size_t maxsize,
        _In_ D3D11_USAGE usage,
        _In_ unsigned int bindFlags,
        _In_ unsigned int cpuAccessFlags,
        _In_ unsigned int miscFlags,
        _In_ unsigned int loadFlags,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView) noexcept
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
            // This is a bit conservative because the hardware could support larger textures than
            // the Feature Level defined minimums, but doing it this way is much easier and more
            // performant for WIC than the 'fail and retry' model used by DDSTextureLoader

            switch (d3dDevice->GetFeatureLevel())
            {
            case D3D_FEATURE_LEVEL_9_1:
            case D3D_FEATURE_LEVEL_9_2:
                maxsize = 2048u /*D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                break;

            case D3D_FEATURE_LEVEL_9_3:
                maxsize = 4096u /*D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                break;

            case D3D_FEATURE_LEVEL_10_0:
            case D3D_FEATURE_LEVEL_10_1:
                maxsize = 8192u /*D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                break;

            default:
                maxsize = size_t(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION);
                break;
            }
        }

        assert(maxsize > 0);

        UINT twidth, theight;
        if (width > maxsize || height > maxsize)
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
        else
        {
            twidth = width;
            theight = height;
        }

        // Determine format
        WICPixelFormatGUID pixelFormat;
        hr = frame->GetPixelFormat(&pixelFormat);
        if (FAILED(hr))
            return hr;

        WICPixelFormatGUID convertGUID;
        memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &pixelFormat, sizeof(GUID));

        size_t bpp = 0;

        DXGI_FORMAT format = _WICToDXGI(pixelFormat);
        if (format == D3DFMT_UNKNOWN)
        {
            for (size_t i = 0; i < _countof(g_WICConvert); ++i)
            {
                if (memcmp(&g_WICConvert[i].source, &pixelFormat, sizeof(WICPixelFormatGUID)) == 0)
                {
                    memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &g_WICConvert[i].target, sizeof(GUID));

                    format = _WICToDXGI(g_WICConvert[i].target);
                    assert(format != D3DFMT_UNKNOWN);
                    bpp = _WICBitsPerPixel(convertGUID);
                    break;
                }
            }

            if (format == D3DFMT_UNKNOWN)
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }
        else
        {
            bpp = _WICBitsPerPixel(pixelFormat);
        }

        if (loadFlags & WIC_LOADER_FORCE_RGBA32)
        {
            memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat32bppRGBA, sizeof(GUID));
            format = D3DFMT_A8B8G8R8;
            bpp = 32;
        }

        if (!bpp)
            return E_FAIL;

        // Handle sRGB formats
        if (loadFlags & WIC_LOADER_FORCE_SRGB)
        {
            format = MakeSRGB(format);
        }
        else if (!(loadFlags & WIC_LOADER_IGNORE_SRGB))
        {
            ComPtr<IWICMetadataQueryReader> metareader;
            if (SUCCEEDED(frame->GetMetadataQueryReader(metareader.GetAddressOf())))
            {
                GUID containerFormat;
                if (SUCCEEDED(metareader->GetContainerFormat(&containerFormat)))
                {
                    // Check for sRGB colorspace metadata
                    bool sRGB = false;

                    PROPVARIANT value;
                    PropVariantInit(&value);

                    if (memcmp(&containerFormat, &GUID_ContainerFormatPng, sizeof(GUID)) == 0)
                    {
                        // Check for sRGB chunk
                        if (SUCCEEDED(metareader->GetMetadataByName(L"/sRGB/RenderingIntent", &value)) && value.vt == VT_UI1)
                        {
                            sRGB = true;
                        }
                    }
                    else if (SUCCEEDED(metareader->GetMetadataByName(L"System.Image.ColorSpace", &value)) && value.vt == VT_UI2 && value.uiVal == 1)
                    {
                        sRGB = true;
                    }

                    (void)PropVariantClear(&value);

                    if (sRGB)
                        format = MakeSRGB(format);
                }
            }
        }

        // Verify our target format is supported by the current device
        // (handles WDDM 1.0 or WDDM 1.1 device driver cases as well as DirectX 11.0 Runtime without 16bpp format support)
        UINT support = 0;
        hr = d3dDevice->CheckFormatSupport(format, &support);
        if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_TEXTURE2D))
        {
            // Fallback to RGBA 32-bit format which is supported by all devices
            memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat32bppRGBA, sizeof(GUID));
            format = D3DFMT_A8B8G8R8;
            bpp = 32;
        }

        // Allocate temporary memory for image
        uint64_t rowBytes = (uint64_t(twidth) * uint64_t(bpp) + 7u) / 8u;
        uint64_t numBytes = rowBytes * uint64_t(height);

        if (rowBytes > UINT32_MAX || numBytes > UINT32_MAX)
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

        auto rowPitch = static_cast<size_t>(rowBytes);
        auto imageSize = static_cast<size_t>(numBytes);

        std::unique_ptr<uint8_t[]> temp(new (std::nothrow) uint8_t[imageSize]);
        if (!temp)
            return E_OUTOFMEMORY;

        // Load image data
        if (memcmp(&convertGUID, &pixelFormat, sizeof(GUID)) == 0
            && twidth == width
            && theight == height)
        {
            // No format conversion or resize needed
            hr = frame->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
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
                hr = scaler->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
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

                hr = FC->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
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

            hr = FC->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
            if (FAILED(hr))
                return hr;
        }

        // D3DUSAGE_AUTOGENMIPMAP

        // Create texture
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = twidth;
        desc.Height = theight;
        desc.MipLevels = (autogen) ? 0u : 1u;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = usage;
        desc.CPUAccessFlags = cpuAccessFlags;

        if (autogen)
        {
            desc.BindFlags = bindFlags | D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags = miscFlags | D3D11_RESOURCE_MISC_GENERATE_MIPS;
        }
        else
        {
            desc.BindFlags = bindFlags;
            desc.MiscFlags = miscFlags;
        }

        D3D11_SUBRESOURCE_DATA initData;
        initData.pSysMem = temp.get();
        initData.SysMemPitch = static_cast<UINT>(rowPitch);
        initData.SysMemSlicePitch = static_cast<UINT>(imageSize);

        ID3D11Texture2D* tex = nullptr;
        hr = d3dDevice->CreateTexture2D(&desc, (autogen) ? nullptr : &initData, &tex);
        if (SUCCEEDED(hr) && tex)
        {
            if (textureView)
            {
                D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                SRVDesc.Format = desc.Format;

                SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                SRVDesc.Texture2D.MipLevels = (autogen) ? unsigned(-1) : 1u;

                hr = d3dDevice->CreateShaderResourceView(tex, &SRVDesc, textureView);
                if (FAILED(hr))
                {
                    tex->Release();
                    return hr;
                }

                if (autogen)
                {
                    assert(d3dContext != nullptr);
                    d3dContext->UpdateSubresource(tex, 0, nullptr, temp.get(), static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize));
                    d3dContext->GenerateMips(*textureView);
                }
            }

            if (texture)
            {
                *texture = tex;
            }
            else
            {
                SetDebugObjectName(tex, "WICTextureLoader");
                tex->Release();
            }
        }

        return hr;
    }
#endif
} // anonymous namespace

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateWICTextureFromMemory(
    LPDIRECT3DDEVICE9 d3dDevice,
    const uint8_t* wicData,
    size_t wicDataSize,
    LPDIRECT3DTEXTURE9* texture,
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

    // TODO -
    hr = E_NOTIMPL;

    return hr;
}

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateWICTextureFromFile(
    LPDIRECT3DDEVICE9 d3dDevice,
    const wchar_t* szFileName,
    LPDIRECT3DTEXTURE9* texture,
    unsigned int loadFlags) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !szFileName || !texture)
        return E_INVALIDARG;

    auto pWIC = _GetWIC();
    if (!pWIC)
        return E_NOINTERFACE;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = pWIC->CreateDecoderFromFilename(szFileName,
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

    // TODO -
    hr = E_NOTIMPL;

    return hr;
}
