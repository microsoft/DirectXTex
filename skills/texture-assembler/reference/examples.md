# Texassemble Usage Examples

This document provides practical examples for common texture assembly scenarios.

## Cubemaps

### Create a cubemap from six JPEG face images

```plaintext
texassemble cube -w 256 -h 256 -o cubemap.dds posx.jpg negx.jpg posy.jpg negy.jpg posz.jpg negz.jpg
```

Faces must be ordered: positive-X, negative-X, positive-Y, negative-Y, positive-Z, negative-Z.

### Create a cubemap with explicit format

```plaintext
texassemble cube -w 512 -h 512 -f R16G16B16A16_FLOAT -o hdr_cubemap.dds posx.hdr negx.hdr posy.hdr negy.hdr posz.hdr negz.hdr
```

### Create a cubemap from a horizontal cross layout

```plaintext
texassemble cube-from-hc -o cubemap.dds cross_layout.png
```

### Create a cubemap from a vertical cross layout

```plaintext
texassemble cube-from-vc -o cubemap.dds vertical_cross.png
```

### Create a cubemap from a vertical cross (flipped -Z)

```plaintext
texassemble cube-from-vc-fnz -o cubemap.dds vertical_cross_fnz.png
```

### Create a cubemap from a horizontal strip

```plaintext
texassemble cube-from-hs -o cubemap.dds strip.png
```

### Create a cubemap from a vertical strip

```plaintext
texassemble cube-from-vs -o cubemap.dds strip.png
```

### Create a cubemap from a horizontal tee layout

```plaintext
texassemble cube-from-ht -o cubemap.dds tee_layout.png
```

## Cubemap Extraction

### Extract cubemap faces to horizontal cross

```plaintext
texassemble h-cross cubemap.dds
```

Output defaults to `cubemap.bmp`. Specify a different format with `-o`:

```plaintext
texassemble h-cross -o cubemap_cross.png cubemap.dds
```

### Extract to vertical cross

```plaintext
texassemble v-cross -o cubemap_vcross.png cubemap.dds
```

### Extract to vertical cross with flipped -Z face

```plaintext
texassemble v-cross-fnz -o cubemap_vcross.png cubemap.dds
```

### Extract to horizontal strip

```plaintext
texassemble h-strip -o cubemap_strip.png cubemap.dds
```

### Extract to vertical strip

```plaintext
texassemble v-strip -o cubemap_vstrip.png cubemap.dds
```

### Extract to horizontal tee

```plaintext
texassemble h-tee -o cubemap_tee.png cubemap.dds
```

## Volume (3D) Textures

### Create a volume texture from image slices

```plaintext
texassemble volume -w 256 -h 256 -o volume.dds slice0.png slice1.png slice2.png slice3.png
```

The number of input images determines the depth of the volume texture.

### Create a volume texture with sRGB colorspace

```plaintext
texassemble volume -w 128 -h 128 -srgb -o volume_srgb.dds slice*.png
```

### Create a volume texture from mixed formats

```plaintext
texassemble volume -w 256 -h 256 -f R8G8B8A8_UNORM -o volume.dds lena.jpg fishingboat.jpg peppers.tiff
```

## Texture Arrays

### Create a 2D texture array

```plaintext
texassemble array -o array.dds frame0.png frame1.png frame2.png frame3.png
```

The size is taken from the first input image; subsequent images are resized to match.

### Create a texture array with explicit size

```plaintext
texassemble array -w 512 -h 512 -o array.dds *.png
```

### Create a texture array with file list

```plaintext
texassemble array -o array.dds --file-list textures.txt
```

### Extract array to horizontal strip

```plaintext
texassemble array-strip -o strip.png array.dds
```

## Cubemap Arrays

### Create a cubemap array (two cubemaps = 12 images)

```plaintext
texassemble cubearray -w 256 -h 256 -o cubearray.dds env0_px.jpg env0_nx.jpg env0_py.jpg env0_ny.jpg env0_pz.jpg env0_nz.jpg env1_px.jpg env1_nx.jpg env1_py.jpg env1_ny.jpg env1_pz.jpg env1_nz.jpg
```

Input must be a multiple of six images. Each group of six forms one cubemap.

## Merging Channels

### Merge RGB from one image with alpha from another (default behavior)

```plaintext
texassemble merge -o combined.png albedo.png alpha_mask.bmp
```

Default swizzle is `rgbB`: RGB from first image, blue channel of second as alpha.

### Merge with alpha channel from second image

```plaintext
texassemble merge --swizzle rgbA -o combined.png color.png mask.png
```

### Pack roughness into alpha channel

```plaintext
texassemble merge --swizzle rgbR -o packed.dds albedo.png roughness.png
```

### Create ORM map (Occlusion, Roughness, Metallic)

```plaintext
texassemble merge --swizzle Rgb -o orm_partial.png occlusion.png roughness.png
```

Then merge metallic into the blue channel with a second pass:

```plaintext
texassemble merge --swizzle rgB -o orm.png orm_partial.png metallic.png
```

### Use all channels from second image

```plaintext
texassemble merge --swizzle RGBA -o output.dds unused.png source.png
```

### Zero out alpha, keep RGB

```plaintext
texassemble merge --swizzle rgb0 -o no_alpha.png color.png unused.png
```

## Animated GIFs

### Convert animated GIF to DDS texture array

```plaintext
texassemble gif -o animation.dds animated.gif
```

Output format is `DXGI_FORMAT_B8G8R8A8_UNORM` with one array item per frame.

### Convert GIF using file background color

```plaintext
texassemble gif --gif-bg-color -o animation.dds animated.gif
```

## Custom Mipmaps

### Create a 2D texture with custom mip levels

```plaintext
texassemble from-mips -o texture.dds mip0_256x256.png mip1_128x128.png mip2_64x64.png mip3_32x32.png
```

Images are ordered from largest (mip 0) to smallest.

### Create a cubemap with custom mip levels

```plaintext
texassemble cube-from-mips -o cubemap.dds mip0_face0.png mip0_face1.png ... mip1_face0.png ...
```

## Pipeline Examples

### Create cubemap then compress with texconv

```plaintext
texassemble cube -w 512 -h 512 -o cubemap_raw.dds posx.hdr negx.hdr posy.hdr negy.hdr posz.hdr negz.hdr
texconv -f BC6H_UF16 -m 0 -y cubemap_raw.dds
```

### Create texture array then add mipmaps and compression

```plaintext
texassemble array -w 1024 -h 1024 -o sprites_raw.dds sprite*.png
texconv -f BC7_UNORM_SRGB -srgb -m 0 -y sprites_raw.dds
```

### Create volume texture then compress

```plaintext
texassemble volume -w 64 -h 64 -o noise3d_raw.dds noise*.png
texconv -f BC4_UNORM -m 0 -y noise3d_raw.dds
```

## Miscellaneous

### Use recursive wildcard for inputs

```plaintext
texassemble array -r -w 256 -h 256 -o array.dds C:\Textures\*.png
```

### Suppress copyright message

```plaintext
texassemble cube -nologo -o cubemap.dds face*.png
```

### Force DX10 header extension

```plaintext
texassemble array -dx10 -o array.dds *.png
```

### Strip mipmaps from mip-mapped input textures

```plaintext
texassemble array --strip-mips -o array.dds mipped_tex1.dds mipped_tex2.dds
```

### Overwrite existing output

```plaintext
texassemble cube -y -o cubemap.dds face*.jpg
```

### Apply tonemap when assembling HDR sources

```plaintext
texassemble cube --tonemap -w 256 -h 256 -o cubemap_ldr.dds posx.hdr negx.hdr posy.hdr negy.hdr posz.hdr negz.hdr
```
