//--------------------------------------------------------------------------------------
// File: DirectXTexJPEG.cpp
//
// DirectXTex Auxilary functions for using the JPEG(https://www.ijg.org) library
//
// For the Windows platform, the strong recommendation is to make use of the WIC
// functions rather than using the open source library. This module exists to support
// Windows Subsystem on Linux.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "DirectXTexP.h"
#include "DirectXTexJPEG.h"

#if __cplusplus < 201703L
#error Requires C++17 (and /Zc:__cplusplus with MSVC)
#endif

#ifndef _BASETSD_H_
#define _BASETSD_H_
#endif

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <jpeglib.h>
#include <jerror.h>


using namespace DirectX;
using std::filesystem::path;
using ScopedFILE = std::unique_ptr<FILE, int(*)(FILE*)>;

namespace
{
#ifdef _WIN32
    ScopedFILE OpenFILE(const path& p) noexcept(false)
    {
        const std::wstring fpath = p.generic_wstring();
        FILE* fp = nullptr;
        if (auto ec = _wfopen_s(&fp, fpath.c_str(), L"rb"); ec)
            throw std::system_error{ static_cast<int>(_doserrno), std::system_category(), "_wfopen_s" };
        return { fp, &fclose };
    }
    ScopedFILE CreateFILE(const path& p) noexcept(false)
    {
        const std::wstring fpath = p.generic_wstring();
        FILE* fp = nullptr;
        if (auto ec = _wfopen_s(&fp, fpath.c_str(), L"w+b"); ec)
            throw std::system_error{ static_cast<int>(_doserrno), std::system_category(), "_wfopen_s" };
        return { fp, &fclose };
    }
#else
    ScopedFILE OpenFILE(const path& p) noexcept(false)
    {
        const std::string fpath = p.generic_string();
        FILE* fp = fopen(fpath.c_str(), "rb");
        if (!fp)
            throw std::system_error{ errno, std::system_category(), "fopen" };
        return { fp, &fclose };
    }
    ScopedFILE CreateFILE(const path& p) noexcept(false)
    {
        const std::string fpath = p.generic_string();
        FILE* fp = fopen(fpath.c_str(), "w+b");
        if (!fp)
            throw std::system_error{ errno, std::system_category(), "fopen" };
        return { fp, &fclose };
    }
#endif

    [[noreturn]] void OnJPEGError(j_common_ptr ptr)
    {
        char msg[JMSG_LENGTH_MAX]{};
        // "0x89 0x50": PNG
        // "0x52 0x49": WEBP
        (*ptr->err->format_message)(ptr, msg);
        jpeg_destroy(ptr);
        throw std::runtime_error{ msg };
    }

    class JPEGDecompress final
    {
        jpeg_error_mgr err;
        jpeg_decompress_struct dec;

    public:
        JPEGDecompress() : err{}, dec{}
        {
            jpeg_std_error(&err);
            err.error_exit = &OnJPEGError;
            dec.err = &err;
            jpeg_create_decompress(&dec);
        }
        ~JPEGDecompress() noexcept
        {
            jpeg_destroy_decompress(&dec);
        }

        void UseInput(FILE* fin) noexcept
        {
            jpeg_stdio_src(&dec, fin);
        }

        static DXGI_FORMAT TranslateColor(J_COLOR_SPACE colorspace) noexcept
        {
            switch (colorspace)
            {
            case JCS_GRAYSCALE: // 1 component
                return DXGI_FORMAT_R8_UNORM;
            case JCS_RGB: // 3 component, Standard RGB
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            case JCS_CMYK: // 4 component. WIC retuns this for CMYK
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            #ifdef LIBJPEG_TURBO_VERSION
            case JCS_EXT_RGBX: // 4 component
            case JCS_EXT_RGBA:
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            case JCS_RGB565:
                return DXGI_FORMAT_B5G6R5_UNORM;
            #endif
            case JCS_YCbCr: // 3 component, YCbCr
            default:
                return DXGI_FORMAT_UNKNOWN;
            }
        }

        void GetMetadata(TexMetadata& metadata) noexcept(false)
        {
            metadata.width = dec.image_width;
            metadata.height = dec.image_height;
            metadata.depth = 1;
            metadata.arraySize = 1;
            metadata.mipLevels = 1;
            metadata.dimension = TEX_DIMENSION_TEXTURE2D;
            metadata.miscFlags2 |= TEX_ALPHA_MODE_OPAQUE;

            metadata.format = TranslateColor(dec.out_color_space);
            if (metadata.format == DXGI_FORMAT_UNKNOWN)
                throw std::runtime_error{ "unexpected out_color_space in jpeg_decompress_struct" };
        }

        HRESULT GetHeader(TexMetadata& metadata) noexcept(false)
        {
            metadata = {};
            switch (jpeg_read_header(&dec, false))
            {
            case JPEG_HEADER_TABLES_ONLY:
                [[fallthrough]];
            case JPEG_HEADER_OK:
                break;
            case JPEG_SUSPENDED:
                return E_FAIL;
            }
            GetMetadata(metadata);
            return S_OK;
        }

    #if !defined(LIBJPEG_TURBO_VERSION)
        // shift pixels with padding in reverse order (to make it work in-memory)
        void ShiftPixels(ScratchImage& image) noexcept
        {
            size_t num_pixels = dec.output_width * dec.output_height;
            uint8_t* dst = image.GetPixels();
            const uint8_t* src = dst;
            for (size_t i = num_pixels - 1; i > 0; i -= 1)
            {
                dst[4*i + 0] = src[3*i + 0];
                dst[4*i + 1] = src[3*i + 1];
                dst[4*i + 2] = src[3*i + 2];
                dst[4*i + 3] = 0;
            }
        }
    #endif

        HRESULT GetImage(TexMetadata& metadata, ScratchImage& image) noexcept(false)
        {
            metadata = {};
            switch (jpeg_read_header(&dec, true))
            {
            case JPEG_HEADER_TABLES_ONLY:
                GetMetadata(metadata);
                [[fallthrough]];
            case JPEG_SUSPENDED:
                return E_FAIL;
            case JPEG_HEADER_OK:
                break;
            }
            GetMetadata(metadata);
            // if the JPEG library doesn't have color space conversion, it should be ERROR_NOT_SUPPORTED
            if (dec.jpeg_color_space == JCS_YCCK)
            {
                // CMYK is the known case
                if (dec.out_color_space == JCS_CMYK)
                    return HRESULT_E_NOT_SUPPORTED;
            }

            if (auto hr = image.Initialize2D(metadata.format, metadata.width, metadata.height, metadata.arraySize, metadata.mipLevels); FAILED(hr))
                return hr;

        #ifdef LIBJPEG_TURBO_VERSION
            // grayscale is the only color space which uses 1 component
            if (dec.out_color_space != JCS_GRAYSCALE)
                // if there is no proper conversion to 4 component, E_FAIL...
                dec.out_color_space = JCS_EXT_RGBX;
        #endif
            if (jpeg_start_decompress(&dec) == false)
                return E_FAIL;

            uint8_t* dest = image.GetPixels();
            const size_t stride = dec.output_width * static_cast<size_t>(dec.output_components);
            std::vector<JSAMPROW> rows(dec.output_height);
            for (size_t i = 0u; i < dec.output_height; ++i)
                rows[i] = dest + (stride * i);

            JDIMENSION leftover = dec.output_height;
            while (leftover > 0)
            {
                JDIMENSION consumed = jpeg_read_scanlines(&dec, &rows[dec.output_scanline], leftover);
                leftover -= consumed;
            }
            if (jpeg_finish_decompress(&dec) == false)
                return E_FAIL;

        #if !defined(LIBJPEG_TURBO_VERSION)
            // if NOT TurboJPEG, we need to make 3 component images to 4 component image
            if (dec.out_color_space != JCS_GRAYSCALE)
                ShiftPixels(image);
        #endif
            return S_OK;
        }

        HRESULT GetImage(ScratchImage& image) noexcept(false)
        {
            TexMetadata metadata{};
            return GetImage(metadata, image);
        }
    };

    class JPEGCompress final
    {
        jpeg_error_mgr err{};
        jpeg_compress_struct enc{};

    public:
        JPEGCompress() : err{}, enc{}
        {
            jpeg_std_error(&err);
            err.error_exit = &OnJPEGError;
            enc.err = &err;
            jpeg_create_compress(&enc);
        }
        ~JPEGCompress() noexcept
        {
            jpeg_destroy_compress(&enc);
        }

        void UseOutput(FILE* fout)
        {
            jpeg_stdio_dest(&enc, fout);
        }

        /// @todo More correct DXGI_FORMAT mapping
        HRESULT WriteImage(const Image& image) noexcept(false)
        {
            switch (image.format)
            {
            case DXGI_FORMAT_R8_UNORM:
                enc.input_components = 1;
                enc.in_color_space = JCS_GRAYSCALE;
                break;
            #ifdef LIBJPEG_TURBO_VERSION
            case DXGI_FORMAT_R8G8B8A8_UNORM:
                enc.input_components = 4;
                enc.in_color_space = JCS_EXT_RGBA;
                break;
            case DXGI_FORMAT_B8G8R8A8_UNORM:
                enc.input_components = 4;
                enc.in_color_space = JCS_EXT_BGRA;
                break;
            #else
            case DXGI_FORMAT_R8G8B8A8_UNORM:
                enc.input_components = 3;
                enc.in_color_space = JCS_RGB;
            #endif

            default:
                return E_INVALIDARG;
            }
            enc.image_width = static_cast<JDIMENSION>(image.width);
            enc.image_height = static_cast<JDIMENSION>(image.height);
            jpeg_set_defaults(&enc);
            jpeg_set_quality(&enc, 100, true);
            // we will write a row each time ...
            jpeg_start_compress(&enc, true);
            const size_t stride = enc.image_width * static_cast<size_t>(enc.input_components);
            while (enc.next_scanline < enc.image_height)
            {
                JSAMPROW rows[1]{ image.pixels + stride * enc.next_scanline };
                jpeg_write_scanlines(&enc, rows, 1);
            }
            jpeg_finish_compress(&enc);
            return S_OK;
        }
    };
}

_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromJPEGFile(
    const wchar_t* file,
    TexMetadata& metadata)
{
    if (!file)
        return E_INVALIDARG;

    try
    {
        auto fin = OpenFILE(file);
        JPEGDecompress decoder{};
        decoder.UseInput(fin.get());
        return decoder.GetHeader(metadata);
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (const std::system_error& ec)
    {
#ifdef _WIN32
        return HRESULT_FROM_WIN32(static_cast<unsigned long>(ec.code().value()));
#else
        return (ec.code().value() == ENOENT) ? HRESULT_ERROR_FILE_NOT_FOUND : E_FAIL;
#endif
    }
    catch (const std::exception&)
    {
        return E_FAIL;
    }
}

_Use_decl_annotations_
HRESULT DirectX::LoadFromJPEGFile(
    const wchar_t* file,
    TexMetadata* metadata,
    ScratchImage&image)
{
    if (!file)
        return E_INVALIDARG;

    image.Release();

    try
    {
        auto fin = OpenFILE(file);
        JPEGDecompress decoder{};
        decoder.UseInput(fin.get());
        if (!metadata)
            return decoder.GetImage(image);
        return decoder.GetImage(*metadata, image);
    }
    catch (const std::bad_alloc&)
    {
        image.Release();
        return E_OUTOFMEMORY;
    }
    catch (const std::system_error& ec)
    {
        image.Release();
#ifdef _WIN32
        return HRESULT_FROM_WIN32(static_cast<unsigned long>(ec.code().value()));
#else
        return (ec.code().value() == ENOENT) ? HRESULT_ERROR_FILE_NOT_FOUND : E_FAIL;
#endif
    }
    catch (const std::exception&)
    {
        image.Release();
        return E_FAIL;
    }
}

_Use_decl_annotations_
HRESULT DirectX::SaveToJPEGFile(
    const Image& image,
    const wchar_t* file)
{
    if (!file)
        return E_INVALIDARG;

    try
    {
        auto fout = CreateFILE(file);
        JPEGCompress encoder{};
        encoder.UseOutput(fout.get());
        return encoder.WriteImage(image);
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (const std::system_error& ec)
    {
#ifdef _WIN32
        return HRESULT_FROM_WIN32(static_cast<unsigned long>(ec.code().value()));
#else
        return (ec.code().value() == ENOENT) ? HRESULT_ERROR_FILE_NOT_FOUND : E_FAIL;
#endif
    }
    catch (const std::exception&)
    {
        return E_FAIL;
    }
}
