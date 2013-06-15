//--------------------------------------------------------------------------------------
// File: Texassemble.cpp
//
// DirectX 11 Texture assembler for cube maps, volume maps, and arrays
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <dxgiformat.h>

#include "directxtex.h"

#include <vector>

using namespace DirectX;

enum OPTIONS    // Note: dwOptions below assumes 32 or less options.
{
    OPT_CUBE = 1,
    OPT_VOLUME,
    OPT_ARRAY,
    OPT_CUBEARRAY,
    OPT_WIDTH,
    OPT_HEIGHT,
    OPT_FORMAT,
    OPT_FILTER,
    OPT_OUTPUTFILE,
    OPT_USE_DX10,
    OPT_NOLOGO,
    OPT_SEPALPHA,
};

struct SConversion
{
    WCHAR szSrc [MAX_PATH];

    SConversion *pNext;
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
    { L"cube",          OPT_CUBE      },
    { L"volume",        OPT_VOLUME    },
    { L"array",         OPT_ARRAY     },
    { L"cubearray",     OPT_CUBEARRAY },
    { L"w",             OPT_WIDTH     },
    { L"h",             OPT_HEIGHT    },
    { L"f",             OPT_FORMAT    },
    { L"if",            OPT_FILTER    },
    { L"o",             OPT_OUTPUTFILE },
    { L"dx10",          OPT_USE_DX10  },
    { L"nologo",        OPT_NOLOGO    },
    { L"sepalpha",      OPT_SEPALPHA  },
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

#ifdef DXGI_1_2_FORMATS
    // DXGI 1.2 formats
    DEFFMT(B4G4R4A4_UNORM),
#endif

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

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

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
            break;
        }
    }
}

void PrintInfo( const TexMetadata& info )
{
    wprintf( L" (%Iux%Iu", info.width, info.height);

    if ( TEX_DIMENSION_TEXTURE3D == info.dimension )
        wprintf( L"x%Iu", info.depth);

    if ( info.mipLevels > 1 )
        wprintf( L",%Iu", info.mipLevels);

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

        wprintf( L"%s ", pValue->pName );
        cch += cchName + 2;
        pValue++;
    }

    wprintf( L"\n");
}


void PrintLogo()
{
    wprintf( L"Microsoft (R) DirectX 11 Texture Assembler (DirectXTex version)\n");
    wprintf( L"Copyright (C) Microsoft Corp. All rights reserved.\n");
    wprintf( L"\n");
}


void PrintUsage()
{
    PrintLogo();

    wprintf( L"Usage: texassemble [-cube | - volume | -array | -cubearray] <options> <files>\n");
    wprintf( L"\n");
    wprintf( L"   -cube               create cubemap\n");
    wprintf( L"   -volume             create volume map\n");
    wprintf( L"   -array              create texture array\n");
    wprintf( L"   -cubearray          create cubemap array\n");
    wprintf( L"   -w <n>              width\n");
    wprintf( L"   -h <n>              height\n");
    wprintf( L"   -f <format>         format\n");
    wprintf( L"   -if <filter>        image filtering\n");
    wprintf( L"   -o <filename>       output filename\n");
    wprintf( L"   -sepalpha           resize alpha channel separately from color channels\n");
    wprintf( L"   -dx10               Force use of 'DX10' extended header\n");
    wprintf( L"   -nologo             suppress copyright message\n");

    wprintf( L"\n");
    wprintf( L"   <format>: ");
    PrintList(13, g_pFormats);

    wprintf( L"\n");
    wprintf( L"   <filter>: ");
    PrintList(13, g_pFilters);
}


//--------------------------------------------------------------------------------------
// Entry-point
//--------------------------------------------------------------------------------------
#pragma prefast(disable : 28198, "Command-line tool, frees all memory on exit")

int __cdecl wmain(_In_ int argc, _In_z_count_(argc) wchar_t* argv[])
{
    // Parameters and defaults
    HRESULT hr;
    INT nReturn;

    size_t width = 0;
    size_t height = 0; 

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    DWORD dwFilter = TEX_FILTER_DEFAULT;
    DWORD dwFilterOpts = 0;

    WCHAR szOutputFile[MAX_PATH] = { 0 };

    // Initialize COM (needed for WIC)
    if( FAILED( hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED) ) )
    {
        wprintf( L"Failed to initialize COM (%08X)\n", hr);
        return 1;
    }

    // Process command line
    DWORD dwOptions = 0;
    SConversion *pConversion = nullptr;
    SConversion **ppConversion = &pConversion;

    size_t images = 0;

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

            if( (OPT_NOLOGO != dwOption) && (OPT_SEPALPHA != dwOption) && (OPT_USE_DX10 != dwOption)
                && (OPT_CUBE != dwOption) && (OPT_VOLUME != dwOption) && (OPT_ARRAY != dwOption) && (OPT_CUBEARRAY != dwOption) )
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
                    wprintf( L"Invalid value specified with -w (%s)\n", pValue);
                    return 1;
                }
                break;

            case OPT_HEIGHT:
                if (swscanf_s(pValue, L"%Iu", &height) != 1)
                {
                    wprintf( L"Invalid value specified with -h (%s)\n", pValue);
                    return 1;
                }
                break;

            case OPT_FORMAT:
                format = (DXGI_FORMAT) LookupByName(pValue, g_pFormats);
                if ( !format )
                {
                    wprintf( L"Invalid value specified with -f (%s)\n", pValue);
                    return 1;
                }
                break;

            case OPT_FILTER:
                dwFilter = LookupByName(pValue, g_pFilters);
                if ( !dwFilter )
                {
                    wprintf( L"Invalid value specified with -if (%s)\n", pValue);
                    return 1;
                }
                break;

            case OPT_SEPALPHA:
                dwFilterOpts |= TEX_FILTER_SEPARATE_ALPHA;
                break;

            case OPT_OUTPUTFILE:
                wcscpy_s(szOutputFile, MAX_PATH, pValue);
                break;
            }
        }
        else
        {         
            SConversion *pConv = new SConversion;
            if ( !pConv )
                return 1;

            wcscpy_s(pConv->szSrc, MAX_PATH, pArg);

            pConv->pNext = nullptr;

            *ppConversion = pConv;
            ppConversion = &pConv->pNext;

            ++images;
        }
    }

    if( !pConversion || images < 2 )
    {
        wprintf( L"ERROR: Need at least 2 images to assemble\n\n");
        PrintUsage();
        return 0;
    }

    switch( dwOptions & ( (1 << OPT_CUBE) | (1 << OPT_VOLUME) | (1 << OPT_ARRAY) | (1 << OPT_CUBEARRAY) ) )
    {
    case (1 << OPT_VOLUME):
    case (1 << OPT_ARRAY):
        break;

    case (1 << OPT_CUBE):
        if ( images != 6 )
        {
            wprintf( L"ERROR: -cube requires six images to form the faces of the cubemap\n");
            return 1;
        }
        break;

    case (1 << OPT_CUBEARRAY):
        if ( ( images < 6) || ( images % 6 ) != 0 )
        {
            wprintf( L"-cubearray requires a multiple of 6 images to form the faces of the cubemaps\n");
            return 1;
        }
        break;

    default:
        wprintf( L"Must use one of: -cube, -volume, -array, or -cubearray\n\n" );
        return 1;
    }

    if(~dwOptions & (1 << OPT_NOLOGO))
        PrintLogo();

    // Convert images
    std::vector<ScratchImage*> loadedImages; 

    for( SConversion *pConv = pConversion; pConv; pConv = pConv->pNext )
    {
        WCHAR ext[_MAX_EXT];
        WCHAR fname[_MAX_FNAME];
        _wsplitpath_s( pConv->szSrc, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT );

        // Load source image
        if( pConv != pConversion )
            wprintf( L"\n");
        else if ( !*szOutputFile )
        {
            if ( _wcsicmp( ext, L".dds" ) == 0 )
            {
                wprintf( L"ERROR: Need to specify output file via -o\n");
                return 1;
            }

            _wmakepath_s( szOutputFile, nullptr, nullptr, fname, L".dds" );
        }

        wprintf( L"reading %s", pConv->szSrc );
        fflush(stdout);

        TexMetadata info;
        ScratchImage *image = new ScratchImage;

        if ( !image )
        {
            wprintf( L" ERROR: Memory allocation failed\n" );
            goto LError;
        }

        if ( _wcsicmp( ext, L".dds" ) == 0 )
        {
            hr = LoadFromDDSFile( pConv->szSrc, DDS_FLAGS_NONE, &info, *image );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED (%x)\n", hr);
                return 1;
            }

            if ( info.arraySize > 1
                 || info.depth > 1
                 || info.mipLevels > 1
                 || info.IsCubemap() )
            {
                wprintf( L"ERROR: Can't assemble complex surfaces\n" );
                return 1;
            }
        }
        else if ( _wcsicmp( ext, L".tga" ) == 0 )
        {
            hr = LoadFromTGAFile( pConv->szSrc, &info, *image );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED (%x)\n", hr);
                return 1;
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

            hr = LoadFromWICFile( pConv->szSrc, dwFilter, &info, *image );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED (%x)\n", hr);
                return 1;
            }
        }

        PrintInfo( info );

        // Convert texture
        fflush(stdout);

        // --- Decompress --------------------------------------------------------------
        if ( IsCompressed( info.format ) )
        {
            const Image* img = image->GetImage(0,0,0);
            assert( img );
            size_t nimg = image->GetImageCount();

            ScratchImage *timage = new ScratchImage;
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                delete image;
                goto LError;
            }

            hr = Decompress( img, nimg, info, DXGI_FORMAT_UNKNOWN /* picks good default */, *timage );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [decompress] (%x)\n", hr);
                delete timage;
                delete image;
                continue;
            }

            const TexMetadata& tinfo = timage->GetMetadata();

            info.format = tinfo.format;

            assert( info.width == tinfo.width );
            assert( info.height == tinfo.height );
            assert( info.depth == tinfo.depth );
            assert( info.arraySize == tinfo.arraySize );
            assert( info.mipLevels == tinfo.mipLevels );
            assert( info.miscFlags == tinfo.miscFlags );
            assert( info.miscFlags2 == tinfo.miscFlags2 );
            assert( info.dimension == tinfo.dimension );

            delete image;
            image = timage;
        }

        // --- Resize ------------------------------------------------------------------
        if ( !width )
        {
            width = info.width;
        }
        if ( !height )
        {
            height = info.height;
        }
        if ( info.width != width || info.height != height )
        {
            ScratchImage *timage = new ScratchImage;
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                delete image;
                goto LError;
            }

            hr = Resize( image->GetImages(), image->GetImageCount(), image->GetMetadata(), width, height, dwFilter | dwFilterOpts, *timage );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [resize] (%x)\n", hr);
                delete timage;
                delete image;
                goto LError;
            }

            const TexMetadata& tinfo = timage->GetMetadata();

            assert( tinfo.width == width && tinfo.height == height && tinfo.mipLevels == 1 );
            info.width = tinfo.width;
            info.height = tinfo.height;
            info.mipLevels = 1;

            assert( info.depth == tinfo.depth );
            assert( info.arraySize == tinfo.arraySize );
            assert( info.miscFlags == tinfo.miscFlags );
            assert( info.miscFlags2 == tinfo.miscFlags2 );
            assert( info.format == tinfo.format );
            assert( info.dimension == tinfo.dimension );

            delete image;
            image = timage;
        }

        // --- Convert -----------------------------------------------------------------
        if ( format == DXGI_FORMAT_UNKNOWN )
        {
            format = info.format;
        }
        else if ( info.format != format && !IsCompressed( format ) )
        {
            ScratchImage *timage = new ScratchImage;
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                delete image;
                goto LError;
            }

            hr = Convert( image->GetImages(), image->GetImageCount(), image->GetMetadata(), format, dwFilter | dwFilterOpts, 0.5f, *timage );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [convert] (%x)\n", hr);
                delete timage;
                delete image;
                goto LError;
            }

            const TexMetadata& tinfo = timage->GetMetadata();

            assert( tinfo.format == format );
            info.format = tinfo.format;

            assert( info.width == tinfo.width );
            assert( info.height == tinfo.height );
            assert( info.depth == tinfo.depth );
            assert( info.arraySize == tinfo.arraySize );
            assert( info.mipLevels == tinfo.mipLevels );
            assert( info.miscFlags == tinfo.miscFlags );
            assert( info.miscFlags2 == tinfo.miscFlags2 );
            assert( info.dimension == tinfo.dimension );

            delete image;
            image = timage;
        }

        loadedImages.push_back( image );
    }

    // --- Create result ---------------------------------------------------------------
    {
        std::vector<Image> imageArray;
        imageArray.reserve( images );

        for( auto it = loadedImages.cbegin(); it != loadedImages.cend(); ++it )
        {
            const Image* img = (*it)->GetImage(0,0,0);
            assert( img != 0 );
            imageArray.push_back( *img );
        }

        ScratchImage result;
        switch( dwOptions & ( (1 << OPT_CUBE) | (1 << OPT_VOLUME) | (1 << OPT_ARRAY) | (1 << OPT_CUBEARRAY) ) )
        {
        case (1 << OPT_VOLUME):
            hr = result.Initialize3DFromImages( &imageArray[0], imageArray.size() );
            break;

        case (1 << OPT_ARRAY):
            hr = result.InitializeArrayFromImages( &imageArray[0], imageArray.size(), (dwOptions & (1 << OPT_USE_DX10)) != 0 );
            break;

        case (1 << OPT_CUBE):
        case (1 << OPT_CUBEARRAY):
            hr = result.InitializeCubeFromImages( &imageArray[0], imageArray.size() );
            break;
        }

        if ( FAILED(hr ) )
        {
            wprintf( L"FAILED building result image (%x)\n", hr);
            goto LError;
        }


        // Write texture
        wprintf( L"\nWriting %s ", szOutputFile);
        PrintInfo( result.GetMetadata() );
        wprintf( L"\n" );
        fflush(stdout);

        hr = SaveToDDSFile( result.GetImages(), result.GetImageCount(), result.GetMetadata(),
                            (dwOptions & (1 << OPT_USE_DX10) ) ? (DDS_FLAGS_FORCE_DX10_EXT|DDS_FLAGS_FORCE_DX10_EXT_MISC2) : DDS_FLAGS_NONE, 
                            szOutputFile );
        if(FAILED(hr))
        {
            wprintf( L"\nFAILED (%x)\n", hr);
            return 1;
        }
        wprintf( L"\n");
    }

    nReturn = 0;

    goto LDone;

LError:
    nReturn = 1;

LDone:

    while(pConversion)
    {
        auto pConv = pConversion;
        pConversion = pConversion->pNext;
        delete pConv;
    }

    return nReturn;
}
