//-------------------------------------------------------------------------------------
// DirectXTex.inl
//
// DirectX Texture Library
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#pragma once

//=====================================================================================
// Bitmask flags enumerator operators
//=====================================================================================
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#endif

DEFINE_ENUM_FLAG_OPERATORS(CP_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(DDS_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(TGA_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(WIC_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(TEX_FR_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(TEX_FILTER_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(TEX_PMALPHA_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(TEX_COMPRESS_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(CNMAP_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(CMSE_FLAGS);
DEFINE_ENUM_FLAG_OPERATORS(CREATETEX_FLAGS);

#ifdef __clang__
#pragma clang diagnostic pop
#endif

// WIC_FILTER modes match TEX_FILTER modes
constexpr WIC_FLAGS operator|(WIC_FLAGS a, TEX_FILTER_FLAGS b) { return static_cast<WIC_FLAGS>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b & TEX_FILTER_MODE_MASK)); }
constexpr WIC_FLAGS operator|(TEX_FILTER_FLAGS a, WIC_FLAGS b) { return static_cast<WIC_FLAGS>(static_cast<uint32_t>(a & TEX_FILTER_MODE_MASK) | static_cast<uint32_t>(b)); }

// TEX_PMALPHA_SRGB match TEX_FILTER_SRGB
constexpr TEX_PMALPHA_FLAGS operator|(TEX_PMALPHA_FLAGS a, TEX_FILTER_FLAGS b) { return static_cast<TEX_PMALPHA_FLAGS>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b & TEX_FILTER_SRGB_MASK)); }
constexpr TEX_PMALPHA_FLAGS operator|(TEX_FILTER_FLAGS a, TEX_PMALPHA_FLAGS b) { return static_cast<TEX_PMALPHA_FLAGS>(static_cast<uint32_t>(a & TEX_FILTER_SRGB_MASK) | static_cast<uint32_t>(b)); }

// TEX_COMPRESS_SRGB match TEX_FILTER_SRGB
constexpr TEX_COMPRESS_FLAGS operator|(TEX_COMPRESS_FLAGS a, TEX_FILTER_FLAGS b) { return static_cast<TEX_COMPRESS_FLAGS>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b & TEX_FILTER_SRGB_MASK)); }
constexpr TEX_COMPRESS_FLAGS operator|(TEX_FILTER_FLAGS a, TEX_COMPRESS_FLAGS b) { return static_cast<TEX_COMPRESS_FLAGS>(static_cast<uint32_t>(a & TEX_FILTER_SRGB_MASK) | static_cast<uint32_t>(b)); }


//=====================================================================================
// DXGI Format Utilities
//=====================================================================================

_Use_decl_annotations_
constexpr bool __cdecl IsValid(DXGI_FORMAT fmt) noexcept
{
    return (static_cast<size_t>(fmt) >= 1 && static_cast<size_t>(fmt) <= 191);
}

_Use_decl_annotations_
inline bool __cdecl IsCompressed(DXGI_FORMAT fmt) noexcept
{
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return true;

    default:
        return false;
    }
}

_Use_decl_annotations_
inline bool __cdecl IsPalettized(DXGI_FORMAT fmt) noexcept
{
    switch (fmt)
    {
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    case DXGI_FORMAT_A8P8:
        return true;

    default:
        return false;
    }
}

_Use_decl_annotations_
inline bool __cdecl IsSRGB(DXGI_FORMAT fmt) noexcept
{
    switch (fmt)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return true;

    default:
        return false;
    }
}


//=====================================================================================
// Image I/O
//=====================================================================================
_Use_decl_annotations_
inline HRESULT __cdecl SaveToDDSMemory(const Image& image, DDS_FLAGS flags, Blob& blob) noexcept
{
    TexMetadata mdata = {};
    mdata.width = image.width;
    mdata.height = image.height;
    mdata.depth = 1;
    mdata.arraySize = 1;
    mdata.mipLevels = 1;
    mdata.format = image.format;
    mdata.dimension = TEX_DIMENSION_TEXTURE2D;

    return SaveToDDSMemory(&image, 1, mdata, flags, blob);
}

_Use_decl_annotations_
inline HRESULT __cdecl SaveToDDSFile(const Image& image, DDS_FLAGS flags, const wchar_t* szFile) noexcept
{
    TexMetadata mdata = {};
    mdata.width = image.width;
    mdata.height = image.height;
    mdata.depth = 1;
    mdata.arraySize = 1;
    mdata.mipLevels = 1;
    mdata.format = image.format;
    mdata.dimension = TEX_DIMENSION_TEXTURE2D;

    return SaveToDDSFile(&image, 1, mdata, flags, szFile);
}


//=====================================================================================
// Compatability helpers
//=====================================================================================
_Use_decl_annotations_
inline HRESULT __cdecl GetMetadataFromTGAMemory(const uint8_t* pSource, size_t size, TexMetadata& metadata) noexcept
{
    return GetMetadataFromTGAMemory(pSource, size, TGA_FLAGS_NONE, metadata);
}

_Use_decl_annotations_
inline HRESULT __cdecl GetMetadataFromTGAFile(const wchar_t* szFile, TexMetadata& metadata) noexcept
{
    return GetMetadataFromTGAFile(szFile, TGA_FLAGS_NONE, metadata);
}

_Use_decl_annotations_
inline HRESULT __cdecl LoadFromTGAMemory(const uint8_t* pSource, size_t size, TexMetadata* metadata, ScratchImage& image) noexcept
{
    return LoadFromTGAMemory(pSource, size, TGA_FLAGS_NONE, metadata, image);
}

_Use_decl_annotations_
inline HRESULT __cdecl LoadFromTGAFile(const wchar_t* szFile, TexMetadata* metadata, ScratchImage& image) noexcept
{
    return LoadFromTGAFile(szFile, TGA_FLAGS_NONE, metadata, image);
}

_Use_decl_annotations_
inline HRESULT __cdecl SaveToTGAMemory(const Image& image, Blob& blob, const TexMetadata* metadata) noexcept
{
    return SaveToTGAMemory(image, TGA_FLAGS_NONE, blob, metadata);
}

_Use_decl_annotations_
inline HRESULT __cdecl SaveToTGAFile(const Image& image, const wchar_t* szFile, const TexMetadata* metadata) noexcept
{
    return SaveToTGAFile(image, TGA_FLAGS_NONE, szFile, metadata);
}


//=====================================================================================
// C++17 helpers
//=====================================================================================
#ifdef __cpp_lib_byte

_Use_decl_annotations_
inline HRESULT __cdecl GetMetadataFromDDSMemory(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata& metadata) noexcept
{
    return GetMetadataFromDDSMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata);
}

_Use_decl_annotations_
inline HRESULT __cdecl LoadFromDDSMemory(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata* metadata, ScratchImage& image) noexcept
{
    return LoadFromDDSMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, image);
}

_Use_decl_annotations_
inline HRESULT __cdecl GetMetadataFromDDSMemoryEx(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata& metadata, DDSMetaData* ddPixelFormat) noexcept
{
    return GetMetadataFromDDSMemoryEx(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, ddPixelFormat);
}

_Use_decl_annotations_
inline HRESULT __cdecl LoadFromDDSMemoryEx(const std::byte* pSource, size_t size, DDS_FLAGS flags, TexMetadata* metadata, DDSMetaData* ddPixelFormat, ScratchImage& image) noexcept
{
    return LoadFromDDSMemoryEx(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, ddPixelFormat, image);
}

_Use_decl_annotations_
inline HRESULT __cdecl GetMetadataFromHDRMemory(const std::byte* pSource, size_t size, TexMetadata& metadata) noexcept
{
    return GetMetadataFromHDRMemory(reinterpret_cast<const uint8_t*>(pSource), size, metadata);
}

_Use_decl_annotations_
inline HRESULT __cdecl LoadFromHDRMemory(const std::byte* pSource, size_t size, TexMetadata* metadata, ScratchImage& image) noexcept
{
    return LoadFromHDRMemory(reinterpret_cast<const uint8_t*>(pSource), size, metadata, image);
}

_Use_decl_annotations_
inline HRESULT __cdecl GetMetadataFromTGAMemory(const std::byte* pSource, size_t size, TGA_FLAGS flags, TexMetadata& metadata) noexcept
{
    return GetMetadataFromTGAMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata);
}

_Use_decl_annotations_
inline HRESULT __cdecl LoadFromTGAMemory(const std::byte* pSource, size_t size, TGA_FLAGS flags, TexMetadata* metadata, ScratchImage& image) noexcept
{
    return LoadFromTGAMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, image);
}

_Use_decl_annotations_
inline HRESULT __cdecl EncodeDDSHeader(const TexMetadata& metadata, DDS_FLAGS flags, std::byte* pDestination, size_t maxsize, size_t& required) noexcept
{
    return EncodeDDSHeader(metadata, flags, reinterpret_cast<uint8_t*>(pDestination), maxsize, required);
}

_Use_decl_annotations_
inline HRESULT __cdecl EncodeDDSHeader(const TexMetadata& metadata, DDS_FLAGS flags, std::nullptr_t, size_t maxsize, size_t& required) noexcept
{
    return EncodeDDSHeader(metadata, flags, static_cast<uint8_t*>(nullptr), maxsize, required);
}

#ifdef _WIN32
_Use_decl_annotations_
inline HRESULT __cdecl GetMetadataFromWICMemory(const std::byte* pSource, size_t size, WIC_FLAGS flags, TexMetadata& metadata, std::function<void __cdecl(IWICMetadataQueryReader*)> getMQR)
{
    return GetMetadataFromWICMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, getMQR);
}

_Use_decl_annotations_
inline HRESULT __cdecl LoadFromWICMemory(const std::byte* pSource, size_t size, WIC_FLAGS flags, TexMetadata* metadata, ScratchImage& image, std::function<void __cdecl(IWICMetadataQueryReader*)> getMQR)
{
    return LoadFromWICMemory(reinterpret_cast<const uint8_t*>(pSource), size, flags, metadata, image, getMQR);
}
#endif // _WIN32

#endif // __cpp_lib_byte
