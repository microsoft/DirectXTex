//--------------------------------------------------------------------------------------
// File: DDSTextureLoader12.h
//
// Functions for loading a DDS texture and creating a Direct3D runtime resource for it
//
// Note these functions are useful as a light-weight runtime loader for DDS files. For
// a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#pragma once

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#ifndef _WIN32
#include <wsl/winadapter.h>
#include <wsl/wrladapter.h>
#endif

#if !defined(_WIN32) || defined(USING_DIRECTX_HEADERS)
#include <directx/d3d12.h>
#include <dxguids/dxguids.h>
#else
#include <d3d12.h>
#pragma comment(lib,"dxguid.lib")
#endif

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>


namespace DirectX
{
#ifndef DDS_ALPHA_MODE_DEFINED
#define DDS_ALPHA_MODE_DEFINED
    enum DDS_ALPHA_MODE : uint32_t
    {
        DDS_ALPHA_MODE_UNKNOWN = 0,
        DDS_ALPHA_MODE_STRAIGHT = 1,
        DDS_ALPHA_MODE_PREMULTIPLIED = 2,
        DDS_ALPHA_MODE_OPAQUE = 3,
        DDS_ALPHA_MODE_CUSTOM = 4,
    };

#endif

#ifndef DDS_LOADER_FLAGS_DEFINED
#define DDS_LOADER_FLAGS_DEFINED

    enum DDS_LOADER_FLAGS : uint32_t
    {
        DDS_LOADER_DEFAULT = 0,
        DDS_LOADER_FORCE_SRGB = 0x1,
        DDS_LOADER_IGNORE_SRGB = 0x2,
        DDS_LOADER_MIP_RESERVE = 0x8,
    };

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
#endif

    DEFINE_ENUM_FLAG_OPERATORS(DDS_LOADER_FLAGS);

#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif

    // Standard version
    HRESULT __cdecl LoadDDSTextureFromMemory(
        _In_ ID3D12Device* d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        size_t ddsDataSize,
        _Outptr_ ID3D12Resource** texture,
        std::vector<D3D12_SUBRESOURCE_DATA>& subresources,
        size_t maxsize = 0,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr,
        _Out_opt_ bool* isCubeMap = nullptr);

    HRESULT __cdecl LoadDDSTextureFromFile(
        _In_ ID3D12Device* d3dDevice,
        _In_z_ const wchar_t* szFileName,
        _Outptr_ ID3D12Resource** texture,
        std::unique_ptr<uint8_t[]>& ddsData,
        std::vector<D3D12_SUBRESOURCE_DATA>& subresources,
        size_t maxsize = 0,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr,
        _Out_opt_ bool* isCubeMap = nullptr);

    // Extended version
    HRESULT __cdecl LoadDDSTextureFromMemoryEx(
        _In_ ID3D12Device* d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        size_t ddsDataSize,
        size_t maxsize,
        D3D12_RESOURCE_FLAGS resFlags,
        DDS_LOADER_FLAGS loadFlags,
        _Outptr_ ID3D12Resource** texture,
        std::vector<D3D12_SUBRESOURCE_DATA>& subresources,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr,
        _Out_opt_ bool* isCubeMap = nullptr);

    HRESULT __cdecl LoadDDSTextureFromFileEx(
        _In_ ID3D12Device* d3dDevice,
        _In_z_ const wchar_t* szFileName,
        size_t maxsize,
        D3D12_RESOURCE_FLAGS resFlags,
        DDS_LOADER_FLAGS loadFlags,
        _Outptr_ ID3D12Resource** texture,
        std::unique_ptr<uint8_t[]>& ddsData,
        std::vector<D3D12_SUBRESOURCE_DATA>& subresources,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr,
        _Out_opt_ bool* isCubeMap = nullptr);
}
