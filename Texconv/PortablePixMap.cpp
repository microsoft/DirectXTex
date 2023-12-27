//--------------------------------------------------------------------------------------
// File:  PortablePixMap.cpp
//
// Utilities for reading & writing Portable PixMap files (PPM/PFM)
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
#include <cstdio>
#include <memory>
#include <new>
#include <tuple>

#include "DirectXTex.h"

using namespace DirectX;

namespace
{
    struct handle_closer { void operator()(HANDLE h) noexcept { if (h) CloseHandle(h); } };

    using ScopedHandle = std::unique_ptr<void, handle_closer>;

    inline HANDLE safe_handle(HANDLE h) noexcept { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

    class auto_delete_file
    {
    public:
        auto_delete_file(HANDLE hFile) noexcept : m_handle(hFile) {}

        auto_delete_file(const auto_delete_file&) = delete;
        auto_delete_file& operator=(const auto_delete_file&) = delete;

        ~auto_delete_file()
        {
            if (m_handle)
            {
                FILE_DISPOSITION_INFO info = {};
                info.DeleteFile = TRUE;
                std::ignore = SetFileInformationByHandle(m_handle, FileDispositionInfo, &info, sizeof(info));
            }
        }

        void clear() noexcept { m_handle = nullptr; }

    private:
        HANDLE m_handle;
    };

    inline size_t FindEOL(_In_z_ const char* pString, size_t max)
    {
        size_t pos = 0;

        //find endl
        while (pos < max)
        {
            if (pString[pos] == '\n')
                return pos;
            pos++;
        }

        return 0;
    }

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
}


//============================================================================
// PPM (Portable PixMap)
// http://paulbourke.net/dataformats/ppm/
//============================================================================

HRESULT __cdecl LoadFromPortablePixMap(
    _In_z_ const wchar_t* szFile,
    _Out_opt_ TexMetadata* metadata,
    _Out_ ScratchImage& image) noexcept
{
    std::unique_ptr<uint8_t[]> ppmData;
    size_t ppmSize;
    HRESULT hr = ReadData(szFile, ppmData, ppmSize);
    if (FAILED(hr))
        return hr;

    if (ppmSize < 3)
        return E_FAIL;

    if (ppmData[0] != 'P' || (ppmData[1] != '3' && ppmData[1] != '6') || !isspace(ppmData[2]))
        return E_FAIL;

    const bool ascii = ppmData[1] == '3';

    enum
    {
        PPM_WIDTH, PPM_HEIGHT, PPM_MAX, PPM_DATA_R, PPM_DATA_G, PPM_DATA_B
    };

    int mode = PPM_WIDTH;

    auto pData = ppmData.get() + 2;
    ppmSize -= 2;

    size_t width = 0;
    uint32_t max = 255;
    uint32_t *pixels = nullptr;
    uint32_t *pixelEnd = nullptr;

    while (ppmSize > 0)
    {
        if (!ascii && mode == PPM_DATA_R)
        {
            // Binary data
            if (max > 255 || !pixels || !pixelEnd)
                return E_UNEXPECTED;

            if (ppmSize > 1 && '\r' == *pData)
            {
                pData++;
                ppmSize--;
            }

            if (*pData != '\n')
                return E_FAIL;

            if (ppmSize > 1)
            {
                pData++;
                ppmSize--;
            }

            while (ppmSize > 0 && (pixels < pixelEnd))
            {
                if (ppmSize < 3)
                {
                    return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                }

                *pixels++ = (255 * pData[0] / max)
                    | ((255 * pData[1] / max) << 8)
                    | ((255 * pData[2] / max) << 16)
                    | 0xff000000;

                pData += 3;
                ppmSize -= 3;
            }

            return (pixels != pixelEnd) ? E_FAIL : S_OK;
        }

        if (isspace(*pData))
        {
            // Whitespace
            pData++;
            ppmSize--;
        }
        else if (*pData == '#')
        {
            // Comment
            while (ppmSize > 0 && *pData != '\n')
            {
                pData++;
                ppmSize--;
            }

            if (ppmSize > 0)
            {
                pData++;
                ppmSize--;
            }
        }
        else
        {
            // ASCII number
            uint32_t u = 0;

            while (ppmSize > 0 && !isspace(*pData))
            {
                if (!isdigit(*pData))
                    return E_FAIL;

                u = u * 10 + (*pData - '0');

                pData++;
                ppmSize--;
            }

            switch (mode)
            {
            case PPM_WIDTH:
                if (u == 0)
                    return E_FAIL;

                if (u > INT32_MAX)
                {
                    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                }

                width = u;
                break;

            case PPM_HEIGHT:
                {
                    if (u == 0 || width == 0)
                        return E_FAIL;

                    if (u > INT32_MAX)
                    {
                        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                    }

                    uint64_t sizeBytes = uint64_t(width) * uint64_t(u) * 4;
                    if (sizeBytes > UINT32_MAX)
                    {
                        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
                    }

                    if (metadata)
                    {
                        *metadata = {};
                        metadata->width = width;
                        metadata->height = u;
                        metadata->depth = metadata->arraySize = metadata->mipLevels = 1;
                        metadata->format = DXGI_FORMAT_R8G8B8A8_UNORM;
                        metadata->dimension = TEX_DIMENSION_TEXTURE2D;
                    }

                    hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, u, 1, 1);
                    if (FAILED(hr))
                        return hr;

                    auto img = image.GetImage(0, 0, 0);

                    pixels = reinterpret_cast<uint32_t*>(img->pixels);
                    pixelEnd = pixels + width * u;
                }
                break;

            case PPM_MAX:
                if (u == 0)
                    return E_FAIL;

                max = u;
                break;

            case PPM_DATA_R:
                if (pixels >= pixelEnd)
                    return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

                *pixels = ((u * 255) / max) | 0xff000000;
                break;

            case PPM_DATA_G:
                if (pixels >= pixelEnd)
                    return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

                *pixels |= ((u * 255) / max) << 8;
                break;

            case PPM_DATA_B:
                if (pixels >= pixelEnd)
                    return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

                *pixels |= ((u * 255) / max) << 16;

                if (++pixels == pixelEnd)
                    return S_OK;

                mode = PPM_DATA_R - 1;
                break;
            }

            mode++;
        }
    }

    return E_FAIL;
}


HRESULT __cdecl SaveToPortablePixMap(
    _In_ const Image& image,
    _In_z_ const wchar_t* szFile) noexcept
{
    switch (image.format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        break;

    default:
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    if ((image.width > INT32_MAX) || (image.height > INT32_MAX))
    {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    char header[256] = {};
    const int len = sprintf_s(header, "P6\n%zu %zu\n255\n", image.width, image.height);
    if (len == -1)
        return E_UNEXPECTED;

    ScratchImage tmpImage;
    if (image.format == DXGI_FORMAT_R8G8B8A8_UNORM || image.format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
    {
        tmpImage.InitializeFromImage(image);
    }
    else
    {
        HRESULT hr = Convert(image,
            IsSRGB(image.format) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM,
            TEX_FILTER_DEFAULT, 0.f, tmpImage);
        if (FAILED(hr))
            return hr;
    }

    ScratchImage data;
    data.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, image.width, image.height, 1, 1, CP_FLAGS_24BPP);

    const auto& img = tmpImage.GetImage(0, 0, 0);
    auto dptr = data.GetPixels();
    for (size_t y = 0; y < image.height; ++y)
    {
        auto sptr = img->pixels + y * image.rowPitch;

        for (size_t x = 0; x < image.width; ++x)
        {
            *(dptr++) = sptr[0];
            *(dptr++) = sptr[1];
            *(dptr++) = sptr[2];
            sptr += 4;
        }
    }

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(szFile,
        GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(szFile,
        GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)));
#endif
    if (!hFile)
        return HRESULT_FROM_WIN32(GetLastError());

    auto_delete_file delonfail(hFile.get());

    DWORD bytesWritten;
    if (!WriteFile(hFile.get(), header, static_cast<DWORD>(len), &bytesWritten, nullptr))
        return HRESULT_FROM_WIN32(GetLastError());

    if (!WriteFile(hFile.get(), data.GetPixels(), static_cast<DWORD>(data.GetPixelsSize()), &bytesWritten, nullptr))
        return HRESULT_FROM_WIN32(GetLastError());

    delonfail.clear();

    return S_OK;
}


//============================================================================
// PFM (Portable Float Map)
// http://paulbourke.net/dataformats/pbmhdr/
// https://oyranos.org/2015/03/portable-float-map-with-16-bit-half/index.html
//============================================================================

HRESULT __cdecl LoadFromPortablePixMapHDR(
    _In_z_ const wchar_t* szFile,
    _Out_opt_ TexMetadata* metadata,
    _Out_ ScratchImage& image) noexcept
{
    std::unique_ptr<uint8_t[]> pfmData;
    size_t pfmSize;
    HRESULT hr = ReadData(szFile, pfmData, pfmSize);
    if (FAILED(hr))
        return hr;

    if (pfmSize < 3)
        return E_FAIL;

    if (pfmData[0] != 'P' || !isspace(pfmData[2]))
        return E_FAIL;

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool monochrome = false;
    bool half16 = false;
    unsigned int bpp = 0;
    switch (pfmData[1])
    {
    case 'f': format = DXGI_FORMAT_R32_FLOAT; monochrome = true; bpp = 4u; break;
    case 'F': format = DXGI_FORMAT_R32G32B32A32_FLOAT; bpp = 16u; break;
    case 'h': format = DXGI_FORMAT_R16_FLOAT; monochrome = true; half16 = true; bpp = 2u; break;
    case 'H': format = DXGI_FORMAT_R16G16B16A16_FLOAT; half16 = true; bpp = 8u; break;
    default:
        return E_FAIL;
    }

    auto pData = reinterpret_cast<const char*>(pfmData.get()) + 3;
    pfmSize -= 3;
    if (!pfmSize)
        return E_FAIL;

    // Ignore any comment lines (some tools add them)
    size_t len = 0;
    while (pfmSize > 0)
    {
        len = FindEOL(pData, std::min<size_t>(256, pfmSize));
        if (!len)
            return E_FAIL;

        if (*pData != '#')
            break;

        pData += len + 1;
        if (pfmSize < (len + 1))
        {
            return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
        }
        pfmSize -= len + 1;
        if (!pfmSize)
            return E_FAIL;
    }

    char dataStr[256] = {};
    char junkStr[256] = {};
    strncpy_s(dataStr, pData, len + 1);

    size_t width = 0, height = 0;
    if (sscanf_s(dataStr, "%zu %zu%s", &width, &height, junkStr, 256) != 2)
        return E_FAIL;

    if ((width > INT32_MAX) || (height > INT32_MAX))
    {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    uint64_t sizeBytes = uint64_t(width) * uint64_t(height) * bpp;
    if (sizeBytes > UINT32_MAX)
    {
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    }

    pData += len + 1;
    if (pfmSize < (len + 1))
    {
        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
    }
    pfmSize -= len + 1;
    if (!pfmSize)
        return E_FAIL;

    // Ignore any comment lines (some tools add them)
    len = 0;
    while (pfmSize > 0)
    {
        len = FindEOL(pData, std::min<size_t>(256, pfmSize));
        if (!len)
            return E_FAIL;

        if (*pData != '#')
            break;

        pData += len + 1;
        if (pfmSize < (len + 1))
        {
            return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
        }
        pfmSize -= len + 1;
        if (!pfmSize)
            return E_FAIL;
    }

    strncpy_s(dataStr, pData, len + 1);

    float aspectRatio = 0.f;
    if (sscanf_s(dataStr, "%f%s", &aspectRatio, junkStr, 256) != 1)
        return E_FAIL;

    const bool bigendian = (aspectRatio >= 0);

    pData += len + 1;
    if (pfmSize < (len + 1))
    {
        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
    }
    pfmSize -= len + 1;
    if (!pfmSize)
        return E_FAIL;

    const uint64_t scanline = uint64_t(width) * (half16 ? sizeof(uint16_t) : sizeof(float)) * (monochrome ? 1 : 3);
    if (uint64_t(pfmSize) < (scanline * uint64_t(height)))
        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

    if (metadata)
    {
        *metadata = {};
        metadata->width = width;
        metadata->height = height;
        metadata->depth = metadata->arraySize = metadata->mipLevels = 1;
        metadata->format = format;
        metadata->dimension = TEX_DIMENSION_TEXTURE2D;
    }

    hr = image.Initialize2D(format, width, height, 1, 1);
    if (FAILED(hr))
        return hr;

    auto img = image.GetImage(0, 0, 0);

    if (half16)
    {
        auto sptr = reinterpret_cast<const uint16_t*>(pData);
        if (monochrome)
        {
            for (size_t y = 0; y < height; ++y)
            {
                auto dptr = reinterpret_cast<uint16_t*>(img->pixels + (height - y - 1) * img->rowPitch);

                for (size_t x = 0; x < width; ++x)
                {
                    *dptr++ = (bigendian) ? _byteswap_ushort(*sptr++) : *sptr++;
                }
            }
        }
        else
        {
            for (size_t y = 0; y < height; ++y)
            {
                auto dptr = reinterpret_cast<uint16_t*>(img->pixels + (height - y - 1) * img->rowPitch);

                for (size_t x = 0; x < width; ++x)
                {
                    if (bigendian)
                    {
                        dptr[0] = _byteswap_ushort(sptr[0]);
                        dptr[1] = _byteswap_ushort(sptr[1]);
                        dptr[2] = _byteswap_ushort(sptr[2]);
                    }
                    else
                    {
                        dptr[0] = sptr[0];
                        dptr[1] = sptr[1];
                        dptr[2] = sptr[2];
                    }

                    dptr[3] = 0x3c00; // 1.f
                    sptr += 3;
                    dptr += 4;
                }
            }
        }
    }
    else
    {
        auto sptr = reinterpret_cast<const uint32_t*>(pData);

        if (monochrome)
        {
            for (size_t y = 0; y < height; ++y)
            {
                auto dptr = reinterpret_cast<uint32_t*>(img->pixels + (height - y - 1) * img->rowPitch);

                for (size_t x = 0; x < width; ++x)
                {
                    *dptr++ = (bigendian) ? _byteswap_ulong(*sptr++) : *sptr++;
                }
            }
        }
        else
        {
            for (size_t y = 0; y < height; ++y)
            {
                auto dptr = reinterpret_cast<uint32_t*>(img->pixels + (height - y - 1) * img->rowPitch);

                for (size_t x = 0; x < width; ++x)
                {
                    if (bigendian)
                    {
                        dptr[0] = _byteswap_ulong(sptr[0]);
                        dptr[1] = _byteswap_ulong(sptr[1]);
                        dptr[2] = _byteswap_ulong(sptr[2]);
                    }
                    else
                    {
                        dptr[0] = sptr[0];
                        dptr[1] = sptr[1];
                        dptr[2] = sptr[2];
                    }

                    dptr[3] = 0x3f800000; // 1.f
                    sptr += 3;
                    dptr += 4;
                }
            }
        }
    }

    return S_OK;
}


// We always save as PF or Pf as that's the most common PFM implementation.
HRESULT __cdecl SaveToPortablePixMapHDR(
    _In_ const Image& image,
    _In_z_ const wchar_t* szFile) noexcept
{
    switch (image.format)
    {
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        break;

    default:
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    if ((image.width > INT32_MAX) || (image.height > INT32_MAX))
    {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    char header[256] = {};
    const int len = sprintf_s(header, "P%c\n%zu %zu\n-1.000000\n",
        (image.format == DXGI_FORMAT_R32_FLOAT) ? 'f' : 'F',
        image.width, image.height);

    if (len == -1)
        return E_UNEXPECTED;

    ScratchImage tmpImage;
    if (image.format == DXGI_FORMAT_R32_FLOAT || image.format == DXGI_FORMAT_R32G32B32_FLOAT)
    {
        tmpImage.InitializeFromImage(image);
    }
    else
    {
        HRESULT hr = Convert(image, DXGI_FORMAT_R32G32B32_FLOAT, TEX_FILTER_DEFAULT, 0.f, tmpImage);
        if (FAILED(hr))
            return hr;
    }

    ScratchImage flipImage;
    HRESULT hr = FlipRotate(*tmpImage.GetImage(0, 0, 0), TEX_FR_FLIP_VERTICAL, flipImage);
    if (FAILED(hr))
        return hr;

    tmpImage.Release();

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(szFile,
        GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(szFile,
        GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)));
#endif
    if (!hFile)
        return HRESULT_FROM_WIN32(GetLastError());

    auto_delete_file delonfail(hFile.get());

    DWORD bytesWritten;
    if (!WriteFile(hFile.get(), header, static_cast<DWORD>(len), &bytesWritten, nullptr))
        return HRESULT_FROM_WIN32(GetLastError());

    if (!WriteFile(hFile.get(), flipImage.GetPixels(), static_cast<DWORD>(flipImage.GetPixelsSize()), &bytesWritten, nullptr))
        return HRESULT_FROM_WIN32(GetLastError());

    delonfail.clear();

    return S_OK;
}
