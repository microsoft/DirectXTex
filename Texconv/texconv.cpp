//--------------------------------------------------------------------------------------
// File: TexConv.cpp
//
// DirectX Texture Converter
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
#define NOMCX
#define NOSERVICE
#define NOHELP
#ifdef  _MSC_VER
#pragma warning(pop)
#endif

#include <ShlObj.h>

#if __cplusplus < 201703L
#error Requires C++17 (and /Zc:__cplusplus with MSVC)
#endif

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <list>
#include <locale>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <tuple>

#include <wrl\client.h>

#include <d3d11.h>
#include <dxgi.h>
#include <dxgiformat.h>

#include <wincodec.h>

#ifdef  _MSC_VER
#pragma warning(disable : 4619 4616 26812)
#endif

#include "DirectXTex.h"
#include "DirectXPackedVector.h"

#ifdef USE_OPENEXR
// See <https://github.com/Microsoft/DirectXTex/wiki/Adding-OpenEXR> for details
#include "DirectXTexEXR.h"
#endif

// See <https://github.com/Microsoft/DirectXTex/wiki/Using-JPEG-PNG-OSS> for details
#ifdef USE_LIBJPEG
#include "DirectXTexJPEG.h"
#endif
#ifdef USE_LIBPNG
#include "DirectXTexPNG.h"
#endif

#ifdef USE_XBOX_EXTS
// See <https://github.com/microsoft/DirectXTex/wiki/DirectXTexXbox> for details
#include "DirectXTexXbox.h"
#endif

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

namespace
{
    enum OPTIONS : uint64_t
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
        OPT_DDS_PERMISSIVE,
        OPT_USE_DX10,
        OPT_USE_DX9,
        OPT_TGA20,
        OPT_TGAZEROALPHA,
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
        OPT_RECONSTRUCT_Z,
        OPT_ROTATE_COLOR,
        OPT_PAPER_WHITE_NITS,
        OPT_BCNONMULT4FIX,
        OPT_SWIZZLE,
    #ifdef USE_XBOX_EXTS
        OPT_USE_XBOX,
        OPT_XGMODE,
    #endif
        OPT_MAX
    };

    enum
    {
        ROTATE_709_TO_HDR10 = 1,
        ROTATE_HDR10_TO_709,
        ROTATE_709_TO_2020,
        ROTATE_2020_TO_709,
        ROTATE_P3D65_TO_HDR10,
        ROTATE_P3D65_TO_2020,
        ROTATE_709_TO_P3D65,
        ROTATE_P3D65_TO_709,
    };

    enum
    {
        FORMAT_DXT5_NM = 1,
        FORMAT_DXT5_RXGB,
    };

    static_assert(OPT_MAX <= 64, "dwOptions is a unsigned int bitfield");

    struct SConversion
    {
        std::wstring szSrc;
        std::wstring szFolder;
    };

    template<typename T>
    struct SValue
    {
        const wchar_t*  name;
        T               value;
    };

    const SValue<uint64_t> g_pOptions[] =
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
        { L"permissive",    OPT_DDS_PERMISSIVE },
        { L"dx10",          OPT_USE_DX10 },
        { L"dx9",           OPT_USE_DX9 },
        { L"tga20",         OPT_TGA20 },
        { L"tgazeroalpha",  OPT_TGAZEROALPHA },
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
        { L"reconstructz",  OPT_RECONSTRUCT_Z },
        { L"rotatecolor",   OPT_ROTATE_COLOR },
        { L"nits",          OPT_PAPER_WHITE_NITS },
        { L"fixbc4x4",      OPT_BCNONMULT4FIX },
        { L"swizzle",       OPT_SWIZZLE },
    #ifdef USE_XBOX_EXTS
        { L"xbox",          OPT_USE_XBOX },
        { L"xgmode",        OPT_XGMODE },
    #endif
        { nullptr,          0 }
    };

#define DEFFMT(fmt) { L## #fmt, DXGI_FORMAT_ ## fmt }

    const SValue<uint32_t> g_pFormats[] =
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

        // D3D11on12 format
        { L"A4B4G4R4_UNORM", DXGI_FORMAT(191) },

    #ifdef USE_XBOX_EXTS
        // Xbox One extended formats
        { L"R10G10B10_7E3_A2_FLOAT", DXGI_FORMAT(116) },
        { L"R10G10B10_6E4_A2_FLOAT", DXGI_FORMAT(117) },
        { L"R10G10B10_SNORM_A2_UNORM", DXGI_FORMAT(189) },
        { L"R4G4_UNORM", DXGI_FORMAT(190) },
    #endif

        { nullptr, DXGI_FORMAT_UNKNOWN }
    };

    const SValue<uint32_t> g_pFormatAliases[] =
    {
        { L"DXT1", DXGI_FORMAT_BC1_UNORM },
        { L"DXT2", DXGI_FORMAT_BC2_UNORM },
        { L"DXT3", DXGI_FORMAT_BC2_UNORM },
        { L"DXT4", DXGI_FORMAT_BC3_UNORM },
        { L"DXT5", DXGI_FORMAT_BC3_UNORM },

        { L"RGBA", DXGI_FORMAT_R8G8B8A8_UNORM },
        { L"BGRA", DXGI_FORMAT_B8G8R8A8_UNORM },
        { L"BGR",  DXGI_FORMAT_B8G8R8X8_UNORM },

        { L"FP16", DXGI_FORMAT_R16G16B16A16_FLOAT },
        { L"FP32", DXGI_FORMAT_R32G32B32A32_FLOAT },

        { L"BPTC", DXGI_FORMAT_BC7_UNORM },
        { L"BPTC_FLOAT", DXGI_FORMAT_BC6H_UF16 },

        { nullptr, DXGI_FORMAT_UNKNOWN }
    };

    const SValue<uint32_t> g_pSpecialFormats[] =
    {
        { L"BC3n", FORMAT_DXT5_NM },
        { L"DXT5nm", FORMAT_DXT5_NM },
        { L"RXGB", FORMAT_DXT5_RXGB },

        { nullptr, 0 }
    };

    const SValue<uint32_t> g_pReadOnlyFormats[] =
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

    #ifdef USE_XBOX_EXTS
        // Xbox One extended formats
        { L"D16_UNORM_S8_UINT", DXGI_FORMAT(118) },
        { L"R16_UNORM_X8_TYPELESS", DXGI_FORMAT(119) },
        { L"X16_TYPELESS_G8_UINT", DXGI_FORMAT(120) },
    #endif

        { nullptr, DXGI_FORMAT_UNKNOWN }
    };

    const SValue<uint32_t> g_pFilters[] =
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

    const SValue<uint32_t> g_pRotateColor[] =
    {
        { L"709to2020",     ROTATE_709_TO_2020 },
        { L"2020to709",     ROTATE_2020_TO_709 },
        { L"709toHDR10",    ROTATE_709_TO_HDR10 },
        { L"HDR10to709",    ROTATE_HDR10_TO_709 },
        { L"P3D65to2020",   ROTATE_P3D65_TO_2020 },
        { L"P3D65toHDR10",  ROTATE_P3D65_TO_HDR10 },
        { L"709toP3D65",    ROTATE_709_TO_P3D65 },
        { L"P3D65to709",    ROTATE_P3D65_TO_709 },
        { nullptr, 0 },
    };

#define CODEC_DDS 0xFFFF0001
#define CODEC_TGA 0xFFFF0002
#define CODEC_HDP 0xFFFF0003
#define CODEC_JXR 0xFFFF0004
#define CODEC_HDR 0xFFFF0005
#define CODEC_PPM 0xFFFF0006
#define CODEC_PFM 0xFFFF0007

#ifdef USE_OPENEXR
#define CODEC_EXR 0xFFFF0008
#endif
#ifdef USE_LIBJPEG
#define CODEC_JPEG 0xFFFF0009
#endif
#ifdef USE_LIBPNG
#define CODEC_PNG 0xFFFF000A
#endif

    const SValue<uint32_t> g_pSaveFileTypes[] =   // valid formats to write to
    {
        { L"bmp",   WIC_CODEC_BMP  },
#ifdef USE_LIBJPEG
        { L"jpg",   CODEC_JPEG     },
        { L"jpeg",  CODEC_JPEG     },
#else
        { L"jpg",   WIC_CODEC_JPEG },
        { L"jpeg",  WIC_CODEC_JPEG },
#endif
#ifdef USE_LIBPNG
        { L"png",   CODEC_PNG      },
#else
        { L"png",   WIC_CODEC_PNG  },
#endif
        { L"dds",   CODEC_DDS      },
        { L"ddx",   CODEC_DDS      },
        { L"tga",   CODEC_TGA      },
        { L"hdr",   CODEC_HDR      },
        { L"tif",   WIC_CODEC_TIFF },
        { L"tiff",  WIC_CODEC_TIFF },
        { L"wdp",   WIC_CODEC_WMP  },
        { L"hdp",   CODEC_HDP      },
        { L"jxr",   CODEC_JXR      },
        { L"ppm",   CODEC_PPM      },
        { L"pfm",   CODEC_PFM      },
    #ifdef USE_OPENEXR
        { L"exr",   CODEC_EXR      },
    #endif
        { L"heic",  WIC_CODEC_HEIF },
        { L"heif",  WIC_CODEC_HEIF },
        { nullptr,  CODEC_DDS      }
    };

    const SValue<uint32_t> g_pFeatureLevels[] =   // valid feature levels for -fl for maximimum size
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
        { L"12.2", 16384 },
        { nullptr, 0 },
    };
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

HRESULT __cdecl LoadFromBMPEx(
    _In_z_ const wchar_t* szFile,
    _In_ WIC_FLAGS flags,
    _Out_opt_ TexMetadata* metadata,
    _Out_ ScratchImage& image) noexcept;

HRESULT __cdecl LoadFromPortablePixMap(
    _In_z_ const wchar_t* szFile,
    _Out_opt_ TexMetadata* metadata,
    _Out_ ScratchImage& image) noexcept;

HRESULT __cdecl SaveToPortablePixMap(
    _In_ const Image& image,
    _In_z_ const wchar_t* szFile) noexcept;

HRESULT __cdecl LoadFromPortablePixMapHDR(
    _In_z_ const wchar_t* szFile,
    _Out_opt_ TexMetadata* metadata,
    _Out_ ScratchImage& image) noexcept;

HRESULT __cdecl SaveToPortablePixMapHDR(
    _In_ const Image& image,
    _In_z_ const wchar_t* szFile) noexcept;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#ifdef  _MSC_VER
#pragma warning( disable : 4616 6211 )
#endif

namespace
{
    inline HANDLE safe_handle(HANDLE h) noexcept { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

    struct find_closer { void operator()(HANDLE h) noexcept { assert(h != INVALID_HANDLE_VALUE); if (h) FindClose(h); } };

    using ScopedFindHandle = std::unique_ptr<void, find_closer>;

    constexpr static bool ispow2(size_t x)
    {
        return ((x != 0) && !(x & (x - 1)));
    }

#ifdef _PREFAST_
#pragma prefast(disable : 26018, "Only used with static internal arrays")
#endif

    template<typename T>
    T LookupByName(const wchar_t *pName, const SValue<T> *pArray)
    {
        while (pArray->name)
        {
            if (!_wcsicmp(pName, pArray->name))
                return pArray->value;

            pArray++;
        }

        return 0;
    }

    template<typename T>
    const wchar_t* LookupByValue(T value, const SValue<T> *pArray)
    {
        while (pArray->name)
        {
            if (value == pArray->value)
                return pArray->name;

            pArray++;
        }

        return L"";
    }

    void SearchForFiles(const std::filesystem::path& path, std::list<SConversion>& files, bool recursive, const wchar_t* folder)
    {
        // Process files
        WIN32_FIND_DATAW findData = {};
        ScopedFindHandle hFile(safe_handle(FindFirstFileExW(path.c_str(),
            FindExInfoBasic, &findData,
            FindExSearchNameMatch, nullptr,
            FIND_FIRST_EX_LARGE_FETCH)));
        if (hFile)
        {
            for (;;)
            {
                if (!(findData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY)))
                {
                    SConversion conv = {};
                    conv.szSrc = path.parent_path().append(findData.cFileName).native();
                    if (folder)
                    {
                        conv.szFolder = folder;
                    }
                    files.push_back(conv);
                }

                if (!FindNextFileW(hFile.get(), &findData))
                    break;
            }
        }

        // Process directories
        if (recursive)
        {
            auto searchDir = path.parent_path().append(L"*");

            hFile.reset(safe_handle(FindFirstFileExW(searchDir.c_str(),
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
                        auto subfolder = (folder)
                            ? (std::wstring(folder) + std::wstring(findData.cFileName) + std::filesystem::path::preferred_separator)
                            : (std::wstring(findData.cFileName) + std::filesystem::path::preferred_separator);

                        auto subdir = path.parent_path().append(findData.cFileName).append(path.filename().c_str());

                        SearchForFiles(subdir, files, recursive, subfolder.c_str());
                    }
                }

                if (!FindNextFileW(hFile.get(), &findData))
                    break;
            }
        }
    }

    void ProcessFileList(std::wifstream& inFile, std::list<SConversion>& files)
    {
        std::list<SConversion> flist;
        std::set<std::wstring> excludes;

        for (;;)
        {
            std::wstring fname;
            std::getline(inFile, fname);
            if (!inFile)
                break;

            if (fname[0] == L'#')
            {
                // Comment
            }
            else if (fname[0] == L'-')
            {
                if (flist.empty())
                {
                    wprintf(L"WARNING: Ignoring the line '%ls' in -flist\n", fname.c_str());
                }
                else
                {
                    std::filesystem::path path(fname.c_str() + 1);
                    auto& npath = path.make_preferred();
                    if (wcspbrk(fname.c_str(), L"?*") != nullptr)
                    {
                        std::list<SConversion> removeFiles;
                        SearchForFiles(npath, removeFiles, false, nullptr);

                        for (auto& it : removeFiles)
                        {
                            std::wstring name = it.szSrc;
                            std::transform(name.begin(), name.end(), name.begin(), towlower);
                            excludes.insert(name);
                        }
                    }
                    else
                    {
                        std::wstring name = npath.c_str();
                        std::transform(name.begin(), name.end(), name.begin(), towlower);
                        excludes.insert(name);
                    }
                }
            }
            else if (wcspbrk(fname.c_str(), L"?*") != nullptr)
            {
                std::filesystem::path path(fname.c_str());
                SearchForFiles(path.make_preferred(), flist, false, nullptr);
            }
            else
            {
                SConversion conv = {};
                std::filesystem::path path(fname.c_str());
                conv.szSrc = path.make_preferred().native();
                flist.push_back(conv);
            }
        }

        inFile.close();

        if (!excludes.empty())
        {
            // Remove any excluded files
            for (auto it = flist.begin(); it != flist.end();)
            {
                std::wstring name = it->szSrc;
                std::transform(name.begin(), name.end(), name.begin(), towlower);
                auto item = it;
                ++it;
                if (excludes.find(name) != excludes.end())
                {
                    flist.erase(item);
                }
            }
        }

        if (flist.empty())
        {
            wprintf(L"WARNING: No file names found in -flist\n");
        }
        else
        {
            files.splice(files.end(), flist);
        }
    }

    void PrintFormat(DXGI_FORMAT Format)
    {
        for (auto pFormat = g_pFormats; pFormat->name; pFormat++)
        {
            if (static_cast<DXGI_FORMAT>(pFormat->value) == Format)
            {
                wprintf(L"%ls", pFormat->name);
                return;
            }
        }

        for (auto pFormat = g_pReadOnlyFormats; pFormat->name; pFormat++)
        {
            if (static_cast<DXGI_FORMAT>(pFormat->value) == Format)
            {
                wprintf(L"%ls", pFormat->name);
                return;
            }
        }

        wprintf(L"*UNKNOWN*");
    }

    void PrintInfo(const TexMetadata& info, bool isXbox)
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
            wprintf(L" \x03B1:Opaque");
            break;
        case TEX_ALPHA_MODE_PREMULTIPLIED:
            wprintf(L" \x03B1:PM");
            break;
        case TEX_ALPHA_MODE_STRAIGHT:
            wprintf(L" \x03B1:NonPM");
            break;
        case TEX_ALPHA_MODE_CUSTOM:
            wprintf(L" \x03B1:Custom");
            break;
        case TEX_ALPHA_MODE_UNKNOWN:
            break;
        }

        if (isXbox)
        {
            wprintf(L" Xbox");
        }

        wprintf(L")");
    }

    void PrintList(size_t cch, const SValue<uint32_t> *pValue)
    {
        while (pValue->name)
        {
            const size_t cchName = wcslen(pValue->name);

            if (cch + cchName + 2 >= 80)
            {
                wprintf(L"\n      ");
                cch = 6;
            }

            wprintf(L"%ls ", pValue->name);
            cch += cchName + 2;
            pValue++;
        }

        wprintf(L"\n");
    }

    void PrintLogo(bool versionOnly)
    {
        wchar_t version[32] = {};

        wchar_t appName[_MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, appName, _MAX_PATH))
        {
            const DWORD size = GetFileVersionInfoSizeW(appName, nullptr);
            if (size > 0)
            {
                auto verInfo = std::make_unique<uint8_t[]>(size);
                if (GetFileVersionInfoW(appName, 0, size, verInfo.get()))
                {
                    LPVOID lpstr = nullptr;
                    UINT strLen = 0;
                    if (VerQueryValueW(verInfo.get(), L"\\StringFileInfo\\040904B0\\ProductVersion", &lpstr, &strLen))
                    {
                        wcsncpy_s(version, reinterpret_cast<const wchar_t*>(lpstr), strLen);
                    }
                }
            }
        }

        if (!*version || wcscmp(version, L"1.0.0.0") == 0)
        {
            swprintf_s(version, L"%03d (library)", DIRECTX_TEX_VERSION);
        }

        if (versionOnly)
        {
            wprintf(L"texconv version %ls\n", version);
        }
        else
        {
        #if defined(USE_XBOX_EXTS) && defined(_USE_SCARLETT)
            wprintf(L"Microsoft (R) DirectX Texture Converter for Microsoft GDKX for Xbox Series X|S [Version %ls]\n", version);
        #elif defined(USE_XBOX_EXTS)
            wprintf(L"Microsoft (R) DirectX Texture Converter for Microsoft GDKX for Xbox One [Version %ls]\n", version);
        #else
            wprintf(L"Microsoft (R) DirectX Texture Converter [DirectXTex] Version %ls\n", version);
        #endif
            wprintf(L"Copyright (C) Microsoft Corp.\n");
        #ifdef _DEBUG
            wprintf(L"*** Debug build ***\n");
        #endif
            wprintf(L"\n");
        }
    }

    _Success_(return)
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
        PrintLogo(false);

        static const wchar_t* const s_usage =
            L"Usage: texconv <options> [--] <files>\n"
            L"\n"
            L"   -r                  wildcard filename search is recursive\n"
            L"     -r:flatten        flatten the directory structure (default)\n"
            L"     -r:keep           keep the directory structure\n"
            L"   -flist <filename>   use text file with a list of input files (one per line)\n"
            L"\n"
            L"   -w <n>              width\n"
            L"   -h <n>              height\n"
            L"   -m <n>              miplevels\n"
            L"   -f <format>         format\n"
            L"\n"
            L"   -if <filter>        image filtering\n"
            L"   -srgb{i|o}          sRGB {input, output}\n"
            L"\n"
            L"   -px <string>        name prefix\n"
            L"   -sx <string>        name suffix\n"
            L"   -o <directory>      output directory\n"
            L"   -l                  force output filename to lower case\n"
            L"   -y                  overwrite existing output file (if any)\n"
            L"   -ft <filetype>      output file type\n"
            L"\n"
            L"   -hflip              horizonal flip of source image\n"
            L"   -vflip              vertical flip of source image\n"
            L"\n"
            L"   -sepalpha           resize/generate mips alpha channel separately\n"
            L"                       from color channels\n"
            L"   -keepcoverage <ref> Preserve alpha coverage in mips for alpha test ref\n"
            L"\n"
            L"   -nowic              Force non-WIC filtering\n"
            L"   -wrap, -mirror      texture addressing mode (wrap, mirror, or clamp)\n"
            L"   -pmalpha            convert final texture to use premultiplied alpha\n"
            L"   -alpha              convert premultiplied alpha to straight alpha\n"
            L"   -at <threshold>     Alpha threshold used for BC1, RGBA5551, and WIC\n"
            L"                       (defaults to 0.5)\n"
            L"\n"
            L"   -fl <feature-level> Set maximum feature level target (defaults to 11.0)\n"
            L"   -pow2               resize to fit a power-of-2, respecting aspect ratio\n"
            L"\n"
            L"   -nmap <options>     converts height-map to normal-map\n"
            L"                       options must be one or more of\n"
            L"                          r, g, b, a, l, m, u, v, i, o\n"
            L"   -nmapamp <weight>   normal map amplitude (defaults to 1.0)\n"
            L"\n"
            L"                       (DDS input only)\n"
            L"   -t{u|f}             TYPELESS format is treated as UNORM or FLOAT\n"
            L"   -dword              Use DWORD instead of BYTE alignment\n"
            L"   -badtails           Fix for older DXTn with bad mipchain tails\n"
            L"   -permissive         Allow some DX9 variants with unusual header values\n"
            L"   -fixbc4x4           Fix for odd-sized BC files that Direct3D can't load\n"
            L"   -xlum               expand legacy L8, L16, and A8P8 formats\n"
            L"\n"
            L"                       (DDS output only)\n"
            L"   -dx10               Force use of 'DX10' extended header\n"
            L"   -dx9                Force use of legacy DX9 header\n"
        #ifdef USE_XBOX_EXTS
            L"   -xbox               Tile/swizzle and use 'XBOX' variant of DDS\n"
            L"   -xgmode <mode>      Tile/swizzle using provided memory layout mode\n"
        #endif
            L"\n"
            L"                       (TGA input only)\n"
            L"   -tgazeroalpha       Allow all zero alpha channel files to be loaded 'as is'\n"
            L"\n"
            L"                       (TGA output only)\n"
            L"   -tga20              Write file including TGA 2.0 extension area\n"
            L"\n"
            L"                       (BMP, PNG, JPG, TIF, WDP output only)\n"
            L"   -wicq <quality>     When writing images with WIC use quality (0.0 to 1.0)\n"
            L"   -wiclossless        When writing images with WIC use lossless mode\n"
            L"   -wicmulti           When writing images with WIC encode multiframe images\n"
            L"\n"
            L"   -nologo             suppress copyright message\n"
            L"   -timing             Display elapsed processing time\n"
            L"\n"
        #ifdef _OPENMP
            L"   -singleproc         Do not use multi-threaded compression\n"
        #endif
            L"   -gpu <adapter>      Select GPU for DirectCompute-based codecs (0 is default)\n"
            L"   -nogpu              Do not use DirectCompute-based codecs\n"
            L"\n"
            L"   -bc <options>       Sets options for BC compression\n"
            L"                       options must be one or more of\n"
            L"                          d, u, q, x\n"
            L"   -aw <weight>        BC7 GPU compressor weighting for alpha error metric\n"
            L"                       (defaults to 1.0)\n"
            L"\n"
            L"   -c <hex-RGB>        colorkey (a.k.a. chromakey) transparency\n"
            L"   -rotatecolor <rot>  rotates color primaries and/or applies a curve\n"
            L"   -nits <value>       paper-white value in nits to use for HDR10 (def: 200.0)\n"
            L"   -tonemap            Apply a tonemap operator based on maximum luminance\n"
            L"   -x2bias             Enable *2 - 1 conversion cases for unorm/pos-only-float\n"
            L"   -inverty            Invert Y (i.e. green) channel values\n"
            L"   -reconstructz       Rebuild Z (blue) channel assuming X/Y are normals\n"
            L"   -swizzle <rgba>     Swizzle image channels using HLSL-style mask\n"
            L"\n"
            L"   '-- ' is needed if any input filepath starts with the '-' or '/' character\n";

        wprintf(L"%ls", s_usage);

        wprintf(L"\n   <format>: ");
        PrintList(13, g_pFormats);
        wprintf(L"      ");
        PrintList(13, g_pFormatAliases);
        wprintf(L"      ");
        PrintList(13, g_pSpecialFormats);

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
            for (UINT adapterIndex = 0;
                SUCCEEDED(dxgiFactory->EnumAdapters(adapterIndex, adapter.ReleaseAndGetAddressOf()));
                ++adapterIndex)
            {
                DXGI_ADAPTER_DESC desc;
                if (SUCCEEDED(adapter->GetDesc(&desc)))
                {
                    wprintf(L"      %u: VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
                }
            }
        }
    }

    const wchar_t* GetErrorDesc(HRESULT hr)
    {
        static wchar_t desc[1024] = {};

        LPWSTR errorText = nullptr;

        const DWORD result = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            nullptr, static_cast<DWORD>(hr),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&errorText), 0, nullptr);

        *desc = 0;

        if (result > 0 && errorText)
        {
            swprintf_s(desc, L": %ls", errorText);

            size_t len = wcslen(desc);
            if (len >= 1)
            {
                desc[len - 1] = 0;
            }

            if (errorText)
                LocalFree(errorText);

            for (wchar_t* ptr = desc; *ptr != 0; ++ptr)
            {
                if (*ptr == L'\r' || *ptr == L'\n')
                {
                    *ptr = L' ';
                }
            }
        }

        return desc;
    }

    _Success_(return)
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

        const D3D_FEATURE_LEVEL featureLevels[] =
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
            nullptr, createDeviceFlags, featureLevels, static_cast<UINT>(std::size(featureLevels)),
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
                        wprintf(L"\n[Using DirectCompute %ls on \"%ls\"]\n",
                            (fl >= D3D_FEATURE_LEVEL_11_0) ? L"5.0" : L"4.0", desc.Description);
                    }
                }
            }

            return true;
        }
        else
            return false;
    }

    void FitPowerOf2(size_t origx, size_t origy, _Inout_ size_t& targetx, _Inout_ size_t& targety, size_t maxsize)
    {
        const float origAR = float(origx) / float(origy);

        if (origx > origy)
        {
            size_t x;
            for (x = maxsize; x > 1; x >>= 1) { if (x <= targetx) break; }
            targetx = x;

            float bestScore = FLT_MAX;
            for (size_t y = maxsize; y > 0; y >>= 1)
            {
                const float score = fabsf((float(x) / float(y)) - origAR);
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
                const float score = fabsf((float(x) / float(y)) - origAR);
                if (score < bestScore)
                {
                    bestScore = score;
                    targetx = x;
                }
            }
        }
    }

    constexpr size_t CountMips(_In_ size_t width, _In_ size_t height) noexcept
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

    constexpr size_t CountMips3D(_In_ size_t width, _In_ size_t height, _In_ size_t depth) noexcept
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

    const XMVECTORF32 c_MaxNitsFor2084 = { { { 10000.0f, 10000.0f, 10000.0f, 1.f } } };

    // HDTV to UHDTV (Rec.709 color primaries into Rec.2020)
    const XMMATRIX c_from709to2020 =
    {
        0.6274040f, 0.0690970f, 0.0163916f, 0.f,
        0.3292820f, 0.9195400f, 0.0880132f, 0.f,
        0.0433136f, 0.0113612f, 0.8955950f, 0.f,
        0.f,        0.f,        0.f,        1.f
    };

    // UHDTV to HDTV
    const XMMATRIX c_from2020to709 =
    {
        1.6604910f,  -0.1245505f, -0.0181508f, 0.f,
        -0.5876411f,  1.1328999f, -0.1005789f, 0.f,
        -0.0728499f, -0.0083494f,  1.1187297f, 0.f,
        0.f,          0.f,         0.f,        1.f
    };

    // DCI-P3-D65 https://en.wikipedia.org/wiki/DCI-P3 to UHDTV (DCI-P3-D65 color primaries into Rec.2020)
    const XMMATRIX c_fromP3D65to2020 =
    {
        0.753845f, 0.0457456f, -0.00121055f, 0.f,
        0.198593f, 0.941777f,   0.0176041f,  0.f,
        0.047562f, 0.0124772f,  0.983607f,   0.f,
        0.f,       0.f,         0.f,         1.f
    };

    // HDTV to DCI-P3-D65 (a.k.a. Display P3 or P3D65)
    const XMMATRIX c_from709toP3D65 =
    {
        0.822461969f, 0.033194199f, 0.017082631f, 0.f,
        0.1775380f,   0.9668058f,   0.0723974f,   0.f,
        0.0000000f,   0.0000000f,   0.9105199f,   0.f,
        0.f,          0.f,          0.f,          1.f
    };

    // DCI-P3-D65 to HDTV
    const XMMATRIX c_fromP3D65to709 =
    {
        1.224940176f,  -0.042056955f, -0.019637555f, 0.f,
        -0.224940176f,  1.042056955f, -0.078636046f, 0.f,
        0.0000000f,     0.0000000f,    1.098273600f, 0.f,
        0.f,            0.f,           0.f,          1.f
    };

    inline float LinearToST2084(float normalizedLinearValue)
    {
        const float ST2084 = pow((0.8359375f + 18.8515625f * pow(abs(normalizedLinearValue), 0.1593017578f)) / (1.0f + 18.6875f * pow(abs(normalizedLinearValue), 0.1593017578f)), 78.84375f);
        return ST2084;  // Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits
    }

    inline float ST2084ToLinear(float ST2084)
    {
        const float normalizedLinear = pow(std::max(pow(abs(ST2084), 1.0f / 78.84375f) - 0.8359375f, 0.0f) / (18.8515625f - 18.6875f * pow(abs(ST2084), 1.0f / 78.84375f)), 1.0f / 0.1593017578f);
        return normalizedLinear;
    }

    bool ParseSwizzleMask(
        _In_reads_(4) const wchar_t* mask,
        _Out_writes_(4) uint32_t* swizzleElements,
        _Out_writes_(4) uint32_t* zeroElements,
        _Out_writes_(4) uint32_t* oneElements)
    {
        if (!mask || !swizzleElements || !zeroElements || !oneElements)
            return false;

        if (!mask[0])
            return false;

        for (uint32_t j = 0; j < 4; ++j)
        {
            if (!mask[j])
                break;

            switch (mask[j])
            {
            case L'R':
            case L'X':
            case L'r':
            case L'x':
                for (uint32_t k = j; k < 4; ++k)
                {
                    swizzleElements[k] = 0;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'G':
            case L'Y':
            case L'g':
            case L'y':
                for (uint32_t k = j; k < 4; ++k)
                {
                    swizzleElements[k] = 1;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'B':
            case L'Z':
            case L'b':
            case L'z':
                for (uint32_t k = j; k < 4; ++k)
                {
                    swizzleElements[k] = 2;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'A':
            case L'W':
            case L'a':
            case L'w':
                for (size_t k = j; k < 4; ++k)
                {
                    swizzleElements[k] = 3;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'0':
                for (uint32_t k = j; k < 4; ++k)
                {
                    swizzleElements[k] = k;
                    zeroElements[k] = 1;
                    oneElements[k] = 0;
                }
                break;

            case L'1':
                for (uint32_t k = j; k < 4; ++k)
                {
                    swizzleElements[k] = k;
                    zeroElements[k] = 0;
                    oneElements[k] = 1;
                }
                break;

            default:
                return false;
            }
        }

        return true;
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
    TEX_FILTER_FLAGS dwFilter = TEX_FILTER_DEFAULT;
    TEX_FILTER_FLAGS dwSRGB = TEX_FILTER_DEFAULT;
    TEX_FILTER_FLAGS dwConvert = TEX_FILTER_DEFAULT;
    TEX_COMPRESS_FLAGS dwCompress = TEX_COMPRESS_DEFAULT;
    TEX_FILTER_FLAGS dwFilterOpts = TEX_FILTER_DEFAULT;
    uint32_t FileType = CODEC_DDS;
    uint32_t maxSize = 16384;
    int adapter = -1;
    float alphaThreshold = TEX_THRESHOLD_DEFAULT;
    float alphaWeight = 1.f;
    CNMAP_FLAGS dwNormalMap = CNMAP_DEFAULT;
    float nmapAmplitude = 1.f;
    float wicQuality = -1.f;
    uint32_t colorKey = 0;
    uint32_t dwRotateColor = 0;
    float paperWhiteNits = 200.f;
    float preserveAlphaCoverageRef = 0.0f;
    bool keepRecursiveDirs = false;
    bool dxt5nm = false;
    bool dxt5rxgb = false;
    uint32_t swizzleElements[4] = { 0, 1, 2, 3 };
    uint32_t zeroElements[4] = {};
    uint32_t oneElements[4] = {};

    wchar_t szPrefix[MAX_PATH] = {};
    wchar_t szSuffix[MAX_PATH] = {};
    std::filesystem::path outputDir;

    // Set locale for output since GetErrorDesc can get localized strings.
    std::locale::global(std::locale(""));

    // Initialize COM (needed for WIC)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        wprintf(L"Failed to initialize COM (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
        return 1;
    }

    // Process command line
    uint64_t dwOptions = 0;
    std::list<SConversion> conversion;
    bool allowOpts = true;

    for (int iArg = 1; iArg < argc; iArg++)
    {
        PWSTR pArg = argv[iArg];

        if (allowOpts
            && ('-' == pArg[0]) && ('-' == pArg[1]))
        {
            if (pArg[2] == 0)
            {
                // "-- " is the POSIX standard for "end of options" marking to escape the '-' and '/' characters at the start of filepaths.
                allowOpts = false;
            }
            else if (!_wcsicmp(pArg,L"--version"))
            {
                PrintLogo(true);
                return 0;
            }
            else if (!_wcsicmp(pArg, L"--help"))
            {
                PrintUsage();
                return 0;
            }
            else
            {
                wprintf(L"Unknown option: %ls\n", pArg);
                return 1;
            }
        }
        else if (allowOpts
            && (('-' == pArg[0]) || ('/' == pArg[0])))
        {
            pArg++;
            PWSTR pValue;

            for (pValue = pArg; *pValue && (':' != *pValue); pValue++);

            if (*pValue)
                *pValue++ = 0;

            const uint64_t dwOption = LookupByName(pArg, g_pOptions);

            if (!dwOption || (dwOptions & (uint64_t(1) << dwOption)))
            {
                PrintUsage();
                return 1;
            }

            dwOptions |= (uint64_t(1) << dwOption);

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
            case OPT_SWIZZLE:
        #ifdef USE_XBOX_EXTS
            case OPT_XGMODE:
        #endif
                // These support either "-arg:value" or "-arg value"
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
                        switch (LookupByName(pValue, g_pSpecialFormats))
                        {
                        case FORMAT_DXT5_NM:
                            format = DXGI_FORMAT_BC3_UNORM;
                            dxt5nm = true;
                            break;

                        case FORMAT_DXT5_RXGB:
                            format = DXGI_FORMAT_BC3_UNORM;
                            dxt5rxgb = true;
                            break;

                        default:
                            wprintf(L"Invalid value specified with -f (%ls)\n", pValue);
                            wprintf(L"\n");
                            PrintUsage();
                            return 1;
                        }
                    }
                }
                break;

            case OPT_FILTER:
                dwFilter = static_cast<TEX_FILTER_FLAGS>(LookupByName(pValue, g_pFilters));
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
                {
                    std::filesystem::path path(pValue);
                    outputDir = path.make_preferred();
                }
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
                if (dwOptions & (uint64_t(1) << OPT_DEMUL_ALPHA))
                {
                    wprintf(L"Can't use -pmalpha and -alpha at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_DEMUL_ALPHA:
                if (dwOptions & (uint64_t(1) << OPT_PREMUL_ALPHA))
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
                    dwNormalMap = CNMAP_DEFAULT;

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
                    wprintf(L"Invalid adapter index (%ls)\n\n", pValue);
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
                if (swscanf_s(pValue, L"%x", &colorKey) != 1)
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
                if (dwOptions & (uint64_t(1) << OPT_USE_DX9))
                {
                    wprintf(L"Can't use -dx9 and -dx10 at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_USE_DX9:
                if (dwOptions & (uint64_t(1) << OPT_USE_DX10))
                {
                    wprintf(L"Can't use -dx9 and -dx10 at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_RECURSIVE:
                if (*pValue)
                {
                    // This option takes 'flatten' or 'keep' with ':' syntax
                    if (!_wcsicmp(pValue, L"keep"))
                    {
                        keepRecursiveDirs = true;
                    }
                    else if (_wcsicmp(pValue, L"flatten") != 0)
                    {
                        wprintf(L"For recursive use -r, -r:flatten, or -r:keep\n\n");
                        PrintUsage();
                        return 1;
                    }
                }
                break;

            case OPT_FILELIST:
                {
                    std::filesystem::path path(pValue);
                    std::wifstream inFile(path.make_preferred().c_str());
                    if (!inFile)
                    {
                        wprintf(L"Error opening -flist file %ls\n", pValue);
                        return 1;
                    }

                    inFile.imbue(std::locale::classic());

                    ProcessFileList(inFile, conversion);
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

            case OPT_SWIZZLE:
                if (!*pValue || wcslen(pValue) > 4)
                {
                    wprintf(L"Invalid value specified with -swizzle (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                else if (!ParseSwizzleMask(pValue, swizzleElements, zeroElements, oneElements))
                {
                    wprintf(L"-swizzle requires a 1 to 4 character mask composed of these letters: r, g, b, a, x, y, w, z, 0, 1\n");
                    return 1;
                }
                break;

        #ifdef USE_XBOX_EXTS
            case OPT_XGMODE:
                {
                #ifdef _USE_SCARLETT
                    static const SValue<uint32_t> s_pXGModes[] =
                    {
                        { L"xboxseriess",  XG_HARDWARE_VERSION_XBOX_SCARLETT_LOCKHART },
                        { L"xboxseriesx",  XG_HARDWARE_VERSION_XBOX_SCARLETT_ANACONDA },
                        { nullptr,     0 },
                    };
                #else
                    static const SValue<uint32_t> s_pXGModes[] =
                    {
                        { L"xboxone",   XG_HARDWARE_VERSION_XBOX_ONE },
                        { L"xboxonex",  XG_HARDWARE_VERSION_XBOX_ONE_X },
                        { L"scorpio",   XG_HARDWARE_VERSION_SCORPIO },
                        { nullptr,     0 },
                    };
                #endif

                    const uint32_t mode = LookupByName(pValue, s_pXGModes);
                    if (!mode)
                    {
                        printf("Invalid value specified with -xgmode (%ls)\n", pValue);
                        wprintf(L"\n   <mode>: ");
                        PrintList(14, s_pXGModes);
                        return 1;
                    }

                    XGSetHardwareVersion(static_cast<XG_HARDWARE_VERSION>(mode));
                    break;
                }
        #endif // USE_XBOX_EXTS
            }
        }
        else if (wcspbrk(pArg, L"?*") != nullptr)
        {
            const size_t count = conversion.size();
            std::filesystem::path path(pArg);
            SearchForFiles(path.make_preferred(), conversion, (dwOptions & (uint64_t(1) << OPT_RECURSIVE)) != 0, nullptr);
            if (conversion.size() <= count)
            {
                wprintf(L"No matching files found for %ls\n", pArg);
                return 1;
            }
        }
        else
        {
            SConversion conv = {};
            std::filesystem::path path(pArg);
            conv.szSrc = path.make_preferred().native();
            conversion.push_back(conv);
        }
    }

    if (conversion.empty())
    {
        PrintUsage();
        return 0;
    }

    if (~dwOptions & (uint64_t(1) << OPT_NOLOGO))
        PrintLogo(false);

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

    LARGE_INTEGER qpcFreq = {};
    std::ignore = QueryPerformanceFrequency(&qpcFreq);

    LARGE_INTEGER qpcStart = {};
    std::ignore = QueryPerformanceCounter(&qpcStart);

    // Convert images
    bool sizewarn = false;
    bool nonpow2warn = false;
    bool non4bc = false;
    bool preserveAlphaCoverage = false;
    ComPtr<ID3D11Device> pDevice;

    int retVal = 0;

    for (auto pConv = conversion.begin(); pConv != conversion.end(); ++pConv)
    {
        if (pConv != conversion.begin())
            wprintf(L"\n");

        // --- Load source image -------------------------------------------------------
        wprintf(L"reading %ls", pConv->szSrc.c_str());
        fflush(stdout);

        TexMetadata info;
        std::unique_ptr<ScratchImage> image(new (std::nothrow) ScratchImage);

        if (!image)
        {
            wprintf(L"\nERROR: Memory allocation failed\n");
            return 1;
        }

        std::filesystem::path curpath(pConv->szSrc);
        auto const ext = curpath.extension();

    #ifndef USE_XBOX_EXTS
        constexpr
    #endif
        bool isXbox = false;
        if (_wcsicmp(ext.c_str(), L".dds") == 0 || _wcsicmp(ext.c_str(), L".ddx") == 0)
        {
        #ifdef USE_XBOX_EXTS
            hr = Xbox::GetMetadataFromDDSFile(curpath.c_str(), info, isXbox);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
                continue;
            }

            if (isXbox)
            {
                Xbox::XboxImage xbox;

                hr = Xbox::LoadFromDDSFile(curpath.c_str(), &info, xbox);
                if (SUCCEEDED(hr))
                {
                    hr = Xbox::Detile(xbox, *image);
                }
            }
            else
        #endif // USE_XBOX_EXTS
            {
                DDS_FLAGS ddsFlags = DDS_FLAGS_ALLOW_LARGE_FILES;
                if (dwOptions & (uint64_t(1) << OPT_DDS_DWORD_ALIGN))
                    ddsFlags |= DDS_FLAGS_LEGACY_DWORD;
                if (dwOptions & (uint64_t(1) << OPT_EXPAND_LUMINANCE))
                    ddsFlags |= DDS_FLAGS_EXPAND_LUMINANCE;
                if (dwOptions & (uint64_t(1) << OPT_DDS_BAD_DXTN_TAILS))
                    ddsFlags |= DDS_FLAGS_BAD_DXTN_TAILS;
                if (dwOptions & (uint64_t(1) << OPT_DDS_PERMISSIVE))
                    ddsFlags |= DDS_FLAGS_PERMISSIVE;

                hr = LoadFromDDSFile(curpath.c_str(), ddsFlags, &info, *image);
            }
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
                continue;
            }

            if (IsTypeless(info.format))
            {
                if (dwOptions & (uint64_t(1) << OPT_TYPELESS_UNORM))
                {
                    info.format = MakeTypelessUNORM(info.format);
                }
                else if (dwOptions & (uint64_t(1) << OPT_TYPELESS_FLOAT))
                {
                    info.format = MakeTypelessFLOAT(info.format);
                }

                if (IsTypeless(info.format))
                {
                    wprintf(L" FAILED due to Typeless format %d\n", info.format);
                    retVal = 1;
                    continue;
                }

                image->OverrideFormat(info.format);
            }
        }
        else if (_wcsicmp(ext.c_str(), L".bmp") == 0)
        {
            hr = LoadFromBMPEx(curpath.c_str(), WIC_FLAGS_NONE | dwFilter, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
                continue;
            }
        }
        else if (_wcsicmp(ext.c_str(), L".tga") == 0)
        {
            TGA_FLAGS tgaFlags = (IsBGR(format)) ? TGA_FLAGS_BGR : TGA_FLAGS_NONE;
            if (dwOptions & (uint64_t(1) << OPT_TGAZEROALPHA))
            {
                tgaFlags |= TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA;
            }

            hr = LoadFromTGAFile(curpath.c_str(), tgaFlags, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
                continue;
            }
        }
        else if (_wcsicmp(ext.c_str(), L".hdr") == 0)
        {
            hr = LoadFromHDRFile(curpath.c_str(), &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
                continue;
            }
        }
        else if (_wcsicmp(ext.c_str(), L".ppm") == 0)
        {
            hr = LoadFromPortablePixMap(curpath.c_str(), &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
                continue;
            }
        }
        else if (_wcsicmp(ext.c_str(), L".pfm") == 0)
        {
            hr = LoadFromPortablePixMapHDR(curpath.c_str(), &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
                continue;
            }
        }
    #ifdef USE_OPENEXR
        else if (_wcsicmp(ext.c_str(), L".exr") == 0)
        {
            hr = LoadFromEXRFile(curpath.c_str(), &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
                continue;
            }
        }
    #endif
    #ifdef USE_LIBJPEG
        else if (_wcsicmp(ext.c_str(), L".jpg") == 0 || _wcsicmp(ext.c_str(), L".jpeg") == 0)
        {
            hr = LoadFromJPEGFile(curpath.c_str(), &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
                continue;
            }
        }
    #endif
    #ifdef USE_LIBPNG
        else if (_wcsicmp(ext.c_str(), L".png") == 0)
        {
            hr = LoadFromPNGFile(curpath.c_str(), &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
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

            WIC_FLAGS wicFlags = WIC_FLAGS_NONE | dwFilter;
            if (FileType == CODEC_DDS)
                wicFlags |= WIC_FLAGS_ALL_FRAMES;

            hr = LoadFromWICFile(curpath.c_str(), wicFlags, &info, *image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
                if (hr == static_cast<HRESULT>(0xc00d5212) /* MF_E_TOPO_CODEC_NOT_FOUND */)
                {
                    if (_wcsicmp(ext.c_str(), L".heic") == 0 || _wcsicmp(ext.c_str(), L".heif") == 0)
                    {
                        wprintf(L"INFO: This format requires installing the HEIF Image Extensions - https://aka.ms/heif\n");
                    }
                    else if (_wcsicmp(ext.c_str(), L".webp") == 0)
                    {
                        wprintf(L"INFO: This format requires installing the WEBP Image Extensions - https://www.microsoft.com/p/webp-image-extensions/9pg2dk419drg\n");
                    }
                }
                continue;
            }
        }

        PrintInfo(info, isXbox);

        size_t tMips = (!mipLevels && info.mipLevels > 1) ? info.mipLevels : mipLevels;

        // Convert texture
        wprintf(L" as");
        fflush(stdout);

        // --- Planar ------------------------------------------------------------------
        if (IsPlanar(info.format))
        {
            auto img = image->GetImage(0, 0, 0);
            assert(img);
            const size_t nimg = image->GetImageCount();

            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            hr = ConvertToSinglePlane(img, nimg, info, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [converttosingleplane] (%08X%ls)\n",
                    static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
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

        const DXGI_FORMAT tformat = (format == DXGI_FORMAT_UNKNOWN) ? info.format : format;

        // --- Decompress --------------------------------------------------------------
        std::unique_ptr<ScratchImage> cimage;
        if (IsCompressed(info.format))
        {
            // Direct3D can only create BC resources with multiple-of-4 top levels
            if ((info.width % 4) != 0 || (info.height % 4) != 0)
            {
                if (dwOptions & (uint64_t(1) << OPT_BCNONMULT4FIX))
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
                        wprintf(L" FAILED [BC non-multiple-of-4 fixup] (%08X%ls)\n",
                            static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
            const size_t nimg = image->GetImageCount();

            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            hr = Decompress(img, nimg, info, DXGI_FORMAT_UNKNOWN /* picks good default */, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [decompress] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
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
        if ((dwOptions & (uint64_t(1) << OPT_DEMUL_ALPHA))
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
                const size_t nimg = image->GetImageCount();

                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                hr = PremultiplyAlpha(img, nimg, info, TEX_PMALPHA_REVERSE | dwSRGB, *timage);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [demultiply alpha] (%08X%ls)\n",
                        static_cast<unsigned int>(hr), GetErrorDesc(hr));
                    retVal = 1;
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
        if (dwOptions & ((uint64_t(1) << OPT_HFLIP) | (uint64_t(1) << OPT_VFLIP)))
        {
            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            TEX_FR_FLAGS dwFlags = TEX_FR_ROTATE0;

            if (dwOptions & (uint64_t(1) << OPT_HFLIP))
                dwFlags |= TEX_FR_FLIP_HORIZONTAL;

            if (dwOptions & (uint64_t(1) << OPT_VFLIP))
                dwFlags |= TEX_FR_FLIP_VERTICAL;

            assert(dwFlags != 0);

            hr = FlipRotate(image->GetImages(), image->GetImageCount(), image->GetMetadata(), dwFlags, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [fliprotate] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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

        if (dwOptions & (uint64_t(1) << OPT_FIT_POWEROF2))
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
                wprintf(L" FAILED [resize] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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

            if (tMips > 0)
            {
                const size_t maxMips = (info.depth > 1)
                    ? CountMips3D(info.width, info.height, info.depth)
                    : CountMips(info.width, info.height);

                if (tMips > maxMips)
                {
                    tMips = maxMips;
                }
            }
        }

        // --- Swizzle (if requested) --------------------------------------------------
        if (swizzleElements[0] != 0 || swizzleElements[1] != 1 || swizzleElements[2] != 2 || swizzleElements[3] != 3
            || zeroElements[0] != 0 || zeroElements[1] != 0 || zeroElements[2] != 0 || zeroElements[3] != 0
            || oneElements[0] != 0 || oneElements[1] != 0 || oneElements[2] != 0 || oneElements[3] != 0)
        {
            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            const XMVECTOR zc = XMVectorSelectControl(zeroElements[0], zeroElements[1], zeroElements[2], zeroElements[3]);
            const XMVECTOR oc = XMVectorSelectControl(oneElements[0], oneElements[1], oneElements[2], oneElements[3]);

            hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                [&, zc, oc](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                {
                    UNREFERENCED_PARAMETER(y);

                    for (size_t j = 0; j < w; ++j)
                    {
                        XMVECTOR pixel = XMVectorSwizzle(inPixels[j],
                            swizzleElements[0], swizzleElements[1], swizzleElements[2], swizzleElements[3]);
                        pixel = XMVectorSelect(pixel, g_XMZero, zc);
                        outPixels[j] = XMVectorSelect(pixel, g_XMOne, oc);
                    }
                }, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [swizzle] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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

        // --- Color rotation (if requested) -------------------------------------------
        if (dwRotateColor)
        {
            if (dwRotateColor == ROTATE_HDR10_TO_709 || dwRotateColor == ROTATE_P3D65_TO_709)
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
                    wprintf(L" FAILED [convert] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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

                        const XMVECTOR paperWhite = XMVectorReplicate(paperWhiteNits);

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

                            const XMVECTOR nvalue = XMVector3Transform(value, c_from709to2020);

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

                        const XMVECTOR paperWhite = XMVectorReplicate(paperWhiteNits);

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

                            const XMVECTOR nvalue = XMVector3Transform(value, c_from2020to709);

                            value = XMVectorSelect(value, nvalue, g_XMSelect1110);

                            outPixels[j] = value;
                        }
                    }, *timage);
                break;

            case ROTATE_P3D65_TO_HDR10:
                hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                    {
                        UNREFERENCED_PARAMETER(y);

                        const XMVECTOR paperWhite = XMVectorReplicate(paperWhiteNits);

                        for (size_t j = 0; j < w; ++j)
                        {
                            XMVECTOR value = inPixels[j];

                            XMVECTOR nvalue = XMVector3Transform(value, c_fromP3D65to2020);

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

            case ROTATE_P3D65_TO_2020:
                hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                    {
                        UNREFERENCED_PARAMETER(y);

                        for (size_t j = 0; j < w; ++j)
                        {
                            XMVECTOR value = inPixels[j];

                            const XMVECTOR nvalue = XMVector3Transform(value, c_fromP3D65to2020);

                            value = XMVectorSelect(value, nvalue, g_XMSelect1110);

                            outPixels[j] = value;
                        }
                    }, *timage);
                break;

            case ROTATE_709_TO_P3D65:
                hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                    {
                        UNREFERENCED_PARAMETER(y);

                        for (size_t j = 0; j < w; ++j)
                        {
                            XMVECTOR value = inPixels[j];

                            const XMVECTOR nvalue = XMVector3Transform(value, c_from709toP3D65);

                            value = XMVectorSelect(value, nvalue, g_XMSelect1110);

                            outPixels[j] = value;
                        }
                    }, *timage);
                break;

            case ROTATE_P3D65_TO_709:
                hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                    [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                    {
                        UNREFERENCED_PARAMETER(y);

                        for (size_t j = 0; j < w; ++j)
                        {
                            XMVECTOR value = inPixels[j];

                            const XMVECTOR nvalue = XMVector3Transform(value, c_fromP3D65to709);

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
                wprintf(L" FAILED [rotate color apply] (%08X%ls)\n",
                    static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
        if (dwOptions & uint64_t(1) << OPT_TONEMAP)
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
                wprintf(L" FAILED [tonemap maxlum] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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

                        const XMVECTOR scale = XMVectorDivide(
                            XMVectorAdd(g_XMOne, XMVectorDivide(value, maxLum)),
                            XMVectorAdd(g_XMOne, value));
                        const XMVECTOR nvalue = XMVectorMultiply(value, scale);

                        value = XMVectorSelect(value, nvalue, g_XMSelect1110);

                        outPixels[j] = value;
                    }
                }, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [tonemap apply] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
        if (dwOptions & (uint64_t(1) << OPT_NORMAL_MAP))
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
                    nmfmt = (BitsPerColor(info.format) > 8) ? DXGI_FORMAT_R16G16B16A16_SNORM : DXGI_FORMAT_R8G8B8A8_SNORM;
                    break;

                case DXGI_FORMAT_BC6H_SF16:
                case DXGI_FORMAT_BC6H_UF16:
                    nmfmt = DXGI_FORMAT_R32G32B32_FLOAT;
                    break;

                default:
                    nmfmt = (BitsPerColor(info.format) > 8) ? DXGI_FORMAT_R16G16B16A16_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
                    break;
                }
            }

            hr = ComputeNormalMap(image->GetImages(), image->GetImageCount(), image->GetMetadata(), dwNormalMap, nmapAmplitude, nmfmt, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [normalmap] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
                wprintf(L" FAILED [convert] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
        if ((dwOptions & (uint64_t(1) << OPT_COLORKEY))
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
                wprintf(L" FAILED [colorkey] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
        if (dwOptions & (uint64_t(1) << OPT_INVERT_Y))
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
                        const XMVECTOR value = inPixels[j];

                        const XMVECTOR inverty = XMVectorSubtract(g_XMOne, value);

                        outPixels[j] = XMVectorSelect(value, inverty, s_selecty);
                    }
                }, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [inverty] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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

        // --- Reconstruct Z Channel ---------------------------------------------------
        if (dwOptions & (uint64_t(1) << OPT_RECONSTRUCT_Z))
        {
            std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
            if (!timage)
            {
                wprintf(L"\nERROR: Memory allocation failed\n");
                return 1;
            }

            bool isunorm = (FormatDataType(info.format) == FORMAT_TYPE_UNORM) != 0;

            hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                [&](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                {
                    static const XMVECTORU32 s_selectz = { { { XM_SELECT_0, XM_SELECT_0, XM_SELECT_1, XM_SELECT_0 } } };

                    UNREFERENCED_PARAMETER(y);

                    for (size_t j = 0; j < w; ++j)
                    {
                        const XMVECTOR value = inPixels[j];

                        XMVECTOR z;
                        if (isunorm)
                        {
                            XMVECTOR x2 = XMVectorMultiplyAdd(value, g_XMTwo, g_XMNegativeOne);
                            x2 = XMVectorSqrt(XMVectorSubtract(g_XMOne, XMVector2Dot(x2, x2)));
                            z = XMVectorMultiplyAdd(x2, g_XMOneHalf, g_XMOneHalf);
                        }
                        else
                        {
                            z = XMVectorSqrt(XMVectorSubtract(g_XMOne, XMVector2Dot(value, value)));
                        }

                        outPixels[j] = XMVectorSelect(value, z, s_selectz);
                    }
                }, *timage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [reconstructz] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
        TEX_FILTER_FLAGS dwFilter3D = dwFilter;
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
                wprintf(L" FAILED [copy to single level] (%08X%ls)\n",
                    static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
                        wprintf(L" FAILED [copy to single level] (%08X%ls)\n",
                            static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
                        wprintf(L" FAILED [copy to single level] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                        return 1;
                    }
                }
            }

            image.swap(timage);
            info.mipLevels = 1;

            if (cimage && (tMips == 1))
            {
                // Special case for trimming mips off compressed images and keeping the original compressed highest level mip
                mdata = cimage->GetMetadata();
                mdata.mipLevels = 1;
                hr = timage->Initialize(mdata);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [copy compressed to single level] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
                wprintf(L" FAILED [mipmaps] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
                wprintf(L" FAILED [keepcoverage] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
                    wprintf(L" FAILED [keepcoverage] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
        if ((dwOptions & (uint64_t(1) << OPT_PREMUL_ALPHA))
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
                const size_t nimg = image->GetImageCount();

                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                hr = PremultiplyAlpha(img, nimg, info, TEX_PMALPHA_DEFAULT | dwSRGB, *timage);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [premultiply alpha] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                    retVal = 1;
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
        if (FileType == CODEC_DDS)
        {
            if (dxt5nm || dxt5rxgb)
            {
                // Prepare for DXT5nm/RXGB
                assert(tformat == DXGI_FORMAT_BC3_UNORM);

                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                if (dxt5nm)
                {
                    hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                        [=](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                        {
                            UNREFERENCED_PARAMETER(y);

                            for (size_t j = 0; j < w; ++j)
                            {
                                outPixels[j] = XMVectorPermute<4, 1, 5, 0>(inPixels[j], g_XMIdentityR0);
                            }
                        }, *timage);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED [DXT5nm] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                        return 1;
                    }
                }
                else
                {
                    hr = TransformImage(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                        [=](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                        {
                            UNREFERENCED_PARAMETER(y);

                            for (size_t j = 0; j < w; ++j)
                            {
                                outPixels[j] = XMVectorSwizzle<3, 1, 2, 0>(inPixels[j]);
                            }
                        }, *timage);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED [DXT5 RXGB] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
                assert(info.format == tinfo.format);
                assert(info.dimension == tinfo.dimension);

                image.swap(timage);
                cimage.reset();
            }

            if (IsCompressed(tformat))
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
                    const size_t nimg = image->GetImageCount();

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

                                if (!(dwOptions & (uint64_t(1) << OPT_NOGPU)))
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

                    TEX_COMPRESS_FLAGS cflags = dwCompress;
                #ifdef _OPENMP
                    if (!(dwOptions & (uint64_t(1) << OPT_FORCE_SINGLEPROC)))
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
                        wprintf(L" FAILED [compress] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                        retVal = 1;
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
        }

        cimage.reset();

        // --- Set alpha mode ----------------------------------------------------------
        if (HasAlpha(info.format)
            && info.format != DXGI_FORMAT_A8_UNORM)
        {
            if (dxt5nm || dxt5rxgb)
            {
                info.SetAlphaMode(TEX_ALPHA_MODE_CUSTOM);
            }
            else if (image->IsAlphaAllOpaque())
            {
                info.SetAlphaMode(TEX_ALPHA_MODE_OPAQUE);
            }
            else if (info.IsPMAlpha())
            {
                // Aleady set TEX_ALPHA_MODE_PREMULTIPLIED
            }
            else if (dwOptions & (uint64_t(1) << OPT_SEPALPHA))
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
            const size_t nimg = image->GetImageCount();

        #ifdef USE_XBOX_EXTS
            const bool isXboxOut = ((FileType == CODEC_DDS) && (dwOptions & (uint64_t(1) << OPT_USE_XBOX))) != 0;
        #else
            constexpr bool isXboxOut = false;
        #endif
            PrintInfo(info, isXboxOut);
            wprintf(L"\n");

            // Figure out dest filename
            std::filesystem::path dest(outputDir);

            if (keepRecursiveDirs && !pConv->szFolder.empty())
            {
                dest.append(pConv->szFolder.c_str());

                std::error_code ec;
                auto apath = std::filesystem::absolute(dest, ec);

                if (ec)
                {
                    wprintf(L" get full path FAILED (%hs)\n", ec.message().c_str());
                    retVal = 1;
                    continue;
                }

                auto const err = static_cast<DWORD>(SHCreateDirectoryExW(nullptr, apath.c_str(), nullptr));
                if (err != ERROR_SUCCESS && err != ERROR_ALREADY_EXISTS)
                {
                    wprintf(L" directory creation FAILED (%08X%ls)\n",
                        static_cast<unsigned int>(HRESULT_FROM_WIN32(err)), GetErrorDesc(HRESULT_FROM_WIN32(err)));
                    retVal = 1;
                    continue;
                }
            }

            if (*szPrefix)
            {
                dest.append(szPrefix);
                dest.concat(curpath.stem().c_str());
                dest.concat(szSuffix);
            }
            else
            {
                dest.append(curpath.stem().c_str());
                dest.concat(szSuffix);
            }

            std::wstring destName = dest.c_str();
            if (dwOptions & (uint64_t(1) << OPT_TOLOWER))
            {
                std::transform(destName.begin(), destName.end(), destName.begin(), towlower);
            }

            // Write texture
            wprintf(L"writing %ls", destName.c_str());
            fflush(stdout);

            if (~dwOptions & (uint64_t(1) << OPT_OVERWRITE))
            {
                if (GetFileAttributesW(destName.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    wprintf(L"\nERROR: Output file already exists, use -y to overwrite:\n");
                    retVal = 1;
                    continue;
                }
            }

            switch (FileType)
            {
            case CODEC_DDS:
            #ifdef USE_XBOX_EXTS
                if (isXboxOut)
                {
                    Xbox::XboxImage xbox;

                    hr = Xbox::Tile(img, nimg, info, xbox);
                    if (SUCCEEDED(hr))
                    {
                        hr = Xbox::SaveToDDSFile(xbox, destName.c_str());
                    }
                }
                else
            #endif // USE_XBOX_EXTS
                {
                    DDS_FLAGS ddsFlags = DDS_FLAGS_NONE;
                    if (dwOptions & (uint64_t(1) << OPT_USE_DX10))
                    {
                        ddsFlags |= DDS_FLAGS_FORCE_DX10_EXT | DDS_FLAGS_FORCE_DX10_EXT_MISC2;
                    }
                    else if (dwOptions & (uint64_t(1) << OPT_USE_DX9))
                    {
                        if (dxt5rxgb)
                        {
                            ddsFlags |= DDS_FLAGS_FORCE_DXT5_RXGB;
                        }

                        ddsFlags |= DDS_FLAGS_FORCE_DX9_LEGACY;
                    }

                    hr = SaveToDDSFile(img, nimg, info, ddsFlags, destName.c_str());
                }
                break;

            case CODEC_TGA:
                hr = SaveToTGAFile(img[0], TGA_FLAGS_NONE, destName.c_str(), (dwOptions & (uint64_t(1) << OPT_TGA20)) ? &info : nullptr);
                break;

            case CODEC_HDR:
                hr = SaveToHDRFile(img[0], destName.c_str());
                break;

            case CODEC_PPM:
                hr = SaveToPortablePixMap(img[0], destName.c_str());
                break;

            case CODEC_PFM:
                hr = SaveToPortablePixMapHDR(img[0], destName.c_str());
                break;

            #ifdef USE_OPENEXR
            case CODEC_EXR:
                hr = SaveToEXRFile(img[0], destName.c_str());
                break;
            #endif
            #ifdef USE_LIBJPEG
            case CODEC_JPEG:
                hr = SaveToJPEGFile(img[0], destName.c_str());
                break;
            #endif
            #ifdef USE_LIBPNG
            case CODEC_PNG:
                hr = SaveToPNGFile(img[0], destName.c_str());
                break;
            #endif

            default:
                {
                    const WICCodecs codec = (FileType == CODEC_HDP || FileType == CODEC_JXR) ? WIC_CODEC_WMP : static_cast<WICCodecs>(FileType);
                    const size_t nimages = (dwOptions & (uint64_t(1) << OPT_WIC_MULTIFRAME)) ? nimg : 1;
                    hr = SaveToWICFile(img, nimages, WIC_FLAGS_NONE, GetWICCodec(codec), destName.c_str(), nullptr,
                        [&](IPropertyBag2* props)
                        {
                            const bool wicLossless = (dwOptions & (uint64_t(1) << OPT_WIC_LOSSLESS)) != 0;

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
                                    std::ignore = props->Write(1, &options, &varValues);
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
                                    std::ignore = props->Write(1, &options, &varValues);
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
                                    std::ignore = props->Write(1, &options, &varValues);
                                }
                                break;
                            }
                        });
                }
                break;
            }

            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                retVal = 1;
                if ((hr == static_cast<HRESULT>(0xc00d5212) /* MF_E_TOPO_CODEC_NOT_FOUND */) && (FileType == WIC_CODEC_HEIF))
                {
                    wprintf(L"INFO: This format requires installing the HEIF Image Extensions - https://aka.ms/heif\n");
                }
                continue;
            }
            wprintf(L"\n");
        }
    }

    if (sizewarn)
    {
        wprintf(L"\nWARNING: Target size exceeds maximum size for feature level (%u)\n", maxSize);
    }

    if (nonpow2warn && maxSize <= 4096)
    {
        // Only emit this warning if ran with -fl set to a 9.x feature level
        wprintf(L"\nWARNING: Not all feature levels support non-power-of-2 textures with mipmaps\n");
    }

    if (non4bc)
        wprintf(L"\nWARNING: Direct3D requires BC image to be multiple of 4 in width & height\n");

    if (dwOptions & (uint64_t(1) << OPT_TIMING))
    {
        LARGE_INTEGER qpcEnd = {};
        std::ignore = QueryPerformanceCounter(&qpcEnd);

        const LONGLONG delta = qpcEnd.QuadPart - qpcStart.QuadPart;
        wprintf(L"\n Processing time: %f seconds\n", double(delta) / double(qpcFreq.QuadPart));
    }

    return retVal;
}
