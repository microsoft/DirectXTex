# Texconv Usage Examples

This document provides practical examples for common texture conversion scenarios.

## Basic Conversions

### Convert PNG to DDS with default settings (mipmaps, no compression)

```plaintext
texconv -y texture.png
```

### Convert with BC7 compression (highest quality)

```plaintext
texconv -f BC7_UNORM -y diffuse.png
```

### Convert with sRGB-aware BC7 compression

```plaintext
texconv -f BC7_UNORM_SRGB -srgb -y diffuse.png
```

### Convert DDS back to PNG

```plaintext
texconv -ft png -y texture.dds
```

### Convert to JPEG with quality setting

```plaintext
texconv -ft jpg -wicq 0.9 -y screenshot.bmp
```

## Batch Processing

### Convert all PNG files in current directory

```plaintext
texconv -f BC7_UNORM -y *.png
```

### Recursively convert all textures preserving directory structure

```plaintext
texconv -r:keep -f BC7_UNORM -o output -y C:\Assets\*.png
```

### Convert files from a list

```plaintext
texconv -f BC7_UNORM -y --file-list textures.txt
```

### Convert with filename modifications

```plaintext
texconv -f BC7_UNORM -px compiled_ -sx _bc7 -o build\textures -y *.png
```

## Resizing

### Resize to specific dimensions

```plaintext
texconv -w 1024 -h 1024 -f BC7_UNORM -y large_texture.png
```

### Fit to power-of-2 (preserving aspect ratio)

```plaintext
texconv -pow2 -f BC1_UNORM -y photo.jpg
```

### Fit to power-of-2 with feature level cap (max 4096)

```plaintext
texconv -pow2 -fl 9.3 -f BC3_UNORM -y photo.jpg
```

## Mipmap Control

### Generate full mipmap chain (default for DDS)

```plaintext
texconv -m 0 -f BC7_UNORM -y texture.png
```

### No mipmaps (single level)

```plaintext
texconv -m 1 -f BC7_UNORM -y ui_element.png
```

### Specific number of mip levels

```plaintext
texconv -m 5 -f BC7_UNORM -y texture.png
```

### Preserve alpha coverage in mipmaps (for foliage/hair)

```plaintext
texconv -f BC7_UNORM --keep-coverage 0.5 -y foliage.png
```

## Normal Maps

### Generate normal map from height map (using luminance)

```plaintext
texconv -nmap lo -nmapamp 2.0 -f R8G8B8A8_UNORM -y heightmap.png
```

### Generate normal map using red channel with higher amplitude

```plaintext
texconv -nmap ro -nmapamp 4.0 -f BC5_UNORM -y heightmap.png
```

### Convert OpenGL normal map to Direct3D convention

```plaintext
texconv --invert-y -f BC5_UNORM -y normal_opengl.png
```

### Reconstruct Z channel from XY-only normal map (BC5)

```plaintext
texconv --reconstruct-z -ft png -y normal_bc5.dds
```

### Swizzle for two-channel normal maps

```plaintext
texconv --swizzle rg -f BC5_UNORM -y normal_map.png
```

## Alpha Handling

### Convert to premultiplied alpha

```plaintext
texconv -pmalpha -f BC3_UNORM -y ui_panel.png
```

### Convert premultiplied alpha back to straight alpha

```plaintext
texconv -alpha -ft png -y premultiplied.dds
```

### Separate alpha processing (non-transparency alpha data)

```plaintext
texconv -sepalpha -f BC3_UNORM -y packed_texture.png
```

### Apply chromakey (green screen removal)

```plaintext
texconv -c 00FF00 -f BC7_UNORM -y greenscreen.png
```

### Set alpha threshold for BC1 1-bit alpha

```plaintext
texconv -at 0.3 -f BC1_UNORM -y cutout.png
```

## HDR and Colorspace

### Tone map HDR to LDR

```plaintext
texconv --tonemap -ft bmp -y environment.hdr
```

### Convert to HDR10 signal

```plaintext
texconv --rotate-color 709toHDR10 --paper-white-nits 200 -f R10G10B10A2_UNORM -y scene.exr
```

### Convert HDR10 back to linear Rec.709

```plaintext
texconv --rotate-color HDR10to709 -f R16G16B16A16_FLOAT -y hdr10_signal.dds
```

### Convert Rec.709 to Display P3

```plaintext
texconv --rotate-color 709toDisplayP3 -f R10G10B10A2_UNORM -y wide_gamut.png
```

### Force sRGB interpretation on output only

```plaintext
texconv -srgbo -f R8G8B8A8_UNORM_SRGB -y linear_source.hdr
```

### Ignore embedded sRGB metadata

```plaintext
texconv --ignore-srgb -f BC7_UNORM -y image_with_bad_metadata.png
```

## Block Compression Options

### BC7 with maximum quality (slow)

```plaintext
texconv -f BC7_UNORM -bc x -y texture.png
```

### BC7 with quick/minimal compression (fast)

```plaintext
texconv -f BC7_UNORM -bc q -y texture.png
```

### BC1-BC3 with dithering

```plaintext
texconv -f BC1_UNORM -bc d -y texture.png
```

### BC1-BC3 with uniform weighting (instead of perceptual)

```plaintext
texconv -f BC1_UNORM -bc u -y texture.png
```

### Force GPU compression for BC6H/BC7

```plaintext
texconv -f BC7_UNORM -gpu 0 -y texture.png
```

### Force CPU-only compression

```plaintext
texconv -f BC7_UNORM -nogpu -y texture.png
```

### Adjust alpha weight for BC7

```plaintext
texconv -f BC7_UNORM -aw 2.0 -y texture_with_important_alpha.png
```

## Image Operations

### Flip vertically (common for OpenGL↔DirectX)

```plaintext
texconv -vflip -f BC7_UNORM -y texture.png
```

### Flip horizontally

```plaintext
texconv -hflip -ft png -y mirrored.png
```

### Swizzle: extract alpha to grayscale

```plaintext
texconv --swizzle aaaa -ft png -y texture_with_alpha.dds
```

### Swizzle: move red channel to alpha, zero RGB

```plaintext
texconv --swizzle 000r -f BC4_UNORM -y roughness.png
```

### Swizzle: pack channels

```plaintext
texconv --swizzle rg01 -f R8G8_UNORM -y normal_xy.png
```

## DDS-Specific Options

### Force DX10 header (required for some formats)

```plaintext
texconv -f BC7_UNORM -dx10 -y texture.png
```

### Force legacy DX9 header

```plaintext
texconv -f BC1_UNORM -dx9 -y texture.png
```

### Handle typeless format as UNORM

```plaintext
texconv -tu -ft png -y typeless_texture.dds
```

### Fix non-multiple-of-4 BC texture

```plaintext
texconv --fix-bc-4x4 -f BC7_UNORM -y odd_size.dds
```

### Load permissive/malformed DDS

```plaintext
texconv --permissive -ft png -y legacy_texture.dds
```

### Expand luminance formats to RGBA

```plaintext
texconv -xlum -ft png -y lightmap.dds
```

## TGA Options

### Write TGA 2.0 with metadata

```plaintext
texconv -ft tga -tga20 -y texture.png
```

### Preserve zero alpha in TGA

```plaintext
texconv --tga-zero-alpha -f R8G8B8A8_UNORM -y legacy.tga
```

## Filtering Options

### Use point filtering (no interpolation)

```plaintext
texconv -w 512 -h 512 -if POINT -y pixel_art.png
```

### Use cubic filtering with dithering

```plaintext
texconv -w 256 -h 256 -if CUBIC_DITHER -f BC1_UNORM -y photo.png
```

### Wrap addressing for tileable textures

```plaintext
texconv -wrap -f BC7_UNORM -y tileable.png
```

### Mirror addressing

```plaintext
texconv -mirror -f BC7_UNORM -y symmetric.png
```

## Game Asset Pipeline Examples

### Diffuse texture (sRGB, BC7, full mipmaps)

```plaintext
texconv -f BC7_UNORM_SRGB -srgb -m 0 -y diffuse.png
```

### Normal map (BC5, linear, full mipmaps)

```plaintext
texconv -f BC5_UNORM -m 0 -y normal.png
```

### Roughness/metallic packed map

```plaintext
texconv -f BC7_UNORM -m 0 -y orm.png
```

### Environment cubemap face (HDR, BC6H)

```plaintext
texconv -f BC6H_UF16 -m 0 -y env_posx.hdr
```

### UI texture (no mipmaps, high quality)

```plaintext
texconv -f BC7_UNORM_SRGB -srgb -m 1 -y button.png
```

### Sprite atlas (premultiplied alpha, no mipmaps)

```plaintext
texconv -pmalpha -f BC3_UNORM_SRGB -srgb -m 1 -y sprites.png
```

### Foliage with alpha coverage preservation

```plaintext
texconv -f BC7_UNORM_SRGB -srgb --keep-coverage 0.5 -y leaves.png
```

### Legacy Direct3D 9 target

```plaintext
texconv -pow2 -fl 9.3 -f BC1_UNORM -dx9 -m 0 -y legacy_texture.bmp
```
