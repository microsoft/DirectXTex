//--------------------------------------------------------------------------------------
// File: WICTextureLoader9.h
//
// Function for loading a WIC image and creating a Direct3D runtime texture for it
// (auto-generating mipmaps if possible)
//
// Note: Assumes application has already called CoInitializeEx
//
// Note these functions are useful for images created as simple 2D textures. For
// more complex resources, DDSTextureLoader is an excellent light-weight runtime loader.
// For a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//--------------------------------------------------------------------------------------

#pragma once

#define DIRECT3D_VERSION 0x900
#include <d3d9.h>

#include <cstdint>

namespace DirectX
{
#ifndef WIC_LOADER_FLAGS_DEFINED
#define WIC_LOADER_FLAGS_DEFINED
    enum WIC_LOADER_FLAGS : uint32_t
    {
        WIC_LOADER_DEFAULT = 0,
        WIC_LOADER_MIP_AUTOGEN = 0x4,
        WIC_LOADER_FORCE_RGBA32 = 0x10,
    };
#endif

    // Standard version
    HRESULT CreateWICTextureFromMemory(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_reads_bytes_(wicDataSize) const uint8_t* wicData,
        _In_ size_t wicDataSize,
        _Outptr_ LPDIRECT3DTEXTURE9* texture,
        _In_ unsigned int loadFlags = 0) noexcept;

    HRESULT CreateWICTextureFromFile(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_z_ const wchar_t* szFileName,
        _Outptr_ LPDIRECT3DTEXTURE9* texture,
        _In_ unsigned int loadFlags = 0) noexcept;
}
