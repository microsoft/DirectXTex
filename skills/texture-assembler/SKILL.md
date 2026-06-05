---
name: texture-assembler
description: >-
  Guide for using the texassemble command-line tool to create cubemaps, volume maps, texture arrays, and other assembled DDS textures from individual images.
  Use this skill when asked about creating cubemaps, volume textures, texture arrays, merging channels, converting animated GIFs to textures, or extracting
  cubemap faces to cross/strip layouts.
license: MIT
metadata:
  author: chuckw
  version: "1.0"
---

# Texassemble Texture Assembler

This skill provides guidance for using the **texassemble** command-line tool to create DDS files containing cubemaps, volume maps, or texture arrays from individual images.

## When to Use

Invoke this skill when:

- Creating cubemaps from six individual face images.
- Creating volume (3D) textures from multiple slice images.
- Creating 1D or 2D texture arrays from multiple images.
- Creating cubemap arrays from multiple sets of face images.
- Extracting cubemap faces into cross, strip, or tee layout images.
- Creating cubemaps from cross, strip, or tee layout images.
- Merging channels from two images into one output.
- Converting animated GIFs into texture arrays.
- Creating textures with custom mipmap levels from individual mip images.

## Installation

### winget (Recommended)

```bash
winget install Microsoft.DirectXTex.Texassemble
```

### vcpkg

```bash
vcpkg install directxtex[tools]
```

## Syntax

```plaintext
texassemble <command> [options] [--file-list <filename>] <file-name(s)>
```

Input files can be `dds`, `tga`, `hdr`, or any WIC-supported format (`bmp`, `jpg`, `png`, `jxr`, `heif`, `webp`, etc.).

The command-line uses Windows-style `-` or `/` for options. It also supports `--version`, `--help`, and GNU long-option style parameters with `--`.

## Commands

| Command | Description |
| --- | --- |
| `cube` | Creates a cubemap from six face images (ordered: +X, -X, +Y, -Y, +Z, -Z) |
| `volume` | Creates a volume (3D) texture from two or more images (depth = image count) |
| `array` | Creates a 1D or 2D texture array from two or more images (requires FL 10.0+) |
| `cubearray` | Creates a cubemap array from a multiple of six images (requires FL 10.1+) |
| `h-cross` | Extracts cubemap faces into a horizontal cross layout image |
| `v-cross` | Extracts cubemap faces into a vertical cross layout image |
| `v-cross-fnz` | Same as `v-cross` but flips the negative-Z face |
| `h-strip` | Extracts cubemap faces into a horizontal strip image |
| `v-strip` | Extracts cubemap faces into a vertical strip image |
| `h-tee` | Extracts cubemap faces into a horizontal "T" layout image |
| `cube-from-hc` | Creates a cubemap from a horizontal cross layout image |
| `cube-from-vc` | Creates a cubemap from a vertical cross layout image |
| `cube-from-vc-fnz` | Creates a cubemap from a vertical cross layout (flipped -Z) image |
| `cube-from-ht` | Creates a cubemap from a horizontal tee layout image |
| `cube-from-hs` | Creates a cubemap from a horizontal strip image |
| `cube-from-vs` | Creates a cubemap from a vertical strip image |
| `array-strip` | Extracts a 1D/2D array into a horizontal strip image |
| `merge` | Creates a texture by merging channels from two input images |
| `gif` | Converts an animated GIF into a texture array |
| `from-mips` | Creates a 2D mipmapped texture from individual mip images |
| `cube-from-mips` | Creates a cubemap texture from individual mip images |

## Common Workflows

### Create a cubemap from six face images

```plaintext
texassemble cube -w 256 -h 256 -o cubemap.dds posx.jpg negx.jpg posy.jpg negy.jpg posz.jpg negz.jpg
```

### Create a volume (3D) texture

```plaintext
texassemble volume -w 256 -h 256 -o volume.dds slice0.png slice1.png slice2.png slice3.png
```

### Create a texture array

```plaintext
texassemble array -o array.dds frame0.png frame1.png frame2.png frame3.png
```

### Extract cubemap to horizontal cross layout

```plaintext
texassemble h-cross cubemap.dds
```

### Create cubemap from a vertical cross image

```plaintext
texassemble cube-from-vc -o cubemap.dds cross_layout.png
```

### Merge channels from two images

```plaintext
texassemble merge -o combined.png color.png alpha_source.bmp
```

### Merge with custom swizzle (roughness into alpha)

```plaintext
texassemble merge --swizzle rgbR -o packed.png albedo.png roughness.png
```

### Convert animated GIF to texture array

```plaintext
texassemble gif -o animation.dds animated.gif
```

### Create mipmapped texture from individual mip images

```plaintext
texassemble from-mips -o texture.dds mip0.png mip1.png mip2.png mip3.png
```

## Options Reference

See [reference/options.md](reference/options.md) for the complete options reference.

### Quick Reference (most common options)

| Option | Description |
| --- | --- |
| `-o <file>` | Output filename (default: DDS based on first input, BMP for cross/strip) |
| `-f <format>` | Output DXGI format (e.g., `R8G8B8A8_UNORM`, `B8G8R8A8_UNORM`) |
| `-w <n>` / `-h <n>` | Output width / height in pixels |
| `-if <filter>` | Image filter (`POINT`, `LINEAR`, `CUBIC`, `FANT`, `BOX`, `TRIANGLE`) |
| `-srgb` | Input and output are sRGB |
| `-y` | Overwrite existing output file |
| `-dx10` | Force DX10 header extension |
| `-fl <level>` | Target feature level (default: `11.0`, max texture size: 16384) |
| `--swizzle <mask>` | Channel swizzle for `merge` command |
| `--strip-mips` | Strip mipmaps from input textures |
| `-r` | Recursive wildcard search |
| `-nologo` | Suppress copyright message |

## Output Format

By default, texassemble writes DDS files. The output filename is derived from the first input file with a `.dds` extension. For cross/strip extraction commands, the default output is a `.bmp` file and you can specify any supported image extension.

## Notes

- The tool does **not** support mipmap generation or texture compression. Use the **texture-converter** skill to further process the resulting DDS file.
- Any texture-compressed input file is decompressed on load.
- Partial cubemaps are not supported; all six faces must be provided.
- Cubemap face order is: positive-X, negative-X, positive-Y, negative-Y, positive-Z, negative-Z.
- Support for OpenEXR (`exr`) format requires building from source with `USE_OPENEXR` defined.
- Additional WIC codecs (HEIF, WEBP) work automatically if installed on the system.
- Texture arrays require Direct3D feature level 10.0 or better hardware.
- Cubemap arrays with more than one cubemap require feature level 10.1 or better.

## Further Reading

- [Texassemble Wiki](https://github.com/microsoft/DirectXTex/wiki/Texassemble)
- [DirectXTex Documentation](https://github.com/microsoft/DirectXTex/wiki)
