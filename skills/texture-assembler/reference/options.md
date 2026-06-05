# Texassemble Options Reference

Complete reference for all texassemble command-line options. Options use Windows-style `-` or `/` prefixes. GNU long-form options use `--`.

## File Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-r` | | Recursive wildcard search for input files |
| `-flist <file>` | `--file-list <file>` | Read input filenames from a text file (one per line). Lines starting with `#` are comments |
| `-o <file>` | | Output filename. Default: DDS based on first input (BMP for cross/strip commands) |
| `-l` | `--to-lowercase` | Force output path and filename to lowercase |
| `-y` | `--overwrite` | Overwrite existing output file. By default, the tool aborts if the output exists |

## Pixel Format Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-f <format>` | `--format <format>` | Output DXGI format (without `DXGI_FORMAT_` prefix). If omitted, uses format of first input |

## Resizing Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-w <n>` | `--width <n>` | Output width in pixels |
| `-h <n>` | `--height <n>` | Output height in pixels. If no size given, uses size of first input |

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
| | `--tonemap` | Apply Reinhard tonemap operator (HDR→LDR based on max luminosity) |

## Alpha Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-alpha` | | Convert premultiplied alpha to straight (non-premultiplied) alpha |
| `-sepalpha` | `--separate-alpha` | Separate alpha channel for resize |

## DirectDraw Surface (DDS) File Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-dx10` | | Force DX10 header extension. Enables alpha mode metadata. May not be compatible with legacy D3DX10/D3DX11 |

## Direct3D Options

| Option | Long Form | Description |
| --- | --- | --- |
| `-fl <level>` | `--feature-level <level>` | Target feature level constraining maximum texture size |

### Feature Levels

| Level | Max Texture Size |
| --- | --- |
| `9.1`, `9.2` | 2048 |
| `9.3` | 4096 |
| `10.0`, `10.1` | 8192 |
| `11.0`, `11.1`, `12.0`, `12.1` | 16384 (default) |

## GIF Options

| Option | Long Form | Description |
| --- | --- | --- |
| | `--gif-bg-color` | Use the background color metadata from the GIF file instead of transparent background (default is transparent, matching browser behavior) |

## Merge Options

| Option | Long Form | Description |
| --- | --- | --- |
| | `--swizzle <mask>` | Channel swizzle mask for the `merge` command (default: `rgbB`) |

### Swizzle Mask Format

The swizzle mask is 1–4 characters using HLSL-style channel notation:

- **Lowercase** letters pull from the **first** image: `r`, `g`, `b`, `a`, `x`, `y`, `z`, `w`
- **Uppercase** letters pull from the **second** image: `R`, `G`, `B`, `A`, `X`, `Y`, `Z`, `W`
- `0` sets the channel to zero
- `1` sets the channel to maximum

Examples:
- `rgbB` — RGB from first image, blue of second image as alpha (default)
- `rgbA` — RGB from first image, alpha of second image as alpha
- `RGBA` — All channels from the second image
- `rgbG` — RGB from first image, green of second image as alpha
- `rg01` — Red and green from first image, blue=0, alpha=max

## Miscellaneous Options

| Option | Long Form | Description |
| --- | --- | --- |
| | `--strip-mips` | For `cube`, `array`, `volume`, or `cubearray` commands: strips mipchain from mip-mapped input textures |
| `-nologo` | | Suppress copyright message |
| | `--version` | Display version information |
| | `--help` | Display usage help |
