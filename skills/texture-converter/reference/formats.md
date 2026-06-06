# Texconv Supported Formats

This document lists the DXGI formats supported by texconv for the `-f` / `--format` option.

## Format Aliases

These short-form aliases are accepted in addition to the full DXGI format names:

| Alias | DXGI Format |
| --- | --- |
| `DXT1` | `DXGI_FORMAT_BC1_UNORM` |
| `DXT2`, `DXT3` | `DXGI_FORMAT_BC2_UNORM` |
| `DXT4`, `DXT5` | `DXGI_FORMAT_BC3_UNORM` |
| `BGRA` | `DXGI_FORMAT_B8G8R8A8_UNORM` |
| `BGR` | `DXGI_FORMAT_B8G8R8X8_UNORM` |
| `FP16` | `DXGI_FORMAT_R16G16B16A16_FLOAT` |
| `FP32` | `DXGI_FORMAT_R32G32B32A32_FLOAT` |
| `RGBA` | `DXGI_FORMAT_R8G8B8A8_UNORM` |
| `BC3n`, `DXT5nm` | BC3 variant for normal maps (swapped channels) |
| `RXGB` | Custom FourCC DXT5 variant for normal maps (use with `-dx9`) |

## Block Compression Formats

| Format | Description | Use Case |
| --- | --- | --- |
| `BC1_UNORM` | 4:1 compression, 1-bit alpha | Opaque or cutout textures |
| `BC1_UNORM_SRGB` | BC1 with sRGB | Opaque diffuse/albedo |
| `BC2_UNORM` | 4:1 compression, explicit 4-bit alpha | Sharp alpha transitions |
| `BC2_UNORM_SRGB` | BC2 with sRGB | Rarely used |
| `BC3_UNORM` | 4:1 compression, interpolated alpha | Smooth alpha gradients |
| `BC3_UNORM_SRGB` | BC3 with sRGB | Diffuse with smooth alpha |
| `BC4_UNORM` | Single-channel (red) | Height maps, roughness |
| `BC4_SNORM` | Single-channel signed | Signed single-channel data |
| `BC5_UNORM` | Two-channel (red + green) | Normal maps (XY) |
| `BC5_SNORM` | Two-channel signed | Signed normal maps |
| `BC6H_UF16` | HDR unsigned half-float | HDR environment maps |
| `BC6H_SF16` | HDR signed half-float | HDR with negative values |
| `BC7_UNORM` | High-quality RGBA | High-quality textures |
| `BC7_UNORM_SRGB` | BC7 with sRGB | High-quality diffuse/albedo |

## Common Uncompressed Formats

| Format | Description |
| --- | --- |
| `R8G8B8A8_UNORM` | 32-bit RGBA |
| `R8G8B8A8_UNORM_SRGB` | 32-bit RGBA sRGB |
| `B8G8R8A8_UNORM` | 32-bit BGRA |
| `B8G8R8X8_UNORM` | 32-bit BGR (no alpha) |
| `R16G16B16A16_FLOAT` | 64-bit RGBA half-float |
| `R32G32B32A32_FLOAT` | 128-bit RGBA float |
| `R10G10B10A2_UNORM` | 10:10:10:2 HDR display |
| `R16G16_UNORM` | Two-channel 16-bit |
| `R8G8_UNORM` | Two-channel 8-bit |
| `R8_UNORM` | Single-channel 8-bit |
| `A8_UNORM` | Alpha-only 8-bit |
| `R16_FLOAT` | Single-channel half-float |
| `R32_FLOAT` | Single-channel float |
| `R9G9B9E5_SHAREDEXP` | Shared-exponent HDR |
| `R11G11B10_FLOAT` | Packed HDR (no alpha) |

## Output File Types

Specified with `-ft` / `--file-type`:

| Type | Extensions | Notes |
| --- | --- | --- |
| `dds` | `.dds`, `.ddx` | Default. Supports mipmaps, arrays, cubemaps, BC |
| `bmp` | `.bmp` | Windows bitmap |
| `jpg` / `jpeg` | `.jpg` | Lossy compression. Quality via `-wicq` |
| `png` | `.png` | Lossless with alpha |
| `tga` | `.tga` | Truevision. Use `-tga20` for 2.0 extension |
| `hdr` | `.hdr` | Radiance RGBE for HDR images |
| `tif` / `tiff` | `.tif` | Multi-page, various compressions |
| `jxr` / `wdp` / `hdp` | `.jxr` | JPEG XR / HD Photo |
| `ppm` / `pfm` | `.ppm`, `.pfm` | Portable pixmap/floatmap (Netpbm) |
| `exr` | `.exr` | OpenEXR (requires `USE_OPENEXR` build) |

## Format Selection Guidelines

### Diffuse/Albedo Textures
- **BC7_UNORM_SRGB**: Best quality, slower compression
- **BC1_UNORM_SRGB**: Good quality for opaque, 4× smaller than BC7
- **BC3_UNORM_SRGB**: When smooth alpha is needed

### Normal Maps
- **BC5_UNORM**: Two-channel XY (reconstruct Z in shader)
- **BC7_UNORM**: When 3-channel normals are needed
- Use `--reconstruct-z` when converting from BC5 back to RGBA

### HDR / Environment Maps
- **BC6H_UF16**: Compressed HDR
- **R16G16B16A16_FLOAT**: Uncompressed HDR
- **R9G9B9E5_SHAREDEXP**: Compact HDR (read-only in some hardware)

### Single-Channel (Roughness, Height, AO)
- **BC4_UNORM**: Compressed single-channel
- **R8_UNORM**: Uncompressed single-channel

### UI / Text
- **BC7_UNORM**: High-quality with alpha
- **R8G8B8A8_UNORM**: When artifacts are unacceptable

## Feature Level Texture Size Limits

| Feature Level | Max Texture 2D Size | Max Volume Depth |
| --- | --- | --- |
| 9.1 | 2048 | 256 |
| 9.2 | 2048 | 256 |
| 9.3 | 4096 | 256 |
| 10.0 | 8192 | 2048 |
| 10.1 | 8192 | 2048 |
| 11.0–12.2 | 16384 | 2048 |
