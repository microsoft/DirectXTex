//--------------------------------------------------------------------------------------
// File: DirectXTexJPEG.cpp
//
// DirectXTex Auxillary functions for using the JPEG(https://www.ijg.org) library
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "DirectXTexP.h"

#include "DirectXTexJPEG.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#if !defined(_BASETSD_H_)
#define _BASETSD_H_
#endif
#include <jpeglib.h>
#include <jerror.h>

namespace DirectX
{
    using std::filesystem::path;

    std::unique_ptr<FILE, int(*)(FILE*)> OpenFILE(const path& p) noexcept(false);
    std::unique_ptr<FILE, int(*)(FILE*)> CreateFILE(const path& p) noexcept(false);

    void OnJPEGError(j_common_ptr ptr)
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
            #if defined(LIBJPEG_TURBO_VERSION)
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
            if (auto hr = image.Initialize2D(metadata.format, metadata.width, metadata.height, metadata.arraySize, metadata.mipLevels); FAILED(hr))
                return hr;

        #if defined(LIBJPEG_TURBO_VERSION)
            dec.out_color_space = JCS_EXT_RGBX;
        #endif
            if (jpeg_start_decompress(&dec) == false)
                return E_FAIL;

            uint8_t* dest = image.GetPixels();
            const size_t stride = dec.output_width * dec.output_components;
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
            #if defined(LIBJPEG_TURBO_VERSION)
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
            const size_t stride = enc.image_width * enc.input_components;
            while (enc.next_scanline < enc.image_height)
            {
                JSAMPROW rows[1]{ image.pixels + stride * enc.next_scanline };
                jpeg_write_scanlines(&enc, rows, 1);
            }
            jpeg_finish_compress(&enc);
            return S_OK;
        }
    };

    HRESULT __cdecl GetMetadataFromJPEGFile(
        _In_z_ const wchar_t* file,
        _Out_ TexMetadata& metadata)
    {
        if (file == nullptr)
            return E_INVALIDARG;
        try
        {
            auto fin = OpenFILE(file);
            JPEGDecompress decoder{};
            decoder.UseInput(fin.get());
            return decoder.GetHeader(metadata);
        }
        catch (const std::system_error& ec)
        {
            return ec.code().value();
        }
        catch (const std::exception&)
        {
            return E_FAIL;
        }
    }

    HRESULT __cdecl LoadFromJPEGFile(
        _In_z_ const wchar_t* file,
        _Out_opt_ TexMetadata* metadata,
        _Out_ ScratchImage&image)
    {
        if (file == nullptr)
            return E_INVALIDARG;
        image.Release();

        try
        {
            auto fin = OpenFILE(file);
            JPEGDecompress decoder{};
            decoder.UseInput(fin.get());
            if (metadata == nullptr)
                return decoder.GetImage(image);
            return decoder.GetImage(*metadata, image);
        }
        catch (const std::system_error& ec)
        {
            return ec.code().value();
        }
        catch (const std::exception&)
        {
            return E_FAIL;
        }
    }

    HRESULT __cdecl SaveToJPEGFile(_In_ const Image& image, _In_z_ const wchar_t* file)
    {
        if (file == nullptr)
            return E_INVALIDARG;
        try
        {
            auto fout = CreateFILE(file);
            JPEGCompress encoder{};
            encoder.UseOutput(fout.get());
            return encoder.WriteImage(image);
        }
        catch (const std::system_error& ec)
        {
            return ec.code().value();
        }
        catch (const std::exception&)
        {
            return E_FAIL;
        }
    }
}
