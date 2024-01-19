//--------------------------------------------------------------------------------------
// File: DirectXTexPNG.cpp
//
// DirectXTex Auxilary functions for using the PNG(http://www.libpng.org/pub/png/libpng.html) library
//
// For the Windows platform, the strong recommendation is to make use of the WIC
// functions rather than using the open source library. This module exists to support
// Windows Subsystem on Linux.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "DirectXTexP.h"
#include "DirectXTexPNG.h"

#if __cplusplus < 201703L
#error Requires C++17 (and /Zc:__cplusplus with MSVC)
#endif

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <png.h>


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

    [[noreturn]] void OnPNGError(png_structp, png_const_charp msg)
    {
        throw std::runtime_error{ msg };
    }

    void OnPNGWarning(png_structp, png_const_charp)
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
            if (!st)
                throw std::runtime_error{ "png_create_read_struct" };
            info = png_create_info_struct(st);
            if (!info)
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
            // color handling
            png_byte color_type = png_get_color_type(st, info);
            if (color_type == PNG_COLOR_TYPE_GRAY)
            {
                // bit_depth will be 8 or 16
                if (png_get_bit_depth(st, info) < 8)
                    png_set_expand_gray_1_2_4_to_8(st);
            }
            else if (color_type == PNG_COLOR_TYPE_PALETTE)
            {
                // request RGB color
                png_set_palette_to_rgb(st);
                if (png_get_valid(st, info, PNG_INFO_tRNS))
                    png_set_tRNS_to_alpha(st);
            }
            // Here we don't know if the pixel data is in BGR/RGB order
            // png_set_bgr(st);
            png_set_alpha_mode(st, PNG_ALPHA_STANDARD, PNG_DEFAULT_sRGB);
            // make 4 component
            // using `png_set_add_alpha` here may confuse `TEX_ALPHA_MODE_OPAQUE` estimation
            if (png_get_channels(st, info) == 3)
                png_set_filler(st, 0, PNG_FILLER_AFTER);
            // prefer DXGI_FORMAT_R8G8B8A8_UNORM. strip in decode
            //if (png_get_bit_depth(st, info) > 8)
            //    png_set_strip_16(st);
            png_read_update_info(st, info);
        }

        /// @note must call `Update` before this
        /// @todo Proper detection of DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
        DXGI_FORMAT GuessFormat() const noexcept(false)
        {
            // 1 or 4. 1 is for gray
            auto c = png_get_channels(st, info);
            if (c == 1)
            {
                if (png_get_bit_depth(st, info) == 16)
                    return DXGI_FORMAT_R16_UNORM;
                // with `png_set_expand_gray_1_2_4_to_8`, libpng will change to R8_UNORM
                return DXGI_FORMAT_R8_UNORM;
            }

            // 8 or 16. expanded if 1, 2, 4
            auto d = png_get_bit_depth(st, info);
            if (d == 16)
                return DXGI_FORMAT_R16G16B16A16_UNORM;
            if (d != 8)
                throw std::runtime_error{ "unexpected info from libpng" };

            int intent = 0;
            if (png_get_sRGB(st, info, &intent) == PNG_INFO_sRGB)
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        /// @todo More correct DXGI_FORMAT mapping
        void GetHeader(TexMetadata& metadata) noexcept(false)
        {
            metadata.width = png_get_image_width(st, info);
            metadata.height = png_get_image_height(st, info);
            metadata.arraySize = 1;
            metadata.mipLevels = 1;
            metadata.depth = 1;
            metadata.dimension = TEX_DIMENSION_TEXTURE2D;
            metadata.format = GuessFormat();
            png_byte color_type = png_get_color_type(st, info);
            bool have_alpha = (color_type & PNG_COLOR_MASK_ALPHA);
            if (have_alpha == false)
                metadata.miscFlags2 |= TEX_ALPHA_MODE_OPAQUE;
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
            if (!st)
                throw std::runtime_error{ "png_create_write_struct" };
            info = png_create_info_struct(st);
            if (!info)
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
            int color_type = PNG_COLOR_TYPE_RGB;
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
                color_type = PNG_COLOR_TYPE_RGBA;
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

            const size_t stride = static_cast<size_t>(channel) * image.width;
            std::vector<png_bytep> rows(image.height);
            for (size_t i = 0u; i< image.height; ++i)
                rows[i] = image.pixels + stride * i;
            png_write_rows(st, rows.data(), static_cast<uint32_t>(rows.size()));

            // actual write will be done here
            png_write_end(st, info);
            return S_OK;
        }
    };
}

_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromPNGFile(
    const wchar_t* file,
    TexMetadata& metadata)
{
    if (!file)
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
HRESULT DirectX::LoadFromPNGFile(
    const wchar_t* file,
    TexMetadata* metadata,
    ScratchImage& image)
{
    if (!file)
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
HRESULT DirectX::SaveToPNGFile(
    const Image& image,
    const wchar_t* file)
{
    if (!file)
        return E_INVALIDARG;

    try
    {
        auto fout = CreateFILE(file);
        PNGCompress encoder{};
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
