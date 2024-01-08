//--------------------------------------------------------------------------------------
// File: DirectXTexEXR.cpp
//
// DirectXTex Auxilary functions for using the OpenEXR library
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "DirectXTexP.h"

#include "DirectXTexEXR.h"

#include <DirectXPackedVector.h>

#include <cassert>
#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>

//
// Requires the OpenEXR library <http://www.openexr.com/> and its dependencies.
//

#ifdef __clang__
#pragma clang diagnostic ignored "-Wswitch-enum"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wshadow-field-in-constructor"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#pragma warning(push)
#pragma warning(disable : 4244 4996)
#include <ImfRgbaFile.h>
#include <ImfIO.h>
#pragma warning(pop)

#ifdef __clang__
#pragma clang diagnostic pop
#endif

static_assert(sizeof(Imf::Rgba) == 8, "Mismatch size");

using namespace DirectX;
using PackedVector::XMHALF4;

#ifdef _WIN32
namespace
{
    class com_exception : public std::exception
    {
    public:
        com_exception(HRESULT hr) noexcept : result(hr) {}

        const char* what() const noexcept override
        {
            static char s_str[64] = {};
            sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(result));
            return s_str;
        }

        HRESULT get_result() const noexcept { return result; }

    private:
        HRESULT result;
    };

    class InputStream : public Imf::IStream
    {
    public:
        InputStream(HANDLE hFile, const char fileName[]) :
            IStream(fileName), m_hFile(hFile)
        {
            const LARGE_INTEGER dist = {};
            LARGE_INTEGER result;
            if (!SetFilePointerEx(m_hFile, dist, &result, FILE_END))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }

            m_EOF = result.QuadPart;

            if (!SetFilePointerEx(m_hFile, dist, nullptr, FILE_BEGIN))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        InputStream(const InputStream&) = delete;
        InputStream& operator = (const InputStream&) = delete;

        InputStream(InputStream&&) = delete;
        InputStream& operator=(InputStream&&) = delete;

        bool read(char c[], int n) override
        {
            DWORD bytesRead;
            if (!ReadFile(m_hFile, c, static_cast<DWORD>(n), &bytesRead, nullptr))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }

            const LARGE_INTEGER dist = {};
            LARGE_INTEGER result;
            if (!SetFilePointerEx(m_hFile, dist, &result, FILE_CURRENT))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }

            return result.QuadPart >= m_EOF;
        }

        uint64_t tellg() override
        {
            const LARGE_INTEGER dist = {};
            LARGE_INTEGER result;
            if (!SetFilePointerEx(m_hFile, dist, &result, FILE_CURRENT))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
            return static_cast<uint64_t>(result.QuadPart);
        }

        void seekg(uint64_t pos) override
        {
            LARGE_INTEGER dist;
            dist.QuadPart = static_cast<LONGLONG>(pos);
            if (!SetFilePointerEx(m_hFile, dist, nullptr, FILE_BEGIN))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        void clear() override
        {
            SetLastError(0);
        }

    private:
        HANDLE m_hFile;
        LONGLONG m_EOF;
    };

    class OutputStream : public Imf::OStream
    {
    public:
        OutputStream(HANDLE hFile, const char fileName[]) :
            OStream(fileName), m_hFile(hFile) {}

        OutputStream(const OutputStream&) = delete;
        OutputStream& operator = (const OutputStream&) = delete;

        OutputStream(OutputStream&&) = delete;
        OutputStream& operator=(OutputStream&&) = delete;

        void write(const char c[], int n) override
        {
            DWORD bytesWritten;
            if (!WriteFile(m_hFile, c, static_cast<DWORD>(n), &bytesWritten, nullptr))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        uint64_t tellp() override
        {
            const LARGE_INTEGER dist = {};
            LARGE_INTEGER result;
            if (!SetFilePointerEx(m_hFile, dist, &result, FILE_CURRENT))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
            return static_cast<uint64_t>(result.QuadPart);
        }

        void seekp(uint64_t pos) override
        {
            LARGE_INTEGER dist;
            dist.QuadPart = static_cast<LONGLONG>(pos);
            if (!SetFilePointerEx(m_hFile, dist, nullptr, FILE_BEGIN))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

    private:
        HANDLE m_hFile;
    };
}
#endif // _WIN32


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Obtain metadata from EXR file on disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromEXRFile(const wchar_t* szFile, TexMetadata& metadata)
{
    if (!szFile)
        return E_INVALIDARG;

#ifdef _WIN32
    std::string fileName;
    const int nameLength = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
    if (nameLength > 0)
    {
        fileName.resize(nameLength);
        const int result = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, fileName.data(), nameLength, nullptr, nullptr);
        if (result <= 0)
        {
            fileName.clear();
        }
    }

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(
        szFile,
        GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
        nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(
        szFile,
        GENERIC_READ, FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr)));
#endif
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    InputStream stream(hFile.get(), fileName.c_str());
#else
    std::wstring wFileName(szFile);
    std::string fileName(wFileName.cbegin(), wFileName.cend());
#endif

    HRESULT hr = S_OK;

    try
    {
#ifdef _WIN32
        Imf::RgbaInputFile file(stream);
#else
        Imf::RgbaInputFile file(fileName.c_str());
#endif

        const auto dw = file.dataWindow();

        const int width = dw.max.x - dw.min.x + 1;
        const int height = dw.max.y - dw.min.y + 1;

        if (width < 1 || height < 1)
            return E_FAIL;

        metadata.width = static_cast<size_t>(width);
        metadata.height = static_cast<size_t>(height);
        metadata.depth = metadata.arraySize = metadata.mipLevels = 1;
        metadata.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        metadata.dimension = TEX_DIMENSION_TEXTURE2D;
    }
#ifdef _WIN32
    catch (const com_exception& exc)
    {
#ifdef _DEBUG
        OutputDebugStringA(exc.what());
#endif
        hr = exc.get_result();
    }
#endif
#if defined(_WIN32) && defined(_DEBUG)
    catch (const std::exception& exc)
    {
        OutputDebugStringA(exc.what());
        hr = E_FAIL;
    }
#else
    catch (const std::exception&)
    {
        hr = E_FAIL;
    }
#endif
    catch (...)
    {
        hr = E_UNEXPECTED;
    }

    return hr;
}


//-------------------------------------------------------------------------------------
// Load a EXR file from disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::LoadFromEXRFile(const wchar_t* szFile, TexMetadata* metadata, ScratchImage& image)
{
    if (!szFile)
        return E_INVALIDARG;

    image.Release();

    if (metadata)
    {
        memset(metadata, 0, sizeof(TexMetadata));
    }

#ifdef _WIN32
    std::string fileName;
    const int nameLength = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
    if (nameLength > 0)
    {
        fileName.resize(nameLength);
        const int result = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, fileName.data(), nameLength, nullptr, nullptr);
        if (result <= 0)
        {
            fileName.clear();
        }
    }

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(
        szFile,
        GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
        nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(
        szFile,
        GENERIC_READ, FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr)));
#endif
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    InputStream stream(hFile.get(), fileName.c_str());
#else
    std::wstring wFileName(szFile);
    std::string fileName(wFileName.cbegin(), wFileName.cend());
#endif

    HRESULT hr = S_OK;

    try
    {
#ifdef _WIN32
        Imf::RgbaInputFile file(stream);
#else
        Imf::RgbaInputFile file(fileName.c_str());
#endif

        auto const dw = file.dataWindow();

        const int width = dw.max.x - dw.min.x + 1;
        const int height = dw.max.y - dw.min.y + 1;

        if (width < 1 || height < 1)
            return E_FAIL;

        if (metadata)
        {
            metadata->width = static_cast<size_t>(width);
            metadata->height = static_cast<size_t>(height);
            metadata->depth = metadata->arraySize = metadata->mipLevels = 1;
            metadata->format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            metadata->dimension = TEX_DIMENSION_TEXTURE2D;
        }

        hr = image.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT,
            static_cast<size_t>(width), static_cast<size_t>(height), 1u, 1u);
        if (FAILED(hr))
            return hr;

        file.setFrameBuffer(reinterpret_cast<Imf::Rgba*>(image.GetPixels()) - dw.min.x - dw.min.y * width, 1, static_cast<size_t>(width));
        file.readPixels(dw.min.y, dw.max.y);
    }
#ifdef _WIN32
    catch (const com_exception& exc)
    {
#ifdef _DEBUG
        OutputDebugStringA(exc.what());
#endif
        hr = exc.get_result();
    }
#endif
#if defined(_WIN32) && defined(_DEBUG)
    catch (const std::exception& exc)
    {
        OutputDebugStringA(exc.what());
        hr = E_FAIL;
    }
#else
    catch (const std::exception&)
    {
        hr = E_FAIL;
    }
#endif
    catch (...)
    {
        hr = E_UNEXPECTED;
    }

    if (FAILED(hr))
    {
        image.Release();
    }

    return hr;
}


//-------------------------------------------------------------------------------------
// Save a EXR file to disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::SaveToEXRFile(const Image& image, const wchar_t* szFile)
{
    if (!szFile)
        return E_INVALIDARG;

    if (!image.pixels)
        return E_POINTER;

    if (image.width > INT32_MAX || image.height > INT32_MAX)
        return /* HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) */ static_cast<HRESULT>(0x80070032L);

    switch (image.format)
    {
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        if ((image.rowPitch % 8) > 0)
            return E_FAIL;
        break;

    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32_FLOAT:
        break;

    default:
        return /* HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) */ static_cast<HRESULT>(0x80070032L);
    }

#ifdef _WIN32
    std::string fileName;
    const int nameLength = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
    if (nameLength > 0)
    {
        fileName.resize(nameLength);
        const int result = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, fileName.data(), nameLength, nullptr, nullptr);
        if (result <= 0)
        {
            fileName.clear();
        }
    }

    // Create file and write header
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(
        szFile,
        GENERIC_WRITE, 0, CREATE_ALWAYS,
        nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(
        szFile,
        GENERIC_WRITE, 0,
        nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
        nullptr)));
#endif
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    auto_delete_file delonfail(hFile.get());

    OutputStream stream(hFile.get(), fileName.c_str());
#else
    std::wstring wFileName(szFile);
    std::string fileName(wFileName.cbegin(), wFileName.cend());
#endif

    HRESULT hr = S_OK;

    try
    {
        const int width = static_cast<int>(image.width);
        const int height = static_cast<int>(image.height);

#ifdef _WIN32
        Imf::RgbaOutputFile file(stream, Imf::Header(width, height), Imf::WRITE_RGBA);
#else
        Imf::RgbaOutputFile file(fileName.c_str(), Imf::Header(width, height), Imf::WRITE_RGBA);
#endif

        if (image.format == DXGI_FORMAT_R16G16B16A16_FLOAT)
        {
            file.setFrameBuffer(reinterpret_cast<const Imf::Rgba*>(image.pixels), 1, image.rowPitch / 8);
            file.writePixels(height);
        }
        else
        {
            const uint64_t bytes = image.width * image.height;

            if (bytes > UINT32_MAX)
            {
                return /* HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) */ static_cast<HRESULT>(0x80070216L);
            }

            std::unique_ptr<XMHALF4> temp(new (std::nothrow) XMHALF4[static_cast<size_t>(bytes)]);
            if (!temp)
                return E_OUTOFMEMORY;

            file.setFrameBuffer(reinterpret_cast<const Imf::Rgba*>(temp.get()), 1, image.width);

            auto sPtr = image.pixels;
            auto dPtr = temp.get();
            if (image.format == DXGI_FORMAT_R32G32B32A32_FLOAT)
            {
                for (int j = 0; j < height; ++j)
                {
                    auto srcPtr = reinterpret_cast<const XMFLOAT4*>(sPtr);
                    auto destPtr = dPtr;
                    for (int k = 0; k < width; ++k, ++srcPtr, ++destPtr)
                    {
                        const XMVECTOR v = XMLoadFloat4(srcPtr);
                        PackedVector::XMStoreHalf4(destPtr, v);
                    }

                    sPtr += image.rowPitch;
                    dPtr += width;

                    file.writePixels(1);
                }
            }
            else
            {
                assert(image.format == DXGI_FORMAT_R32G32B32_FLOAT);

                for (int j = 0; j < height; ++j)
                {
                    auto srcPtr = reinterpret_cast<const XMFLOAT3*>(sPtr);
                    auto destPtr = dPtr;
                    for (int k = 0; k < width; ++k, ++srcPtr, ++destPtr)
                    {
                        XMVECTOR v = XMLoadFloat3(srcPtr);
                        v = XMVectorSelect(g_XMIdentityR3, v, g_XMSelect1110);
                        PackedVector::XMStoreHalf4(destPtr, v);
                    }

                    sPtr += image.rowPitch;
                    dPtr += width;

                    file.writePixels(1);
                }
            }
        }
    }
#ifdef _WIN32
    catch (const com_exception& exc)
    {
#ifdef _DEBUG
        OutputDebugStringA(exc.what());
#endif
        hr = exc.get_result();
    }
#endif
#if defined(_WIN32) && defined(_DEBUG)
    catch (const std::exception& exc)
    {
        OutputDebugStringA(exc.what());
        hr = E_FAIL;
    }
#else
    catch (const std::exception&)
    {
        hr = E_FAIL;
    }
#endif
    catch (...)
    {
        hr = E_UNEXPECTED;
    }

    if (FAILED(hr))
        return hr;

#ifdef _WIN32
    delonfail.clear();
#endif

    return S_OK;
}
