# Texconv Options Reference

Complete reference for all texconv command-line options. Options use Windows-style `-` or `/` prefixes. GNU long-form options use `--` and accept space, `=`, or `:` as value separators (e.g., `--width 100`, `--width=100`, `--width:100`).

## File Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-r` | | Recursive wildcard search. Use `-r:keep` to preserve subdirectory structure, `-r:flatten` (default) to flatten |
| `-flist <file>` | `--file-list <file>` | Read input filenames from a text file (one per line). Lines starting with `#` are comments |
| `-o <dir>` | | Output directory |
| `-px <string>` | `--prefix <string>` | Prefix for output filename |
| `-sx <string>` | `--suffix <string>` | Suffix for output filename |
| `-l` | `--to-lowercase` | Force output path and filename to lowercase |
| `-y` | `--overwrite` | Overwrite existing output files. By default, existing files are skipped |

## File and Pixel Format Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-ft <type>` | `--file-type <type>` | Output file type (default: `dds`) |
| `-f <format>` | `--format <format>` | Output DXGI format (without `DXGI_FORMAT_` prefix) |

### Supported Output File Types

| Type | Extensions | Notes |
| --- | --- | --- |
| `bmp` | `.bmp` | Windows BMP |
| `jpg`, `jpeg` | `.jpg` | JPEG (lossy, use `-wicq` for quality) |
| `png` | `.png` | PNG (lossless with alpha) |
| `dds`, `ddx` | `.dds` | DirectDraw Surface (default) |
| `tga` | `.tga` | Truevision TGA |
| `hdr` | `.hdr` | Radiance RGBE |
| `tif`, `tiff` | `.tif` | Tagged Image File Format |
| `wdp`, `hdp`, `jxr` | `.jxr` | JPEG XR / Windows Media Photo |
| `ppm`, `pfm` | `.ppm`, `.pfm` | Portable PixMap / FloatMap (Netpbm) |
| `exr` | `.exr` | OpenEXR (requires `USE_OPENEXR` build) |

### Common Format Aliases

| Alias | DXGI Format |
| --- | --- |
| `DXT1` | `BC1_UNORM` |
| `DXT2`, `DXT3` | `BC2_UNORM` |
| `DXT4`, `DXT5` | `BC3_UNORM` |
| `BGRA` | `B8G8R8A8_UNORM` |
| `BGR` | `B8G8R8X8_UNORM` |
| `FP16` | `R16G16B16A16_FLOAT` |
| `FP32` | `R32G32B32A32_FLOAT` |
| `RGBA` | `R8G8B8A8_UNORM` |

## Resize and Mipmap Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-w <n>` | `--width <n>` | Output width in pixels |
| `-h <n>` | `--height <n>` | Output height in pixels |
| `-m <n>` | `--mip-levels <n>` | Mipmap levels to generate. `0` = full chain (default for DDS), `1` = no mipmaps |
| `-pow2` | `--fit-power-of-2` | Fit to power-of-2 dimensions, minimizing aspect ratio changes. Max size governed by `-fl` |

## Filtering Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-if <filter>` | `--image-filter <filter>` | Image filter for resizing (see below) |
| `-wrap` | | Wrap texture addressing mode for filtering |
| `-mirror` | | Mirror texture addressing mode for filtering |
| `-nowic` | | Force non-WIC filtering code paths |

### Filter Values

| Filter | Description |
| --- | --- |
| `POINT` | Nearest-neighbor (no interpolation) |
| `LINEAR` | Bilinear interpolation |
| `CUBIC` | Bicubic interpolation |
| `FANT` | Fant (equivalent to box for downscaling) |
| `BOX` | Box filter |
| `TRIANGLE` | Triangle (tent) filter |

Each filter also has `_DITHER` (4×4 ordered dither) and `_DITHER_DIFFUSION` (error diffusion dither) variants:
- `POINT_DITHER`, `POINT_DITHER_DIFFUSION`
- `LINEAR_DITHER`, `LINEAR_DITHER_DIFFUSION`
- `CUBIC_DITHER`, `CUBIC_DITHER_DIFFUSION`
- `FANT_DITHER`, `FANT_DITHER_DIFFUSION`
- `BOX_DITHER`, `BOX_DITHER_DIFFUSION`
- `TRIANGLE_DITHER`, `TRIANGLE_DITHER_DIFFUSION`

## Colorspace Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-srgb` | | Both input and output are sRGB (gamma ~2.2) |
| `-srgbi` | `--srgb-in` | Only input is sRGB |
| `-srgbo` | `--srgb-out` | Only output is sRGB |
| | `--rotate-color <rot>` | Color rotation / HDR10 curve conversion |
| `-nits <n>` | `--paper-white-nits <n>` | Paper-white nits for HDR10 conversions (default: 200.0, max: 10000) |
| | `--tonemap` | Apply Reinhard tonemap operator (HDR→LDR based on max luminosity) |
| | `--ignore-srgb` | Ignore colorspace metadata in source WIC/TGA files |

### Color Rotation Values

| Value | Description |
| --- | --- |
| `709to2020` | Rec.709 → Rec.2020 color primaries |
| `2020to709` | Rec.2020 → Rec.709 color primaries |
| `709toHDR10` | Rec.709 → Rec.2020 + ST.2084 curve (HDR10 signal) |
| `HDR10to709` | HDR10 signal → Rec.709 linear values |
| `P3to2020` | DCI-P3 → Rec.2020 color primaries |
| `P3toHDR10` | DCI-P3 → Rec.2020 + ST.2084 curve (HDR10 signal) |
| `709toDisplayP3` | Rec.709 → Display-P3 (D65 white point) |
| `DisplayP3to709` | Display-P3 (D65 white point) → Rec.709 |

## Alpha Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-pmalpha` | `--premultiplied-alpha` | Convert to premultiplied alpha. Sets `DDS_ALPHA_MODE_PREMULTIPLIED` unless alpha is all opaque |
| `-alpha` | | Convert premultiplied alpha to straight (non-premultiplied) alpha |
| `-sepalpha` | `--separate-alpha` | Separate alpha channel for resize/mipmap generation. Implies `DDS_ALPHA_MODE_CUSTOM` |
| `-at <n>` | `--alpha-threshold <n>` | Alpha threshold for 1-bit alpha formats (BC1, RGBA5551). Default: 0.5 |
| | `--keep-coverage <n>` | Preserve alpha coverage in generated mipmaps for alpha test reference value (0 to 1) |
| `-c <hex>` | `--color-key <hex>` | Chromakey color in hexadecimal RGB. Replaces matching color with alpha 0.0. E.g., `00FF00` for green |

## Block Compression (BC) Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-bc <flags>` | `--block-compress <flags>` | BC compression flags (combinable) |
| `-gpu <n>` | | Use GPU at adapter index for BC6H/BC7 (default: 0) |
| `-nogpu` | | Force software codec for BC6H/BC7 |
| | `--single-proc` | Disable OpenMP multi-threading for BC6H/BC7 |
| `-aw <n>` | `--alpha-weight <n>` | Alpha weight for BC7 GPU compressor error metric (default: 1.0) |

### BC Compression Flags

| Flag | Applies To | Description |
| --- | --- | --- |
| `u` | BC1–BC3 | Uniform weighting (instead of perceptual) |
| `d` | BC1–BC3 | Enable dithering |
| `q` | BC7 | Quick/minimal compression (mode 6 only) |
| `x` | BC7 | Maximum compression (enables modes 0 & 2) |

## Normal Map Generation Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-nmap <flags>` | `--normal-map <flags>` | Convert height map to normal map (flags below) |
| `-nmapamp <n>` | `--normal-map-amplitude <n>` | Normal map amplitude (default: 1.0) |
| | `--invert-y` | Invert green channel (OpenGL↔Direct3D normal convention) |
| | `--reconstruct-z` | Rebuild Z (blue) channel assuming X/Y are normals (for BC5 → RGBA) |
| | `--x2-bias` | Enable `*2 - 1` conversion for unorm↔float/snorm (used with normal maps) |

### Normal Map Flags

Must include one channel source flag (`r`, `g`, `b`, `a`, or `l`):

| Flag | Description |
| --- | --- |
| `r` | Use red channel as height |
| `g` | Use green channel as height |
| `b` | Use blue channel as height |
| `a` | Use alpha channel as height |
| `l` | Use luminance (from RGB) as height |

Optional modifier flags:

| Flag | Description |
| --- | --- |
| `m` | Mirror in U & V (default: wrap) |
| `u` | Mirror in U only |
| `v` | Mirror in V only |
| `i` | Invert sign of computed normal |
| `o` | Compute rough occlusion term in alpha channel |

## Image Operation Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-hflip` | `--horizontal-flip` | Horizontal flip |
| `-vflip` | `--vertical-flip` | Vertical flip |
| | `--swizzle <mask>` | Swizzle channels using HLSL-style mask (1–4 chars: `rgba`, `rrra`, `rg01`, etc.). `0` = zero, `1` = max |

## DirectDraw Surface (DDS) File Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-tu` | `--typeless-unorm` | Treat typeless formats as UNORM |
| `-tf` | `--typeless-float` | Treat typeless formats as FLOAT |
| `-dword` | `--dword-alignment` | Use DWORD alignment for legacy 24bpp files |
| | `--bad-tails` | Tolerate incorrect DXTn mipchain blocks smaller than 4×4 |
| | `--permissive` | Allow malformed/variant DDS headers to load |
| | `--ignore-mips` | Load only top-level mipmap |
| | `--fix-bc-4x4` | Resize non-multiple-of-4 BC textures to valid dimensions |
| `-xlum` | `--expand-luminance` | Expand L8/A8L8/L16 to 8:8:8:8 or 16:16:16:16 (instead of 1 or 2 channel) |
| `-dx10` | | Force DX10 header extension (enables alpha mode metadata, may break legacy readers) |
| `-dx9` | | Force legacy DX9 headers (fails for BC6/BC7/UINT/SINT, strips SRGB) |

## Targa TruVision (TGA) File Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-tga20` | | Include TGA 2.0 extension area (gamma, alpha mode, timestamp) |
| | `--tga-zero-alpha` | Preserve all-zero alpha channels as-is (default treats as opaque) |

## Windows Image Component (WIC) Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-wicq <n>` | `--wic-quality <n>` | Image quality 0.0–1.0 (applies to `jpg`, `tif`, `heif`, `jxr`) |
| | `--wic-lossless` | Lossless encoding (applies to `jxr`). Ignores wic-quality |
| | `--wic-uncompressed` | Uncompressed encoding (applies to `tif`, `heif`). Ignores wic-quality |
| | `--wic-multiframe` | Write multiframe images (applies to `gif`, `tif`). Default writes first frame only |

## Direct3D Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-fl <level>` | `--feature-level <level>` | Target feature level constraining max texture size |

### Feature Levels

| Level | Max 2D Texture Size |
| --- | --- |
| `9.1`, `9.2` | 2048 |
| `9.3` | 4096 |
| `10.0`, `10.1` | 8192 |
| `11.0`, `11.1`, `12.0`, `12.1`, `12.2` | 16384 (default) |

## Miscellaneous Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-nologo` | | Suppress copyright message |
| | `--timing` | Display compression timing information |
| | `--version` | Display version information |
| | `--help` | Display usage help |
