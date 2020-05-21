//--------------------------------------------------------------------------------------
// File: TexConv.cpp
//
// DirectX Texture Converter
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

#include <wrl\client.h>

#include <d3d11.h>
#include <dxgi.h>
#include <dxgiformat.h>

#include <wincodec.h>

#pragma warning(disable : 4619 4616 26812)

#include "DirectXTex.h"

#include "DirectXPackedVector.h"

//Uncomment to add support for OpenEXR (.exr)
//#define USE_OPENEXR

#ifdef USE_OPENEXR
// See <https://github.com/Microsoft/DirectXTex/wiki/Adding-OpenEXR> for details
#include "DirectXTexEXR.h"
#endif

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

enum OPTIONS
{
    OPT_RECURSIVE = 1,
    OPT_FILELIST,
    OPT_WIDTH,
    OPT_HEIGHT,
    OPT_MIPLEVELS,
    OPT_FORMAT,
    OPT_FILTER,
    OPT_SRGBI,
    OPT_SRGBO,
    OPT_SRGB,
    OPT_PREFIX,
    OPT_SUFFIX,
    OPT_OUTPUTDIR,
    OPT_TOLOWER,
    OPT_OVERWRITE,
    OPT_FILETYPE,
    OPT_HFLIP,
    OPT_VFLIP,
    OPT_DDS_DWORD_ALIGN,
    OPT_DDS_BAD_DXTN_TAILS,
    OPT_USE_DX10,
    OPT_USE_DX9,
    OPT_TGA20,
    OPT_WIC_QUALITY,
    OPT_WIC_LOSSLESS,
    OPT_WIC_MULTIFRAME,
    OPT_NOLOGO,
    OPT_TIMING,
    OPT_SEPALPHA,
    OPT_NO_WIC,
    OPT_TYPELESS_UNORM,
    OPT_TYPELESS_FLOAT,
    OPT_PREMUL_ALPHA,
    OPT_DEMUL_ALPHA,
    OPT_EXPAND_LUMINANCE,
    OPT_TA_WRAP,
    OPT_TA_MIRROR,
    OPT_FORCE_SINGLEPROC,
    OPT_GPU,
    OPT_NOGPU,
    OPT_FEATURE_LEVEL,
    OPT_FIT_POWEROF2,
    OPT_ALPHA_THRESHOLD,
    OPT_ALPHA_WEIGHT,
    OPT_NORMAL_MAP,
    OPT_NORMAL_MAP_AMPLITUDE,
    OPT_BC_COMPRESS,
    OPT_COLORKEY,
    OPT_TONEMAP,
    OPT_X2_BIAS,
    OPT_PRESERVE_ALPHA_COVERAGE,
    OPT_INVERT_Y,
    OPT_ROTATE_COLOR,
    OPT_PAPER_WHITE_NITS,
    OPT_BCNONMULT4FIX,
    OPT_MAX
};

enum
{
    ROTATE_709_TO_HDR10 = 1,
    ROTATE_HDR10_TO_709,
    ROTATE_709_TO_2020,
    ROTATE_2020_TO_709,
    ROTATE_P3_TO_HDR10,
    ROTATE_P3_TO_2020,
};

static_assert(OPT_MAX <= 64, "dwOptions is a DWORD64 bitfield");

struct SConversion
{
    wchar_t szSrc[MAX_PATH];
    wchar_t szDest[MAX_PATH];
};

struct SValue
{
    LPCWSTR pName;
    DWORD dwValue;
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

const SValue g_pOptions[] =
{
    { L"r",             OPT_RECURSIVE },
    { L"flist",         OPT_FILELIST },
    { L"w",             OPT_WIDTH },
    { L"h",             OPT_HEIGHT },
    { L"m",             OPT_MIPLEVELS },
    { L"f",             OPT_FORMAT },
    { L"if",            OPT_FILTER },
    { L"srgbi",         OPT_SRGBI },
    { L"srgbo",         OPT_SRGBO },
    { L"srgb",          OPT_SRGB },
    { L"px",            OPT_PREFIX },
    { L"sx",            OPT_SUFFIX },
    { L"o",             OPT_OUTPUTDIR },
    { L"l",             OPT_TOLOWER },
    { L"y",             OPT_OVERWRITE },
    { L"ft",            OPT_FILETYPE },
    { L"hflip",         OPT_HFLIP },
    { L"vflip",         OPT_VFLIP },
    { L"dword",         OPT_DDS_DWORD_ALIGN },
    { L"badtails",      OPT_DDS_BAD_DXTN_TAILS },
    { L"dx10",          OPT_USE_DX10 },
    { L"dx9",           OPT_USE_DX9 },
    { L"tga20",         OPT_TGA20 },
    { L"wicq",          OPT_WIC_QUALITY },
    { L"wiclossless",   OPT_WIC_LOSSLESS },
    { L"wicmulti",      OPT_WIC_MULTIFRAME },
    { L"nologo",        OPT_NOLOGO },
    { L"timing",        OPT_TIMING },
    { L"sepalpha",      OPT_SEPALPHA },
    { L"keepcoverage",  OPT_PRESERVE_ALPHA_COVERAGE },
    { L"nowic",         OPT_NO_WIC },
    { L"tu",            OPT_TYPELESS_UNORM },
    { L"tf",            OPT_TYPELESS_FLOAT },
    { L"pmalpha",       OPT_PREMUL_ALPHA },
    { L"alpha",         OPT_DEMUL_ALPHA },
    { L"xlum",          OPT_EXPAND_LUMINANCE },
    { L"wrap",          OPT_TA_WRAP },
    { L"mirror",        OPT_TA_MIRROR },
    { L"singleproc",    OPT_FORCE_SINGLEPROC },
    { L"gpu",           OPT_GPU },
    { L"nogpu",         OPT_NOGPU },
    { L"fl",            OPT_FEATURE_LEVEL },
    { L"pow2",          OPT_FIT_POWEROF2 },
    { L"at",            OPT_ALPHA_THRESHOLD },
    { L"aw",            OPT_ALPHA_WEIGHT },
    { L"nmap",          OPT_NORMAL_MAP },
    { L"nmapamp",       OPT_NORMAL_MAP_AMPLITUDE },
    { L"bc",            OPT_BC_COMPRESS },
    { L"c",             OPT_COLORKEY },
    { L"tonemap",       OPT_TONEMAP },
    { L"x2bias",        OPT_X2_BIAS },
    { L"inverty",       OPT_INVERT_Y },
    { L"rotatecolor",   OPT_ROTATE_COLOR },
    { L"nits",          OPT_PAPER_WHITE_NITS },
    { L"fixbc4x4",      OPT_BCNONMULT4FIX},
    { nullptr,          0 }
};

#define DEFFMT(fmt) { L## #fmt, DXGI_FORMAT_ ## fmt }

const SValue g_pFormats[] =
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
    DEFFMT(R9G9B9E5_SHAREDEXP),
    DEFFMT(R8G8_B8G8_UNORM),
    DEFFMT(G8R8_G8B8_UNORM),
    DEFFMT(BC1_UNORM),
    DEFFMT(BC1_UNORM_SRGB),
    DEFFMT(BC2_UNORM),
    DEFFMT(BC2_UNORM_SRGB),
    DEFFMT(BC3_UNORM),
    DEFFMT(BC3_UNORM_SRGB),
    DEFFMT(BC4_UNORM),
    DEFFMT(BC4_SNORM),
    DEFFMT(BC5_UNORM),
    DEFFMT(BC5_SNORM),
    DEFFMT(B5G6R5_UNORM),
    DEFFMT(B5G5R5A1_UNORM),

    // DXGI 1.1 formats
    DEFFMT(B8G8R8A8_UNORM),
    DEFFMT(B8G8R8X8_UNORM),
    DEFFMT(R10G10B10_XR_BIAS_A2_UNORM),
    DEFFMT(B8G8R8A8_UNORM_SRGB),
    DEFFMT(B8G8R8X8_UNORM_SRGB),
    DEFFMT(BC6H_UF16),
    DEFFMT(BC6H_SF16),
    DEFFMT(BC7_UNORM),
    DEFFMT(BC7_UNORM_SRGB),

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

const SValue g_pFormatAliases[] =
{
    { L"DXT1", DXGI_FORMAT_BC1_UNORM },
    { L"DXT2", DXGI_FORMAT_BC2_UNORM },
    { L"DXT3", DXGI_FORMAT_BC2_UNORM },
    { L"DXT4", DXGI_FORMAT_BC3_UNORM },
    { L"DXT5", DXGI_FORMAT_BC3_UNORM },

    { L"RGBA", DXGI_FORMAT_R8G8B8A8_UNORM },
    { L"BGRA", DXGI_FORMAT_B8G8R8A8_UNORM },

    { L"FP16", DXGI_FORMAT_R16G16B16A16_FLOAT },
    { L"FP32", DXGI_FORMAT_R32G32B32A32_FLOAT },

    { L"BPTC", DXGI_FORMAT_BC7_UNORM },
    { L"BPTC_FLOAT", DXGI_FORMAT_BC6H_UF16 },

    { nullptr, DXGI_FORMAT_UNKNOWN }
};

const SValue g_pReadOnlyFormats[] =
{
    DEFFMT(R32G32B32A32_TYPELESS),
    DEFFMT(R32G32B32_TYPELESS),
    DEFFMT(R16G16B16A16_TYPELESS),
    DEFFMT(R32G32_TYPELESS),
    DEFFMT(R32G8X24_TYPELESS),
    DEFFMT(D32_FLOAT_S8X24_UINT),
    DEFFMT(R32_FLOAT_X8X24_TYPELESS),
    DEFFMT(X32_TYPELESS_G8X24_UINT),
    DEFFMT(R10G10B10A2_TYPELESS),
    DEFFMT(R8G8B8A8_TYPELESS),
    DEFFMT(R16G16_TYPELESS),
    DEFFMT(R32_TYPELESS),
    DEFFMT(D32_FLOAT),
    DEFFMT(R24G8_TYPELESS),
    DEFFMT(D24_UNORM_S8_UINT),
    DEFFMT(R24_UNORM_X8_TYPELESS),
    DEFFMT(X24_TYPELESS_G8_UINT),
    DEFFMT(R8G8_TYPELESS),
    DEFFMT(R16_TYPELESS),
    DEFFMT(R8_TYPELESS),
    DEFFMT(BC1_TYPELESS),
    DEFFMT(BC2_TYPELESS),
    DEFFMT(BC3_TYPELESS),
    DEFFMT(BC4_TYPELESS),
    DEFFMT(BC5_TYPELESS),

    // DXGI 1.1 formats
    DEFFMT(B8G8R8A8_TYPELESS),
    DEFFMT(B8G8R8X8_TYPELESS),
    DEFFMT(BC6H_TYPELESS),
    DEFFMT(BC7_TYPELESS),

    // DXGI 1.2 formats
    DEFFMT(NV12),
    DEFFMT(P010),
    DEFFMT(P016),
    DEFFMT(420_OPAQUE),
    DEFFMT(NV11),

    // DXGI 1.3 formats
    { L"P208", DXGI_FORMAT(130) },
    { L"V208", DXGI_FORMAT(131) },
    { L"V408", DXGI_FORMAT(132) },

    { nullptr, DXGI_FORMAT_UNKNOWN }
};

const SValue g_pFilters[] =
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

const SValue g_pRotateColor[] =
{
    { L"709to2020", ROTATE_709_TO_2020 },
    { L"2020to709", ROTATE_2020_TO_709 },
    { L"709toHDR10", ROTATE_709_TO_HDR10 },
    { L"HDR10to709", ROTATE_HDR10_TO_709 },
    { L"P3to2020", ROTATE_P3_TO_2020 },
    { L"P3toHDR10", ROTATE_P3_TO_HDR10 },
    { nullptr, 0 },
};

#define CODEC_DDS 0xFFFF0001
#define CODEC_TGA 0xFFFF0002
#define CODEC_HDP 0xFFFF0003
#define CODEC_JXR 0xFFFF0004
#define CODEC_HDR 0xFFFF0005

#ifdef USE_OPENEXR
#define CODEC_EXR 0xFFFF0006
#endif

const SValue g_pSaveFileTypes[] =   // valid formats to write to
{
    { L"BMP",   WIC_CODEC_BMP  },
    { L"JPG",   WIC_CODEC_JPEG },
    { L"JPEG",  WIC_CODEC_JPEG },
    { L"PNG",   WIC_CODEC_PNG  },
    { L"DDS",   CODEC_DDS      },
    { L"TGA",   CODEC_TGA      },
    { L"HDR",   CODEC_HDR      },
    { L"TIF",   WIC_CODEC_TIFF },
    { L"TIFF",  WIC_CODEC_TIFF },
    { L"WDP",   WIC_CODEC_WMP  },
    { L"HDP",   CODEC_HDP      },
    { L"JXR",   CODEC_JXR      },
#ifdef USE_OPENEXR
    { L"EXR",   CODEC_EXR      },
#endif
    { nullptr,  CODEC_DDS      }
};

const SValue g_pFeatureLevels[] =   // valid feature levels for -fl for maximimum size
{
    { L"9.1",  2048 },
    { L"9.2",  2048 },
    { L"9.3",  4096 },
    { L"10.0", 8192 },
    { L"10.1", 8192 },
    { L"11.0", 16384 },
    { L"11.1", 16384 },
    { L"12.0", 16384 },
    { L"12.1", 16384 },
    { nullptr, 0 },
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#pragma warning( disable : 4616 6211 )

namespace
{
    inline HANDLE safe_handle(HANDLE h) { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

    struct handle_closer { void operator()(HANDLE h) { assert(h != INVALID_HANDLE_VALUE); if (h) CloseHandle(h); } };

    using ScopedHandle = std::unique_ptr<void, handle_closer>;

    struct find_closer { void operator()(HANDLE h) { assert(h != INVALID_HANDLE_VALUE); if (h) FindClose(h); } };

    using ScopedFindHandle = std::unique_ptr<void, find_closer>;

    inline static bool ispow2(size_t x)
    {
        return ((x != 0) && !(x & (x - 1)));
    }

#ifdef _PREFAST_
#pragma prefast(disable : 26018, "Only used with static internal arrays")
#endif

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
        WIN32_FIND_DATAW findData = {};
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

                if (!FindNextFileW(hFile.get(), &findData))
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

                if (!FindNextFileW(hFile.get(), &findData))
                    break;
            }
        }
    }

    void PrintFormat(DXGI_FORMAT Format)
    {
        for (const SValue *pFormat = g_pFormats; pFormat->pName; pFormat++)
        {
            if (static_cast<DXGI_FORMAT>(pFormat->dwValue) == Format)
            {
                wprintf(L"%ls", pFormat->pName);
                return;
            }
        }

        for (const SValue *pFormat = g_pReadOnlyFormats; pFormat->pName; pFormat++)
        {
            if (static_cast<DXGI_FORMAT>(pFormat->dwValue) == Format)
            {
                wprintf(L"%ls", pFormat->pName);
                return;
            }
        }

        wprintf(L"*UNKNOWN*");
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
            wprintf(L"%ls", (info.arraySize > 1) ? L" 1DArray" : L" 1D");
            break;

        case TEX_DIMENSION_TEXTURE2D:
            if (info.IsCubemap())
            {
                wprintf(L"%ls", (info.arraySize > 6) ? L" CubeArray" : L" Cube");
            }
            else
            {
                wprintf(L"%ls", (info.arraySize > 1) ? L" 2DArray" : L" 2D");
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
        case TEX_ALPHA_MODE_CUSTOM:
            wprintf(L" \x0e0:Custom");
            break;
        case TEX_ALPHA_MODE_UNKNOWN:
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
        wprintf(L"Microsoft (R) DirectX Texture Converter (DirectXTex version)\n");
        wprintf(L"Copyright (C) Microsoft Corp. All rights reserved.\n");
#ifdef _DEBUG
        wprintf(L"*** Debug build ***\n");
#endif
        wprintf(L"\n");
    }

    _Success_(return != false)
        bool GetDXGIFactory(_Outptr_ IDXGIFactory1** pFactory)
    {
        if (!pFactory)
            return false;

        *pFactory = nullptr;

        typedef HRESULT(WINAPI* pfn_CreateDXGIFactory1)(REFIID riid, _Out_ void **ppFactory);

        static pfn_CreateDXGIFactory1 s_CreateDXGIFactory1 = nullptr;

        if (!s_CreateDXGIFactory1)
        {
            HMODULE hModDXGI = LoadLibraryW(L"dxgi.dll");
            if (!hModDXGI)
                return false;

            s_CreateDXGIFactory1 = reinterpret_cast<pfn_CreateDXGIFactory1>(reinterpret_cast<void*>(GetProcAddress(hModDXGI, "CreateDXGIFactory1")));
            if (!s_CreateDXGIFactory1)
                return false;
        }

        return SUCCEEDED(s_CreateDXGIFactory1(IID_PPV_ARGS(pFactory)));
    }

    void PrintUsage()
    {
        PrintLogo();

        wprintf(L"Usage: texconv <options> <files>\n\n");
        wprintf(L"   -r                  wildcard filename search is recursive\n");
        wprintf(L"   -flist <filename>   use text file with a list of input files (one per line)\n");
        wprintf(L"\n   -w <n>              width\n");
        wprintf(L"   -h <n>              height\n");
        wprintf(L"   -m <n>              miplevels\n");
        wprintf(L"   -f <format>         format\n");
        wprintf(L"\n   -if <filter>        image filtering\n");
        wprintf(L"   -srgb{i|o}          sRGB {input, output}\n");
        wprintf(L"\n   -px <string>        name prefix\n");
        wprintf(L"   -sx <string>        name suffix\n");
        wprintf(L"   -o <directory>      output directory\n");
        wprintf(L"   -l                  force output filename to lower case\n");
        wprintf(L"   -y                  overwrite existing output file (if any)\n");
        wprintf(L"   -ft <filetype>      output file type\n");
        wprintf(L"\n   -hflip              horizonal flip of source image\n");
        wprintf(L"   -vflip              vertical flip of source image\n");
        wprintf(L"\n   -sepalpha           resize/generate mips alpha channel separately\n");
        wprintf(L"                       from color channels\n");
        wprintf(L"   -keepcoverage <ref> Preserve alpha coverage in mips for alpha test ref\n");
        wprintf(L"\n   -nowic              Force non-WIC filtering\n");
        wprintf(L"   -wrap, -mirror      texture addressing mode (wrap, mirror, or clamp)\n");
        wprintf(L"   -pmalpha            convert final texture to use premultiplied alpha\n");
        wprintf(L"   -alpha              convert premultiplied alpha to straight alpha\n");
        wprintf(
            L"   -at <threshold>     Alpha threshold used for BC1, RGBA5551, and WIC\n"
            L"                       (defaults to 0.5)\n");
        wprintf(L"\n   -fl <feature-level> Set maximum feature level target (defaults to 11.0)\n");
        wprintf(L"   -pow2               resize to fit a power-of-2, respecting aspect ratio\n");
        wprintf(
            L"\n   -nmap <options>     converts height-map to normal-map\n"
            L"                       options must be one or more of\n"
            L"                          r, g, b, a, l, m, u, v, i, o\n");
        wprintf(L"   -nmapamp <weight>   normal map amplitude (defaults to 1.0)\n");
        wprintf(L"\n                       (DDS input only)\n");
        wprintf(L"   -t{u|f}             TYPELESS format is treated as UNORM or FLOAT\n");
        wprintf(L"   -dword              Use DWORD instead of BYTE alignment\n");
        wprintf(L"   -badtails           Fix for older DXTn with bad mipchain tails\n");
        wprintf(L"   -fixbc4x4           Fix for odd-sized BC files that Direct3D can't load\n");
        wprintf(L"   -xlum               expand legacy L8, L16, and A8P8 formats\n");
        wprintf(L"\n                       (DDS output only)\n");
        wprintf(L"   -dx10               Force use of 'DX10' extended header\n");
        wprintf(L"   -dx9                Force use of legacy DX9 header\n");
        wprintf(L"\n                       (TGA output only)\n");
        wprintf(L"   -tga20              Write file including TGA 2.0 extension area\n");
        wprintf(L"\n                       (BMP, PNG, JPG, TIF, WDP output only)\n");
        wprintf(L"   -wicq <quality>     When writing images with WIC use quality (0.0 to 1.0)\n");
        wprintf(L"   -wiclossless        When writing images with WIC use lossless mode\n");
        wprintf(L"   -wicmulti           When writing images with WIC encode multiframe images\n");
        wprintf(L"\n   -nologo             suppress copyright message\n");
        wprintf(L"   -timing             Display elapsed processing time\n\n");
#ifdef _OPENMP
        wprintf(L"   -singleproc         Do not use multi-threaded compression\n");
#endif
        wprintf(L"   -gpu <adapter>      Select GPU for DirectCompute-based codecs (0 is default)\n");
        wprintf(L"   -nogpu              Do not use DirectCompute-based codecs\n");
        wprintf(
            L"\n   -bc <options>       Sets options for BC compression\n"
            L"                       options must be one or more of\n"
            L"                          d, u, q, x\n");
        wprintf(
            L"   -aw <weight>        BC7 GPU compressor weighting for alpha error metric\n"
            L"                       (defaults to 1.0)\n");
        wprintf(L"\n   -c <hex-RGB>        colorkey (a.k.a. chromakey) transparency\n");
        wprintf(L"   -rotatecolor <rot>  rotates color primaries and/or applies a curve\n");
        wprintf(L"   -nits <value>       paper-white value in nits to use for HDR10 (def: 200.0)\n");
        wprintf(L"   -tonemap            Apply a tonemap operator based on maximum luminance\n");
        wprintf(L"   -x2bias             Enable *2 - 1 conversion cases for unorm/pos-only-float\n");
        wprintf(L"   -inverty            Invert Y (i.e. green) channel values\n");

        wprintf(L"\n   <format>: ");
        PrintList(13, g_pFormats);
        wprintf(L"      ");
        PrintList(13, g_pFormatAliases);

        wprintf(L"\n   <filter>: ");
        PrintList(13, g_pFilters);

        wprintf(L"\n   <rot>: ");
        PrintList(13, g_pRotateColor);

        wprintf(L"\n   <filetype>: ");
        PrintList(15, g_pSaveFileTypes);

        wprintf(L"\n   <feature-level>: ");
        PrintList(13, g_pFeatureLevels);

        ComPtr<IDXGIFactory1> dxgiFactory;
        if (GetDXGIFactory(dxgiFactory.GetAddressOf()))
        {
            wprintf(L"\n   <adapter>:\n");

            ComPtr<IDXGIAdapter> adapter;
            for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters(adapterIndex, adapter.ReleaseAndGetAddressOf()); ++adapterIndex)
            {
                DXGI_ADAPTER_DESC desc;
                if (SUCCEEDED(adapter->GetDesc(&desc)))
                {
                    wprintf(L"      %u: VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
                }
            }
        }
    }

    _Success_(return != false)
        bool CreateDevice(int adapter, _Outptr_ ID3D11Device** pDevice)
    {
        if (!pDevice)
            return false;

        *pDevice = nullptr;

        static PFN_D3D11_CREATE_DEVICE s_DynamicD3D11CreateDevice = nullptr;

        if (!s_DynamicD3D11CreateDevice)
        {
            HMODULE hModD3D11 = LoadLibraryW(L"d3d11.dll");
            if (!hModD3D11)
                return false;

            s_DynamicD3D11CreateDevice = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(reinterpret_cast<void*>(GetProcAddress(hModD3D11, "D3D11CreateDevice")));
            if (!s_DynamicD3D11CreateDevice)
                return false;
        }

        D3D_FEATURE_LEVEL featureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };

        UINT createDeviceFlags = 0;
#ifdef _DEBUG
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        ComPtr<IDXGIAdapter> pAdapter;
        if (adapter >= 0)
        {
            ComPtr<IDXGIFactory1> dxgiFactory;
            if (GetDXGIFactory(dxgiFactory.GetAddressOf()))
            {
                if (FAILED(dxgiFactory->EnumAdapters(static_cast<UINT>(adapter), pAdapter.GetAddressOf())))
                {
                    wprintf(L"\nERROR: Invalid GPU adapter index (%d)!\n", adapter);
                    return false;
                }
            }
        }

        D3D_FEATURE_LEVEL fl;
        HRESULT hr = s_DynamicD3D11CreateDevice(pAdapter.Get(),
            (pAdapter) ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
            nullptr, createDeviceFlags, featureLevels, _countof(featureLevels),
            D3D11_SDK_VERSION, pDevice, &fl, nullptr);
        if (SUCCEEDED(hr))
        {
            if (fl < D3D_FEATURE_LEVEL_11_0)
            {
                D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts;
                hr = (*pDevice)->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts));
                if (FAILED(hr))
                    memset(&hwopts, 0, sizeof(hwopts));

                if (!hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
                {
                    if (*pDevice)
                    {
                        (*pDevice)->Release();
                        *pDevice = nullptr;
                    }
                    hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                }
            }
        }

        if (SUCCEEDED(hr))
        {
            ComPtr<IDXGIDevice> dxgiDevice;
            hr = (*pDevice)->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()));
            if (SUCCEEDED(hr))
            {
                hr = dxgiDevice->GetAdapter(pAdapter.ReleaseAndGetAddressOf());
                if (SUCCEEDED(hr))
                {
                    DXGI_ADAPTER_DESC desc;
                    hr = pAdapter->GetDesc(&desc);
                    if (SUCCEEDED(hr))
                    {
                        wprintf(L"\n[Using DirectCompute on \"%ls\"]\n", desc.Description);
                    }
                }
            }

            return true;
        }
        else
            return false;
    }

    void FitPowerOf2(size_t origx, size_t origy, size_t& targetx, size_t& targety, size_t maxsize)
    {
        float origAR = float(origx) / float(origy);

        if (origx > origy)
        {
            size_t x;
            for (x = maxsize; x > 1; x >>= 1) { if (x <= targetx) break; }
            targetx = x;

            float bestScore = FLT_MAX;
            for (size_t y = maxsize; y > 0; y >>= 1)
            {
                float score = fabsf((float(x) / float(y)) - origAR);
                if (score < bestScore)
                {
                    bestScore = score;
                    targety = y;
                }
            }
        }
        else
        {
            size_t y;
            for (y = maxsize; y > 1; y >>= 1) { if (y <= targety) break; }
            targety = y;

            float bestScore = FLT_MAX;
            for (size_t x = maxsize; x > 0; x >>= 1)
            {
                float score = fabsf((float(x) / float(y)) - origAR);
                if (score < bestScore)
                {
                    bestScore = score;
                    targetx = x;
                }
            }
        }
    }

    const XMVECTORF32 c_MaxNitsFor2084 = { { { 10000.0f, 10000.0f, 10000.0f, 1.f } } };

    const XMMATRIX c_from709to2020 =
    {
        XMVECTOR { 0.6274040f, 0.0690970f, 0.0163916f, 0.f },
        XMVECTOR { 0.3292820f, 0.9195400f, 0.0880132f, 0.f },
        XMVECTOR { 0.0433136f, 0.0113612f, 0.8955950f, 0.f },
        XMVECTOR { 0.f,        0.f,        0.f,        1.f }
    };

    const XMMATRIX c_from2020to709 =
    {
        XMVECTOR { 1.6604910f,  -0.1245505f, -0.0181508f, 0.f },
        XMVECTOR { -0.5876411f,  1.1328999f, -0.1005789f, 0.f },
        XMVECTOR { -0.0728499f, -0.0083494f,  1.1187297f, 0.f },
        XMVECTOR { 0.f,          0.f,         0.f,        1.f }
    };

    const XMMATRIX c_fromP3to2020 =
    {
        XMVECTOR { 0.753845f, 0.0457456f, -0.00121055f, 0.f },
        XMVECTOR { 0.198593f, 0.941777f,   0.0176041f,  0.f },
        XMVECTOR { 0.047562f, 0.0124772f,  0.983607f,   0.f },
        XMVECTOR { 0.f,       0.f,         0.f,         1.f }
    };

    inline float LinearToST2084(float normalizedLinearValue)
    {
        float ST2084 = pow((0.8359375f + 18.8515625f * pow(abs(normalizedLinearValue), 0.1593017578f)) / (1.0f + 18.6875f * pow(abs(normalizedLinearValue), 0.1593017578f)), 78.84375f);
        return ST2084;  // Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits
    }

    inline float ST2084ToLinear(float ST2084)
    {
        float normalizedLinear = pow(std::max(pow(abs(ST2084), 1.0f / 78.84375f) - 0.8359375f, 0.0f) / (18.8515625f - 18.6875f * pow(abs(ST2084), 1.0f / 78.84375f)), 1.0f / 0.1593017578f);
        return normalizedLinear;
    }

    HRESULT ReadData(_In_z_ const wchar_t* szFile, std::unique_ptr<uint8_t[]>& blob, size_t& bmpSize)
    {
        blob.reset();

        ScopedHandle hFile(safe_handle(CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_FLAG_SEQUENTIAL_SCAN, nullptr)));
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

        // File is too big for 32-bit allocation, so reject read (4 GB should be plenty large enough)
        if (fileInfo.EndOfFile.HighPart > 0)
        {
            return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        }

        // Zero-sized files assumed to be invalid
        if (fileInfo.EndOfFile.LowPart < 1)
        {
            return E_FAIL;
        }

        // Read file
        blob.reset(new (std::nothrow) uint8_t[fileInfo.EndOfFile.LowPart]);
        if (!blob)
        {
            return E_OUTOFMEMORY;
        }

        DWORD bytesRead = 0;
        if (!ReadFile(hFile.get(), blob.get(), fileInfo.EndOfFile.LowPart, &bytesRead, nullptr))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (bytesRead != fileInfo.EndOfFile.LowPart)
        {
            return E_FAIL;
        }

        bmpSize = fileInfo.EndOfFile.LowPart;

        return S_OK;
    }

    HRESULT LoadFromExtendedBMPMemory(_In_reads_bytes_(size) const void* pSource, _In_ size_t size, _Out_opt_ TexMetadata* metadata, _Out_ ScratchImage& image)
    {
        // This loads from non-standard BMP files that are not supported by WIC
        image.Release();

        if (size < (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)))
            return E_FAIL;

        // Valid BMP files always start with 'BM' at the top
        auto filehdr = reinterpret_cast<const BITMAPFILEHEADER*>(pSource);
        if (filehdr->bfType != 0x4D42)
            return E_FAIL;

        if (size < filehdr->bfOffBits)
            return E_FAIL;

        auto header = reinterpret_cast<const BITMAPINFOHEADER*>(reinterpret_cast<const uint8_t*>(pSource) + sizeof(BITMAPFILEHEADER));
        if (header->biSize != sizeof(BITMAPINFOHEADER))
            return E_FAIL;

        if (header->biWidth < 1 || header->biHeight < 1 || header->biPlanes != 1 || header->biBitCount != 16)
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        switch (header->biCompression)
        {
        case 0x31545844: // FourCC "DXT1"
            format = DXGI_FORMAT_BC1_UNORM;
            break;
        case 0x33545844: // FourCC "DXT3"
            format = DXGI_FORMAT_BC2_UNORM;
            break;
        case 0x35545844: // FourCC "DXT5"
            format = DXGI_FORMAT_BC3_UNORM;
            break;

        default:
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        HRESULT hr = image.Initialize2D(format, size_t(header->biWidth), size_t(header->biHeight), 1, 1);
        if (FAILED(hr))
            return hr;

        if (header->biSizeImage != image.GetPixelsSize())
            return E_UNEXPECTED;

        size_t remaining = size - filehdr->bfOffBits;
        if (!remaining)
            return E_FAIL;

        if (remaining < image.GetPixelsSize())
            return E_UNEXPECTED;

        auto pixels = reinterpret_cast<const uint8_t*>(pSource) + filehdr->bfOffBits;

        memcpy(image.GetPixels(), pixels, image.GetPixelsSize());

        if (metadata)
        {
            *metadata = image.GetMetadata();
        }

        return S_OK;
    }
}

//--------------------------------------------------------------------------------------
// Entry-point
//--------------------------------------------------------------------------------------
#ifdef _PREFAST_
#pragma prefast(disable : 28198, "Command-line tool, frees all memory on exit")
#endif

int __cdecl wmain(_In_ int argc, _In_z_count_(argc) wchar_t* argv[])
{
    // Parameters and defaults
    size_t width = 0;
    size_t height = 0;
    size_t mipLevels = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    DWORD dwFilter = TEX_FILTER_DEFAULT;
    DWORD dwSRGB = 0;
    DWORD dwConvert = 0;
    DWORD dwCompress = TEX_COMPRESS_DEFAULT;
    DWORD dwFilterOpts = 0;
    DWORD FileType = CODEC_DDS;
    DWORD maxSize = 16384;
    int adapter = -1;
    float alphaThreshold = TEX_THRESHOLD_DEFAULT;
    float alphaWeight = 1.f;
    DWORD dwNormalMap = 0;
    float nmapAmplitude = 1.f;
    float wicQuality = -1.f;
    DWORD colorKey = 0;
    DWORD dwRotateColor = 0;
    float paperWhiteNits = 200.f;
    float preserveAlphaCoverageRef = 0.0f;

    wchar_t szPrefix[MAX_PATH] = {};
    wchar_t szSuffix[MAX_PATH] = {};
    wchar_t szOutputDir[MAX_PATH] = {};

    // Initialize COM (needed for WIC)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        wprintf(L"Failed to initialize COM (%08X)\n", static_cast<unsigned int>(hr));
        return 1;
    }

    // Process command line
    DWORD64 dwOptions = 0;
    std::list<SConversion> conversion;

    for (int iArg = 1; iArg < argc; iArg++)
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

            if (!dwOption || (dwOptions & (DWORD64(1) << dwOption)))
            {
                PrintUsage();
                return 1;
            }

            dwOptions |= (DWORD64(1) << dwOption);

            // Handle options with additional value parameter
            switch (dwOption)
            {
            case OPT_WIDTH:
            case OPT_HEIGHT:
            case OPT_MIPLEVELS:
            case OPT_FORMAT:
            case OPT_FILTER:
            case OPT_PREFIX:
            case OPT_SUFFIX:
            case OPT_OUTPUTDIR:
            case OPT_FILETYPE:
            case OPT_GPU:
            case OPT_FEATURE_LEVEL:
            case OPT_ALPHA_THRESHOLD:
            case OPT_ALPHA_WEIGHT:
            case OPT_NORMAL_MAP:
            case OPT_NORMAL_MAP_AMPLITUDE:
            case OPT_WIC_QUALITY:
            case OPT_BC_COMPRESS:
            case OPT_COLORKEY:
            case OPT_FILELIST:
            case OPT_ROTATE_COLOR:
            case OPT_PAPER_WHITE_NITS:
            case OPT_PRESERVE_ALPHA_COVERAGE:
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
                break;
            }

            switch (dwOption)
            {
            case OPT_WIDTH:
                if (swscanf_s(pValue, L"%zu", &width) != 1)
                {
                    wprintf(L"Invalid value specified with -w (%ls)\n", pValue);
                    wprintf(L"\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_HEIGHT:
                if (swscanf_s(pValue, L"%zu", &height) != 1)
                {
                    wprintf(L"Invalid value specified with -h (%ls)\n", pValue);
                    printf("\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_MIPLEVELS:
                if (swscanf_s(pValue, L"%zu", &mipLevels) != 1)
                {
                    wprintf(L"Invalid value specified with -m (%ls)\n", pValue);
                    wprintf(L"\n");
                    PrintUsage();
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
                        wprintf(L"\n");
                        PrintUsage();
                        return 1;
                    }
                }
                break;

            case OPT_FILTER:
                dwFilter = LookupByName(pValue, g_pFilters);
                if (!dwFilter)
                {
                    wprintf(L"Invalid value specified with -if (%ls)\n", pValue);
                    wprintf(L"\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_ROTATE_COLOR:
                dwRotateColor = LookupByName(pValue, g_pRotateColor);
                if (!dwRotateColor)
                {
                    wprintf(L"Invalid value specified with -rotatecolor (%ls)\n", pValue);
                    wprintf(L"\n");
                    PrintUsage();
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

            case OPT_PREFIX:
                wcscpy_s(szPrefix, MAX_PATH, pValue);
                break;

            case OPT_SUFFIX:
                wcscpy_s(szSuffix, MAX_PATH, pValue);
                break;

            case OPT_OUTPUTDIR:
                wcscpy_s(szOutputDir, MAX_PATH, pValue);
                break;

            case OPT_FILETYPE:
                FileType = LookupByName(pValue, g_pSaveFileTypes);
                if (!FileType)
                {
                    wprintf(L"Invalid value specified with -ft (%ls)\n", pValue);
                    wprintf(L"\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_PREMUL_ALPHA:
                if (dwOptions & (DWORD64(1) << OPT_DEMUL_ALPHA))
                {
                    wprintf(L"Can't use -pmalpha and -alpha at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_DEMUL_ALPHA:
                if (dwOptions & (DWORD64(1) << OPT_PREMUL_ALPHA))
                {
                    wprintf(L"Can't use -pmalpha and -alpha at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                break;

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

            case OPT_NORMAL_MAP:
            {
                dwNormalMap = 0;

                if (wcschr(pValue, L'l'))
                {
                    dwNormalMap |= CNMAP_CHANNEL_LUMINANCE;
                }
                else if (wcschr(pValue, L'r'))
                {
                    dwNormalMap |= CNMAP_CHANNEL_RED;
                }
                else if (wcschr(pValue, L'g'))
                {
                    dwNormalMap |= CNMAP_CHANNEL_GREEN;
                }
                else if (wcschr(pValue, L'b'))
                {
                    dwNormalMap |= CNMAP_CHANNEL_BLUE;
                }
                else if (wcschr(pValue, L'a'))
                {
                    dwNormalMap |= CNMAP_CHANNEL_ALPHA;
                }
                else
                {
                    wprintf(L"Invalid value specified for -nmap (%ls), missing l, r, g, b, or a\n\n", pValue);
                    PrintUsage();
                    return 1;
                }

                if (wcschr(pValue, L'm'))
                {
                    dwNormalMap |= CNMAP_MIRROR;
                }
                else
                {
                    if (wcschr(pValue, L'u'))
                    {
                        dwNormalMap |= CNMAP_MIRROR_U;
                    }
                    if (wcschr(pValue, L'v'))
                    {
                        dwNormalMap |= CNMAP_MIRROR_V;
                    }
                }

                if (wcschr(pValue, L'i'))
                {
                    dwNormalMap |= CNMAP_INVERT_SIGN;
                }

                if (wcschr(pValue, L'o'))
                {
                    dwNormalMap |= CNMAP_COMPUTE_OCCLUSION;
                }
            }
            break;

            case OPT_NORMAL_MAP_AMPLITUDE:
                if (!dwNormalMap)
                {
                    wprintf(L"-nmapamp requires -nmap\n\n");
                    PrintUsage();
                    return 1;
                }
                else if (swscanf_s(pValue, L"%f", &nmapAmplitude) != 1)
                {
                    wprintf(L"Invalid value specified with -nmapamp (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                else if (nmapAmplitude < 0.f)
                {
                    wprintf(L"Normal map amplitude must be positive (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_GPU:
                if (swscanf_s(pValue, L"%d", &adapter) != 1)
                {
                    wprintf(L"Invalid value specified with -gpu (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                else if (adapter < 0)
                {
                    wprintf(L"Adapter index (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_FEATURE_LEVEL:
                maxSize = LookupByName(pValue, g_pFeatureLevels);
                if (!maxSize)
                {
                    wprintf(L"Invalid value specified with -fl (%ls)\n", pValue);
                    wprintf(L"\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_ALPHA_THRESHOLD:
                if (swscanf_s(pValue, L"%f", &alphaThreshold) != 1)
                {
                    wprintf(L"Invalid value specified with -at (%ls)\n", pValue);
                    wprintf(L"\n");
                    PrintUsage();
                    return 1;
                }
                else if (alphaThreshold < 0.f)
                {
                    wprintf(L"-at (%ls) parameter must be positive\n", pValue);
                    wprintf(L"\n");
                    return 1;
                }
                break;

            case OPT_ALPHA_WEIGHT:
                if (swscanf_s(pValue, L"%f", &alphaWeight) != 1)
                {
                    wprintf(L"Invalid value specified with -aw (%ls)\n", pValue);
                    wprintf(L"\n");
                    PrintUsage();
                    return 1;
                }
                else if (alphaWeight < 0.f)
                {
                    wprintf(L"-aw (%ls) parameter must be positive\n", pValue);
                    wprintf(L"\n");
                    return 1;
                }
                break;

            case OPT_BC_COMPRESS:
            {
                dwCompress = TEX_COMPRESS_DEFAULT;

                bool found = false;
                if (wcschr(pValue, L'u'))
                {
                    dwCompress |= TEX_COMPRESS_UNIFORM;
                    found = true;
                }

                if (wcschr(pValue, L'd'))
                {
                    dwCompress |= TEX_COMPRESS_DITHER;
                    found = true;
                }

                if (wcschr(pValue, L'q'))
                {
                    dwCompress |= TEX_COMPRESS_BC7_QUICK;
                    found = true;
                }

                if (wcschr(pValue, L'x'))
                {
                    dwCompress |= TEX_COMPRESS_BC7_USE_3SUBSETS;
                    found = true;
                }

                if ((dwCompress & (TEX_COMPRESS_BC7_QUICK | TEX_COMPRESS_BC7_USE_3SUBSETS)) == (TEX_COMPRESS_BC7_QUICK | TEX_COMPRESS_BC7_USE_3SUBSETS))
                {
                    wprintf(L"Can't use -bc x (max) and -bc q (quick) at same time\n\n");
                    PrintUsage();
                    return 1;
                }

                if (!found)
                {
                    wprintf(L"Invalid value specified for -bc (%ls), missing d, u, q, or x\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
            }
            break;

            case OPT_WIC_QUALITY:
                if (swscanf_s(pValue, L"%f", &wicQuality) != 1
                    || (wicQuality < 0.f)
                    || (wicQuality > 1.f))
                {
                    wprintf(L"Invalid value specified with -wicq (%ls)\n", pValue);
                    printf("\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_COLORKEY:
                if (swscanf_s(pValue, L"%lx", &colorKey) != 1)
                {
                    printf("Invalid value specified with -c (%ls)\n", pValue);
                    printf("\n");
                    PrintUsage();
                    return 1;
                }
                colorKey &= 0xFFFFFF;
                break;

            case OPT_X2_BIAS:
                dwConvert |= TEX_FILTER_FLOAT_X2BIAS;
                break;

            case OPT_USE_DX10:
                if (dwOptions & (DWORD64(1) << OPT_USE_DX9))
                {
                    wprintf(L"Can't use -dx9 and -dx10 at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_USE_DX9:
                if (dwOptions & (DWORD64(1) << OPT_USE_DX10))
                {
                    wprintf(L"Can't use -dx9 and -dx10 at same time\n\n");
                    PrintUsage();
                    return 1;
                }
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

            case OPT_PAPER_WHITE_NITS:
                if (swscanf_s(pValue, L"%f", &paperWhiteNits) != 1)
                {
                    wprintf(L"Invalid value specified with -nits (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                else if (paperWhiteNits > 10000.f || paperWhiteNits <= 0.f)
                {
                    wprintf(L"-nits (%ls) parameter must be between 0 and 10000\n\n", pValue);
                    return 1;
                }
                break;

            case OPT_PRESERVE_ALPHA_COVERAGE:
                if (swscanf_s(pValue, L"%f", &preserveAlphaCoverageRef) != 1)
                {
                    wprintf(L"Invalid value specified with -keepcoverage (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                else if (preserveAlphaCoverageRef < 0.0f || preserveAlphaCoverageRef > 1.0f)
                {
                    wprintf(L"-keepcoverage (%ls) parameter must be between 0.0 and 1.0\n\n", pValue);
                    return 1;
                }
                break;
            }
        }
        else if (wcspbrk(pArg, L"?*") != nullptr)
        {
            size_t count = conversion.size();
            SearchForFiles(pArg, conversion, (dwOptions & (DWORD64(1) << OPT_RECURSIVE)) != 0);
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

            conv.szDest[0] = 0;

            conversion.push_back(conv);
        }
    }

    if (conversion.empty())
    {
        PrintUsage();
        return 0;
    }

    if (~dwOptions & (DWORD64(1) << OPT_NOLOGO))
        PrintLogo();

    // Work out out filename prefix and suffix
    if (szOutputDir[0] && (L'\\' != szOutputDir[wcslen(szOutputDir) - 1]))
        wcscat_s(szOutputDir, MAX_PATH, L"\\");

    if (szPrefix[0])
        wcscat_s(szOutputDir, MAX_PATH, szPrefix);

    wcscpy_s(szPrefix, MAX_PATH, szOutputDir);

    auto fileTypeName = LookupByValue(FileType, g_pSaveFileTypes);

    if (fileTypeName)
    {
        wcscat_s(szSuffix, MAX_PATH, L".");
        wcscat_s(szSuffix, MAX_PATH, fileTypeName);
    }
    else
    {
        wcscat_s(szSuffix, MAX_PATH, L".unknown");
    }

    if (FileType != CODEC_DDS)
    {
        mipLevels = 1;
    }

    LARGE_INTEGER qpcFreq;
    if (!QueryPerformanceFrequency(&qpcFreq))
    {
        qpcFreq.QuadPart = 0;
    }

    LARGE_INTEGER qpcStart;
    if (!QueryPerformanceCounter(&qpcStart))
    {
        qpcStart.QuadPart = 0;
    }

    // Convert images
    bool sizewarn = false;
    bool nonpow2warn = false;
    bool non4bc = false;
    bool preserveAlphaCoverage = false;
    ComPtr<ID3D11Device> pDevice;

    for (auto pConv = conversion.begin(); pConv != conversion.end(); ++pConv)
    {
        if (pConv != conversion.begin())
            wprintf(L"\n");

        // --- Load source image -------------------------------------------------------
        wprintf(L"reading %ls", pConv->szSrc);
        fflush(stdout);

        wchar_t ext[_MAX_EXT];
        wchar_t fname[_MAX_FNAME];
        _wsplitpath_s(pConv->szSrc, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT);

        TexMetadata info;
        std::unique_ptr<ScratchImage> image(new (std::nothrow) ScratchImage);

        if (!image)
        {
            wprintf(L"\nERROR: Memory allocation failed\n");
            return 1;
        }

        if (_wcsicmp(ext, L".dds") == 0)
        {
            DWORD ddsFlags = DDS_FLAGS_NONE;
            if (dwOptions & (DWORD64(1) << OPT_DDS_DWORD_ALIGN))
                ddsFlags |= DDS_FLAGS_LEGACY_DWORD;
            if (dwOptions & (DWORD64(1) << OPT_EXPAND_LUMINANCE))
                ddsFlags |= DDS_FLAGS_EXPAND_LUMINANCE;
            if (dwOptions & (DWORD64(1) << OPT_DDS_BAD_DXTN_TAILS))
                ddsFlags |= DDS_FLAGS_BAD_DXTN_TAILS;

            hr = LoadFromDDSFile(pConv->szSrc, ddsFlags, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                continue;
            }

            if (IsTypeless(info.format))
            {
                if (dwOptions & (DWORD64(1) << OPT_TYPELESS_UNORM))
                {
                    info.format = MakeTypelessUNORM(info.format);
                }
                else if (dwOptions & (DWORD64(1) << OPT_TYPELESS_FLOAT))
                {
                    info.format = MakeTypelessFLOAT(info.format);
                }

                if (IsTypeless(info.format))
                {
                    wprintf(L" FAILED due to Typeless format %d\n", info.format);
                    continue;
                }

                image->OverrideFormat(info.format);
            }
        }
        else if (_wcsicmp(ext, L".bmp") == 0)
        {
            std::unique_ptr<uint8_t[]> bmpData;
            size_t bmpSize;
            hr = ReadData(pConv->szSrc, bmpData, bmpSize);
            if (SUCCEEDED(hr))
            {
                hr = LoadFromWICMemory(bmpData.get(), bmpSize, dwFilter, &info, *image);
                if (FAILED(hr))
                {
                    if (SUCCEEDED(LoadFromExtendedBMPMemory(bmpData.get(), bmpSize, &info, *image)))
                    {
                        hr = S_OK;
                    }
                }
            }
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                continue;
            }
        }
        else if (_wcsicmp(ext, L".tga") == 0)
        {
            hr = LoadFromTGAFile(pConv->szSrc, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                continue;
            }
        }
        else if (_wcsicmp(ext, L".hdr") == 0)
        {
            hr = LoadFromHDRFile(pConv->szSrc, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                continue;
            }
        }
#ifdef USE_OPENEXR
        else if (_wcsicmp(ext, L".exr") == 0)
        {
            hr = LoadFromEXRFile(pConv->szSrc, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                continue;
            }
        }
#endif
        else
        {
            // WIC shares the same filter values for mode and dither
            static_assert(static_cast<int>(WIC_FLAGS_DITHER) == static_cast<int>(TEX_FILTER_DITHER), "WIC_FLAGS_* & TEX_FILTER_* should match");
            static_assert(static_cast<int>(WIC_FLAGS_DITHER_DIFFUSION) == static_cast<int>(TEX_FILTER_DITHER_DIFFUSION), "WIC_FLAGS_* & TEX_FILTER_* should match");
            static_assert(static_cast<int>(WIC_FLAGS_FILTER_POINT) == static_cast<int>(TEX_FILTER_POINT), "WIC_FLAGS_* & TEX_FILTER_* should match");
            static_assert(static_cast<int>(WIC_FLAGS_FILTER_LINEAR) == static_cast<int>(TEX_FILTER_LINEAR), "WIC_FLAGS_* & TEX_FILTER_* should match");
            static_assert(static_cast<int>(WIC_FLAGS_FILTER_CUBIC) == static_cast<int>(TEX_FILTER_CUBIC), "WIC_FLAGS_* & TEX_FILTER_* should match");
            static_assert(static_cast<int>(WIC_FLAGS_FILTER_FANT) == static_cast<int>(TEX_FILTER_FANT), "WIC_FLAGS_* & TEX_FILTER_* should match");

            DWORD wicFlags = dwFilter;
            if (FileType == CODEC_DDS)
                wicFlags |= WIC_FLAGS_ALL_FRAMES;

            hr = LoadFromWICFile(pConv->szSrc, wicFlags, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                continue;
            }
        }

        PrintInfo(info);

        size_t tMips = (!mipLevels && info.mipLevels > 1) ? info.mipLevels : mipLevels;

        // Convert texture
        wprintf(L" as");
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
                wprintf(L" FAILED [converttosingleplane] (%x)\n", static_cast<unsigned int>(hr));
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

        DXGI_FORMAT tformat = (format == DXGI_FORMAT_UNKNOWN) ? info.format : format;

        // --- Decompress --------------------------------------------------------------
        std::unique_ptr<ScratchImage> cimage;
        if (IsCompressed(info.format))
        {
            // Direct3D can only create BC resources with multiple-of-4 top levels
            if ((info.width % 4) != 0 || (info.height % 4) != 0)
            {
                if (dwOptions & (DWORD64(1) << OPT_BCNONMULT4FIX))
                {
                    std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                    if (!timage)
                    {
                        wprintf(L"\nERROR: Memory allocation failed\n");
                        return 1;
                    }

                    // If we started with < 4x4 then no need to generate mips
                    if (info.width < 4 && info.height < 4)
                    {
                        tMips = 1;
                    }

                    // Fix by changing size but also have to trim any mip-levels which can be invalid
                    TexMetadata mdata = image->GetMetadata();
                    mdata.width = (info.width + 3u) & ~0x3u;
                    mdata.height = (info.height + 3u) & ~0x3u;
                    mdata.mipLevels = 1;
                    hr = timage->Initialize(mdata);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED [BC non-multiple-of-4 fixup] (%x)\n", static_cast<unsigned int>(hr));
                        return 1;
                    }

                    if (mdata.dimension == TEX_DIMENSION_TEXTURE3D)
                    {
                        for (size_t d = 0; d < mdata.depth; ++d)
                        {
                            auto simg = image->GetImage(0, 0, d);
                            auto dimg = timage->GetImage(0, 0, d);

                            memcpy_s(dimg->pixels, dimg->slicePitch, simg->pixels, simg->slicePitch);
                        }
                    }
                    else
                    {
                        for (size_t i = 0; i < mdata.arraySize; ++i)
                        {
                            auto simg = image->GetImage(0, i, 0);
                            auto dimg = timage->GetImage(0, i, 0);

                            memcpy_s(dimg->pixels, dimg->slicePitch, simg->pixels, simg->slicePitch);
                        }
                    }

                    info.width = mdata.width;
                    info.height = mdata.height;
                    info.mipLevels = mdata.mipLevels;
                    image.swap(timage);
                }
                else if (IsCompressed(tformat))
                {
                    non4bc = true;
                }
            }

            auto img = image->GetImage(0, 0, 0);
            assert(img);
            size_t nimg = image->GetImageCount();

            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            hr = Decompress(img, nimg, info, DXGI_FORMAT_UNKNOWN /* picks good default */, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [decompress] (%x)\n", static_cast<unsigned int>(hr));
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

            if (FileType == CODEC_DDS)
            {
                // Keep the original compressed image in case we can reuse it
                cimage.reset(image.release());
                image.reset(timage.release());
            }
            else
            {
                image.swap(timage);
            }
        }

        // --- Undo Premultiplied Alpha (if requested) ---------------------------------
        if ((dwOptions & (DWORD64(1) << OPT_DEMUL_ALPHA))
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
                    wprintf(L" FAILED [demultiply alpha] (%x)\n", static_cast<unsigned int>(hr));
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
                cimage.reset();
            }
        }

        // --- Flip/Rotate -------------------------------------------------------------
        if (dwOptions & ((DWORD64(1) << OPT_HFLIP) | (DWORD64(1) << OPT_VFLIP)))
        {
            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            DWORD dwFlags = 0;

            if (dwOptions & (DWORD64(1) << OPT_HFLIP))
                dwFlags |= TEX_FR_FLIP_HORIZONTAL;

            if (dwOptions & (DWORD64(1) << OPT_VFLIP))
                dwFlags |= TEX_FR_FLIP_VERTICAL;

            assert(dwFlags != 0);

            hr = FlipRotate(image->GetImages(), image->GetImageCount(), image->GetMetadata(), dwFlags, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [fliprotate] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            auto& tinfo = timage->GetMetadata();

            info.width = tinfo.width;
            info.height = tinfo.height;

            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.mipLevels == tinfo.mipLevels);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.format == tinfo.format);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
            cimage.reset();
        }

        // --- Resize ------------------------------------------------------------------
        size_t twidth = (!width) ? info.width : width;
        if (twidth > maxSize)
        {
            if (!width)
                twidth = maxSize;
            else
                sizewarn = true;
        }

        size_t theight = (!height) ? info.height : height;
        if (theight > maxSize)
        {
            if (!height)
                theight = maxSize;
            else
                sizewarn = true;
        }

        if (dwOptions & (DWORD64(1) << OPT_FIT_POWEROF2))
        {
            FitPowerOf2(info.width, info.height, twidth, theight, maxSize);
        }

        if (info.width != twidth || info.height != theight)
        {
            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            hr = Resize(image->GetImages(), image->GetImageCount(), image->GetMetadata(), twidth, theight, dwFilter | dwFilterOpts, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [resize] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            auto& tinfo = timage->GetMetadata();

            assert(tinfo.width == twidth && tinfo.height == theight && tinfo.mipLevels == 1);
            info.width = tinfo.width;
            info.height = tinfo.height;
            info.mipLevels = 1;

            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.format == tinfo.format);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
            cimage.reset();
        }

        // --- Color rotation (if requested) -------------------------------------------
        if (dwRotateColor)
        {
            if (dwRotateColor == ROTATE_HDR10_TO_709)
            {
                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                hr = Convert(image->GetImages(), image->GetImageCount(), image->GetMetadata(), DXGI_FORMAT_R16G16B16A16_FLOAT,
                    dwFilter | dwFilterOpts | dwSRGB | dwConvert, alphaThreshold, *timage);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [convert] (%x)\n", static_cast<unsigned int>(hr));
                    return 1;
                }

#ifndef NDEBUG
                auto& tinfo = timage->GetMetadata();
#endif

                assert(tinfo.format == DXGI_FORMAT_R16G16B16A16_FLOAT);
                info.format = DXGI_FORMAT_R16G16B16A16_FLOAT;

                assert(info.width == tinfo.width);
                assert(info.height == tinfo.height);
                assert(info.depth == tinfo.depth);
                assert(info.arraySize == tinfo.arraySize);
                assert(info.mipLevels == tinfo.mipLevels);
                assert(info.miscFlags == tinfo.miscFlags);
                assert(info.dimension == tinfo.dimension);

                image.swap(timage);
                cimage.reset();
            }

            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            switch (dwRotateColor)
            {
            case ROTATE_709_TO_HDR10:
                hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                    {
                        UNREFERENCED_PARAMETER(y);

                        XMVECTOR paperWhite = XMVectorReplicate(paperWhiteNits);

                        for (size_t j = 0; j < w; ++j)
                        {
                            XMVECTOR value = inPixels[j];

                            XMVECTOR nvalue = XMVector3Transform(value, c_from709to2020);

                            // Convert to ST.2084
                            nvalue = XMVectorDivide(XMVectorMultiply(nvalue, paperWhite), c_MaxNitsFor2084);

                            XMFLOAT4A tmp;
                            XMStoreFloat4A(&tmp, nvalue);

                            tmp.x = LinearToST2084(tmp.x);
                            tmp.y = LinearToST2084(tmp.y);
                            tmp.z = LinearToST2084(tmp.z);

                            nvalue = XMLoadFloat4A(&tmp);

                            value = XMVectorSelect(value, nvalue, g_XMSelect1110);

                            outPixels[j] = value;
                        }
                    }, *timage);
                break;

            case ROTATE_709_TO_2020:
                hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                    {
                        UNREFERENCED_PARAMETER(y);

                        for (size_t j = 0; j < w; ++j)
                        {
                            XMVECTOR value = inPixels[j];

                            XMVECTOR nvalue = XMVector3Transform(value, c_from709to2020);

                            value = XMVectorSelect(value, nvalue, g_XMSelect1110);

                            outPixels[j] = value;
                        }
                    }, *timage);
                break;

            case ROTATE_HDR10_TO_709:
                hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                    {
                        UNREFERENCED_PARAMETER(y);

                        XMVECTOR paperWhite = XMVectorReplicate(paperWhiteNits);

                        for (size_t j = 0; j < w; ++j)
                        {
                            XMVECTOR value = inPixels[j];

                            // Convert from ST.2084
                            XMFLOAT4A tmp;
                            XMStoreFloat4A(&tmp, value);

                            tmp.x = ST2084ToLinear(tmp.x);
                            tmp.y = ST2084ToLinear(tmp.y);
                            tmp.z = ST2084ToLinear(tmp.z);

                            XMVECTOR nvalue = XMLoadFloat4A(&tmp);

                            nvalue = XMVectorDivide(XMVectorMultiply(nvalue, c_MaxNitsFor2084), paperWhite);

                            nvalue = XMVector3Transform(nvalue, c_from2020to709);

                            value = XMVectorSelect(value, nvalue, g_XMSelect1110);

                            outPixels[j] = value;
                        }
                    }, *timage);
                break;

            case ROTATE_2020_TO_709:
                hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                    {
                        UNREFERENCED_PARAMETER(y);

                        for (size_t j = 0; j < w; ++j)
                        {
                            XMVECTOR value = inPixels[j];

                            XMVECTOR nvalue = XMVector3Transform(value, c_from2020to709);

                            value = XMVectorSelect(value, nvalue, g_XMSelect1110);

                            outPixels[j] = value;
                        }
                    }, *timage);
                break;

            case ROTATE_P3_TO_HDR10:
                hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                    {
                        UNREFERENCED_PARAMETER(y);

                        XMVECTOR paperWhite = XMVectorReplicate(paperWhiteNits);

                        for (size_t j = 0; j < w; ++j)
                        {
                            XMVECTOR value = inPixels[j];

                            XMVECTOR nvalue = XMVector3Transform(value, c_fromP3to2020);

                            // Convert to ST.2084
                            nvalue = XMVectorDivide(XMVectorMultiply(nvalue, paperWhite), c_MaxNitsFor2084);

                            XMFLOAT4A tmp;
                            XMStoreFloat4A(&tmp, nvalue);

                            tmp.x = LinearToST2084(tmp.x);
                            tmp.y = LinearToST2084(tmp.y);
                            tmp.z = LinearToST2084(tmp.z);

                            nvalue = XMLoadFloat4A(&tmp);

                            value = XMVectorSelect(value, nvalue, g_XMSelect1110);

                            outPixels[j] = value;
                        }
                    }, *timage);
                break;

            case ROTATE_P3_TO_2020:
                hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                    {
                        UNREFERENCED_PARAMETER(y);

                        for (size_t j = 0; j < w; ++j)
                        {
                            XMVECTOR value = inPixels[j];

                            XMVECTOR nvalue = XMVector3Transform(value, c_fromP3to2020);

                            value = XMVectorSelect(value, nvalue, g_XMSelect1110);

                            outPixels[j] = value;
                        }
                    }, *timage);
                break;

            default:
                hr = E_NOTIMPL;
                break;
            }
            if (FAILED(hr))
            {
                wprintf(L" FAILED [rotate color apply] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

#ifndef NDEBUG
            auto& tinfo = timage->GetMetadata();
#endif

            assert(info.width == tinfo.width);
            assert(info.height == tinfo.height);
            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.mipLevels == tinfo.mipLevels);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.format == tinfo.format);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
            cimage.reset();
        }

        // --- Tonemap (if requested) --------------------------------------------------
        if (dwOptions & DWORD64(1) << OPT_TONEMAP)
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
                [&](const XMVECTOR* pixels, size_t w, size_t y)
                {
                    UNREFERENCED_PARAMETER(y);

                    for (size_t j = 0; j < w; ++j)
                    {
                        static const XMVECTORF32 s_luminance = { { { 0.3f, 0.59f, 0.11f, 0.f } } };

                        XMVECTOR v = *pixels++;

                        v = XMVector3Dot(v, s_luminance);

                        maxLum = XMVectorMax(v, maxLum);
                    }
                });
            if (FAILED(hr))
            {
                wprintf(L" FAILED [tonemap maxlum] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            // Reinhard et al, "Photographic Tone Reproduction for Digital Images"
            // http://www.cs.utah.edu/~reinhard/cdrom/
            maxLum = XMVectorMultiply(maxLum, maxLum);

            hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                {
                    UNREFERENCED_PARAMETER(y);

                    for (size_t j = 0; j < w; ++j)
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
                wprintf(L" FAILED [tonemap apply] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

#ifndef NDEBUG
            auto& tinfo = timage->GetMetadata();
#endif

            assert(info.width == tinfo.width);
            assert(info.height == tinfo.height);
            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.mipLevels == tinfo.mipLevels);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.format == tinfo.format);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
            cimage.reset();
        }

        // --- Convert -----------------------------------------------------------------
        if (dwOptions & (DWORD64(1) << OPT_NORMAL_MAP))
        {
            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            DXGI_FORMAT nmfmt = tformat;
            if (IsCompressed(tformat))
            {
                switch (tformat)
                {
                case DXGI_FORMAT_BC4_SNORM:
                case DXGI_FORMAT_BC5_SNORM:
                    nmfmt = DXGI_FORMAT_R8G8B8A8_SNORM;
                    break;

                case DXGI_FORMAT_BC6H_SF16:
                case DXGI_FORMAT_BC6H_UF16:
                    nmfmt = DXGI_FORMAT_R32G32B32_FLOAT;
                    break;

                default:
                    nmfmt = DXGI_FORMAT_R8G8B8A8_UNORM;
                    break;
                }
            }

            hr = ComputeNormalMap(image->GetImages(), image->GetImageCount(), image->GetMetadata(), dwNormalMap, nmapAmplitude, nmfmt, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [normalmap] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            auto& tinfo = timage->GetMetadata();

            assert(tinfo.format == nmfmt);
            info.format = tinfo.format;

            assert(info.width == tinfo.width);
            assert(info.height == tinfo.height);
            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.mipLevels == tinfo.mipLevels);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
            cimage.reset();
        }
        else if (info.format != tformat && !IsCompressed(tformat))
        {
            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            hr = Convert(image->GetImages(), image->GetImageCount(), image->GetMetadata(), tformat,
                dwFilter | dwFilterOpts | dwSRGB | dwConvert, alphaThreshold, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [convert] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            auto& tinfo = timage->GetMetadata();

            assert(tinfo.format == tformat);
            info.format = tinfo.format;

            assert(info.width == tinfo.width);
            assert(info.height == tinfo.height);
            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.mipLevels == tinfo.mipLevels);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
            cimage.reset();
        }

        // --- ColorKey/ChromaKey ------------------------------------------------------
        if ((dwOptions & (DWORD64(1) << OPT_COLORKEY))
            && HasAlpha(info.format))
        {
            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            XMVECTOR colorKeyValue = XMLoadColor(reinterpret_cast<const XMCOLOR*>(&colorKey));

            hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                {
                    static const XMVECTORF32 s_tolerance = { { { 0.2f, 0.2f, 0.2f, 0.f } } };

                    UNREFERENCED_PARAMETER(y);

                    for (size_t j = 0; j < w; ++j)
                    {
                        XMVECTOR value = inPixels[j];

                        if (XMVector3NearEqual(value, colorKeyValue, s_tolerance))
                        {
                            value = g_XMZero;
                        }
                        else
                        {
                            value = XMVectorSelect(g_XMOne, value, g_XMSelect1110);
                        }

                        outPixels[j] = value;
                    }
                }, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [colorkey] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

#ifndef NDEBUG
            auto& tinfo = timage->GetMetadata();
#endif

            assert(info.width == tinfo.width);
            assert(info.height == tinfo.height);
            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.mipLevels == tinfo.mipLevels);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.format == tinfo.format);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
            cimage.reset();
        }

        // --- Invert Y Channel --------------------------------------------------------
        if (dwOptions & (DWORD64(1) << OPT_INVERT_Y))
        {
            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                {
                    static const XMVECTORU32 s_selecty = { { { XM_SELECT_0, XM_SELECT_1, XM_SELECT_0, XM_SELECT_0 } } };

                    UNREFERENCED_PARAMETER(y);

                    for (size_t j = 0; j < w; ++j)
                    {
                        XMVECTOR value = inPixels[j];

                        XMVECTOR inverty = XMVectorSubtract(g_XMOne, value);

                        outPixels[j] = XMVectorSelect(value, inverty, s_selecty);
                    }
                }, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [inverty] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

#ifndef NDEBUG
            auto& tinfo = timage->GetMetadata();
#endif

            assert(info.width == tinfo.width);
            assert(info.height == tinfo.height);
            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.mipLevels == tinfo.mipLevels);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.format == tinfo.format);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
            cimage.reset();
        }

        // --- Determine whether preserve alpha coverage is required (if requested) ----
        if (preserveAlphaCoverageRef > 0.0f && HasAlpha(info.format) && !image->IsAlphaAllOpaque())
        {
            preserveAlphaCoverage = true;
        }

        // --- Generate mips -----------------------------------------------------------
        DWORD dwFilter3D = dwFilter;
        if (!ispow2(info.width) || !ispow2(info.height) || !ispow2(info.depth))
        {
            if (!tMips || info.mipLevels != 1)
            {
                nonpow2warn = true;
            }

            if (info.dimension == TEX_DIMENSION_TEXTURE3D)
            {
                // Must force triangle filter for non-power-of-2 volume textures to get correct results
                dwFilter3D = TEX_FILTER_TRIANGLE;
            }
        }

        if ((!tMips || info.mipLevels != tMips || preserveAlphaCoverage) && (info.mipLevels != 1))
        {
            // Mips generation only works on a single base image, so strip off existing mip levels
            // Also required for preserve alpha coverage so that existing mips are regenerated

            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            TexMetadata mdata = info;
            mdata.mipLevels = 1;
            hr = timage->Initialize(mdata);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [copy to single level] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            if (info.dimension == TEX_DIMENSION_TEXTURE3D)
            {
                for (size_t d = 0; d < info.depth; ++d)
                {
                    hr = CopyRectangle(*image->GetImage(0, 0, d), Rect(0, 0, info.width, info.height),
                        *timage->GetImage(0, 0, d), TEX_FILTER_DEFAULT, 0, 0);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED [copy to single level] (%x)\n", static_cast<unsigned int>(hr));
                        return 1;
                    }
                }
            }
            else
            {
                for (size_t i = 0; i < info.arraySize; ++i)
                {
                    hr = CopyRectangle(*image->GetImage(0, i, 0), Rect(0, 0, info.width, info.height),
                        *timage->GetImage(0, i, 0), TEX_FILTER_DEFAULT, 0, 0);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED [copy to single level] (%x)\n", static_cast<unsigned int>(hr));
                        return 1;
                    }
                }
            }

            image.swap(timage);
            info.mipLevels = image->GetMetadata().mipLevels;

            if (cimage && (tMips == 1))
            {
                // Special case for trimming mips off compressed images and keeping the original compressed highest level mip
                mdata = cimage->GetMetadata();
                mdata.mipLevels = 1;
                hr = timage->Initialize(mdata);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [copy compressed to single level] (%x)\n", static_cast<unsigned int>(hr));
                    return 1;
                }

                if (mdata.dimension == TEX_DIMENSION_TEXTURE3D)
                {
                    for (size_t d = 0; d < mdata.depth; ++d)
                    {
                        auto simg = cimage->GetImage(0, 0, d);
                        auto dimg = timage->GetImage(0, 0, d);

                        memcpy_s(dimg->pixels, dimg->slicePitch, simg->pixels, simg->slicePitch);
                    }
                }
                else
                {
                    for (size_t i = 0; i < mdata.arraySize; ++i)
                    {
                        auto simg = cimage->GetImage(0, i, 0);
                        auto dimg = timage->GetImage(0, i, 0);

                        memcpy_s(dimg->pixels, dimg->slicePitch, simg->pixels, simg->slicePitch);
                    }
                }

                cimage.swap(timage);
            }
            else
            {
                cimage.reset();
            }
        }

        if ((!tMips || info.mipLevels != tMips) && (info.width > 1 || info.height > 1 || info.depth > 1))
        {
            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            if (info.dimension == TEX_DIMENSION_TEXTURE3D)
            {
                hr = GenerateMipMaps3D(image->GetImages(), image->GetImageCount(), image->GetMetadata(), dwFilter3D | dwFilterOpts, tMips, *timage);
            }
            else
            {
                hr = GenerateMipMaps(image->GetImages(), image->GetImageCount(), image->GetMetadata(), dwFilter | dwFilterOpts, tMips, *timage);
            }
            if (FAILED(hr))
            {
                wprintf(L" FAILED [mipmaps] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            auto& tinfo = timage->GetMetadata();
            info.mipLevels = tinfo.mipLevels;

            assert(info.width == tinfo.width);
            assert(info.height == tinfo.height);
            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.format == tinfo.format);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
            cimage.reset();
        }

        // --- Preserve mipmap alpha coverage (if requested) ---------------------------
        if (preserveAlphaCoverage && info.mipLevels != 1 && (info.dimension != TEX_DIMENSION_TEXTURE3D))
        {
            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            hr = timage->Initialize(image->GetMetadata());
            if (FAILED(hr))
            {
                wprintf(L" FAILED [keepcoverage] (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            const size_t items = image->GetMetadata().arraySize;
            for (size_t item = 0; item < items; ++item)
            {
                auto img = image->GetImage(0, item, 0);
                assert(img);

                hr = ScaleMipMapsAlphaForCoverage(img, info.mipLevels, info, item, preserveAlphaCoverageRef, *timage);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [keepcoverage] (%x)\n", static_cast<unsigned int>(hr));
                    return 1;
                }
            }

#ifndef NDEBUG
            auto& tinfo = timage->GetMetadata();
#endif

            assert(info.width == tinfo.width);
            assert(info.height == tinfo.height);
            assert(info.depth == tinfo.depth);
            assert(info.arraySize == tinfo.arraySize);
            assert(info.mipLevels == tinfo.mipLevels);
            assert(info.miscFlags == tinfo.miscFlags);
            assert(info.dimension == tinfo.dimension);

            image.swap(timage);
            cimage.reset();
        }

        // --- Premultiplied alpha (if requested) --------------------------------------
        if ((dwOptions & (DWORD64(1) << OPT_PREMUL_ALPHA))
            && HasAlpha(info.format)
            && info.format != DXGI_FORMAT_A8_UNORM)
        {
            if (info.IsPMAlpha())
            {
                printf("\nWARNING: Image is already using premultiplied alpha\n");
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

                hr = PremultiplyAlpha(img, nimg, info, dwSRGB, *timage);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [premultiply alpha] (%x)\n", static_cast<unsigned int>(hr));
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
                cimage.reset();
            }
        }

        // --- Compress ----------------------------------------------------------------
        if (IsCompressed(tformat) && (FileType == CODEC_DDS))
        {
            if (cimage && (cimage->GetMetadata().format == tformat))
            {
                // We never changed the image and it was already compressed in our desired format, use original data
                image.reset(cimage.release());

                auto& tinfo = image->GetMetadata();

                if ((tinfo.width % 4) != 0 || (tinfo.height % 4) != 0)
                {
                    non4bc = true;
                }

                info.format = tinfo.format;
                assert(info.width == tinfo.width);
                assert(info.height == tinfo.height);
                assert(info.depth == tinfo.depth);
                assert(info.arraySize == tinfo.arraySize);
                assert(info.mipLevels == tinfo.mipLevels);
                assert(info.miscFlags == tinfo.miscFlags);
                assert(info.dimension == tinfo.dimension);
            }
            else
            {
                cimage.reset();

                auto img = image->GetImage(0, 0, 0);
                assert(img);
                size_t nimg = image->GetImageCount();

                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                bool bc6hbc7 = false;
                switch (tformat)
                {
                case DXGI_FORMAT_BC6H_TYPELESS:
                case DXGI_FORMAT_BC6H_UF16:
                case DXGI_FORMAT_BC6H_SF16:
                case DXGI_FORMAT_BC7_TYPELESS:
                case DXGI_FORMAT_BC7_UNORM:
                case DXGI_FORMAT_BC7_UNORM_SRGB:
                    bc6hbc7 = true;

                    {
                        static bool s_tryonce = false;

                        if (!s_tryonce)
                        {
                            s_tryonce = true;

                            if (!(dwOptions & (DWORD64(1) << OPT_NOGPU)))
                            {
                                if (!CreateDevice(adapter, pDevice.GetAddressOf()))
                                    wprintf(L"\nWARNING: DirectCompute is not available, using BC6H / BC7 CPU codec\n");
                            }
                            else
                            {
                                wprintf(L"\nWARNING: using BC6H / BC7 CPU codec\n");
                            }
                        }
                    }
                    break;

                default:
                    break;
                }

                DWORD cflags = dwCompress;
#ifdef _OPENMP
                if (!(dwOptions & (DWORD64(1) << OPT_FORCE_SINGLEPROC)))
                {
                    cflags |= TEX_COMPRESS_PARALLEL;
                }
#endif

                if ((img->width % 4) != 0 || (img->height % 4) != 0)
                {
                    non4bc = true;
                }

                if (bc6hbc7 && pDevice)
                {
                    hr = Compress(pDevice.Get(), img, nimg, info, tformat, dwCompress | dwSRGB, alphaWeight, *timage);
                }
                else
                {
                    hr = Compress(img, nimg, info, tformat, cflags | dwSRGB, alphaThreshold, *timage);
                }
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [compress] (%x)\n", static_cast<unsigned int>(hr));
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
        }
        else
        {
            cimage.reset();
        }

        // --- Set alpha mode ----------------------------------------------------------
        if (HasAlpha(info.format)
            && info.format != DXGI_FORMAT_A8_UNORM)
        {
            if (image->IsAlphaAllOpaque())
            {
                info.SetAlphaMode(TEX_ALPHA_MODE_OPAQUE);
            }
            else if (info.IsPMAlpha())
            {
                // Aleady set TEX_ALPHA_MODE_PREMULTIPLIED
            }
            else if (dwOptions & (DWORD64(1) << OPT_SEPALPHA))
            {
                info.SetAlphaMode(TEX_ALPHA_MODE_CUSTOM);
            }
            else if (info.GetAlphaMode() == TEX_ALPHA_MODE_UNKNOWN)
            {
                info.SetAlphaMode(TEX_ALPHA_MODE_STRAIGHT);
            }
        }
        else
        {
            info.SetAlphaMode(TEX_ALPHA_MODE_UNKNOWN);
        }

        // --- Save result -------------------------------------------------------------
        {
            auto img = image->GetImage(0, 0, 0);
            assert(img);
            size_t nimg = image->GetImageCount();

            PrintInfo(info);
            wprintf(L"\n");

            // Figure out dest filename
            wchar_t *pchSlash, *pchDot;

            wcscpy_s(pConv->szDest, MAX_PATH, szPrefix);

            pchSlash = wcsrchr(pConv->szSrc, L'\\');
            if (pchSlash)
                wcscat_s(pConv->szDest, MAX_PATH, pchSlash + 1);
            else
                wcscat_s(pConv->szDest, MAX_PATH, pConv->szSrc);

            pchSlash = wcsrchr(pConv->szDest, '\\');
            pchDot = wcsrchr(pConv->szDest, '.');

            if (pchDot > pchSlash)
                *pchDot = 0;

            wcscat_s(pConv->szDest, MAX_PATH, szSuffix);

            if (dwOptions & (DWORD64(1) << OPT_TOLOWER))
            {
                (void)_wcslwr_s(pConv->szDest);
            }

            // Write texture
            wprintf(L"writing %ls", pConv->szDest);
            fflush(stdout);

            if (~dwOptions & (DWORD64(1) << OPT_OVERWRITE))
            {
                if (GetFileAttributesW(pConv->szDest) != INVALID_FILE_ATTRIBUTES)
                {
                    wprintf(L"\nERROR: Output file already exists, use -y to overwrite:\n");
                    continue;
                }
            }

            switch (FileType)
            {
            case CODEC_DDS:
            {
                DWORD ddsFlags = DDS_FLAGS_NONE;
                if (dwOptions & (DWORD64(1) << OPT_USE_DX10))
                {
                    ddsFlags |= DDS_FLAGS_FORCE_DX10_EXT | DDS_FLAGS_FORCE_DX10_EXT_MISC2;
                }
                else if (dwOptions & (DWORD64(1) << OPT_USE_DX9))
                {
                    ddsFlags |= DDS_FLAGS_FORCE_DX9_LEGACY;
                }

                hr = SaveToDDSFile(img, nimg, info, ddsFlags, pConv->szDest);
                break;
            }

            case CODEC_TGA:
                hr = SaveToTGAFile(img[0], pConv->szDest, (dwOptions & (DWORD64(1) << OPT_TGA20)) ? &info : nullptr);
                break;

            case CODEC_HDR:
                hr = SaveToHDRFile(img[0], pConv->szDest);
                break;

#ifdef USE_OPENEXR
            case CODEC_EXR:
                hr = SaveToEXRFile(img[0], pConv->szDest);
                break;
#endif

            default:
            {
                WICCodecs codec = (FileType == CODEC_HDP || FileType == CODEC_JXR) ? WIC_CODEC_WMP : static_cast<WICCodecs>(FileType);
                size_t nimages = (dwOptions & (DWORD64(1) << OPT_WIC_MULTIFRAME)) ? nimg : 1;
                hr = SaveToWICFile(img, nimages, WIC_FLAGS_NONE, GetWICCodec(codec), pConv->szDest, nullptr,
                    [&](IPropertyBag2* props)
                    {
                        bool wicLossless = (dwOptions & (DWORD64(1) << OPT_WIC_LOSSLESS)) != 0;

                        switch (FileType)
                        {
                        case WIC_CODEC_JPEG:
                            if (wicLossless || wicQuality >= 0.f)
                            {
                                PROPBAG2 options = {};
                                VARIANT varValues = {};
                                options.pstrName = const_cast<wchar_t*>(L"ImageQuality");
                                varValues.vt = VT_R4;
                                varValues.fltVal = (wicLossless) ? 1.f : wicQuality;
                                (void)props->Write(1, &options, &varValues);
                            }
                            break;

                        case WIC_CODEC_TIFF:
                        {
                            PROPBAG2 options = {};
                            VARIANT varValues = {};
                            if (wicLossless)
                            {
                                options.pstrName = const_cast<wchar_t*>(L"TiffCompressionMethod");
                                varValues.vt = VT_UI1;
                                varValues.bVal = WICTiffCompressionNone;
                            }
                            else if (wicQuality >= 0.f)
                            {
                                options.pstrName = const_cast<wchar_t*>(L"CompressionQuality");
                                varValues.vt = VT_R4;
                                varValues.fltVal = wicQuality;
                            }
                            (void)props->Write(1, &options, &varValues);
                        }
                        break;

                        case WIC_CODEC_WMP:
                        case CODEC_HDP:
                        case CODEC_JXR:
                        {
                            PROPBAG2 options = {};
                            VARIANT varValues = {};
                            if (wicLossless)
                            {
                                options.pstrName = const_cast<wchar_t*>(L"Lossless");
                                varValues.vt = VT_BOOL;
                                varValues.bVal = TRUE;
                            }
                            else if (wicQuality >= 0.f)
                            {
                                options.pstrName = const_cast<wchar_t*>(L"ImageQuality");
                                varValues.vt = VT_R4;
                                varValues.fltVal = wicQuality;
                            }
                            (void)props->Write(1, &options, &varValues);
                        }
                        break;
                        }
                    });
            }
            break;
            }

            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                continue;
            }
            wprintf(L"\n");
        }
    }

    if (sizewarn)
    {
        wprintf(L"\nWARNING: Target size exceeds maximum size for feature level (%lu)\n", maxSize);
    }

    if (nonpow2warn && maxSize <= 4096)
    {
        // Only emit this warning if ran with -fl set to a 9.x feature level
        wprintf(L"\nWARNING: Not all feature levels support non-power-of-2 textures with mipmaps\n");
    }

    if (non4bc)
        wprintf(L"\nWARNING: Direct3D requires BC image to be multiple of 4 in width & height\n");

    if (dwOptions & (DWORD64(1) << OPT_TIMING))
    {
        LARGE_INTEGER qpcEnd;
        if (QueryPerformanceCounter(&qpcEnd))
        {
            LONGLONG delta = qpcEnd.QuadPart - qpcStart.QuadPart;
            wprintf(L"\n Processing time: %f seconds\n", double(delta) / double(qpcFreq.QuadPart));
        }
    }

    return 0;
}
