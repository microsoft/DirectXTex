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
        std::ignore = fread(ptr, len, 1, fin);
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

        void Update(PNG_FLAGS& flags) noexcept(false)
        {
            png_read_info(st, info);

            // check for unsupported cases
            auto interlacing = png_get_interlace_type(st, info);
            if (interlacing != PNG_INTERLACE_NONE)
            {
                throw std::invalid_argument{ "interlacing not supported" };
            }

            // color handling
            auto color_type = png_get_color_type(st, info);
            if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
            {
                png_set_expand(st);
            }
            else if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            {
                flags |= PNG_FLAGS_IGNORE_SRGB;
                png_set_gray_to_rgb(st);
            }

            // bit-depth
            if (png_get_bit_depth(st, info) > 8)
            {
                png_set_swap(st);
            }

            // Request libpng the alpha change, but keep RGB untouched.
            png_set_alpha_mode(st, PNG_ALPHA_STANDARD, PNG_GAMMA_LINEAR);

            // Deal with custom color profiles
            if( png_get_valid( st, info, PNG_INFO_gAMA ) )
            {
                double gamma = 0;
                double screen_gamma = 2.2;

                if( png_get_gAMA( st, info, &gamma ) )
                {
                    // If gamma == 1.0, then the data is internally linear.
                    if( abs( gamma - 1.0 ) > 1e-6 )
                        png_set_gamma( st, screen_gamma, gamma );
                }
            }

            // make 4 component
            // using `png_set_add_alpha` here may confuse `TEX_ALPHA_MODE_OPAQUE` estimation
            if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_PALETTE)
                png_set_filler(st, png_get_bit_depth(st, info) == 16 ? 0xffff : 0xff, PNG_FILLER_AFTER);

            png_read_update_info(st, info);
        }

        /// @note must call `Update` before this
        DXGI_FORMAT GuessFormat(PNG_FLAGS flags) const noexcept(false)
        {
            auto c = png_get_channels(st, info);
            if (c == 1)
            {
                return (png_get_bit_depth(st, info) == 16) ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
            }

            // 8 or 16. expanded if 1, 2, 4
            auto d = png_get_bit_depth(st, info);
            if (d == 16)
                return DXGI_FORMAT_R16G16B16A16_UNORM;
            if (d != 8)
                throw std::runtime_error{ "unexpected info from libpng" };

            // RGB/BGR and sRGB or not
            DXGI_FORMAT linear = DXGI_FORMAT_R8G8B8A8_UNORM;
            DXGI_FORMAT srgb = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            if (flags & PNG_FLAGS_BGR)
            {
                png_set_bgr(st);
                linear = DXGI_FORMAT_B8G8R8A8_UNORM;
                srgb = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            }

            if (flags & PNG_FLAGS_IGNORE_SRGB)
                return linear;

            int intent = 0;
            if (png_get_sRGB(st, info, &intent) != 0)
                return srgb;

            if( png_get_valid( st, info, PNG_INFO_gAMA ) )
            {
                double gamma = 0;
                if( png_get_gAMA( st, info, &gamma ) )
                {
                    // This PNG is explicitly linear.
                    if( abs( gamma - 1.0 ) <= 1e-6 )
                        return linear;
                }
            }

            return (flags & PNG_FLAGS_DEFAULT_LINEAR) ? linear : srgb;
        }

        /// @todo More correct DXGI_FORMAT mapping
        void GetHeader(PNG_FLAGS flags, TexMetadata& metadata) noexcept(false)
        {
            metadata = {};
            metadata.width = png_get_image_width(st, info);
            metadata.height = png_get_image_height(st, info);
            metadata.arraySize = 1;
            metadata.mipLevels = 1;
            metadata.depth = 1;
            metadata.dimension = TEX_DIMENSION_TEXTURE2D;
            metadata.format = GuessFormat(flags);
            auto color_type = png_get_color_type(st, info);
            bool have_alpha = (color_type & PNG_COLOR_MASK_ALPHA);
            if (have_alpha == false
                && (metadata.format != DXGI_FORMAT_R8_UNORM)
                && (metadata.format != DXGI_FORMAT_R16_UNORM))
            {
                if (metadata.format == DXGI_FORMAT_B8G8R8A8_UNORM)
                    metadata.format = DXGI_FORMAT_B8G8R8X8_UNORM;
                else if (metadata.format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
                    metadata.format = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
                else
                    metadata.miscFlags2 |= TEX_ALPHA_MODE_OPAQUE;
            }
        }

        /// @todo More correct DXGI_FORMAT mapping
        HRESULT GetImage(PNG_FLAGS flags, TexMetadata& metadata, ScratchImage& image) noexcept(false)
        {
            metadata = {};
            GetHeader(flags, metadata);

            if (auto hr = image.Initialize2D(metadata.format, metadata.width, metadata.height, 1, 1); FAILED(hr))
                return hr;

            const auto img = *image.GetImage(0, 0, 0);
            if (!img.pixels)
                return E_POINTER;

            std::vector<png_byte*> rows(img.height);
            for (auto i = 0u; i < img.height; ++i)
            {
                rows[i] = img.pixels + (img.rowPitch * i);
            }
            png_read_rows(st, rows.data(), nullptr, static_cast<uint32_t>(rows.size()));
            png_read_end(st, info);
            return S_OK;
        }

        HRESULT GetImage(PNG_FLAGS flags, ScratchImage& image) noexcept(false)
        {
            TexMetadata metadata{};
            return GetImage(flags, metadata, image);
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

        HRESULT WriteImage(PNG_FLAGS flags, const Image& image) noexcept(false)
        {
            int color_type = PNG_COLOR_TYPE_RGB;
            bool using_bgr = false;
            bool using_srgb = false;
            int bit_depth = 8;
            bool strip_alpha = false;
            switch (image.format)
            {
            case DXGI_FORMAT_R8_UNORM:
                color_type = PNG_COLOR_TYPE_GRAY;
                break;

            case DXGI_FORMAT_R16_UNORM:
                color_type = PNG_COLOR_TYPE_GRAY;
                bit_depth = 16;
                break;

            case DXGI_FORMAT_B8G8R8A8_UNORM:
                using_bgr = true;
                [[fallthrough]];
            case DXGI_FORMAT_R8G8B8A8_UNORM:
                color_type = PNG_COLOR_TYPE_RGBA;
                break;

            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
                using_bgr = true;
                [[fallthrough]];
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                using_srgb = true;
                color_type = PNG_COLOR_TYPE_RGBA;
                break;

            case DXGI_FORMAT_R16G16B16A16_UNORM:
                color_type = PNG_COLOR_TYPE_RGBA;
                bit_depth = 16;
                break;

            case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
                using_srgb = true;
                [[fallthrough]];
            case DXGI_FORMAT_B8G8R8X8_UNORM:
                using_bgr = true;
                strip_alpha = true;
                color_type = PNG_COLOR_TYPE_RGB;
                break;

            default:
                return HRESULT_E_NOT_SUPPORTED;
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

            if (strip_alpha)
                png_set_filler(st, 0, PNG_FILLER_AFTER);
            if (using_bgr)
                png_set_bgr(st);
            if (bit_depth == 16)
                png_set_swap(st);

            if (color_type != PNG_COLOR_TYPE_GRAY)
            {
                if (flags & PNG_FLAGS_FORCE_LINEAR)
                {
                    png_set_gAMA(st, info, 1.0);
                }
                else if (using_srgb || (flags & PNG_FLAGS_FORCE_SRGB))
                {
                    png_set_sRGB(st, info, PNG_sRGB_INTENT_PERCEPTUAL);
                }
            }

            std::vector<png_bytep> rows(image.height);
            for (size_t i = 0u; i< image.height; ++i)
            {
                rows[i] = image.pixels + (image.rowPitch * i);
            }
            png_write_image(st, rows.data());
            png_write_end(st, info);
            return S_OK;
        }
    };
}

_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromPNGFile(
    const wchar_t* file,
    PNG_FLAGS flags,
    TexMetadata& metadata)
{
    if (!file)
        return E_INVALIDARG;

    try
    {
        auto fin = OpenFILE(file);
        PNGDecompress decoder{};
        decoder.UseInput(fin.get());
        decoder.Update(flags);
        decoder.GetHeader(flags, metadata);
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
    catch (const std::invalid_argument&)
    {
        return HRESULT_E_NOT_SUPPORTED;
    }
    catch (const std::exception&)
    {
        return E_FAIL;
    }
}

_Use_decl_annotations_
HRESULT DirectX::LoadFromPNGFile(
    const wchar_t* file,
    PNG_FLAGS flags,
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
        decoder.Update(flags);
        if (metadata == nullptr)
            return decoder.GetImage(flags, image);
        return decoder.GetImage(flags, *metadata, image);
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
    catch (const std::invalid_argument&)
    {
        return HRESULT_E_NOT_SUPPORTED;
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
    PNG_FLAGS flags,
    const wchar_t* file)
{
    if (!file)
        return E_INVALIDARG;

    try
    {
        auto fout = CreateFILE(file);
        PNGCompress encoder{};
        encoder.UseOutput(fout.get());
        return encoder.WriteImage(flags, image);
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
