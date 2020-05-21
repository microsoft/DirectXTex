//--------------------------------------------------------------------------------------
// File: Texdiag.cpp
//
// DirectX Texture diagnostic tool
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

#include <dxgiformat.h>

#pragma warning(disable : 4619 4616 26812)

#include "DirectXTex.h"

//Uncomment to add support for OpenEXR (.exr)
//#define USE_OPENEXR

#ifdef USE_OPENEXR
// See <https://github.com/Microsoft/DirectXTex/wiki/Adding-OpenEXR> for details
#include "DirectXTexEXR.h"
#endif

using namespace DirectX;

enum COMMANDS
{
    CMD_INFO = 1,
    CMD_ANALYZE,
    CMD_COMPARE,
    CMD_DIFF,
    CMD_DUMPBC,
    CMD_DUMPDDS,
    CMD_MAX
};

enum OPTIONS
{
    OPT_RECURSIVE = 1,
    OPT_FORMAT,
    OPT_FILTER,
    OPT_DDS_DWORD_ALIGN,
    OPT_DDS_BAD_DXTN_TAILS,
    OPT_OUTPUTFILE,
    OPT_TOLOWER,
    OPT_OVERWRITE,
    OPT_FILETYPE,
    OPT_NOLOGO,
    OPT_TYPELESS_UNORM,
    OPT_TYPELESS_FLOAT,
    OPT_EXPAND_LUMINANCE,
    OPT_TARGET_PIXELX,
    OPT_TARGET_PIXELY,
    OPT_FILELIST,
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
    { L"info",      CMD_INFO },
    { L"analyze",   CMD_ANALYZE },
    { L"compare",   CMD_COMPARE },
    { L"diff",      CMD_DIFF },
    { L"dumpbc",    CMD_DUMPBC },
    { L"dumpdds",   CMD_DUMPDDS },
    { nullptr,      0 }
};

const SValue g_pOptions[] =
{
    { L"r",         OPT_RECURSIVE },
    { L"f",         OPT_FORMAT },
    { L"if",        OPT_FILTER },
    { L"dword",     OPT_DDS_DWORD_ALIGN },
    { L"badtails",  OPT_DDS_BAD_DXTN_TAILS },
    { L"nologo",    OPT_NOLOGO },
    { L"o",         OPT_OUTPUTFILE },
    { L"l",         OPT_TOLOWER },
    { L"y",         OPT_OVERWRITE },
    { L"ft",        OPT_FILETYPE },
    { L"tu",        OPT_TYPELESS_UNORM },
    { L"tf",        OPT_TYPELESS_FLOAT },
    { L"xlum",      OPT_EXPAND_LUMINANCE },
    { L"targetx",   OPT_TARGET_PIXELX },
    { L"targety",   OPT_TARGET_PIXELY },
    { L"flist",     OPT_FILELIST },
    { nullptr,      0 }
};

#define DEFFMT(fmt) { L## #fmt, DXGI_FORMAT_ ## fmt }

const SValue g_pFormats[] =
{
    // List does not include _TYPELESS, depth/stencil, or BC formats
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
    DEFFMT(B4G4R4A4_UNORM),

    { nullptr, DXGI_FORMAT_UNKNOWN }
};

const SValue g_pFormatAliases[] =
{
    { L"RGBA", DXGI_FORMAT_R8G8B8A8_UNORM },
    { L"BGRA", DXGI_FORMAT_B8G8R8A8_UNORM },

    { L"FP16", DXGI_FORMAT_R16G16B16A16_FLOAT },
    { L"FP32", DXGI_FORMAT_R32G32B32A32_FLOAT },

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
    DEFFMT(BC1_UNORM),
    DEFFMT(BC1_UNORM_SRGB),
    DEFFMT(BC2_TYPELESS),
    DEFFMT(BC2_UNORM),
    DEFFMT(BC2_UNORM_SRGB),
    DEFFMT(BC3_TYPELESS),
    DEFFMT(BC3_UNORM),
    DEFFMT(BC3_UNORM_SRGB),
    DEFFMT(BC4_TYPELESS),
    DEFFMT(BC4_UNORM),
    DEFFMT(BC4_SNORM),
    DEFFMT(BC5_TYPELESS),
    DEFFMT(BC5_UNORM),
    DEFFMT(BC5_SNORM),

    // DXGI 1.1 formats
    DEFFMT(B8G8R8A8_TYPELESS),
    DEFFMT(B8G8R8X8_TYPELESS),
    DEFFMT(BC6H_TYPELESS),
    DEFFMT(BC6H_UF16),
    DEFFMT(BC6H_SF16),
    DEFFMT(BC7_TYPELESS),
    DEFFMT(BC7_UNORM),
    DEFFMT(BC7_UNORM_SRGB),

    // DXGI 1.2 formats
    DEFFMT(AI44),
    DEFFMT(IA44),
    DEFFMT(P8),
    DEFFMT(A8P8),
    DEFFMT(NV12),
    DEFFMT(P010),
    DEFFMT(P016),
    DEFFMT(420_OPAQUE),
    DEFFMT(NV11),

    // DXGI 1.3 formats
    { L"P208", DXGI_FORMAT(130) },
    { L"V208", DXGI_FORMAT(131) },
    { L"V408", DXGI_FORMAT(132) },

    // Xbox-specific formats
    { L"R10G10B10_7E3_A2_FLOAT (Xbox)", DXGI_FORMAT(116) },
    { L"R10G10B10_6E4_A2_FLOAT (Xbox)", DXGI_FORMAT(117) },
    { L"D16_UNORM_S8_UINT (Xbox)", DXGI_FORMAT(118) },
    { L"R16_UNORM_X8_TYPELESS (Xbox)", DXGI_FORMAT(119) },
    { L"X16_TYPELESS_G8_UINT (Xbox)", DXGI_FORMAT(120) },
    { L"R10G10B10_SNORM_A2_UNORM (Xbox)", DXGI_FORMAT(189) },
    { L"R4G4_UNORM (Xbox)", DXGI_FORMAT(190) },

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
    { nullptr,                      TEX_FILTER_DEFAULT }
};

#define CODEC_DDS 0xFFFF0001
#define CODEC_TGA 0xFFFF0002
#define CODEC_HDR 0xFFFF0005

#ifdef USE_OPENEXR
#define CODEC_EXR 0xFFFF0006
#endif

const SValue g_pDumpFileTypes[] =
{
    { L"BMP",   WIC_CODEC_BMP  },
    { L"JPG",   WIC_CODEC_JPEG },
    { L"JPEG",  WIC_CODEC_JPEG },
    { L"PNG",   WIC_CODEC_PNG  },
    { L"TGA",   CODEC_TGA      },
    { L"HDR",   CODEC_HDR      },
    { L"TIF",   WIC_CODEC_TIFF },
    { L"TIFF",  WIC_CODEC_TIFF },
    { L"JXR",   WIC_CODEC_WMP  },
#ifdef USE_OPENEXR
    { L"EXR",   CODEC_EXR      },
#endif
    { nullptr,  CODEC_DDS      }
};

const SValue g_pExtFileTypes[] =
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

    using ScopedFindHandle = std::unique_ptr<void, find_closer>;

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
        wprintf(L"Microsoft (R) DirectX Texture Diagnostic Tool\n");
        wprintf(L"Copyright (C) Microsoft Corp. All rights reserved.\n");
#ifdef _DEBUG
        wprintf(L"*** Debug build ***\n");
#endif
        wprintf(L"\n");
    }


    void PrintUsage()
    {
        PrintLogo();

        wprintf(L"Usage: texdiag <command> <options> <files>\n\n");
        wprintf(L"   info                Output image metadata\n");
        wprintf(L"   analyze             Analyze and summarize image information\n");
        wprintf(L"   compare             Compare two images with MSE error metric\n");
        wprintf(L"   diff                Generate difference image from two images\n");
        wprintf(L"   dumpbc              Dump out compressed blocks (DDS BC only)\n");
        wprintf(L"   dumpdds             Dump out all the images in a complex DDS\n\n");
        wprintf(L"   -r                  wildcard filename search is recursive\n");
        wprintf(L"   -if <filter>        image filtering\n");
        wprintf(L"\n                       (DDS input only)\n");
        wprintf(L"   -t{u|f}             TYPELESS format is treated as UNORM or FLOAT\n");
        wprintf(L"   -dword              Use DWORD instead of BYTE alignment\n");
        wprintf(L"   -badtails           Fix for older DXTn with bad mipchain tails\n");
        wprintf(L"   -xlum               expand legacy L8, L16, and A8P8 formats\n");
        wprintf(L"\n                       (diff only)\n");
        wprintf(L"   -f <format>         format\n");
        wprintf(L"   -o <filename>       output filename\n");
        wprintf(L"   -l                  force output filename to lower case\n");
        wprintf(L"   -y                  overwrite existing output file (if any)\n");
        wprintf(L"\n                       (dumpbc only)\n");
        wprintf(L"   -targetx <num>      dump pixels at location x (defaults to all)\n");
        wprintf(L"   -targety <num>      dump pixels at location y (defaults to all)\n");
        wprintf(L"\n                       (dumpdds only)\n");
        wprintf(L"   -ft <filetype>      output file type\n");
        wprintf(L"\n   -nologo             suppress copyright message\n");
        wprintf(L"   -flist <filename>   use text file with a list of input files (one per line)\n");

        wprintf(L"\n   <format>: ");
        PrintList(13, g_pFormats);
        wprintf(L"      ");
        PrintList(13, g_pFormatAliases);

        wprintf(L"\n   <filter>: ");
        PrintList(13, g_pFilters);

        wprintf(L"\n   <filetype>: ");
        PrintList(15, g_pDumpFileTypes);
    }

    HRESULT LoadImage(const wchar_t *fileName, DWORD dwOptions, DWORD dwFilter, TexMetadata& info, std::unique_ptr<ScratchImage>& image)
    {
        if (!fileName)
            return E_INVALIDARG;

        image.reset(new (std::nothrow) ScratchImage);
        if (!image)
            return E_OUTOFMEMORY;

        wchar_t ext[_MAX_EXT];
        _wsplitpath_s(fileName, nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT);

        if (_wcsicmp(ext, L".dds") == 0)
        {
            DWORD ddsFlags = DDS_FLAGS_NONE;
            if (dwOptions & (1 << OPT_DDS_DWORD_ALIGN))
                ddsFlags |= DDS_FLAGS_LEGACY_DWORD;
            if (dwOptions & (1 << OPT_EXPAND_LUMINANCE))
                ddsFlags |= DDS_FLAGS_EXPAND_LUMINANCE;
            if (dwOptions & (1 << OPT_DDS_BAD_DXTN_TAILS))
                ddsFlags |= DDS_FLAGS_BAD_DXTN_TAILS;

            HRESULT hr = LoadFromDDSFile(fileName, ddsFlags, &info, *image);
            if (FAILED(hr))
                return hr;

            if (IsTypeless(info.format))
            {
                if (dwOptions & (1 << OPT_TYPELESS_UNORM))
                {
                    info.format = MakeTypelessUNORM(info.format);
                }
                else if (dwOptions & (1 << OPT_TYPELESS_FLOAT))
                {
                    info.format = MakeTypelessFLOAT(info.format);
                }

                if (IsTypeless(info.format))
                    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

                image->OverrideFormat(info.format);
            }

            return S_OK;
        }
        else if (_wcsicmp(ext, L".tga") == 0)
        {
            return LoadFromTGAFile(fileName, &info, *image);
        }
        else if (_wcsicmp(ext, L".hdr") == 0)
        {
            return LoadFromHDRFile(fileName, &info, *image);
        }
#ifdef USE_OPENEXR
        else if (_wcsicmp(ext, L".exr") == 0)
        {
            return LoadFromEXRFile(fileName, &info, *image);
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

            return LoadFromWICFile(fileName, dwFilter | WIC_FLAGS_ALL_FRAMES, &info, *image);
        }
    }

    HRESULT SaveImage(const Image* image, const wchar_t *fileName, DWORD codec)
    {
        switch (codec)
        {
        case CODEC_DDS:
            return SaveToDDSFile(*image, DDS_FLAGS_NONE, fileName);

        case CODEC_TGA:
            return SaveToTGAFile(*image, fileName);

        case CODEC_HDR:
            return SaveToHDRFile(*image, fileName);

#ifdef USE_OPENEXR
        case CODEC_EXR:
            return SaveToEXRFile(*image, fileName);
#endif

        default:
            return SaveToWICFile(*image, WIC_FLAGS_NONE, GetWICCodec(static_cast<WICCodecs>(codec)), fileName);
        }
    }

    //--------------------------------------------------------------------------------------
    struct AnalyzeData
    {
        XMFLOAT4 imageMin;
        XMFLOAT4 imageMax;
        XMFLOAT4 imageAvg;
        XMFLOAT4 imageVariance;
        XMFLOAT4 imageStdDev;
        float luminance;
        size_t   specials_x;
        size_t   specials_y;
        size_t   specials_z;
        size_t   specials_w;

        void Print()
        {
            wprintf(L"\t  Minimum - (%f %f %f %f)\n", imageMin.x, imageMin.y, imageMin.z, imageMin.w);
            wprintf(L"\t  Average - (%f %f %f %f)\n", imageAvg.x, imageAvg.y, imageAvg.z, imageAvg.w);
            wprintf(L"\t  Maximum - (%f %f %f %f)\n", imageMax.x, imageMax.y, imageMax.z, imageMax.w);
            wprintf(L"\t Variance - (%f %f %f %f)\n", imageVariance.x, imageVariance.y, imageVariance.z, imageVariance.w);
            wprintf(L"\t  Std Dev - (%f %f %f %f)\n", imageStdDev.x, imageStdDev.y, imageStdDev.z, imageStdDev.w);

            wprintf(L"\tLuminance - %f (maximum)\n", luminance);

            if ((specials_x > 0) || (specials_y > 0) || (specials_z > 0) || (specials_w > 0))
            {
                wprintf(L"     FP specials - (%zu %zu %zu %zu)\n", specials_x, specials_y, specials_z, specials_w);
            }
        }
    };

    HRESULT Analyze(const Image& image, _Out_ AnalyzeData& result)
    {
        memset(&result, 0, sizeof(AnalyzeData));

        // First pass
        XMVECTOR minv = g_XMFltMax;
        XMVECTOR maxv = XMVectorNegate(g_XMFltMax);
        XMVECTOR acc = g_XMZero;
        XMVECTOR luminance = g_XMZero;

        size_t totalPixels = 0;

        HRESULT hr = EvaluateImage(image, [&](const XMVECTOR * pixels, size_t width, size_t y)
            {
                static const XMVECTORF32 s_luminance = { { {  0.3f, 0.59f, 0.11f, 0.f } } };

                UNREFERENCED_PARAMETER(y);

                for (size_t x = 0; x < width; ++x)
                {
                    XMVECTOR v = *pixels++;
                    luminance = XMVectorMax(luminance, XMVector3Dot(v, s_luminance));
                    minv = XMVectorMin(minv, v);
                    maxv = XMVectorMax(maxv, v);
                    acc = XMVectorAdd(v, acc);
                    ++totalPixels;

                    XMFLOAT4 f;
                    XMStoreFloat4(&f, v);
                    if (!isfinite(f.x))
                    {
                        ++result.specials_x;
                    }

                    if (!isfinite(f.y))
                    {
                        ++result.specials_y;
                    }

                    if (!isfinite(f.z))
                    {
                        ++result.specials_z;
                    }

                    if (!isfinite(f.w))
                    {
                        ++result.specials_w;
                    }
                }
            });
        if (FAILED(hr))
            return hr;

        if (!totalPixels)
            return S_FALSE;

        result.luminance = XMVectorGetX(luminance);
        XMStoreFloat4(&result.imageMin, minv);
        XMStoreFloat4(&result.imageMax, maxv);

        XMVECTOR pixelv = XMVectorReplicate(float(totalPixels));
        XMVECTOR avgv = XMVectorDivide(acc, pixelv);
        XMStoreFloat4(&result.imageAvg, avgv);

        // Second pass
        acc = g_XMZero;

        hr = EvaluateImage(image, [&](const XMVECTOR * pixels, size_t width, size_t y)
            {
                UNREFERENCED_PARAMETER(y);

                for (size_t x = 0; x < width; ++x)
                {
                    XMVECTOR v = *pixels++;

                    XMVECTOR diff = XMVectorSubtract(v, avgv);
                    acc = XMVectorMultiplyAdd(diff, diff, acc);
                }
            });
        if (FAILED(hr))
            return hr;

        XMStoreFloat4(&result.imageVariance, acc);

        XMVECTOR stddev = XMVectorSqrt(acc);

        XMStoreFloat4(&result.imageStdDev, stddev);

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    struct AnalyzeBCData
    {
        size_t blocks;
        size_t blockHist[15];

        void Print(DXGI_FORMAT fmt)
        {
            wprintf(L"\t        Compression - ");
            PrintFormat(fmt);
            wprintf(L"\n\t       Total blocks - %zu\n", blocks);

            switch (fmt)
            {
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
                wprintf(L"\t     4 color blocks - %zu\n", blockHist[0]);
                wprintf(L"\t     3 color blocks - %zu\n", blockHist[1]);
                break;

                // BC2 only has a single 'type' of block

            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
                wprintf(L"\t     8 alpha blocks - %zu\n", blockHist[0]);
                wprintf(L"\t     6 alpha blocks - %zu\n", blockHist[1]);
                break;

            case DXGI_FORMAT_BC4_UNORM:
            case DXGI_FORMAT_BC4_SNORM:
                wprintf(L"\t     8 red blocks - %zu\n", blockHist[0]);
                wprintf(L"\t     6 red blocks - %zu\n", blockHist[1]);
                break;

            case DXGI_FORMAT_BC5_UNORM:
            case DXGI_FORMAT_BC5_SNORM:
                wprintf(L"\t     8 red blocks - %zu\n", blockHist[0]);
                wprintf(L"\t     6 red blocks - %zu\n", blockHist[1]);
                wprintf(L"\t   8 green blocks - %zu\n", blockHist[2]);
                wprintf(L"\t   6 green blocks - %zu\n", blockHist[3]);
                break;

            case DXGI_FORMAT_BC6H_UF16:
            case DXGI_FORMAT_BC6H_SF16:
                for (size_t j = 1; j <= 14; ++j)
                {
                    if (blockHist[j] > 0)
                        wprintf(L"\t     Mode %02zu blocks - %zu\n", j, blockHist[j]);
                }
                if (blockHist[0] > 0)
                    wprintf(L"\tReserved mode blcks - %zu\n", blockHist[0]);
                break;

            case DXGI_FORMAT_BC7_UNORM:
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                for (size_t j = 0; j <= 7; ++j)
                {
                    if (blockHist[j] > 0)
                        wprintf(L"\t     Mode %02zu blocks - %zu\n", j, blockHist[j]);
                }
                if (blockHist[8] > 0)
                    wprintf(L"\tReserved mode blcks - %zu\n", blockHist[8]);
                break;

            default:
                break;
            }
        }
    };

#pragma pack(push,1)
    struct BC1Block
    {
        uint16_t    rgb[2]; // 565 colors
        uint32_t    bitmap; // 2bpp rgb bitmap
    };

    struct BC2Block
    {
        uint32_t    bitmap[2];  // 4bpp alpha bitmap
        BC1Block    bc1;        // BC1 rgb data
    };

    struct BC3Block
    {
        uint8_t     alpha[2];   // alpha values
        uint8_t     bitmap[6];  // 3bpp alpha bitmap
        BC1Block    bc1;        // BC1 rgb data
    };

    struct BC4UBlock
    {
        uint8_t red_0;
        uint8_t red_1;
        uint8_t indices[6];
    };

    struct BC4SBlock
    {
        int8_t red_0;
        int8_t red_1;
        uint8_t indices[6];
    };

    struct BC5UBlock
    {
        BC4UBlock u;
        BC4UBlock v;
    };

    struct BC5SBlock
    {
        BC4SBlock u;
        BC4SBlock v;
    };
#pragma pack(pop)

    HRESULT AnalyzeBC(const Image& image, _Out_ AnalyzeBCData& result)
    {
        memset(&result, 0, sizeof(AnalyzeBCData));

        size_t sbpp;
        switch (image.format)
        {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            sbpp = 8;
            break;

        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            sbpp = 16;
            break;

        default:
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        const uint8_t *pSrc = image.pixels;
        const size_t rowPitch = image.rowPitch;

        for (size_t h = 0; h < image.height; h += 4)
        {
            const uint8_t *sptr = pSrc;

            for (size_t count = 0; count < rowPitch; count += sbpp)
            {
                switch (image.format)
                {
                case DXGI_FORMAT_BC1_UNORM:
                case DXGI_FORMAT_BC1_UNORM_SRGB:
                {
                    auto block = reinterpret_cast<const BC1Block*>(sptr);

                    if (block->rgb[0] <= block->rgb[1])
                    {
                        // Transparent block
                        ++result.blockHist[1];
                    }
                    else
                    {
                        // Opaque block
                        ++result.blockHist[0];
                    }
                }
                break;

                // BC2 only has a single 'type' of block

                case DXGI_FORMAT_BC3_UNORM:
                case DXGI_FORMAT_BC3_UNORM_SRGB:
                {
                    auto block = reinterpret_cast<const BC3Block*>(sptr);

                    if (block->alpha[0] > block->alpha[1])
                    {
                        // 8 alpha block
                        ++result.blockHist[0];
                    }
                    else
                    {
                        // 6 alpha block
                        ++result.blockHist[1];
                    }
                }
                break;

                case DXGI_FORMAT_BC4_UNORM:
                {
                    auto block = reinterpret_cast<const BC4UBlock*>(sptr);

                    if (block->red_0 > block->red_1)
                    {
                        // 8 red block
                        ++result.blockHist[0];
                    }
                    else
                    {
                        // 6 red block
                        ++result.blockHist[1];
                    }
                }
                break;

                case DXGI_FORMAT_BC4_SNORM:
                {
                    auto block = reinterpret_cast<const BC4SBlock*>(sptr);

                    if (block->red_0 > block->red_1)
                    {
                        // 8 red block
                        ++result.blockHist[0];
                    }
                    else
                    {
                        // 6 red block
                        ++result.blockHist[1];
                    }
                }
                break;

                case DXGI_FORMAT_BC5_UNORM:
                {
                    auto block = reinterpret_cast<const BC5UBlock*>(sptr);

                    if (block->u.red_0 > block->u.red_1)
                    {
                        // 8 red block
                        ++result.blockHist[0];
                    }
                    else
                    {
                        // 6 red block
                        ++result.blockHist[1];
                    }

                    if (block->v.red_0 > block->v.red_1)
                    {
                        // 8 green block
                        ++result.blockHist[2];
                    }
                    else
                    {
                        // 6 green block
                        ++result.blockHist[3];
                    }
                }
                break;

                case DXGI_FORMAT_BC5_SNORM:
                {
                    auto block = reinterpret_cast<const BC5SBlock*>(sptr);

                    if (block->u.red_0 > block->u.red_1)
                    {
                        // 8 red block
                        ++result.blockHist[0];
                    }
                    else
                    {
                        // 6 red block
                        ++result.blockHist[1];
                    }

                    if (block->v.red_0 > block->v.red_1)
                    {
                        // 8 green block
                        ++result.blockHist[2];
                    }
                    else
                    {
                        // 6 green block
                        ++result.blockHist[3];
                    }
                }
                break;

                case DXGI_FORMAT_BC6H_UF16:
                case DXGI_FORMAT_BC6H_SF16:
                    switch (*sptr & 0x03)
                    {
                    case 0x00:
                        // Mode 1 (2 bits, 00)
                        ++result.blockHist[1];
                        break;

                    case 0x01:
                        // Mode 2 (2 bits, 01)
                        ++result.blockHist[2];
                        break;

                    default:
                        switch (*sptr & 0x1F)
                        {
                        case 0x02:
                            // Mode 3 (5 bits, 00010)
                            ++result.blockHist[3];
                            break;

                        case 0x06:
                            // Mode 4 (5 bits, 00110)
                            ++result.blockHist[4];
                            break;

                        case 0x0A:
                            // Mode 5 (5 bits, 01010)
                            ++result.blockHist[5];
                            break;

                        case 0x0E:
                            // Mode 6 (5 bits, 01110)
                            ++result.blockHist[6];
                            break;

                        case 0x12:
                            // Mode 7 (5 bits, 10010)
                            ++result.blockHist[7];
                            break;

                        case 0x16:
                            // Mode 8 (5 bits, 10110)
                            ++result.blockHist[8];
                            break;

                        case 0x1A:
                            // Mode 9 (5 bits, 11010)
                            ++result.blockHist[9];
                            break;

                        case 0x1E:
                            // Mode 10 (5 bits, 11110)
                            ++result.blockHist[10];
                            break;

                        case 0x03:
                            // Mode 11 (5 bits, 00011)
                            ++result.blockHist[11];
                            break;

                        case 0x07:
                            // Mode 12 (5 bits, 00111)
                            ++result.blockHist[12];
                            break;

                        case 0x0B:
                            // Mode 13 (5 bits, 01011)
                            ++result.blockHist[13];
                            break;

                        case 0x0F:
                            // Mode 14 (5 bits, 01111)
                            ++result.blockHist[14];
                            break;

                        case 0x13: // Reserved mode (5 bits, 10011)
                        case 0x17: // Reserved mode (5 bits, 10111)
                        case 0x1B: // Reserved mode (5 bits, 11011)
                        case 0x1F: // Reserved mode (5 bits, 11111)
                        default:
                            ++result.blockHist[0];
                            break;
                        }
                        break;
                    }
                    break;

                case DXGI_FORMAT_BC7_UNORM:
                case DXGI_FORMAT_BC7_UNORM_SRGB:
                    if (*sptr & 0x01)
                    {
                        // Mode 0 (1)
                        ++result.blockHist[0];
                    }
                    else if (*sptr & 0x02)
                    {
                        // Mode 1 (01)
                        ++result.blockHist[1];
                    }
                    else if (*sptr & 0x04)
                    {
                        // Mode 2 (001)
                        ++result.blockHist[2];
                    }
                    else if (*sptr & 0x08)
                    {
                        // Mode 3 (0001)
                        ++result.blockHist[3];
                    }
                    else if (*sptr & 0x10)
                    {
                        // Mode 4 (00001)
                        ++result.blockHist[4];
                    }
                    else if (*sptr & 0x20)
                    {
                        // Mode 5 (000001)
                        ++result.blockHist[5];
                    }
                    else if (*sptr & 0x40)
                    {
                        // Mode 6 (0000001)
                        ++result.blockHist[6];
                    }
                    else if (*sptr & 0x80)
                    {
                        // Mode 7 (00000001)
                        ++result.blockHist[7];
                    }
                    else
                    {
                        // Reserved mode 8 (00000000)
                        ++result.blockHist[8];
                    }
                    break;

                default:
                    break;
                }

                sptr += sbpp;
                ++result.blocks;
            }

            pSrc += rowPitch;
        }

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    HRESULT Difference(const Image& image1, const Image& image2, DWORD dwFilter, DXGI_FORMAT format, ScratchImage& result)
    {
        if (!image1.pixels || !image2.pixels)
            return E_POINTER;

        if (image1.width != image2.width
            || image1.height != image2.height)
            return E_FAIL;

        ScratchImage tempA;
        const Image* imageA = &image1;
        if (IsCompressed(image1.format))
        {
            HRESULT hr = Decompress(image1, DXGI_FORMAT_R32G32B32A32_FLOAT, tempA);
            if (FAILED(hr))
                return hr;

            imageA = tempA.GetImage(0, 0, 0);
        }

        ScratchImage tempB;
        const Image* imageB = &image2;
        if (image2.format != DXGI_FORMAT_R32G32B32A32_FLOAT)
        {
            if (IsCompressed(image2.format))
            {
                HRESULT hr = Decompress(image2, DXGI_FORMAT_R32G32B32A32_FLOAT, tempB);
                if (FAILED(hr))
                    return hr;

                imageB = tempB.GetImage(0, 0, 0);
            }
            else
            {
                HRESULT hr = Convert(image2, DXGI_FORMAT_R32G32B32A32_FLOAT, dwFilter, TEX_THRESHOLD_DEFAULT, tempB);
                if (FAILED(hr))
                    return hr;

                imageB = tempB.GetImage(0, 0, 0);
            }
        }

        if (!imageA || !imageB)
            return E_POINTER;

        ScratchImage diffImage;
        HRESULT hr = TransformImage(*imageA, [&](XMVECTOR* outPixels, const XMVECTOR * inPixels, size_t width, size_t y)
            {
                auto *inPixelsB = reinterpret_cast<XMVECTOR*>(imageB->pixels + (y*imageB->rowPitch));

                for (size_t x = 0; x < width; ++x)
                {
                    XMVECTOR v1 = *inPixels++;
                    XMVECTOR v2 = *inPixelsB++;

                    v1 = XMVectorSubtract(v1, v2);
                    v1 = XMVectorAbs(v1);

                    v1 = XMVectorSelect(g_XMIdentityR3, v1, g_XMSelect1110);

                    *outPixels++ = v1;
                }
            }, (format == DXGI_FORMAT_R32G32B32A32_FLOAT) ? result : diffImage);
        if (FAILED(hr))
            return hr;

        if (format == DXGI_FORMAT_R32G32B32A32_FLOAT)
            return S_OK;

        return Convert(diffImage.GetImages(), diffImage.GetImageCount(), diffImage.GetMetadata(), format, dwFilter, TEX_THRESHOLD_DEFAULT, result);
    }


    //--------------------------------------------------------------------------------------
    // Partition, Shape, Fixup
    const uint8_t g_aFixUp[3][64][3] =
    {
        {   // No fix-ups for 1st subset for BC6H or BC7
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },
            { 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 },{ 0, 0, 0 }
        },

        {   // BC6H/BC7 Partition Set Fixups for 2 Subsets
            { 0,15, 0 },{ 0,15, 0 },{ 0,15, 0 },{ 0,15, 0 },
            { 0,15, 0 },{ 0,15, 0 },{ 0,15, 0 },{ 0,15, 0 },
            { 0,15, 0 },{ 0,15, 0 },{ 0,15, 0 },{ 0,15, 0 },
            { 0,15, 0 },{ 0,15, 0 },{ 0,15, 0 },{ 0,15, 0 },
            { 0,15, 0 },{ 0, 2, 0 },{ 0, 8, 0 },{ 0, 2, 0 },
            { 0, 2, 0 },{ 0, 8, 0 },{ 0, 8, 0 },{ 0,15, 0 },
            { 0, 2, 0 },{ 0, 8, 0 },{ 0, 2, 0 },{ 0, 2, 0 },
            { 0, 8, 0 },{ 0, 8, 0 },{ 0, 2, 0 },{ 0, 2, 0 },

            // BC7 Partition Set Fixups for 2 Subsets (second-half)
            { 0,15, 0 },{ 0,15, 0 },{ 0, 6, 0 },{ 0, 8, 0 },
            { 0, 2, 0 },{ 0, 8, 0 },{ 0,15, 0 },{ 0,15, 0 },
            { 0, 2, 0 },{ 0, 8, 0 },{ 0, 2, 0 },{ 0, 2, 0 },
            { 0, 2, 0 },{ 0,15, 0 },{ 0,15, 0 },{ 0, 6, 0 },
            { 0, 6, 0 },{ 0, 2, 0 },{ 0, 6, 0 },{ 0, 8, 0 },
            { 0,15, 0 },{ 0,15, 0 },{ 0, 2, 0 },{ 0, 2, 0 },
            { 0,15, 0 },{ 0,15, 0 },{ 0,15, 0 },{ 0,15, 0 },
            { 0,15, 0 },{ 0, 2, 0 },{ 0, 2, 0 },{ 0,15, 0 }
        },

        {   // BC7 Partition Set Fixups for 3 Subsets
            { 0, 3,15 },{ 0, 3, 8 },{ 0,15, 8 },{ 0,15, 3 },
            { 0, 8,15 },{ 0, 3,15 },{ 0,15, 3 },{ 0,15, 8 },
            { 0, 8,15 },{ 0, 8,15 },{ 0, 6,15 },{ 0, 6,15 },
            { 0, 6,15 },{ 0, 5,15 },{ 0, 3,15 },{ 0, 3, 8 },
            { 0, 3,15 },{ 0, 3, 8 },{ 0, 8,15 },{ 0,15, 3 },
            { 0, 3,15 },{ 0, 3, 8 },{ 0, 6,15 },{ 0,10, 8 },
            { 0, 5, 3 },{ 0, 8,15 },{ 0, 8, 6 },{ 0, 6,10 },
            { 0, 8,15 },{ 0, 5,15 },{ 0,15,10 },{ 0,15, 8 },
            { 0, 8,15 },{ 0,15, 3 },{ 0, 3,15 },{ 0, 5,10 },
            { 0, 6,10 },{ 0,10, 8 },{ 0, 8, 9 },{ 0,15,10 },
            { 0,15, 6 },{ 0, 3,15 },{ 0,15, 8 },{ 0, 5,15 },
            { 0,15, 3 },{ 0,15, 6 },{ 0,15, 6 },{ 0,15, 8 },
            { 0, 3,15 },{ 0,15, 3 },{ 0, 5,15 },{ 0, 5,15 },
            { 0, 5,15 },{ 0, 8,15 },{ 0, 5,15 },{ 0,10,15 },
            { 0, 5,15 },{ 0,10,15 },{ 0, 8,15 },{ 0,13,15 },
            { 0,15, 3 },{ 0,12,15 },{ 0, 3,15 },{ 0, 3, 8 }
        }
    };

    inline static bool IsFixUpOffset(_In_range_(0, 2) size_t uPartitions, _In_range_(0, 63) uint64_t uShape, _In_range_(0, 15) size_t uOffset)
    {
        for (size_t p = 0; p <= uPartitions; p++)
        {
            if (uOffset == g_aFixUp[uPartitions][uShape][p])
            {
                return true;
            }
        }
        return false;
    }

    //--------------------------------------------------------------------------------------
#define SIGN_EXTEND(x,nb) ((((x)&(1<<((nb)-1)))?((~0)^((1<<(nb))-1)):0)|(x))

#define NUM_PIXELS_PER_BLOCK 16

    void Print565(uint16_t rgb)
    {
        auto r = float(((rgb >> 11) & 31) * (1.0f / 31.0f));
        auto g = float(((rgb >> 5) & 63) * (1.0f / 63.0f));
        auto b = float(((rgb >> 0) & 31) * (1.0f / 31.0f));

        wprintf(L"(R: %.3f, G: %.3f, B: %.3f)", r, g, b);
    }

    void PrintIndex2bpp(uint32_t bitmap)
    {
        for (size_t j = 0; j < NUM_PIXELS_PER_BLOCK; ++j, bitmap >>= 2)
        {
            wprintf(L"%u%ls", bitmap & 0x3, ((j < (NUM_PIXELS_PER_BLOCK - 1)) && ((j % 4) == 3)) ? L" | " : L" ");
        }
    }

    void PrintIndex2bpp(uint64_t bitmap, size_t parts, uint64_t shape)
    {
        for (size_t j = 0; j < NUM_PIXELS_PER_BLOCK; ++j)
        {
            if (IsFixUpOffset(parts, shape, j))
            {
                wprintf(L"%llu%ls", bitmap & 0x1, ((j < (NUM_PIXELS_PER_BLOCK - 1)) && ((j % 4) == 3)) ? L" | " : L" ");
                bitmap >>= 1;
            }
            else
            {
                wprintf(L"%llu%ls", bitmap & 0x3, ((j < (NUM_PIXELS_PER_BLOCK - 1)) && ((j % 4) == 3)) ? L" | " : L" ");
                bitmap >>= 2;
            }
        }
    }

    void PrintIndex3bpp(uint64_t bitmap, size_t parts, uint64_t shape)
    {
        for (size_t j = 0; j < NUM_PIXELS_PER_BLOCK; ++j)
        {
            if (IsFixUpOffset(parts, shape, j))
            {
                wprintf(L"%llu%ls", bitmap & 0x3, ((j < (NUM_PIXELS_PER_BLOCK - 1)) && ((j % 4) == 3)) ? L" | " : L" ");
                bitmap >>= 2;
            }
            else
            {
                wprintf(L"%llu%ls", bitmap & 0x7, ((j < (NUM_PIXELS_PER_BLOCK - 1)) && ((j % 4) == 3)) ? L" | " : L" ");
                bitmap >>= 3;
            }
        }
    }

    void PrintIndex4bpp(uint64_t bitmap, size_t parts, uint64_t shape)
    {
        for (size_t j = 0; j < NUM_PIXELS_PER_BLOCK; ++j)
        {
            if (IsFixUpOffset(parts, shape, j))
            {
                wprintf(L"%llX%ls", bitmap & 0x7, ((j < (NUM_PIXELS_PER_BLOCK - 1)) && ((j % 4) == 3)) ? L" | " : L" ");
                bitmap >>= 3;
            }
            else
            {
                wprintf(L"%llX%ls", bitmap & 0xF, ((j < (NUM_PIXELS_PER_BLOCK - 1)) && ((j % 4) == 3)) ? L" | " : L" ");
                bitmap >>= 4;
            }
        }
    }

    void PrintIndex3bpp(const uint8_t data[6])
    {
        uint32_t bitmap = uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16);

        size_t j = 0;
        for (; j < (NUM_PIXELS_PER_BLOCK / 2); ++j, bitmap >>= 3)
        {
            wprintf(L"%u%ls", bitmap & 0x7, ((j % 4) == 3) ? L" | " : L" ");
        }

        bitmap = uint32_t(data[3]) | (uint32_t(data[4]) << 8) | (uint32_t(data[5]) << 16);

        for (; j < NUM_PIXELS_PER_BLOCK; ++j, bitmap >>= 3)
        {
            wprintf(L"%u%ls", bitmap & 0x7, ((j < (NUM_PIXELS_PER_BLOCK - 1)) && ((j % 4) == 3)) ? L" | " : L" ");
        }
    }

    const wchar_t* GetRotBits(uint64_t rot)
    {
        switch (rot)
        {
        case 1: return L" (R<->A)";
        case 2: return L" (G<->A)";
        case 3: return L" (B<->A)";
        default: return L"";
        }
    }

    HRESULT DumpBCImage(const Image& image, int pixelx, int pixely)
    {
        size_t sbpp;
        switch (image.format)
        {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            sbpp = 8;
            break;

        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            sbpp = 16;
            break;

        default:
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        const uint8_t *pSrc = image.pixels;
        const size_t rowPitch = image.rowPitch;

        size_t nblock = 0;
        for (size_t h = 0; h < image.height; h += 4, pSrc += rowPitch)
        {
            if (pixely >= 0)
            {
                if ((pixely < int(h)) || (pixely >= int(h + 4)))
                    continue;
            }

            const uint8_t *sptr = pSrc;

            size_t w = 0;
            for (size_t count = 0; count < rowPitch; count += sbpp, w += 4, ++nblock, sptr += sbpp)
            {
                if (pixelx >= 0)
                {
                    if ((pixelx < int(w)) || (pixelx >= int(w + 4)))
                        continue;
                }

                wprintf(L"   Block %zu (pixel: %zu x %zu)\n", nblock, w, h);
                switch (image.format)
                {
                case DXGI_FORMAT_BC1_UNORM:
                case DXGI_FORMAT_BC1_UNORM_SRGB:
                {
                    auto block = reinterpret_cast<const BC1Block*>(sptr);

                    if (block->rgb[0] <= block->rgb[1])
                    {
                        // Transparent block
                        wprintf(L"\tTransparent - E0: ");
                    }
                    else
                    {
                        // Opaque block
                        wprintf(L"\t     Opaque - E0: ");
                    }

                    Print565(block->rgb[0]);
                    wprintf(L"\n\t              E1: ");
                    Print565(block->rgb[1]);
                    wprintf(L"\n\t           Index: ");
                    PrintIndex2bpp(block->bitmap);
                    wprintf(L"\n");
                }
                break;

                case DXGI_FORMAT_BC2_UNORM:
                case DXGI_FORMAT_BC2_UNORM_SRGB:
                {
                    auto block = reinterpret_cast<const BC2Block*>(sptr);

                    wprintf(L"\tColor - E0: ");
                    Print565(block->bc1.rgb[0]);
                    wprintf(L"\n\t        E1: ");
                    Print565(block->bc1.rgb[1]);
                    wprintf(L"\n\t     Index: ");
                    PrintIndex2bpp(block->bc1.bitmap);
                    wprintf(L"\n");

                    wprintf(L"\tAlpha - ");

                    size_t j = 0;
                    uint32_t bitmap = block->bitmap[0];
                    for (; j < (NUM_PIXELS_PER_BLOCK / 2); ++j, bitmap >>= 4)
                    {
                        wprintf(L"%X%ls", bitmap & 0xF, ((j % 4) == 3) ? L" | " : L" ");
                    }

                    bitmap = block->bitmap[1];
                    for (; j < NUM_PIXELS_PER_BLOCK; ++j, bitmap >>= 4)
                    {
                        wprintf(L"%X%ls", bitmap & 0xF, ((j < (NUM_PIXELS_PER_BLOCK - 1)) && ((j % 4) == 3)) ? L" | " : L" ");
                    }

                    wprintf(L"\n");
                }
                break;

                case DXGI_FORMAT_BC3_UNORM:
                case DXGI_FORMAT_BC3_UNORM_SRGB:
                {
                    auto block = reinterpret_cast<const BC3Block*>(sptr);

                    wprintf(L"\tColor - E0: ");
                    Print565(block->bc1.rgb[0]);
                    wprintf(L"\n\t        E1: ");
                    Print565(block->bc1.rgb[1]);
                    wprintf(L"\n\t     Index: ");
                    PrintIndex2bpp(block->bc1.bitmap);
                    wprintf(L"\n");

                    wprintf(L"\tAlpha - E0: %0.3f  E1: %0.3f (%u)\n\t     Index: ",
                        (float(block->alpha[0]) / 255.f),
                        (float(block->alpha[1]) / 255.f), (block->alpha[0] > block->alpha[1]) ? 8 : 6);

                    PrintIndex3bpp(block->bitmap);

                    wprintf(L"\n");
                }
                break;

                case DXGI_FORMAT_BC4_UNORM:
                {
                    auto block = reinterpret_cast<const BC4UBlock*>(sptr);

                    wprintf(L"\t   E0: %0.3f  E1: %0.3f (%u)\n\tIndex: ",
                        (float(block->red_0) / 255.f),
                        (float(block->red_1) / 255.f), (block->red_0 > block->red_1) ? 8 : 6);

                    PrintIndex3bpp(block->indices);

                    wprintf(L"\n");
                }
                break;

                case DXGI_FORMAT_BC4_SNORM:
                {
                    auto block = reinterpret_cast<const BC4SBlock*>(sptr);

                    wprintf(L"\t   E0: %0.3f  E1: %0.3f (%u)\n\tIndex: ",
                        (float(block->red_0) / 127.f),
                        (float(block->red_1) / 127.f), (block->red_0 > block->red_1) ? 8 : 6);

                    PrintIndex3bpp(block->indices);

                    wprintf(L"\n");
                }
                break;

                case DXGI_FORMAT_BC5_UNORM:
                {
                    auto block = reinterpret_cast<const BC5UBlock*>(sptr);

                    wprintf(L"\tU -   E0: %0.3f  E1: %0.3f (%u)\n\t   Index: ",
                        (float(block->u.red_0) / 255.f),
                        (float(block->u.red_1) / 255.f), (block->u.red_0 > block->u.red_1) ? 8 : 6);

                    PrintIndex3bpp(block->u.indices);

                    wprintf(L"\n");

                    wprintf(L"\tV -   E0: %0.3f  E1: %0.3f (%u)\n\t   Index: ",
                        (float(block->v.red_0) / 255.f),
                        (float(block->v.red_1) / 255.f), (block->v.red_0 > block->v.red_1) ? 8 : 6);

                    PrintIndex3bpp(block->v.indices);

                    wprintf(L"\n");
                }
                break;

                case DXGI_FORMAT_BC5_SNORM:
                {
                    auto block = reinterpret_cast<const BC5SBlock*>(sptr);

                    wprintf(L"\tU -   E0: %0.3f  E1: %0.3f (%u)\n\t   Index: ",
                        (float(block->u.red_0) / 127.f),
                        (float(block->u.red_1) / 127.f), (block->u.red_0 > block->u.red_1) ? 8 : 6);

                    PrintIndex3bpp(block->u.indices);

                    wprintf(L"\n");

                    wprintf(L"\tV -   E0: %0.3f  E1: %0.3f (%u)\n\t   Index: ",
                        (float(block->v.red_0) / 127.f),
                        (float(block->v.red_1) / 127.f), (block->v.red_0 > block->v.red_1) ? 8 : 6);

                    PrintIndex3bpp(block->v.indices);

                    wprintf(L"\n");
                }
                break;

                case DXGI_FORMAT_BC6H_UF16:
                case DXGI_FORMAT_BC6H_SF16:
                    // http://msdn.microsoft.com/en-us/library/windows/desktop/hh308952.aspx#decoding_the_bc6h_format

                    switch (*sptr & 0x03)
                    {
                    case 0x00:
                        // Mode 1 (2 bits, 00)
                    {
                        struct bc6h_mode1
                        {
                            uint64_t mode : 2; // { M, 0}, { M, 1}
                            uint64_t gy4 : 1;  // {GY, 4}
                            uint64_t by4 : 1;  // {BY, 4}
                            uint64_t bz4 : 1;  // {BZ, 4}
                            uint64_t rw : 10;  // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}, {RW, 8}, {RW, 9}
                            uint64_t gw : 10;  // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}, {GW, 8}, {GW, 9}
                            uint64_t bw : 10;  // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}, {BW, 8}, {BW, 9}
                            uint64_t rx : 5;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}, {RX, 4}
                            uint64_t gz4 : 1;  // {GZ, 4}
                            uint64_t gy : 4;   // {GY, 0}, {GY, 1}, {GY, 2}, {GY, 3}
                            uint64_t gx : 5;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}, {GX, 4}
                            uint64_t bz0 : 1;  // {BZ, 0},
                            uint64_t gz : 4;   // {GZ, 0}, {GZ, 1}, {GZ, 2}, {GZ, 3}
                            uint64_t bx : 5;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}, {BX, 4}
                            uint64_t bz1 : 1;  // {BZ, 1}
                            uint64_t by : 3;   // {BY, 0}, {BY, 1}, {BY, 2}
                            uint64_t by3 : 1;  // {BY, 3}
                            uint64_t ry : 5;   // {RY, 0}, {RY, 1}, {RY, 2}, {RY, 3}, {RY, 4}
                            uint64_t bz2 : 1;  // {BZ, 2}
                            uint64_t rz : 5;   // {RZ, 0}, {RZ, 1}, {RZ, 2}, {RZ, 3}, {RZ, 4}
                            uint64_t bz3 : 1;  // {BZ, 3}
                            uint64_t d : 5;    // { D, 0}, { D, 1}, { D, 2}, { D, 3}, { D, 4}
                            uint64_t indices : 46;
                        };
                        static_assert(sizeof(bc6h_mode1) == 16, "Block size must be 16 bytes");

                        bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                        auto m = reinterpret_cast<const bc6h_mode1*>(sptr);

                        XMINT3 e0_A(int(m->rw), int(m->gw), int(m->bw));
                        XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));
                        XMINT3 e1_A(int(m->ry),
                            int(m->gy | (m->gy4 << 4)),
                            int(m->by | (m->by3 << 3) | (m->by4 << 4)));
                        XMINT3 e1_B(int(m->rz),
                            int(m->gz | (m->gz4 << 4)),
                            int(m->bz0 | (m->bz1 << 1) | (m->bz2 << 2) | (m->bz3 << 3) | (m->bz4 << 4)));

                        if (bSigned)
                        {
                            e0_A.x = SIGN_EXTEND(e0_A.x, 10);
                            e0_A.y = SIGN_EXTEND(e0_A.y, 10);
                            e0_A.z = SIGN_EXTEND(e0_A.z, 10);

                            e0_B.x = SIGN_EXTEND(e0_B.x, 5);
                            e0_B.y = SIGN_EXTEND(e0_B.y, 5);
                            e0_B.z = SIGN_EXTEND(e0_B.z, 5);

                            e1_A.x = SIGN_EXTEND(e1_A.x, 5);
                            e1_A.y = SIGN_EXTEND(e1_A.y, 5);
                            e1_A.z = SIGN_EXTEND(e1_A.z, 5);

                            e1_B.x = SIGN_EXTEND(e1_B.x, 5);
                            e1_B.y = SIGN_EXTEND(e1_B.y, 5);
                            e1_B.z = SIGN_EXTEND(e1_B.z, 5);
                        }

                        wprintf(L"\tMode 1 - [10 5 5 5] shape %llu\n", m->d);
                        wprintf(L"\t         E0(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                        wprintf(L"\t         E0(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                        wprintf(L"\t         E1(A): (%04X, %04X, %04X)\n", e1_A.x & 0xFFFF, e1_A.y & 0xFFFF, e1_A.z & 0xFFFF);
                        wprintf(L"\t         E1(B): (%04X, %04X, %04X)\n", e1_B.x & 0xFFFF, e1_B.y & 0xFFFF, e1_B.z & 0xFFFF);
                        wprintf(L"\t         Index: ");
                        PrintIndex3bpp(m->indices, 1, m->d);
                        wprintf(L"\n");
                    }
                    break;

                    case 0x01:
                        // Mode 2 (2 bits, 01)
                    {
                        struct bc6h_mode2
                        {
                            uint64_t mode : 2; // { M, 0}, { M, 1}
                            uint64_t gy5 : 1;  // {GY, 5}
                            uint64_t gz45 : 2; // {GZ, 4}, {GZ, 5}
                            uint64_t rw : 7;   // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}
                            uint64_t bz : 2;   // {BZ, 0}, {BZ, 1}
                            uint64_t by4 : 1;  // {BY, 4},
                            uint64_t gw : 7;   // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}
                            uint64_t by5 : 1;  // {BY, 5}
                            uint64_t bz2 : 1;  // {BZ, 2}
                            uint64_t gy4 : 1;  // {GY, 4}
                            uint64_t bw : 7;   // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}
                            uint64_t bz3 : 1;  // {BZ, 3}
                            uint64_t bz5 : 1;  // {BZ, 5}
                            uint64_t bz4 : 1;  // {BZ, 4}
                            uint64_t rx : 6;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}, {RX, 4}, {RX, 5}
                            uint64_t gy : 4;   // {GY, 0}, {GY, 1}, {GY, 2}, {GY, 3}
                            uint64_t gx : 6;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}, {GX, 4}, {GX, 5}
                            uint64_t gz : 4;   // {GZ, 0}, {GZ, 1}, {GZ, 2}, {GZ, 3}
                            uint64_t bx : 5;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}, {BX, 4}, {BX, 5}
                            uint64_t by : 4;   // {BY, 0}, {BY, 1}, {BY, 2}, {BY, 3}
                            uint64_t ry : 6;   // {RY, 0}, {RY, 1}, {RY, 2}, {RY, 3}, {RY, 4}, {RY, 5}
                            uint64_t rz : 6;   // {RZ, 0}, {RZ, 1}, {RZ, 2}, {RZ, 3}, {RZ, 4}, {RZ, 5},
                            uint64_t d : 5;    // { D, 0}, { D, 1}, { D, 2}, { D, 3}, { D, 4}
                            uint64_t indices : 46;

                        };
                        static_assert(sizeof(bc6h_mode2) == 16, "Block size must be 16 bytes");

                        bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                        auto m = reinterpret_cast<const bc6h_mode2*>(sptr);

                        XMINT3 e0_A(int(m->rw), int(m->gw), int(m->bw));
                        XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));
                        XMINT3 e1_A(int(m->ry),
                            int(m->gy | (m->gy4 << 4) | (m->gy5 << 5)),
                            int(m->by | (m->by4 << 4) | (m->by5 << 5)));
                        XMINT3 e1_B(int(m->rz),
                            int(m->gz | (m->gz45 << 4)),
                            int(m->bz | (m->bz2 << 2) | (m->bz3 << 3) | (m->bz4 << 4) | (m->bz5 << 5)));

                        if (bSigned)
                        {
                            e0_A.x = SIGN_EXTEND(e0_A.x, 7);
                            e0_A.y = SIGN_EXTEND(e0_A.y, 7);
                            e0_A.z = SIGN_EXTEND(e0_A.z, 7);

                            e0_B.x = SIGN_EXTEND(e0_B.x, 6);
                            e0_B.y = SIGN_EXTEND(e0_B.y, 6);
                            e0_B.z = SIGN_EXTEND(e0_B.z, 6);

                            e1_A.x = SIGN_EXTEND(e1_A.x, 6);
                            e1_A.y = SIGN_EXTEND(e1_A.y, 6);
                            e1_A.z = SIGN_EXTEND(e1_A.z, 6);

                            e1_B.x = SIGN_EXTEND(e1_B.x, 6);
                            e1_B.y = SIGN_EXTEND(e1_B.y, 6);
                            e1_B.z = SIGN_EXTEND(e1_B.z, 6);
                        }

                        wprintf(L"\tMode 2 - [7 6 6 6] shape %llu\n", m->d);
                        wprintf(L"\t         E0(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                        wprintf(L"\t         E0(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                        wprintf(L"\t         E1(A): (%04X, %04X, %04X)\n", e1_A.x & 0xFFFF, e1_A.y & 0xFFFF, e1_A.z & 0xFFFF);
                        wprintf(L"\t         E1(B): (%04X, %04X, %04X)\n", e1_B.x & 0xFFFF, e1_B.y & 0xFFFF, e1_B.z & 0xFFFF);
                        wprintf(L"\t         Index: ");
                        PrintIndex3bpp(m->indices, 1, m->d);
                        wprintf(L"\n");
                    }
                    break;

                    default:
                        switch (*sptr & 0x1F)
                        {
                        case 0x02:
                            // Mode 3 (5 bits, 00010)
                        {
                            struct bc6h_mode3
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 10;  // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}, {RW, 8}, {RW, 9}
                                uint64_t gw : 10;  // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}, {GW, 8}, {GW, 9}
                                uint64_t bw : 10;  // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}, {BW, 8}, {BW, 9}
                                uint64_t rx : 5;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}, {RX, 4}
                                uint64_t rw10 : 1; // {RW,10}
                                uint64_t gy : 4;   // {GY, 0}, {GY, 1}, {GY, 2}, {GY, 3}
                                uint64_t gx : 4;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}
                                uint64_t gw10 : 1; // {GW,10}
                                uint64_t bz0 : 1;  // {BZ, 0}
                                uint64_t gz : 4;   // {GZ, 0}, {GZ, 1}, {GZ, 2}, {GZ, 3}
                                uint64_t bx : 4;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}
                                uint64_t bw10 : 1; // {BW,10}
                                uint64_t bz1 : 1;  // {BZ, 1}
                                uint64_t by : 3;   // {BY, 0}, {BY, 1}, {BY, 2}
                                uint64_t by3 : 1;  // {BY, 3}
                                uint64_t ry : 5;   // {RY, 0}, {RY, 1}, {RY, 2}, {RY, 3}, {RY, 4}
                                uint64_t bz2 : 1;  // {BZ, 2}
                                uint64_t rz : 5;   // {RZ, 0}, {RZ, 1}, {RZ, 2}, {RZ, 3}, {RZ, 4}
                                uint64_t bz3 : 1;  // {BZ, 3}
                                uint64_t d : 5;    // { D, 0}, { D, 1}, { D, 2}, { D, 3}, { D, 4}
                                uint64_t indices : 46;

                            };
                            static_assert(sizeof(bc6h_mode3) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode3*>(sptr);

                            XMINT3 e0_A(int(m->rw | (m->rw10 << 10)),
                                int(m->gw | (m->gw10 << 10)),
                                int(m->bw | (m->bw10 << 10)));
                            XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));
                            XMINT3 e1_A(int(m->ry), int(m->gy),
                                int(m->by | (m->by3 << 3)));
                            XMINT3 e1_B(int(m->rz),
                                int(m->gz),
                                int(m->bz0 | (m->bz1 << 1) | (m->bz2 << 2) | (m->bz3 << 3)));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 11);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 11);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 11);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 5);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 4);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 4);

                                e1_A.x = SIGN_EXTEND(e1_A.x, 5);
                                e1_A.y = SIGN_EXTEND(e1_A.y, 4);
                                e1_A.z = SIGN_EXTEND(e1_A.z, 4);

                                e1_B.x = SIGN_EXTEND(e1_B.x, 5);
                                e1_B.y = SIGN_EXTEND(e1_B.y, 4);
                                e1_B.z = SIGN_EXTEND(e1_B.z, 4);
                            }

                            wprintf(L"\tMode 3 - [11 5 4 4] shape %llu\n", m->d);
                            wprintf(L"\t         E0(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E0(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         E1(A): (%04X, %04X, %04X)\n", e1_A.x & 0xFFFF, e1_A.y & 0xFFFF, e1_A.z & 0xFFFF);
                            wprintf(L"\t         E1(B): (%04X, %04X, %04X)\n", e1_B.x & 0xFFFF, e1_B.y & 0xFFFF, e1_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex3bpp(m->indices, 1, m->d);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x06:
                            // Mode 4 (5 bits, 00110)
                        {
                            struct bc6h_mode4
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 10;  // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}, {RW, 8}, {RW, 9}
                                uint64_t gw : 10;  // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}, {GW, 8}, {GW, 9}
                                uint64_t bw : 10;  // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}, {BW, 8}, {BW, 9}
                                uint64_t rx : 4;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}
                                uint64_t rw10 : 1; // {RW,10}
                                uint64_t gz4 : 1;  // {GZ, 4}
                                uint64_t gy : 4;   // {GY, 0}, {GY, 1}, {GY, 2}, {GY, 3}
                                uint64_t gx : 5;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}, {GX, 4}
                                uint64_t gw10 : 1; // {GW,10}
                                uint64_t gz : 4;   // {GZ, 0}, {GZ, 1}, {GZ, 2}, {GZ, 3}
                                uint64_t bx : 4;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}
                                uint64_t bw10 : 1; // {BW,10}
                                uint64_t bz1 : 1;  // {BZ, 1}
                                uint64_t by : 3;   // {BY, 0}, {BY, 1}, {BY, 2}
                                uint64_t by3 : 1;  // {BY, 3}
                                uint64_t ry : 4;   // {RY, 0}, {RY, 1}, {RY, 2}, {RY, 3}
                                uint64_t bz0 : 1;  // {BZ, 0}
                                uint64_t bz2 : 1;  // {BZ, 2}
                                uint64_t rz : 4;   // {RZ, 0}, {RZ, 1}, {RZ, 2}, {RZ, 3}
                                uint64_t gy4 : 1;  // {GY, 4}
                                uint64_t bz3 : 1;  // {BZ, 3}
                                uint64_t d : 5;    // { D, 0}, { D, 1}, { D, 2}, { D, 3}, { D, 4}
                                uint64_t indices : 46;

                            };
                            static_assert(sizeof(bc6h_mode4) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode4*>(sptr);

                            XMINT3 e0_A(int(m->rw | (m->rw10 << 10)),
                                int(m->gw | (m->gw10 << 10)),
                                int(m->bw | (m->bw10 << 10)));
                            XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));
                            XMINT3 e1_A(int(m->ry),
                                int(m->gy | (m->gy4 << 4)),
                                int(m->by | (m->by3 << 3)));
                            XMINT3 e1_B(int(m->rz),
                                int(m->gz | (m->gz4 << 4)),
                                int(m->bz0 | (m->bz1 << 1) | (m->bz2 << 2) | (m->bz3 << 3)));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 11);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 11);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 11);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 4);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 5);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 4);

                                e1_A.x = SIGN_EXTEND(e1_A.x, 4);
                                e1_A.y = SIGN_EXTEND(e1_A.y, 5);
                                e1_A.z = SIGN_EXTEND(e1_A.z, 4);

                                e1_B.x = SIGN_EXTEND(e1_B.x, 4);
                                e1_B.y = SIGN_EXTEND(e1_B.y, 5);
                                e1_B.z = SIGN_EXTEND(e1_B.z, 4);
                            }

                            wprintf(L"\tMode 4 - [11 4 5 4] shape %llu\n", m->d);
                            wprintf(L"\t         E0(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E0(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         E1(A): (%04X, %04X, %04X)\n", e1_A.x & 0xFFFF, e1_A.y & 0xFFFF, e1_A.z & 0xFFFF);
                            wprintf(L"\t         E1(B): (%04X, %04X, %04X)\n", e1_B.x & 0xFFFF, e1_B.y & 0xFFFF, e1_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex3bpp(m->indices, 1, m->d);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x0A:
                            // Mode 5 (5 bits, 01010)
                        {
                            struct bc6h_mode5
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 10;  // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}, {RW, 8}, {RW, 9}
                                uint64_t gw : 10;  // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}, {GW, 8}, {GW, 9}
                                uint64_t bw : 10;  // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}, {BW, 8}, {BW, 9}
                                uint64_t rx : 4;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}
                                uint64_t rw10 : 1; // {RW,10}
                                uint64_t by4 : 1;  // {BY, 4}
                                uint64_t gy : 4;   // {GY, 0}, {GY, 1}, {GY, 2}, {GY, 3}
                                uint64_t gx : 4;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}
                                uint64_t gw10 : 1; // {GW,10}
                                uint64_t bz0 : 1;  // {BZ, 0}
                                uint64_t gz : 4;   // {GZ, 0}, {GZ, 1}, {GZ, 2}, {GZ, 3}
                                uint64_t bx : 5;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}, {BX, 4}
                                uint64_t bw10 : 1; // {BW,10}
                                uint64_t by : 3;   // {BY, 0}, {BY, 1}, {BY, 2}
                                uint64_t by3 : 1;  // {BY, 3}
                                uint64_t ry : 4;   // {RY, 0}, {RY, 1}, {RY, 2}, {RY, 3}
                                uint64_t bz12 : 2; // {BZ, 1}, {BZ, 2}
                                uint64_t rz : 5;   // {RZ, 0}, {RZ, 1}, {RZ, 2}, {RZ, 3}, {BZ, 4}
                                uint64_t bz3 : 1;  // {BZ, 3}
                                uint64_t d : 5;    // { D, 0}, { D, 1}, { D, 2}, { D, 3}, { D, 4}
                                uint64_t indices : 46;
                            };
                            static_assert(sizeof(bc6h_mode5) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode5*>(sptr);

                            XMINT3 e0_A(int(m->rw | (m->rw10 << 10)),
                                int(m->gw | (m->gw10 << 10)),
                                int(m->bw | (m->bw10 << 10)));
                            XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));
                            XMINT3 e1_A(int(m->ry), int(m->gy),
                                int(m->by | (m->by3 << 3) | (m->by4 << 4)));
                            XMINT3 e1_B(int(m->rz), int(m->gz),
                                int(m->bz0 | (m->bz12 << 1) | (m->bz3 << 3)));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 11);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 11);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 11);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 4);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 4);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 5);

                                e1_A.x = SIGN_EXTEND(e1_A.x, 4);
                                e1_A.y = SIGN_EXTEND(e1_A.y, 4);
                                e1_A.z = SIGN_EXTEND(e1_A.z, 5);

                                e1_B.x = SIGN_EXTEND(e1_B.x, 4);
                                e1_B.y = SIGN_EXTEND(e1_B.y, 4);
                                e1_B.z = SIGN_EXTEND(e1_B.z, 5);
                            }

                            wprintf(L"\tMode 5 - [11 4 4 5] shape %llu\n", m->d);
                            wprintf(L"\t         E0(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E0(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         E1(A): (%04X, %04X, %04X)\n", e1_A.x & 0xFFFF, e1_A.y & 0xFFFF, e1_A.z & 0xFFFF);
                            wprintf(L"\t         E1(B): (%04X, %04X, %04X)\n", e1_B.x & 0xFFFF, e1_B.y & 0xFFFF, e1_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex3bpp(m->indices, 1, m->d);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x0E:
                            // Mode 6 (5 bits, 01110)
                        {
                            struct bc6h_mode6
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 9;   // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}, {RW, 8}
                                uint64_t by4 : 1;  // {BY, 4}
                                uint64_t gw : 9;   // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}, {GW, 8}
                                uint64_t gy4 : 1;  // {GY, 4}
                                uint64_t bw : 9;   // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}, {BW, 8}
                                uint64_t bz4 : 1;  // {BZ, 4}
                                uint64_t rx : 5;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}, {RX, 4}
                                uint64_t gz4 : 1;  // {GZ, 4}
                                uint64_t gy : 4;   // {GY, 0}, {GY, 1}, {GY, 2}, {GY, 3}
                                uint64_t gx : 5;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}, {GX, 4}
                                uint64_t bz0 : 1;  // {BZ, 0}
                                uint64_t gz : 4;   // {GZ, 0}, {GZ, 1}, {GZ, 2}, {GZ, 3}
                                uint64_t bx : 5;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}, {BX, 4}
                                uint64_t bz1 : 1;  // {BZ, 1}
                                uint64_t by : 3;   // {BY, 0}, {BY, 1}, {BY, 2}
                                uint64_t by3 : 1;  // {BY, 3}
                                uint64_t ry : 5;   // {RY, 0}, {RY, 1}, {RY, 2}, {RY, 3}, {RY, 4},
                                uint64_t bz2 : 1;  // {BZ, 2}
                                uint64_t rz : 5;   // {RZ, 0}, {RZ, 1}, {RZ, 2}, {RZ, 3}, {BZ, 4}
                                uint64_t bz3 : 1;  // {BZ, 3}
                                uint64_t d : 5;    // { D, 0}, { D, 1}, { D, 2}, { D, 3}, { D, 4}
                                uint64_t indices : 46;
                            };
                            static_assert(sizeof(bc6h_mode6) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode6*>(sptr);

                            XMINT3 e0_A(int(m->rw), int(m->gw), int(m->bw));
                            XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));
                            XMINT3 e1_A(int(m->ry),
                                int(m->gy | (m->gy4 << 4)),
                                int(m->by | (m->by3 << 3) | (m->by4 << 4)));
                            XMINT3 e1_B(int(m->rz),
                                int(m->gz | (m->gz4 << 4)),
                                int(m->bz0 | (m->bz1 << 1) | (m->bz2 << 2) | (m->bz3 << 3) | (m->bz4 << 4)));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 9);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 9);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 9);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 5);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 5);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 5);

                                e1_A.x = SIGN_EXTEND(e1_A.x, 5);
                                e1_A.y = SIGN_EXTEND(e1_A.y, 5);
                                e1_A.z = SIGN_EXTEND(e1_A.z, 5);

                                e1_B.x = SIGN_EXTEND(e1_B.x, 5);
                                e1_B.y = SIGN_EXTEND(e1_B.y, 5);
                                e1_B.z = SIGN_EXTEND(e1_B.z, 5);
                            }

                            wprintf(L"\tMode 6 - [9 5 5 5] shape %llu\n", m->d);
                            wprintf(L"\t         E0(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E0(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         E1(A): (%04X, %04X, %04X)\n", e1_A.x & 0xFFFF, e1_A.y & 0xFFFF, e1_A.z & 0xFFFF);
                            wprintf(L"\t         E1(B): (%04X, %04X, %04X)\n", e1_B.x & 0xFFFF, e1_B.y & 0xFFFF, e1_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex3bpp(m->indices, 1, m->d);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x12:
                            // Mode 7 (5 bits, 10010)
                        {
                            struct bc6h_mode7
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 8;   // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}
                                uint64_t gz4 : 1;  // {GZ, 4}
                                uint64_t by4 : 1;  // {BY, 4}
                                uint64_t gw : 8;   // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}
                                uint64_t bz2 : 1;  // {BZ, 2}
                                uint64_t gy4 : 1;  // {GY, 4}
                                uint64_t bw : 8;   // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}
                                uint64_t bz3 : 1;  // {BZ, 3}
                                uint64_t bz4 : 1;  // {BZ, 4}
                                uint64_t rx : 6;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}, {RX, 4}, {RX, 5}
                                uint64_t gy : 4;   // {GY, 0}, {GY, 1}, {GY, 2}, {GY, 3}
                                uint64_t gx : 5;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}, {GX, 4}
                                uint64_t bz0 : 1;  // {BZ, 0}
                                uint64_t gz : 4;   // {GZ, 0}, {GZ, 1}, {GZ, 2}, {GZ, 3}
                                uint64_t bx : 5;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}, {BX, 4}
                                uint64_t bz1 : 1;  // {BZ, 1}
                                uint64_t by : 3;   // {BY, 0}, {BY, 1}, {BY, 2}
                                uint64_t by3 : 1;  // {BY, 3}
                                uint64_t ry : 6;   // {RY, 0}, {RY, 1}, {RY, 2}, {RY, 3}, {RY, 4}, {RY, 5}
                                uint64_t rz : 6;   // {RZ, 0}, {RZ, 1}, {RZ, 2}, {RZ, 3}, {RZ, 4}, {RZ, 5}
                                uint64_t d : 5;    // { D, 0}, { D, 1}, { D, 2}, { D, 3}, { D, 4}
                                uint64_t indices : 46;
                            };
                            static_assert(sizeof(bc6h_mode7) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode7*>(sptr);

                            XMINT3 e0_A(int(m->rw), int(m->gw), int(m->bw));
                            XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));
                            XMINT3 e1_A(int(m->ry),
                                int(m->gy | (m->gy4 << 4)),
                                int(m->by | (m->by3 << 3) | (m->by4 << 4)));
                            XMINT3 e1_B(int(m->rz),
                                int(m->gz | (m->gz4 << 4)),
                                int(m->bz0 | (m->bz1 << 1) | (m->bz2 << 2) | (m->bz3 << 3) | (m->bz4 << 4)));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 8);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 8);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 8);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 6);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 5);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 5);

                                e1_A.x = SIGN_EXTEND(e1_A.x, 6);
                                e1_A.y = SIGN_EXTEND(e1_A.y, 5);
                                e1_A.z = SIGN_EXTEND(e1_A.z, 5);

                                e1_B.x = SIGN_EXTEND(e1_B.x, 6);
                                e1_B.y = SIGN_EXTEND(e1_B.y, 5);
                                e1_B.z = SIGN_EXTEND(e1_B.z, 5);
                            }

                            wprintf(L"\tMode 7 - [8 6 5 5] shape %llu\n", m->d);
                            wprintf(L"\t         E0(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E0(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         E1(A): (%04X, %04X, %04X)\n", e1_A.x & 0xFFFF, e1_A.y & 0xFFFF, e1_A.z & 0xFFFF);
                            wprintf(L"\t         E1(B): (%04X, %04X, %04X)\n", e1_B.x & 0xFFFF, e1_B.y & 0xFFFF, e1_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex3bpp(m->indices, 1, m->d);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x16:
                            // Mode 8 (5 bits, 10110)
                        {
                            struct bc6h_mode8
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 8;   // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}
                                uint64_t bz0 : 1;  // {BZ, 0}
                                uint64_t by4 : 1;  // {BY, 4}
                                uint64_t gw : 8;   // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}
                                uint64_t gy5 : 1;  // {GY, 5}
                                uint64_t gy4 : 1;  // {GY, 4}
                                uint64_t bw : 8;   // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}
                                uint64_t gz5 : 1;  // {GZ, 5}
                                uint64_t bz4 : 1;  // {BZ, 4}
                                uint64_t rx : 5;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}, {RX, 4}
                                uint64_t gz4 : 1;  // {GZ, 4}
                                uint64_t gy : 4;   // {GY, 0}, {GY, 1}, {GY, 2}, {GY, 3}
                                uint64_t gx : 6;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}, {GX, 4}, {GX, 5}
                                uint64_t gz : 4;   // {GZ, 0}, {GZ, 1}, {GZ, 2}, {GZ, 3}
                                uint64_t bx : 5;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}, {BX, 4}
                                uint64_t bz1 : 1;  // {BZ, 1}
                                uint64_t by : 3;   // {BY, 0}, {BY, 1}, {BY, 2}
                                uint64_t by3 : 1;  // {BY, 3}
                                uint64_t ry : 5;   // {RY, 0}, {RY, 1}, {RY, 2}, {RY, 3}, {RY, 4}
                                uint64_t bz2 : 1;  // {BZ, 2}
                                uint64_t rz : 5;   // {RZ, 0}, {RZ, 1}, {RZ, 2}, {RZ, 3}, {RZ, 4}
                                uint64_t bz3 : 1;  // {BZ, 3}
                                uint64_t d : 5;    // { D, 0}, { D, 1}, { D, 2}, { D, 3}, { D, 4}
                                uint64_t indices : 46;
                            };
                            static_assert(sizeof(bc6h_mode8) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode8*>(sptr);

                            XMINT3 e0_A(int(m->rw), int(m->gw), int(m->bw));
                            XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));
                            XMINT3 e1_A(int(m->ry),
                                int(m->gy | (m->gy4 << 4) | (m->gy5 << 5)),
                                int(m->by | (m->by3 << 3) | (m->by4 << 4)));
                            XMINT3 e1_B(int(m->rz),
                                int(m->gz | (m->gz4 << 4) | (m->gz5 << 5)),
                                int(m->bz0 | (m->bz1 << 1) | (m->bz2 << 2) | (m->bz3 << 3) | (m->bz4 << 4)));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 8);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 8);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 8);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 5);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 6);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 5);

                                e1_A.x = SIGN_EXTEND(e1_A.x, 5);
                                e1_A.y = SIGN_EXTEND(e1_A.y, 6);
                                e1_A.z = SIGN_EXTEND(e1_A.z, 5);

                                e1_B.x = SIGN_EXTEND(e1_B.x, 5);
                                e1_B.y = SIGN_EXTEND(e1_B.y, 6);
                                e1_B.z = SIGN_EXTEND(e1_B.z, 5);
                            }

                            wprintf(L"\tMode 8 - [8 5 6 5] shape %llu\n", m->d);
                            wprintf(L"\t         E0(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E0(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         E1(A): (%04X, %04X, %04X)\n", e1_A.x & 0xFFFF, e1_A.y & 0xFFFF, e1_A.z & 0xFFFF);
                            wprintf(L"\t         E1(B): (%04X, %04X, %04X)\n", e1_B.x & 0xFFFF, e1_B.y & 0xFFFF, e1_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex3bpp(m->indices, 1, m->d);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x1A:
                            // Mode 9 (5 bits, 11010)
                        {
                            struct bc6h_mode9
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 8;   // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}
                                uint64_t bz1 : 1;  // {BZ, 1}
                                uint64_t by4 : 1;  // {BY, 4}
                                uint64_t gw : 8;   // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}
                                uint64_t by5 : 1;  // {BY, 5}
                                uint64_t gy4 : 1;  // {GY, 4}
                                uint64_t bw : 8;   // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}
                                uint64_t bz5 : 1;  // {BZ, 5}
                                uint64_t bz4 : 1;  // {BZ, 4}
                                uint64_t rx : 5;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}, {RX, 4}
                                uint64_t gz4 : 1;  // {GZ, 4}
                                uint64_t gy : 4;   // {GY, 0}, {GY, 1}, {GY, 2}, {GY, 3}
                                uint64_t gx : 5;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}, {GX, 4}
                                uint64_t bz0 : 1;  // {BZ, 0}
                                uint64_t gz : 4;   // {GZ, 0}, {GZ, 1}, {GZ, 2}, {GZ, 3}
                                uint64_t bx : 6;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}, {BX, 4}, {BX, 5}
                                uint64_t by : 3;   // {BY, 0}, {BY, 1}, {BY, 2}
                                uint64_t by3 : 1;  // {BY, 3}
                                uint64_t ry : 5;   // {RY, 0}, {RY, 1}, {RY, 2}, {RY, 3}, {RY, 4}
                                uint64_t bz2 : 1;  // {BZ, 2}
                                uint64_t rz : 5;   // {RZ, 0}, {RZ, 1}, {RZ, 2}, {RZ, 3}, {RZ, 4}
                                uint64_t bz3 : 1;  // {BZ, 3}
                                uint64_t d : 5;    // { D, 0}, { D, 1}, { D, 2}, { D, 3}, { D, 4}
                                uint64_t indices : 46;
                            };
                            static_assert(sizeof(bc6h_mode9) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode9*>(sptr);

                            XMINT3 e0_A(int(m->rw), int(m->gw), int(m->bw));
                            XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));
                            XMINT3 e1_A(int(m->ry),
                                int(m->gy | (m->gy4 << 4)),
                                int(m->by | (m->by3 << 3) | (m->by4 << 4) | (m->by5 << 5)));
                            XMINT3 e1_B(int(m->rz),
                                int(m->gz | (m->gz4 << 4)),
                                int(m->bz0 | (m->bz1 << 1) | (m->bz2 << 2) | (m->bz3 << 3) | (m->bz4 << 4) | (m->bz5 << 5)));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 8);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 8);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 8);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 5);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 5);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 6);

                                e1_A.x = SIGN_EXTEND(e1_A.x, 5);
                                e1_A.y = SIGN_EXTEND(e1_A.y, 5);
                                e1_A.z = SIGN_EXTEND(e1_A.z, 6);

                                e1_B.x = SIGN_EXTEND(e1_B.x, 5);
                                e1_B.y = SIGN_EXTEND(e1_B.y, 5);
                                e1_B.z = SIGN_EXTEND(e1_B.z, 6);
                            }

                            wprintf(L"\tMode 9 - [8 5 5 6] shape %llu\n", m->d);
                            wprintf(L"\t         E0(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E0(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         E1(A): (%04X, %04X, %04X)\n", e1_A.x & 0xFFFF, e1_A.y & 0xFFFF, e1_A.z & 0xFFFF);
                            wprintf(L"\t         E1(B): (%04X, %04X, %04X)\n", e1_B.x & 0xFFFF, e1_B.y & 0xFFFF, e1_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex3bpp(m->indices, 1, m->d);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x1E:
                            // Mode 10 (5 bits, 11110)
                        {
                            struct bc6h_mode10
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 6;   // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}
                                uint64_t gz4 : 1;  // {GZ, 4}
                                uint64_t bz : 2;  // {BZ, 0}, {BZ, 1}
                                uint64_t by4 : 1;  // {BY, 4}
                                uint64_t gw : 6;   // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}
                                uint64_t gy5 : 1;  // {GY, 5}
                                uint64_t by5 : 1;  // {BY, 5}
                                uint64_t bz2 : 1;  // {BZ, 2}
                                uint64_t gy4 : 1;  // {GY, 4}
                                uint64_t bw : 6;   // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {GZ, 5}
                                uint64_t bz3 : 1;  // {BZ, 3}
                                uint64_t bz5 : 1;  // {BZ, 5}
                                uint64_t bz4 : 1;  // {BZ, 4}
                                uint64_t rx : 6;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}, {RX, 4}, {RX, 5}
                                uint64_t gy : 4;   // {GY, 0}, {GY, 1}, {GY, 2}, {GY, 3}
                                uint64_t gx : 6;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}, {GX, 4}, {GX, 5}
                                uint64_t gz : 4;   // {GZ, 0}, {GZ, 1}, {GZ, 2}, {GZ, 3}
                                uint64_t bx : 6;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}, {BX, 4}, {BX, 5}
                                uint64_t by : 3;   // {BY, 0}, {BY, 1}, {BY, 2}
                                uint64_t by3 : 1;  // {BY, 3}
                                uint64_t ry : 6;   // {RY, 0}, {RY, 1}, {RY, 2}, {RY, 3}, {RY, 4}, {RY, 5}
                                uint64_t rz : 6;   // {RZ, 0}, {RZ, 1}, {RZ, 2}, {RZ, 3}, {RZ, 4}, {RZ, 5}
                                uint64_t d : 5;    // { D, 0}, { D, 1}, { D, 2}, { D, 3}, { D, 4}
                                uint64_t indices : 46;
                            };
                            static_assert(sizeof(bc6h_mode10) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode10*>(sptr);

                            XMINT3 e0_A(int(m->rw), int(m->gw), int(m->bw));
                            XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));
                            XMINT3 e1_A(int(m->ry),
                                int(m->gy | (m->gy4 << 4) | (m->gy5 << 5)),
                                int(m->by | (m->by3 << 3) | (m->by4 << 4) | (m->by5 << 5)));
                            XMINT3 e1_B(int(m->rz),
                                int(m->gz | (m->gz4 << 4)),
                                int(m->bz | (m->bz2 << 2) | (m->bz3 << 3) | (m->bz4 << 4) | (m->bz5 << 5)));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 6);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 6);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 6);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 6);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 6);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 6);

                                e1_A.x = SIGN_EXTEND(e1_A.x, 6);
                                e1_A.y = SIGN_EXTEND(e1_A.y, 6);
                                e1_A.z = SIGN_EXTEND(e1_A.z, 6);

                                e1_B.x = SIGN_EXTEND(e1_B.x, 6);
                                e1_B.y = SIGN_EXTEND(e1_B.y, 6);
                                e1_B.z = SIGN_EXTEND(e1_B.z, 6);
                            }

                            wprintf(L"\tMode 10 - [6 6 6 6] shape %llu\n", m->d);
                            wprintf(L"\t         E0(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E0(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         E1(A): (%04X, %04X, %04X)\n", e1_A.x & 0xFFFF, e1_A.y & 0xFFFF, e1_A.z & 0xFFFF);
                            wprintf(L"\t         E1(B): (%04X, %04X, %04X)\n", e1_B.x & 0xFFFF, e1_B.y & 0xFFFF, e1_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex3bpp(m->indices, 1, m->d);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x03:
                            // Mode 11 (5 bits, 00011)
                        {
                            struct bc6h_mode11
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 10;  // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}, {RW, 8}, {RW, 9}
                                uint64_t gw : 10;  // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}, {GW, 8}, {GW, 9}
                                uint64_t bw : 10;  // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}, {BW, 8}, {BW, 9}
                                uint64_t rx : 10;  // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}, {RX, 4}, {RX, 5}, {RX, 6}, {RX, 7}, {RX, 8}, {RX, 9}
                                uint64_t gx : 10;  // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}, {GX, 4}, {GX, 5}, {GX, 6}, {GX, 7}, {GX, 8}, {GX, 9}
                                uint64_t bx : 9;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}, {BX, 4}, {BX, 5}, {BX, 6}, {BX, 7}, {BX, 8}
                                uint64_t bx9 : 1;  // {BX, 9}
                                uint64_t indices : 63;
                            };
                            static_assert(sizeof(bc6h_mode11) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode11*>(sptr);

                            XMINT3 e0_A(int(m->rw), int(m->gw), int(m->bw));
                            XMINT3 e0_B(int(m->rx), int(m->gx),
                                int(m->bx | (m->bx9 << 9)));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 10);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 10);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 10);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 10);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 10);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 10);
                            }

                            wprintf(L"\tMode 11 - [10 10]\n");
                            wprintf(L"\t         E(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex4bpp(m->indices, 0, 0);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x07:
                            // Mode 12 (5 bits, 00111)
                        {
                            struct bc6h_mode12
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 10;  // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}, {RW, 8}, {RW, 9}
                                uint64_t gw : 10;  // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}, {GW, 8}, {GW, 9}
                                uint64_t bw : 10;  // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}, {BW, 8}, {BW, 9}
                                uint64_t rx : 9;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}, {RX, 4}, {RX, 5}, {RX, 6}, {RX, 7}, {RX, 8}
                                uint64_t rw10 : 1; // {RW,10}
                                uint64_t gx : 9;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}, {GX, 4}, {GX, 5}, {GX, 6}, {GX, 7}, {GX, 8}
                                uint64_t gw10 : 1; // {GW,10}
                                uint64_t bx : 9;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}, {BX, 4}, {BX, 5}, {BX, 6}, {BX, 7}, {BX, 8}
                                uint64_t bw10 : 1; // {BW,10}
                                uint64_t indices : 63;
                            };
                            static_assert(sizeof(bc6h_mode12) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode12*>(sptr);

                            XMINT3 e0_A(int(m->rw | (m->rw10 << 10)),
                                int(m->gw | (m->gw10 << 10)),
                                int(m->bw | (m->bw10 << 10)));
                            XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 11);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 11);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 11);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 9);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 9);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 9);
                            }

                            wprintf(L"\tMode 12 - [11 9]\n");
                            wprintf(L"\t         E(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex4bpp(m->indices, 0, 0);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x0B:
                            // Mode 13 (5 bits, 01011)
                        {
                            struct bc6h_mode13
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 10;  // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}, {RW, 8}, {RW, 9}
                                uint64_t gw : 10;  // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}, {GW, 8}, {GW, 9}
                                uint64_t bw : 10;  // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}, {BW, 8}, {BW, 9}
                                uint64_t rx : 8;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}, {RX, 4}, {RX, 5}, {RX, 6}, {RX, 7}
                                uint64_t rw11 : 1; // {RW,11}
                                uint64_t rw10 : 1; // {RW,10}
                                uint64_t gx : 8;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}, {GX, 4}, {GX, 5}, {GX, 6}, {GX, 7}
                                uint64_t gw11 : 1; // {GW,11}
                                uint64_t gw10 : 1; // {GW,10}
                                uint64_t bx : 8;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}, {BX, 4}, {BX, 5}, {BX, 6}, {BX, 7}
                                uint64_t bw11 : 1; // {BW,11}
                                uint64_t bw10 : 1; // {BW,10}
                                uint64_t indices : 63;
                            };
                            static_assert(sizeof(bc6h_mode13) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode13*>(sptr);

                            XMINT3 e0_A(int(m->rw | (m->rw10 << 10) | (m->rw11 << 11)),
                                int(m->gw | (m->gw10 << 10) | (m->gw11 << 11)),
                                int(m->bw | (m->bw10 << 10) | (m->bw11 << 11)));
                            XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 12);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 12);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 12);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 8);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 8);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 8);
                            }

                            wprintf(L"\tMode 13 - [12 8]\n");
                            wprintf(L"\t         E(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex4bpp(m->indices, 0, 0);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x0F:
                            // Mode 14 (5 bits, 01111)
                        {
                            struct bc6h_mode14
                            {
                                uint64_t mode : 5; // { M, 0}, { M, 1}, { M, 2}, { M, 3}, { M, 4}
                                uint64_t rw : 10;  // {RW, 0}, {RW, 1}, {RW, 2}, {RW, 3}, {RW, 4}, {RW, 5}, {RW, 6}, {RW, 7}, {RW, 8}, {RW, 9}
                                uint64_t gw : 10;  // {GW, 0}, {GW, 1}, {GW, 2}, {GW, 3}, {GW, 4}, {GW, 5}, {GW, 6}, {GW, 7}, {GW, 8}, {GW, 9}
                                uint64_t bw : 10;  // {BW, 0}, {BW, 1}, {BW, 2}, {BW, 3}, {BW, 4}, {BW, 5}, {BW, 6}, {BW, 7}, {BW, 8}, {BW, 9}
                                uint64_t rx : 4;   // {RX, 0}, {RX, 1}, {RX, 2}, {RX, 3}
                                uint64_t rw15 : 1; // {RW,15}
                                uint64_t rw14 : 1; // {RW,14}
                                uint64_t rw13 : 1; // {RW,13}
                                uint64_t rw12 : 1; // {RW,12}
                                uint64_t rw11 : 1; // {RW,11}
                                uint64_t rw10 : 1; // {RW,10}
                                uint64_t gx : 4;   // {GX, 0}, {GX, 1}, {GX, 2}, {GX, 3}
                                uint64_t gw15 : 1; // {GW,15}
                                uint64_t gw14 : 1; // {GW,14}
                                uint64_t gw13 : 1; // {GW,13}
                                uint64_t gw12 : 1; // {GW,12}
                                uint64_t gw11 : 1; // {GW,11}
                                uint64_t gw10 : 1; // {GW,10}
                                uint64_t bx : 4;   // {BX, 0}, {BX, 1}, {BX, 2}, {BX, 3}
                                uint64_t bw15 : 1; // {BW,15}
                                uint64_t bw14 : 1; // {BW,14}
                                uint64_t bw13 : 1; // {BW,13}
                                uint64_t bw12 : 1; // {BW,12}
                                uint64_t bw11 : 1; // {BW,11}
                                uint64_t bw10 : 1; // {BW,10}
                                uint64_t indices : 63;
                            };
                            static_assert(sizeof(bc6h_mode14) == 16, "Block size must be 16 bytes");

                            bool bSigned = (image.format == DXGI_FORMAT_BC6H_SF16) ? true : false;

                            auto m = reinterpret_cast<const bc6h_mode14*>(sptr);

                            XMINT3 e0_A(int(m->rw | (m->rw10 << 10) | (m->rw11 << 11) | (m->rw12 << 12) | (m->rw13 << 13) | (m->rw14 << 14) | (m->rw15 << 15)),
                                int(m->gw | (m->gw10 << 10) | (m->gw11 << 11) | (m->gw12 << 12) | (m->gw13 << 13) | (m->gw14 << 14) | (m->gw15 << 15)),
                                int(m->bw | (m->bw10 << 10) | (m->bw11 << 11) | (m->bw12 << 12) | (m->bw13 << 13) | (m->bw14 << 14) | (m->bw15 << 15)));
                            XMINT3 e0_B(int(m->rx), int(m->gx), int(m->bx));

                            if (bSigned)
                            {
                                e0_A.x = SIGN_EXTEND(e0_A.x, 16);
                                e0_A.y = SIGN_EXTEND(e0_A.y, 16);
                                e0_A.z = SIGN_EXTEND(e0_A.z, 16);

                                e0_B.x = SIGN_EXTEND(e0_B.x, 4);
                                e0_B.y = SIGN_EXTEND(e0_B.y, 4);
                                e0_B.z = SIGN_EXTEND(e0_B.z, 4);
                            }

                            wprintf(L"\tMode 14 - [16 4]\n");
                            wprintf(L"\t         E(A): (%04X, %04X, %04X)\n", e0_A.x & 0xFFFF, e0_A.y & 0xFFFF, e0_A.z & 0xFFFF);
                            wprintf(L"\t         E(B): (%04X, %04X, %04X)\n", e0_B.x & 0xFFFF, e0_B.y & 0xFFFF, e0_B.z & 0xFFFF);
                            wprintf(L"\t         Index: ");
                            PrintIndex4bpp(m->indices, 0, 0);
                            wprintf(L"\n");
                        }
                        break;

                        case 0x13: // Reserved mode (5 bits, 10011)
                            wprintf(L"\tERROR - Reserved mode 10011\n");
                            break;

                        case 0x17: // Reserved mode (5 bits, 10111)
                            wprintf(L"\tERROR - Reserved mode 10011\n");
                            break;

                        case 0x1B: // Reserved mode (5 bits, 11011)
                            wprintf(L"\tERROR - Reserved mode 11011\n");
                            break;

                        case 0x1F: // Reserved mode (5 bits, 11111)
                            wprintf(L"\tERROR - Reserved mode 11111\n");
                            break;

                        default:
                            break;
                        }
                        break;
                    }
                    break;

                case DXGI_FORMAT_BC7_UNORM:
                case DXGI_FORMAT_BC7_UNORM_SRGB:
                    // http://msdn.microsoft.com/en-us/library/windows/desktop/hh308954.aspx

                    if (*sptr & 0x01)
                    {
                        // Mode 0 (1)
                        struct bc7_mode0
                        {
                            uint64_t mode : 1;
                            uint64_t part : 4;
                            uint64_t r0 : 4;
                            uint64_t r1 : 4;
                            uint64_t r2 : 4;
                            uint64_t r3 : 4;
                            uint64_t r4 : 4;
                            uint64_t r5 : 4;
                            uint64_t g0 : 4;
                            uint64_t g1 : 4;
                            uint64_t g2 : 4;
                            uint64_t g3 : 4;
                            uint64_t g4 : 4;
                            uint64_t g5 : 4;
                            uint64_t b0 : 4;
                            uint64_t b1 : 4;
                            uint64_t b2 : 3;
                            uint64_t b2n : 1;
                            uint64_t b3 : 4;
                            uint64_t b4 : 4;
                            uint64_t b5 : 4;
                            uint64_t P0 : 1;
                            uint64_t P1 : 1;
                            uint64_t P2 : 1;
                            uint64_t P3 : 1;
                            uint64_t P4 : 1;
                            uint64_t P5 : 1;
                            uint64_t index : 45;
                        };
                        static_assert(sizeof(bc7_mode0) == 16, "Block size must be 16 bytes");

                        auto m = reinterpret_cast<const bc7_mode0*>(sptr);

                        wprintf(L"\tMode 0 - [4 4 4] partition %llu\n", m->part);
                        wprintf(L"\t         E0:(%0.3f, %0.3f, %0.3f)\n", float((m->r0 << 1) | m->P0) / 31.f, float((m->g0 << 1) | m->P0) / 31.f, float((m->b0 << 1) | m->P0) / 31.f);
                        wprintf(L"\t         E1:(%0.3f, %0.3f, %0.3f)\n", float((m->r1 << 1) | m->P1) / 31.f, float((m->g1 << 1) | m->P1) / 31.f, float((m->b1 << 1) | m->P1) / 31.f);
                        wprintf(L"\t         E2:(%0.3f, %0.3f, %0.3f)\n", float((m->r2 << 1) | m->P2) / 31.f, float((m->g2 << 1) | m->P2) / 31.f, float(((m->b2 | (m->b2n << 3)) << 1) | m->P2) / 31.f);
                        wprintf(L"\t         E3:(%0.3f, %0.3f, %0.3f)\n", float((m->r3 << 1) | m->P3) / 31.f, float((m->g3 << 1) | m->P3) / 31.f, float((m->b3 << 1) | m->P3) / 31.f);
                        wprintf(L"\t         E4:(%0.3f, %0.3f, %0.3f)\n", float((m->r4 << 1) | m->P4) / 31.f, float((m->g4 << 1) | m->P4) / 31.f, float((m->b4 << 1) | m->P4) / 31.f);
                        wprintf(L"\t         E5:(%0.3f, %0.3f, %0.3f)\n", float((m->r5 << 1) | m->P5) / 31.f, float((m->g5 << 1) | m->P5) / 31.f, float((m->b5 << 1) | m->P5) / 31.f);
                        wprintf(L"\t      Index: ");
                        PrintIndex2bpp(m->index, 2, m->part);
                        wprintf(L"\n");
                    }
                    else if (*sptr & 0x02)
                    {
                        // Mode 1 (01)
                        struct bc7_mode1
                        {
                            uint64_t mode : 2;
                            uint64_t part : 6;
                            uint64_t r0 : 6;
                            uint64_t r1 : 6;
                            uint64_t r2 : 6;
                            uint64_t r3 : 6;
                            uint64_t g0 : 6;
                            uint64_t g1 : 6;
                            uint64_t g2 : 6;
                            uint64_t g3 : 6;
                            uint64_t b0 : 6;
                            uint64_t b1 : 2;
                            uint64_t b1n : 4;
                            uint64_t b2 : 6;
                            uint64_t b3 : 6;
                            uint64_t P0 : 1;
                            uint64_t P1 : 1;
                            uint64_t index : 46;
                        };
                        static_assert(sizeof(bc7_mode1) == 16, "Block size must be 16 bytes");

                        auto m = reinterpret_cast<const bc7_mode1*>(sptr);

                        wprintf(L"\tMode 1 - [6 6 6] partition %llu\n", m->part);
                        wprintf(L"\t         E0:(%0.3f, %0.3f, %0.3f)\n", float((m->r0 << 1) | m->P0) / 127.f, float((m->g0 << 1) | m->P0) / 127.f, float((m->b0 << 1) | m->P0) / 127.f);
                        wprintf(L"\t         E1:(%0.3f, %0.3f, %0.3f)\n", float((m->r1 << 1) | m->P0) / 127.f, float((m->g1 << 1) | m->P0) / 127.f, float(((m->b1 | (m->b1n << 2)) << 1) | m->P0) / 127.f);
                        wprintf(L"\t         E2:(%0.3f, %0.3f, %0.3f)\n", float((m->r2 << 1) | m->P1) / 127.f, float((m->g2 << 1) | m->P1) / 127.f, float((m->b2 << 1) | m->P1) / 127.f);
                        wprintf(L"\t         E3:(%0.3f, %0.3f, %0.3f)\n", float((m->r3 << 1) | m->P1) / 127.f, float((m->g3 << 1) | m->P1) / 127.f, float((m->b3 << 1) | m->P1) / 127.f);
                        wprintf(L"\t      Index: ");
                        PrintIndex3bpp(m->index, 1, m->part);
                        wprintf(L"\n");
                    }
                    else if (*sptr & 0x04)
                    {
                        // Mode 2 (001)
                        struct bc7_mode2
                        {
                            uint64_t mode : 3;
                            uint64_t part : 6;
                            uint64_t r0 : 5;
                            uint64_t r1 : 5;
                            uint64_t r2 : 5;
                            uint64_t r3 : 5;
                            uint64_t r4 : 5;
                            uint64_t r5 : 5;
                            uint64_t g0 : 5;
                            uint64_t g1 : 5;
                            uint64_t g2 : 5;
                            uint64_t g3 : 5;
                            uint64_t g4 : 5;
                            uint64_t g5 : 5;
                            uint64_t b0 : 5;
                            uint64_t b1 : 5;
                            uint64_t b2 : 5;
                            uint64_t b3 : 5;
                            uint64_t b4 : 5;
                            uint64_t b5 : 5;
                            uint64_t index : 29;
                        };
                        static_assert(sizeof(bc7_mode2) == 16, "Block size must be 16 bytes");

                        auto m = reinterpret_cast<const bc7_mode2*>(sptr);

                        wprintf(L"\tMode 2 - [5 5 5] partition %llu\n", m->part);
                        wprintf(L"\t         E0:(%0.3f, %0.3f, %0.3f)\n", float(m->r0) / 31.f, float(m->g0) / 31.f, float(m->b0) / 31.f);
                        wprintf(L"\t         E1:(%0.3f, %0.3f, %0.3f)\n", float(m->r1) / 31.f, float(m->g1) / 31.f, float(m->b1) / 31.f);
                        wprintf(L"\t         E2:(%0.3f, %0.3f, %0.3f)\n", float(m->r2) / 31.f, float(m->g2) / 31.f, float(m->b2) / 31.f);
                        wprintf(L"\t         E3:(%0.3f, %0.3f, %0.3f)\n", float(m->r3) / 31.f, float(m->g3) / 31.f, float(m->b3) / 31.f);
                        wprintf(L"\t         E4:(%0.3f, %0.3f, %0.3f)\n", float(m->r4) / 31.f, float(m->g4) / 31.f, float(m->b4) / 31.f);
                        wprintf(L"\t         E5:(%0.3f, %0.3f, %0.3f)\n", float(m->r5) / 31.f, float(m->g5) / 31.f, float(m->b5) / 31.f);
                        wprintf(L"\t      Index: ");
                        PrintIndex2bpp(m->index, 2, m->part);
                        wprintf(L"\n");
                    }
                    else if (*sptr & 0x08)
                    {
                        // Mode 3 (0001)
                        struct bc7_mode3
                        {
                            uint64_t mode : 4;
                            uint64_t part : 6;
                            uint64_t r0 : 7;
                            uint64_t r1 : 7;
                            uint64_t r2 : 7;
                            uint64_t r3 : 7;
                            uint64_t g0 : 7;
                            uint64_t g1 : 7;
                            uint64_t g2 : 7;
                            uint64_t g3 : 5;
                            uint64_t g3n : 2;
                            uint64_t b0 : 7;
                            uint64_t b1 : 7;
                            uint64_t b2 : 7;
                            uint64_t b3 : 7;
                            uint64_t P0 : 1;
                            uint64_t P1 : 1;
                            uint64_t P2 : 1;
                            uint64_t P3 : 1;
                            uint64_t index : 30;
                        };
                        static_assert(sizeof(bc7_mode3) == 16, "Block size must be 16 bytes");

                        auto m = reinterpret_cast<const bc7_mode3*>(sptr);

                        wprintf(L"\tMode 3 - [7 7 7] partition %llu\n", m->part);
                        wprintf(L"\t         E0:(%0.3f, %0.3f, %0.3f)\n", float((m->r0 << 1) | m->P0) / 255.f, float((m->g0 << 1) | m->P0) / 255.f, float((m->b0 << 1) | m->P0) / 255.f);
                        wprintf(L"\t         E1:(%0.3f, %0.3f, %0.3f)\n", float((m->r1 << 1) | m->P1) / 255.f, float((m->g1 << 1) | m->P1) / 255.f, float((m->b1 << 1) | m->P1) / 255.f);
                        wprintf(L"\t         E2:(%0.3f, %0.3f, %0.3f)\n", float((m->r2 << 1) | m->P2) / 255.f, float((m->g2 << 1) | m->P2) / 255.f, float((m->b2 << 1) | m->P2) / 255.f);
                        wprintf(L"\t         E3:(%0.3f, %0.3f, %0.3f)\n", float((m->r3 << 1) | m->P3) / 255.f, float(((m->g3 | (m->g3n << 5)) << 1) | m->P3) / 255.f, float((m->b3 << 1) | m->P3) / 255.f);
                        wprintf(L"\t      Index: ");
                        PrintIndex2bpp(m->index, 1, m->part);
                        wprintf(L"\n");
                    }
                    else if (*sptr & 0x10)
                    {
                        // Mode 4 (00001)
                        struct bc7_mode4
                        {
                            uint64_t mode : 5;
                            uint64_t rot : 2;
                            uint64_t idx : 1;
                            uint64_t r0 : 5;
                            uint64_t r1 : 5;
                            uint64_t g0 : 5;
                            uint64_t g1 : 5;
                            uint64_t b0 : 5;
                            uint64_t b1 : 5;
                            uint64_t a0 : 6;
                            uint64_t a1 : 6;
                            uint64_t color_index : 14;
                            uint64_t color_indexn : 17;
                            uint64_t alpha_index : 47;
                        };
                        static_assert(sizeof(bc7_mode4) == 16, "Block size must be 16 bytes");

                        auto m = reinterpret_cast<const bc7_mode4*>(sptr);

                        wprintf(L"\tMode 4 - [5 5 5 A6] indx mode %ls, rot-bits %llu%ls\n", m->idx ? L"3-bit" : L"2-bit", m->rot, GetRotBits(m->rot));
                        wprintf(L"\t         C0:(%0.3f, %0.3f, %0.3f)\n", float(m->r0) / 31.f, float(m->g0) / 31.f, float(m->b0) / 31.f);
                        wprintf(L"\t         C1:(%0.3f, %0.3f, %0.3f)\n", float(m->r1) / 31.f, float(m->g1) / 31.f, float(m->b1) / 31.f);
                        wprintf(L"\t         A0:(%0.3f)\n", float(m->a0) / 63.f);
                        wprintf(L"\t         A1:(%0.3f)\n", float(m->a1) / 63.f);
                        wprintf(L"\t    Colors: ");

                        uint64_t color_index = uint64_t(m->color_index) | uint64_t(m->color_indexn << 14);
                        if (m->idx)
                            PrintIndex3bpp(color_index, 0, 0);
                        else
                            PrintIndex2bpp(color_index, 0, 0);
                        wprintf(L"\n");
                        wprintf(L"\t     Alpha: ");
                        PrintIndex3bpp(m->alpha_index, 0, 0);
                        wprintf(L"\n");
                    }
                    else if (*sptr & 0x20)
                    {
                        // Mode 5 (000001)
                        struct bc7_mode5
                        {
                            uint64_t mode : 6;
                            uint64_t rot : 2;
                            uint64_t r0 : 7;
                            uint64_t r1 : 7;
                            uint64_t g0 : 7;
                            uint64_t g1 : 7;
                            uint64_t b0 : 7;
                            uint64_t b1 : 7;
                            uint64_t a0 : 8;
                            uint64_t a1 : 6;
                            uint64_t a1n : 2;
                            uint64_t color_index : 31;
                            uint64_t alpha_index : 31;
                        };
                        static_assert(sizeof(bc7_mode5) == 16, "Block size must be 16 bytes");

                        auto m = reinterpret_cast<const bc7_mode5*>(sptr);

                        wprintf(L"\tMode 5 - [7 7 7 A8] rot-bits %llu%ls\n", m->rot, GetRotBits(m->rot));
                        wprintf(L"\t         C0:(%0.3f, %0.3f, %0.3f)\n", float(m->r0) / 127.f, float(m->g0) / 127.f, float(m->b0) / 127.f);
                        wprintf(L"\t         C1:(%0.3f, %0.3f, %0.3f)\n", float(m->r1) / 127.f, float(m->g1) / 127.f, float(m->b1) / 127.f);
                        wprintf(L"\t         A0:(%0.3f)\n", float(m->a0) / 255.f);
                        wprintf(L"\t         A1:(%0.3f)\n", float(m->a1 | (m->a1n << 6)) / 255.f);
                        wprintf(L"\t    Colors: ");
                        PrintIndex2bpp(m->color_index, 0, 0);
                        wprintf(L"\n");
                        wprintf(L"\t     Alpha: ");
                        PrintIndex2bpp(m->alpha_index, 0, 0);
                        wprintf(L"\n");
                    }
                    else if (*sptr & 0x40)
                    {
                        // Mode 6 (0000001)
                        struct bc7_mode6
                        {
                            uint64_t mode : 7;
                            uint64_t r0 : 7;
                            uint64_t r1 : 7;
                            uint64_t g0 : 7;
                            uint64_t g1 : 7;
                            uint64_t b0 : 7;
                            uint64_t b1 : 7;
                            uint64_t a0 : 7;
                            uint64_t a1 : 7;
                            uint64_t P0 : 1;
                            uint64_t P1 : 1;
                            uint64_t index : 63;

                        };
                        static_assert(sizeof(bc7_mode6) == 16, "Block size must be 16 bytes");

                        auto m = reinterpret_cast<const bc7_mode6*>(sptr);

                        wprintf(L"\tMode 6 - [7 7 7 A7]\n");
                        wprintf(L"\t         C0:(%0.3f, %0.3f, %0.3f)\n", float((m->r0 << 1) | m->P0) / 255.f, float((m->g0 << 1) | m->P0) / 255.f, float((m->b0 << 1) | m->P0) / 255.f);
                        wprintf(L"\t         C1:(%0.3f, %0.3f, %0.3f)\n", float((m->r1 << 1) | m->P1) / 255.f, float((m->g1 << 1) | m->P1) / 255.f, float((m->b1 << 1) | m->P1) / 255.f);
                        wprintf(L"\t         A0:(%0.3f)\n", float((m->a0 << 1) | m->P0) / 255.f);
                        wprintf(L"\t         A1:(%0.3f)\n", float((m->a1 << 1) | m->P1) / 255.f);
                        wprintf(L"\t      Index: ");
                        PrintIndex4bpp(m->index, 0, 0);
                        wprintf(L"\n");
                    }
                    else if (*sptr & 0x80)
                    {
                        // Mode 7 (00000001)
                        struct bc7_mode7
                        {
                            uint64_t mode : 8;
                            uint64_t part : 6;
                            uint64_t r0 : 5;
                            uint64_t r1 : 5;
                            uint64_t r2 : 5;
                            uint64_t r3 : 5;
                            uint64_t g0 : 5;
                            uint64_t g1 : 5;
                            uint64_t g2 : 5;
                            uint64_t g3 : 5;
                            uint64_t b0 : 5;
                            uint64_t b1 : 5;
                            uint64_t b2 : 5;
                            uint64_t b3 : 5;
                            uint64_t a0 : 5;
                            uint64_t a1 : 5;
                            uint64_t a2 : 5;
                            uint64_t a3 : 5;
                            uint64_t P0 : 1;
                            uint64_t P1 : 1;
                            uint64_t P2 : 1;
                            uint64_t P3 : 1;
                            uint64_t index : 30;

                        };
                        static_assert(sizeof(bc7_mode7) == 16, "Block size must be 16 bytes");

                        auto m = reinterpret_cast<const bc7_mode7*>(sptr);

                        wprintf(L"\tMode 7 - [5 5 5 A5] partition %llu\n", m->part);
                        wprintf(L"\t         C0:(%0.3f, %0.3f, %0.3f)\n", float((m->r0 << 1) | m->P0) / 63.f, float((m->g0 << 1) | m->P0) / 63.f, float((m->b0 << 1) | m->P0) / 63.f);
                        wprintf(L"\t         C1:(%0.3f, %0.3f, %0.3f)\n", float((m->r1 << 1) | m->P1) / 63.f, float((m->g1 << 1) | m->P1) / 63.f, float((m->b1 << 1) | m->P1) / 63.f);
                        wprintf(L"\t         C2:(%0.3f, %0.3f, %0.3f)\n", float((m->r2 << 1) | m->P2) / 63.f, float((m->g2 << 1) | m->P2) / 63.f, float((m->b2 << 1) | m->P2) / 63.f);
                        wprintf(L"\t         C3:(%0.3f, %0.3f, %0.3f)\n", float((m->r3 << 1) | m->P3) / 63.f, float((m->g3 << 1) | m->P3) / 63.f, float((m->b3 << 1) | m->P3) / 63.f);
                        wprintf(L"\t         A0:(%0.3f)\n", float((m->a0 << 1) | m->P0) / 63.f);
                        wprintf(L"\t         A1:(%0.3f)\n", float((m->a1 << 1) | m->P1) / 63.f);
                        wprintf(L"\t         A2:(%0.3f)\n", float((m->a2 << 1) | m->P2) / 63.f);
                        wprintf(L"\t         A3:(%0.3f)\n", float((m->a3 << 1) | m->P3) / 63.f);
                        wprintf(L"\t      Index: ");
                        PrintIndex4bpp(m->index, 1, m->part);
                        wprintf(L"\n");
                    }
                    else
                    {
                        // Reserved mode 8 (00000000)
                        wprintf(L"\tERROR - Reserved mode 8\n");
                    }
                    break;
                }
            }
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
    DWORD dwFilter = TEX_FILTER_DEFAULT;
    int pixelx = -1;
    int pixely = -1;
    DXGI_FORMAT diffFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    DWORD fileType = WIC_CODEC_BMP;
    wchar_t szOutputFile[MAX_PATH] = {};

    // Initialize COM (needed for WIC)
    HRESULT hr = hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        wprintf(L"Failed to initialize COM (%08X)\n", static_cast<unsigned int>(hr));
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
    case CMD_INFO:
    case CMD_ANALYZE:
    case CMD_COMPARE:
    case CMD_DIFF:
    case CMD_DUMPBC:
    case CMD_DUMPDDS:
        break;

    default:
        wprintf(L"Must use one of: info, analyze, compare, diff, dumpbc, or dumpdds\n\n");
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
            case OPT_FILTER:
            case OPT_FORMAT:
            case OPT_FILETYPE:
            case OPT_OUTPUTFILE:
            case OPT_TARGET_PIXELX:
            case OPT_TARGET_PIXELY:
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
                break;

            default:
                break;
            }

            switch (dwOption)
            {
            case OPT_FORMAT:
                if (dwCommand != CMD_DIFF)
                {
                    wprintf(L"-f only valid for use with diff command\n");
                    return 1;
                }
                else
                {
                    diffFormat = static_cast<DXGI_FORMAT>(LookupByName(pValue, g_pFormats));
                    if (!diffFormat)
                    {
                        diffFormat = static_cast<DXGI_FORMAT>(LookupByName(pValue, g_pFormatAliases));
                        if (!diffFormat)
                        {
                            wprintf(L"Invalid value specified with -f (%ls)\n", pValue);
                            return 1;
                        }
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

            case OPT_OUTPUTFILE:
                if (dwCommand != CMD_DIFF)
                {
                    wprintf(L"-o only valid for use with diff command\n");
                    return 1;
                }
                else
                {
                    wcscpy_s(szOutputFile, MAX_PATH, pValue);

                    wchar_t ext[_MAX_EXT];
                    _wsplitpath_s(szOutputFile, nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT);

                    fileType = LookupByName(ext, g_pExtFileTypes);
                }
                break;

            case OPT_FILETYPE:
                if (dwCommand != CMD_DUMPDDS)
                {
                    wprintf(L"-ft only valid for use with dumpdds command\n");
                    return 1;
                }
                else
                {
                    fileType = LookupByName(pValue, g_pDumpFileTypes);
                    if (!fileType)
                    {
                        wprintf(L"Invalid value specified with -ft (%ls)\n", pValue);
                        wprintf(L"\n");
                        PrintUsage();
                        return 1;
                    }
                }
                break;

            case OPT_TARGET_PIXELX:
                if (dwCommand != CMD_DUMPBC)
                {
                    wprintf(L"-targetx only valid with dumpbc command\n");
                    return 1;
                }
                else if (swscanf_s(pValue, L"%d", &pixelx) != 1)
                {
                    wprintf(L"Invalid value for pixel x location (%ls)\n", pValue);
                    return 1;
                }
                break;

            case OPT_TARGET_PIXELY:
                if (dwCommand != CMD_DUMPBC)
                {
                    wprintf(L"-targety only valid with dumpbc command\n");
                    return 1;
                }
                else if (swscanf_s(pValue, L"%d", &pixely) != 1)
                {
                    wprintf(L"Invalid value for pixel y location (%ls)\n", pValue);
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

            default:
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
    case CMD_COMPARE:
    case CMD_DIFF:
        // --- Compare/Diff ------------------------------------------------------------
        if (conversion.size() != 2)
        {
            wprintf(L"ERROR: compare/diff needs exactly two images\n");
            return 1;
        }
        else
        {
            auto pImage1 = conversion.cbegin();

            wprintf(L"1: %ls", pImage1->szSrc);
            fflush(stdout);

            TexMetadata info1;
            std::unique_ptr<ScratchImage> image1;
            hr = LoadImage(pImage1->szSrc, dwOptions, dwFilter, info1, image1);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            auto pImage2 = conversion.cbegin();
            std::advance(pImage2, 1);

            wprintf(L"\n2: %ls", pImage2->szSrc);
            fflush(stdout);

            TexMetadata info2;
            std::unique_ptr<ScratchImage> image2;
            hr = LoadImage(pImage2->szSrc, dwOptions, dwFilter, info2, image2);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            wprintf(L"\n");
            fflush(stdout);

            if (info1.height != info2.height
                || info1.width != info2.width)
            {
                wprintf(L"ERROR: Can only compare/diff images of the same width & height\n");
                return 1;
            }

            if (dwCommand == CMD_DIFF)
            {
                if (!*szOutputFile)
                {
                    wchar_t ext[_MAX_EXT];
                    wchar_t fname[_MAX_FNAME];
                    _wsplitpath_s(pImage1->szSrc, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT);
                    if (_wcsicmp(ext, L".bmp") == 0)
                    {
                        wprintf(L"ERROR: Need to specify output file via -o\n");
                        return 1;
                    }

                    _wmakepath_s(szOutputFile, nullptr, nullptr, fname, L".bmp");
                }

                if (image1->GetImageCount() > 1 || image2->GetImageCount() > 1)
                    wprintf(L"WARNING: ignoring all images but first one in each file\n");

                ScratchImage diffImage;
                hr = Difference(*image1->GetImage(0, 0, 0), *image2->GetImage(0, 0, 0), dwFilter, diffFormat, diffImage);
                if (FAILED(hr))
                {
                    wprintf(L"Failed diffing images (%08X)\n", static_cast<unsigned int>(hr));
                    return 1;
                }

                if (dwOptions & (1 << OPT_TOLOWER))
                {
                    (void)_wcslwr_s(szOutputFile);
                }

                if (~dwOptions & (1 << OPT_OVERWRITE))
                {
                    if (GetFileAttributesW(szOutputFile) != INVALID_FILE_ATTRIBUTES)
                    {
                        wprintf(L"\nERROR: Output file already exists, use -y to overwrite\n");
                        return 1;
                    }
                }

                hr = SaveImage(diffImage.GetImage(0, 0, 0), szOutputFile, fileType);
                if (FAILED(hr))
                {
                    wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                    return 1;
                }

                wprintf(L"Difference %ls\n", szOutputFile);
            }
            else if ((info1.depth == 1
                && info1.arraySize == 1
                && info1.mipLevels == 1)
                || info1.depth != info2.depth
                || info1.arraySize != info2.arraySize
                || info1.mipLevels != info2.mipLevels
                || image1->GetImageCount() != image2->GetImageCount())
            {
                // Compare single image
                if (image1->GetImageCount() > 1 || image2->GetImageCount() > 1)
                    wprintf(L"WARNING: ignoring all images but first one in each file\n");

                float mse, mseV[4];
                hr = ComputeMSE(*image1->GetImage(0, 0, 0), *image2->GetImage(0, 0, 0), mse, mseV);
                if (FAILED(hr))
                {
                    wprintf(L"Failed comparing images (%08X)\n", static_cast<unsigned int>(hr));
                    return 1;
                }

                wprintf(L"Result: %f (%f %f %f %f) PSNR %f dB\n", mse, mseV[0], mseV[1], mseV[2], mseV[3],
                    10.0 * log10(3.0 / (double(mseV[0]) + double(mseV[1]) + double(mseV[2]))));
            }
            else
            {
                // Compare all images
                float min_mse = FLT_MAX;
                float min_mseV[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };

                float max_mse = -FLT_MAX;
                float max_mseV[4] = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };

                double sum_mse = 0;
                double sum_mseV[4] = { 0, 0, 0, 0 };

                size_t total_images = 0;

                if (info1.depth > 1)
                {
                    wprintf(L"Results by mip (%3zu) and slice (%3zu)\n\n", info1.mipLevels, info1.depth);

                    size_t depth = info1.depth;
                    for (size_t mip = 0; mip < info1.mipLevels; ++mip)
                    {
                        for (size_t slice = 0; slice < depth; ++slice)
                        {
                            const Image* img1 = image1->GetImage(mip, 0, slice);
                            const Image* img2 = image2->GetImage(mip, 0, slice);

                            if (!img1
                                || !img2
                                || img1->height != img2->height
                                || img1->width != img2->width)
                            {
                                wprintf(L"ERROR: Unexpected mismatch at slice %3zu, mip %3zu\n", slice, mip);
                                return 1;
                            }
                            else
                            {
                                float mse, mseV[4];
                                hr = ComputeMSE(*img1, *img2, mse, mseV);
                                if (FAILED(hr))
                                {
                                    wprintf(L"Failed comparing images at slice %3zu, mip %3zu (%08X)\n", slice, mip, static_cast<unsigned int>(hr));
                                    return 1;
                                }

                                min_mse = std::min(min_mse, mse);
                                max_mse = std::max(max_mse, mse);
                                sum_mse += double(mse);

                                for (size_t j = 0; j < 4; ++j)
                                {
                                    min_mseV[j] = std::min(min_mseV[j], mseV[j]);
                                    max_mseV[j] = std::max(max_mseV[j], mseV[j]);
                                    sum_mseV[j] += double(mseV[j]);
                                }

                                ++total_images;

                                wprintf(L"[%3zu,%3zu]: %f (%f %f %f %f) PSNR %f dB\n", mip, slice, mse, mseV[0], mseV[1], mseV[2], mseV[3],
                                    10.0 * log10(3.0 / (double(mseV[0]) + double(mseV[1]) + double(mseV[2]))));
                            }
                        }

                        if (depth > 1)
                            depth >>= 1;
                    }
                }
                else
                {
                    wprintf(L"Results by item (%3zu) and mip (%3zu)\n\n", info1.arraySize, info1.mipLevels);

                    for (size_t item = 0; item < info1.arraySize; ++item)
                    {
                        for (size_t mip = 0; mip < info1.mipLevels; ++mip)
                        {
                            const Image* img1 = image1->GetImage(mip, item, 0);
                            const Image* img2 = image2->GetImage(mip, item, 0);

                            if (!img1
                                || !img2
                                || img1->height != img2->height
                                || img1->width != img2->width)
                            {
                                wprintf(L"ERROR: Unexpected mismatch at item %3zu, mip %3zu\n", item, mip);
                                return 1;
                            }
                            else
                            {
                                float mse, mseV[4];
                                hr = ComputeMSE(*img1, *img2, mse, mseV);
                                if (FAILED(hr))
                                {
                                    wprintf(L"Failed comparing images at item %3zu, mip %3zu (%08X)\n", item, mip, static_cast<unsigned int>(hr));
                                    return 1;
                                }

                                min_mse = std::min(min_mse, mse);
                                max_mse = std::max(max_mse, mse);
                                sum_mse += double(mse);

                                for (size_t j = 0; j < 4; ++j)
                                {
                                    min_mseV[j] = std::min(min_mseV[j], mseV[j]);
                                    max_mseV[j] = std::max(max_mseV[j], mseV[j]);
                                    sum_mseV[j] += double(mseV[j]);
                                }

                                ++total_images;

                                wprintf(L"[%3zu,%3zu]: %f (%f %f %f %f) PSNR %f dB\n", item, mip, mse, mseV[0], mseV[1], mseV[2], mseV[3],
                                    10.0 * log10(3.0 / (double(mseV[0]) + double(mseV[1]) + double(mseV[2]))));
                            }
                        }
                    }
                }

                // Output multi-image stats
                if (total_images > 1)
                {
                    wprintf(L"\n    Minimum MSE: %f (%f %f %f %f) PSNR %f dB\n", min_mse, min_mseV[0], min_mseV[1], min_mseV[2], min_mseV[3],
                        10.0 * log10(3.0 / (double(min_mseV[0]) + double(min_mseV[1]) + double(min_mseV[2]))));
                    double total_mseV0 = sum_mseV[0] / double(total_images);
                    double total_mseV1 = sum_mseV[1] / double(total_images);
                    double total_mseV2 = sum_mseV[2] / double(total_images);
                    wprintf(L"    Average MSE: %f (%f %f %f %f) PSNR %f dB\n", sum_mse / double(total_images),
                        total_mseV0,
                        total_mseV1,
                        total_mseV2,
                        sum_mseV[3] / double(total_images),
                        10.0 * log10(3.0 / (total_mseV0 + total_mseV1 + total_mseV2)));
                    wprintf(L"    Maximum MSE: %f (%f %f %f %f) PSNR %f dB\n", max_mse, max_mseV[0], max_mseV[1], max_mseV[2], max_mseV[3],
                        10.0 * log10(3.0 / (double(max_mseV[0]) + double(max_mseV[1]) + double(max_mseV[2]))));
                }
            }
        }
        break;

    default:
        for (auto pConv = conversion.cbegin(); pConv != conversion.cend(); ++pConv)
        {
            // Load source image
            if (pConv != conversion.begin())
                wprintf(L"\n");

            wprintf(L"%ls", pConv->szSrc);
            fflush(stdout);

            TexMetadata info;
            std::unique_ptr<ScratchImage> image;
            hr = LoadImage(pConv->szSrc, dwOptions, dwFilter, info, image);
            if (FAILED(hr))
            {
                wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                return 1;
            }

            wprintf(L"\n");
            fflush(stdout);

            if (dwCommand == CMD_INFO)
            {
                // --- Info ----------------------------------------------------------------
                wprintf(L"        width = %zu\n", info.width);
                wprintf(L"       height = %zu\n", info.height);
                wprintf(L"        depth = %zu\n", info.depth);
                wprintf(L"    mipLevels = %zu\n", info.mipLevels);
                wprintf(L"    arraySize = %zu\n", info.arraySize);
                wprintf(L"       format = ");
                PrintFormat(info.format);
                wprintf(L"\n    dimension = ");
                switch (info.dimension)
                {
                case TEX_DIMENSION_TEXTURE1D:
                    wprintf(L"%ls", (info.arraySize > 1) ? L"1DArray\n" : L"1D\n");
                    break;

                case TEX_DIMENSION_TEXTURE2D:
                    if (info.IsCubemap())
                    {
                        wprintf(L"%ls", (info.arraySize > 6) ? L"CubeArray\n" : L"Cube\n");
                    }
                    else
                    {
                        wprintf(L"%ls", (info.arraySize > 1) ? L"2DArray\n" : L"2D\n");
                    }
                    break;

                case TEX_DIMENSION_TEXTURE3D:
                    wprintf(L" 3D");
                    break;
                }

                wprintf(L"   alpha mode = ");
                switch (info.GetAlphaMode())
                {
                case TEX_ALPHA_MODE_OPAQUE:
                    wprintf(L"Opaque");
                    break;
                case TEX_ALPHA_MODE_PREMULTIPLIED:
                    wprintf(L"Premultiplied");
                    break;
                case TEX_ALPHA_MODE_STRAIGHT:
                    wprintf(L"Straight");
                    break;
                case TEX_ALPHA_MODE_CUSTOM:
                    wprintf(L"Custom");
                    break;
                case TEX_ALPHA_MODE_UNKNOWN:
                    wprintf(L"Unknown");
                    break;
                }

                wprintf(L"\n       images = %zu\n", image->GetImageCount());

                auto sizeInKb = static_cast<uint32_t>(image->GetPixelsSize() / 1024);

                wprintf(L"   pixel size = %u (KB)\n\n", sizeInKb);
            }
            else if (dwCommand == CMD_DUMPDDS)
            {
                // --- Dump DDS ------------------------------------------------------------
                if (IsCompressed(info.format))
                {
                    wprintf(L"ERROR: dumpdds only operates on non-compressed format DDS files\n");
                    return 1;
                }

                wchar_t ext[_MAX_EXT];
                wchar_t fname[_MAX_FNAME];
                _wsplitpath_s(pConv->szSrc, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, nullptr, 0);

                wcscpy_s(ext, LookupByValue(fileType, g_pDumpFileTypes));

                if (info.depth > 1)
                {
                    wprintf(L"Writing by mip (%3zu) and slice (%3zu)...", info.mipLevels, info.depth);

                    size_t depth = info.depth;
                    for (size_t mip = 0; mip < info.mipLevels; ++mip)
                    {
                        for (size_t slice = 0; slice < depth; ++slice)
                        {
                            const Image* img = image->GetImage(mip, 0, slice);

                            if (!img)
                            {
                                wprintf(L"ERROR: Unexpected error at slice %3zu, mip %3zu\n", slice, mip);
                                return 1;
                            }
                            else
                            {
                                wchar_t subFname[_MAX_FNAME];
                                if (info.mipLevels > 1)
                                {
                                    swprintf_s(subFname, L"%ls_slice%03zu_mip%03zu", fname, slice, mip);
                                }
                                else
                                {
                                    swprintf_s(subFname, L"%ls_slice%03zu", fname, slice);
                                }

                                _wmakepath_s(szOutputFile, nullptr, nullptr, subFname, ext);

                                hr = SaveImage(img, szOutputFile, fileType);
                                if (FAILED(hr))
                                {
                                    wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                                    return 1;
                                }
                            }
                        }

                        if (depth > 1)
                            depth >>= 1;
                    }
                    wprintf(L"\n");
                }
                else
                {
                    wprintf(L"Writing by item (%3zu) and mip (%3zu)...", info.arraySize, info.mipLevels);

                    for (size_t item = 0; item < info.arraySize; ++item)
                    {
                        for (size_t mip = 0; mip < info.mipLevels; ++mip)
                        {
                            const Image* img = image->GetImage(mip, item, 0);

                            if (!img)
                            {
                                wprintf(L"ERROR: Unexpected error at item %3zu, mip %3zu\n", item, mip);
                                return 1;
                            }
                            else
                            {
                                wchar_t subFname[_MAX_FNAME];
                                if (info.mipLevels > 1)
                                {
                                    swprintf_s(subFname, L"%ls_item%03zu_mip%03zu", fname, item, mip);
                                }
                                else
                                {
                                    swprintf_s(subFname, L"%ls_item%03zu", fname, item);
                                }

                                _wmakepath_s(szOutputFile, nullptr, nullptr, subFname, ext);

                                hr = SaveImage(img, szOutputFile, fileType);
                                if (FAILED(hr))
                                {
                                    wprintf(L" FAILED (%x)\n", static_cast<unsigned int>(hr));
                                    return 1;
                                }
                            }
                        }
                    }

                    wprintf(L"\n");
                }
            }
            else if (dwCommand == CMD_DUMPBC)
            {
                // --- Dump BC -------------------------------------------------------------
                if (!IsCompressed(info.format))
                {
                    wprintf(L"ERROR: dumpbc only operates on BC format DDS files\n");
                    return 1;
                }

                if (pixelx >= int(info.width)
                    || pixely >= int(info.height))
                {
                    wprintf(L"WARNING: Specified pixel location (%d x %d) is out of range for image (%zu x %zu)\n", pixelx, pixely, info.width, info.height);
                    continue;
                }

                wprintf(L"Compression: ");
                PrintFormat(info.format);
                wprintf(L"\n");

                if (info.depth > 1)
                {
                    wprintf(L"Results by mip (%3zu) and slice (%3zu)\n", info.mipLevels, info.depth);

                    size_t depth = info.depth;
                    for (size_t mip = 0; mip < info.mipLevels; ++mip)
                    {
                        for (size_t slice = 0; slice < depth; ++slice)
                        {
                            const Image* img = image->GetImage(mip, 0, slice);

                            if (!img)
                            {
                                wprintf(L"ERROR: Unexpected error at slice %3zu, mip %3zu\n", slice, mip);
                                return 1;
                            }
                            else
                            {
                                wprintf(L"\n[%3zu, %3zu]:\n", mip, slice);

                                hr = DumpBCImage(*img, pixelx, pixely);
                                if (FAILED(hr))
                                {
                                    wprintf(L"ERROR: Failed dumping image at slice %3zu, mip %3zu (%08X)\n", slice, mip, static_cast<unsigned int>(hr));
                                    return 1;
                                }
                            }
                        }

                        if (depth > 1)
                            depth >>= 1;

                        if (pixelx > 0)
                            pixelx >>= 1;

                        if (pixely > 0)
                            pixely >>= 1;
                    }
                }
                else
                {
                    wprintf(L"Results by item (%3zu) and mip (%3zu)\n", info.arraySize, info.mipLevels);

                    for (size_t item = 0; item < info.arraySize; ++item)
                    {
                        int tpixelx = pixelx;
                        int tpixely = pixely;

                        for (size_t mip = 0; mip < info.mipLevels; ++mip)
                        {
                            const Image* img = image->GetImage(mip, item, 0);

                            if (!img)
                            {
                                wprintf(L"ERROR: Unexpected error at item %3zu, mip %3zu\n", item, mip);
                                return 1;
                            }
                            else
                            {
                                if (image->GetImageCount() > 1)
                                {
                                    wprintf(L"\n[%3zu, %3zu]:\n", item, mip);
                                }
                                hr = DumpBCImage(*img, tpixelx, tpixely);
                                if (FAILED(hr))
                                {
                                    wprintf(L"ERROR: Failed dumping image at item %3zu, mip %3zu (%08X)\n", item, mip, static_cast<unsigned int>(hr));
                                    return 1;
                                }
                            }

                            if (tpixelx > 0)
                                tpixelx >>= 1;

                            if (tpixely > 0)
                                tpixely >>= 1;
                        }
                    }
                }
            }
            else
            {
                // --- Analyze -------------------------------------------------------------
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

                if (info.depth > 1)
                {
                    wprintf(L"Results by mip (%3zu) and slice (%3zu)\n\n", info.mipLevels, info.depth);

                    size_t depth = info.depth;
                    for (size_t mip = 0; mip < info.mipLevels; ++mip)
                    {
                        for (size_t slice = 0; slice < depth; ++slice)
                        {
                            const Image* img = image->GetImage(mip, 0, slice);

                            if (!img)
                            {
                                wprintf(L"ERROR: Unexpected error at slice %3zu, mip %3zu\n", slice, mip);
                                return 1;
                            }
                            else
                            {
                                AnalyzeData data;
                                hr = Analyze(*img, data);
                                if (FAILED(hr))
                                {
                                    wprintf(L"ERROR: Failed analyzing image at slice %3zu, mip %3zu (%08X)\n", slice, mip, static_cast<unsigned int>(hr));
                                    return 1;
                                }

                                wprintf(L"Result slice %3zu, mip %3zu:\n", slice, mip);
                                data.Print();
                            }

                            if (IsCompressed(info.format))
                            {
                                AnalyzeBCData data;
                                hr = AnalyzeBC(*img, data);
                                if (FAILED(hr))
                                {
                                    wprintf(L"ERROR: Failed analyzing BC image at slice %3zu, mip %3zu (%08X)\n", slice, mip, static_cast<unsigned int>(hr));
                                    return 1;
                                }

                                data.Print(img->format);
                            }
                            wprintf(L"\n");
                        }

                        if (depth > 1)
                            depth >>= 1;
                    }
                }
                else
                {
                    wprintf(L"Results by item (%3zu) and mip (%3zu)\n\n", info.arraySize, info.mipLevels);

                    for (size_t item = 0; item < info.arraySize; ++item)
                    {
                        for (size_t mip = 0; mip < info.mipLevels; ++mip)
                        {
                            const Image* img = image->GetImage(mip, item, 0);

                            if (!img)
                            {
                                wprintf(L"ERROR: Unexpected error at item %3zu, mip %3zu\n", item, mip);
                                return 1;
                            }
                            else
                            {
                                AnalyzeData data;
                                hr = Analyze(*img, data);
                                if (FAILED(hr))
                                {
                                    wprintf(L"ERROR: Failed analyzing image at item %3zu, mip %3zu (%08X)\n", item, mip, static_cast<unsigned int>(hr));
                                    return 1;
                                }

                                if (image->GetImageCount() > 1)
                                {
                                    wprintf(L"Result item %3zu, mip %3zu:\n", item, mip);
                                }
                                data.Print();
                            }

                            if (IsCompressed(info.format))
                            {
                                AnalyzeBCData data;
                                hr = AnalyzeBC(*img, data);
                                if (FAILED(hr))
                                {
                                    wprintf(L"ERROR: Failed analyzing BC image at item %3zu, mip %3zu (%08X)\n", item, mip, static_cast<unsigned int>(hr));
                                    return 1;
                                }

                                data.Print(img->format);
                            }
                            wprintf(L"\n");
                        }
                    }
                }
            }
        }
        break;
    }

    return 0;
}
