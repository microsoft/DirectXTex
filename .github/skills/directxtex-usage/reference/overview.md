# DirectXTex API Reference

This document provides an overview of the DirectXTex public API. Full function signatures with SAL annotations can be found in `DirectXTex/DirectXTex.h`.

## Namespace

All DirectXTex functions and types reside in the `DirectX` namespace.

## DXGI Format Utilities

| Function | Description |
| --- | --- |
| `IsValid` | Returns true if fmt is a valid DXGI_FORMAT value |
| `IsCompressed` | Returns true for BC1–BC7 block-compressed formats |
| `IsPacked` | Returns true for packed formats (e.g., R8G8_B8G8) |
| `IsVideo` | Returns true for video-specific formats |
| `IsPlanar` | Returns true for planar formats (e.g., NV12) |
| `IsPalettized` | Returns true for palettized formats |
| `IsDepthStencil` | Returns true for depth/stencil formats |
| `IsSRGB` | Returns true for sRGB formats |
| `IsBGR` | Returns true for BGR-ordered formats |
| `IsTypeless` | Returns true for typeless formats |
| `HasAlpha` | Returns true if the format includes an alpha channel |
| `BitsPerPixel` | Returns bits per pixel for the format |
| `BitsPerColor` | Returns bits per color channel |
| `BytesPerBlock` | Returns bytes per block for compressed formats |
| `FormatDataType` | Returns the data type classification (float, unorm, etc.) |
| `ComputePitch` | Computes row and slice pitch for a given format and dimensions |
| `ComputeScanlines` | Computes number of scanlines including BC padding |
| `CalculateMipLevels` | Calculates valid mip level count for 1D/2D textures |
| `CalculateMipLevels3D` | Calculates valid mip level count for volume textures |
| `MakeSRGB` | Converts format to its sRGB equivalent |
| `MakeLinear` | Converts format to its linear (non-sRGB) equivalent |
| `MakeTypeless` | Converts format to its typeless equivalent |
| `MakeTypelessUNORM` | Converts typeless format to UNORM equivalent |
| `MakeTypelessFLOAT` | Converts typeless format to FLOAT equivalent |

## Structures

### TexMetadata

Describes complete texture resource properties.

| Field | Type | Description |
| --- | --- | --- |
| `width` | `size_t` | Texture width in pixels |
| `height` | `size_t` | Texture height (1 for 1D textures) |
| `depth` | `size_t` | Texture depth (1 for 1D/2D textures) |
| `arraySize` | `size_t` | Array size (multiple of 6 for cubemaps) |
| `mipLevels` | `size_t` | Number of mip levels |
| `miscFlags` | `uint32_t` | Miscellaneous flags (e.g., `TEX_MISC_TEXTURECUBE`) |
| `miscFlags2` | `uint32_t` | Additional flags (alpha mode encoded here) |
| `format` | `DXGI_FORMAT` | Pixel format |
| `dimension` | `TEX_DIMENSION` | Resource dimension (1D, 2D, 3D) |

Methods: `ComputeIndex`, `IsCubemap`, `IsPMAlpha`, `SetAlphaMode`, `GetAlphaMode`, `IsVolumemap`, `CalculateSubresource`

### DDSMetaData

Contains the original DDS pixel format header information for advanced use cases.

### Image

Describes a single 2D image surface within a `ScratchImage`.

| Field | Type | Description |
| --- | --- | --- |
| `width` | `size_t` | Image width |
| `height` | `size_t` | Image height |
| `format` | `DXGI_FORMAT` | Pixel format |
| `rowPitch` | `size_t` | Bytes per row |
| `slicePitch` | `size_t` | Total bytes for the image |
| `pixels` | `uint8_t*` | Pointer to pixel data |

### ScratchImage

RAII container that owns texture pixel data. Move-only (copy deleted).

| Method | Description |
| --- | --- |
| `Initialize` | Initialize from TexMetadata |
| `Initialize1D` / `Initialize2D` / `Initialize3D` | Initialize for specific dimension |
| `InitializeCube` | Initialize as cubemap |
| `InitializeFromImage` | Initialize from a single Image |
| `InitializeArrayFromImages` | Initialize array from multiple Images |
| `InitializeCubeFromImages` | Initialize cubemap from face Images |
| `Initialize3DFromImages` | Initialize volume from depth-slice Images |
| `Release` | Free all memory |
| `OverrideFormat` | Change format tag without conversion |
| `GetMetadata` | Access TexMetadata |
| `GetImage` | Access specific image in the texture |
| `GetImages` / `GetImageCount` | Access all images |
| `GetPixels` / `GetPixelsSize` | Access raw pixel memory |
| `IsAlphaAllOpaque` | Check if alpha is all opaque |

### Blob

16-byte-aligned memory buffer for serialized output.

| Method | Description |
| --- | --- |
| `Initialize` | Allocate buffer |
| `Release` | Free buffer |
| `GetBufferPointer` | Get mutable pointer |
| `GetConstBufferPointer` | Get const pointer |
| `GetBufferSize` | Get buffer size |
| `Resize` | Reallocate to new size |
| `Trim` | Reduce size without reallocation |

### Rect

Simple rectangle for `CopyRectangle` operations: `x`, `y`, `w`, `h`.

### TileShape

Describes a tile shape for tiled/reserved resources (D3D11/D3D12 interop).

## Image I/O Functions

> See `DirectXTex/DirectXTex.h` for full function signatures with SAL annotations.

### DDS

| Function | Description |
| --- | --- |
| `GetMetadataFromDDSFile` | Read metadata from a DDS file |
| `GetMetadataFromDDSMemory` | Read metadata from DDS data in memory |
| `GetMetadataFromDDSFileEx` | Read metadata and pixel format from a DDS file |
| `GetMetadataFromDDSMemoryEx` | Read metadata and pixel format from DDS data in memory |
| `LoadFromDDSFile` | Load a DDS file into a ScratchImage |
| `LoadFromDDSMemory` | Load DDS data from memory into a ScratchImage |
| `LoadFromDDSFileEx` | Load a DDS file with extended pixel format info |
| `LoadFromDDSMemoryEx` | Load DDS data from memory with extended pixel format info |
| `SaveToDDSFile` | Save image(s) to a DDS file |
| `SaveToDDSMemory` | Save image(s) to a DDS blob in memory |

### HDR (Radiance RGBE)

| Function | Description |
| --- | --- |
| `GetMetadataFromHDRFile` | Read metadata from an HDR file |
| `GetMetadataFromHDRMemory` | Read metadata from HDR data in memory |
| `LoadFromHDRFile` | Load an HDR file into a ScratchImage |
| `LoadFromHDRMemory` | Load HDR data from memory into a ScratchImage |
| `SaveToHDRFile` | Save a single image to an HDR file |
| `SaveToHDRMemory` | Save a single image to an HDR blob in memory |

### TGA (Targa)

| Function | Description |
| --- | --- |
| `GetMetadataFromTGAFile` | Read metadata from a TGA file |
| `GetMetadataFromTGAMemory` | Read metadata from TGA data in memory |
| `LoadFromTGAFile` | Load a TGA file into a ScratchImage |
| `LoadFromTGAMemory` | Load TGA data from memory into a ScratchImage |
| `SaveToTGAFile` | Save a single image to a TGA file |
| `SaveToTGAMemory` | Save a single image to a TGA blob in memory |

### WIC (Windows only — PNG, BMP, JPEG, TIFF, GIF, etc.)

| Function | Description |
| --- | --- |
| `GetMetadataFromWICFile` | Read metadata from a WIC-supported file |
| `GetMetadataFromWICMemory` | Read metadata from WIC-supported data in memory |
| `LoadFromWICFile` | Load a WIC-supported file into a ScratchImage |
| `LoadFromWICMemory` | Load WIC-supported data from memory into a ScratchImage |
| `SaveToWICFile` | Save image(s) to a WIC-supported file format |
| `SaveToWICMemory` | Save image(s) to a WIC-supported blob in memory |

## Texture Processing Functions

### Resize

| Function | Description |
| --- | --- |
| `Resize` | Resize image(s) to a new width and height |

### Convert (Format Conversion)

| Function | Description |
| --- | --- |
| `Convert` | Convert image(s) to a new pixel format |
| `ConvertEx` | Convert with extended options and progress callback |
| `ConvertToSinglePlane` | Convert planar format to equivalent non-planar format |

### Mipmap Generation

| Function | Description |
| --- | --- |
| `GenerateMipMaps` | Generate a full or partial mipmap chain for 1D/2D textures |
| `GenerateMipMaps3D` | Generate a mipmap chain for volume (3D) textures |
| `ScaleMipMapsAlphaForCoverage` | Adjust mip alpha values to preserve coverage |

### Block Compression

| Function | Description |
| --- | --- |
| `Compress` | CPU block compression (BC1–BC7) |
| `Compress` (D3D11) | GPU-accelerated BC6H/BC7 compression via DirectCompute |
| `CompressEx` | Compress with extended options and progress callback |
| `CompressEx` (D3D11) | GPU-accelerated compress with extended options |
| `Decompress` | Decompress block-compressed image(s) to a standard format |

### Flip/Rotate (Windows only)

| Function | Description |
| --- | --- |
| `FlipRotate` | Flip and/or rotate image(s) |

### Premultiplied Alpha

| Function | Description |
| --- | --- |
| `PremultiplyAlpha` | Convert between straight and premultiplied alpha |

### Normal Map

| Function | Description |
| --- | --- |
| `ComputeNormalMap` | Generate a normal map from a height map image |

### Misc Image Operations

| Function | Description |
| --- | --- |
| `CopyRectangle` | Copy a rectangular region between images |
| `ComputeMSE` | Compute mean-squared error between two images |
| `EvaluateImage` | Iterate over image pixels with a read-only callback |
| `TransformImage` | Transform image pixels with a read/write callback |

## DDS Helper Functions

| Function | Description |
| --- | --- |
| `EncodeDDSHeader` | Encode a DDS header from TexMetadata |

## Tiling Utilities

| Function | Description |
| --- | --- |
| `ComputeTileShape` | Compute the tile dimensions for a given format and dimension |

## WIC Utilities (Windows only)

| Function | Description |
| --- | --- |
| `GetWICCodec` | Get the WIC container format GUID for a codec enum |
| `GetWICFactory` | Get the shared WIC imaging factory |
| `SetWICFactory` | Set a custom WIC imaging factory |

## Direct3D 11 Interop

| Function | Description |
| --- | --- |
| `IsSupportedTexture` | Check if a texture format/layout is supported by the device |
| `CreateTexture` | Create a D3D11 texture resource from image data |
| `CreateShaderResourceView` | Create a D3D11 SRV from image data |
| `CreateTextureEx` | Create a D3D11 texture with explicit usage/bind flags |
| `CreateShaderResourceViewEx` | Create a D3D11 SRV with explicit usage/bind flags |
| `CaptureTexture` | Copy a D3D11 texture resource into a ScratchImage |

## Direct3D 12 Interop

| Function | Description |
| --- | --- |
| `IsSupportedTexture` | Check if a texture format/layout is supported by the device |
| `CreateTexture` | Create a D3D12 committed resource from metadata |
| `CreateTextureEx` | Create a D3D12 committed resource with explicit resource flags |
| `PrepareUpload` | Prepare subresource data for upload via copy queue |
| `CaptureTexture` | Copy a D3D12 texture resource into a ScratchImage |

## Flags Enumerations

| Enum | Purpose |
| --- | --- |
| `CP_FLAGS` | Pitch computation alignment options |
| `DDS_FLAGS` | DDS load/save behavior options |
| `TGA_FLAGS` | TGA load/save options |
| `WIC_FLAGS` | WIC load/save options |
| `TEX_FR_FLAGS` | Flip/rotate operations |
| `TEX_FILTER_FLAGS` | Filtering mode for resize/convert/mipgen |
| `TEX_PMALPHA_FLAGS` | Premultiplied alpha conversion options |
| `TEX_COMPRESS_FLAGS` | Block compression options |
| `CNMAP_FLAGS` | Normal map generation options |
| `CMSE_FLAGS` | Mean-squared error comparison options |
| `CREATETEX_FLAGS` | Direct3D texture creation options |

## Auxiliary Modules

### DirectXTexEXR (`Auxiliary/DirectXTexEXR.h`)

OpenEXR format support via the OpenEXR library.

### DirectXTexJPEG (`Auxiliary/DirectXTexJPEG.h`)

JPEG support via libjpeg (cross-platform, does not require WIC).

### DirectXTexPNG (`Auxiliary/DirectXTexPNG.h`)

PNG support via libpng (cross-platform, does not require WIC).

### DirectXTexXbox (`Auxiliary/DirectXTexXbox.h`)

Xbox-specific tiling/detiling and DDS extensions for Xbox One and Xbox Series X|S.
