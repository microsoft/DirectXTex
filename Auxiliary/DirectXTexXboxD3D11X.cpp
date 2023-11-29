//-------------------------------------------------------------------------------------
// DirectXTexD3D11X.cpp
//  
// DirectXTex Auxillary functions for creating resouces from XboxImage containers
// via the CreatePlacement APIs
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//-------------------------------------------------------------------------------------

#include "DirectXTexP.h"
#include "DirectXTexXbox.h"

#ifdef _GAMING_XBOX
#error This module is not supported for GDK
#elif !defined(_XBOX_ONE) || !defined(_TITLE)
#error This module only supports Xbox One exclusive apps
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
        XALLOC_ALIGNMENT_64K);
}

//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Create a texture resource
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Xbox::CreateTexture(
    ID3D11DeviceX* d3dDevice,
    const XboxImage& xbox,
    ID3D11Resource** ppResource,
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

    HRESULT hr = E_FAIL;

    switch (metadata.dimension)
    {
    case DirectX::TEX_DIMENSION_TEXTURE1D:
        {
            D3D11_TEXTURE1D_DESC desc = {};
            desc.Width = static_cast<UINT>(metadata.width);
            desc.MipLevels = static_cast<UINT>(metadata.mipLevels);
            desc.ArraySize = static_cast<UINT>(metadata.arraySize);
            desc.Format = metadata.format;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            ID3D11Texture1D* tex = nullptr;
            hr = d3dDevice->CreatePlacementTexture1D(&desc, xbox.GetTileMode(), 0, *grfxMemory, &tex);
            if (SUCCEEDED(hr) && tex)
            {
                *ppResource = tex;
            }
        }
        break;

    case DirectX::TEX_DIMENSION_TEXTURE2D:
        {
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = static_cast<UINT>(metadata.width);
            desc.Height = static_cast<UINT>(metadata.height);
            desc.MipLevels = static_cast<UINT>(metadata.mipLevels);
            desc.ArraySize = static_cast<UINT>(metadata.arraySize);
            desc.Format = metadata.format;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.MiscFlags = (metadata.miscFlags & DirectX::TEX_MISC_TEXTURECUBE) ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

            ID3D11Texture2D* tex = nullptr;
            hr = d3dDevice->CreatePlacementTexture2D(&desc, xbox.GetTileMode(), 0, *grfxMemory, &tex);
            if (SUCCEEDED(hr) && tex)
            {
                *ppResource = tex;
            }
        }
        break;

    case DirectX::TEX_DIMENSION_TEXTURE3D:
        {
            D3D11_TEXTURE3D_DESC desc = {};
            desc.Width = static_cast<UINT>(metadata.width);
            desc.Height = static_cast<UINT>(metadata.height);
            desc.Depth = static_cast<UINT>(metadata.depth);
            desc.MipLevels = static_cast<UINT>(metadata.mipLevels);
            desc.Format = metadata.format;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            ID3D11Texture3D* tex = nullptr;
            hr = d3dDevice->CreatePlacementTexture3D(&desc, xbox.GetTileMode(), 0, *grfxMemory, &tex);
            if (SUCCEEDED(hr) && tex)
            {
                *ppResource = tex;
            }
        }
        break;

    default:
        hr = E_FAIL;
        break;
    }

    if (FAILED(hr))
    {
        XMemFree(grfxMemory, c_XMemAllocAttributes);
        *grfxMemory = nullptr;
    }

    return hr;
}


//-------------------------------------------------------------------------------------
// Create a shader resource view and associated texture
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT Xbox::CreateShaderResourceView(
    ID3D11DeviceX* d3dDevice,
    const XboxImage& xbox,
    ID3D11ShaderResourceView** ppSRV,
    void** grfxMemory)
{
    if (!ppSRV)
        return E_INVALIDARG;

    *ppSRV = nullptr;

    ComPtr<ID3D11Resource> resource;
    HRESULT hr = CreateTexture(d3dDevice, xbox, resource.GetAddressOf(), grfxMemory);
    if (FAILED(hr))
        return hr;

    assert(resource);

    auto& metadata = xbox.GetMetadata();

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = metadata.format;

    switch (metadata.dimension)
    {
    case DirectX::TEX_DIMENSION_TEXTURE1D:
        if (metadata.arraySize > 1)
        {
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
            SRVDesc.Texture1DArray.MipLevels = static_cast<UINT>(metadata.mipLevels);
            SRVDesc.Texture1DArray.ArraySize = static_cast<UINT>(metadata.arraySize);
        }
        else
        {
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
            SRVDesc.Texture1D.MipLevels = static_cast<UINT>(metadata.mipLevels);
        }
        break;

    case DirectX::TEX_DIMENSION_TEXTURE2D:
        if (metadata.IsCubemap())
        {
            if (metadata.arraySize > 6)
            {
                assert((metadata.arraySize % 6) == 0);
                SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                SRVDesc.TextureCubeArray.MipLevels = static_cast<UINT>(metadata.mipLevels);
                SRVDesc.TextureCubeArray.NumCubes = static_cast<UINT>(metadata.arraySize / 6);
            }
            else
            {
                SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                SRVDesc.TextureCube.MipLevels = static_cast<UINT>(metadata.mipLevels);
            }
        }
        else if (metadata.arraySize > 1)
        {
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            SRVDesc.Texture2DArray.MipLevels = static_cast<UINT>(metadata.mipLevels);
            SRVDesc.Texture2DArray.ArraySize = static_cast<UINT>(metadata.arraySize);
        }
        else
        {
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);
        }
        break;

    case DirectX::TEX_DIMENSION_TEXTURE3D:
        assert(metadata.arraySize == 1);
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        SRVDesc.Texture3D.MipLevels = static_cast<UINT>(metadata.mipLevels);
        break;

    default:
        assert(grfxMemory != nullptr);
        XMemFree(grfxMemory, c_XMemAllocAttributes);
        *grfxMemory = nullptr;
        return E_FAIL;
    }

    hr = d3dDevice->CreateShaderResourceView(resource.Get(), &SRVDesc, ppSRV);
    if (FAILED(hr))
    {
        XMemFree(grfxMemory, c_XMemAllocAttributes);
        *grfxMemory = nullptr;
    }

    return hr;
}


//-------------------------------------------------------------------------------------
// Free allocated graphics memory
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
void Xbox::FreeTextureMemory(ID3D11DeviceX* d3dDevice, void* grfxMemory)
{
    UNREFERENCED_PARAMETER(d3dDevice); // used only for overload resolution

    if (grfxMemory)
    {
        XMemFree(grfxMemory, c_XMemAllocAttributes);
    }
}
