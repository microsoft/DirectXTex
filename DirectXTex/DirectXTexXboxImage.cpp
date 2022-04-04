//--------------------------------------------------------------------------------------
// File: DirectXTexXboxImage.cpp
//
// DirectXTex Auxillary functions for Xbox texture blob
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "DirectXTexP.h"
#include "DirectXTexXbox.h"

using namespace DirectX;
using namespace Xbox;

// Sanity check XG library values against DirectXTex's values
static_assert(static_cast<int>(XG_FORMAT_UNKNOWN)                       == static_cast<int>(DXGI_FORMAT_UNKNOWN), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32B32A32_TYPELESS)         == static_cast<int>(DXGI_FORMAT_R32G32B32A32_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32B32A32_FLOAT)            == static_cast<int>(DXGI_FORMAT_R32G32B32A32_FLOAT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32B32A32_UINT)             == static_cast<int>(DXGI_FORMAT_R32G32B32A32_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32B32A32_SINT)             == static_cast<int>(DXGI_FORMAT_R32G32B32A32_SINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32B32_TYPELESS)            == static_cast<int>(DXGI_FORMAT_R32G32B32_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32B32_FLOAT)               == static_cast<int>(DXGI_FORMAT_R32G32B32_FLOAT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32B32_UINT)                == static_cast<int>(DXGI_FORMAT_R32G32B32_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32B32_SINT)                == static_cast<int>(DXGI_FORMAT_R32G32B32_SINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16B16A16_TYPELESS)         == static_cast<int>(DXGI_FORMAT_R16G16B16A16_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16B16A16_FLOAT)            == static_cast<int>(DXGI_FORMAT_R16G16B16A16_FLOAT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16B16A16_UNORM)            == static_cast<int>(DXGI_FORMAT_R16G16B16A16_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16B16A16_UINT)             == static_cast<int>(DXGI_FORMAT_R16G16B16A16_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16B16A16_SNORM)            == static_cast<int>(DXGI_FORMAT_R16G16B16A16_SNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16B16A16_SINT)             == static_cast<int>(DXGI_FORMAT_R16G16B16A16_SINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32_TYPELESS)               == static_cast<int>(DXGI_FORMAT_R32G32_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32_FLOAT)                  == static_cast<int>(DXGI_FORMAT_R32G32_FLOAT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32_UINT)                   == static_cast<int>(DXGI_FORMAT_R32G32_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G32_SINT)                   == static_cast<int>(DXGI_FORMAT_R32G32_SINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32G8X24_TYPELESS)             == static_cast<int>(DXGI_FORMAT_R32G8X24_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_D32_FLOAT_S8X24_UINT)          == static_cast<int>(DXGI_FORMAT_D32_FLOAT_S8X24_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32_FLOAT_X8X24_TYPELESS)      == static_cast<int>(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_X32_TYPELESS_G8X24_UINT)       == static_cast<int>(DXGI_FORMAT_X32_TYPELESS_G8X24_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R10G10B10A2_TYPELESS)          == static_cast<int>(DXGI_FORMAT_R10G10B10A2_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R10G10B10A2_UNORM)             == static_cast<int>(DXGI_FORMAT_R10G10B10A2_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R10G10B10A2_UINT)              == static_cast<int>(DXGI_FORMAT_R10G10B10A2_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R11G11B10_FLOAT)               == static_cast<int>(DXGI_FORMAT_R11G11B10_FLOAT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8B8A8_TYPELESS)             == static_cast<int>(DXGI_FORMAT_R8G8B8A8_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8B8A8_UNORM)                == static_cast<int>(DXGI_FORMAT_R8G8B8A8_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8B8A8_UNORM_SRGB)           == static_cast<int>(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8B8A8_UINT)                 == static_cast<int>(DXGI_FORMAT_R8G8B8A8_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8B8A8_SNORM)                == static_cast<int>(DXGI_FORMAT_R8G8B8A8_SNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8B8A8_SINT)                 == static_cast<int>(DXGI_FORMAT_R8G8B8A8_SINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16_TYPELESS)               == static_cast<int>(DXGI_FORMAT_R16G16_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16_FLOAT)                  == static_cast<int>(DXGI_FORMAT_R16G16_FLOAT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16_UNORM)                  == static_cast<int>(DXGI_FORMAT_R16G16_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16_UINT)                   == static_cast<int>(DXGI_FORMAT_R16G16_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16_SNORM)                  == static_cast<int>(DXGI_FORMAT_R16G16_SNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16G16_SINT)                   == static_cast<int>(DXGI_FORMAT_R16G16_SINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32_TYPELESS)                  == static_cast<int>(DXGI_FORMAT_R32_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_D32_FLOAT)                     == static_cast<int>(DXGI_FORMAT_D32_FLOAT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32_FLOAT)                     == static_cast<int>(DXGI_FORMAT_R32_FLOAT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32_UINT)                      == static_cast<int>(DXGI_FORMAT_R32_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R32_SINT)                      == static_cast<int>(DXGI_FORMAT_R32_SINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R24G8_TYPELESS)                == static_cast<int>(DXGI_FORMAT_R24G8_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_D24_UNORM_S8_UINT)             == static_cast<int>(DXGI_FORMAT_D24_UNORM_S8_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R24_UNORM_X8_TYPELESS)         == static_cast<int>(DXGI_FORMAT_R24_UNORM_X8_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_X24_TYPELESS_G8_UINT)          == static_cast<int>(DXGI_FORMAT_X24_TYPELESS_G8_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8_TYPELESS)                 == static_cast<int>(DXGI_FORMAT_R8G8_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8_UNORM)                    == static_cast<int>(DXGI_FORMAT_R8G8_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8_UINT)                     == static_cast<int>(DXGI_FORMAT_R8G8_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8_SNORM)                    == static_cast<int>(DXGI_FORMAT_R8G8_SNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8_SINT)                     == static_cast<int>(DXGI_FORMAT_R8G8_SINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16_TYPELESS)                  == static_cast<int>(DXGI_FORMAT_R16_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16_FLOAT)                     == static_cast<int>(DXGI_FORMAT_R16_FLOAT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_D16_UNORM)                     == static_cast<int>(DXGI_FORMAT_D16_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16_UNORM)                     == static_cast<int>(DXGI_FORMAT_R16_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16_UINT)                      == static_cast<int>(DXGI_FORMAT_R16_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16_SNORM)                     == static_cast<int>(DXGI_FORMAT_R16_SNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R16_SINT)                      == static_cast<int>(DXGI_FORMAT_R16_SINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8_TYPELESS)                   == static_cast<int>(DXGI_FORMAT_R8_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8_UNORM)                      == static_cast<int>(DXGI_FORMAT_R8_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8_UINT)                       == static_cast<int>(DXGI_FORMAT_R8_UINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8_SNORM)                      == static_cast<int>(DXGI_FORMAT_R8_SNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8_SINT)                       == static_cast<int>(DXGI_FORMAT_R8_SINT), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_A8_UNORM)                      == static_cast<int>(DXGI_FORMAT_A8_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R1_UNORM)                      == static_cast<int>(DXGI_FORMAT_R1_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R9G9B9E5_SHAREDEXP)            == static_cast<int>(DXGI_FORMAT_R9G9B9E5_SHAREDEXP), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R8G8_B8G8_UNORM)               == static_cast<int>(DXGI_FORMAT_R8G8_B8G8_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_G8R8_G8B8_UNORM)               == static_cast<int>(DXGI_FORMAT_G8R8_G8B8_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC1_TYPELESS)                  == static_cast<int>(DXGI_FORMAT_BC1_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC1_UNORM)                     == static_cast<int>(DXGI_FORMAT_BC1_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC1_UNORM_SRGB)                == static_cast<int>(DXGI_FORMAT_BC1_UNORM_SRGB), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC2_TYPELESS)                  == static_cast<int>(DXGI_FORMAT_BC2_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC2_UNORM)                     == static_cast<int>(DXGI_FORMAT_BC2_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC2_UNORM_SRGB)                == static_cast<int>(DXGI_FORMAT_BC2_UNORM_SRGB), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC3_TYPELESS)                  == static_cast<int>(DXGI_FORMAT_BC3_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC3_UNORM)                     == static_cast<int>(DXGI_FORMAT_BC3_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC3_UNORM_SRGB)                == static_cast<int>(DXGI_FORMAT_BC3_UNORM_SRGB), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC4_TYPELESS)                  == static_cast<int>(DXGI_FORMAT_BC4_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC4_UNORM)                     == static_cast<int>(DXGI_FORMAT_BC4_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC4_SNORM)                     == static_cast<int>(DXGI_FORMAT_BC4_SNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC5_TYPELESS)                  == static_cast<int>(DXGI_FORMAT_BC5_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC5_UNORM)                     == static_cast<int>(DXGI_FORMAT_BC5_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC5_SNORM)                     == static_cast<int>(DXGI_FORMAT_BC5_SNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_B5G6R5_UNORM)                  == static_cast<int>(DXGI_FORMAT_B5G6R5_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_B5G5R5A1_UNORM)                == static_cast<int>(DXGI_FORMAT_B5G5R5A1_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_B8G8R8A8_UNORM)                == static_cast<int>(DXGI_FORMAT_B8G8R8A8_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_B8G8R8X8_UNORM)                == static_cast<int>(DXGI_FORMAT_B8G8R8X8_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_R10G10B10_XR_BIAS_A2_UNORM)    == static_cast<int>(DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_B8G8R8A8_TYPELESS)             == static_cast<int>(DXGI_FORMAT_B8G8R8A8_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_B8G8R8A8_UNORM_SRGB)           == static_cast<int>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_B8G8R8X8_TYPELESS)             == static_cast<int>(DXGI_FORMAT_B8G8R8X8_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_B8G8R8X8_UNORM_SRGB)           == static_cast<int>(DXGI_FORMAT_B8G8R8X8_UNORM_SRGB), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC6H_TYPELESS)                 == static_cast<int>(DXGI_FORMAT_BC6H_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC6H_UF16)                     == static_cast<int>(DXGI_FORMAT_BC6H_UF16), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC6H_SF16)                     == static_cast<int>(DXGI_FORMAT_BC6H_SF16), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC7_TYPELESS)                  == static_cast<int>(DXGI_FORMAT_BC7_TYPELESS), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC7_UNORM)                     == static_cast<int>(DXGI_FORMAT_BC7_UNORM), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_BC7_UNORM_SRGB)                == static_cast<int>(DXGI_FORMAT_BC7_UNORM_SRGB), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_AYUV)                          == static_cast<int>(DXGI_FORMAT_AYUV), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_Y410)                          == static_cast<int>(DXGI_FORMAT_Y410), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_Y416)                          == static_cast<int>(DXGI_FORMAT_Y416), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_NV12)                          == static_cast<int>(DXGI_FORMAT_NV12), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_P010)                          == static_cast<int>(DXGI_FORMAT_P010), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_P016)                          == static_cast<int>(DXGI_FORMAT_P016), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_420_OPAQUE)                    == static_cast<int>(DXGI_FORMAT_420_OPAQUE), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_YUY2)                          == static_cast<int>(DXGI_FORMAT_YUY2), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_Y210)                          == static_cast<int>(DXGI_FORMAT_Y210), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_Y216)                          == static_cast<int>(DXGI_FORMAT_Y216), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_NV11)                          == static_cast<int>(DXGI_FORMAT_NV11), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_AI44)                          == static_cast<int>(DXGI_FORMAT_AI44), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_IA44)                          == static_cast<int>(DXGI_FORMAT_IA44), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_P8)                            == static_cast<int>(DXGI_FORMAT_P8), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_A8P8)                          == static_cast<int>(DXGI_FORMAT_A8P8), "XG vs. DXGI mismatch");
static_assert(static_cast<int>(XG_FORMAT_B4G4R4A4_UNORM)                == static_cast<int>(DXGI_FORMAT_B4G4R4A4_UNORM), "XG vs. DXGI mismatch");

static_assert(static_cast<int>(XG_RESOURCE_DIMENSION_TEXTURE1D) == static_cast<int>(TEX_DIMENSION_TEXTURE1D), "XG vs. Direct3D 11 mismatch");
static_assert(static_cast<int>(XG_RESOURCE_DIMENSION_TEXTURE2D) == static_cast<int>(TEX_DIMENSION_TEXTURE2D), "XG vs. Direct3D 11 mismatch");
static_assert(static_cast<int>(XG_RESOURCE_DIMENSION_TEXTURE3D) == static_cast<int>(TEX_DIMENSION_TEXTURE3D), "XG vs. Direct3D 11 mismatch");

static_assert(static_cast<int>(XG_RESOURCE_MISC_TEXTURECUBE) == static_cast<int>(TEX_MISC_TEXTURECUBE), "XG vs. Direct3D 11 mismatch");

//--------------------------------------------------------------------------------------
// Initialize memory
//--------------------------------------------------------------------------------------

XboxImage& XboxImage::operator= (XboxImage&& moveFrom) noexcept
{
    if (this != &moveFrom)
    {
        Release();

        dataSize = moveFrom.dataSize;
        baseAlignment = moveFrom.baseAlignment;
        tilemode = moveFrom.tilemode;
        metadata = moveFrom.metadata;
        memory = moveFrom.memory;

        moveFrom.dataSize = 0;
        moveFrom.baseAlignment = 0;
        moveFrom.tilemode = c_XboxTileModeInvalid;
        moveFrom.memory = nullptr;
    }
    return *this;
}

_Use_decl_annotations_
HRESULT XboxImage::Initialize(const XG_TEXTURE1D_DESC& desc, const XG_RESOURCE_LAYOUT& layout, uint32_t miscFlags2)
{
    if (!layout.SizeBytes || !layout.BaseAlignmentBytes)
        return E_INVALIDARG;

    Release();

    if (layout.SizeBytes > UINT32_MAX
        || layout.BaseAlignmentBytes > UINT32_MAX)
        return E_FAIL;

    memory = reinterpret_cast<uint8_t*>(_aligned_malloc(layout.SizeBytes, 16));
    if (!memory)
        return E_OUTOFMEMORY;

    memset(memory, 0, layout.SizeBytes);

    memset(&metadata, 0, sizeof(metadata));
    metadata.width = desc.Width;
    metadata.height = 1;
    metadata.depth = 1;
    metadata.arraySize = desc.ArraySize;
    metadata.mipLevels = layout.MipLevels;
    metadata.format = static_cast<DXGI_FORMAT>(desc.Format);
    metadata.dimension = TEX_DIMENSION_TEXTURE1D;
    metadata.miscFlags2 = miscFlags2;

    dataSize = static_cast<uint32_t>(layout.SizeBytes);
    baseAlignment = static_cast<uint32_t>(layout.BaseAlignmentBytes);
#if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
    tilemode = desc.SwizzleMode;
#else
    tilemode = desc.TileMode;
#endif

    return S_OK;
}


_Use_decl_annotations_
HRESULT XboxImage::Initialize(const XG_TEXTURE2D_DESC& desc, const XG_RESOURCE_LAYOUT& layout, uint32_t miscFlags2)
{
    if (!layout.SizeBytes || !layout.BaseAlignmentBytes)
        return E_INVALIDARG;

    Release();

    if (layout.SizeBytes > UINT32_MAX
        || layout.BaseAlignmentBytes > UINT32_MAX)
        return E_FAIL;

    memory = reinterpret_cast<uint8_t*>(_aligned_malloc(layout.SizeBytes, 16));
    if (!memory)
        return E_OUTOFMEMORY;

    memset(memory, 0, layout.SizeBytes);

    memset(&metadata, 0, sizeof(metadata));
    metadata.width = desc.Width;
    metadata.height = desc.Height;
    metadata.depth = 1;
    metadata.arraySize = desc.ArraySize;
    metadata.mipLevels = layout.MipLevels;
    metadata.miscFlags = (desc.MiscFlags & XG_RESOURCE_MISC_TEXTURECUBE) ? TEX_MISC_TEXTURECUBE : 0;
    metadata.format = static_cast<DXGI_FORMAT>(desc.Format);
    metadata.dimension = TEX_DIMENSION_TEXTURE2D;
    metadata.miscFlags2 = miscFlags2;

    dataSize = static_cast<uint32_t>(layout.SizeBytes);
    baseAlignment = static_cast<uint32_t>(layout.BaseAlignmentBytes);
#if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
    tilemode = desc.SwizzleMode;
#else
    tilemode = desc.TileMode;
#endif

    return S_OK;
}


_Use_decl_annotations_
HRESULT XboxImage::Initialize(const XG_TEXTURE3D_DESC& desc, const XG_RESOURCE_LAYOUT& layout, uint32_t miscFlags2)
{
    if (!layout.SizeBytes || !layout.BaseAlignmentBytes)
        return E_INVALIDARG;

    Release();

    if (layout.SizeBytes > UINT32_MAX
        || layout.BaseAlignmentBytes > UINT32_MAX)
        return E_FAIL;

    memory = reinterpret_cast<uint8_t*>(_aligned_malloc(layout.SizeBytes, 16));
    if (!memory)
        return E_OUTOFMEMORY;

    memset(memory, 0, layout.SizeBytes);

    memset(&metadata, 0, sizeof(metadata));
    metadata.width = desc.Width;
    metadata.height = desc.Height;
    metadata.depth = desc.Depth;
    metadata.arraySize = 1;
    metadata.mipLevels = layout.MipLevels;
    metadata.format = static_cast<DXGI_FORMAT>(desc.Format);
    metadata.dimension = TEX_DIMENSION_TEXTURE3D;
    metadata.miscFlags2 = miscFlags2;

    dataSize = static_cast<uint32_t>(layout.SizeBytes);
    baseAlignment = static_cast<uint32_t>(layout.BaseAlignmentBytes);
#if defined(_GAMING_XBOX_SCARLETT) || defined(_USE_SCARLETT)
    tilemode = desc.SwizzleMode;
#else
    tilemode = desc.TileMode;
#endif

    return S_OK;
}


_Use_decl_annotations_
HRESULT XboxImage::Initialize(const DirectX::TexMetadata& mdata, XboxTileMode tm, uint32_t size, uint32_t alignment)
{
    if (!size || !alignment || tm == c_XboxTileModeInvalid)
        return E_INVALIDARG;

    Release();

    memory = reinterpret_cast<uint8_t*>(_aligned_malloc(size, 16));
    if (!memory)
        return E_OUTOFMEMORY;

    memset(memory, 0, size);

    metadata = mdata;

    dataSize = size;
    baseAlignment = alignment;
    tilemode = tm;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Release memory
//--------------------------------------------------------------------------------------
void XboxImage::Release()
{
    if (memory)
    {
        _aligned_free(memory);
        memory = nullptr;
    }
}
