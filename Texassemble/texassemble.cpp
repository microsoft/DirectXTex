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

#include <memory>
#include <list>
#include <vector>

#include <dxgiformat.h>

#include "directxtex.h"

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
    OPT_MAX
};

static_assert( OPT_MAX <= 32, "dwOptions is a DWORD bitfield" );

struct SConversion
{
    WCHAR szSrc [MAX_PATH];
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

    // DXGI 1.2 formats
    DEFFMT(B4G4R4A4_UNORM),

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
    size_t width = 0;
    size_t height = 0; 

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    DWORD dwFilter = TEX_FILTER_DEFAULT;
    DWORD dwFilterOpts = 0;

    WCHAR szOutputFile[MAX_PATH] = { 0 };

    // Initialize COM (needed for WIC)
    HRESULT hr = hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
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
                    wprintf( L"Invalid value specified with -w (%ls)\n", pValue);
                    return 1;
                }
                break;

            case OPT_HEIGHT:
                if (swscanf_s(pValue, L"%Iu", &height) != 1)
                {
                    wprintf( L"Invalid value specified with -h (%ls)\n", pValue);
                    return 1;
                }
                break;

            case OPT_FORMAT:
                format = (DXGI_FORMAT) LookupByName(pValue, g_pFormats);
                if ( !format )
                {
                    wprintf( L"Invalid value specified with -f (%ls)\n", pValue);
                    return 1;
                }
                break;

            case OPT_FILTER:
                dwFilter = LookupByName(pValue, g_pFilters);
                if ( !dwFilter )
                {
                    wprintf( L"Invalid value specified with -if (%ls)\n", pValue);
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
            SConversion conv;
            wcscpy_s(conv.szSrc, MAX_PATH, pArg);

            conversion.push_back(conv);
        }
    }

    if(conversion.empty())
    {
        PrintUsage();
        return 0;
    }

    switch( dwOptions & ( (1 << OPT_CUBE) | (1 << OPT_VOLUME) | (1 << OPT_ARRAY) | (1 << OPT_CUBEARRAY) ) )
    {
    case (1 << OPT_VOLUME):
    case (1 << OPT_ARRAY):
    case (1 << OPT_CUBE):
    case (1 << OPT_CUBEARRAY):
        break;

    default:
        wprintf( L"Must use one of: -cube, -volume, -array, or -cubearray\n\n" );
        return 1;
    }

    if(~dwOptions & (1 << OPT_NOLOGO))
        PrintLogo();

    // Convert images
    size_t images = 0;

    std::vector<std::unique_ptr<ScratchImage>> loadedImages; 

    for( auto pConv = conversion.begin(); pConv != conversion.end(); ++pConv )
    {
        WCHAR ext[_MAX_EXT];
        WCHAR fname[_MAX_FNAME];
        _wsplitpath_s( pConv->szSrc, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT );

        // Load source image
        if( pConv != conversion.begin() )
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

        wprintf( L"reading %ls", pConv->szSrc );
        fflush(stdout);

        TexMetadata info;
        std::unique_ptr<ScratchImage> image( new (std::nothrow) ScratchImage );
        if ( !image )
        {
            wprintf( L" ERROR: Memory allocation failed\n" );
            return 1;
        }

        if ( _wcsicmp( ext, L".dds" ) == 0 )
        {
            hr = LoadFromDDSFile( pConv->szSrc, DDS_FLAGS_NONE, &info, *image.get() );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED (%x)\n", hr);
                return 1;
            }

            if ( info.depth > 1
                 || info.mipLevels > 1
                 || info.IsCubemap() )
            {
                wprintf( L" ERROR: Can't assemble complex surfaces\n" );
                return 1;
            }
        }
        else if ( _wcsicmp( ext, L".tga" ) == 0 )
        {
            hr = LoadFromTGAFile( pConv->szSrc, &info, *image.get() );
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

            hr = LoadFromWICFile( pConv->szSrc, dwFilter | WIC_FLAGS_ALL_FRAMES, &info, *image.get() );
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

            std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                return 1;
            }

            hr = Decompress( img, nimg, info, DXGI_FORMAT_UNKNOWN /* picks good default */, *timage.get() );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [decompress] (%x)\n", hr);
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

            image.swap( timage );
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
            std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                return 1;
            }

            hr = Resize( image->GetImages(), image->GetImageCount(), image->GetMetadata(), width, height, dwFilter | dwFilterOpts, *timage.get() );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [resize] (%x)\n", hr);
                return 1;
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

            image.swap( timage );
        }

        // --- Convert -----------------------------------------------------------------
        if ( format == DXGI_FORMAT_UNKNOWN )
        {
            format = info.format;
        }
        else if ( info.format != format && !IsCompressed( format ) )
        {
            std::unique_ptr<ScratchImage> timage( new (std::nothrow) ScratchImage );
            if ( !timage )
            {
                wprintf( L" ERROR: Memory allocation failed\n" );
                return 1;
            }

            hr = Convert( image->GetImages(), image->GetImageCount(), image->GetMetadata(), format, dwFilter | dwFilterOpts, 0.5f, *timage.get() );
            if ( FAILED(hr) )
            {
                wprintf( L" FAILED [convert] (%x)\n", hr);
                return 1;
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

            image.swap( timage );
        }

        images += info.arraySize;
        loadedImages.push_back( std::move( image ) );
    }

    if( images < 2 )
    {
        wprintf( L" ERROR: Need at least 2 images to assemble\n\n");
        return 1;
    }

    switch( dwOptions & ( (1 << OPT_CUBE) | (1 << OPT_VOLUME) | (1 << OPT_ARRAY) | (1 << OPT_CUBEARRAY) ) )
    {
    case (1 << OPT_CUBE):
        if ( images != 6 )
        {
            wprintf( L" ERROR: -cube requires six images to form the faces of the cubemap\n");
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
    }

    // --- Create result ---------------------------------------------------------------
    {
        std::vector<Image> imageArray;
        imageArray.reserve( images );

        for( auto it = loadedImages.cbegin(); it != loadedImages.cend(); ++it )
        {
            const ScratchImage* simage = it->get();
            assert( simage != 0 );
            for( size_t j = 0; j < simage->GetMetadata().arraySize; ++j )
            {
                const Image* img = simage->GetImage(0,j,0);
                assert( img != 0 );
                imageArray.push_back( *img );
            }
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
            return 1;
        }

        // Write texture
        wprintf( L"\nWriting %ls ", szOutputFile);
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

    return 0;
}
