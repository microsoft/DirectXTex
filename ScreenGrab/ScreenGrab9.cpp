//--------------------------------------------------------------------------------------
// File: ScreenGrab9.cpp
//
// Function for saving 2D surface to a file (aka a 'screenshot'
// when used on a Direct3D 9's GetFrontBufferData).
//
// Note these functions are useful as a light-weight runtime screen grabber. For
// full-featured texture capture, DDS writer, and texture processing pipeline,
// see the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "ScreenGrab9.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <tuple>

#include <wincodec.h>

#include <wrl\client.h>

#ifdef _MSC_VER
// Off by default warnings
#pragma warning(disable : 4619 4616 4061 4062 4623 4626 5027)
// C4619/4616 #pragma warning warnings
// C4061 enumerator 'x' in switch of enum 'y' is not explicitly handled by a case label
// C4062 enumerator 'x' in switch of enum 'y' is not handled
// C4623 default constructor was implicitly defined as deleted
// C4626 assignment operator was implicitly defined as deleted
// C5027 move assignment operator was implicitly defined as deleted
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

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
namespace
{
#pragma pack(push,1)

#define DDS_MAGIC 0x20534444 // "DDS "

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

#define DDS_FOURCC        0x00000004  // DDPF_FOURCC
#define DDS_RGB           0x00000040  // DDPF_RGB
#define DDS_RGBA          0x00000041  // DDPF_RGB | DDPF_ALPHAPIXELS
#define DDS_LUMINANCE     0x00020000  // DDPF_LUMINANCE
#define DDS_LUMINANCEA    0x00020001  // DDPF_LUMINANCE | DDPF_ALPHAPIXELS
#define DDS_ALPHA         0x00000002  // DDPF_ALPHA
#define DDS_BUMPDUDV      0x00080000  // DDPF_BUMPDUDV
#define DDS_BUMPLUMINANCE 0x00040000  // DDPF_BUMPLUMINANCE

#define DDS_HEADER_FLAGS_TEXTURE        0x00001007  // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT
#define DDS_HEADER_FLAGS_MIPMAP         0x00020000  // DDSD_MIPMAPCOUNT
#define DDS_HEADER_FLAGS_PITCH          0x00000008  // DDSD_PITCH
#define DDS_HEADER_FLAGS_LINEARSIZE     0x00080000  // DDSD_LINEARSIZE

#define DDS_SURFACE_FLAGS_TEXTURE 0x00001000 // DDSCAPS_TEXTURE

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

    const DDS_PIXELFORMAT DDSPF_DXT1 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','1'), 0, 0, 0, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_DXT2 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','2'), 0, 0, 0, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_DXT3 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','3'), 0, 0, 0, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_DXT4 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','4'), 0, 0, 0, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_DXT5 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','5'), 0, 0, 0, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_R8G8_B8G8 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R','G','B','G'), 0, 0, 0, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_G8R8_G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('G','R','G','B'), 0, 0, 0, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_YUY2 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('Y','U','Y','2'), 0, 0, 0, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_UYVY =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('U','Y','V','Y'), 0, 0, 0, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_A8R8G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };

    const DDS_PIXELFORMAT DDSPF_X8R8G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB,  0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0 };

    const DDS_PIXELFORMAT DDSPF_A8B8G8R8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };

    const DDS_PIXELFORMAT DDSPF_X8B8G8R8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0 };

    const DDS_PIXELFORMAT DDSPF_G16R16 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB,  0, 32, 0x0000ffff, 0xffff0000, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_R5G6B5 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0 };

    const DDS_PIXELFORMAT DDSPF_A1R5G5B5 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x7c00, 0x03e0, 0x001f, 0x8000 };

    const DDS_PIXELFORMAT DDSPF_X1R5G5B5 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x7c00, 0x03e0, 0x001f, 0 };

    const DDS_PIXELFORMAT DDSPF_A4R4G4B4 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x0f00, 0x00f0, 0x000f, 0xf000 };

    const DDS_PIXELFORMAT DDSPF_X4R4G4B4 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x0f00, 0x00f0, 0x000f, 0 };

    const DDS_PIXELFORMAT DDSPF_R8G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 24, 0xff0000, 0x00ff00, 0x0000ff, 0 };

    const DDS_PIXELFORMAT DDSPF_A8R3G3B2 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00e0, 0x001c, 0x0003, 0xff00 };

    const DDS_PIXELFORMAT DDSPF_R3G3B2 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 8, 0xe0, 0x1c, 0x03, 0 };

    const DDS_PIXELFORMAT DDSPF_A4L4 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 8, 0x0f, 0, 0, 0xf0 };

    const DDS_PIXELFORMAT DDSPF_L8 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE, 0,  8, 0xff, 0, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_L16 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE, 0, 16, 0xffff, 0, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_A8L8 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 16, 0x00ff, 0, 0, 0xff00 };

    const DDS_PIXELFORMAT DDSPF_A8 =
    { sizeof(DDS_PIXELFORMAT), DDS_ALPHA, 0, 8, 0, 0, 0, 0xff };

    const DDS_PIXELFORMAT DDSPF_V8U8 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 16, 0x00ff, 0xff00, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_Q8W8V8U8 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };

    const DDS_PIXELFORMAT DDSPF_V16U16 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 32, 0x0000ffff, 0xffff0000, 0, 0 };

    const DDS_PIXELFORMAT DDSPF_A2W10V10U10 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 };

    const DDS_PIXELFORMAT DDSPF_L6V5U5 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPLUMINANCE, 0, 16, 0x001f, 0x03e0, 0xfc00, 0 };

    const DDS_PIXELFORMAT DDSPF_X8L8V8U8 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPLUMINANCE, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0 };

    // Note these 10:10:10:2 format RGB masks are reversed to support a long-standing bug in D3DX
    const DDS_PIXELFORMAT DDSPF_A2R10G10B10 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000 };
    const DDS_PIXELFORMAT DDSPF_A2B10G10R10 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 };

    //-----------------------------------------------------------------------------
    struct handle_closer { void operator()(HANDLE h) noexcept { if (h) CloseHandle(h); } };

    using ScopedHandle = std::unique_ptr<void, handle_closer>;

    inline HANDLE safe_handle(HANDLE h) noexcept { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

    class auto_delete_file
    {
    public:
        auto_delete_file(HANDLE hFile) noexcept : m_handle(hFile) {}
        ~auto_delete_file()
        {
            if (m_handle)
            {
                FILE_DISPOSITION_INFO info = {};
                info.DeleteFile = TRUE;
                std::ignore = SetFileInformationByHandle(m_handle, FileDispositionInfo, &info, sizeof(info));
            }
        }

        auto_delete_file(const auto_delete_file&) = delete;
        auto_delete_file& operator=(const auto_delete_file&) = delete;

        auto_delete_file(const auto_delete_file&&) = delete;
        auto_delete_file& operator=(const auto_delete_file&&) = delete;

        void clear() noexcept { m_handle = nullptr; }

    private:
        HANDLE m_handle;
    };

    class auto_delete_file_wic
    {
    public:
        auto_delete_file_wic(ComPtr<IWICStream>& hFile, const wchar_t* szFile) noexcept : m_filename(szFile), m_handle(hFile) {}
        ~auto_delete_file_wic()
        {
            if (m_filename)
            {
                m_handle.Reset();
                DeleteFileW(m_filename);
            }
        }

        auto_delete_file_wic(const auto_delete_file_wic&) = delete;
        auto_delete_file_wic& operator=(const auto_delete_file_wic&) = delete;

        auto_delete_file_wic(const auto_delete_file_wic&&) = delete;
        auto_delete_file_wic& operator=(const auto_delete_file_wic&&) = delete;

        void clear() noexcept { m_filename = nullptr; }

    private:
        const wchar_t* m_filename;
        ComPtr<IWICStream>& m_handle;
    };


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
        #if !defined(D3D_DISABLE_9EX)
        case D3DFMT_S8_LOCKABLE:
        #endif
            return 8;

        case D3DFMT_DXT1:
            return 4;

        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/directshow/htm/directxvideoaccelerationdxvavideosubtypes.asp
        case MAKEFOURCC('A', 'I', '4', '4'):
        case MAKEFOURCC('I', 'A', '4', '4'):
            return 8;

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
    // Determines if the format is block compressed
    //--------------------------------------------------------------------------------------
    bool IsCompressed(_In_ D3DFORMAT fmt) noexcept
    {
        switch (fmt)
        {
        case D3DFMT_DXT1:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            return true;

        default:
            return false;
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
    bool g_WIC2 = false;

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

    IWICImagingFactory* GetWIC()
    {
        static INIT_ONCE s_initOnce = INIT_ONCE_STATIC_INIT;

        IWICImagingFactory* factory = nullptr;
        if (!InitOnceExecuteOnce(&s_initOnce,
            InitializeWICFactory,
            nullptr,
            reinterpret_cast<LPVOID*>(&factory)))
        {
            return nullptr;
        }

        return factory;
    }
} // anonymous namespace


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::SaveDDSTextureToFile(
    LPDIRECT3DSURFACE9 pSource,
    const wchar_t* fileName) noexcept
{
    if (!pSource || !fileName)
        return E_INVALIDARG;

    D3DSURFACE_DESC desc = {};
    HRESULT hr = pSource->GetDesc(&desc);
    if (FAILED(hr))
        return hr;

    if (desc.Type != D3DRTYPE_SURFACE && desc.Type != D3DRTYPE_TEXTURE)
    {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    // Create file
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(fileName,
        GENERIC_WRITE | DELETE, 0, CREATE_ALWAYS, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(fileName,
        GENERIC_WRITE | DELETE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)));
#endif
    if (!hFile)
        return HRESULT_FROM_WIN32(GetLastError());

    auto_delete_file delonfail(hFile.get());

    // Setup header
    constexpr size_t MAX_HEADER_SIZE = sizeof(uint32_t) + sizeof(DDS_HEADER);
    uint8_t fileHeader[MAX_HEADER_SIZE] = {};

    *reinterpret_cast<uint32_t*>(&fileHeader[0]) = DDS_MAGIC;

    auto header = reinterpret_cast<DDS_HEADER*>(&fileHeader[0] + sizeof(uint32_t));
    constexpr size_t headerSize = sizeof(uint32_t) + sizeof(DDS_HEADER);
    header->size = sizeof(DDS_HEADER);
    header->flags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
    header->height = desc.Height;
    header->width = desc.Width;
    header->mipMapCount = 1;
    header->caps = DDS_SURFACE_FLAGS_TEXTURE;

    switch (desc.Format)
    {
    case D3DFMT_R8G8B8:          memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_R8G8B8, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_A8R8G8B8:        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_A8R8G8B8, sizeof(DDS_PIXELFORMAT));    break;
    case D3DFMT_X8R8G8B8:        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_X8R8G8B8, sizeof(DDS_PIXELFORMAT));    break;
    case D3DFMT_R5G6B5:          memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_R5G6B5, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_X1R5G5B5:        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_X1R5G5B5, sizeof(DDS_PIXELFORMAT));    break;
    case D3DFMT_A1R5G5B5:        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_A1R5G5B5, sizeof(DDS_PIXELFORMAT));    break;
    case D3DFMT_A4R4G4B4:        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_A4R4G4B4, sizeof(DDS_PIXELFORMAT));    break;
    case D3DFMT_R3G3B2:          memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_R3G3B2, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_A8:              memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_A8, sizeof(DDS_PIXELFORMAT));          break;
    case D3DFMT_A8R3G3B2:        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_A8R3G3B2, sizeof(DDS_PIXELFORMAT));    break;
    case D3DFMT_X4R4G4B4:        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_X4R4G4B4, sizeof(DDS_PIXELFORMAT));    break;
    case D3DFMT_A2B10G10R10:     memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_A2B10G10R10, sizeof(DDS_PIXELFORMAT)); break;
    case D3DFMT_A8B8G8R8:        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_A8B8G8R8, sizeof(DDS_PIXELFORMAT));    break;
    case D3DFMT_X8B8G8R8:        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_X8B8G8R8, sizeof(DDS_PIXELFORMAT));    break;
    case D3DFMT_G16R16:          memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_G16R16, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_A2R10G10B10:     memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_A2R10G10B10, sizeof(DDS_PIXELFORMAT)); break;
    case D3DFMT_L8:              memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_L8, sizeof(DDS_PIXELFORMAT));          break;
    case D3DFMT_A8L8:            memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_A8L8, sizeof(DDS_PIXELFORMAT));        break;
    case D3DFMT_A4L4:            memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_A4L4, sizeof(DDS_PIXELFORMAT));        break;
    case D3DFMT_V8U8:            memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_V8U8, sizeof(DDS_PIXELFORMAT));        break;
    case D3DFMT_L6V5U5:          memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_L6V5U5, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_X8L8V8U8:        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_X8L8V8U8, sizeof(DDS_PIXELFORMAT));    break;
    case D3DFMT_Q8W8V8U8:        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_Q8W8V8U8, sizeof(DDS_PIXELFORMAT));    break;
    case D3DFMT_V16U16:          memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_V16U16, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_A2W10V10U10:     memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_A2W10V10U10, sizeof(DDS_PIXELFORMAT)); break;
    case D3DFMT_L16:             memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_L16, sizeof(DDS_PIXELFORMAT));         break;

        // FourCC formats
    case D3DFMT_UYVY:            memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_UYVY, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_R8G8_B8G8:       memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_R8G8_B8G8, sizeof(DDS_PIXELFORMAT)); break;
    case D3DFMT_YUY2:            memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_YUY2, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_G8R8_G8B8:       memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_G8R8_G8B8, sizeof(DDS_PIXELFORMAT)); break;
    case D3DFMT_DXT1:            memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_DXT1, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_DXT2:            memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_DXT2, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_DXT3:            memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_DXT3, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_DXT4:            memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_DXT4, sizeof(DDS_PIXELFORMAT));      break;
    case D3DFMT_DXT5:            memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_DXT5, sizeof(DDS_PIXELFORMAT));      break;

        // Legacy D3DX formats using D3DFMT enum value as FourCC
    case D3DFMT_A16B16G16R16:     header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 36;  break;
    case D3DFMT_Q16W16V16U16:     header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 110; break;
    case D3DFMT_R16F:             header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 111; break;
    case D3DFMT_G16R16F:          header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 112; break;
    case D3DFMT_A16B16G16R16F:    header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 113; break;
    case D3DFMT_R32F:             header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 114; break;
    case D3DFMT_G32R32F:          header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 115; break;
    case D3DFMT_A32B32G32R32F:    header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 116; break;
    case D3DFMT_CxV8U8:           header->ddspf.size = sizeof(DDS_PIXELFORMAT); header->ddspf.flags = DDS_FOURCC; header->ddspf.fourCC = 117; break;

    default:
        // No support for paletted formats D3DFMT_P8, D3DFMT_A8P8
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    size_t rowPitch, slicePitch, rowCount;
    hr = GetSurfaceInfo(desc.Width, desc.Height, desc.Format, &slicePitch, &rowPitch, &rowCount);
    if (FAILED(hr))
        return hr;

    if (rowPitch > UINT32_MAX || slicePitch > UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

    if (IsCompressed(desc.Format))
    {
        header->flags |= DDS_HEADER_FLAGS_LINEARSIZE;
        header->pitchOrLinearSize = static_cast<uint32_t>(slicePitch);
    }
    else
    {
        header->flags |= DDS_HEADER_FLAGS_PITCH;
        header->pitchOrLinearSize = static_cast<uint32_t>(rowPitch);
    }

    // Setup pixels
    std::unique_ptr<uint8_t[]> pixels(new (std::nothrow) uint8_t[slicePitch]);
    if (!pixels)
        return E_OUTOFMEMORY;

    D3DLOCKED_RECT lockedRect = {};
    hr = pSource->LockRect(&lockedRect, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr))
        return hr;

    auto sptr = static_cast<const uint8_t*>(lockedRect.pBits);
    if (!sptr)
    {
        pSource->UnlockRect();
        return E_POINTER;
    }

    uint8_t* dptr = pixels.get();

    const size_t msize = std::min<size_t>(rowPitch, static_cast<size_t>(lockedRect.Pitch));
    for (size_t h = 0; h < rowCount; ++h)
    {
        memcpy_s(dptr, rowPitch, sptr, msize);
        sptr += lockedRect.Pitch;
        dptr += rowPitch;
    }

    pSource->UnlockRect();

    // Write header & pixels
    DWORD bytesWritten;
    if (!WriteFile(hFile.get(), fileHeader, static_cast<DWORD>(headerSize), &bytesWritten, nullptr))
        return HRESULT_FROM_WIN32(GetLastError());

    if (bytesWritten != headerSize)
        return E_FAIL;

    if (!WriteFile(hFile.get(), pixels.get(), static_cast<DWORD>(slicePitch), &bytesWritten, nullptr))
        return HRESULT_FROM_WIN32(GetLastError());

    if (bytesWritten != slicePitch)
        return E_FAIL;

    delonfail.clear();

    return S_OK;
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::SaveWICTextureToFile(
    LPDIRECT3DSURFACE9 pSource,
    REFGUID guidContainerFormat,
    const wchar_t* fileName,
    const GUID* targetFormat,
    std::function<void(IPropertyBag2*)> setCustomProps)
{
    if (!pSource || !fileName)
        return E_INVALIDARG;

    D3DSURFACE_DESC desc = {};
    HRESULT hr = pSource->GetDesc(&desc);
    if (FAILED(hr))
        return hr;

    if (desc.Type != D3DRTYPE_SURFACE && desc.Type != D3DRTYPE_TEXTURE)
    {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    auto pWIC = GetWIC();
    if (!pWIC)
        return E_NOINTERFACE;

    // Determine source format's WIC equivalent
    WICPixelFormatGUID pfGuid = {};
    switch (desc.Format)
    {
    case D3DFMT_R8G8B8:         pfGuid = GUID_WICPixelFormat24bppBGR; break;
    case D3DFMT_A8R8G8B8:       pfGuid = GUID_WICPixelFormat32bppBGRA; break;
    case D3DFMT_X8R8G8B8:       pfGuid = GUID_WICPixelFormat32bppBGR; break;
    case D3DFMT_R5G6B5:         pfGuid = GUID_WICPixelFormat16bppBGR565; break;
    case D3DFMT_X1R5G5B5:       pfGuid = GUID_WICPixelFormat16bppBGR555; break;
    case D3DFMT_A1R5G5B5:       pfGuid = GUID_WICPixelFormat16bppBGRA5551; break;
    case D3DFMT_A8:             pfGuid = GUID_WICPixelFormat8bppAlpha; break;
    case D3DFMT_A2B10G10R10:    pfGuid = GUID_WICPixelFormat32bppRGBA1010102; break;
    case D3DFMT_A8B8G8R8:       pfGuid = GUID_WICPixelFormat32bppRGBA; break;
    case D3DFMT_A16B16G16R16:   pfGuid = GUID_WICPixelFormat64bppRGBA; break;
    case D3DFMT_L8:             pfGuid = GUID_WICPixelFormat8bppGray; break;
    case D3DFMT_L16:            pfGuid = GUID_WICPixelFormat16bppGray; break;
    case D3DFMT_R16F:           pfGuid = GUID_WICPixelFormat16bppGrayHalf; break;
    case D3DFMT_A16B16G16R16F:  pfGuid = GUID_WICPixelFormat64bppRGBAHalf; break;
    case D3DFMT_R32F:           pfGuid = GUID_WICPixelFormat32bppGrayFloat; break;
    case D3DFMT_A32B32G32R32F:  pfGuid = GUID_WICPixelFormat128bppRGBAFloat; break;

    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
    case D3DFMT_X8B8G8R8:
        if (g_WIC2)
            pfGuid = GUID_WICPixelFormat32bppRGB;
        else
            HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        break;
    #endif

    default:
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    ComPtr<IWICStream> stream;
    hr = pWIC->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromFilename(fileName, GENERIC_WRITE);
    if (FAILED(hr))
        return hr;

    auto_delete_file_wic delonfail(stream, fileName);

    ComPtr<IWICBitmapEncoder> encoder;
    hr = pWIC->CreateEncoder(guidContainerFormat, nullptr, encoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(frame.GetAddressOf(), props.GetAddressOf());
    if (FAILED(hr))
        return hr;

    if (targetFormat && memcmp(&guidContainerFormat, &GUID_ContainerFormatBmp, sizeof(WICPixelFormatGUID)) == 0 && g_WIC2)
    {
        // Opt-in to the WIC2 support for writing 32-bit Windows BMP files with an alpha channel
        PROPBAG2 option = {};
        option.pstrName = const_cast<wchar_t*>(L"EnableV5Header32bppBGRA");

        VARIANT varValue;
        varValue.vt = VT_BOOL;
        varValue.boolVal = VARIANT_TRUE;
        std::ignore = props->Write(1, &option, &varValue);
    }

    if (setCustomProps)
    {
        setCustomProps(props.Get());
    }

    hr = frame->Initialize(props.Get());
    if (FAILED(hr))
        return hr;

    hr = frame->SetSize(desc.Width, desc.Height);
    if (FAILED(hr))
        return hr;

    hr = frame->SetResolution(72, 72);
    if (FAILED(hr))
        return hr;

    // Pick a target format
    WICPixelFormatGUID targetGuid = {};
    if (targetFormat)
    {
        targetGuid = *targetFormat;
    }
    else
    {
        // Screenshots don't typically include the alpha channel of the render target
        switch (desc.Format)
        {
        #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        case D3DFMT_A32B32G32R32F:
        case D3DFMT_A16B16G16R16F:
            if (g_WIC2)
            {
                targetGuid = GUID_WICPixelFormat96bppRGBFloat;
            }
            else
            {
                targetGuid = GUID_WICPixelFormat24bppBGR;
            }
            break;
        #endif

        case D3DFMT_A16B16G16R16: targetGuid = GUID_WICPixelFormat48bppBGR; break;
        case D3DFMT_R5G6B5:       targetGuid = GUID_WICPixelFormat16bppBGR565; break;

        case D3DFMT_A1R5G5B5:
        case D3DFMT_X1R5G5B5:
            targetGuid = GUID_WICPixelFormat16bppBGR555;
            break;

        case D3DFMT_L16:
            targetGuid = GUID_WICPixelFormat16bppGray;
            break;

        case D3DFMT_R32F:
        case D3DFMT_R16F:
        case D3DFMT_L8:
            targetGuid = GUID_WICPixelFormat8bppGray;
            break;

        default:
            targetGuid = GUID_WICPixelFormat24bppBGR;
            break;
        }
    }

    hr = frame->SetPixelFormat(&targetGuid);
    if (FAILED(hr))
        return hr;

    if (targetFormat && memcmp(targetFormat, &targetGuid, sizeof(WICPixelFormatGUID)) != 0)
    {
        // Requested output pixel format is not supported by the WIC codec
        return E_FAIL;
    }

    // Encode WIC metadata
    ComPtr<IWICMetadataQueryWriter> metawriter;
    if (SUCCEEDED(frame->GetMetadataQueryWriter(metawriter.GetAddressOf())))
    {
        PROPVARIANT value;
        PropVariantInit(&value);

        value.vt = VT_LPSTR;
        value.pszVal = const_cast<char*>("DirectXTK");

        if (memcmp(&guidContainerFormat, &GUID_ContainerFormatPng, sizeof(GUID)) == 0)
        {
            // Set Software name
            std::ignore = metawriter->SetMetadataByName(L"/tEXt/{str=Software}", &value);
        }
        else
        {
            // Set Software name
            std::ignore = metawriter->SetMetadataByName(L"System.ApplicationName", &value);
        }
    }

    D3DLOCKED_RECT lockedRect = {};
    hr = pSource->LockRect(&lockedRect, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr))
        return hr;

    const uint64_t imageSize = uint64_t(lockedRect.Pitch) * uint64_t(desc.Height);
    if (imageSize > UINT32_MAX)
    {
        pSource->UnlockRect();
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    }

    if (memcmp(&targetGuid, &pfGuid, sizeof(WICPixelFormatGUID)) != 0)
    {
        // Conversion required to write
        ComPtr<IWICBitmap> source;
        hr = pWIC->CreateBitmapFromMemory(desc.Width, desc.Height,
            pfGuid,
            static_cast<UINT>(lockedRect.Pitch), static_cast<UINT>(imageSize),
            static_cast<BYTE*>(lockedRect.pBits), source.GetAddressOf());
        if (FAILED(hr))
        {
            pSource->UnlockRect();
            return hr;
        }

        ComPtr<IWICFormatConverter> FC;
        hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
        if (FAILED(hr))
        {
            pSource->UnlockRect();
            return hr;
        }

        BOOL canConvert = FALSE;
        hr = FC->CanConvert(pfGuid, targetGuid, &canConvert);
        if (FAILED(hr) || !canConvert)
        {
            pSource->UnlockRect();
            return E_UNEXPECTED;
        }

        hr = FC->Initialize(source.Get(), targetGuid, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut);
        if (FAILED(hr))
        {
            pSource->UnlockRect();
            return hr;
        }

        WICRect rect = { 0, 0, static_cast<INT>(desc.Width), static_cast<INT>(desc.Height) };
        hr = frame->WriteSource(FC.Get(), &rect);
    }
    else
    {
        // No conversion required
        hr = frame->WritePixels(desc.Height,
            static_cast<UINT>(lockedRect.Pitch), static_cast<UINT>(imageSize),
            static_cast<BYTE*>(lockedRect.pBits));
    }

    pSource->UnlockRect();

    if (FAILED(hr))
        return hr;

    hr = frame->Commit();
    if (FAILED(hr))
        return hr;

    hr = encoder->Commit();
    if (FAILED(hr))
        return hr;

    delonfail.clear();

    return S_OK;
}
