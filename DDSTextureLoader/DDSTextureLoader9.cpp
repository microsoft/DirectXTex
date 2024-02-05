//--------------------------------------------------------------------------------------
// File: DDSTextureLoader9.cpp
//
// Functions for loading a DDS texture and creating a Direct3D runtime resource for it
//
// Note these functions are useful as a light-weight runtime loader for DDS files. For
// a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "DDSTextureLoader9.h"

#include <d3d9types.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <new>

#include <wrl/client.h>

#ifdef _MSC_VER
// Off by default warnings
#pragma warning(disable : 4619 4616 4623 4626 5027)
// C4619/4616 #pragma warning warnings
// C4623 default constructor was implicitly defined as deleted
// C4626 assignment operator was implicitly defined as deleted
// C5027 move assignment operator was implicitly defined as deleted
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

using namespace DirectX;
using Microsoft::WRL::ComPtr;

//--------------------------------------------------------------------------------------
// Macros
//--------------------------------------------------------------------------------------
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
                ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24 ))
#endif /* defined(MAKEFOURCC) */

//--------------------------------------------------------------------------------------
// DDS file structure definitions
//
// See DDS.h in the 'Texconv' sample and the 'DirectXTex' library
//--------------------------------------------------------------------------------------
#pragma pack(push,1)

constexpr uint32_t DDS_MAGIC = 0x20534444; // "DDS "

struct DDS_PIXELFORMAT
{
    uint32_t    size;
    uint32_t    flags;
    uint32_t    fourCC;
    uint32_t    RGBBitCount;
    uint32_t    RBitMask;
    uint32_t    GBitMask;
    uint32_t    BBitMask;
    uint32_t    ABitMask;
};

#define DDS_FOURCC          0x00000004  // DDPF_FOURCC
#define DDS_RGB             0x00000040  // DDPF_RGB
#define DDS_LUMINANCE       0x00020000  // DDPF_LUMINANCE
#define DDS_ALPHA           0x00000002  // DDPF_ALPHA
#define DDS_BUMPDUDV        0x00080000  // DDPF_BUMPDUDV
#define DDS_BUMPLUMINANCE   0x00040000  // DDPF_BUMPLUMINANCE

#define DDS_HEADER_FLAGS_VOLUME         0x00800000  // DDSD_DEPTH

#define DDS_CUBEMAP_POSITIVEX 0x00000600 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX
#define DDS_CUBEMAP_NEGATIVEX 0x00000a00 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX
#define DDS_CUBEMAP_POSITIVEY 0x00001200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY
#define DDS_CUBEMAP_NEGATIVEY 0x00002200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY
#define DDS_CUBEMAP_POSITIVEZ 0x00004200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ
#define DDS_CUBEMAP_NEGATIVEZ 0x00008200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ

#define DDS_CUBEMAP_ALLFACES ( DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX |\
                               DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |\
                               DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ )

#define DDS_CUBEMAP 0x00000200 // DDSCAPS2_CUBEMAP

struct DDS_HEADER
{
    uint32_t        size;
    uint32_t        flags;
    uint32_t        height;
    uint32_t        width;
    uint32_t        pitchOrLinearSize;
    uint32_t        depth; // only if DDS_HEADER_FLAGS_VOLUME is set in flags
    uint32_t        mipMapCount;
    uint32_t        reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t        caps;
    uint32_t        caps2;
    uint32_t        caps3;
    uint32_t        caps4;
    uint32_t        reserved2;
};

#pragma pack(pop)

//--------------------------------------------------------------------------------------
namespace
{
    struct handle_closer { void operator()(HANDLE h) noexcept { if (h) CloseHandle(h); } };

    using ScopedHandle = std::unique_ptr<void, handle_closer>;

    inline HANDLE safe_handle(HANDLE h) noexcept { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

    //--------------------------------------------------------------------------------------
    HRESULT LoadTextureDataFromMemory(
        _In_reads_(ddsDataSize) const uint8_t* ddsData,
        size_t ddsDataSize,
        const DDS_HEADER** header,
        const uint8_t** bitData,
        size_t* bitSize) noexcept
    {
        if (!header || !bitData || !bitSize)
        {
            return E_POINTER;
        }

        *bitSize = 0;

        if (ddsDataSize > UINT32_MAX)
        {
            return E_FAIL;
        }

        if (ddsDataSize < (sizeof(uint32_t) + sizeof(DDS_HEADER)))
        {
            return E_FAIL;
        }

        // DDS files always start with the same magic number ("DDS ")
        auto const dwMagicNumber = *reinterpret_cast<const uint32_t*>(ddsData);
        if (dwMagicNumber != DDS_MAGIC)
        {
            return E_FAIL;
        }

        auto hdr = reinterpret_cast<const DDS_HEADER*>(ddsData + sizeof(uint32_t));

        // Verify header to validate DDS file
        if (hdr->size != sizeof(DDS_HEADER) ||
            hdr->ddspf.size != sizeof(DDS_PIXELFORMAT))
        {
            return E_FAIL;
        }

        // Check for DX10 extension
        if ((hdr->ddspf.flags & DDS_FOURCC) &&
            (MAKEFOURCC('D', 'X', '1', '0') == hdr->ddspf.fourCC))
        {
            // We don't support the new DX10 header for Direct3D 9
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        // setup the pointers in the process request
        *header = hdr;
        auto offset = sizeof(uint32_t) + sizeof(DDS_HEADER);
        *bitData = ddsData + offset;
        *bitSize = ddsDataSize - offset;

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    HRESULT LoadTextureDataFromFile(
        _In_z_ const wchar_t* fileName,
        std::unique_ptr<uint8_t[]>& ddsData,
        const DDS_HEADER** header,
        const uint8_t** bitData,
        size_t* bitSize) noexcept
    {
        if (!header || !bitData || !bitSize)
        {
            return E_POINTER;
        }

        *bitSize = 0;

        // open the file
    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
        ScopedHandle hFile(safe_handle(CreateFile2(fileName,
            GENERIC_READ,
            FILE_SHARE_READ,
            OPEN_EXISTING,
            nullptr)));
    #else
        ScopedHandle hFile(safe_handle(CreateFileW(fileName,
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr)));
    #endif

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

        // File is too big for 32-bit allocation, so reject read
        if (fileInfo.EndOfFile.HighPart > 0)
        {
            return E_FAIL;
        }

        // Need at least enough data to fill the header and magic number to be a valid DDS
        if (fileInfo.EndOfFile.LowPart < (sizeof(uint32_t) + sizeof(DDS_HEADER)))
        {
            return E_FAIL;
        }

        // create enough space for the file data
        ddsData.reset(new (std::nothrow) uint8_t[fileInfo.EndOfFile.LowPart]);
        if (!ddsData)
        {
            return E_OUTOFMEMORY;
        }

        // read the data in
        DWORD bytesRead = 0;
        if (!ReadFile(hFile.get(),
            ddsData.get(),
            fileInfo.EndOfFile.LowPart,
            &bytesRead,
            nullptr
        ))
        {
            ddsData.reset();
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (bytesRead < fileInfo.EndOfFile.LowPart)
        {
            ddsData.reset();
            return E_FAIL;
        }

        // DDS files always start with the same magic number ("DDS ")
        auto const dwMagicNumber = *reinterpret_cast<const uint32_t*>(ddsData.get());
        if (dwMagicNumber != DDS_MAGIC)
        {
            ddsData.reset();
            return E_FAIL;
        }

        auto hdr = reinterpret_cast<const DDS_HEADER*>(ddsData.get() + sizeof(uint32_t));

        // Verify header to validate DDS file
        if (hdr->size != sizeof(DDS_HEADER) ||
            hdr->ddspf.size != sizeof(DDS_PIXELFORMAT))
        {
            ddsData.reset();
            return E_FAIL;
        }

        // Check for DX10 extension
        if ((hdr->ddspf.flags & DDS_FOURCC) &&
            (MAKEFOURCC('D', 'X', '1', '0') == hdr->ddspf.fourCC))
        {
            // We don't support the new DX10 header for Direct3D 9
            ddsData.reset();
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        // setup the pointers in the process request
        *header = hdr;
        auto offset = sizeof(uint32_t) + sizeof(DDS_HEADER);
        *bitData = ddsData.get() + offset;
        *bitSize = fileInfo.EndOfFile.LowPart - offset;

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Return the BPP for a particular format
    //--------------------------------------------------------------------------------------
    size_t BitsPerPixel(_In_ D3DFORMAT fmt) noexcept
    {
        switch (static_cast<int>(fmt))
        {
        case D3DFMT_A32B32G32R32F:
            return 128;

        case D3DFMT_A16B16G16R16:
        case D3DFMT_Q16W16V16U16:
        case D3DFMT_A16B16G16R16F:
        case D3DFMT_G32R32F:
            return 64;

        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
        case D3DFMT_A2B10G10R10:
        case D3DFMT_A8B8G8R8:
        case D3DFMT_X8B8G8R8:
        case D3DFMT_G16R16:
        case D3DFMT_A2R10G10B10:
        case D3DFMT_Q8W8V8U8:
        case D3DFMT_V16U16:
        case D3DFMT_X8L8V8U8:
        case D3DFMT_A2W10V10U10:
        case D3DFMT_D32:
        case D3DFMT_D24S8:
        case D3DFMT_D24X8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D32F_LOCKABLE:
        case D3DFMT_D24FS8:
        case D3DFMT_INDEX32:
        case D3DFMT_G16R16F:
        case D3DFMT_R32F:
        #if !defined(D3D_DISABLE_9EX)
        case D3DFMT_D32_LOCKABLE:
        #endif
            return 32;

        case D3DFMT_R8G8B8:
            return 24;

        case D3DFMT_A4R4G4B4:
        case D3DFMT_X4R4G4B4:
        case D3DFMT_R5G6B5:
        case D3DFMT_L16:
        case D3DFMT_A8L8:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A8R3G3B2:
        case D3DFMT_V8U8:
        case D3DFMT_CxV8U8:
        case D3DFMT_L6V5U5:
        case D3DFMT_G8R8_G8B8:
        case D3DFMT_R8G8_B8G8:
        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D15S1:
        case D3DFMT_D16:
        case D3DFMT_INDEX16:
        case D3DFMT_R16F:
        case D3DFMT_YUY2:
            // From DX docs, reference/d3d/enums/d3dformat.asp
            // (note how it says that D3DFMT_R8G8_B8G8 is "A 16-bit packed RGB format analogous to UYVY (U0Y0, V0Y1, U2Y2, and so on)")
        case D3DFMT_UYVY:
            return 16;

        case D3DFMT_R3G3B2:
        case D3DFMT_A8:
        case D3DFMT_A8P8:
        case D3DFMT_P8:
        case D3DFMT_L8:
        case D3DFMT_A4L4:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/directshow/htm/directxvideoaccelerationdxvavideosubtypes.asp
        case MAKEFOURCC('A', 'I', '4', '4'):
        case MAKEFOURCC('I', 'A', '4', '4'):
        #if !defined(D3D_DISABLE_9EX)
        case D3DFMT_S8_LOCKABLE:
        #endif
            return 8;

        case D3DFMT_DXT1:
            return 4;

        case MAKEFOURCC('Y', 'V', '1', '2'):
            return 12;

        #if !defined(D3D_DISABLE_9EX)
        case D3DFMT_A1:
            return 1;
        #endif

        default:
            return 0;
        }
    }


    //--------------------------------------------------------------------------------------
    // Get surface information for a particular format
    //--------------------------------------------------------------------------------------
    HRESULT GetSurfaceInfo(
        _In_ size_t width,
        _In_ size_t height,
        _In_ D3DFORMAT fmt,
        size_t* outNumBytes,
        _Out_opt_ size_t* outRowBytes,
        _Out_opt_ size_t* outNumRows) noexcept
    {
        uint64_t numBytes = 0;
        uint64_t rowBytes = 0;
        uint64_t numRows = 0;

        bool bc = false;
        bool packed = false;
        size_t bpe = 0;
        switch (static_cast<int>(fmt))
        {
        case D3DFMT_DXT1:
            bc = true;
            bpe = 8;
            break;

        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            bc = true;
            bpe = 16;
            break;

        case D3DFMT_R8G8_B8G8:
        case D3DFMT_G8R8_G8B8:
        case D3DFMT_UYVY:
        case D3DFMT_YUY2:
            packed = true;
            bpe = 4;
            break;

        default:
            break;
        }

        if (bc)
        {
            uint64_t numBlocksWide = 0;
            if (width > 0)
            {
                numBlocksWide = std::max<uint64_t>(1u, (uint64_t(width) + 3u) / 4u);
            }
            uint64_t numBlocksHigh = 0;
            if (height > 0)
            {
                numBlocksHigh = std::max<uint64_t>(1u, (uint64_t(height) + 3u) / 4u);
            }
            rowBytes = numBlocksWide * bpe;
            numRows = numBlocksHigh;
            numBytes = rowBytes * numBlocksHigh;
        }
        else if (packed)
        {
            rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
            numRows = uint64_t(height);
            numBytes = rowBytes * height;
        }
        else
        {
            const size_t bpp = BitsPerPixel(fmt);
            if (!bpp)
                return E_INVALIDARG;

            rowBytes = (uint64_t(width) * bpp + 7u) / 8u; // round up to nearest byte
            numRows = uint64_t(height);
            numBytes = rowBytes * height;
        }

    #if defined(_M_IX86) || defined(_M_ARM) || defined(_M_HYBRID_X86_ARM64)
        static_assert(sizeof(size_t) == 4, "Not a 32-bit platform!");
        if (numBytes > UINT32_MAX || rowBytes > UINT32_MAX || numRows > UINT32_MAX)
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    #else
        static_assert(sizeof(size_t) == 8, "Not a 64-bit platform!");
    #endif

        if (outNumBytes)
        {
            *outNumBytes = static_cast<size_t>(numBytes);
        }
        if (outRowBytes)
        {
            *outRowBytes = static_cast<size_t>(rowBytes);
        }
        if (outNumRows)
        {
            *outNumRows = static_cast<size_t>(numRows);
        }

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
#define ISBITMASK( r,g,b,a ) ( ddpf.RBitMask == r && ddpf.GBitMask == g && ddpf.BBitMask == b && ddpf.ABitMask == a )

    D3DFORMAT GetD3D9Format(const DDS_PIXELFORMAT& ddpf) noexcept
    {
        if (ddpf.flags & DDS_RGB)
        {
            switch (ddpf.RGBBitCount)
            {
            case 32:
                if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
                {
                    return D3DFMT_A8R8G8B8;
                }
                if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0))
                {
                    return D3DFMT_X8R8G8B8;
                }
                if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
                {
                    return D3DFMT_A8B8G8R8;
                }
                if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0))
                {
                    return D3DFMT_X8B8G8R8;
                }

                // Note that many common DDS reader/writers (including D3DX) swap the
                // the RED/BLUE masks for 10:10:10:2 formats. We assume
                // below that the 'backwards' header mask is being used since it is most
                // likely written by D3DX.

                // For 'correct' writers this should be 0x3ff00000,0x000ffc00,0x000003ff for BGR data
                if (ISBITMASK(0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000))
                {
                    return D3DFMT_A2R10G10B10;
                }

                // For 'correct' writers this should be 0x000003ff,0x000ffc00,0x3ff00000 for RGB data
                if (ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000))
                {
                    return D3DFMT_A2B10G10R10;
                }

                if (ISBITMASK(0x0000ffff, 0xffff0000, 0x00000000, 0x00000000))
                {
                    return D3DFMT_G16R16;
                }
                if (ISBITMASK(0xffffffff, 0x00000000, 0x00000000, 0x00000000))
                {
                    return D3DFMT_R32F; // D3DX writes this out as a FourCC of 114
                }
                break;

            case 24:
                if (ISBITMASK(0xff0000, 0x00ff00, 0x0000ff, 0))
                {
                    return D3DFMT_R8G8B8;
                }
                break;

            case 16:
                if (ISBITMASK(0xf800, 0x07e0, 0x001f, 0x0000))
                {
                    return D3DFMT_R5G6B5;
                }
                if (ISBITMASK(0x7c00, 0x03e0, 0x001f, 0x8000))
                {
                    return D3DFMT_A1R5G5B5;
                }
                if (ISBITMASK(0x7c00, 0x03e0, 0x001f, 0))
                {
                    return D3DFMT_X1R5G5B5;
                }
                if (ISBITMASK(0x0f00, 0x00f0, 0x000f, 0xf000))
                {
                    return D3DFMT_A4R4G4B4;
                }
                if (ISBITMASK(0x0f00, 0x00f0, 0x000f, 0))
                {
                    return D3DFMT_X4R4G4B4;
                }
                if (ISBITMASK(0x00e0, 0x001c, 0x0003, 0xff00))
                {
                    return D3DFMT_A8R3G3B2;
                }

                // NVTT versions 1.x wrote these as RGB instead of LUMINANCE
                if (ISBITMASK(0xffff, 0, 0, 0))
                {
                    return D3DFMT_L16;
                }
                if (ISBITMASK(0x00ff, 0, 0, 0xff00))
                {
                    return D3DFMT_A8L8;
                }
                break;

            case 8:
                if (ISBITMASK(0xe0, 0x1c, 0x03, 0))
                {
                    return D3DFMT_R3G3B2;
                }

                // NVTT versions 1.x wrote these as RGB instead of LUMINANCE
                if (ISBITMASK(0xff, 0, 0, 0))
                {
                    return D3DFMT_L8;
                }

                // Paletted texture formats are typically not supported on modern video cards aka D3DFMT_P8, D3DFMT_A8P8
                break;

            default:
                return D3DFMT_UNKNOWN;
            }
        }
        else if (ddpf.flags & DDS_LUMINANCE)
        {
            switch (ddpf.RGBBitCount)
            {
            case 16:
                if (ISBITMASK(0xffff, 0, 0, 0))
                {
                    return D3DFMT_L16;
                }
                if (ISBITMASK(0x00ff, 0, 0, 0xff00))
                {
                    return D3DFMT_A8L8;
                }
                break;

            case 8:
                if (ISBITMASK(0x0f, 0, 0, 0xf0))
                {
                    return D3DFMT_A4L4;
                }
                if (ISBITMASK(0xff, 0, 0, 0))
                {
                    return D3DFMT_L8;
                }
                if (ISBITMASK(0x00ff, 0, 0, 0xff00))
                {
                    return D3DFMT_A8L8; // Some DDS writers assume the bitcount should be 8 instead of 16
                }
                break;

            default:
                return D3DFMT_UNKNOWN;
            }
        }
        else if (ddpf.flags & DDS_ALPHA)
        {
            if (8 == ddpf.RGBBitCount)
            {
                return D3DFMT_A8;
            }
        }
        else if (ddpf.flags & DDS_BUMPDUDV)
        {
            switch (ddpf.RGBBitCount)
            {
            case 32:
                if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
                {
                    return D3DFMT_Q8W8V8U8;
                }
                if (ISBITMASK(0x0000ffff, 0xffff0000, 0x00000000, 0x00000000))
                {
                    return D3DFMT_V16U16;
                }
                if (ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000))
                {
                    return D3DFMT_A2W10V10U10;
                }
                break;

            case 16:
                if (ISBITMASK(0x00ff, 0xff00, 0, 0))
                {
                    return D3DFMT_V8U8;
                }
                break;

            default:
                return D3DFMT_UNKNOWN;
            }
        }
        else if (ddpf.flags & DDS_BUMPLUMINANCE)
        {
            switch (ddpf.RGBBitCount)
            {
            case 32:
                if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0))
                {
                    return D3DFMT_X8L8V8U8;
                }
                break;

            case 16:
                if (ISBITMASK(0x001f, 0x03e0, 0xfc00, 0))
                {
                    return D3DFMT_L6V5U5;
                }
                break;

            default:
                return D3DFMT_UNKNOWN;
            }
        }
        else if (ddpf.flags & DDS_FOURCC)
        {
            if (MAKEFOURCC('D', 'X', 'T', '1') == ddpf.fourCC)
            {
                return D3DFMT_DXT1;
            }
            if (MAKEFOURCC('D', 'X', 'T', '2') == ddpf.fourCC)
            {
                return D3DFMT_DXT2;
            }
            if (MAKEFOURCC('D', 'X', 'T', '3') == ddpf.fourCC)
            {
                return D3DFMT_DXT3;
            }
            if (MAKEFOURCC('D', 'X', 'T', '4') == ddpf.fourCC)
            {
                return D3DFMT_DXT4;
            }
            if (MAKEFOURCC('D', 'X', 'T', '5') == ddpf.fourCC)
            {
                return D3DFMT_DXT5;
            }

            if (MAKEFOURCC('R', 'G', 'B', 'G') == ddpf.fourCC)
            {
                return D3DFMT_R8G8_B8G8;
            }
            if (MAKEFOURCC('G', 'R', 'G', 'B') == ddpf.fourCC)
            {
                return D3DFMT_G8R8_G8B8;
            }

            if (MAKEFOURCC('U', 'Y', 'V', 'Y') == ddpf.fourCC)
            {
                return D3DFMT_UYVY;
            }
            if (MAKEFOURCC('Y', 'U', 'Y', '2') == ddpf.fourCC)
            {
                return D3DFMT_YUY2;
            }

            // Check for D3DFORMAT enums being set here
            switch (ddpf.fourCC)
            {
            case D3DFMT_A16B16G16R16:
            case D3DFMT_Q16W16V16U16:
            case D3DFMT_R16F:
            case D3DFMT_G16R16F:
            case D3DFMT_A16B16G16R16F:
            case D3DFMT_R32F:
            case D3DFMT_G32R32F:
            case D3DFMT_A32B32G32R32F:
            case D3DFMT_CxV8U8:
                return static_cast<D3DFORMAT>(ddpf.fourCC);

            default:
                return D3DFMT_UNKNOWN;
            }
        }

        return D3DFMT_UNKNOWN;
    }

#undef ISBITMASK


//--------------------------------------------------------------------------------------
    HRESULT CreateTextureFromDDS(
        _In_ LPDIRECT3DDEVICE9 device,
        _In_ const DDS_HEADER* header,
        _In_reads_bytes_(bitSize) const uint8_t* bitData,
        _In_ size_t bitSize,
        _In_ DWORD usage,
        _In_ D3DPOOL pool,
        _Outptr_ LPDIRECT3DBASETEXTURE9* texture,
        bool generateMipsIfMissing) noexcept
    {
        HRESULT hr = S_OK;

        UINT iWidth = header->width;
        UINT iHeight = header->height;

        UINT iMipCount = header->mipMapCount;
        if (0 == iMipCount)
        {
            iMipCount = 1;
        }

        // Bound sizes (for security purposes we don't trust DDS file metadata larger than the D3D 10 hardware requirements)
        if (iMipCount > 14u /*D3D10_REQ_MIP_LEVELS*/)
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        // We could support a subset of 'DX10' extended header DDS files, but we'll assume here we are only
        // supporting legacy DDS files for a Direct3D9 device

        const D3DFORMAT fmt = GetD3D9Format(header->ddspf);
        if (fmt == D3DFMT_UNKNOWN || BitsPerPixel(fmt) == 0)
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        if (header->flags & DDS_HEADER_FLAGS_VOLUME)
        {
            UINT iDepth = header->depth;

            if ((iWidth > 2048u /*D3D10_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/)
                || (iHeight > 2048u /*D3D10_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/)
                || (iDepth > 2048u /*D3D10_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/))
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }

            // Create the volume texture (let the runtime do the validation)
            ComPtr<IDirect3DVolumeTexture9> pTexture;
            hr = device->CreateVolumeTexture(iWidth, iHeight, iDepth, iMipCount,
                usage, fmt, pool, pTexture.GetAddressOf(), nullptr);
            if (FAILED(hr))
                return hr;

            ComPtr<IDirect3DVolumeTexture9> pStagingTexture;
            if (pool == D3DPOOL_DEFAULT)
            {
                hr = device->CreateVolumeTexture(iWidth, iHeight, iDepth, iMipCount,
                    0u, fmt, D3DPOOL_SYSTEMMEM, pStagingTexture.GetAddressOf(), nullptr);
                if (FAILED(hr))
                    return hr;
            }
            else
            {
                pStagingTexture = pTexture;
            }

            // Lock, fill, unlock
            size_t NumBytes = 0;
            size_t RowBytes = 0;
            size_t NumRows = 0;
            const uint8_t* pSrcBits = bitData;
            const uint8_t* pEndBits = bitData + bitSize;
            D3DLOCKED_BOX LockedBox = {};

            for (UINT i = 0; i < iMipCount; ++i)
            {
                GetSurfaceInfo(iWidth, iHeight, fmt, &NumBytes, &RowBytes, &NumRows);

                if (NumBytes > UINT32_MAX || RowBytes > UINT32_MAX)
                    return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

                if ((pSrcBits + (NumBytes * iDepth)) > pEndBits)
                {
                    return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                }

                if (SUCCEEDED(pStagingTexture->LockBox(i, &LockedBox, nullptr, 0)))
                {
                    auto pDestBits = static_cast<uint8_t*>(LockedBox.pBits);

                    for (UINT j = 0; j < iDepth; ++j)
                    {
                        uint8_t* dptr = pDestBits;
                        const uint8_t* sptr = pSrcBits;

                        // Copy stride line by line
                        for (size_t h = 0; h < NumRows; h++)
                        {
                            memcpy_s(dptr, static_cast<size_t>(LockedBox.RowPitch), sptr, RowBytes);
                            dptr += LockedBox.RowPitch;
                            sptr += RowBytes;
                        }

                        pDestBits += LockedBox.SlicePitch;
                        pSrcBits += NumBytes;
                    }

                    pStagingTexture->UnlockBox(i);
                }

                iWidth = iWidth >> 1;
                iHeight = iHeight >> 1;
                iDepth = iDepth >> 1;
                if (iWidth == 0)
                    iWidth = 1;
                if (iHeight == 0)
                    iHeight = 1;
                if (iDepth == 0)
                    iDepth = 1;
            }

            if (pool == D3DPOOL_DEFAULT)
            {
                hr = device->UpdateTexture(pStagingTexture.Get(), pTexture.Get());
                if (FAILED(hr))
                    return hr;
            }

            *texture = pTexture.Detach();
        }
        else if (header->caps2 & DDS_CUBEMAP)
        {
            if ((iWidth > 8192u /*D3D10_REQ_TEXTURECUBE_DIMENSION*/)
                || (iHeight > 8192u /*D3D10_REQ_TEXTURECUBE_DIMENSION*/))
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }

            // We require at least one face to be defined, and the faces must be square
            if ((header->caps2 & DDS_CUBEMAP_ALLFACES) == 0 || iHeight != iWidth)
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }

            // Create the cubemap (let the runtime do the validation)
            ComPtr<IDirect3DCubeTexture9> pTexture;
            hr = device->CreateCubeTexture(iWidth, iMipCount,
                usage, fmt, pool, pTexture.GetAddressOf(), nullptr);
            if (FAILED(hr))
                return hr;

            ComPtr<IDirect3DCubeTexture9> pStagingTexture;
            if (pool == D3DPOOL_DEFAULT)
            {
                hr = device->CreateCubeTexture(iWidth, iMipCount,
                    0u, fmt, D3DPOOL_SYSTEMMEM, pStagingTexture.GetAddressOf(), nullptr);
                if (FAILED(hr))
                    return hr;
            }
            else
            {
                pStagingTexture = pTexture;
            }

            // Lock, fill, unlock
            size_t NumBytes = 0;
            size_t RowBytes = 0;
            size_t NumRows = 0;
            const uint8_t* pSrcBits = bitData;
            const uint8_t* pEndBits = bitData + bitSize;
            D3DLOCKED_RECT LockedRect = {};

            UINT mask = DDS_CUBEMAP_POSITIVEX & ~DDS_CUBEMAP;
            for (UINT f = 0; f < 6; ++f, mask <<= 1)
            {
                if (!(header->caps2 & mask))
                    continue;

                UINT w = iWidth;
                UINT h = iHeight;
                for (UINT i = 0; i < iMipCount; ++i)
                {
                    GetSurfaceInfo(w, h, fmt, &NumBytes, &RowBytes, &NumRows);

                    if (NumBytes > UINT32_MAX || RowBytes > UINT32_MAX)
                        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

                    if ((pSrcBits + NumBytes) > pEndBits)
                    {
                        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                    }

                    if (SUCCEEDED(pStagingTexture->LockRect(static_cast<D3DCUBEMAP_FACES>(f), i, &LockedRect, nullptr, 0)))
                    {
                        auto pDestBits = static_cast<uint8_t*>(LockedRect.pBits);

                        // Copy stride line by line
                        for (size_t r = 0; r < NumRows; r++)
                        {
                            memcpy_s(pDestBits, static_cast<size_t>(LockedRect.Pitch), pSrcBits, RowBytes);
                            pDestBits += LockedRect.Pitch;
                            pSrcBits += RowBytes;
                        }

                        pStagingTexture->UnlockRect(static_cast<D3DCUBEMAP_FACES>(f), i);
                    }

                    w = w >> 1;
                    h = h >> 1;
                    if (w == 0)
                        w = 1;
                    if (h == 0)
                        h = 1;
                }
            }

            if (pool == D3DPOOL_DEFAULT)
            {
                hr = device->UpdateTexture(pStagingTexture.Get(), pTexture.Get());
                if (FAILED(hr))
                    return hr;
            }

            *texture = pTexture.Detach();
        }
        else
        {
            if ((iWidth > 8192u /*D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION*/)
                || (iHeight > 8192u /*D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION*/))
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }

            // Create the texture (let the runtime do the validation)
            if (generateMipsIfMissing)
                usage |= D3DUSAGE_AUTOGENMIPMAP;

            ComPtr<IDirect3DTexture9> pTexture;
            hr = device->CreateTexture(iWidth, iHeight, iMipCount,
                usage, fmt, pool,
                pTexture.GetAddressOf(), nullptr);
            if (FAILED(hr))
                return hr;

            ComPtr<IDirect3DTexture9> pStagingTexture;
            if (pool == D3DPOOL_DEFAULT)
            {
                hr = device->CreateTexture(iWidth, iHeight, iMipCount,
                    0u, fmt, D3DPOOL_SYSTEMMEM, pStagingTexture.GetAddressOf(), nullptr);
                if (FAILED(hr))
                    return hr;
            }
            else
            {
                pStagingTexture = pTexture;
            }

            // Lock, fill, unlock
            size_t NumBytes = 0;
            size_t RowBytes = 0;
            size_t NumRows = 0;
            const uint8_t* pSrcBits = bitData;
            const uint8_t* pEndBits = bitData + bitSize;
            D3DLOCKED_RECT LockedRect = {};

            for (UINT i = 0; i < iMipCount; ++i)
            {
                GetSurfaceInfo(iWidth, iHeight, fmt, &NumBytes, &RowBytes, &NumRows);

                if (NumBytes > UINT32_MAX || RowBytes > UINT32_MAX)
                    return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

                if ((pSrcBits + NumBytes) > pEndBits)
                {
                    return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                }

                if (SUCCEEDED(pStagingTexture->LockRect(i, &LockedRect, nullptr, 0)))
                {
                    auto pDestBits = static_cast<uint8_t*>(LockedRect.pBits);

                    // Copy stride line by line
                    for (UINT h = 0; h < NumRows; h++)
                    {
                        memcpy_s(pDestBits, static_cast<size_t>(LockedRect.Pitch), pSrcBits, RowBytes);
                        pDestBits += LockedRect.Pitch;
                        pSrcBits += RowBytes;
                    }

                    pStagingTexture->UnlockRect(i);
                }

                iWidth = iWidth >> 1;
                iHeight = iHeight >> 1;
                if (iWidth == 0)
                    iWidth = 1;
                if (iHeight == 0)
                    iHeight = 1;
            }

            if (pool == D3DPOOL_DEFAULT)
            {
                hr = device->UpdateTexture(pStagingTexture.Get(), pTexture.Get());
                if (FAILED(hr))
                    return hr;
            }

            *texture = pTexture.Detach();
        }

        return hr;
    }
} // anonymous namespace

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemory(
    LPDIRECT3DDEVICE9 d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    LPDIRECT3DBASETEXTURE9* texture,
    bool generateMipsIfMissing) noexcept
{
    return CreateDDSTextureFromMemoryEx(d3dDevice, ddsData, ddsDataSize, 0u, D3DPOOL_DEFAULT, generateMipsIfMissing, texture);
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemoryEx(
    LPDIRECT3DDEVICE9 d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    _In_ DWORD usage,
    _In_ D3DPOOL pool,
    bool generateMipsIfMissing,
    LPDIRECT3DBASETEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !ddsData || !texture)
    {
        return E_INVALIDARG;
    }

    // Validate DDS file in memory
    const DDS_HEADER* header = nullptr;
    const uint8_t* bitData = nullptr;
    size_t bitSize = 0;

    HRESULT hr = LoadTextureDataFromMemory(ddsData, ddsDataSize,
        &header,
        &bitData,
        &bitSize
    );
    if (FAILED(hr))
        return hr;

    return CreateTextureFromDDS(
        d3dDevice,
        header,
        bitData,
        bitSize,
        usage,
        pool,
        texture,
        generateMipsIfMissing);
}

// Type-specific standard versions
_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemory(
    LPDIRECT3DDEVICE9 d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    LPDIRECT3DTEXTURE9* texture,
    bool generateMipsIfMissing) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !ddsData || !ddsDataSize || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromMemoryEx(d3dDevice, ddsData, ddsDataSize, 0u, D3DPOOL_DEFAULT, generateMipsIfMissing, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_TEXTURE)
        {
            *texture = static_cast<LPDIRECT3DTEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemory(
    LPDIRECT3DDEVICE9 d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    LPDIRECT3DCUBETEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !ddsData || !ddsDataSize || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromMemoryEx(d3dDevice, ddsData, ddsDataSize, 0u, D3DPOOL_DEFAULT, false, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_CUBETEXTURE)
        {
            *texture = static_cast<LPDIRECT3DCUBETEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemory(
    LPDIRECT3DDEVICE9 d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    LPDIRECT3DVOLUMETEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !ddsData || !ddsDataSize || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromMemoryEx(d3dDevice, ddsData, ddsDataSize, 0u, D3DPOOL_DEFAULT, false, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_VOLUMETEXTURE)
        {
            *texture = static_cast<LPDIRECT3DVOLUMETEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}

// Type-specific extended versions
_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemoryEx(
    LPDIRECT3DDEVICE9 d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    DWORD usage,
    D3DPOOL pool,
    bool generateMipsIfMissing,
    LPDIRECT3DTEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !ddsData || !ddsDataSize || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromMemoryEx(d3dDevice, ddsData, ddsDataSize, usage, pool, generateMipsIfMissing, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_TEXTURE)
        {
            *texture = static_cast<LPDIRECT3DTEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemoryEx(
    LPDIRECT3DDEVICE9 d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    DWORD usage,
    D3DPOOL pool,
    LPDIRECT3DCUBETEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !ddsData || !ddsDataSize || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromMemoryEx(d3dDevice, ddsData, ddsDataSize, usage, pool, false, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_CUBETEXTURE)
        {
            *texture = static_cast<LPDIRECT3DCUBETEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemoryEx(
    LPDIRECT3DDEVICE9 d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    DWORD usage,
    D3DPOOL pool,
    LPDIRECT3DVOLUMETEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !ddsData || !ddsDataSize || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromMemoryEx(d3dDevice, ddsData, ddsDataSize, usage, pool, false, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_VOLUMETEXTURE)
        {
            *texture = static_cast<LPDIRECT3DVOLUMETEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFile(
    LPDIRECT3DDEVICE9 d3dDevice,
    const wchar_t* fileName,
    LPDIRECT3DBASETEXTURE9* texture,
    bool generateMipsIfMissing) noexcept
{
    return CreateDDSTextureFromFileEx(d3dDevice, fileName, 0u, D3DPOOL_DEFAULT, generateMipsIfMissing, texture);
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFileEx(
    LPDIRECT3DDEVICE9 d3dDevice,
    const wchar_t* fileName,
    _In_ DWORD usage,
    _In_ D3DPOOL pool,
    bool generateMipsIfMissing,
    LPDIRECT3DBASETEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !fileName || !texture)
        return E_INVALIDARG;

    const DDS_HEADER* header = nullptr;
    const uint8_t* bitData = nullptr;
    size_t bitSize = 0;

    std::unique_ptr<uint8_t[]> ddsData;
    HRESULT hr = LoadTextureDataFromFile(fileName,
        ddsData,
        &header,
        &bitData,
        &bitSize
    );
    if (FAILED(hr))
    {
        return hr;
    }

    return CreateTextureFromDDS(
        d3dDevice,
        header,
        bitData,
        bitSize,
        usage,
        pool,
        texture,
        generateMipsIfMissing);
}

// Type-specific standard versions
_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFile(
    LPDIRECT3DDEVICE9 d3dDevice,
    const wchar_t* fileName,
    LPDIRECT3DTEXTURE9* texture,
    bool generateMipsIfMissing) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !fileName || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromFileEx(d3dDevice, fileName, 0u, D3DPOOL_DEFAULT, generateMipsIfMissing, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_TEXTURE)
        {
            *texture = static_cast<LPDIRECT3DTEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFile(
    LPDIRECT3DDEVICE9 d3dDevice,
    const wchar_t* fileName,
    LPDIRECT3DCUBETEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !fileName || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromFileEx(d3dDevice, fileName, 0u, D3DPOOL_DEFAULT, false, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_CUBETEXTURE)
        {
            *texture = static_cast<LPDIRECT3DCUBETEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFile(
    LPDIRECT3DDEVICE9 d3dDevice,
    const wchar_t* szFileName,
    LPDIRECT3DVOLUMETEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !szFileName || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromFileEx(d3dDevice, szFileName, 0u, D3DPOOL_DEFAULT, false, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_VOLUMETEXTURE)
        {
            *texture = static_cast<LPDIRECT3DVOLUMETEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}

// Type-specific extended versions
_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFileEx(
    LPDIRECT3DDEVICE9 d3dDevice,
    const wchar_t* fileName,
    DWORD usage,
    D3DPOOL pool,
    bool generateMipsIfMissing,
    LPDIRECT3DTEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !fileName || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromFileEx(d3dDevice, fileName, usage, pool, generateMipsIfMissing, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_TEXTURE)
        {
            *texture = static_cast<LPDIRECT3DTEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFileEx(
    LPDIRECT3DDEVICE9 d3dDevice,
    const wchar_t* fileName,
    DWORD usage,
    D3DPOOL pool,
    LPDIRECT3DCUBETEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !fileName || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromFileEx(d3dDevice, fileName, usage, pool, false, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_CUBETEXTURE)
        {
            *texture = static_cast<LPDIRECT3DCUBETEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFileEx(
    LPDIRECT3DDEVICE9 d3dDevice,
    const wchar_t* szFileName,
    DWORD usage,
    D3DPOOL pool,
    LPDIRECT3DVOLUMETEXTURE9* texture) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }

    if (!d3dDevice || !szFileName || !texture)
        return E_INVALIDARG;

    ComPtr<IDirect3DBaseTexture9> tex;
    HRESULT hr = CreateDDSTextureFromFileEx(d3dDevice, szFileName, usage, pool, false, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = E_FAIL;
        if (tex->GetType() == D3DRTYPE_VOLUMETEXTURE)
        {
            *texture = static_cast<LPDIRECT3DVOLUMETEXTURE9>(tex.Detach());
            return S_OK;
        }
    }

    return hr;
}
