//--------------------------------------------------------------------------------------
// File: TexConv.cpp
//
// DirectX 11 Texture Converter
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <memory>
#include <list>

#include <wrl\client.h>

#include <dxgiformat.h>

#include "directxtex.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

enum OPTIONS    // Note: dwOptions below assumes 32 or less options.
{
    OPT_WIDTH = 1,
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
    OPT_FILETYPE,
    OPT_HFLIP,
    OPT_VFLIP,
    OPT_DDS_DWORD_ALIGN,
    OPT_USE_DX10,
    OPT_NOLOGO,
    OPT_SEPALPHA,
    OPT_TYPELESS_UNORM,
    OPT_TYPELESS_FLOAT,
    OPT_PREMUL_ALPHA,
    OPT_EXPAND_LUMINANCE,
    OPT_TA_WRAP,
    OPT_TA_MIRROR,
    OPT_FORCE_SINGLEPROC,
    OPT_NOGPU,
    OPT_FEATURE_LEVEL,
    OPT_FIT_POWEROF2,
    OPT_ALPHA_WEIGHT,
    OPT_NORMAL_MAP,
    OPT_NORMAL_MAP_AMPLITUDE,
    OPT_MAX
};

static_assert( OPT_MAX <= 32, "dwOptions is a DWORD bitfield" );

struct SConversion
{
    WCHAR szSrc [MAX_PATH];
    WCHAR szDest[MAX_PATH];
};

struct SValue
{
    LPCWSTR pName;
    DWORD dwValue;
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

SValue g_pOptions[] = 
{
    { L"w",             OPT_WIDTH     },
    { L"h",             OPT_HEIGHT    },
    { L"m",             OPT_MIPLEVELS },
    { L"f",             OPT_FORMAT    },
    { L"if",            OPT_FILTER    },
    { L"srgbi",         OPT_SRGBI     },
    { L"srgbo",         OPT_SRGBO     },
    { L"srgb",          OPT_SRGB      },
    { L"px",            OPT_PREFIX    },
    { L"sx",            OPT_SUFFIX    },
    { L"o",             OPT_OUTPUTDIR },
    { L"ft",            OPT_FILETYPE  },
    { L"hflip",         OPT_HFLIP     },
    { L"vflip",         OPT_VFLIP     },
    { L"dword",         OPT_DDS_DWORD_ALIGN },
    { L"dx10",          OPT_USE_DX10  },
    { L"nologo",        OPT_NOLOGO    },
    { L"sepalpha",      OPT_SEPALPHA  },
    { L"tu",            OPT_TYPELESS_UNORM },
    { L"tf",            OPT_TYPELESS_FLOAT },
    { L"pmalpha",       OPT_PREMUL_ALPHA },
    { L"xlum",          OPT_EXPAND_LUMINANCE },
    { L"wrap",          OPT_TA_WRAP },
    { L"mirror",        OPT_TA_MIRROR },
    { L"singleproc",    OPT_FORCE_SINGLEPROC },
    { L"nogpu",         OPT_NOGPU     },
    { L"fl",            OPT_FEATURE_LEVEL },
    { L"pow2",          OPT_FIT_POWEROF2 },
    { L"aw",            OPT_ALPHA_WEIGHT },
    { L"nmap",          OPT_NORMAL_MAP },
    { L"nmapamp",       OPT_NORMAL_MAP_AMPLITUDE },
    { nullptr,          0             }
};

#define DEFFMT(fmt) { L#fmt, DXGI_FORMAT_ ## fmt }

SValue g_pFormats[] = 
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

SValue g_pReadOnlyFormats[] = 
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

    { nullptr, DXGI_FORMAT_UNKNOWN }
};

SValue g_pFilters[] = 
{
    { L"POINT",                     TEX_FILTER_POINT },
    { L"LINEAR",                    TEX_FILTER_LINEAR },
    { L"CUBIC",                     TEX_FILTER_CUBIC },
    { L"FANT",                      TEX_FILTER_FANT },
    { L"BOX",                       TEX_FILTER_BOX },
    { L"TRIANGLE",                  TEX_FILTER_TRIANGLE },
    { L"POINT_DITHER",              TEX_FILTER_POINT  | TEX_FILTER_DITHER },
    { L"LINEAR_DITHER",             TEX_FILTER_LINEAR | TEX_FILTER_DITHER },
    { L"CUBIC_DITHER",              TEX_FILTER_CUBIC  | TEX_FILTER_DITHER },
    { L"FANT_DITHER",               TEX_FILTER_FANT   | TEX_FILTER_DITHER },
    { L"BOX_DITHER",                TEX_FILTER_BOX    | TEX_FILTER_DITHER },
    { L"TRIANGLE_DITHER",           TEX_FILTER_TRIANGLE | TEX_FILTER_DITHER },
    { L"POINT_DITHER_DIFFUSION",    TEX_FILTER_POINT  | TEX_FILTER_DITHER_DIFFUSION },
    { L"LINEAR_DITHER_DIFFUSION",   TEX_FILTER_LINEAR | TEX_FILTER_DITHER_DIFFUSION },
    { L"CUBIC_DITHER_DIFFUSION",    TEX_FILTER_CUBIC  | TEX_FILTER_DITHER_DIFFUSION },
    { L"FANT_DITHER_DIFFUSION",     TEX_FILTER_FANT   | TEX_FILTER_DITHER_DIFFUSION },
    { L"BOX_DITHER_DIFFUSION",      TEX_FILTER_BOX    | TEX_FILTER_DITHER_DIFFUSION },
    { L"TRIANGLE_DITHER_DIFFUSION", TEX_FILTER_TRIANGLE | TEX_FILTER_DITHER_DIFFUSION },
    { nullptr,                      TEX_FILTER_DEFAULT                              }
};

#define CODEC_DDS 0xFFFF0001 
#define CODEC_TGA 0xFFFF0002

SValue g_pSaveFileTypes[] =     // valid formats to write to
{
    { L"BMP",           WIC_CODEC_BMP  },
    { L"JPG",           WIC_CODEC_JPEG },
    { L"JPEG",          WIC_CODEC_JPEG },
    { L"PNG",           WIC_CODEC_PNG  },
    { L"DDS",           CODEC_DDS      },
    { L"TGA",           CODEC_TGA      },
    { L"TIF",           WIC_CODEC_TIFF },
    { L"TIFF",          WIC_CODEC_TIFF },
    { L"WDP",           WIC_CODEC_WMP  },
    { L"HDP",           WIC_CODEC_WMP  },
    { nullptr,          CODEC_DDS      }
};

SValue g_pFeatureLevels[] =     // valid feature levels for -fl for maximimum size
{
    { L"9.1",           2048 },
    { L"9.2",           2048 },
    { L"9.3",           4096 },
    { L"10.0",          8192 },
    { L"10.1",          8192 },
    { L"11.0",          16384 },
    { L"11.1",          16384 },
    { nullptr,          0 },
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#pragma warning( disable : 4616 6211 )

inline static bool ispow2(size_t x)
{
    return ((x != 0) && !(x & (x - 1)));
}

#pragma prefast(disable : 26018, "Only used with static internal arrays")

DWORD LookupByName(const WCHAR *pName, const SValue *pArray)
{
    while(pArray->pName)
    {
        if(!_wcsicmp(pName, pArray->pName))
            return pArray->dwValue;

        pArray++;
    }

    return 0;
}

const WCHAR* LookupByValue(DWORD pValue, const SValue *pArray)
{
    while(pArray->pName)
    {
        if(pValue == pArray->dwValue)
            return pArray->pName;

        pArray++;
    }

    return L"";
}

void PrintFormat(DXGI_FORMAT Format)
{
    for(SValue *pFormat = g_pFormats; pFormat->pName; pFormat++)
    {
        if((DXGI_FORMAT) pFormat->dwValue == Format)
        {
            wprintf( pFormat->pName );
            return;
        }
    }

    for(SValue *pFormat = g_pReadOnlyFormats; pFormat->pName; pFormat++)
    {
        if((DXGI_FORMAT) pFormat->dwValue == Format)
        {
            wprintf( pFormat->pName );
            return;
        }
    }

    wprintf( L"*UNKNOWN*" );
}

void PrintInfo( const TexMetadata& info )
{
    wprintf( L" (%Iux%Iu", info.width, info.height);

    if ( TEX_DIMENSION_TEXTURE3D == info.dimension )
        wprintf( L"x%Iu", info.depth);

    if ( info.mipLevels > 1 )
        wprintf( L",%Iu", info.mipLevels);

    if ( info.arraySize > 1 )
        wprintf( L",%Iu", info.arraySize);

    wprintf( L" ");
    PrintFormat( info.format );

    switch ( info.dimension )
    {
    case TEX_DIMENSION_TEXTURE1D:
        wprintf( (info.arraySize > 1) ? L" 1DArray" : L" 1D" );
        break;

    case TEX_DIMENSION_TEXTURE2D:
        if ( info.IsCubemap() )
        {
            wprintf( (info.arraySize > 6) ? L" CubeArray" : L" Cube" );
        }
        else
        {
            wprintf( (info.arraySize > 1) ? L" 2DArray" : L" 2D" );
        }
        break;

    case TEX_DIMENSION_TEXTURE3D:
        wprintf( L" 3D");
        break;
    }

    wprintf( L")");
}


void PrintList(size_t cch, SValue *pValue)
{
    while(pValue->pName)
    {
        size_t cchName = wcslen(pValue->pName);
        
        if(cch + cchName + 2>= 80)
        {
            wprintf( L"\n      ");
            cch = 6;
        }

        wprintf( L"%ls ", pValue->pName );
        cch += cchName + 2;
        pValue++;
    }

    wprintf( L"\n");
}


void PrintLogo()
{
    wprintf( L"Microsoft (R) DirectX 11 Texture Converter (DirectXTex version)\n");
    wprintf( L"Copyright (C) Microsoft Corp. All rights reserved.\n");
    wprintf( L"\n");
}


void PrintUsage()
{
    PrintLogo();

    wprintf( L"Usage: texconv <options> <files>\n");
    wprintf( L"\n");
    wprintf( L"   -w <n>              width\n");
    wprintf( L"   -h <n>              height\n");
    wprintf( L"   -m <n>              miplevels\n");
    wprintf( L"   -f <format>         format\n");
    wprintf( L"   -if <filter>        image filtering\n");
    wprintf( L"   -srgb{i|o}          sRGB {input, output}\n");
    wprintf( L"   -px <string>        name prefix\n");
    wprintf( L"   -sx <string>        name suffix\n");
    wprintf( L"   -o <directory>      output directory\n");
    wprintf( L"   -ft <filetype>      output file type\n");
    wprintf( L"   -hflip              horizonal flip of source image\n");
    wprintf( L"   -vflip              vertical flip of source image\n");
    wprintf( L"   -sepalpha           resize/generate mips alpha channel separately\n");
    wprintf( L"                       from color channels\n");
    wprintf( L"   -wrap, -mirror      texture addressing mode (wrap, mirror, or clamp)\n");
    wprintf( L"   -pmalpha            convert final texture to use premultiplied alpha\n");
    wprintf( L"   -pow2               resize to fit a power-of-2, respecting aspect ratio\n" );
    wprintf( L"   -aw <weight>        BC7 GPU compressor weighting for alpha error metric\n"
             L"                       (defaults to 1.0)\n" );
    wprintf (L"   -nmap <options>     converts height-map to normal-map\n"
             L"                       options must be one or more of\n"
             L"                          r, g, b, a, l, m, u, v, i, o\n" );
    wprintf (L"   -nmapamp <weight>   normal map amplitude (defaults to 1.0)\n" );
    wprintf( L"   -fl <feature-level> Set maximum feature level target (defaults to 11.0)\n");
    wprintf( L"\n                       (DDS input only)\n");
    wprintf( L"   -t{u|f}             TYPELESS format is treated as UNORM or FLOAT\n");
    wprintf( L"   -dword              Use DWORD instead of BYTE alignment\n");
    wprintf( L"   -xlum               expand legacy L8, L16, and A8P8 formats\n");
    wprintf( L"\n                       (DDS output only)\n");
    wprintf( L"   -dx10               Force use of 'DX10' extended header\n");
    wprintf( L"\n   -nologo             suppress copyright message\n");
#ifdef _OPENMP
    wprintf( L"   -singleproc         Do not use multi-threaded compression\n");
#endif
    wprintf( L"   -nogpu              Do not use DirectCompute-based codecs\n");

    wprintf( L"\n");
    wprintf( L"   <format>: ");
    PrintList(13, g_pFormats);

    wprintf( L"\n");
    wprintf( L"   <filter>: ");
    PrintList(13, g_pFilters);

    wprintf( L"\n");
    wprintf( L"   <filetype>: ");
    PrintList(15, g_pSaveFileTypes);

    wprintf( L"\n");
    wprintf( L"   <feature-level>: ");
    PrintList(13, g_pFeatureLevels);

}

_Success_(return != false)
bool CreateDevice( _Outptr_ ID3D11Device** pDevice )
{
    if ( !pDevice )
        return false;

    *pDevice  = nullptr;

    static PFN_D3D11_CREATE_DEVICE s_DynamicD3D11CreateDevice = nullptr;
   
    if ( !s_DynamicD3D11CreateDevice )
    {            
        HMODULE hModD3D11 = LoadLibrary( L"d3d11.dll" );
        if ( !hModD3D11 )
            return false;

        s_DynamicD3D11CreateDevice = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>( reinterpret_cast<void*>( GetProcAddress( hModD3D11, "D3D11CreateDevice" ) ) ); 
        if ( !s_DynamicD3D11CreateDevice )
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

    D3D_FEATURE_LEVEL fl;
    HRESULT hr = s_DynamicD3D11CreateDevice( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevels, _countof(featureLevels), 
                                             D3D11_SDK_VERSION, pDevice, &fl, nullptr );
    if ( SUCCEEDED(hr) )
    {
        if ( fl < D3D_FEATURE_LEVEL_11_0 )
        {
            D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts;
            hr = (*pDevice)->CheckFeatureSupport( D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts) );
            if ( FAILED(hr) )
                memset( &hwopts, 0, sizeof(hwopts) );

            if ( !hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x )
            {
                if ( *pDevice )
                {
                    (*pDevice)->Release();
                    *pDevice = nullptr;
                }
                hr = HRESULT_FROM_WIN32( ERROR_NOT_SUPPORTED );
            }
        }
    }

    if ( SUCCEEDED(hr) )
    {
        ComPtr<IDXGIDevice> dxgiDevice;
        hr = (*pDevice)->QueryInterface( __uuidof( IDXGIDevice ), reinterpret_cast< void** >( dxgiDevice.GetAddressOf() ) );
        if ( SUCCEEDED(hr) )
        {
            ComPtr<IDXGIAdapter> pAdapter;
            hr = dxgiDevice->GetAdapter( pAdapter.GetAddressOf() );
            if ( SUCCEEDED(hr) )
            {
                DXGI_ADAPTER_DESC desc;
                hr = pAdapter->GetDesc( &desc );
                if ( SUCCEEDED(hr) )
                {
                    wprintf( L"\n[Using DirectCompute on \"%ls\"]\n", desc.Description );
                }
            }
        }

        return true;
    }
    else
        return false;
}

void FitPowerOf2( size_t origx, size_t origy, size_t& targetx, size_t& targety, size_t maxsize )
{
    float origAR = float(origx) / float(origy);

    if ( origx > origy )
    {
        size_t x;
        for( x = maxsize; x > 1; x >>= 1 ) { if ( x <= targetx ) break; };
        targetx = x;

        float bestScore = FLT_MAX;
        for( size_t y = maxsize; y > 0; y >>= 1 )
        {
            float score = fabs( (float(x) / float(y)) - origAR );
            if ( score < bestScore )
            {
                bestScore = score;
                targety = y;
            }
        }
    }
    else
    {
        size_t y;
        for( y = maxsize; y > 1; y >>= 1 ) { if ( y <= targety ) break; };
        targety = y;

        float bestScore = FLT_MAX;
        for( size_t x = maxsize; x > 0; x >>= 1 )
        {
            float score = fabs( (float(x) / float(y)) - origAR );
            if ( score < bestScore )
            {
                bestScore = score;
                targetx = x;
            }
        }
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
    size_t mipLevels = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    DWORD dwFilter = TEX_FILTER_DEFAULT;
    DWORD dwSRGB = 0;
    DWORD dwFilterOpts = 0;
    DWORD FileType = CODEC_DDS;
    DWORD maxSize = 16384;
    float alphaWeight = 1.f;
    DWORD dwNormalMap = 0;
    float nmapAmplitude = 1.f;
    
    WCHAR szPrefix   [MAX_PATH];
    WCHAR szSuffix   [MAX_PATH];
    WCHAR szOutputDir[MAX_PATH];

    szPrefix[0]    = 0;
    szSuffix[0]    = 0;
    szOutputDir[0] = 0;

    // Initialize COM (needed for WIC)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if( FAILED(hr) )
    {
        wprintf( L"Failed to initialize COM (%08X)\n", hr);
        return 1;
    }

    // Process command line
    DWORD dwOptions = 0;
    std::list<SConversion> conversion;

    for(int iArg = 1; iArg < argc; iArg++)
    {
        PWSTR pArg = argv[iArg];

        if(('-' == pArg[0]) || ('/' == pArg[0]))
        {
            pArg++;
            PWSTR pValue;

            for(pValue = pArg; *pValue && (':' != *pValue); pValue++);

            if(*pValue)
                *pValue++ = 0;

            DWORD dwOption = LookupByName(pArg, g_pOptions);

            if(!dwOption || (dwOptions & (1 << dwOption)))
            {
                PrintUsage();
                return 1;
            }

            dwOptions |= 1 << dwOption;

            if( (OPT_NOLOGO != dwOption) && (OPT_TYPELESS_UNORM != dwOption) && (OPT_TYPELESS_FLOAT != dwOption)
                && (OPT_SEPALPHA != dwOption) && (OPT_PREMUL_ALPHA != dwOption) && (OPT_EXPAND_LUMINANCE != dwOption)
                && (OPT_TA_WRAP != dwOption) && (OPT_TA_MIRROR != dwOption)
                && (OPT_FORCE_SINGLEPROC != dwOption) && (OPT_NOGPU != dwOption) && (OPT_FIT_POWEROF2 != dwOption)
                && (OPT_SRGB != dwOption) && (OPT_SRGBI != dwOption) && (OPT_SRGBO != dwOption)
                && (OPT_HFLIP != dwOption) && (OPT_VFLIP != dwOption)
                && (OPT_DDS_DWORD_ALIGN != dwOption) && (OPT_USE_DX10 != dwOption) )
            {
                if(!*pValue)
                {
                    if((iArg + 1 >= argc))
                    {
                        PrintUsage();
                        return 1;
                    }

                    iArg++;
                    pValue = argv[iArg];
                }
            }

            switch(dwOption)
            {
            case OPT_WIDTH:
                if (swscanf_s(pValue, L"%Iu", &width) != 1)
                {
                    wprintf( L"Invalid value specified with -w (%ls)\n", pValue);
                    wprintf( L"\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_HEIGHT:
                if (swscanf_s(pValue, L"%Iu", &height) != 1)
                {
                    wprintf( L"Invalid value specified with -h (%ls)\n", pValue);
                    printf("\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_MIPLEVELS:
                if (swscanf_s(pValue, L"%Iu", &mipLevels) != 1)
                {
                    wprintf( L"Invalid value specified with -m (%ls)\n", pValue);
                    wprintf( L"\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_FORMAT:
                format = (DXGI_FORMAT) LookupByName(pValue, g_pFormats);
                if ( !format )
                {
                    wprintf( L"Invalid value specified with -f (%ls)\n", pValue);
                    wprintf( L"\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_FILTER:
                dwFilter = LookupByName(pValue, g_pFilters);
                if ( !dwFilter )
                {
                    wprintf( L"Invalid value specified with -if (%ls)\n", pValue);
                    wprintf( L"\n");
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
                if ( !FileType )
                {
                    wprintf( L"Invalid value specified with -ft (%ls)\n", pValue);
                    wprintf( L"\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_TA_WRAP:
                if ( dwFilterOpts & TEX_FILTER_MIRROR )
                {
                    wprintf( L"Can't use -wrap and -mirror at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                dwFilterOpts |= TEX_FILTER_WRAP;
                break;

            case OPT_TA_MIRROR:
                if ( dwFilterOpts & TEX_FILTER_WRAP )
                {
                    wprintf( L"Can't use -wrap and -mirror at same time\n\n");
                    PrintUsage();
                    return 1;
                }
                dwFilterOpts |= TEX_FILTER_MIRROR;
                break;

            case OPT_NORMAL_MAP:
                {
                    dwNormalMap = 0;

                    if ( wcschr( pValue, L'l' ) )
                    {
                        dwNormalMap |= CNMAP_CHANNEL_LUMINANCE;
                    }
                    else if ( wcschr( pValue, L'r' ) )
                    {
                        dwNormalMap |= CNMAP_CHANNEL_RED;
                    }
                    else if ( wcschr( pValue, L'g' ) )
                    {
                        dwNormalMap |= CNMAP_CHANNEL_GREEN;
                    }
                    else if ( wcschr( pValue, L'b' ) )
                    {
                        dwNormalMap |= CNMAP_CHANNEL_BLUE;
                    }
                    else if ( wcschr( pValue, L'a' ) )
                    {
                        dwNormalMap |= CNMAP_CHANNEL_ALPHA;
                    }
                    else
                    {
                        wprintf( L"Invalid value specified for -nmap (%ls), missing l, r, g, b, or a\n\n", pValue );
                        PrintUsage();
                        return 1;                        
                    }

                    if ( wcschr( pValue, L'm' ) )
                    {
                        dwNormalMap |= CNMAP_MIRROR;
                    }
                    else
                    {
                        if ( wcschr( pValue, L'u' ) )
                        {
                            dwNormalMap |= CNMAP_MIRROR_U;
                        }
                        if ( wcschr( pValue, L'v' ) )
                        {
                            dwNormalMap |= CNMAP_MIRROR_V;
                        }
                    }

                    if ( wcschr( pValue, L'i' ) )
                    {
                        dwNormalMap |= CNMAP_INVERT_SIGN;
                    }

                    if ( wcschr( pValue, L'o' ) )
                    {
                        dwNormalMap |= CNMAP_COMPUTE_OCCLUSION;
                    }   
                }
                break;

            case OPT_NORMAL_MAP_AMPLITUDE:
                if ( !dwNormalMap )
                {
                    wprintf( L"-nmapamp requires -nmap\n\n" );
                    PrintUsage();
                    return 1;
                }
                else if (swscanf_s(pValue, L"%f", &nmapAmplitude) != 1)
                {
                    wprintf( L"Invalid value specified with -nmapamp (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                else if ( nmapAmplitude < 0.f )
                {
                    wprintf( L"Normal map amplitude must be positive (%ls)\n\n", pValue);
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_FEATURE_LEVEL:
                maxSize = LookupByName( pValue, g_pFeatureLevels );
                if ( !maxSize )
                {
                    wprintf( L"Invalid value specified with -fl (%ls)\n", pValue);
                    wprintf( L"\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_ALPHA_WEIGHT:
                if (swscanf_s(pValue, L"%f", &alphaWeight) != 1)
                {
                    wprintf( L"Invalid value specified with -aw (%ls)\n", pValue);
                    wprintf( L"\n");
                    PrintUsage();
                    return 1;
                }
                else if ( alphaWeight < 0.f )
                {
                    wprintf( L"-aw (%ls) parameter must be positive\n", pValue);
                    wprintf( L"\n");
                    return 1;
                }
                break;
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

    if(conversion.empty())
    {
        PrintUsage();
        return 0;
    }

    if(~dwOptions & (1 << OPT_NOLOGO))
        PrintLogo();

    // Work out out filename prefix and suffix
    if(szOutputDir[0] && (L'\\' != szOutputDir[wcslen(szOutputDir) - 1]))
        wcscat_s( szOutputDir, MAX_PATH, L"\\" );

    if(szPrefix[0])
        wcscat_s(szOutputDir, MAX_PATH, szPrefix);

    wcscpy_s(szPrefix, MAX_PATH, szOutputDir);

    const WCHAR* fileTypeName = LookupByValue(FileType, g_pSaveFileTypes);

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

    // Convert images
    bool nonpow2warn = false;
    bool non4bc = false;
    ComPtr<ID3D11Device> pDevice;

    for( auto pConv = conversion.begin(); pConv != conversion.end(); ++pConv )
    {
        if ( pConv != conversion.begin() )
            wprintf( L"\n");

        // Load source image
        wprintf( L"reading %ls", pConv->szSrc );
        fflush(stdout);

        WCHAR ext[_MAX_EXT];
        WCHAR fname[_MAX_FNAME];
        _wsplitpath_s( pConv->szSrc, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT );

        TexMetadata info;
        std::unique_ptr<ScratchImage> image( new (std::nothrow) ScratchImage );

        if ( !image )
        {
            wprintf( L" ERROR: Memory allocation failed\n" );
            return 1;
        }

        if ( _wcsicmp( ext, L".dds" ) == 0 )
        {
            DWORD ddsFlags = DDS_FLAGS_NONE;
            if ( dwOptions & (1 << OPT_DDS_DWORD_ALIGN) )
                ddsFlags |= DDS_FLAGS_LEGACY_DWORD;
            if ( dwOptions & (1 << OPT_EXPAND_LUMINANCE) )
                ddsFlags |= DDS_FLAGS_EXPAND_LUMINANCE;

            hr = LoadFromDDSFile( pConv->szSrc, ddsFlags, &info, *image );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED (%x)\n", hr);
                continue;
            }

            if ( IsTypeless( info.format ) )
            {
                if ( dwOptions & (1 << OPT_TYPELESS_UNORM) )
                {
                    info.format = MakeTypelessUNORM( info.format );
                }
                else if ( dwOptions & (1 << OPT_TYPELESS_FLOAT) )
                {
                    info.format = MakeTypelessFLOAT( info.format );
                }

                if ( IsTypeless( info.format ) )
                {
                    wprintf( L" FAILED due to Typeless format %d\n", info.format );
                    continue;
                }

                image->OverrideFormat( info.format );
            }
        }
        else if ( _wcsicmp( ext, L".tga" ) == 0 )
        {
            hr = LoadFromTGAFile( pConv->szSrc, &info, *image );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED (%x)\n", hr);
                continue;
            }
        }
        else
        {
            // WIC shares the same filter values for mode and dither
            static_assert( WIC_FLAGS_DITHER == TEX_FILTER_DITHER, "WIC_FLAGS_* & TEX_FILTER_* should match" );
            static_assert( WIC_FLAGS_DITHER_DIFFUSION == TEX_FILTER_DITHER_DIFFUSION, "WIC_FLAGS_* & TEX_FILTER_* should match"  );
            static_assert( WIC_FLAGS_FILTER_POINT == TEX_FILTER_POINT, "WIC_FLAGS_* & TEX_FILTER_* should match"  );
            static_assert( WIC_FLAGS_FILTER_LINEAR == TEX_FILTER_LINEAR, "WIC_FLAGS_* & TEX_FILTER_* should match"  );
            static_assert( WIC_FLAGS_FILTER_CUBIC == TEX_FILTER_CUBIC, "WIC_FLAGS_* & TEX_FILTER_* should match"  );
            static_assert( WIC_FLAGS_FILTER_FANT == TEX_FILTER_FANT, "WIC_FLAGS_* & TEX_FILTER_* should match"  );

            DWORD wicFlags = dwFilter;
            if (FileType == CODEC_DDS)
                wicFlags |= WIC_FLAGS_ALL_FRAMES;
 
            hr = LoadFromWICFile( pConv->szSrc, wicFlags, &info, *image );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED (%x)\n", hr);
                continue;
            }
        }

        PrintInfo( info );

        size_t tMips = ( !mipLevels && info.mipLevels > 1 ) ? info.mipLevels : mipLevels;

        bool sizewarn = false;

        size_t twidth = ( !width ) ? info.width : width;
        if ( twidth > maxSize )
        {
            if ( !width )
                twidth = maxSize;
            else
                sizewarn = true;
        }

        size_t theight = ( !height ) ? info.height : height;
        if ( theight > maxSize )
        {
            if ( !height )
                theight = maxSize;
            else
                sizewarn = true;
        }

        if ( sizewarn )
        {
            wprintf( L"\nWARNING: Target size exceeds maximum size for feature level (%u)\n", maxSize );
        }

        if (dwOptions & (1 << OPT_FIT_POWEROF2))
        {
            FitPowerOf2( info.width, info.height, twidth, theight, maxSize );
        }

        // Convert texture
        wprintf( L" as");
        fflush(stdout);

        // --- Planar ------------------------------------------------------------------
        if ( IsPlanar( info.format ) )
        {
            auto img = image->GetImage(0,0,0);
            assert( img );
            size_t nimg = image->GetImageCount();

            std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                return 1;
            }

            hr = ConvertToSinglePlane( img, nimg, info, *timage );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [converttosingeplane] (%x)\n", hr);
                continue;
            }

            auto& tinfo = timage->GetMetadata();

            info.format = tinfo.format;

            assert( info.width == tinfo.width );
            assert( info.height == tinfo.height );
            assert( info.depth == tinfo.depth );
            assert( info.arraySize == tinfo.arraySize );
            assert( info.mipLevels == tinfo.mipLevels );
            assert( info.miscFlags == tinfo.miscFlags );
            assert( info.miscFlags2 == tinfo.miscFlags2 );
            assert( info.dimension == tinfo.dimension );

            image.swap( timage );
        }

        DXGI_FORMAT tformat = ( format == DXGI_FORMAT_UNKNOWN ) ? info.format : format;

        // --- Decompress --------------------------------------------------------------
        std::unique_ptr<ScratchImage> cimage;
        if ( IsCompressed( info.format ) )
        {
            auto img = image->GetImage(0,0,0);
            assert( img );
            size_t nimg = image->GetImageCount();

            std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                return 1;
            }

            hr = Decompress( img, nimg, info, DXGI_FORMAT_UNKNOWN /* picks good default */, *timage );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [decompress] (%x)\n", hr);
                continue;
            }

            auto& tinfo = timage->GetMetadata();

            info.format = tinfo.format;

            assert( info.width == tinfo.width );
            assert( info.height == tinfo.height );
            assert( info.depth == tinfo.depth );
            assert( info.arraySize == tinfo.arraySize );
            assert( info.mipLevels == tinfo.mipLevels );
            assert( info.miscFlags == tinfo.miscFlags );
            assert( info.miscFlags2 == tinfo.miscFlags2 );
            assert( info.dimension == tinfo.dimension );

            if ( FileType == CODEC_DDS )
            {
                // Keep the original compressed image in case we can reuse it
                cimage.reset( image.release() );
                image.reset( timage.release() );
            }
            else
            {
                image.swap( timage );
            }
        }

        // --- Flip/Rotate -------------------------------------------------------------
        if ( dwOptions & ( (1 << OPT_HFLIP) | (1 << OPT_VFLIP) ) )
        {
            std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                return 1;
            }

            DWORD dwFlags = 0;

            if ( dwOptions & (1 << OPT_HFLIP) )
                dwFlags |= TEX_FR_FLIP_HORIZONTAL;

            if ( dwOptions & (1 << OPT_VFLIP) )
                dwFlags |= TEX_FR_FLIP_VERTICAL;

            assert( dwFlags != 0 );

            hr = FlipRotate( image->GetImages(), image->GetImageCount(), image->GetMetadata(), dwFlags, *timage );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [fliprotate] (%x)\n", hr);
                return 1;
            }

            auto& tinfo = timage->GetMetadata();

            assert( tinfo.width == twidth && tinfo.height == theight );

            info.width = tinfo.width;
            info.height = tinfo.height;

            assert( info.depth == tinfo.depth );
            assert( info.arraySize == tinfo.arraySize );
            assert( info.mipLevels == tinfo.mipLevels );
            assert( info.miscFlags == tinfo.miscFlags );
            assert( info.miscFlags2 == tinfo.miscFlags2 );
            assert( info.format == tinfo.format );
            assert( info.dimension == tinfo.dimension );

            image.swap( timage );
            cimage.reset();
        }

        // --- Resize ------------------------------------------------------------------
        if ( info.width != twidth || info.height != theight )
        {
            std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                return 1;
            }

            hr = Resize( image->GetImages(), image->GetImageCount(), image->GetMetadata(), twidth, theight, dwFilter | dwFilterOpts, *timage );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [resize] (%x)\n", hr);
                return 1;
            }

            auto& tinfo = timage->GetMetadata();

            assert( tinfo.width == twidth && tinfo.height == theight && tinfo.mipLevels == 1 );
            info.width = tinfo.width;
            info.height = tinfo.height;
            info.mipLevels = 1;

            assert( info.depth == tinfo.depth );
            assert( info.arraySize == tinfo.arraySize );
            assert( info.miscFlags == tinfo.miscFlags );
            assert( info.miscFlags2 == tinfo.miscFlags2 );
            assert( info.format == tinfo.format );
            assert( info.dimension == tinfo.dimension );

            image.swap( timage );
            cimage.reset();
        }

        // --- Convert -----------------------------------------------------------------
        if ( dwOptions & (1 << OPT_NORMAL_MAP) )
        {
            std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                return 1;
            }

            DXGI_FORMAT nmfmt = tformat;
            if ( IsCompressed( tformat ) )
            {
                nmfmt = (dwNormalMap & CNMAP_COMPUTE_OCCLUSION) ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R32G32B32_FLOAT;
            }

            hr = ComputeNormalMap( image->GetImages(), image->GetImageCount(), image->GetMetadata(), dwNormalMap, nmapAmplitude, nmfmt, *timage );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [normalmap] (%x)\n", hr);
                return 1;
            }            

            auto& tinfo = timage->GetMetadata();

            assert( tinfo.format == nmfmt );
            info.format = tinfo.format;

            assert( info.width == tinfo.width );
            assert( info.height == tinfo.height );
            assert( info.depth == tinfo.depth );
            assert( info.arraySize == tinfo.arraySize );
            assert( info.mipLevels == tinfo.mipLevels );
            assert( info.miscFlags == tinfo.miscFlags );
            assert( info.miscFlags2 == tinfo.miscFlags2 );
            assert( info.dimension == tinfo.dimension );

            image.swap( timage );
            cimage.reset();
        }
        else if ( info.format != tformat && !IsCompressed( tformat ) )
        {
            std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                return 1;
            }

            hr = Convert( image->GetImages(), image->GetImageCount(), image->GetMetadata(), tformat, dwFilter | dwFilterOpts | dwSRGB, 0.5f, *timage );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [convert] (%x)\n", hr);
                return 1;
            }

            auto& tinfo = timage->GetMetadata();

            assert( tinfo.format == tformat );
            info.format = tinfo.format;

            assert( info.width == tinfo.width );
            assert( info.height == tinfo.height );
            assert( info.depth == tinfo.depth );
            assert( info.arraySize == tinfo.arraySize );
            assert( info.mipLevels == tinfo.mipLevels );
            assert( info.miscFlags == tinfo.miscFlags );
            assert( info.miscFlags2 == tinfo.miscFlags2 );
            assert( info.dimension == tinfo.dimension );

            image.swap( timage );
            cimage.reset();
        }

        // --- Generate mips -----------------------------------------------------------
        if ( !ispow2(info.width) || !ispow2(info.height) || !ispow2(info.depth) )
        {
            if ( info.dimension == TEX_DIMENSION_TEXTURE3D )
            {
                if ( !tMips )
                {
                    tMips = 1;
                }
                else
                {
                    wprintf( L" ERROR: Cannot generate mips for non-power-of-2 volume textures\n" );
                    return 1;
                }
            }
            else if ( !tMips || info.mipLevels != 1 )
            {
                nonpow2warn = true;
            }
        }

        if ( (!tMips || info.mipLevels != tMips) && ( info.mipLevels != 1 ) )
        {
            // Mips generation only works on a single base image, so strip off existing mip levels
            std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                return 1;
            }

            TexMetadata mdata = info;
            mdata.mipLevels = 1;
            hr = timage->Initialize( mdata );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [copy to single level] (%x)\n", hr);
                return 1;
            }

            if ( info.dimension == TEX_DIMENSION_TEXTURE3D )
            {
                for( size_t d = 0; d < info.depth; ++d )
                {
                    hr = CopyRectangle( *image->GetImage( 0, 0, d ), Rect( 0, 0, info.width, info.height ),
                                        *timage->GetImage( 0, 0, d ), TEX_FILTER_DEFAULT, 0, 0 );
                    if ( FAILED(hr) )
                    {
                        wprintf( L" FAILED [copy to single level] (%x)\n", hr);
                        return 1;
                    }
                }
            }
            else
            {
                for( size_t i = 0; i < info.arraySize; ++i )
                {
                    hr = CopyRectangle( *image->GetImage( 0, i, 0 ), Rect( 0, 0, info.width, info.height ),
                                        *timage->GetImage( 0, i, 0 ), TEX_FILTER_DEFAULT, 0, 0 );
                    if ( FAILED(hr) )
                    {
                        wprintf( L" FAILED [copy to single level] (%x)\n", hr);
                        return 1;
                    }
                }
            }

            image.swap( timage );
            info.mipLevels = image->GetMetadata().mipLevels;

            if ( cimage && ( tMips == 1 ) )
            {
                // Special case for trimming mips off compressed images and keeping the original compressed highest level mip
                mdata = cimage->GetMetadata();
                mdata.mipLevels = 1;
                hr = timage->Initialize( mdata );
                if ( FAILED(hr) )
                {
                    wprintf( L" FAILED [copy compressed to single level] (%x)\n", hr);
                    return 1;
                }

                if ( mdata.dimension == TEX_DIMENSION_TEXTURE3D )
                {
                    for( size_t d = 0; d < mdata.depth; ++d )
                    {
                        auto simg = cimage->GetImage( 0, 0, d );
                        auto dimg = timage->GetImage( 0, 0, d );

                        memcpy_s( dimg->pixels, dimg->slicePitch, simg->pixels, simg->slicePitch );
                    }
                }
                else
                {
                    for( size_t i = 0; i < mdata.arraySize; ++i )
                    {
                        auto simg = cimage->GetImage( 0, i, 0 );
                        auto dimg = timage->GetImage( 0, i, 0 );

                        memcpy_s( dimg->pixels, dimg->slicePitch, simg->pixels, simg->slicePitch );
                    }
                }

                cimage.swap( timage );
            }
            else
            {
                cimage.reset();
            }
        }

        if ( ( !tMips || info.mipLevels != tMips ) && ( info.width > 1 || info.height > 1 || info.depth > 1 ) )
        {
            std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                return 1;
            }

            if ( info.dimension == TEX_DIMENSION_TEXTURE3D )
            {
                hr = GenerateMipMaps3D( image->GetImages(), image->GetImageCount(), image->GetMetadata(), dwFilter | dwFilterOpts, tMips, *timage );
            }
            else
            {
                hr = GenerateMipMaps( image->GetImages(), image->GetImageCount(), image->GetMetadata(), dwFilter | dwFilterOpts, tMips, *timage );
            }
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [mipmaps] (%x)\n", hr);
                return 1;
            }

            auto& tinfo = timage->GetMetadata();
            info.mipLevels = tinfo.mipLevels;

            assert( info.width == tinfo.width );
            assert( info.height == tinfo.height );
            assert( info.depth == tinfo.depth );
            assert( info.arraySize == tinfo.arraySize );
            assert( info.mipLevels == tinfo.mipLevels );
            assert( info.miscFlags == tinfo.miscFlags );
            assert( info.miscFlags2 == tinfo.miscFlags2 );
            assert( info.dimension == tinfo.dimension );


            image.swap( timage );
            cimage.reset();
        }

        // --- Premultiplied alpha (if requested) --------------------------------------
        if ( ( dwOptions & (1 << OPT_PREMUL_ALPHA) )
             && HasAlpha( info.format )
             && info.format != DXGI_FORMAT_A8_UNORM )
        {
            if ( info.IsPMAlpha() )
            {
                printf("WARNING: Image is already using premultiplied alpha\n");
            }
            else
            {
                auto img = image->GetImage(0,0,0);
                assert( img );
                size_t nimg = image->GetImageCount();

                std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
                if ( !timage )
                {
                    wprintf( L" ERROR: Memory allocation failed\n" );
                    return 1;
                }

                hr = PremultiplyAlpha( img, nimg, info, dwSRGB, *timage );
                if ( FAILED(hr) )
                {
                    wprintf( L" FAILED [premultiply alpha] (%x)\n", hr);
                    continue;
                }

                auto& tinfo = timage->GetMetadata();
                info.miscFlags2 = tinfo.miscFlags2;
 
                assert( info.width == tinfo.width );
                assert( info.height == tinfo.height );
                assert( info.depth == tinfo.depth );
                assert( info.arraySize == tinfo.arraySize );
                assert( info.mipLevels == tinfo.mipLevels );
                assert( info.miscFlags == tinfo.miscFlags );
                assert( info.miscFlags2 == tinfo.miscFlags2 );
                assert( info.dimension == tinfo.dimension );

                image.swap( timage );
                cimage.reset();
            }
        }

        // --- Compress ----------------------------------------------------------------
        if ( IsCompressed( tformat ) && (FileType == CODEC_DDS) )
        {
            if ( cimage && ( cimage->GetMetadata().format == tformat ) )
            {
                // We never changed the image and it was already compressed in our desired format, use original data
                image.reset( cimage.release() );

                auto& tinfo = image->GetMetadata();

                if ( (tinfo.width % 4) != 0 || (tinfo.height % 4) != 0 )
                { 
                    non4bc = true;
                }

                info.format = tinfo.format;
                assert( info.width == tinfo.width );
                assert( info.height == tinfo.height );
                assert( info.depth == tinfo.depth );
                assert( info.arraySize == tinfo.arraySize );
                assert( info.mipLevels == tinfo.mipLevels );
                assert( info.miscFlags == tinfo.miscFlags );
                assert( info.miscFlags2 == tinfo.miscFlags2 );
                assert( info.dimension == tinfo.dimension );
            }
            else
            {
                cimage.reset();

                auto img = image->GetImage(0,0,0);
                assert( img );
                size_t nimg = image->GetImageCount();

                std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
                if ( !timage )
                {
                    wprintf( L" ERROR: Memory allocation failed\n" );
                    return 1;
                }

                bool bc6hbc7=false;
                switch( tformat )
                {
                case DXGI_FORMAT_BC6H_TYPELESS:
                case DXGI_FORMAT_BC6H_UF16:
                case DXGI_FORMAT_BC6H_SF16:
                case DXGI_FORMAT_BC7_TYPELESS:
                case DXGI_FORMAT_BC7_UNORM:
                case DXGI_FORMAT_BC7_UNORM_SRGB:
                    bc6hbc7=true;

                    {
                        static bool s_tryonce = false;

                        if ( !s_tryonce )
                        {
                            s_tryonce = true;

                            if ( !(dwOptions & (1 << OPT_NOGPU) ) )
                            {
                                if ( !CreateDevice( pDevice.GetAddressOf() ) )
                                    wprintf( L"\nWARNING: DirectCompute is not available, using BC6H / BC7 CPU codec\n" );
                            }
                            else
                            {
                                wprintf( L"\nWARNING: using BC6H / BC7 CPU codec\n" );
                            }
                        }
                    }
                    break;
                }

                DWORD cflags = TEX_COMPRESS_DEFAULT;
#ifdef _OPENMP
                if ( bc6hbc7 && !(dwOptions & (1 << OPT_FORCE_SINGLEPROC) ) )
                {
                    cflags |= TEX_COMPRESS_PARALLEL;
                }
#endif

                if ( (img->width % 4) != 0 || (img->height % 4) != 0 )
                { 
                    non4bc = true;
                }

                if ( bc6hbc7 && pDevice )
                {
                    hr = Compress( pDevice.Get(), img, nimg, info, tformat, dwSRGB, alphaWeight, *timage );
                }
                else
                {
                    hr = Compress( img, nimg, info, tformat, cflags | dwSRGB, 0.5f, *timage );
                }
                if ( FAILED(hr) )
                {
                    wprintf( L" FAILED [compress] (%x)\n", hr);
                    continue;
                }

                auto& tinfo = timage->GetMetadata();

                info.format = tinfo.format;
                assert( info.width == tinfo.width );
                assert( info.height == tinfo.height );
                assert( info.depth == tinfo.depth );
                assert( info.arraySize == tinfo.arraySize );
                assert( info.mipLevels == tinfo.mipLevels );
                assert( info.miscFlags == tinfo.miscFlags );
                assert( info.miscFlags2 == tinfo.miscFlags2 );
                assert( info.dimension == tinfo.dimension );

                image.swap( timage );
            }
        }
        else
        {
            cimage.reset();
        }

        // --- Set alpha mode ----------------------------------------------------------
        if ( HasAlpha( info.format )
             && info.format != DXGI_FORMAT_A8_UNORM )
        {
            if ( image->IsAlphaAllOpaque() )
            {
                info.SetAlphaMode(TEX_ALPHA_MODE_OPAQUE);
            }
            else if ( info.IsPMAlpha() )
            {
                // Aleady set TEX_ALPHA_MODE_PREMULTIPLIED
            }
            else if ( dwOptions & (1 << OPT_SEPALPHA) )
            {
                info.SetAlphaMode(TEX_ALPHA_MODE_CUSTOM);
            }
            else
            {
                info.SetAlphaMode(TEX_ALPHA_MODE_STRAIGHT);
            }
        }
        else
        {
            info.miscFlags2 &= ~TEX_MISC2_ALPHA_MODE_MASK;
        }

        // --- Save result -------------------------------------------------------------
        {
            auto img = image->GetImage(0,0,0);
            assert( img );
            size_t nimg = image->GetImageCount();

            PrintInfo( info );
            wprintf( L"\n");

            // Figure out dest filename
            WCHAR *pchSlash, *pchDot;

            wcscpy_s(pConv->szDest, MAX_PATH, szPrefix);

            pchSlash = wcsrchr(pConv->szSrc, L'\\');
            if(pchSlash != 0)
                wcscat_s(pConv->szDest, MAX_PATH, pchSlash + 1);
            else
                wcscat_s(pConv->szDest, MAX_PATH, pConv->szSrc);

            pchSlash = wcsrchr(pConv->szDest, '\\');
            pchDot = wcsrchr(pConv->szDest, '.');

            if(pchDot > pchSlash)
                *pchDot = 0;

            wcscat_s(pConv->szDest, MAX_PATH, szSuffix);

            // Write texture
            wprintf( L"writing %ls", pConv->szDest);
            fflush(stdout);

            switch( FileType )
            {
            case CODEC_DDS:
                hr = SaveToDDSFile( img, nimg, info,
                                    (dwOptions & (1 << OPT_USE_DX10) ) ? (DDS_FLAGS_FORCE_DX10_EXT|DDS_FLAGS_FORCE_DX10_EXT_MISC2) : DDS_FLAGS_NONE, 
                                    pConv->szDest );
                break;

            case CODEC_TGA:
                hr = SaveToTGAFile( img[0], pConv->szDest );
                break;

            default:
                hr = SaveToWICFile( img, nimg, WIC_FLAGS_ALL_FRAMES, GetWICCodec( static_cast<WICCodecs>(FileType) ), pConv->szDest );
                break;
            }

            if(FAILED(hr))
            {
                wprintf( L" FAILED (%x)\n", hr);
                continue;
            }
            wprintf( L"\n");
        }
    }

    if ( nonpow2warn )
        wprintf( L"\n WARNING: Not all feature levels support non-power-of-2 textures with mipmaps\n" );

    if ( non4bc )
        wprintf( L"\n WARNING: Direct3D requires BC image to be multiple of 4 in width & height\n" );

    return 0;
}
