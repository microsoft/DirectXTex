//--------------------------------------------------------------------------------------
// File: DirectXTexXboxDDS.cpp
//
// DirectXTex Auxilary functions for saving "XBOX" Xbox variants of DDS files
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "DirectXTexP.h"
#include "DirectXTexXbox.h"

#include "DDS.h"

#if defined(_GAMING_XBOX) || defined(_USE_GXDK)
#include "gxdk.h"
#else
#include "xdk.h"
#endif

using namespace DirectX;
using namespace Xbox;

namespace
{
    const DDS_PIXELFORMAT DDSPF_XBOX =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('X','B','O','X'), 0, 0, 0, 0, 0 };

#pragma pack(push,1)

    struct DDS_HEADER_XBOX
        // Must match structure in XboxDDSTextureLoader module
    {
        DXGI_FORMAT dxgiFormat;
        uint32_t    resourceDimension;
        uint32_t    miscFlag; // see DDS_RESOURCE_MISC_FLAG
        uint32_t    arraySize;
        uint32_t    miscFlags2; // see DDS_MISC_FLAGS2
        uint32_t    tileMode; // see XG_TILE_MODE / XG_SWIZZLE_MODE
        uint32_t    baseAlignment;
        uint32_t    dataSize;
        uint32_t    xdkVer; // matching _XDK_VER / _GXDK_VER
    };

#pragma pack(pop)

    constexpr uint32_t XBOX_TILEMODE_SCARLETT = 0x1000000;

    static_assert(sizeof(DDS_HEADER_XBOX) == 36, "DDS XBOX Header size mismatch");
    static_assert(sizeof(DDS_HEADER_XBOX) >= sizeof(DDS_HEADER_DXT10), "DDS XBOX Header should be larger than DX10 header");

    constexpr size_t XBOX_HEADER_SIZE = sizeof(uint32_t) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_XBOX);

    //-------------------------------------------------------------------------------------
    // Decodes DDS header using XBOX extended header (variant of DX10 header)
    //-------------------------------------------------------------------------------------
    HRESULT DecodeDDSHeader(
        _In_reads_bytes_(size) const void* pSource,
        size_t size,
        DirectX::TexMetadata& metadata,
        _Out_opt_ DDSMetaData* ddPixelFormat,
        _Out_opt_ XboxTileMode* tmode,
        _Out_opt_ uint32_t* dataSize,
        _Out_opt_ uint32_t* baseAlignment)
    {
        if (!pSource)
            return E_INVALIDARG;

        if (tmode)
        {
            *tmode = c_XboxTileModeInvalid;
        }
        if (dataSize)
        {
            *dataSize = 0;
        }
        if (baseAlignment)
        {
            *baseAlignment = 0;
        }

        metadata = {};

        if (ddPixelFormat)
        {
            *ddPixelFormat = {};
        }

        if (size < (sizeof(DDS_HEADER) + sizeof(uint32_t)))
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }

        // DDS files always start with the same magic number ("DDS ")
        auto const dwMagicNumber = *reinterpret_cast<const uint32_t*>(pSource);
        if (dwMagicNumber != DDS_MAGIC)
        {
            return E_FAIL;
        }

        auto pHeader = reinterpret_cast<const DDS_HEADER*>(reinterpret_cast<const uint8_t*>(pSource) + sizeof(uint32_t));

        // Verify header to validate DDS file
        if (pHeader->size != sizeof(DDS_HEADER)
            || pHeader->ddspf.size != sizeof(DDS_PIXELFORMAT))
        {
            return E_FAIL;
        }

        metadata.mipLevels = pHeader->mipMapCount;
        if (metadata.mipLevels == 0)
            metadata.mipLevels = 1;

        // Check for XBOX extension
        if (!(pHeader->ddspf.flags & DDS_FOURCC)
            || (MAKEFOURCC('X', 'B', 'O', 'X') != pHeader->ddspf.fourCC))
        {
            // We know it's a DDS file, but it's not an XBOX extension
            return S_FALSE;
        }

        // Buffer must be big enough for both headers and magic value
        if (size < XBOX_HEADER_SIZE)
        {
            return E_FAIL;
        }

        auto xboxext = reinterpret_cast<const DDS_HEADER_XBOX*>(
            reinterpret_cast<const uint8_t*>(pSource) + sizeof(uint32_t) + sizeof(DDS_HEADER));

    #ifdef _GXDK_VER
        if (xboxext->xdkVer < _GXDK_VER)
        {
            OutputDebugStringA("WARNING: DDS XBOX file may be outdated and need regeneration\n");
        }
    #elif defined(_XDK_VER)
        if (xboxext->xdkVer < _XDK_VER)
        {
            OutputDebugStringA("WARNING: DDS XBOX file may be outdated and need regeneration\n");
        }
    #endif

        metadata.arraySize = xboxext->arraySize;
        if (metadata.arraySize == 0)
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }

        metadata.format = xboxext->dxgiFormat;
        if (!IsValid(metadata.format))
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }

        static_assert(static_cast<int>(TEX_MISC_TEXTURECUBE) == static_cast<int>(DDS_RESOURCE_MISC_TEXTURECUBE), "DDS header mismatch");

        metadata.miscFlags = xboxext->miscFlag & ~static_cast<uint32_t>(TEX_MISC_TEXTURECUBE);

        switch (xboxext->resourceDimension)
        {
        case DDS_DIMENSION_TEXTURE1D:

            if ((pHeader->flags & DDS_HEIGHT) && pHeader->height != 1)
            {
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            }

            metadata.width = pHeader->width;
            metadata.height = 1;
            metadata.depth = 1;
            metadata.dimension = TEX_DIMENSION_TEXTURE1D;
            break;

        case DDS_DIMENSION_TEXTURE2D:
            if (xboxext->miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE)
            {
                metadata.miscFlags |= TEX_MISC_TEXTURECUBE;
                metadata.arraySize *= 6;
            }

            metadata.width = pHeader->width;
            metadata.height = pHeader->height;
            metadata.depth = 1;
            metadata.dimension = TEX_DIMENSION_TEXTURE2D;
            break;

        case DDS_DIMENSION_TEXTURE3D:
            if (!(pHeader->flags & DDS_HEADER_FLAGS_VOLUME))
            {
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            }

            if (metadata.arraySize > 1)
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

            metadata.width = pHeader->width;
            metadata.height = pHeader->height;
            metadata.depth = pHeader->depth;
            metadata.dimension = TEX_DIMENSION_TEXTURE3D;
            break;

        default:
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }

        if (static_cast<XboxTileMode>(xboxext->tileMode) == c_XboxTileModeInvalid)
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }

    #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
        else if (!(xboxext->tileMode & XBOX_TILEMODE_SCARLETT))
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }
    #else
        else if (xboxext->tileMode & XBOX_TILEMODE_SCARLETT)
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }
    #endif

        static_assert(static_cast<int>(TEX_MISC2_ALPHA_MODE_MASK) == static_cast<int>(DDS_MISC_FLAGS2_ALPHA_MODE_MASK), "DDS header mismatch");

        static_assert(static_cast<int>(TEX_ALPHA_MODE_UNKNOWN) == static_cast<int>(DDS_ALPHA_MODE_UNKNOWN), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_STRAIGHT) == static_cast<int>(DDS_ALPHA_MODE_STRAIGHT), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_PREMULTIPLIED) == static_cast<int>(DDS_ALPHA_MODE_PREMULTIPLIED), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_OPAQUE) == static_cast<int>(DDS_ALPHA_MODE_OPAQUE), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_CUSTOM) == static_cast<int>(DDS_ALPHA_MODE_CUSTOM), "DDS header mismatch");

        metadata.miscFlags2 = xboxext->miscFlags2;

        if (tmode)
        {
            *tmode = static_cast<XboxTileMode>(xboxext->tileMode & ~XBOX_TILEMODE_SCARLETT);
        }

        if (baseAlignment)
            *baseAlignment = xboxext->baseAlignment;

        if (dataSize)
            *dataSize = xboxext->dataSize;

        // Handle DDS-specific metadata
        if (ddPixelFormat)
        {
            ddPixelFormat->size = pHeader->ddspf.size;
            ddPixelFormat->flags = pHeader->ddspf.flags;
            ddPixelFormat->fourCC = pHeader->ddspf.fourCC;
            ddPixelFormat->RGBBitCount = pHeader->ddspf.RGBBitCount;
            ddPixelFormat->RBitMask = pHeader->ddspf.RBitMask;
            ddPixelFormat->GBitMask = pHeader->ddspf.GBitMask;
            ddPixelFormat->BBitMask = pHeader->ddspf.BBitMask;
            ddPixelFormat->ABitMask = pHeader->ddspf.ABitMask;
        }

        return S_OK;
    }


    //-------------------------------------------------------------------------------------
    // Encodes DDS file header (magic value, header, XBOX extended header)
    //-------------------------------------------------------------------------------------
    HRESULT EncodeDDSHeader(
        const XboxImage& xbox,
        _Out_writes_(maxsize) void* pDestination,
        size_t maxsize)
    {
        if (!pDestination)
            return E_POINTER;

        if (maxsize < XBOX_HEADER_SIZE)
            return E_NOT_SUFFICIENT_BUFFER;

        *reinterpret_cast<uint32_t*>(pDestination) = DDS_MAGIC;

        auto header = reinterpret_cast<DDS_HEADER*>(reinterpret_cast<uint8_t*>(pDestination) + sizeof(uint32_t));

        memset(header, 0, sizeof(DDS_HEADER));
        header->size = sizeof(DDS_HEADER);
        header->flags = DDS_HEADER_FLAGS_TEXTURE;
        header->caps = DDS_SURFACE_FLAGS_TEXTURE;

        auto& metadata = xbox.GetMetadata();

        if (metadata.mipLevels > 0)
        {
            header->flags |= DDS_HEADER_FLAGS_MIPMAP;

            if (metadata.mipLevels > UINT32_MAX)
                return E_INVALIDARG;

            header->mipMapCount = static_cast<uint32_t>(metadata.mipLevels);

            if (header->mipMapCount > 1)
                header->caps |= DDS_SURFACE_FLAGS_MIPMAP;
        }

        switch (metadata.dimension)
        {
        case TEX_DIMENSION_TEXTURE1D:
            if (metadata.width > UINT32_MAX)
                return E_INVALIDARG;

            header->width = static_cast<uint32_t>(metadata.width);
            header->height = header->depth = 1;
            break;

        case TEX_DIMENSION_TEXTURE2D:
            if (metadata.height > UINT32_MAX
                || metadata.width > UINT32_MAX)
                return E_INVALIDARG;

            header->height = static_cast<uint32_t>(metadata.height);
            header->width = static_cast<uint32_t>(metadata.width);
            header->depth = 1;

            if (metadata.IsCubemap())
            {
                header->caps |= DDS_SURFACE_FLAGS_CUBEMAP;
                header->caps2 |= DDS_CUBEMAP_ALLFACES;
            }
            break;

        case TEX_DIMENSION_TEXTURE3D:
            if (metadata.height > UINT32_MAX
                || metadata.width > UINT32_MAX
                || metadata.depth > UINT32_MAX)
                return E_INVALIDARG;

            header->flags |= DDS_HEADER_FLAGS_VOLUME;
            header->caps2 |= DDS_FLAGS_VOLUME;
            header->height = static_cast<uint32_t>(metadata.height);
            header->width = static_cast<uint32_t>(metadata.width);
            header->depth = static_cast<uint32_t>(metadata.depth);
            break;

        default:
            return E_FAIL;
        }

        size_t rowPitch, slicePitch;
        ComputePitch(metadata.format, metadata.width, metadata.height, rowPitch, slicePitch, CP_FLAGS_NONE);

        if (slicePitch > UINT32_MAX
            || rowPitch > UINT32_MAX)
            return E_FAIL;

        if (IsCompressed(metadata.format))
        {
            header->flags |= DDS_HEADER_FLAGS_LINEARSIZE;
            header->pitchOrLinearSize = static_cast<uint32_t>(slicePitch);
        }
        else
        {
            header->flags |= DDS_HEADER_FLAGS_PITCH;
            header->pitchOrLinearSize = static_cast<uint32_t>(rowPitch);
        }

        memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_XBOX, sizeof(DDS_PIXELFORMAT));

        // Setup XBOX extended header
        auto xboxext = reinterpret_cast<DDS_HEADER_XBOX*>(reinterpret_cast<uint8_t*>(header) + sizeof(DDS_HEADER));

        memset(xboxext, 0, sizeof(DDS_HEADER_XBOX));
        xboxext->dxgiFormat = metadata.format;
        xboxext->resourceDimension = metadata.dimension;

        if (metadata.arraySize > UINT32_MAX)
            return E_INVALIDARG;

        static_assert(static_cast<int>(TEX_MISC_TEXTURECUBE) == static_cast<int>(DDS_RESOURCE_MISC_TEXTURECUBE), "DDS header mismatch");
        xboxext->miscFlag = metadata.miscFlags & ~static_cast<uint32_t>(TEX_MISC_TEXTURECUBE);

        if (metadata.miscFlags & TEX_MISC_TEXTURECUBE)
        {
            xboxext->miscFlag |= TEX_MISC_TEXTURECUBE;
            assert((metadata.arraySize % 6) == 0);
            xboxext->arraySize = static_cast<UINT>(metadata.arraySize / 6);
        }
        else
        {
            xboxext->arraySize = static_cast<UINT>(metadata.arraySize);
        }

        static_assert(static_cast<int>(TEX_MISC2_ALPHA_MODE_MASK) == static_cast<int>(DDS_MISC_FLAGS2_ALPHA_MODE_MASK), "DDS header mismatch");

        static_assert(static_cast<int>(TEX_ALPHA_MODE_UNKNOWN) == static_cast<int>(DDS_ALPHA_MODE_UNKNOWN), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_STRAIGHT) == static_cast<int>(DDS_ALPHA_MODE_STRAIGHT), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_PREMULTIPLIED) == static_cast<int>(DDS_ALPHA_MODE_PREMULTIPLIED), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_OPAQUE) == static_cast<int>(DDS_ALPHA_MODE_OPAQUE), "DDS header mismatch");
        static_assert(static_cast<int>(TEX_ALPHA_MODE_CUSTOM) == static_cast<int>(DDS_ALPHA_MODE_CUSTOM), "DDS header mismatch");

        xboxext->miscFlags2 = metadata.miscFlags2;

    #if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
        xboxext->tileMode = static_cast<uint32_t>(xbox.GetTileMode()) | XBOX_TILEMODE_SCARLETT;
    #else
        xboxext->tileMode = static_cast<uint32_t>(xbox.GetTileMode());
    #endif

        xboxext->baseAlignment = xbox.GetAlignment();
        xboxext->dataSize = xbox.GetSize();
    #ifdef _GXDK_VER
        xboxext->xdkVer = _GXDK_VER;
    #elif defined(_XDK_VER)
        xboxext->xdkVer = _XDK_VER;
    #endif

        return S_OK;
    }
}


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Obtain metadata from DDS file in memory/on disk
//-------------------------------------------------------------------------------------

_Use_decl_annotations_
HRESULT Xbox::GetMetadataFromDDSMemory(
    const void* pSource,
    size_t size,
    TexMetadata& metadata,
    bool& isXbox)
{
    return Xbox::GetMetadataFromDDSMemoryEx(pSource, size, metadata, isXbox, nullptr);
}

_Use_decl_annotations_
HRESULT Xbox::GetMetadataFromDDSMemoryEx(
    const void* pSource,
    size_t size,
    TexMetadata& metadata,
    bool& isXbox,
    DDSMetaData* ddPixelFormat)
{
    if (!pSource || !size)
        return E_INVALIDARG;

    isXbox = false;

    HRESULT hr = DecodeDDSHeader(pSource, size, metadata, ddPixelFormat, nullptr, nullptr, nullptr);

    if (hr == S_FALSE)
    {
        hr = DirectX::GetMetadataFromDDSMemoryEx(pSource, size, DirectX::DDS_FLAGS_NONE, metadata, ddPixelFormat);
    }
    else if (SUCCEEDED(hr))
    {
        isXbox = true;
    }

    return hr;
}

_Use_decl_annotations_
HRESULT Xbox::GetMetadataFromDDSFile(
    const wchar_t* szFile,
    TexMetadata& metadata,
    bool& isXbox)
{
    return Xbox::GetMetadataFromDDSFileEx(szFile, metadata, isXbox, nullptr);
}

_Use_decl_annotations_
HRESULT Xbox::GetMetadataFromDDSFileEx(
    const wchar_t* szFile,
    TexMetadata& metadata,
    bool& isXbox,
    DDSMetaData* ddPixelFormat)
{
    if (!szFile)
        return E_INVALIDARG;

    isXbox = false;

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(szFile, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, nullptr)));
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

    // File is too big for 32-bit allocation, so reject read (4 GB should be plenty large enough for a valid DDS file)
    if (fileInfo.EndOfFile.HighPart > 0)
    {
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    }

    // Need at least enough data to fill the standard header and magic number to be a valid DDS
    if (fileInfo.EndOfFile.LowPart < (sizeof(DDS_HEADER) + sizeof(uint32_t)))
    {
        return E_FAIL;
    }

    // Read the header in (including extended header if present)
    uint8_t header[XBOX_HEADER_SIZE] = {};

    DWORD bytesRead = 0;
    if (!ReadFile(hFile.get(), header, XBOX_HEADER_SIZE, &bytesRead, nullptr))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    HRESULT hr = DecodeDDSHeader(header, bytesRead, metadata, ddPixelFormat, nullptr, nullptr, nullptr);

    if (hr == S_FALSE)
    {
        hr = DirectX::GetMetadataFromDDSMemoryEx(header, bytesRead, DirectX::DDS_FLAGS_NONE, metadata, ddPixelFormat);
    }
    else if (SUCCEEDED(hr))
    {
        isXbox = true;
    }

    return hr;
}


//-------------------------------------------------------------------------------------
// Load a DDS file in memory
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Xbox::LoadFromDDSMemory(
    const void* pSource,
    size_t size,
    TexMetadata* metadata,
    XboxImage& xbox)
{
    return Xbox::LoadFromDDSMemoryEx(pSource, size, metadata, nullptr, xbox);
}

_Use_decl_annotations_
HRESULT Xbox::LoadFromDDSMemoryEx(
    const void* pSource,
    size_t size,
    TexMetadata* metadata,
    DDSMetaData* ddPixelFormat,
    XboxImage& xbox)
{
    if (!pSource || !size)
        return E_INVALIDARG;

    xbox.Release();

    TexMetadata mdata;
    uint32_t dataSize;
    uint32_t baseAlignment;
    XboxTileMode tmode;
    HRESULT hr = DecodeDDSHeader(pSource, size, mdata, ddPixelFormat, &tmode, &dataSize, &baseAlignment);
    if (hr == S_FALSE)
    {
        // It's a DDS, but not an XBOX variant
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }
    if (FAILED(hr))
    {
        return hr;
    }

    if (!dataSize || !baseAlignment)
    {
        return E_FAIL;
    }

    if (size <= XBOX_HEADER_SIZE)
    {
        return E_FAIL;
    }

    // Copy tiled data
    const size_t remaining = size - XBOX_HEADER_SIZE;

    if (remaining < dataSize)
    {
        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
    }

    hr = xbox.Initialize(mdata, tmode, dataSize, baseAlignment);
    if (FAILED(hr))
        return hr;

    assert(xbox.GetPointer() != nullptr);

    memcpy(xbox.GetPointer(), reinterpret_cast<const uint8_t*>(pSource) + XBOX_HEADER_SIZE, dataSize);

    if (metadata)
        memcpy(metadata, &mdata, sizeof(TexMetadata));

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Load a DDS file from disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Xbox::LoadFromDDSFile(
    const wchar_t* szFile,
    TexMetadata* metadata,
    XboxImage& xbox)
{
    return Xbox::LoadFromDDSFileEx(szFile, metadata, nullptr, xbox);
}

_Use_decl_annotations_
HRESULT Xbox::LoadFromDDSFileEx(
    const wchar_t* szFile,
    TexMetadata* metadata,
    DDSMetaData* ddPixelFormat,
    XboxImage& xbox)
{
    if (!szFile)
        return E_INVALIDARG;

    xbox.Release();

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(szFile, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, nullptr)));
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

    // File is too big for 32-bit allocation, so reject read (4 GB should be plenty large enough for a valid DDS file)
    if (fileInfo.EndOfFile.HighPart > 0)
    {
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    }

    // Need at least enough data to fill the standard header and magic number to be a valid DDS
    if (fileInfo.EndOfFile.LowPart < (sizeof(DDS_HEADER) + sizeof(uint32_t)))
    {
        return E_FAIL;
    }

    // Read the header in (including extended header if present)
    uint8_t header[XBOX_HEADER_SIZE] = {};

    DWORD bytesRead = 0;
    if (!ReadFile(hFile.get(), header, XBOX_HEADER_SIZE, &bytesRead, nullptr))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    TexMetadata mdata;
    XboxTileMode tmode;
    uint32_t dataSize;
    uint32_t baseAlignment;
    HRESULT hr = DecodeDDSHeader(header, bytesRead, mdata, ddPixelFormat, &tmode, &dataSize, &baseAlignment);
    if (hr == S_FALSE)
    {
        // It's a DDS, but not an XBOX variant
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }
    if (FAILED(hr))
        return hr;

    if (!dataSize || !baseAlignment)
    {
        return E_FAIL;
    }

    // Read tiled data
    const DWORD remaining = fileInfo.EndOfFile.LowPart - XBOX_HEADER_SIZE;
    if (remaining == 0)
        return E_FAIL;

    if (remaining < dataSize)
    {
        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
    }

    hr = xbox.Initialize(mdata, tmode, dataSize, baseAlignment);
    if (FAILED(hr))
        return hr;

    assert(xbox.GetPointer() != nullptr);

    if (!ReadFile(hFile.get(), xbox.GetPointer(), dataSize, &bytesRead, nullptr))
    {
        xbox.Release();
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (metadata)
        memcpy(metadata, &mdata, sizeof(TexMetadata));

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Save a DDS file to memory
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Xbox::SaveToDDSMemory(const XboxImage& xbox, Blob& blob)
{
    if (!xbox.GetPointer() || !xbox.GetSize() || !xbox.GetAlignment())
        return E_INVALIDARG;

    blob.Release();

    HRESULT hr = blob.Initialize(XBOX_HEADER_SIZE + xbox.GetSize());
    if (FAILED(hr))
        return hr;

    // Copy header
    auto pDestination = reinterpret_cast<uint8_t*>(blob.GetBufferPointer());
    assert(pDestination);

    hr = EncodeDDSHeader(xbox, pDestination, XBOX_HEADER_SIZE);
    if (FAILED(hr))
    {
        blob.Release();
        return hr;
    }

    // Copy tiled data
    const size_t remaining = blob.GetBufferSize() - XBOX_HEADER_SIZE;
    pDestination += XBOX_HEADER_SIZE;

    if (!remaining)
    {
        blob.Release();
        return E_FAIL;
    }

    if (remaining < xbox.GetSize())
    {
        blob.Release();
        return E_UNEXPECTED;
    }

    memcpy(pDestination, xbox.GetPointer(), xbox.GetSize());

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Save a DDS file to disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Xbox::SaveToDDSFile(const XboxImage& xbox, const wchar_t* szFile)
{
    if (!szFile || !xbox.GetPointer() || !xbox.GetSize() || !xbox.GetAlignment())
        return E_INVALIDARG;

    // Create DDS Header
    uint8_t header[XBOX_HEADER_SIZE] = {};
    HRESULT hr = EncodeDDSHeader(xbox, header, XBOX_HEADER_SIZE);
    if (FAILED(hr))
        return hr;

    // Create file and write header
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(szFile,
        GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(szFile,
        GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)));
#endif
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    DWORD bytesWritten;
    if (!WriteFile(hFile.get(), header, static_cast<DWORD>(XBOX_HEADER_SIZE), &bytesWritten, nullptr))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (bytesWritten != XBOX_HEADER_SIZE)
    {
        return E_FAIL;
    }

    // Write tiled data
    if (!WriteFile(hFile.get(), xbox.GetPointer(), static_cast<DWORD>(xbox.GetSize()), &bytesWritten, nullptr))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (bytesWritten != xbox.GetSize())
    {
        return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Adapters for /Zc:wchar_t- clients

#if defined(_MSC_VER) && !defined(_NATIVE_WCHAR_T_DEFINED)

namespace Xbox
{
    HRESULT __cdecl GetMetadataFromDDSFile(
        _In_z_ const __wchar_t* szFile,
        _Out_ DirectX::TexMetadata& metadata,
        _Out_ bool& isXbox)
    {
        return GetMetadataFromDDSFile(reinterpret_cast<const unsigned short*>(szFile), metadata, isXbox);
    }

    HRESULT __cdecl GetMetadataFromDDSFileEx(
        _In_z_ const __wchar_t* szFile,
        _Out_ DirectX::TexMetadata& metadata,
        _Out_ bool& isXbox,
        _Out_opt_ DirectX::DDSMetaData* ddPixelFormat)
    {
        return GetMetadataFromDDSFileEx(reinterpret_cast<const unsigned short*>(szFile), metadata, isXbox, ddPixelFormat);
    }

    HRESULT __cdecl LoadFromDDSFile(
        _In_z_ const __wchar_t* szFile,
        _Out_opt_ DirectX::TexMetadata* metadata,
        _Out_ XboxImage& image)
    {
        return LoadFromDDSFile(reinterpret_cast<const unsigned short*>(szFile), metadata, image);
    }

    HRESULT __cdecl LoadFromDDSFileEx(
        _In_z_ const __wchar_t* szFile,
        _Out_opt_ DirectX::TexMetadata* metadata,
        _Out_opt_ DirectX::DDSMetaData* ddPixelFormat,
        _Out_ XboxImage& image)
    {
        return LoadFromDDSFileEx(reinterpret_cast<const unsigned short*>(szFile), metadata, ddPixelFormat, image);
    }

    HRESULT __cdecl SaveToDDSFile(_In_ const XboxImage& xbox, _In_z_ const __wchar_t* szFile)
    {
        return SaveToDDSFile(xbox, reinterpret_cast<const unsigned short*>(szFile));
    }
}

#endif // !_NATIVE_WCHAR_T_DEFINED
