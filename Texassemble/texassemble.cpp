//--------------------------------------------------------------------------------------
// File: Texassemble.cpp
//
// DirectX Texture assembler for cube maps, volume maps, and arrays
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

#if __cplusplus < 201703L
#error Requires C++17 (and /Zc:__cplusplus with MSVC)
#endif

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <wrl/client.h>

#include <dxgiformat.h>

#include <DirectXPackedVector.h>
#include <wincodec.h>

#ifdef  _MSC_VER
#pragma warning(disable : 4619 4616 26812)
#endif

#include "DirectXTex.h"

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

#define TOOL_VERSION DIRECTX_TEX_VERSION
#include "CmdLineHelpers.h"

using namespace Helpers;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    const wchar_t* g_ToolName = L"texassemble";
    const wchar_t* g_Description = L"Microsoft (R) DirectX Texture Assembler [DirectXTex]";

    enum COMMANDS : uint32_t
    {
        CMD_CUBE = 1,
        CMD_VOLUME,
        CMD_ARRAY,
        CMD_CUBEARRAY,
        CMD_H_CROSS,
        CMD_V_CROSS,
        CMD_V_CROSS_FNZ,
        CMD_H_TEE,
        CMD_H_STRIP,
        CMD_V_STRIP,
        CMD_MERGE,
        CMD_GIF,
        CMD_ARRAY_STRIP,
        CMD_CUBE_FROM_HC,
        CMD_CUBE_FROM_VC,
        CMD_CUBE_FROM_VC_FNZ,
        CMD_CUBE_FROM_HT,
        CMD_CUBE_FROM_HS,
        CMD_CUBE_FROM_VS,
        CMD_FROM_MIPS,
        CMD_MAX
    };

    enum OPTIONS : uint32_t
    {
        OPT_RECURSIVE = 1,
        OPT_TOLOWER,
        OPT_OVERWRITE,
        OPT_USE_DX10,
        OPT_NOLOGO,
        OPT_SEPALPHA,
        OPT_NO_WIC,
        OPT_DEMUL_ALPHA,
        OPT_TA_WRAP,
        OPT_TA_MIRROR,
        OPT_TONEMAP,
        OPT_GIF_BGCOLOR,
        OPT_STRIP_MIPS,
        OPT_FLAGS_MAX,
        OPT_FILELIST,
        OPT_WIDTH,
        OPT_HEIGHT,
        OPT_FORMAT,
        OPT_FILTER,
        OPT_SRGBI,
        OPT_SRGBO,
        OPT_SRGB,
        OPT_OUTPUTFILE,
        OPT_FEATURE_LEVEL,
        OPT_SWIZZLE,
        OPT_VERSION,
        OPT_HELP,
    };

    static_assert(OPT_FLAGS_MAX <= 32, "dwOptions is a unsigned int bitfield");

    //////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////

    const SValue<uint32_t> g_pCommands[] =
    {
        { L"cube",              CMD_CUBE },
        { L"volume",            CMD_VOLUME },
        { L"array",             CMD_ARRAY },
        { L"cubearray",         CMD_CUBEARRAY },
        { L"h-cross",           CMD_H_CROSS },
        { L"v-cross",           CMD_V_CROSS },
        { L"v-cross-fnz",       CMD_V_CROSS_FNZ },
        { L"h-tee",             CMD_H_TEE },
        { L"h-strip",           CMD_H_STRIP },
        { L"v-strip",           CMD_V_STRIP },
        { L"merge",             CMD_MERGE },
        { L"gif",               CMD_GIF },
        { L"array-strip",       CMD_ARRAY_STRIP },
        { L"cube-from-hc",      CMD_CUBE_FROM_HC },
        { L"cube-from-vc",      CMD_CUBE_FROM_VC },
        { L"cube-from-vc-fnz",  CMD_CUBE_FROM_VC_FNZ },
        { L"cube-from-ht",      CMD_CUBE_FROM_HT },
        { L"cube-from-hs",      CMD_CUBE_FROM_HS },
        { L"cube-from-vs",      CMD_CUBE_FROM_VS },
        { L"from-mips",         CMD_FROM_MIPS },
        { nullptr,          0 }
    };

    const SValue<uint32_t> g_pOptions[] =
    {
        { L"r",         OPT_RECURSIVE },
        { L"flist",     OPT_FILELIST },
        { L"w",         OPT_WIDTH },
        { L"h",         OPT_HEIGHT },
        { L"f",         OPT_FORMAT },
        { L"if",        OPT_FILTER },
        { L"srgbi",     OPT_SRGBI },
        { L"srgbo",     OPT_SRGBO },
        { L"srgb",      OPT_SRGB },
        { L"o",         OPT_OUTPUTFILE },
        { L"l",         OPT_TOLOWER },
        { L"y",         OPT_OVERWRITE },
        { L"dx10",      OPT_USE_DX10 },
        { L"nologo",    OPT_NOLOGO },
        { L"sepalpha",  OPT_SEPALPHA },
        { L"nowic",     OPT_NO_WIC },
        { L"alpha",     OPT_DEMUL_ALPHA },
        { L"wrap",      OPT_TA_WRAP },
        { L"mirror",    OPT_TA_MIRROR },
        { L"fl",        OPT_FEATURE_LEVEL },

        // Deprecated options (recommend using new -- alternatives)
        { L"tonemap",   OPT_TONEMAP },
        { L"bgcolor",   OPT_GIF_BGCOLOR },
        { L"swizzle",   OPT_SWIZZLE },
        { L"stripmips", OPT_STRIP_MIPS },

        { nullptr,      0 }
    };

    const SValue<uint32_t> g_pOptionsLong[] =
    {
        { L"feature-level",         OPT_FEATURE_LEVEL },
        { L"file-list",             OPT_FILELIST },
        { L"format",                OPT_FORMAT },
        { L"gif-bg-color",          OPT_GIF_BGCOLOR },
        { L"height",                OPT_HEIGHT },
        { L"help",                  OPT_HELP },
        { L"image-filter",          OPT_FILTER },
        { L"overwrite",             OPT_OVERWRITE },
        { L"separate-alpha",        OPT_SEPALPHA },
        { L"srgb-in",               OPT_SRGBI },
        { L"srgb-out",              OPT_SRGBO },
        { L"strip-mips",            OPT_STRIP_MIPS },
        { L"swizzle",               OPT_SWIZZLE },
        { L"to-lowercase",          OPT_TOLOWER },
        { L"tonemap",               OPT_TONEMAP },
        { L"version",               OPT_VERSION },
        { L"width",                 OPT_WIDTH },
        { nullptr,                  0 }
    };

    #define DEFFMT(fmt) { L## #fmt, DXGI_FORMAT_ ## fmt }

    const SValue<DXGI_FORMAT> g_pFormats[] =
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

        // D3D11on12 format
        { L"A4B4G4R4_UNORM", DXGI_FORMAT(191) },

        { nullptr, DXGI_FORMAT_UNKNOWN }
    };

    #undef DEFFMT

    const SValue<DXGI_FORMAT> g_pFormatAliases[] =
    {
        { L"RGBA", DXGI_FORMAT_R8G8B8A8_UNORM },
        { L"BGRA", DXGI_FORMAT_B8G8R8A8_UNORM },
        { L"BGR",  DXGI_FORMAT_B8G8R8X8_UNORM },

        { L"FP16", DXGI_FORMAT_R16G16B16A16_FLOAT },
        { L"FP32", DXGI_FORMAT_R32G32B32A32_FLOAT },

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

    constexpr uint32_t CODEC_DDS = 0xFFFF0001;
    constexpr uint32_t CODEC_TGA = 0xFFFF0002;
    constexpr uint32_t CODEC_HDR = 0xFFFF0005;

    #ifdef USE_OPENEXR
    constexpr uint32_t CODEC_EXR = 0xFFFF0008;
    #endif
    #ifdef USE_LIBJPEG
    constexpr uint32_t CODEC_JPEG = 0xFFFF0009;
    #endif
    #ifdef USE_LIBPNG
    constexpr uint32_t CODEC_PNG = 0xFFFF000A;
    #endif

    const SValue<uint32_t> g_pExtFileTypes[] =
    {
        { L".BMP",  WIC_CODEC_BMP  },
    #ifdef USE_LIBJPEG
        { L".JPG",  CODEC_JPEG     },
        { L".JPEG", CODEC_JPEG     },
    #else
        { L".JPG",  WIC_CODEC_JPEG },
        { L".JPEG", WIC_CODEC_JPEG },
    #endif
    #ifdef USE_LIBPNG
        { L".PNG",  CODEC_PNG      },
    #else
        { L".PNG",  WIC_CODEC_PNG  },
    #endif
        { L".DDS",  CODEC_DDS      },
        { L".TGA",  CODEC_TGA      },
        { L".HDR",  CODEC_HDR      },
        { L".TIF",  WIC_CODEC_TIFF },
        { L".TIFF", WIC_CODEC_TIFF },
        { L".WDP",  WIC_CODEC_WMP  },
        { L".HDP",  WIC_CODEC_WMP  },
        { L".JXR",  WIC_CODEC_WMP  },
    #ifdef USE_OPENEXR
        { L".EXR",  CODEC_EXR      },
    #endif
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

    const SValue<uint32_t> g_pFeatureLevelsCube[] = // valid feature levels for -fl for maximum cubemap size
    {
        { L"9.1",  512 },
        { L"9.2",  512 },
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

    const SValue<uint32_t> g_pFeatureLevelsArray[] = // valid feature levels for -fl for maximum array size
    {
        { L"9.1",  1 },
        { L"9.2",  1 },
        { L"9.3",  1 },
        { L"10.0", 512 },
        { L"10.1", 512 },
        { L"11.0", 2048 },
        { L"11.1", 2048 },
        { L"12.0", 2048 },
        { L"12.1", 2048 },
        { L"12.2", 2048 },
        { nullptr, 0 },
    };

    const SValue<uint32_t> g_pFeatureLevelsVolume[] = // valid feature levels for -fl for maximum depth size
    {
        { L"9.1",  256 },
        { L"9.2",  256 },
        { L"9.3",  256 },
        { L"10.0", 2048 },
        { L"10.1", 2048 },
        { L"11.0", 2048 },
        { L"11.1", 2048 },
        { L"12.0", 2048 },
        { L"12.1", 2048 },
        { L"12.2", 2048 },
        { nullptr, 0 },
    };
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

HRESULT LoadAnimatedGif(const wchar_t* szFile,
    std::vector<std::unique_ptr<ScratchImage>>& loadedImages,
    bool usebgcolor);

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

namespace
{
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
        PrintFormat(info.format, g_pFormats);

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

    void PrintUsage()
    {
        PrintLogo(false, g_ToolName, g_Description);

        static const wchar_t* const s_usage =
            L"Usage: texassemble <command> <options> [--] <files>\n"
            L"\nCOMMANDS\n"
            L"   cube                create cubemap\n"
            L"   volume              create volume map\n"
            L"   array               create texture array\n"
            L"   cubearray           create cubemap array\n"
            L"   h-cross or v-cross  create a cross image from a cubemap\n"
            L"   v-cross-fnz         create a cross image flipping the -Z face\n"
            L"   h-tee               create a 'T' image from a cubemap\n"
            L"   h-strip or v-strip  create a strip image from a cubemap\n"
            L"   array-strip         create a strip image from a 1D/2D array\n"
            L"   merge               create texture from rgb image and alpha image\n"
            L"   gif                 create array from animated gif\n"
            L"   cube-from-hc        create cubemap from a h-cross image\n"
            L"   cube-from-vc        create cubemap from a v-cross image\n"
            L"   cube-from-vc-fnz    create cubemap from a v-cross image flipping the -Z face\n"
            L"   cube-from-ht        create cubemap from a h-tee image\n"
            L"   cube-from-hs        create cubemap from a h-strip image\n"
            L"   cube-from-vs        create cubemap from a v-strip image\n"
            L"\nOPTIONS\n"
            L"   -r                  wildcard filename search is recursive\n"
            L"   -flist <filename>, --file-list <filename>\n"
            L"                       use text file with a list of input files (one per line)\n"
            L"\n"
            L"   -w <n>, --width <n>                     width for output\n"
            L"   -h <n>, --height <n>                    height for output\n"
            L"   -f <format>, --format <format>          pixel format for output\n"
            L"\n"
            L"   -if <filter>, --image-filter <filter>   image filtering\n"
            L"   -srgb{i|o}, --srgb-in, --srgb-out       sRGB {input, output}\n"
            L"\n"
            L"   -o <filename>                           output filename\n"
            L"   -l, --to-lowercase                      force output filename to lower case\n"
            L"   -y, --overwrite                         overwrite existing output file (if any)\n"
            L"\n"
            L"   -sepalpha, --separate-alpha   resize/generate mips alpha channel separately from color channels\n"
            L"\n"
            L"   -nowic              Force non-WIC filtering\n"
            L"   -wrap, -mirror      texture addressing mode (wrap, mirror, or clamp)\n"
            L"   -alpha              convert premultiplied alpha to straight alpha\n"
            L"   -dx10               Force use of 'DX10' extended header\n"
            L"   -nologo             suppress copyright message\n"
            L"\n"
            L"   -fl <feature-level>, --feature-level <feature-level>\n"
            L"                       Set maximum feature level target (defaults to 11.0)\n"
            L"\n"
            L"   -tonemap            Apply a tonemap operator based on maximum luminance\n"
            L"\n"
            L"                       (gif only)\n"
            L"   --gif-bg-color      Use background color instead of transparency\n"
            L"\n"
            L"                       (merge only)\n"
            L"   --swizzle <rgba>    Select channels for merge (defaults to rgbB)\n"
            L"\n"
            L"                       (cube, volume, array, cubearray, merge only)\n"
            L"   --strip-mips        Use only base image from input dds files\n"
            L"\n"
            L"   '-- ' is needed if any input filepath starts with the '-' or '/' character\n";

        wprintf(L"%ls", s_usage);

        wprintf(L"\n   <format>: ");
        PrintList(13, g_pFormats);
        wprintf(L"      ");
        PrintList(13, g_pFormatAliases);

        wprintf(L"\n   <filter>: ");
        PrintList(13, g_pFilters);

        wprintf(L"\n   <feature-level>: ");
        PrintList(13, g_pFeatureLevels);
    }

    HRESULT SaveImageFile(const Image& img, uint32_t fileType, const wchar_t* szOutputFile)
    {
        switch (fileType)
        {
        case CODEC_DDS:
            return SaveToDDSFile(img, DDS_FLAGS_NONE, szOutputFile);

        case CODEC_TGA:
            return SaveToTGAFile(img, TGA_FLAGS_NONE, szOutputFile);

        case CODEC_HDR:
            return SaveToHDRFile(img, szOutputFile);

    #ifdef USE_OPENEXR
        case CODEC_EXR:
            return SaveToEXRFile(img, szOutputFile);
    #endif
    #ifdef USE_LIBJPEG
        case CODEC_JPEG:
            return SaveToJPEGFile(img, szOutputFile);
    #endif
    #ifdef USE_LIBPNG
        case CODEC_PNG:
            return SaveToPNGFile(img, szOutputFile);
    #endif

        default:
            {
                HRESULT hr = SaveToWICFile(img, WIC_FLAGS_NONE, GetWICCodec(static_cast<WICCodecs>(fileType)), szOutputFile);
                if ((hr == static_cast<HRESULT>(0xc00d5212) /* MF_E_TOPO_CODEC_NOT_FOUND */) && (fileType == WIC_CODEC_HEIF))
                {
                    wprintf(L"\nINFO: This format requires installing the HEIF Image Extensions - https://aka.ms/heif\n");
                }
                return hr;
            }
        }
    }

    bool ParseSwizzleMask(
        _In_reads_(4) const wchar_t* mask,
        _Out_writes_(4) uint32_t* permuteElements,
        _Out_writes_(4) uint32_t* zeroElements,
        _Out_writes_(4) uint32_t* oneElements)
    {
        if (!mask || !permuteElements || !zeroElements || !oneElements)
            return false;

        if (!mask[0])
            return false;

        for (uint32_t j = 0; j < 4; ++j)
        {
            if (!mask[j])
                break;

            switch (mask[j])
            {
            case L'r':
            case L'x':
                for (uint32_t k = j; k < 4; ++k)
                {
                    permuteElements[k] = 0;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'R':
            case L'X':
                for (uint32_t k = j; k < 4; ++k)
                {
                    permuteElements[k] = 4;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'g':
            case L'y':
                for (uint32_t k = j; k < 4; ++k)
                {
                    permuteElements[k] = 1;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'G':
            case L'Y':
                for (uint32_t k = j; k < 4; ++k)
                {
                    permuteElements[k] = 5;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'b':
            case L'z':
                for (uint32_t k = j; k < 4; ++k)
                {
                    permuteElements[k] = 2;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'B':
            case L'Z':
                for (uint32_t k = j; k < 4; ++k)
                {
                    permuteElements[k] = 6;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'a':
            case L'w':
                for (uint32_t k = j; k < 4; ++k)
                {
                    permuteElements[k] = 3;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'A':
            case L'W':
                for (uint32_t k = j; k < 4; ++k)
                {
                    permuteElements[k] = 7;
                    zeroElements[k] = 0;
                    oneElements[k] = 0;
                }
                break;

            case L'0':
                for (uint32_t k = j; k < 4; ++k)
                {
                    permuteElements[k] = k;
                    zeroElements[k] = 1;
                    oneElements[k] = 0;
                }
                break;

            case L'1':
                for (uint32_t k = j; k < 4; ++k)
                {
                    permuteElements[k] = k;
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

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    TEX_FILTER_FLAGS dwFilter = TEX_FILTER_DEFAULT;
    TEX_FILTER_FLAGS dwSRGB = TEX_FILTER_DEFAULT;
    TEX_FILTER_FLAGS dwFilterOpts = TEX_FILTER_DEFAULT;
    uint32_t fileType = WIC_CODEC_BMP;
    uint32_t maxSize = 16384;
    uint32_t maxCube = 16384;
    uint32_t maxArray = 2048;
    uint32_t maxVolume = 2048;

    // DXTex's Open Alpha onto Surface always loaded alpha from the blue channel
    uint32_t permuteElements[4] = { 0, 1, 2, 6 };
    uint32_t zeroElements[4] = {};
    uint32_t oneElements[4] = {};

    std::wstring outputFile;

    // Set locale for output since GetErrorDesc can get localized strings.
    std::locale::global(std::locale(""));

    // Initialize COM (needed for WIC)
    HRESULT hr = hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        wprintf(L"Failed to initialize COM (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
        return 1;
    }

    // Process command line
    if (argc < 2)
    {
        PrintUsage();
        return 0;
    }

    // check for these before the command
    if (('-' == argv[1][0]) && ('-' == argv[1][1]))
    {
        if (!_wcsicmp(argv[1], L"--version"))
        {
            PrintLogo(true, g_ToolName, g_Description);
            return 0;
        }
        else if (!_wcsicmp(argv[1], L"--help"))
        {
            PrintUsage();
            return 0;
        }
    }

    const uint32_t dwCommand = LookupByName(argv[1], g_pCommands);
    switch (dwCommand)
    {
    case CMD_CUBE:
    case CMD_VOLUME:
    case CMD_ARRAY:
    case CMD_CUBEARRAY:
    case CMD_H_CROSS:
    case CMD_V_CROSS:
    case CMD_V_CROSS_FNZ:
    case CMD_H_TEE:
    case CMD_H_STRIP:
    case CMD_V_STRIP:
    case CMD_MERGE:
    case CMD_GIF:
    case CMD_ARRAY_STRIP:
    case CMD_CUBE_FROM_HC:
    case CMD_CUBE_FROM_VC:
    case CMD_CUBE_FROM_VC_FNZ:
    case CMD_CUBE_FROM_HT:
    case CMD_CUBE_FROM_HS:
    case CMD_CUBE_FROM_VS:
    case CMD_FROM_MIPS:
        break;

    default:
        wprintf(L"Must use one of: ");
        PrintList(4, g_pCommands);
        return 1;
    }

    uint32_t dwOptions = 0;
    std::list<SConversion> conversion;
    bool allowOpts = true;

    for (int iArg = 2; iArg < argc; ++iArg)
    {
        PWSTR pArg = argv[iArg];

        if (allowOpts && (('-' == pArg[0]) || ('/' == pArg[0])))
        {
            uint32_t dwOption = 0;
            PWSTR pValue = nullptr;

            if (('-' == pArg[0]) && ('-' == pArg[1]))
            {
                if (pArg[2] == 0)
                {
                    // "-- " is the POSIX standard for "end of options" marking to escape the '-' and '/' characters at the start of filepaths.
                    allowOpts = false;
                    continue;
                }
                else
                {
                    pArg += 2;

                    for (pValue = pArg; *pValue && (':' != *pValue) && ('=' != *pValue); ++pValue);

                    if (*pValue)
                        *pValue++ = 0;

                    dwOption = LookupByName(pArg, g_pOptionsLong);
                }
            }
            else
            {
                pArg++;

                for (pValue = pArg; *pValue && (':' != *pValue) && ('=' != *pValue); ++pValue);

                if (*pValue)
                    *pValue++ = 0;

                dwOption = LookupByName(pArg, g_pOptions);

                if (!dwOption)
                {
                    if (LookupByName(pArg, g_pOptionsLong))
                    {
                        wprintf(L"ERROR: did you mean `--%ls` (with two dashes)?\n", pArg);
                        return 1;
                    }
                }
            }

            switch (dwOption)
            {
            case 0:
                wprintf(L"ERROR: Unknown option: `%ls`\n\nUse %ls --help\n", pArg, g_ToolName);
                return 1;

            case OPT_FILELIST:
            case OPT_WIDTH:
            case OPT_HEIGHT:
            case OPT_FORMAT:
            case OPT_FILTER:
            case OPT_SRGBI:
            case OPT_SRGBO:
            case OPT_SRGB:
            case OPT_OUTPUTFILE:
            case OPT_FEATURE_LEVEL:
            case OPT_SWIZZLE:
                // These don't use flag bits
                break;

            case OPT_VERSION:
                PrintLogo(true, g_ToolName, g_Description);
                return 0;

            case OPT_HELP:
                PrintUsage();
                return 0;

            default:
                if (dwOptions & (UINT32_C(1) << dwOption))
                {
                    wprintf(L"ERROR: Duplicate option: `%ls`\n\n", pArg);
                    return 1;
                }

                dwOptions |= (UINT32_C(1) << dwOption);
                break;
            }

            // Handle options with additional value parameter
            switch (dwOption)
            {
            case OPT_FILELIST:
            case OPT_WIDTH:
            case OPT_HEIGHT:
            case OPT_FORMAT:
            case OPT_FILTER:
            case OPT_OUTPUTFILE:
            case OPT_FEATURE_LEVEL:
            case OPT_SWIZZLE:
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

            default:
                break;
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
                dwFilter = static_cast<TEX_FILTER_FLAGS>(LookupByName(pValue, g_pFilters));
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
                    std::filesystem::path path(pValue);
                    outputFile = path.make_preferred().native();

                    fileType = LookupByName(path.extension().c_str(), g_pExtFileTypes);

                    switch (dwCommand)
                    {
                    case CMD_H_CROSS:
                    case CMD_V_CROSS:
                    case CMD_V_CROSS_FNZ:
                    case CMD_H_TEE:
                    case CMD_H_STRIP:
                    case CMD_V_STRIP:
                    case CMD_MERGE:
                    case CMD_ARRAY_STRIP:
                    case CMD_FROM_MIPS:
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

            case OPT_FEATURE_LEVEL:
                maxSize = LookupByName(pValue, g_pFeatureLevels);
                maxCube = LookupByName(pValue, g_pFeatureLevelsCube);
                maxArray = LookupByName(pValue, g_pFeatureLevelsArray);
                maxVolume = LookupByName(pValue, g_pFeatureLevelsVolume);
                if (!maxSize || !maxCube || !maxArray || !maxVolume)
                {
                    wprintf(L"Invalid value specified with -fl (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_GIF_BGCOLOR:
                if (dwCommand != CMD_GIF)
                {
                    wprintf(L"-bgcolor only applies to gif command\n");
                    return 1;
                }
                break;

            case OPT_SWIZZLE:
                if (dwCommand != CMD_MERGE)
                {
                    wprintf(L"-swizzle only applies to merge command\n");
                    return 1;
                }
                if (!*pValue || wcslen(pValue) > 4)
                {
                    wprintf(L"Invalid value specified with -swizzle (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                else if (!ParseSwizzleMask(pValue, permuteElements, zeroElements, oneElements))
                {
                    wprintf(L"-swizzle requires a 1 to 4 character mask composed of these letters: r, g, b, a, x, y, w, z, 0, 1.\n    Lowercase letters are from the first image, upper-case letters are from the second image.\n");
                    return 1;
                }
                break;

            case OPT_STRIP_MIPS:
                switch (dwCommand)
                {
                case CMD_CUBE:
                case CMD_VOLUME:
                case CMD_ARRAY:
                case CMD_CUBEARRAY:
                case CMD_MERGE:
                    break;

                default:
                    wprintf(L"-stripmips only applies to cube, volume, array, cubearray, or merge commands\n");
                    return 1;
                }
                break;

            default:
                break;
            }
        }
        else if (wcspbrk(pArg, L"?*") != nullptr)
        {
            const size_t count = conversion.size();
            std::filesystem::path path(pArg);
            SearchForFiles(path.make_preferred(), conversion, (dwOptions & (UINT32_C(1) << OPT_RECURSIVE)) != 0, nullptr);
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

    if (~dwOptions & (UINT32_C(1) << OPT_NOLOGO))
        PrintLogo(false, g_ToolName, g_Description);

    switch (dwCommand)
    {
    case CMD_H_CROSS:
    case CMD_V_CROSS:
    case CMD_V_CROSS_FNZ:
    case CMD_H_TEE:
    case CMD_H_STRIP:
    case CMD_V_STRIP:
    case CMD_GIF:
    case CMD_ARRAY_STRIP:
    case CMD_CUBE_FROM_HC:
    case CMD_CUBE_FROM_VC:
    case CMD_CUBE_FROM_VC_FNZ:
    case CMD_CUBE_FROM_HT:
    case CMD_CUBE_FROM_HS:
    case CMD_CUBE_FROM_VS:
        if (conversion.size() > 1)
        {
            wprintf(L"ERROR: cross/strip/gif/cube-from-* output only accepts 1 input file\n");
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

    default:
        break;
    }

    // Convert images
    size_t images = 0;

    std::vector<std::unique_ptr<ScratchImage>> loadedImages;

    if (dwCommand == CMD_GIF)
    {
        std::filesystem::path curpath(conversion.front().szSrc);

        wprintf(L"reading %ls", curpath.c_str());
        fflush(stdout);

        if (outputFile.empty())
        {
            outputFile = curpath.stem().concat(L".dds").native();
        }

        hr = LoadAnimatedGif(curpath.c_str(), loadedImages, (dwOptions & (UINT32_C(1) << OPT_GIF_BGCOLOR)) != 0);
        if (FAILED(hr))
        {
            wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
            return 1;
        }
    }
    else
    {
        size_t conversionIndex = 0;
        for (auto pConv = conversion.begin(); pConv != conversion.end(); ++pConv)
        {
            std::filesystem::path curpath(pConv->szSrc);
            auto const ext = curpath.extension();

            // Load source image
            if (pConv != conversion.begin())
                wprintf(L"\n");
            else if (outputFile.empty())
            {
                switch (dwCommand)
                {
                case CMD_H_CROSS:
                case CMD_V_CROSS:
                case CMD_V_CROSS_FNZ:
                case CMD_H_TEE:
                case CMD_H_STRIP:
                case CMD_V_STRIP:
                case CMD_ARRAY_STRIP:
                    outputFile = curpath.stem().concat(L".bmp").native();
                    break;

                default:
                    if (_wcsicmp(curpath.extension().c_str(), L".dds") == 0)
                    {
                        wprintf(L"ERROR: Need to specify output file via -o\n");
                        return 1;
                    }

                    outputFile = curpath.stem().concat(L".dds").native();
                    break;
                }
            }

            wprintf(L"reading %ls", curpath.c_str());
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
            case CMD_V_CROSS_FNZ:
            case CMD_H_TEE:
            case CMD_H_STRIP:
            case CMD_V_STRIP:
                if (_wcsicmp(ext.c_str(), L".dds") == 0)
                {
                    hr = LoadFromDDSFile(curpath.c_str(), DDS_FLAGS_ALLOW_LARGE_FILES, &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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

            case CMD_ARRAY_STRIP:
                if (_wcsicmp(ext.c_str(), L".dds") == 0)
                {
                    hr = LoadFromDDSFile(curpath.c_str(), DDS_FLAGS_ALLOW_LARGE_FILES, &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                        return 1;
                    }

                    if (info.dimension == TEX_DIMENSION_TEXTURE3D || info.arraySize < 2 || info.IsCubemap())
                    {
                        wprintf(L"\nERROR: Input must be a 1D/2D array\n");
                        return 1;
                    }
                }
                else
                {
                    wprintf(L"\nERROR: Input must be a dds of a 1D/2D array\n");
                    return 1;
                }
                break;

            default:
                if (_wcsicmp(ext.c_str(), L".dds") == 0)
                {
                    hr = LoadFromDDSFile(curpath.c_str(), DDS_FLAGS_ALLOW_LARGE_FILES, &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                        return 1;
                    }

                    if (info.IsVolumemap() || info.IsCubemap())
                    {
                        wprintf(L"\nERROR: Can't assemble complex surfaces\n");
                        return 1;
                    }
                    else if ((info.mipLevels > 1) && ((dwOptions & (UINT32_C(1) << OPT_STRIP_MIPS)) == 0))
                    {
                        switch (dwCommand)
                        {
                        case CMD_CUBE:
                        case CMD_VOLUME:
                        case CMD_ARRAY:
                        case CMD_CUBEARRAY:
                        case CMD_MERGE:
                            wprintf(L"\nERROR: Can't assemble using input mips. To ignore mips, try again with -stripmips\n");
                            return 1;

                        default:
                            break;
                       }
                    }
                }
                else if (_wcsicmp(ext.c_str(), L".tga") == 0)
                {
                    TGA_FLAGS tgaFlags = (IsBGR(format)) ? TGA_FLAGS_BGR : TGA_FLAGS_NONE;

                    hr = LoadFromTGAFile(curpath.c_str(), tgaFlags, &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                        return 1;
                    }
                }
                else if (_wcsicmp(ext.c_str(), L".hdr") == 0)
                {
                    hr = LoadFromHDRFile(curpath.c_str(), &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                        return 1;
                    }
                }
            #ifdef USE_OPENEXR
                else if (_wcsicmp(ext.c_str(), L".exr") == 0)
                {
                    hr = LoadFromEXRFile(curpath.c_str(), &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                        return 1;
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
                        return 1;
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
                        return 1;
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

                    hr = LoadFromWICFile(curpath.c_str(), WIC_FLAGS_ALL_FRAMES | dwFilter, &info, *image);
                    if (FAILED(hr))
                    {
                        wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
                    wprintf(L" FAILED [converttosingleplane] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                    return 1;
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
                const size_t nimg = image->GetImageCount();

                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                hr = Decompress(img, nimg, info, DXGI_FORMAT_UNKNOWN /* picks good default */, *timage.get());
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [decompress] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                    return 1;
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

            // --- Strip Mips (if requested) -----------------------------------------------
            if ((info.mipLevels > 1) && (dwOptions & (UINT32_C(1) << OPT_STRIP_MIPS)))
            {
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
                    wprintf(L" FAILED [copy to single level] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
                            wprintf(L" FAILED [copy to single level] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
            }

            // --- Undo Premultiplied Alpha (if requested) ---------------------------------
            if ((dwOptions & (UINT32_C(1) << OPT_DEMUL_ALPHA))
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
                        wprintf(L" FAILED [demultiply alpha] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                        return 1;
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
            size_t targetWidth = width;
            size_t targetHeight = height;
            if (dwCommand == CMD_FROM_MIPS)
            {
                size_t mipdiv = 1;
                for (size_t i = 0; i < conversionIndex; ++i)
                {
                    mipdiv = mipdiv + mipdiv;
                }

                targetWidth /= mipdiv;
                targetHeight /= mipdiv;
                if (targetWidth == 0 || targetHeight == 0)
                {
                    wprintf(L"\nERROR: Too many input mips provided. For the dimensions of the first mip provided, only %zu input mips can be used.\n", conversionIndex);
                    return 1;
                }
            }
            if (info.width != targetWidth || info.height != targetHeight)
            {
                std::unique_ptr<ScratchImage> timage(new (std::nothrow) ScratchImage);
                if (!timage)
                {
                    wprintf(L"\nERROR: Memory allocation failed\n");
                    return 1;
                }

                hr = Resize(image->GetImages(), image->GetImageCount(), image->GetMetadata(), targetWidth, targetHeight, dwFilter | dwFilterOpts, *timage.get());
                if (FAILED(hr))
                {
                    wprintf(L" FAILED [resize] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                    return 1;
                }

                auto& tinfo = timage->GetMetadata();

                assert(tinfo.width == targetWidth && tinfo.height == targetHeight && tinfo.mipLevels == 1);
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
            if (dwOptions & (UINT32_C(1) << OPT_TONEMAP))
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
                    wprintf(L" FAILED [convert] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                    return 1;
                }

                auto& tinfo = timage->GetMetadata();

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
            ++conversionIndex;
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
    case CMD_V_CROSS_FNZ:
    case CMD_H_TEE:
    case CMD_H_STRIP:
    case CMD_V_STRIP:
    case CMD_GIF:
    case CMD_CUBE_FROM_HC:
    case CMD_CUBE_FROM_VC:
    case CMD_CUBE_FROM_VC_FNZ:
    case CMD_CUBE_FROM_HT:
    case CMD_CUBE_FROM_HS:
    case CMD_CUBE_FROM_VS:
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
    case CMD_V_CROSS_FNZ:
    case CMD_H_TEE:
    case CMD_H_STRIP:
    case CMD_V_STRIP:
        {
            size_t twidth = 0;
            size_t theight = 0;

            switch (dwCommand)
            {
            case CMD_H_CROSS:
            case CMD_H_TEE:
                twidth = width * 4;
                theight = height * 3;
                break;

            case CMD_V_CROSS:
            case CMD_V_CROSS_FNZ:
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
                break;

            default:
                break;
            }

            ScratchImage result;
            hr = result.Initialize2D(format, twidth, theight, 1, 1);
            if (FAILED(hr))
            {
                wprintf(L"FAILED setting up result image (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }

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

                const Rect rect(0, 0, width, height);

                size_t offsetx = 0;
                size_t offsety = 0;
                TEX_FR_FLAGS flipRotate = TEX_FR_ROTATE0;

                switch (dwCommand)
                {
                case CMD_H_CROSS:
                    {
                        //    +Y
                        // -X +Z +X -Z
                        //    -Y

                        static const size_t s_offsetx[6] = { 2, 0, 1, 1, 1, 3 };
                        static const size_t s_offsety[6] = { 1, 1, 0, 2, 1, 1 };

                        offsetx = s_offsetx[index] * width;
                        offsety = s_offsety[index] * height;
                        break;
                    }

                case CMD_V_CROSS:
                    {
                        //    +Y
                        // -X +Z +X
                        //    -Y
                        //    -Z
                        static const size_t s_offsetx[6] = { 2, 0, 1, 1, 1, 1 };
                        static const size_t s_offsety[6] = { 1, 1, 0, 2, 1, 3 };

                        offsetx = s_offsetx[index] * width;
                        offsety = s_offsety[index] * height;
                        break;
                    }

                case CMD_V_CROSS_FNZ:
                    {
                        //    +Y
                        // -X +Z +X
                        //    -Y
                        //    -Z (flipped H/V)
                        static const size_t s_offsetx[6] = { 2, 0, 1, 1, 1, 1 };
                        static const size_t s_offsety[6] = { 1, 1, 0, 2, 1, 3 };

                        offsetx = s_offsetx[index] * width;
                        offsety = s_offsety[index] * height;

                        if (index == 5)
                        {
                            flipRotate = TEX_FR_ROTATE180;
                        }
                        break;
                    }

                case CMD_H_TEE:
                    {
                        // +Y
                        // +Z +X -Z -X
                        // -Y

                        static const size_t s_offsetx[6] = { 1, 3, 0, 0, 0, 2 };
                        static const size_t s_offsety[6] = { 1, 1, 0, 2, 1, 1 };

                        offsetx = s_offsetx[index] * width;
                        offsety = s_offsety[index] * height;
                        break;
                    }

                case CMD_H_STRIP:
                    // +X -X +Y -Y +Z -Z
                    offsetx = index * width;
                    break;

                case CMD_V_STRIP:
                    // +X
                    // -X
                    // +Y
                    // -Y
                    // +Z
                    // -Z
                    offsety = index * height;
                    break;

                default:
                    break;
                }

                if (flipRotate != TEX_FR_ROTATE0)
                {
                    ScratchImage tmp;
                    hr = FlipRotate(*img, flipRotate, tmp);
                    if (SUCCEEDED(hr))
                    {
                        hr = CopyRectangle(*tmp.GetImage(0,0,0), rect, *dest, dwFilter | dwFilterOpts, offsetx, offsety);
                    }
                }
                else
                {
                    hr = CopyRectangle(*img, rect, *dest, dwFilter | dwFilterOpts, offsetx, offsety);
                }

                if (FAILED(hr))
                {
                    wprintf(L"FAILED building result image (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                    return 1;
                }
            }

            // Write cross/strip
            wprintf(L"\nWriting %ls ", outputFile.c_str());
            PrintInfo(result.GetMetadata());
            wprintf(L"\n");
            fflush(stdout);

            if (dwOptions & (UINT32_C(1) << OPT_TOLOWER))
            {
                std::transform(outputFile.begin(), outputFile.end(), outputFile.begin(), towlower);
            }

            if (~dwOptions & (UINT32_C(1) << OPT_OVERWRITE))
            {
                if (GetFileAttributesW(outputFile.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    wprintf(L"\nERROR: Output file already exists, use -y to overwrite\n");
                    return 1;
                }
            }

            hr = SaveImageFile(*dest, fileType, outputFile.c_str());
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }
            break;
        }

    case CMD_MERGE:
        {
            // Capture data from our second source image
            ScratchImage tempImage;
            hr = Convert(*loadedImages[1]->GetImage(0, 0, 0), DXGI_FORMAT_R32G32B32A32_FLOAT,
                dwFilter | dwFilterOpts | dwSRGB, TEX_THRESHOLD_DEFAULT, tempImage);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [convert second input] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }

            const Image& img = *tempImage.GetImage(0, 0, 0);

            // Merge with our first source image
            const Image& rgb = *loadedImages[0]->GetImage(0, 0, 0);

            const XMVECTOR zc = XMVectorSelectControl(zeroElements[0], zeroElements[1], zeroElements[2], zeroElements[3]);
            const XMVECTOR oc = XMVectorSelectControl(oneElements[0], oneElements[1], oneElements[2], oneElements[3]);

            ScratchImage result;
            hr = TransformImage(rgb, [&, zc, oc](XMVECTOR* outPixels, const XMVECTOR* inPixels, size_t w, size_t y)
                {
                    const XMVECTOR *inPixels2 = reinterpret_cast<XMVECTOR*>(img.pixels + img.rowPitch * y);

                    for (size_t j = 0; j < w; ++j)
                    {
                        XMVECTOR pixel = XMVectorPermute(inPixels[j], inPixels2[j],
                            permuteElements[0], permuteElements[1], permuteElements[2], permuteElements[3]);
                        pixel = XMVectorSelect(pixel, g_XMZero, zc);
                        outPixels[j] = XMVectorSelect(pixel, g_XMOne, oc);
                    }
                }, result);
            if (FAILED(hr))
            {
                wprintf(L" FAILED [merge image] (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }

            // Write merged texture
            wprintf(L"\nWriting %ls ", outputFile.c_str());
            PrintInfo(result.GetMetadata());
            wprintf(L"\n");
            fflush(stdout);

            if (dwOptions & (UINT32_C(1) << OPT_TOLOWER))
            {
                std::transform(outputFile.begin(), outputFile.end(), outputFile.begin(), towlower);
            }

            if (~dwOptions & (UINT32_C(1) << OPT_OVERWRITE))
            {
                if (GetFileAttributesW(outputFile.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    wprintf(L"\nERROR: Output file already exists, use -y to overwrite\n");
                    return 1;
                }
            }

            hr = SaveImageFile(*result.GetImage(0, 0, 0), fileType, outputFile.c_str());
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }
            break;
        }

    case CMD_ARRAY_STRIP:
        {
            const size_t twidth = width;
            const size_t theight = height * images;

            ScratchImage result;
            hr = result.Initialize2D(format, twidth, theight, 1, 1);
            if (FAILED(hr))
            {
                wprintf(L"FAILED setting up result image (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }

            auto src = loadedImages.cbegin();
            auto dest = result.GetImage(0, 0, 0);

            for (size_t index = 0; index < images; ++index)
            {
                auto img = (*src)->GetImage(0, index, 0);
                if (!img)
                {
                    wprintf(L"FAILED: Unexpected error\n");
                    return 1;
                }

                const Rect rect(0, 0, width, height);

                constexpr size_t offsetx = 0;
                size_t offsety = 0;

                offsety = index * height;

                hr = CopyRectangle(*img, rect, *dest, dwFilter | dwFilterOpts, offsetx, offsety);
                if (FAILED(hr))
                {
                    wprintf(L"FAILED building result image (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                    return 1;
                }
            }

            // Write array strip
            wprintf(L"\nWriting %ls ", outputFile.c_str());
            PrintInfo(result.GetMetadata());
            wprintf(L"\n");
            fflush(stdout);

            if (dwOptions & (UINT32_C(1) << OPT_TOLOWER))
            {
                std::transform(outputFile.begin(), outputFile.end(), outputFile.begin(), towlower);
            }

            if (~dwOptions & (UINT32_C(1) << OPT_OVERWRITE))
            {
                if (GetFileAttributesW(outputFile.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    wprintf(L"\nERROR: Output file already exists, use -y to overwrite\n");
                    return 1;
                }
            }

            hr = SaveImageFile(*dest, fileType, outputFile.c_str());
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }
            break;
        }

    case CMD_CUBE_FROM_HC:
    case CMD_CUBE_FROM_VC:
    case CMD_CUBE_FROM_VC_FNZ:
    case CMD_CUBE_FROM_HT:
    case CMD_CUBE_FROM_HS:
    case CMD_CUBE_FROM_VS:
        {
            auto src = loadedImages.cbegin();
            auto img = (*src)->GetImage(0, 0, 0);
            size_t ratio_w = 1;
            size_t ratio_h = 1;

            switch (dwCommand)
            {
            case CMD_CUBE_FROM_HC:
            case CMD_CUBE_FROM_HT:
                ratio_w = 4;
                ratio_h = 3;
                break;

            case CMD_CUBE_FROM_VC:
            case CMD_CUBE_FROM_VC_FNZ:
                ratio_w = 3;
                ratio_h = 4;
                break;

            case CMD_CUBE_FROM_HS:
                ratio_w = 6;
                break;

            case CMD_CUBE_FROM_VS:
                ratio_h = 6;
                break;

            default:
                break;
            }

            size_t twidth = width / ratio_w;
            size_t theight = height / ratio_h;

            if (((width % ratio_w) != 0) || ((height % ratio_h) != 0))
            {
                wprintf(L"\nWARNING: %ls expects %zu:%zu aspect ratio\n", g_pCommands[dwCommand - 1].name, ratio_w, ratio_h);
            }

            if (twidth > maxCube || theight > maxCube)
            {
                wprintf(L"\nWARNING: Target size exceeds maximum cube dimensions for feature level (%u)\n", maxCube);
            }

            ScratchImage result;
            hr = result.InitializeCube(format, twidth, theight, 1, 1);
            if (FAILED(hr))
            {
                wprintf(L"FAILED setting up result image (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }

            for (size_t index = 0; index < 6; ++index)
            {
                size_t offsetx = 0;
                size_t offsety = 0;
                TEX_FR_FLAGS flipRotate = TEX_FR_ROTATE0;

                switch (dwCommand)
                {
                case CMD_CUBE_FROM_HC:
                    {
                        //    +Y
                        // -X +Z +X -Z
                        //    -Y

                        static const size_t s_offsetx[6] = { 2, 0, 1, 1, 1, 3 };
                        static const size_t s_offsety[6] = { 1, 1, 0, 2, 1, 1 };

                        offsetx = s_offsetx[index] * twidth;
                        offsety = s_offsety[index] * theight;
                        break;
                    }

                case CMD_CUBE_FROM_VC:
                    {
                        //    +Y
                        // -X +Z +X
                        //    -Y
                        //    -Z

                        static const size_t s_offsetx[6] = { 2, 0, 1, 1, 1, 1 };
                        static const size_t s_offsety[6] = { 1, 1, 0, 2, 1, 3 };

                        offsetx = s_offsetx[index] * twidth;
                        offsety = s_offsety[index] * theight;
                        break;
                    }

                case CMD_CUBE_FROM_VC_FNZ:
                    {
                        //    +Y
                        // -X +Z +X
                        //    -Y
                        //    -Z (flipped H/V)

                        static const size_t s_offsetx[6] = { 2, 0, 1, 1, 1, 1 };
                        static const size_t s_offsety[6] = { 1, 1, 0, 2, 1, 3 };

                        offsetx = s_offsetx[index] * twidth;
                        offsety = s_offsety[index] * theight;

                        if (index == 5)
                        {
                            flipRotate = TEX_FR_ROTATE180;
                        }
                        break;
                    }

                case CMD_CUBE_FROM_HT:
                    {
                        // +Y
                        // +Z +X -Z -X
                        // -Y

                        static const size_t s_offsetx[6] = { 1, 3, 0, 0, 0, 2 };
                        static const size_t s_offsety[6] = { 1, 1, 0, 2, 1, 1 };

                        offsetx = s_offsetx[index] * twidth;
                        offsety = s_offsety[index] * theight;
                        break;
                    }

                case CMD_CUBE_FROM_HS:
                    // +X -X +Y -Y +Z -Z
                    offsetx = index * twidth;
                    break;

                case CMD_CUBE_FROM_VS:
                    // +X
                    // -X
                    // +Y
                    // -Y
                    // +Z
                    // -Z
                    offsety = index * theight;
                    break;

                default:
                    break;
                }

                const Rect rect(offsetx, offsety, twidth, theight);
                const Image* dest = result.GetImage(0, index, 0);
                hr = CopyRectangle(*img, rect, *dest, dwFilter | dwFilterOpts, 0, 0);

                if (FAILED(hr))
                {
                    wprintf(L"FAILED building result image (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                    return 1;
                }

                if (flipRotate != TEX_FR_ROTATE0)
                {
                    ScratchImage tmp;
                    hr = FlipRotate(*dest, flipRotate, tmp);
                    if (SUCCEEDED(hr))
                    {
                        hr = CopyRectangle(*tmp.GetImage(0,0,0), Rect(0, 0, twidth, theight), *dest, dwFilter | dwFilterOpts, 0, 0);
                    }
                }
            }

            // Write texture
            wprintf(L"\nWriting %ls ", outputFile.c_str());
            PrintInfo(result.GetMetadata());
            wprintf(L"\n");
            fflush(stdout);

            if (dwOptions & (UINT32_C(1) << OPT_TOLOWER))
            {
                std::transform(outputFile.begin(), outputFile.end(), outputFile.begin(), towlower);
            }

            if (~dwOptions & (UINT32_C(1) << OPT_OVERWRITE))
            {
                if (GetFileAttributesW(outputFile.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    wprintf(L"\nERROR: Output file already exists, use -y to overwrite\n");
                    return 1;
                }
            }

            hr = SaveToDDSFile(result.GetImages(), result.GetImageCount(), result.GetMetadata(),
                (dwOptions & (UINT32_C(1) << OPT_USE_DX10)) ? (DDS_FLAGS_FORCE_DX10_EXT | DDS_FLAGS_FORCE_DX10_EXT_MISC2) : DDS_FLAGS_NONE,
                outputFile.c_str());
            if (FAILED(hr))
            {
                wprintf(L"\nFAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }
            break;
        }
    case CMD_FROM_MIPS:
        {
            auto src = loadedImages.cbegin();
            ScratchImage result;
            hr = result.Initialize2D(format, width, height, 1, images);
            if (FAILED(hr))
            {
                wprintf(L"FAILED setting up result image (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }
            size_t mipdiv = 1;
            size_t index = 0;
            for (auto it = src; it != loadedImages.cend(); ++it)
            {
                auto dest = result.GetImage(index, 0, 0);
                const ScratchImage* simage = it->get();
                assert(simage != nullptr);
                const Image* img = simage->GetImage(0, 0, 0);
                assert(img != nullptr);
                hr = CopyRectangle(*img, Rect(0, 0, width / mipdiv, height / mipdiv), *dest, dwFilter | dwFilterOpts, 0, 0);
                if (FAILED(hr))
                {
                    wprintf(L"FAILED building result image (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                    return 1;
                }
                index++;
                mipdiv *= 2;
            }
            // Write texture2D
            wprintf(L"\nWriting %ls ", outputFile.c_str());
            PrintInfo(result.GetMetadata());
            wprintf(L"\n");
            fflush(stdout);

            if (dwOptions & (UINT32_C(1) << OPT_TOLOWER))
            {
                std::transform(outputFile.begin(), outputFile.end(), outputFile.begin(), towlower);
            }

            if (~dwOptions & (UINT32_C(1) << OPT_OVERWRITE))
            {
                if (GetFileAttributesW(outputFile.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    wprintf(L"\nERROR: Output file already exists, use -y to overwrite\n");
                    return 1;
                }
            }

            hr = SaveToDDSFile(result.GetImages(), result.GetImageCount(), result.GetMetadata(),
                (dwOptions & (UINT32_C(1) << OPT_USE_DX10)) ? (DDS_FLAGS_FORCE_DX10_EXT | DDS_FLAGS_FORCE_DX10_EXT_MISC2) : DDS_FLAGS_NONE,
                outputFile.c_str());
            if (FAILED(hr))
            {
                wprintf(L"\nFAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
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
                assert(simage != nullptr);
                for (size_t j = 0; j < simage->GetMetadata().arraySize; ++j)
                {
                    const Image* img = simage->GetImage(0, j, 0);
                    assert(img != nullptr);
                    imageArray.push_back(*img);
                }
            }

            switch (dwCommand)
            {
            case CMD_CUBE:
                if (imageArray[0].width > maxCube || imageArray[0].height > maxCube)
                {
                    wprintf(L"\nWARNING: Target size exceeds maximum cube dimensions for feature level (%u)\n", maxCube);
                }
                break;

            case CMD_VOLUME:
                if (imageArray[0].width > maxVolume || imageArray[0].height > maxVolume || imageArray.size() > maxVolume)
                {
                    wprintf(L"\nWARNING: Target size exceeds volume extent for feature level (%u)\n", maxVolume);
                }
                break;

            case CMD_ARRAY:
                if (imageArray[0].width > maxSize || imageArray[0].height > maxSize || imageArray.size() > maxArray)
                {
                    wprintf(L"\nWARNING: Target size exceeds maximum size for feature level (size %u, array %u)\n", maxSize, maxArray);
                }
                break;

            case CMD_CUBEARRAY:
                if (imageArray[0].width > maxCube || imageArray[0].height > maxCube || imageArray.size() > maxArray)
                {
                    wprintf(L"\nWARNING: Target size exceeds maximum cube dimensions for feature level (size %u, array %u)\n", maxCube, maxArray);
                }
                break;

            default:
                if (imageArray[0].width > maxSize || imageArray[0].height > maxSize)
                {
                    wprintf(L"\nWARNING: Target size exceeds maximum size for feature level (%u)\n", maxSize);
                }
                break;
            }

            ScratchImage result;
            switch (dwCommand)
            {
            case CMD_VOLUME:
                hr = result.Initialize3DFromImages(&imageArray[0], imageArray.size());
                break;

            case CMD_ARRAY:
            case CMD_GIF:
                hr = result.InitializeArrayFromImages(&imageArray[0], imageArray.size(), (dwOptions & (UINT32_C(1) << OPT_USE_DX10)) != 0);
                break;

            case CMD_CUBE:
            case CMD_CUBEARRAY:
                hr = result.InitializeCubeFromImages(&imageArray[0], imageArray.size());
                break;

            default:
                break;
            }

            if (FAILED(hr))
            {
                wprintf(L"FAILED building result image (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }

            // Write texture
            wprintf(L"\nWriting %ls ", outputFile.c_str());
            PrintInfo(result.GetMetadata());
            wprintf(L"\n");
            fflush(stdout);

            if (dwOptions & (UINT32_C(1) << OPT_TOLOWER))
            {
                std::transform(outputFile.begin(), outputFile.end(), outputFile.begin(), towlower);
            }

            if (~dwOptions & (UINT32_C(1) << OPT_OVERWRITE))
            {
                if (GetFileAttributesW(outputFile.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    wprintf(L"\nERROR: Output file already exists, use -y to overwrite\n");
                    return 1;
                }
            }

            hr = SaveToDDSFile(result.GetImages(), result.GetImageCount(), result.GetMetadata(),
                (dwOptions & (UINT32_C(1) << OPT_USE_DX10)) ? (DDS_FLAGS_FORCE_DX10_EXT | DDS_FLAGS_FORCE_DX10_EXT_MISC2) : DDS_FLAGS_NONE,
                outputFile.c_str());
            if (FAILED(hr))
            {
                wprintf(L"\nFAILED (%08X%ls)\n", static_cast<unsigned int>(hr), GetErrorDesc(hr));
                return 1;
            }
            break;
        }
    }

    return 0;
}
