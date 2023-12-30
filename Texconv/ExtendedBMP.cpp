//--------------------------------------------------------------------------------------
// File: ExtendedBMP.cpp
//
// Utilities for reading BMP files including the DXTn unofficial "FS70"
// extension created for Microsoft flight simulators.
//
// http://www.mwgfx.co.uk/programs/dxtbmp.htm
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

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

#include "DirectXTex.h"

using namespace DirectX;

namespace
{
    struct handle_closer { void operator()(HANDLE h) noexcept { if (h) CloseHandle(h); } };

    using ScopedHandle = std::unique_ptr<void, handle_closer>;

    inline HANDLE safe_handle(HANDLE h) noexcept { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

    HRESULT ReadData(_In_z_ const wchar_t* szFile, std::unique_ptr<uint8_t[]>& blob, size_t& blobSize)
    {
        blob.reset();

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

        blobSize = fileInfo.EndOfFile.LowPart;

        return S_OK;
    }

    HRESULT LoadFromExtendedBMPMemory(
        _In_reads_bytes_(size) const void* pSource,
        _In_ size_t size,
        _Out_opt_ TexMetadata* metadata,
        _Out_ ScratchImage& image)
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

        const size_t remaining = size - filehdr->bfOffBits;
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

HRESULT __cdecl LoadFromBMPEx(
    _In_z_ const wchar_t* szFile,
    _In_ WIC_FLAGS flags,
    _Out_opt_ TexMetadata* metadata,
    _Out_ ScratchImage& image) noexcept
{
    std::unique_ptr<uint8_t[]> bmpData;
    size_t bmpSize;
    HRESULT hr = ReadData(szFile, bmpData, bmpSize);
    if (FAILED(hr))
        return hr;

    hr = LoadFromWICMemory(bmpData.get(), bmpSize, flags, metadata, image);
    if (FAILED(hr))
    {
        hr = LoadFromExtendedBMPMemory(bmpData.get(), bmpSize, metadata, image);
    }

    return hr;
}
