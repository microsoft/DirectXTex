---
name: directxtex-usage
description: Guide for integrating and using the DirectXTex texture processing library in new projects. Use this skill when asked about adding DirectXTex to a project, loading/saving textures, format conversion, mipmap generation, block compression, or Direct3D resource creation.
---

# DirectXTex Usage Guide

This skill provides guidance for integrating the DirectXTex texture processing library into a C++ project.

## When to Use

Invoke this skill when:

- Adding DirectXTex as a dependency to a new or existing project.
- Writing code that processes texture data (resizing, format conversion, mipmap generation, DirectX Block Compression, etc.).
- Needing to understand the typical DirectXTex processing pipeline.

## Overview

DirectXTex is a texture processing library for Direct3D 11 and Direct3D 12 applications. It provides support for reading and writing DDS files, and performing various texture content processing operations including resizing, format conversion, mipmap generation, block compression, and normal map creation.

- **Repository**: <https://github.com/microsoft/DirectXTex>
- **Documentation**: <https://github.com/microsoft/DirectXTex/wiki>
- **NuGet Packages**: `directxtex_desktop_win10`, `directxtex_uwp`
- **vcpkg Port**: `directxtex`

## Integration Methods

### vcpkg manifest-mode (Recommended)

In your `vcpkg.json` file, add the following:

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "dependencies": [
    "directxtex"
  ]
}
```

### vcpkg (classic)

```bash
vcpkg install directxtex
```

Features: `dx12` (DirectX 12 API support), `openexr` (OpenEXR support), `tools` (command-line tools). Triplets: `x64-windows`, `x64-linux`, `arm64-windows`, etc.

For DLL usage (`x64-windows` default triplet), define `DIRECTX_TEX_IMPORT` in your consuming project. For static library usage, use `-static-md` triplet variants.

CMakeLists.txt:

```cmake
find_package(directxtex CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Microsoft::DirectXTex)
```

### NuGet

Use `directxtex_desktop_win10` for Win32 desktop applications or `directxtex_uwp` for UWP apps.

### Project Reference

Add the appropriate `.vcxproj` from the `DirectXTex/` folder to your solution and add a project reference. Add the `DirectXTex` directory to your Additional Include Directories.

## Header Include

```cpp
#include <d3d12.h>        // or <d3d11_1.h> — include BEFORE DirectXTex.h
#include <DirectXTex.h>

using namespace DirectX;
```

For auxiliary features, include additional headers:

```cpp
#include <DirectXTexEXR.h>   // OpenEXR support
#include <DirectXTexJPEG.h>  // libjpeg support (non-WIC)
#include <DirectXTexPNG.h>   // libpng support (non-WIC)
#include <DirectXTexXbox.h>  // Xbox tiling extensions
```

> DirectXTexJPEG, DirectXTexPNG are typically only used on Linux.

## Key Concepts

### ScratchImage

`ScratchImage` is the primary image container. It owns pixel memory and provides access to individual mip levels, array slices, and volume depth slices.

```cpp
ScratchImage image;
HRESULT hr = LoadFromDDSFile(L"texture.dds", DDS_FLAGS_NONE, nullptr, image);
if (FAILED(hr))
    // handle error

const TexMetadata& metadata = image.GetMetadata();
const Image* img = image.GetImage(0, 0, 0); // mip 0, item 0, slice 0
```

### TexMetadata

Describes the texture dimensions, format, mip levels, array size, and type (1D, 2D, 3D, cubemap).

### Blob

A memory buffer used for serialized output (e.g., saving to memory rather than files).

### Error Handling

All processing functions return `HRESULT`. Check with `FAILED()` / `SUCCEEDED()` macros.

## Common Workflows

### Loading Textures

```cpp
// From DDS file
ScratchImage image;
HRESULT hr = LoadFromDDSFile(L"texture.dds", DDS_FLAGS_NONE, nullptr, image);

// From WIC file (PNG, BMP, JPEG, TIFF, etc.) — Windows only
ScratchImage image;
HRESULT hr = LoadFromWICFile(L"texture.png", WIC_FLAGS_NONE, nullptr, image);

// From TGA file
ScratchImage image;
HRESULT hr = LoadFromTGAFile(L"texture.tga", TGA_FLAGS_NONE, nullptr, image);

// From HDR file
ScratchImage image;
HRESULT hr = LoadFromHDRFile(L"texture.hdr", nullptr, image);

// From memory
ScratchImage image;
HRESULT hr = LoadFromDDSMemory(pData, dataSize, DDS_FLAGS_NONE, nullptr, image);
```

### Saving Textures

```cpp
// Save to DDS file
const Image* img = image.GetImages();
size_t nimages = image.GetImageCount();
const TexMetadata& metadata = image.GetMetadata();
HRESULT hr = SaveToDDSFile(img, nimages, metadata, DDS_FLAGS_NONE, L"output.dds");

// Save single image to WIC file (PNG) — Windows only
hr = SaveToWICFile(*image.GetImage(0, 0, 0), WIC_FLAGS_NONE,
    GetWICCodec(WIC_CODEC_PNG), L"output.png");

// Save to memory blob
Blob blob;
hr = SaveToDDSMemory(img, nimages, metadata, DDS_FLAGS_NONE, blob);
```

### Format Conversion

```cpp
ScratchImage converted;
HRESULT hr = Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
    DXGI_FORMAT_R8G8B8A8_UNORM, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, converted);
```

### Resizing

```cpp
ScratchImage resized;
HRESULT hr = Resize(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
    1024, 1024, TEX_FILTER_DEFAULT, resized);
```

### Mipmap Generation

```cpp
ScratchImage mipChain;
HRESULT hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
    TEX_FILTER_DEFAULT, 0, mipChain);
// 0 levels = generate full mip chain
```

### Block Compression

```cpp
ScratchImage compressed;
HRESULT hr = Compress(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
    DXGI_FORMAT_BC7_UNORM, TEX_COMPRESS_DEFAULT, TEX_THRESHOLD_DEFAULT, compressed);

// GPU-accelerated BC6H/BC7 compression (Direct3D 11)
ScratchImage compressed;
HRESULT hr = Compress(pDevice, image.GetImages(), image.GetImageCount(), image.GetMetadata(),
    DXGI_FORMAT_BC7_UNORM, TEX_COMPRESS_DEFAULT, TEX_ALPHA_WEIGHT_DEFAULT, compressed);
```

### Decompression

```cpp
ScratchImage decompressed;
HRESULT hr = Decompress(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
    DXGI_FORMAT_R8G8B8A8_UNORM, decompressed);
```

### Normal Map Generation

```cpp
ScratchImage normalMap;
HRESULT hr = ComputeNormalMap(*image.GetImage(0, 0, 0),
    CNMAP_CHANNEL_LUMINANCE, 2.0f,
    DXGI_FORMAT_R8G8B8A8_UNORM, normalMap);
```

### Premultiplied Alpha

```cpp
ScratchImage pmAlpha;
HRESULT hr = PremultiplyAlpha(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
    TEX_PMALPHA_DEFAULT, pmAlpha);
```

### Creating Direct3D 11 Resources

```cpp
#include <d3d11.h>
#include <DirectXTex.h>

// Create texture resource
ID3D11Resource* pTexture = nullptr;
HRESULT hr = CreateTexture(pDevice, image.GetImages(), image.GetImageCount(),
    image.GetMetadata(), &pTexture);

// Create shader resource view directly
ID3D11ShaderResourceView* pSRV = nullptr;
hr = CreateShaderResourceView(pDevice, image.GetImages(), image.GetImageCount(),
    image.GetMetadata(), &pSRV);
```

### Creating Direct3D 12 Resources

```cpp
#include <d3d12.h>
#include <DirectXTex.h>

// Create committed resource
ID3D12Resource* pTexture = nullptr;
HRESULT hr = CreateTexture(pDevice, image.GetMetadata(), &pTexture);

// Prepare upload data for CopyTextureRegion
std::vector<D3D12_SUBRESOURCE_DATA> subresources;
hr = PrepareUpload(pDevice, image.GetImages(), image.GetImageCount(),
    image.GetMetadata(), subresources);
```

## Typical Texture Pipeline

A common offline texture processing pipeline:

```cpp
// 1. Load source image
ScratchImage source;
HRESULT hr = LoadFromWICFile(L"diffuse.png", WIC_FLAGS_NONE, nullptr, source);
if (FAILED(hr)) return hr;

// 2. Resize if needed
ScratchImage resized;
if (source.GetMetadata().width != 1024 || source.GetMetadata().height != 1024)
{
    hr = Resize(*source.GetImage(0, 0, 0), 1024, 1024, TEX_FILTER_DEFAULT, resized);
    if (FAILED(hr)) return hr;
}
else
{
    resized = std::move(source);
}

// 3. Generate mipmaps
ScratchImage mipChain;
hr = GenerateMipMaps(*resized.GetImage(0, 0, 0), TEX_FILTER_DEFAULT, 0, mipChain);
if (FAILED(hr)) return hr;

// 4. Compress to BC7
ScratchImage compressed;
hr = Compress(mipChain.GetImages(), mipChain.GetImageCount(), mipChain.GetMetadata(),
    DXGI_FORMAT_BC7_UNORM, TEX_COMPRESS_DEFAULT, TEX_THRESHOLD_DEFAULT, compressed);
if (FAILED(hr)) return hr;

// 5. Save as DDS
hr = SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(),
    compressed.GetMetadata(), DDS_FLAGS_NONE, L"diffuse.dds");
```

## Format Utilities

DirectXTex provides utility functions for querying DXGI format properties:

```cpp
bool compressed = IsCompressed(DXGI_FORMAT_BC7_UNORM);     // true
size_t bpp = BitsPerPixel(DXGI_FORMAT_R8G8B8A8_UNORM);    // 32
bool srgb = IsSRGB(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);     // true

DXGI_FORMAT srgbFmt = MakeSRGB(DXGI_FORMAT_R8G8B8A8_UNORM);   // _SRGB variant
DXGI_FORMAT linearFmt = MakeLinear(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB); // non-SRGB

size_t rowPitch, slicePitch;
ComputePitch(DXGI_FORMAT_R8G8B8A8_UNORM, 256, 256, rowPitch, slicePitch);
```

## Platform Support

| Platform | DDS | HDR | TGA | WIC | Direct3D 11 | Direct3D 12 |
| --- | --- | --- | --- | --- | --- | --- |
| Windows desktop | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| UWP | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Xbox (GDK) | ✓ | ✓ | ✓ | — | ✓ | ✓ |
| Linux | ✓ | ✓ | ✓ | — | — | — |

## Command-Line Tools

DirectXTex includes three CLI tools for texture processing:

- **texconv** — Convert images to DDS with compression, mipmaps, and resizing
- **texassemble** — Create cubemaps, texture arrays, and volume maps from individual images
- **texdiag** — Inspect and validate DDS files

Example texconv usage:

```bash
texconv -f BC7_UNORM -m 0 -y diffuse.png
```

## Further Reading

- [Getting Started](https://github.com/microsoft/DirectXTex/wiki/Getting-Started)
- [DirectXTex Wiki](https://github.com/microsoft/DirectXTex/wiki)
- [API Reference](reference/overview.md)
