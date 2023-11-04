//--------------------------------------------------------------------------------------
// File: DirectXTexPNG.cpp
//
// DirectXTex Auxillary functions for using the PNG(http://www.libpng.org/pub/png/libpng.html) library
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "DirectXTexP.h"

#include "DirectXTexPNG.h"

#include <filesystem>
#include <cstdio>
#include <png.h>

namespace DirectX
{
    using std::filesystem::path;

    std::unique_ptr<FILE, int(*)(FILE*)> OpenFILE(const path& p) noexcept(false);
    std::unique_ptr<FILE, int(*)(FILE*)> CreateFILE(const path& p) noexcept(false);

    [[noreturn]] void OnPNGError(png_structp, png_const_charp msg)
    {
        throw std::runtime_error{ msg };
    }

    [[noreturn]] void OnPNGWarning(png_structp, png_const_charp)
    {
        // drain warning messages ...
    }

    /// @note If the PNG contains some extra chunks like EXIF, this will be used
    void OnPNGRead(png_structp st, png_bytep ptr, size_t len)
    {
        FILE* fin = reinterpret_cast<FILE*>(png_get_io_ptr(st));
        fread(ptr, len, 1, fin);
    }


    /// @see http://www.libpng.org/pub/png/libpng.html
    /// @see http://www.libpng.org/pub/png/libpng-manual.txt
    class PNGDecompress final
    {
        png_structp st;
        png_infop info;

    public:
        PNGDecompress() noexcept(false) : st{ nullptr }, info{ nullptr }
        {
            st = png_create_read_struct(PNG_LIBPNG_VER_STRING, this, &OnPNGError, &OnPNGWarning);
            if (st == nullptr)
                throw std::runtime_error{ "png_create_read_struct" };
            info = png_create_info_struct(st);
            if (info == nullptr)
            {
                png_destroy_read_struct(&st, nullptr, nullptr);
                throw std::runtime_error{ "png_create_info_struct" };
            }
        }

        ~PNGDecompress() noexcept
        {
            png_destroy_read_struct(&st, &info, nullptr);
        }

        void UseInput(FILE* fin) noexcept
        {
            // we assume the signature is unread. if so, we have to use `png_set_sig_bytes`
            png_init_io(st, fin);
            png_set_read_fn(st, fin, &OnPNGRead);
        }

        void Update() noexcept(false)
        {
            png_read_info(st, info);
            if (png_get_color_type(st, info) == PNG_COLOR_TYPE_RGB)
                png_set_add_alpha(st, 0, PNG_FILLER_AFTER);
            if (png_get_bit_depth(st, info) > 8)
                png_set_strip_16(st);
            png_read_update_info(st, info);
        }

        /// @todo More correct DXGI_FORMAT mapping
        void GetHeader(TexMetadata& metadata) noexcept(false)
        {
            metadata.width = png_get_image_width(st, info);
            metadata.height = png_get_image_height(st, info);
            metadata.arraySize = 1;
            metadata.mipLevels = 1;
            metadata.dimension = TEX_DIMENSION_TEXTURE2D;
            auto c = png_get_channels(st, info);
            auto d = png_get_bit_depth(st, info);
            switch (c)
            {
            case 1:
                metadata.format = DXGI_FORMAT_R8_UNORM;
                break;
            case 4:
                metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM;
                break;
            case 3:
            default:
                throw std::runtime_error{ "png_get_channels" };
            }
            metadata.depth = d / 8; // expect 1
        }

        /// @todo More correct DXGI_FORMAT mapping
        HRESULT GetImage(TexMetadata& metadata, ScratchImage& image) noexcept(false)
        {
            metadata = {};
            GetHeader(metadata);

            if (auto hr = image.Initialize2D(metadata.format, metadata.width, metadata.height, metadata.arraySize, metadata.mipLevels); FAILED(hr))
                return hr;
            auto* dest = image.GetPixels();
            const auto stride = metadata.width * png_get_channels(st, info);
            std::vector<png_byte*> rows(metadata.height);
            for (auto i = 0u; i < metadata.height; ++i)
                rows[i] = dest + (stride * i);

            png_read_rows(st, rows.data(), nullptr, static_cast<uint32_t>(rows.size()));
            png_read_end(st, info);
            return S_OK;
        }

        HRESULT GetImage(ScratchImage& image) noexcept(false)
        {
            TexMetadata metadata{};
            return GetImage(metadata, image);
        }
    };

    /// @see http://www.libpng.org/pub/png/libpng.html
    /// @see http://www.libpng.org/pub/png/libpng-manual.txt
    class PNGCompress final
    {
        png_structp st;
        png_infop info;

    public:
        PNGCompress() noexcept(false) : st{ nullptr }, info{ nullptr }
        {
            st = png_create_write_struct(PNG_LIBPNG_VER_STRING, this, &OnPNGError, &OnPNGWarning);
            if (st == nullptr)
                throw std::runtime_error{ "png_create_write_struct" };
            info = png_create_info_struct(st);
            if (info == nullptr)
            {
                png_destroy_write_struct(&st, nullptr);
                throw std::runtime_error{ "png_create_info_struct" };
            }
            png_set_compression_level(st, 0);
        }

        ~PNGCompress() noexcept
        {
            png_destroy_write_struct(&st, &info);
        }

        void UseOutput(FILE* fout) noexcept
        {
            png_init_io(st, fout);
        }

        HRESULT WriteImage(const Image& image) noexcept(false)
        {
            int color_type = PNG_COLOR_TYPE_RGBA;
            bool using_bgr = false;
            int channel = 4;
            int bit_depth = 8;
            switch (image.format)
            {
            case DXGI_FORMAT_R8_UNORM:
                color_type = PNG_COLOR_TYPE_GRAY;
                break;
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            case DXGI_FORMAT_B8G8R8X8_UNORM:
                using_bgr = true;
                [[fallthrough]];
            case DXGI_FORMAT_R8G8B8A8_UNORM:
                channel = 4;
                bit_depth = 8;
                break;
            default:
                return E_INVALIDARG;
            }

            png_set_IHDR(st, info,
                static_cast<uint32_t>(image.width),
                static_cast<uint32_t>(image.height),
                bit_depth,
                color_type,
                PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);
            png_write_info(st, info);
            if (using_bgr)
                png_set_bgr(st);

            const size_t stride = channel * image.width;
            std::vector<png_bytep> rows(image.height);
            for (size_t i = 0u; i< image.height; ++i)
                rows[i] = image.pixels + stride * i;
            png_write_rows(st, rows.data(), static_cast<uint32_t>(rows.size()));

            // actual write will be done here
            png_write_end(st, info);
            return S_OK;
        }
    };

    HRESULT __cdecl GetMetadataFromPNGFile(
        _In_z_ const wchar_t* file,
        _Out_ TexMetadata& metadata)
    {
        if (file == nullptr)
            return E_INVALIDARG;
        try
        {
            auto fin = OpenFILE(file);
            PNGDecompress decoder{};
            decoder.UseInput(fin.get());
            decoder.Update();
            decoder.GetHeader(metadata);
            return S_OK;
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

    HRESULT __cdecl LoadFromPNGFile(
        _In_z_ const wchar_t* file,
        _Out_opt_ TexMetadata* metadata,
        _Out_ ScratchImage& image)
    {
        if (file == nullptr)
            return E_INVALIDARG;
        image.Release();

        try
        {
            auto fin = OpenFILE(file);
            PNGDecompress decoder{};
            decoder.UseInput(fin.get());
            decoder.Update();
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

    HRESULT __cdecl SaveToPNGFile(_In_ const Image& image, _In_z_ const wchar_t* file)
    {
        if (file == nullptr)
            return E_INVALIDARG;
        try
        {
            auto fout = CreateFILE(file);
            PNGCompress encoder{};
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
