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
        JPEG_FLAGS userFlags;

    public:
        JPEGDecompress(JPEG_FLAGS flags) : err{}, dec{}, userFlags(flags)
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

        DXGI_FORMAT TranslateColor(J_COLOR_SPACE colorspace) noexcept
        {
            switch (colorspace)
            {
            case JCS_GRAYSCALE: // 1 component
                return DXGI_FORMAT_R8_UNORM;
            case JCS_RGB: // 3 component, Standard RGB
                return (userFlags & JPEG_FLAGS_DEFAULT_LINEAR) ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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
            metadata.format = TranslateColor(dec.out_color_space);
            if (metadata.format == DXGI_FORMAT_UNKNOWN)
            {
                throw std::runtime_error{ "unexpected out_color_space in jpeg_decompress_struct" };
            }
            if (metadata.format == DXGI_FORMAT_R8G8B8A8_UNORM
                || metadata.format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
            {
                metadata.miscFlags2 |= TEX_ALPHA_MODE_OPAQUE;
            }
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

    #ifndef LIBJPEG_TURBO_VERSION
        // shift pixels with padding in reverse order (to make it work in-memory)
        void ShiftPixels(const Image& image) noexcept
        {
            uint8_t* scanline = image.pixels;
            for (size_t y = 0; y < image.height; ++y)
            {
                for (size_t i = (image.width - 1); i > 0; i -= 1)
                {
                    scanline[4*i + 0] = scanline[3*i + 0];
                    scanline[4*i + 1] = scanline[3*i + 1];
                    scanline[4*i + 2] = scanline[3*i + 2];
                    scanline[4*i + 3] = 0xff;
                }

                scanline += image.rowPitch;
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

            if (auto hr = image.Initialize2D(metadata.format, metadata.width, metadata.height, 1u, 1u); FAILED(hr))
                return hr;

        #ifdef LIBJPEG_TURBO_VERSION
            if (dec.out_color_space != JCS_GRAYSCALE)
            {
                dec.out_color_space = JCS_EXT_RGBA;
            }
        #endif
            if (jpeg_start_decompress(&dec) == false)
                return E_FAIL;

            const auto img = *image.GetImage(0, 0, 0);

            std::vector<JSAMPROW> rows(dec.output_height);
            for (size_t i = 0u; i < dec.output_height; ++i)
            {
                rows[i] = img.pixels + (i * img.rowPitch);
            }

            JDIMENSION leftover = dec.output_height;
            while (leftover > 0)
            {
                JDIMENSION consumed = jpeg_read_scanlines(&dec, &rows[dec.output_scanline], leftover);
                leftover -= consumed;
            }

            if (jpeg_finish_decompress(&dec) == false)
                return E_FAIL;

        #ifndef LIBJPEG_TURBO_VERSION
            // if NOT TurboJPEG, we need to make 3 component images to 4 component image
            if (dec.out_color_space != JCS_GRAYSCALE)
            {
                ShiftPixels(img);
            }
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
        jpeg_error_mgr err;
        jpeg_compress_struct enc;
        JPEG_FLAGS userFlags;

    public:
        JPEGCompress(JPEG_FLAGS flags) : err{}, enc{}, userFlags(flags)
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
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                enc.input_components = 4;
                enc.in_color_space = JCS_EXT_RGBA;
                break;
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
                enc.input_components = 4;
                enc.in_color_space = JCS_EXT_BGRA;
                break;
            case DXGI_FORMAT_B8G8R8X8_UNORM:
            case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
                enc.input_components = 4;
                enc.in_color_space = JCS_EXT_BGRX;
                break;
            #else
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                enc.input_components = 3;
                enc.in_color_space = JCS_RGB;
                break;
            #endif

            default:
                return HRESULT_E_NOT_SUPPORTED;
            }

            // TODO: Add sRGB intent?

            enc.image_width = static_cast<JDIMENSION>(image.width);
            enc.image_height = static_cast<JDIMENSION>(image.height);
            jpeg_set_defaults(&enc);
            jpeg_set_quality(&enc, 100, true);

            // we will write a row each time ...
            jpeg_start_compress(&enc, true);

        #ifndef LIBJPEG_TURBO_VERSION
            if (enc.input_components == 3)
            {
                const size_t stride = enc.image_width * static_cast<size_t>(enc.input_components);
                auto scanline = std::make_unique<uint8_t[]>(stride);
                JSAMPROW rows[1]{ scanline.get() };

                while (enc.next_scanline < enc.image_height)
                {
                    // Copy 4 to 3 components
                    const uint8_t* src = image.pixels + enc.next_scanline * image.rowPitch;
                    uint8_t* dst = scanline.get();
                    for(size_t i=0; i < image.width; ++i)
                    {
                        dst[3*i + 0] = src[4*i + 0];
                        dst[3*i + 1] = src[4*i + 1];
                        dst[3*i + 2] = src[4*i + 2];
                    }

                    jpeg_write_scanlines(&enc, rows, 1);
                }
            }
            else
        #endif
            {
                while (enc.next_scanline < enc.image_height)
                {
                    JSAMPROW rows[1]{ image.pixels + image.rowPitch * enc.next_scanline };
                    jpeg_write_scanlines(&enc, rows, 1);
                }
            }

            jpeg_finish_compress(&enc);
            return S_OK;
        }
    };
}

_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromJPEGFile(
    const wchar_t* file,
    JPEG_FLAGS flags,
    TexMetadata& metadata)
{
    if (!file)
        return E_INVALIDARG;

    try
    {
        auto fin = OpenFILE(file);
        JPEGDecompress decoder(flags);
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
    JPEG_FLAGS flags,
    TexMetadata* metadata,
    ScratchImage&image)
{
    if (!file)
        return E_INVALIDARG;

    image.Release();

    try
    {
        auto fin = OpenFILE(file);
        JPEGDecompress decoder(flags);
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
    JPEG_FLAGS flags,
    const wchar_t* file)
{
    if (!file)
        return E_INVALIDARG;

    try
    {
        auto fout = CreateFILE(file);
        JPEGCompress encoder(flags);
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
