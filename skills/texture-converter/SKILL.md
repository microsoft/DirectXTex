---
name: texture-converter
description: >-
  Guide for using the texconv command-line texture conversion tool. Use this skill when asked about converting textures, compressing images
  to DDS/BC formats, generating mipmaps, resizing textures, creating normal maps, or performing HDR tone mapping from the command line.
license: MIT
metadata:
  author: chuckw
  version: "1.0"
---

# Texconv Texture Converter

This skill provides guidance for using the **texconv** command-line tool to convert, compress, resize, and process texture images.

## When to Use

Invoke this skill when:

- Converting image files (PNG, BMP, JPG, TGA, HDR, etc.) to DDS or other formats.
- Applying block compression (BC1–BC7) to textures for runtime use.
- Generating mipmaps for texture files.
- Resizing textures or fitting them to power-of-2 dimensions.
- Creating normal maps from height maps.
- Performing HDR tone mapping or colorspace conversions.
- Batch-processing texture files using wildcards or file lists.

## Installation

### winget (Recommended)

```bash
winget install Microsoft.DirectXTex.Texconv
```

### vcpkg

```bash
vcpkg install directxtex[tools]
```

## Syntax

```
texconv [options] [--file-list <filename>] <file-name(s)>
```

Input files can be `dds`, `tga`, `hdr`, `phm`, `ppm`, `pfm`, or any WIC-supported format (`bmp`, `jpg`, `png`, `jxr`, `heif`, `webp`, etc.).

The command-line uses Windows-style `-` or `/` for options. It also supports `--version`, `--help`, and GNU long-option style parameters with `--`.

## Common Workflows

### Convert to BC7 compressed DDS with mipmaps

```
texconv -f BC7_UNORM -m 0 -y diffuse.png
```

### Convert to BC1 (DXT1) with power-of-2 resize

```
texconv -pow2 -f BC1_UNORM cat.jpg
```

### Batch convert all PNG files recursively

```
texconv -r C:\Textures\*.png
```

### Convert HDR to BMP with tone mapping

```
texconv myimage.hdr -tonemap -ft BMP
```

### Generate normal map from height map

```
texconv -nmap lo -nmapamp 2 -f R8G8B8A8_UNORM heightmap.png
```

### Convert for legacy Direct3D 9 with feature level cap

```
texconv -pow2 -fl 9.3 -f BC3_UNORM -m 1 *.bmp
```

### Output to a specific directory with prefix/suffix

```
texconv -o output_dir -px hd_ -sx _bc7 -f BC7_UNORM *.png
```

### Convert to PNG from DDS

```
texconv -ft png -y texture.dds
```

### Premultiplied alpha conversion

```
texconv -pmalpha -f BC3_UNORM ui_element.png
```

### HDR10 colorspace conversion

```
texconv --rotate-color 709toHDR10 -nits 200 -f R10G10B10A2_UNORM scene.hdr
```

### Swizzle channels

```
texconv --swizzle rrrg -f BC5_UNORM normal.png
```

## Options Reference

See [reference/options.md](reference/options.md) for the complete options reference.

### Quick Reference (most common options)

| Option | Description |
| --- | --- |
| `-f <format>` | Output DXGI format (e.g., `BC7_UNORM`, `R8G8B8A8_UNORM`) |
| `-ft <type>` | Output file type (`dds`, `png`, `bmp`, `jpg`, `tga`, `hdr`, etc.) |
| `-o <dir>` | Output directory |
| `-y` | Overwrite existing files |
| `-m <n>` | Mipmap levels (`0` = full chain, `1` = none) |
| `-w <n>` / `-h <n>` | Output width / height in pixels |
| `-pow2` | Fit to power-of-2 dimensions |
| `-if <filter>` | Image filter (`POINT`, `LINEAR`, `CUBIC`, `FANT`, `BOX`, `TRIANGLE`) |
| `-srgb` | Input and output are sRGB |
| `-pmalpha` | Convert to premultiplied alpha |
| `-bc <flags>` | BC compression flags (`u`, `d`, `q`, `x`) |
| `-nmap <flags>` | Generate normal map from height map |
| `-r` | Recursive wildcard search |
| `-nologo` | Suppress copyright message |

## Output Format

By default, texconv writes DDS files. The output filename matches the input filename with the appropriate extension. Use `-ft` to change the output format, `-o` for output directory, and `-px`/`-sx` for filename prefix/suffix.

## Notes

- When no size is specified (`-w` / `-h`), the output size matches the input.
- DDS output defaults to generating all mipmap levels (`-m 0`). Use `-m 1` to suppress mipmaps.
- BC6H and BC7 compression uses GPU acceleration by default when available. Use `-nogpu` to force CPU.
- For cubemaps, volume maps, or texture arrays from individual files, use **texassemble** instead.
- Support for OpenEXR (`exr`) format requires building from source with `USE_OPENEXR` defined.
- Additional WIC codecs (HEIF, WEBP) work automatically if installed on the system.

## Further Reading

- [Texconv Wiki](https://github.com/microsoft/DirectXTex/wiki/Texconv)
- [DirectXTex Documentation](https://github.com/microsoft/DirectXTex/wiki)
- [Image Formats](https://github.com/microsoft/DirectXTex/wiki/Image-formats)
- [Filter Flags](https://github.com/microsoft/DirectXTex/wiki/Filter-Flags)
