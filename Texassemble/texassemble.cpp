//--------------------------------------------------------------------------------------
// File: Texassemble.cpp
//
// DirectX Texture assembler for cube maps, volume maps, and arrays
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//--------------------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable : 4005)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP
#pragma warning(pop)

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <fstream>
#include <memory>
#include <list>
#include <vector>

#include <wrl/client.h>

#include <dxgiformat.h>

#include <DirectXPackedVector.h>
#include <wincodec.h>

#include "directxtex.h"

//Uncomment to add support for OpenEXR (.exr)
//#define USE_OPENEXR

#ifdef USE_OPENEXR
// See <https://github.com/Microsoft/DirectXTex/wiki/Adding-OpenEXR> for details
#include "DirectXTexEXR.h"
#endif

using namespace DirectX;
using Microsoft::WRL::ComPtr;

enum COMMANDS
{
    CMD_CUBE = 1,
    CMD_VOLUME,
    CMD_ARRAY,
    CMD_CUBEARRAY,
    CMD_H_CROSS,
    CMD_V_CROSS,
    CMD_H_STRIP,
    CMD_V_STRIP,
    CMD_MERGE,
    CMD_GIF,
    CMD_MAX
};

enum OPTIONS
{
    OPT_RECURSIVE = 1,
    OPT_WIDTH,
    OPT_HEIGHT,
    OPT_FORMAT,
    OPT_FILTER,
    OPT_SRGBI,
    OPT_SRGBO,
    OPT_SRGB,
    OPT_OUTPUTFILE,
    OPT_OVERWRITE,
    OPT_USE_DX10,
    OPT_NOLOGO,
    OPT_SEPALPHA,
    OPT_NO_WIC,
    OPT_DEMUL_ALPHA,
    OPT_TA_WRAP,
    OPT_TA_MIRROR,
    OPT_TONEMAP,
    OPT_FILELIST,
    OPT_GIF_BGCOLOR,
    OPT_MAX
};

static_assert(OPT_MAX <= 32, "dwOptions is a DWORD bitfield");

struct SConversion
{
    wchar_t szSrc[MAX_PATH];
};

struct SValue
{
    LPCWSTR pName;
    DWORD dwValue;
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

const SValue g_pCommands[] =
{
    { L"cube",      CMD_CUBE },
    { L"volume",    CMD_VOLUME },
    { L"array",     CMD_ARRAY },
    { L"cubearray", CMD_CUBEARRAY },
    { L"h-cross",   CMD_H_CROSS },
    { L"v-cross",   CMD_V_CROSS },
    { L"h-strip",   CMD_H_STRIP },
    { L"v-strip",   CMD_V_STRIP },
    { L"merge",     CMD_MERGE },
    { L"gif",       CMD_GIF },
    { nullptr,      0 }
};

const SValue g_pOptions [] =
{
    { L"r",         OPT_RECURSIVE },
    { L"w",         OPT_WIDTH },
    { L"h",         OPT_HEIGHT },
    { L"f",         OPT_FORMAT },
    { L"if",        OPT_FILTER },
    { L"srgbi",     OPT_SRGBI },
    { L"srgbo",     OPT_SRGBO },
    { L"srgb",      OPT_SRGB },
    { L"o",         OPT_OUTPUTFILE },
    { L"y",         OPT_OVERWRITE },
    { L"dx10",      OPT_USE_DX10 },
    { L"nologo",    OPT_NOLOGO },
    { L"sepalpha",  OPT_SEPALPHA },
    { L"nowic",     OPT_NO_WIC },
    { L"alpha",     OPT_DEMUL_ALPHA },
    { L"wrap",      OPT_TA_WRAP },
    { L"mirror",    OPT_TA_MIRROR },
    { L"tonemap",   OPT_TONEMAP },
    { L"flist",     OPT_FILELIST },
    { L"bgcolor",   OPT_GIF_BGCOLOR },
    { nullptr,      0 }
};

#define DEFFMT(fmt) { L#fmt, DXGI_FORMAT_ ## fmt }

const SValue g_pFormats [] =
{
    // List does not include _TYPELESS or depth/stencil formats
    DEFFMT(R32G32B32A32_FLOAT),
    DEFFMT(R32G32B32A32_UINT),
    DEFFMT(R32G32B32A32_SINT),
    DEFFMT(R32G32B32_FLOAT),
    DEFFMT(R32G32B32_UINT),
    DEFFMT(R32G32B32_SINT),
    DEFFMT(R16G16B16A16_FLOAT),
    DEFFMT(R16G16B16A16_UNORM),
    DEFFMT(R16G16B16A16_UINT),
    DEFFMT(R16G16B16A16_SNORM),
    DEFFMT(R16G16B16A16_SINT),
    DEFFMT(R32G32_FLOAT),
    DEFFMT(R32G32_UINT),
    DEFFMT(R32G32_SINT),
    DEFFMT(R10G10B10A2_UNORM),
    DEFFMT(R10G10B10A2_UINT),
    DEFFMT(R11G11B10_FLOAT),
    DEFFMT(R8G8B8A8_UNORM),
    DEFFMT(R8G8B8A8_UNORM_SRGB),
    DEFFMT(R8G8B8A8_UINT),
    DEFFMT(R8G8B8A8_SNORM),
    DEFFMT(R8G8B8A8_SINT),
    DEFFMT(R16G16_FLOAT),
    DEFFMT(R16G16_UNORM),
    DEFFMT(R16G16_UINT),
    DEFFMT(R16G16_SNORM),
    DEFFMT(R16G16_SINT),
    DEFFMT(R32_FLOAT),
    DEFFMT(R32_UINT),
    DEFFMT(R32_SINT),
    DEFFMT(R8G8_UNORM),
    DEFFMT(R8G8_UINT),
    DEFFMT(R8G8_SNORM),
    DEFFMT(R8G8_SINT),
    DEFFMT(R16_FLOAT),
    DEFFMT(R16_UNORM),
    DEFFMT(R16_UINT),
    DEFFMT(R16_SNORM),
    DEFFMT(R16_SINT),
    DEFFMT(R8_UNORM),
    DEFFMT(R8_UINT),
    DEFFMT(R8_SNORM),
    DEFFMT(R8_SINT),
    DEFFMT(A8_UNORM),
    //DEFFMT(R1_UNORM)
    DEFFMT(R9G9B9E5_SHAREDEXP),
    DEFFMT(R8G8_B8G8_UNORM),
    DEFFMT(G8R8_G8B8_UNORM),
    DEFFMT(B5G6R5_UNORM),
    DEFFMT(B5G5R5A1_UNORM),

    // DXGI 1.1 formats
    DEFFMT(B8G8R8A8_UNORM),
    DEFFMT(B8G8R8X8_UNORM),
    DEFFMT(R10G10B10_XR_BIAS_A2_UNORM),
    DEFFMT(B8G8R8A8_UNORM_SRGB),
    DEFFMT(B8G8R8X8_UNORM_SRGB),

    // DXGI 1.2 formats
    DEFFMT(AYUV),
    DEFFMT(Y410),
    DEFFMT(Y416),
    DEFFMT(YUY2),
    DEFFMT(Y210),
    DEFFMT(Y216),
    // No support for legacy paletted video formats (AI44, IA44, P8, A8P8)
    DEFFMT(B4G4R4A4_UNORM),

    { nullptr, DXGI_FORMAT_UNKNOWN }
};

const SValue g_pFormatAliases [] =
{
    { L"RGBA", DXGI_FORMAT_R8G8B8A8_UNORM },
    { L"BGRA", DXGI_FORMAT_B8G8R8A8_UNORM },

    { L"FP16", DXGI_FORMAT_R16G16B16A16_FLOAT },
    { L"FP32", DXGI_FORMAT_R32G32B32A32_FLOAT },

    { nullptr, DXGI_FORMAT_UNKNOWN }
};

const SValue g_pFilters [] =
{
    { L"POINT",                     TEX_FILTER_POINT },
    { L"LINEAR",                    TEX_FILTER_LINEAR },
    { L"CUBIC",                     TEX_FILTER_CUBIC },
    { L"FANT",                      TEX_FILTER_FANT },
    { L"BOX",                       TEX_FILTER_BOX },
    { L"TRIANGLE",                  TEX_FILTER_TRIANGLE },
    { L"POINT_DITHER",              TEX_FILTER_POINT | TEX_FILTER_DITHER },
    { L"LINEAR_DITHER",             TEX_FILTER_LINEAR | TEX_FILTER_DITHER },
    { L"CUBIC_DITHER",              TEX_FILTER_CUBIC | TEX_FILTER_DITHER },
    { L"FANT_DITHER",               TEX_FILTER_FANT | TEX_FILTER_DITHER },
    { L"BOX_DITHER",                TEX_FILTER_BOX | TEX_FILTER_DITHER },
    { L"TRIANGLE_DITHER",           TEX_FILTER_TRIANGLE | TEX_FILTER_DITHER },
    { L"POINT_DITHER_DIFFUSION",    TEX_FILTER_POINT | TEX_FILTER_DITHER_DIFFUSION },
    { L"LINEAR_DITHER_DIFFUSION",   TEX_FILTER_LINEAR | TEX_FILTER_DITHER_DIFFUSION },
    { L"CUBIC_DITHER_DIFFUSION",    TEX_FILTER_CUBIC | TEX_FILTER_DITHER_DIFFUSION },
    { L"FANT_DITHER_DIFFUSION",     TEX_FILTER_FANT | TEX_FILTER_DITHER_DIFFUSION },
    { L"BOX_DITHER_DIFFUSION",      TEX_FILTER_BOX | TEX_FILTER_DITHER_DIFFUSION },
    { L"TRIANGLE_DITHER_DIFFUSION", TEX_FILTER_TRIANGLE | TEX_FILTER_DITHER_DIFFUSION },
    { nullptr,                      TEX_FILTER_DEFAULT                              }
};

#define CODEC_DDS 0xFFFF0001 
#define CODEC_TGA 0xFFFF0002
#define CODEC_HDR 0xFFFF0005
#define CODEC_EXR 0xFFFF0006

const SValue g_pExtFileTypes [] =
{
    { L".BMP",  WIC_CODEC_BMP },
    { L".JPG",  WIC_CODEC_JPEG },
    { L".JPEG", WIC_CODEC_JPEG },
    { L".PNG",  WIC_CODEC_PNG },
    { L".DDS",  CODEC_DDS },
    { L".TGA",  CODEC_TGA },
    { L".HDR",  CODEC_HDR },
    { L".TIF",  WIC_CODEC_TIFF },
    { L".TIFF", WIC_CODEC_TIFF },
    { L".WDP",  WIC_CODEC_WMP },
    { L".HDP",  WIC_CODEC_WMP },
    { L".JXR",  WIC_CODEC_WMP },
    #ifdef USE_OPENEXR
    { L"EXR",   CODEC_EXR },
    #endif
    { nullptr,  CODEC_DDS }
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

namespace
{
    inline HANDLE safe_handle(HANDLE h) { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

    struct find_closer { void operator()(HANDLE h) { assert(h != INVALID_HANDLE_VALUE); if (h) FindClose(h); } };

    typedef std::unique_ptr<void, find_closer> ScopedFindHandle;

#pragma prefast(disable : 26018, "Only used with static internal arrays")

    DWORD LookupByName(const wchar_t *pName, const SValue *pArray)
    {
        while (pArray->pName)
        {
            if (!_wcsicmp(pName, pArray->pName))
                return pArray->dwValue;

            pArray++;
        }

        return 0;
    }


    const wchar_t* LookupByValue(DWORD pValue, const SValue *pArray)
    {
        while (pArray->pName)
        {
            if (pValue == pArray->dwValue)
                return pArray->pName;

            pArray++;
        }

        return L"";
    }


    void SearchForFiles(const wchar_t* path, std::list<SConversion>& files, bool recursive)
    {
        // Process files
        WIN32_FIND_DATA findData = {};
        ScopedFindHandle hFile(safe_handle(FindFirstFileExW(path,
            FindExInfoBasic, &findData,
            FindExSearchNameMatch, nullptr,
            FIND_FIRST_EX_LARGE_FETCH)));
        if (hFile)
        {
            for (;;)
            {
                if (!(findData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY)))
                {
                    wchar_t drive[_MAX_DRIVE] = {};
                    wchar_t dir[_MAX_DIR] = {};
                    _wsplitpath_s(path, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);

                    SConversion conv;
                    _wmakepath_s(conv.szSrc, drive, dir, findData.cFileName, nullptr);
                    files.push_back(conv);
                }

                if (!FindNextFile(hFile.get(), &findData))
                    break;
            }
        }

        // Process directories
        if (recursive)
        {
            wchar_t searchDir[MAX_PATH] = {};
            {
                wchar_t drive[_MAX_DRIVE] = {};
                wchar_t dir[_MAX_DIR] = {};
                _wsplitpath_s(path, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);
                _wmakepath_s(searchDir, drive, dir, L"*", nullptr);
            }

            hFile.reset(safe_handle(FindFirstFileExW(searchDir,
                FindExInfoBasic, &findData,
                FindExSearchLimitToDirectories, nullptr,
                FIND_FIRST_EX_LARGE_FETCH)));
            if (!hFile)
                return;

            for (;;)
            {
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (findData.cFileName[0] != L'.')
                    {
                        wchar_t subdir[MAX_PATH] = {};

                        {
                            wchar_t drive[_MAX_DRIVE] = {};
                            wchar_t dir[_MAX_DIR] = {};
                            wchar_t fname[_MAX_FNAME] = {};
                            wchar_t ext[_MAX_FNAME] = {};
                            _wsplitpath_s(path, drive, dir, fname, ext);
                            wcscat_s(dir, findData.cFileName);
                            _wmakepath_s(subdir, drive, dir, fname, ext);
                        }

                        SearchForFiles(subdir, files, recursive);
                    }
                }

                if (!FindNextFile(hFile.get(), &findData))
                    break;
            }
        }
    }


    void PrintFormat(DXGI_FORMAT Format)
    {
        for (const SValue *pFormat = g_pFormats; pFormat->pName; pFormat++)
        {
            if ((DXGI_FORMAT)pFormat->dwValue == Format)
            {
                wprintf(pFormat->pName);
                break;
            }
        }
    }


    void PrintInfo(const TexMetadata& info)
    {
        wprintf(L" (%zux%zu", info.width, info.height);

        if (TEX_DIMENSION_TEXTURE3D == info.dimension)
            wprintf(L"x%zu", info.depth);

        if (info.mipLevels > 1)
            wprintf(L",%zu", info.mipLevels);

        if (info.arraySize > 1)
            wprintf(L",%zu", info.arraySize);

        wprintf(L" ");
        PrintFormat(info.format);

        switch (info.dimension)
        {
        case TEX_DIMENSION_TEXTURE1D:
            wprintf((info.arraySize > 1) ? L" 1DArray" : L" 1D");
            break;

        case TEX_DIMENSION_TEXTURE2D:
            if (info.IsCubemap())
            {
                wprintf((info.arraySize > 6) ? L" CubeArray" : L" Cube");
            }
            else
            {
                wprintf((info.arraySize > 1) ? L" 2DArray" : L" 2D");
            }
            break;

        case TEX_DIMENSION_TEXTURE3D:
            wprintf(L" 3D");
            break;
        }

        switch (info.GetAlphaMode())
        {
        case TEX_ALPHA_MODE_OPAQUE:
            wprintf(L" \x0e0:Opaque");
            break;
        case TEX_ALPHA_MODE_PREMULTIPLIED:
            wprintf(L" \x0e0:PM");
            break;
        case TEX_ALPHA_MODE_STRAIGHT:
            wprintf(L" \x0e0:NonPM");
            break;
        }

        wprintf(L")");
    }


    void PrintList(size_t cch, const SValue *pValue)
    {
        while (pValue->pName)
        {
            size_t cchName = wcslen(pValue->pName);

            if (cch + cchName + 2 >= 80)
            {
                wprintf(L"\n      ");
                cch = 6;
            }

            wprintf(L"%ls ", pValue->pName);
            cch += cchName + 2;
            pValue++;
        }

        wprintf(L"\n");
    }


    void PrintLogo()
    {
        wprintf(L"Microsoft (R) DirectX Texture Assembler (DirectXTex version)\n");
        wprintf(L"Copyright (C) Microsoft Corp. All rights reserved.\n");
#ifdef _DEBUG
        wprintf(L"*** Debug build ***\n");
#endif
        wprintf(L"\n");
    }


    void PrintUsage()
    {
        PrintLogo();

        wprintf(L"Usage: texassemble <command> <options> <files>\n\n");
        wprintf(L"   cube                create cubemap\n");
        wprintf(L"   volume              create volume map\n");
        wprintf(L"   array               create texture array\n");
        wprintf(L"   cubearray           create cubemap array\n");
        wprintf(L"   h-cross or v-cross  create a cross image from a cubemap\n");
        wprintf(L"   h-strip or v-strip  create a strip image from a cubemap\n");
        wprintf(L"   merge               create texture from rgb image and alpha image\n");
        wprintf(L"   gif                 create array from animated gif\n\n");
        wprintf(L"   -r                  wildcard filename search is recursive\n");
        wprintf(L"   -w <n>              width\n");
        wprintf(L"   -h <n>              height\n");
        wprintf(L"   -f <format>         format\n");
        wprintf(L"   -if <filter>        image filtering\n");
        wprintf(L"   -srgb{i|o}          sRGB {input, output}\n");
        wprintf(L"   -o <filename>       output filename\n");
        wprintf(L"   -y                  overwrite existing output file (if any)\n");
        wprintf(L"   -sepalpha           resize alpha channel separately from color channels\n");
        wprintf(L"   -nowic              Force non-WIC filtering\n");
        wprintf(L"   -wrap, -mirror      texture addressing mode (wrap, mirror, or clamp)\n");
        wprintf(L"   -alpha              convert premultiplied alpha to straight alpha\n");
        wprintf(L"   -dx10               Force use of 'DX10' extended header\n");
        wprintf(L"   -nologo             suppress copyright message\n");
        wprintf(L"   -tonemap            Apply a tonemap operator based on maximum luminance\n");
        wprintf(L"   -flist <filename>   use text file with a list of input files (one per line)\n");
        wprintf(L"\n                       (gif only)\n");
        wprintf(L"   -bgcolor            Use background color instead of transparency\n");

        wprintf(L"\n   <format>: ");
        PrintList(13, g_pFormats);
        wprintf(L"      ");
        PrintList(13, g_pFormatAliases);

        wprintf(L"\n   <filter>: ");
        PrintList(13, g_pFilters);
    }

    HRESULT SaveImageFile(const Image& img, DWORD fileType, const wchar_t* szOutputFile)
    {
        switch (fileType)
        {
        case CODEC_DDS:
            return SaveToDDSFile(img, DDS_FLAGS_NONE, szOutputFile);

        case CODEC_TGA:
            return SaveToTGAFile(img, szOutputFile);

        case CODEC_HDR:
            return SaveToHDRFile(img, szOutputFile);

        #ifdef USE_OPENEXR
        case CODEC_EXR:
            return SaveToEXRFile(img, szOutputFile);
        #endif

        default:
            return SaveToWICFile(img, WIC_FLAGS_NONE, GetWICCodec(static_cast<WICCodecs>(fileType)), szOutputFile);
        }
    }

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

        auto ptr = reinterpret_cast<uint8_t*>(img.pixels + clipped.top * img.rowPitch + clipped.left * sizeof(uint32_t));

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

    void BlendRectangle(const Image& composed, const Image& raw, const RECT& destRect)
    {
        using namespace DirectX::PackedVector;

        RECT clipped =
        {
            (destRect.left < 0) ? 0 : destRect.left,
            (destRect.top < 0) ? 0 : destRect.top,
            (destRect.right > static_cast<long>(composed.width)) ? static_cast<long>(composed.width) : destRect.right,
            (destRect.bottom > static_cast<long>(composed.height)) ? static_cast<long>(composed.height) : destRect.bottom
        };

        auto rawPtr = reinterpret_cast<uint8_t*>(raw.pixels);
        auto composedPtr = reinterpret_cast<uint8_t*>(composed.pixels + clipped.top * composed.rowPitch + clipped.left * sizeof(uint32_t));

        for (long y = clipped.top; y < clipped.bottom; ++y)
        {
            auto srcPtr = reinterpret_cast<uint32_t*>(rawPtr);
            auto destPtr = reinterpret_cast<uint32_t*>(composedPtr);
            for (long x = clipped.left; x < clipped.right; ++x)
            {
                XMVECTOR a = XMLoadUByteN4(reinterpret_cast<const XMUBYTEN4*>(srcPtr++));
                XMVECTOR b = XMLoadUByteN4(reinterpret_cast<const XMUBYTEN4*>(destPtr));

                XMVECTOR alpha = XMVectorSplatW(a);

                XMVECTOR blended = XMVectorMultiply(a, alpha) + XMVectorMultiply(b, XMVectorSubtract(g_XMOne, alpha));

                blended = XMVectorSelect(g_XMIdentityR3, blended, g_XMSelect1110);

                XMStoreUByteN4(reinterpret_cast<XMUBYTEN4*>(destPtr++), blended);
            }

            rawPtr += raw.rowPitch;
            composedPtr += composed.rowPitch;
        }
    }

    HRESULT LoadAnimatedGif(const wchar_t* szFile, std::vector<std::unique_ptr<ScratchImage>>& loadedImages, bool usebgcolor)
    {
        // https://code.msdn.microsoft.com/windowsapps/Windows-Imaging-Component-65abbc6a/
        // http://www.imagemagick.org/Usage/anim_basics/#dispose

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

        // Get background color
        UINT bgColor = 0;
        if (usebgcolor)
        {
            // Most browsers just ignore the background color metadata and always use transparency
            hr = metareader->GetMetadataByName(L"/logscrdesc/GlobalColorTableFlag", &propValue);
            if (SUCCEEDED(hr))
            {
                bool hasTable = (propValue.vt == VT_BOOL && propValue.boolVal);
                PropVariantClear(&propValue);

                if (hasTable)
                {
                    hr = metareader->GetMetadataByName(L"/logscrdesc/BackgroundColorIndex", &propValue);
                    if (SUCCEEDED(hr))
                    {
                        if (propValue.vt == VT_UI1)
                        {
                            uint8_t index = propValue.bVal;

                            ComPtr<IWICPalette> palette;
                            hr = pWIC->CreatePalette(palette.GetAddressOf());
                            if (FAILED(hr))
                                return hr;

                            hr = decoder->CopyPalette(palette.Get());
                            if (FAILED(hr))
                                return hr;

                            WICColor rgColors[256];
                            UINT actualColors = 0;
                            hr = palette->GetColors(_countof(rgColors), rgColors, &actualColors);
                            if (FAILED(hr))
                                return hr;

                            if (index < actualColors)
                            {
                                bgColor = rgColors[index];
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
            std::unique_ptr<ScratchImage> frameImage(new (std::nothrow) ScratchImage);
            if (!frameImage)
                return E_OUTOFMEMORY;

            if (disposal == DM_PREVIOUS)
            {
                hr = frameImage->InitializeFromImage(*loadedImages[previousFrame]->GetImage(0, 0, 0));
            }
            else if (iframe > 0)
            {
                hr = frameImage->InitializeFromImage(*loadedImages[iframe-1]->GetImage(0, 0, 0));
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

            if (!iframe)
            {
                Rect fullRect(0, 0, img->width, img->height);

                hr = CopyRectangle(*img, fullRect, *composedImage, TEX_FILTER_DEFAULT, rct.left, rct.top);
                if (FAILED(hr))
                    return hr;
            }
            else
            {
                BlendRectangle(*composedImage, *img, rct);
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
}


//--------------------------------------------------------------------------------------
// Entry-point
//--------------------------------------------------------------------------------------
#pragma prefast(disable : 28198, "Command-line tool, frees all memory on exit")

int __cdecl wmain(_In_ int argc, _In_z_count_(argc) wchar_t* argv[])
{
    // Parameters and defaults
    size_t width = 0;
    size_t height = 0;

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    DWORD dwFilter = TEX_FILTER_DEFAULT;
    DWORD dwSRGB = 0;
    DWORD dwFilterOpts = 0;
    DWORD fileType = WIC_CODEC_BMP;

    wchar_t szOutputFile[MAX_PATH] = {};

    // Initialize COM (needed for WIC)
    HRESULT hr = hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        wprintf(L"Failed to initialize COM (%08X)\n", hr);
        return 1;
    }

    // Process command line
    if (argc < 2)
    {
        PrintUsage();
        return 0;
    }

    DWORD dwCommand = LookupByName(argv[1], g_pCommands);
    switch (dwCommand)
    {
    case CMD_CUBE:
    case CMD_VOLUME:
    case CMD_ARRAY:
    case CMD_CUBEARRAY:
    case CMD_H_CROSS:
    case CMD_V_CROSS:
    case CMD_H_STRIP:
    case CMD_V_STRIP:
    case CMD_MERGE:
    case CMD_GIF:
        break;

    default:
        wprintf(L"Must use one of: cube, volume, array, cubearray,\n   h-cross, v-cross, h-strip, v-strip\n   merge, gif\n\n");
        return 1;
    }

    DWORD dwOptions = 0;
    std::list<SConversion> conversion;

    for (int iArg = 2; iArg < argc; iArg++)
    {
        PWSTR pArg = argv[iArg];

        if (('-' == pArg[0]) || ('/' == pArg[0]))
        {
            pArg++;
            PWSTR pValue;

            for (pValue = pArg; *pValue && (':' != *pValue); pValue++);

            if (*pValue)
                *pValue++ = 0;

            DWORD dwOption = LookupByName(pArg, g_pOptions);

            if (!dwOption || (dwOptions & (1 << dwOption)))
            {
                PrintUsage();
                return 1;
            }

            dwOptions |= 1 << dwOption;

            // Handle options with additional value parameter
            switch (dwOption)
            {
            case OPT_WIDTH:
            case OPT_HEIGHT:
            case OPT_FORMAT:
            case OPT_FILTER:
            case OPT_OUTPUTFILE:
            case OPT_FILELIST:
                if (!*pValue)
                {
                    if ((iArg + 1 >= argc))
                    {
                        PrintUsage();
                        return 1;
                    }

                    iArg++;
                    pValue = argv[iArg];
                }
            }

            switch (dwOption)
            {
            case OPT_WIDTH:
                if (swscanf_s(pValue, L"%zu", &width) != 1)
                {
                    wprintf(L"Invalid value specified with -w (%ls)\n", pValue);
                    return 1;
                }
                break;

            case OPT_HEIGHT:
                if (swscanf_s(pValue, L"%zu", &height) != 1)
                {
                    wprintf(L"Invalid value specified with -h (%ls)\n", pValue);
                    return 1;
                }
                break;

            case OPT_FORMAT:
                format = static_cast<DXGI_FORMAT>(LookupByName(pValue, g_pFormats));
                if (!format)
                {
                    format = static_cast<DXGI_FORMAT>(LookupByName(pValue, g_pFormatAliases));
                    if (!format)
                    {
                        wprintf(L"Invalid value specified with -f (%ls)\n", pValue);
                        return 1;
                    }
                }
                break;

            case OPT_FILTER:
                dwFilter = LookupByName(pValue, g_pFilters);
                if (!dwFilter)
                {
                    wprintf(L"Invalid value specified with -if (%ls)\n", pValue);
                    return 1;
                }
                break;

            case OPT_SRGBI:
                dwSRGB |= TEX_FILTER_SRGB_IN;
                break;

            case OPT_SRGBO:
                dwSRGB |= TEX_FILTER_SRGB_OUT;
                break;

            case OPT_SRGB:
                dwSRGB |= TEX_FILTER_SRGB;
                break;

            case OPT_SEPALPHA:
                dwFilterOpts |= TEX_FILTER_SEPARATE_ALPHA;
                break;

            case OPT_NO_WIC:
                dwFilterOpts |= TEX_FILTER_FORCE_NON_WIC;
                break;

            case OPT_OUTPUTFILE:
            {
                wcscpy_s(szOutputFile, MAX_PATH, pValue);

                wchar_t ext[_MAX_EXT];
                _wsplitpath_s(szOutputFile, nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT);

                fileType = LookupByName(ext, g_pExtFileTypes);

                switch (dwCommand)
                {
                case CMD_H_CROSS:
                case CMD_V_CROSS:
                case CMD_H_STRIP:
                case CMD_V_STRIP:
                case CMD_MERGE:
                    break;

                default:
                    if (fileType != CODEC_DDS)
                    {
                        wprintf(L"Assembled output file must be a dds\n");
                        return 1;
                    }
                }
                break;
            }

            case OPT_TA_WRAP:
                if (dwFilterOpts & TEX_FILTER_MIRROR)
                {
                    wprintf(L"Can't use -wrap and -mirror at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                dwFilterOpts |= TEX_FILTER_WRAP;
                break;

            case OPT_TA_MIRROR:
                if (dwFilterOpts & TEX_FILTER_WRAP)
                {
                    wprintf(L"Can't use -wrap and -mirror at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                dwFilterOpts |= TEX_FILTER_MIRROR;
                break;

            case OPT_FILELIST:
                {
                    std::wifstream inFile(pValue);
                    if (!inFile)
                    {
                        wprintf(L"Error opening -flist file %ls\n", pValue);
                        return 1;
                    }
                    wchar_t fname[1024] = {};
                    for (;;)
                    {
                        inFile >> fname;
                        if (!inFile)
                            break;

                        if (*fname == L'#')
                        {
                            // Comment
                        }
                        else if (*fname == L'-')
                        {
                            wprintf(L"Command-line arguments not supported in -flist file\n");
                            return 1;
                        }
                        else if (wcspbrk(fname, L"?*") != nullptr)
                        {
                            wprintf(L"Wildcards not supported in -flist file\n");
                            return 1;
                        }
                        else
                        {
                            SConversion conv;
                            wcscpy_s(conv.szSrc, MAX_PATH, fname);
                            conversion.push_back(conv);
                        }

                        inFile.ignore(1000, '\n');
                    }
                    inFile.close();
                }
                break;

            case OPT_GIF_BGCOLOR:
                if (dwCommand != CMD_GIF)
                {
                    wprintf(L"-bgcolor only applies to gif command\n");
                    return 1;
                }
                break;
            }
        }
        else if (wcspbrk(pArg, L"?*") != nullptr)
        {
            size_t count = conversion.size();
            SearchForFiles(pArg, conversion, (dwOptions & (1 << OPT_RECURSIVE)) != 0);
            if (conversion.size() <= count)
            {
                wprintf(L"No matching files found for %ls\n", pArg);
                return 1;
            }
        }
        else
        {
            SConversion conv;
            wcscpy_s(conv.szSrc, MAX_PATH, pArg);

            conversion.push_back(conv);
        }
    }

    if (conversion.empty())
    {
        PrintUsage();
        return 0;
    }

    if (~dwOptions & (1 << OPT_NOLOGO))
        PrintLogo();

    switch (dwCommand)
    {
    case CMD_H_CROSS:
    case CMD_V_CROSS:
    case CMD_H_STRIP:
    case CMD_V_STRIP:
    case CMD_GIF:
        if (conversion.size() > 1)
        {
            wprintf(L"ERROR: cross/strip/gif output only accepts 1 input file\n");
            return 1;
        }
        break;

    case CMD_MERGE:
        if (conversion.size() > 2)
        {
            wprintf(L"ERROR: merge output only accepts 2 input files\n");
            return 1;
        }
        break;
    }

    // Convert images
    size_t images = 0;

    std::vector<std::unique_ptr<ScratchImage>> loadedImages;

    if (dwCommand == CMD_GIF)
    {
        wchar_t ext[_MAX_EXT];
        wchar_t fname[_MAX_FNAME];
        _wsplitpath_s(conversion.front().szSrc, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT);

        wprintf(L"reading %ls", conversion.front().szSrc);
        fflush(stdout);

        if (!*szOutputFile)
        {
            _wmakepath_s(szOutputFile, nullptr, nullptr, fname, L".dds");
        }

        hr = LoadAnimatedGif(conversion.front().szSrc, loadedImages, (dwOptions & (1 << OPT_GIF_BGCOLOR)) != 0);
        if (FAILED(hr))
        {
            wprintf(L" FAILED (%x)\n", hr);
            return 1;
        }
    }
    else
    {
        for (auto pConv = conversion.begin(); pConv != conversion.end(); ++pConv)
        {
            wchar_t ext[_MAX_EXT];
            wchar_t fname[_MAX_FNAME];
            _wsplitpath_s(pConv->szSrc, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT);

            // Load source image
            if (pConv != conversion.begin())
                wprintf(L"\n");
            else if (!*szOutputFile)
            {
                switch (dwCommand)
                {
                case CMD_H_CROSS:
                case CMD_V_CROSS:
                case CMD_H_STRIP:
                case CMD_V_STRIP:
                    _wmakepath_s(szOutputFile, nullptr, nullptr, fname, L".bmp");
                    break;

                default:
                    if (_wcsicmp(ext, L".dds") == 0)
                    {
                        wprintf(L"ERROR: Need to specify output file via -o\n");
                        return 1;
                    }

                    _wmakepath_s(szOutputFile, nullptr, nullptr, fname, L".dds");
                    break;
                }
            }

            wprintf(L"reading %ls", pConv->szSrc);
            fflush(stdout);

            TexMetadata info;
            std::unique_ptr<ScratchImage> image(new (std::nothrow) ScratchImage);
            if (!image)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            switch (dwCommand)
            {
            case CMD_H_CROSS:
            case CMD_V_CROSS:
            case CMD_H_STRIP:
            case CMD_V_STRIP:
                if (_wcsicmp(ext, L".dds") == 0)
                {
                    hr = LoadFromDDSFile(pConv->szSrc, DDS_FLAGS_NONE, &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%x)\n", hr);
                        return 1;
                    }

                    if (!info.IsCubemap())
                    {
                        wprintf(L"\nERROR: Input must be a cubemap\n");
                        return 1;
                    }
                    else if (info.arraySize != 6)
                    {
                        wprintf(L"\nWARNING: Only the first cubemap in an array is written out as a cross/strip\n");
                    }
                }
                else
                {
                    wprintf(L"\nERROR: Input must be a dds of a cubemap\n");
                    return 1;
                }
                break;

            default:
                if (_wcsicmp(ext, L".dds") == 0)
                {
                    hr = LoadFromDDSFile(pConv->szSrc, DDS_FLAGS_NONE, &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%x)\n", hr);
                        return 1;
                    }

                    if (info.mipLevels > 1
                        || info.IsVolumemap()
                        || info.IsCubemap())
                    {
                        wprintf(L"\nERROR: Can't assemble complex surfaces\n");
                        return 1;
                    }
                }
                else if (_wcsicmp(ext, L".tga") == 0)
                {
                    hr = LoadFromTGAFile(pConv->szSrc, &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%x)\n", hr);
                        return 1;
                    }
                }
                else if (_wcsicmp(ext, L".hdr") == 0)
                {
                    hr = LoadFromHDRFile(pConv->szSrc, &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%x)\n", hr);
                        return 1;
                    }
                }
#ifdef USE_OPENEXR
                else if (_wcsicmp(ext, L".exr") == 0)
                {
                    hr = LoadFromEXRFile(pConv->szSrc, &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%x)\n", hr);
                        continue;
                    }
                }
#endif
                else
                {
                    // WIC shares the same filter values for mode and dither
                    static_assert(WIC_FLAGS_DITHER == TEX_FILTER_DITHER, "WIC_FLAGS_* & TEX_FILTER_* should match");
                    static_assert(WIC_FLAGS_DITHER_DIFFUSION == TEX_FILTER_DITHER_DIFFUSION, "WIC_FLAGS_* & TEX_FILTER_* should match");
                    static_assert(WIC_FLAGS_FILTER_POINT == TEX_FILTER_POINT, "WIC_FLAGS_* & TEX_FILTER_* should match");
                    static_assert(WIC_FLAGS_FILTER_LINEAR == TEX_FILTER_LINEAR, "WIC_FLAGS_* & TEX_FILTER_* should match");
                    static_assert(WIC_FLAGS_FILTER_CUBIC == TEX_FILTER_CUBIC, "WIC_FLAGS_* & TEX_FILTER_* should match");
                    static_assert(WIC_FLAGS_FILTER_FANT == TEX_FILTER_FANT, "WIC_FLAGS_* & TEX_FILTER_* should match");

                    hr = LoadFromWICFile(pConv->szSrc, dwFilter | WIC_FLAGS_ALL_FRAMES, &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%x)\n", hr);
                        return 1;
                    }
                }
                break;
            }

            PrintInfo(info);

            // Convert texture
            fflush(stdout);

            // --- Planar ------------------------------------------------------------------
            if (IsPlanar(info.format))
            {
                auto img = image->GetImage(0, 0, 0);
                assert(img);
                size_t nimg = image->GetImageCount();

                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                hr = ConvertToSinglePlane(img, nimg, info, *timage);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [converttosingleplane] (%x)\n", hr);
                    continue;
                }

                auto& tinfo = timage->GetMetadata();

                info.format = tinfo.format;

                assert(info.width == tinfo.width);
                assert(info.height == tinfo.height);
                assert(info.depth == tinfo.depth);
                assert(info.arraySize == tinfo.arraySize);
                assert(info.mipLevels == tinfo.mipLevels);
                assert(info.miscFlags == tinfo.miscFlags);
                assert(info.dimension == tinfo.dimension);

                image.swap(timage);
            }

            // --- Decompress --------------------------------------------------------------
            if (IsCompressed(info.format))
            {
                const Image* img = image->GetImage(0, 0, 0);
                assert(img);
                size_t nimg = image->GetImageCount();

                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                hr = Decompress(img, nimg, info, DXGI_FORMAT_UNKNOWN /* picks good default */, *timage.get());
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [decompress] (%x)\n", hr);
                    continue;
                }

                const TexMetadata& tinfo = timage->GetMetadata();

                info.format = tinfo.format;

                assert(info.width == tinfo.width);
                assert(info.height == tinfo.height);
                assert(info.depth == tinfo.depth);
                assert(info.arraySize == tinfo.arraySize);
                assert(info.mipLevels == tinfo.mipLevels);
                assert(info.miscFlags == tinfo.miscFlags);
                assert(info.dimension == tinfo.dimension);

                image.swap(timage);
            }

            // --- Undo Premultiplied Alpha (if requested) ---------------------------------
            if ((dwOptions & (1 << OPT_DEMUL_ALPHA))
                && HasAlpha(info.format)
                && info.format != DXGI_FORMAT_A8_UNORM)
            {
                if (info.GetAlphaMode() == TEX_ALPHA_MODE_STRAIGHT)
                {
                    printf("\nWARNING: Image is already using straight alpha\n");
                }
                else if (!info.IsPMAlpha())
                {
                    printf("\nWARNING: Image is not using premultipled alpha\n");
                }
                else
                {
                    auto img = image->GetImage(0, 0, 0);
                    assert(img);
                    size_t nimg = image->GetImageCount();

                    std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                    if (!timage)
                    {
                        wprintf(L"\nERROR: Memory allocation failed\n");
                        return 1;
                    }

                    hr = PremultiplyAlpha(img, nimg, info, TEX_PMALPHA_REVERSE | dwSRGB, *timage);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED [demultiply alpha] (%x)\n", hr);
                        continue;
                    }

                    auto& tinfo = timage->GetMetadata();
                    info.miscFlags2 = tinfo.miscFlags2;

                    assert(info.width == tinfo.width);
                    assert(info.height == tinfo.height);
                    assert(info.depth == tinfo.depth);
                    assert(info.arraySize == tinfo.arraySize);
                    assert(info.mipLevels == tinfo.mipLevels);
                    assert(info.miscFlags == tinfo.miscFlags);
                    assert(info.dimension == tinfo.dimension);

                    image.swap(timage);
                }
            }

            // --- Resize ------------------------------------------------------------------
            if (!width)
            {
                width = info.width;
            }
            if (!height)
            {
                height = info.height;
            }
            if (info.width != width || info.height != height)
            {
                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                hr = Resize(image->GetImages(), image->GetImageCount(), image->GetMetadata(), width, height, dwFilter | dwFilterOpts, *timage.get());
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [resize] (%x)\n", hr);
                    return 1;
                }

                const TexMetadata& tinfo = timage->GetMetadata();

                assert(tinfo.width == width && tinfo.height == height && tinfo.mipLevels == 1);
                info.width = tinfo.width;
                info.height = tinfo.height;
                info.mipLevels = 1;

                assert(info.depth == tinfo.depth);
                assert(info.arraySize == tinfo.arraySize);
                assert(info.miscFlags == tinfo.miscFlags);
                assert(info.format == tinfo.format);
                assert(info.dimension == tinfo.dimension);

                image.swap(timage);
            }

            // --- Tonemap (if requested) --------------------------------------------------
            if (dwOptions & (1 << OPT_TONEMAP))
            {
                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                // Compute max luminosity across all images
                XMVECTOR maxLum = XMVectorZero();
                hr = EvaluateImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](const XMVECTOR* pixels, size_t width, size_t y)
                {
                    UNREFERENCED_PARAMETER(y);

                    for (size_t j = 0; j < width; ++j)
                    {
                        static const XMVECTORF32 s_luminance = { 0.3f, 0.59f, 0.11f, 0.f };

                        XMVECTOR v = *pixels++;

                        v = XMVector3Dot(v, s_luminance);

                        maxLum = XMVectorMax(v, maxLum);
                    }
                });
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [tonemap maxlum] (%x)\n", hr);
                    return 1;
                }

                // Reinhard et al, "Photographic Tone Reproduction for Digital Images" 
                // http://www.cs.utah.edu/~reinhard/cdrom/
                maxLum = XMVectorMultiply(maxLum, maxLum);

                hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t width, size_t y)
                {
                    UNREFERENCED_PARAMETER(y);

                    for (size_t j = 0; j < width; ++j)
                    {
                        XMVECTOR value = inPixels[j];

                        XMVECTOR scale = XMVectorDivide(
                            XMVectorAdd(g_XMOne, XMVectorDivide(value, maxLum)),
                            XMVectorAdd(g_XMOne, value));
                        XMVECTOR nvalue = XMVectorMultiply(value, scale);

                        value = XMVectorSelect(value, nvalue, g_XMSelect1110);

                        outPixels[j] = value;
                    }
                }, *timage);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [tonemap apply] (%x)\n", hr);
                    return 1;
                }

                auto& tinfo = timage->GetMetadata();
                tinfo;

                assert(info.width == tinfo.width);
                assert(info.height == tinfo.height);
                assert(info.depth == tinfo.depth);
                assert(info.arraySize == tinfo.arraySize);
                assert(info.mipLevels == tinfo.mipLevels);
                assert(info.miscFlags == tinfo.miscFlags);
                assert(info.format == tinfo.format);
                assert(info.dimension == tinfo.dimension);

                image.swap(timage);
            }

            // --- Convert -----------------------------------------------------------------
            if (format == DXGI_FORMAT_UNKNOWN)
            {
                format = info.format;
            }
            else if (info.format != format && !IsCompressed(format))
            {
                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                hr = Convert(image->GetImages(), image->GetImageCount(), image->GetMetadata(), format,
                    dwFilter | dwFilterOpts | dwSRGB, TEX_THRESHOLD_DEFAULT, *timage.get());
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [convert] (%x)\n", hr);
                    return 1;
                }

                const TexMetadata& tinfo = timage->GetMetadata();

                assert(tinfo.format == format);
                info.format = tinfo.format;

                assert(info.width == tinfo.width);
                assert(info.height == tinfo.height);
                assert(info.depth == tinfo.depth);
                assert(info.arraySize == tinfo.arraySize);
                assert(info.mipLevels == tinfo.mipLevels);
                assert(info.miscFlags == tinfo.miscFlags);
                assert(info.dimension == tinfo.dimension);

                image.swap(timage);
            }

            images += info.arraySize;
            loadedImages.emplace_back(std::move(image));
        }
    }

    switch (dwCommand)
    {
    case CMD_CUBE:
        if (images != 6)
        {
            wprintf(L"\nERROR: cube requires six images to form the faces of the cubemap\n");
            return 1;
        }
        break;

    case CMD_CUBEARRAY:
        if ((images < 6) || (images % 6) != 0)
        {
            wprintf(L"cubearray requires a multiple of 6 images to form the faces of the cubemaps\n");
            return 1;
        }
        break;

    case CMD_H_CROSS:
    case CMD_V_CROSS:
    case CMD_H_STRIP:
    case CMD_V_STRIP:
    case CMD_GIF:
        break;

    default:
        if (images < 2)
        {
            wprintf(L"\nERROR: Need at least 2 images to assemble\n\n");
            return 1;
        }
        break;
    }

    // --- Create result ---------------------------------------------------------------
    switch (dwCommand)
    {
    case CMD_H_CROSS:
    case CMD_V_CROSS:
    case CMD_H_STRIP:
    case CMD_V_STRIP:
    {
        size_t twidth = 0;
        size_t theight = 0;

        switch (dwCommand)
        {
        case CMD_H_CROSS:
            //      posy
            // negx posz posx negz
            //      negy
            twidth = width * 4;
            theight = height * 3;
            break;

        case CMD_V_CROSS:
            //      posy
            // posz posx negz
            //      negy
            //      negx
            twidth = width * 3;
            theight = height * 4;
            break;

        case CMD_H_STRIP:
            twidth = width * 6;
            theight = height;
            break;

        case CMD_V_STRIP:
            twidth = width;
            theight = height * 6;
        }

        ScratchImage result;
        hr = result.Initialize2D(format, twidth, theight, 1, 1);
        if (FAILED(hr))
        {
            wprintf(L"FAILED setting up result image (%x)\n", hr);
            return 1;
        }

        memset(result.GetPixels(), 0, result.GetPixelsSize());

        auto src = loadedImages.cbegin();
        auto dest = result.GetImage(0, 0, 0);

        for (size_t index = 0; index < 6; ++index)
        {
            auto img = (*src)->GetImage(0, index, 0);
            if (!img)
            {
                wprintf(L"FAILED: Unexpected error\n");
                return 1;
            }

            Rect rect(0, 0, width, height);

            size_t offsetx = 0;
            size_t offsety = 0;

            switch (dwCommand)
            {
            case CMD_H_CROSS:
            {
                //      posy
                // negx posz posx negz
                //      negy

                static const size_t s_offsetx[6] = { 2, 0, 1, 1, 1, 3 };
                static const size_t s_offsety[6] = { 1, 1, 0, 2, 1, 1 };

                offsetx = s_offsetx[index] * width;
                offsety = s_offsety[index] * height;
                break;
            }

            case CMD_V_CROSS:
            {
                //      posy
                // posz posx negz
                //      negy
                //      negx

                static const size_t s_offsetx[6] = { 1, 1, 1, 1, 0, 2 };
                static const size_t s_offsety[6] = { 1, 3, 0, 2, 1, 1 };

                offsetx = s_offsetx[index] * width;
                offsety = s_offsety[index] * height;
                break;
            }

            case CMD_H_STRIP:
                // posx, negx, posy, negy, posz, negz
                offsetx = index * width;
                break;

            case CMD_V_STRIP:
                // posx, negx, posy, negy, posz, negz
                offsety = index * height;
                break;
            }

            hr = CopyRectangle(*img, rect, *dest, dwFilter | dwFilterOpts, offsetx, offsety);
            if (FAILED(hr))
            {
                wprintf(L"FAILED building result image (%x)\n", hr);
                return 1;
            }
        }

        // Write cross/strip
        wprintf(L"\nWriting %ls ", szOutputFile);
        PrintInfo(result.GetMetadata());
        wprintf(L"\n");
        fflush(stdout);

        if (~dwOptions & (1 << OPT_OVERWRITE))
        {
            if (GetFileAttributesW(szOutputFile) != INVALID_FILE_ATTRIBUTES)
            {
                wprintf(L"\nERROR: Output file already exists, use -y to overwrite\n");
                return 1;
            }
        }

        hr = SaveImageFile(*dest, fileType, szOutputFile);
        if (FAILED(hr))
        {
            wprintf(L" FAILED (%x)\n", hr);
            return 1;
        }
        break;
    }

    case CMD_MERGE:
    {
        // Capture alpha from source image (the second input filename)
        ScratchImage alphaImage;
        hr = alphaImage.Initialize2D(DXGI_FORMAT_R32_FLOAT, width, height, 1, 1);
        if (FAILED(hr))
        {
            wprintf(L"FAILED setting up alpha image (%x)\n", hr);
            return 1;
        }

        const Image& img = *alphaImage.GetImage(0, 0, 0);

        hr = EvaluateImage(*loadedImages[1]->GetImage(0, 0, 0),
            [&](const XMVECTOR* pixels, size_t width, size_t y)
        {
            auto alphaPtr = reinterpret_cast<float*>(img.pixels + img.rowPitch * y);

            for (size_t j = 0; j < width; ++j)
            {
                XMVECTOR value = pixels[j];

                // DXTex's Open Alpha onto Surface always loaded alpha from the blue channel

                *alphaPtr++ = XMVectorGetZ(value);
            }
        });
        if (FAILED(hr))
        {
            wprintf(L" FAILED [reading alpha image] (%x)\n", hr);
            return 1;
        }

        // Merge with rgb from our source iamge (the first input filename)
        const Image& rgb = *loadedImages[0]->GetImage(0, 0, 0);

        ScratchImage result;
        hr = TransformImage(rgb, [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t width, size_t y)
        {
            auto alphaPtr = reinterpret_cast<float*>(img.pixels + img.rowPitch * y);

            for (size_t j = 0; j < width; ++j)
            {
                XMVECTOR value = inPixels[j];

                XMVECTOR nvalue = XMVectorReplicate(*alphaPtr++);

                value = XMVectorSelect(nvalue, value, g_XMSelect1110);

                outPixels[j] = value;
            }
        }, result);
        if (FAILED(hr))
        {
            wprintf(L" FAILED [merge image] (%x)\n", hr);
            return 1;
        }

        // Write merged texture
        wprintf(L"\nWriting %ls ", szOutputFile);
        PrintInfo(result.GetMetadata());
        wprintf(L"\n");
        fflush(stdout);

        if (~dwOptions & (1 << OPT_OVERWRITE))
        {
            if (GetFileAttributesW(szOutputFile) != INVALID_FILE_ATTRIBUTES)
            {
                wprintf(L"\nERROR: Output file already exists, use -y to overwrite\n");
                return 1;
            }
        }

        hr = SaveImageFile(*result.GetImage(0, 0, 0), fileType, szOutputFile);
        if (FAILED(hr))
        {
            wprintf(L" FAILED (%x)\n", hr);
            return 1;
        }
        break;
    }

    default:
    {
        std::vector<Image> imageArray;
        imageArray.reserve(images);

        for (auto it = loadedImages.cbegin(); it != loadedImages.cend(); ++it)
        {
            const ScratchImage* simage = it->get();
            assert(simage != 0);
            for (size_t j = 0; j < simage->GetMetadata().arraySize; ++j)
            {
                const Image* img = simage->GetImage(0, j, 0);
                assert(img != 0);
                imageArray.push_back(*img);
            }
        }

        ScratchImage result;
        switch (dwCommand)
        {
        case CMD_VOLUME:
            hr = result.Initialize3DFromImages(&imageArray[0], imageArray.size());
            break;

        case CMD_ARRAY:
        case CMD_GIF:
            hr = result.InitializeArrayFromImages(&imageArray[0], imageArray.size(), (dwOptions & (1 << OPT_USE_DX10)) != 0);
            break;

        case CMD_CUBE:
        case CMD_CUBEARRAY:
            hr = result.InitializeCubeFromImages(&imageArray[0], imageArray.size());
            break;
        }

        if (FAILED(hr))
        {
            wprintf(L"FAILED building result image (%x)\n", hr);
            return 1;
        }

        // Write texture
        wprintf(L"\nWriting %ls ", szOutputFile);
        PrintInfo(result.GetMetadata());
        wprintf(L"\n");
        fflush(stdout);

        if (~dwOptions & (1 << OPT_OVERWRITE))
        {
            if (GetFileAttributesW(szOutputFile) != INVALID_FILE_ATTRIBUTES)
            {
                wprintf(L"\nERROR: Output file already exists, use -y to overwrite\n");
                return 1;
            }
        }

        hr = SaveToDDSFile(result.GetImages(), result.GetImageCount(), result.GetMetadata(),
            (dwOptions & (1 << OPT_USE_DX10)) ? (DDS_FLAGS_FORCE_DX10_EXT | DDS_FLAGS_FORCE_DX10_EXT_MISC2) : DDS_FLAGS_NONE,
            szOutputFile);
        if (FAILED(hr))
        {
            wprintf(L"\nFAILED (%x)\n", hr);
            return 1;
        }
        break;
    }
    }

    return 0;
}
