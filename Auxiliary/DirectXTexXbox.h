//--------------------------------------------------------------------------------------
// File: DirectXTexXbox.h
//
// DirectXTex Auxilary functions for Xbox texture processing
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef _M_X64
#error This tool is only supported for x64 native
#endif

#include "DirectXTex.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonportable-system-include-path"
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

#if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
#include <xg_xs.h>
#else
#include <xg.h>
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef _GAMING_XBOX_SCARLETT
#include <d3d12_xs.h>
#elif defined(_GAMING_XBOX)
#include <d3d12_x.h>
#elif defined(_XBOX_ONE) && defined(_TITLE)
#include <d3d11_x.h>
#else
#include <d3d11_1.h>
#endif

#include <cstdint>
#include <utility>

#define DIRECTX_TEX_XBOX_VERSION 150

namespace Xbox
{
#if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
    using XboxTileMode = XG_SWIZZLE_MODE;
    constexpr XboxTileMode c_XboxTileModeInvalid = XG_SWIZZLE_MODE_INVALID;
    constexpr XboxTileMode c_XboxTileModeLinear = XG_SWIZZLE_MODE_LINEAR;
#else
    using XboxTileMode = XG_TILE_MODE;
    constexpr XboxTileMode c_XboxTileModeInvalid = XG_TILE_MODE_INVALID;
    constexpr XboxTileMode c_XboxTileModeLinear = XG_TILE_MODE_LINEAR;
#endif

    class XboxImage
    {
    public:
        XboxImage() noexcept
            : dataSize(0), baseAlignment(0), tilemode(c_XboxTileModeInvalid), metadata{}, memory(nullptr) {}
        XboxImage(XboxImage&& moveFrom) noexcept
            : dataSize(0), baseAlignment(0), tilemode(c_XboxTileModeInvalid), metadata{}, memory(nullptr) { *this = std::move(moveFrom); }
        ~XboxImage() { Release(); }

        XboxImage& __cdecl operator= (XboxImage&& moveFrom) noexcept;

        XboxImage(const XboxImage&) = delete;
        XboxImage& operator=(const XboxImage&) = delete;

        HRESULT __cdecl Initialize(_In_ const XG_TEXTURE1D_DESC& desc, _In_ const XG_RESOURCE_LAYOUT& layout, _In_ uint32_t miscFlags2 = 0);
        HRESULT __cdecl Initialize(_In_ const XG_TEXTURE2D_DESC& desc, _In_ const XG_RESOURCE_LAYOUT& layout, _In_ uint32_t miscFlags2 = 0);
        HRESULT __cdecl Initialize(_In_ const XG_TEXTURE3D_DESC& desc, _In_ const XG_RESOURCE_LAYOUT& layout, _In_ uint32_t miscFlags2 = 0);
        HRESULT __cdecl Initialize(_In_ const DirectX::TexMetadata& mdata, _In_ XboxTileMode tm, _In_ uint32_t size, _In_ uint32_t alignment);

        void __cdecl Release();

        const DirectX::TexMetadata& GetMetadata() const { return metadata; }
        XboxTileMode GetTileMode() const { return tilemode; }

        uint32_t GetSize() const { return dataSize; }
        uint32_t GetAlignment() const { return baseAlignment; }
        uint8_t* GetPointer() const { return memory; }

    private:
        uint32_t                dataSize;
        uint32_t                baseAlignment;
        XboxTileMode            tilemode;
        DirectX::TexMetadata    metadata;
        uint8_t*                memory;
    };

    //---------------------------------------------------------------------------------
    // Image I/O

    HRESULT __cdecl GetMetadataFromDDSMemory(
        _In_reads_bytes_(size) const uint8_t* pSource, _In_ size_t size,
        _Out_ DirectX::TexMetadata& metadata, _Out_ bool& isXbox);
    HRESULT __cdecl GetMetadataFromDDSFile(
        _In_z_ const wchar_t* szFile, _Out_ DirectX::TexMetadata& metadata, _Out_ bool& isXbox);

    HRESULT __cdecl GetMetadataFromDDSMemoryEx(
        _In_reads_bytes_(size) const uint8_t* pSource, _In_ size_t size,
        _Out_ DirectX::TexMetadata& metadata, _Out_ bool& isXbox,
        _Out_opt_ DirectX::DDSMetaData* ddPixelFormat);
    HRESULT __cdecl GetMetadataFromDDSFileEx(
        _In_z_ const wchar_t* szFile, _Out_ DirectX::TexMetadata& metadata, _Out_ bool& isXbox,
        _Out_opt_ DirectX::DDSMetaData* ddPixelFormat);

    HRESULT __cdecl LoadFromDDSMemory(
        _In_reads_bytes_(size) const uint8_t* pSource, _In_ size_t size,
        _Out_opt_ DirectX::TexMetadata* metadata, _Out_ XboxImage& image);
    HRESULT __cdecl LoadFromDDSFile(
        _In_z_ const wchar_t* szFile,
        _Out_opt_ DirectX::TexMetadata* metadata, _Out_ XboxImage& image);

    HRESULT __cdecl LoadFromDDSMemoryEx(
        _In_reads_bytes_(size) const uint8_t* pSource, _In_ size_t size,
        _Out_opt_ DirectX::TexMetadata* metadata,
        _Out_opt_ DirectX::DDSMetaData* ddPixelFormat,
        _Out_ XboxImage& image);
    HRESULT __cdecl LoadFromDDSFileEx(
        _In_z_ const wchar_t* szFile,
        _Out_opt_ DirectX::TexMetadata* metadata,
        _Out_opt_ DirectX::DDSMetaData* ddPixelFormat,
        _Out_ XboxImage& image);

    HRESULT __cdecl SaveToDDSMemory(_In_ const XboxImage& xbox, _Out_ DirectX::Blob& blob);
    HRESULT __cdecl SaveToDDSFile(_In_ const XboxImage& xbox, _In_z_ const wchar_t* szFile);

#ifdef __cpp_lib_byte
    inline HRESULT __cdecl GetMetadataFromDDSMemory(
        _In_reads_bytes_(size) const std::byte* pSource, _In_ size_t size,
        _Out_ DirectX::TexMetadata& metadata, _Out_ bool& isXbox)
    {
        return GetMetadataFromDDSMemory(reinterpret_cast<const uint8_t*>(pSource), size, metadata, isXbox);
    }

    inline HRESULT __cdecl GetMetadataFromDDSMemoryEx(
        _In_reads_bytes_(size) const std::byte* pSource, _In_ size_t size,
        _Out_ DirectX::TexMetadata& metadata, _Out_ bool& isXbox,
        _Out_opt_ DirectX::DDSMetaData* ddPixelFormat)
    {
        return GetMetadataFromDDSMemoryEx(reinterpret_cast<const uint8_t*>(pSource), size, metadata, isXbox, ddPixelFormat);
    }

    inline HRESULT __cdecl LoadFromDDSMemory(
        _In_reads_bytes_(size) const std::byte* pSource, _In_ size_t size,
        _Out_opt_ DirectX::TexMetadata* metadata, _Out_ XboxImage& image)
    {
        return LoadFromDDSMemory(reinterpret_cast<const uint8_t*>(pSource), size, metadata, image);
    }

    inline HRESULT __cdecl LoadFromDDSMemoryEx(
        _In_reads_bytes_(size) const std::byte* pSource, _In_ size_t size,
        _Out_opt_ DirectX::TexMetadata* metadata,
        _Out_opt_ DirectX::DDSMetaData* ddPixelFormat,
        _Out_ XboxImage& image)
    {
        return LoadFromDDSMemoryEx(reinterpret_cast<const uint8_t*>(pSource), size, metadata, ddPixelFormat, image);
    }
#endif // __cpp_lib_byte

    //---------------------------------------------------------------------------------
    // Xbox Texture Tiling / Detiling (requires XG DLL to be present at runtime)

    HRESULT __cdecl Tile(_In_ const DirectX::Image& srcImage, _Out_ XboxImage& xbox, _In_ XboxTileMode mode = c_XboxTileModeInvalid);
    HRESULT __cdecl Tile(
        _In_ const DirectX::Image* srcImages, _In_ size_t nimages, _In_ const DirectX::TexMetadata& metadata,
        _Out_ XboxImage& xbox, _In_ XboxTileMode mode = c_XboxTileModeInvalid);

    HRESULT __cdecl Detile(_In_ const XboxImage& xbox, _Out_ DirectX::ScratchImage& image);

    //---------------------------------------------------------------------------------
    // Direct3D 11.X functions

#if defined(_XBOX_ONE) && defined(_TITLE) && defined(__d3d11_x_h__)

    HRESULT __cdecl CreateTexture(
        _In_ ID3D11DeviceX* d3dDevice,
        _In_ const XboxImage& xbox, _Outptr_opt_ ID3D11Resource** ppResource, _Outptr_ void** grfxMemory);

    HRESULT __cdecl CreateShaderResourceView(
        _In_ ID3D11DeviceX* d3dDevice,
        _In_ const XboxImage& xbox, _Outptr_opt_ ID3D11ShaderResourceView** ppSRV, _Outptr_ void** grfxMemory);

    void __cdecl FreeTextureMemory(_In_ ID3D11DeviceX* d3dDevice, _In_opt_ void* grfxMemory);

#endif

    //---------------------------------------------------------------------------------
    // Direct3D 12.X functions

#if ((defined(_XBOX_ONE) && defined(_TITLE)) || defined(_GAMING_XBOX)) && (defined(__d3d12_x_h__) || defined(__XBOX_D3D12_X__))

    HRESULT __cdecl CreateTexture(
        _In_ ID3D12Device* d3dDevice,
        _In_ const XboxImage& xbox, _Outptr_opt_ ID3D12Resource** ppResource, _Outptr_ void** grfxMemory);

    void __cdecl FreeTextureMemory(_In_ ID3D12Device* d3dDevice, _In_opt_ void* grfxMemory);

#endif

    //---------------------------------------------------------------------------------
    // DDS helper functions
    HRESULT __cdecl EncodeDDSHeader(
        const XboxImage& xbox,
        _Out_writes_bytes_(maxsize) uint8_t* pDestination, _In_ size_t maxsize) noexcept;

#ifdef __cpp_lib_byte
    inline HRESULT __cdecl EncodeDDSHeader(
        const XboxImage& xbox,
        _Out_writes_bytes_(maxsize) std::byte* pDestination, _In_ size_t maxsize) noexcept
    {
        return EncodeDDSHeader(xbox, reinterpret_cast<uint8_t*>(pDestination), maxsize);
    }

    inline HRESULT __cdecl EncodeDDSHeader(
        const XboxImage& xbox,
        _Reserved_ std::nullptr_t, _In_ size_t maxsize) noexcept
    {
        return EncodeDDSHeader(xbox, static_cast<uint8_t*>(nullptr), maxsize);
    }
#endif

} // namespace
