//-------------------------------------------------------------------------------------
// DirectXTexD3D12X.cpp
//  
// DirectXTex Auxilary functions for creating resouces from XboxImage containers
// via the CreatePlacedResourceX API
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//-------------------------------------------------------------------------------------

#include "DirectXTexP.h"
#include "DirectXTexXbox.h"

#if !(defined(_XBOX_ONE) && defined(_TITLE)) && !defined(_GAMING_XBOX)
#error This module only supports Xbox exclusive apps
#endif

#ifdef _GAMING_XBOX
#include <xmem.h>
#endif

using namespace Xbox;
using Microsoft::WRL::ComPtr;

namespace
{
    //--------------------------------------------------------------------------------------
    // Default XMemAlloc attributes for texture loading
    //--------------------------------------------------------------------------------------
    const uint64_t c_XMemAllocAttributes = MAKE_XALLOC_ATTRIBUTES(
        eXALLOCAllocatorId_MiddlewareReservedMin,
        0,
        XALLOC_MEMTYPE_GRAPHICS_WRITECOMBINE_GPU_READONLY,
        XALLOC_PAGESIZE_64KB,
        XALLOC_ALIGNMENT_64K
    #ifdef _GAMING_XBOX
        , 0
    #endif
    );
}

//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Create a texture resource
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Xbox::CreateTexture(
    ID3D12Device* d3dDevice,
    const XboxImage& xbox,
    ID3D12Resource** ppResource,
    void** grfxMemory)
{
    if (!d3dDevice || !ppResource || !grfxMemory)
        return E_INVALIDARG;

    *grfxMemory = nullptr;
    *ppResource = nullptr;

    if (!xbox.GetPointer() || !xbox.GetAlignment() || !xbox.GetSize() || xbox.GetTileMode() == c_XboxTileModeInvalid)
        return E_INVALIDARG;

    // Allocate graphics memory
    *grfxMemory = XMemAlloc(xbox.GetSize(), c_XMemAllocAttributes);
    if (!*grfxMemory)
        return E_OUTOFMEMORY;

    // Copy tiled data into graphics memory
    memcpy(*grfxMemory, xbox.GetPointer(), xbox.GetSize());

    // Create texture resource
    auto& metadata = xbox.GetMetadata();

    D3D12_RESOURCE_DESC desc = {};
    desc.Width = static_cast<UINT>(metadata.width);
    desc.Height = static_cast<UINT>(metadata.height);
    desc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
    desc.DepthOrArraySize = (metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE3D) ? static_cast<UINT16>(metadata.depth) : static_cast<UINT16>(metadata.arraySize);
    desc.Format = metadata.format;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
    desc.Layout = static_cast<D3D12_TEXTURE_LAYOUT>(0x100 | xbox.GetTileMode());

    HRESULT hr = d3dDevice->CreatePlacedResourceX(
        reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(*grfxMemory),
        &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_GRAPHICS_PPV_ARGS(ppResource));

    if (FAILED(hr))
    {
        XMemFree(*grfxMemory, c_XMemAllocAttributes);
        *grfxMemory = nullptr;
    }

    return hr;
}


//-------------------------------------------------------------------------------------
// Free allocated graphics memory
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
void Xbox::FreeTextureMemory(ID3D12Device* d3dDevice, void* grfxMemory)
{
    UNREFERENCED_PARAMETER(d3dDevice); // used only for overload resolution

    if (grfxMemory)
    {
        XMemFree(grfxMemory, c_XMemAllocAttributes);
    }
}
