# DirectXTex texture processing library

http://go.microsoft.com/fwlink/?LinkId=248926

Release available for download on [GitHub](https://github.com/microsoft/DirectXTex/releases)

## Release History

### October 17, 2022
* Minor fix for ``CompileShaders.cmd`` to address additional 'paths with spaces' issues
* Minor CMake and CMakePresets updates
* Code review

### July 29, 2022
* Added ``MakeLinear`` DXGI_FORMAT utility function.
* *breaking change* ``CreateTextureEx`` and ``CreateShaderResourceViewEx`` functions now use ``CREATETEX_FLAGS`` instead of a ``bool forceSRGB`` parameter.
* Updates for MinGW ABI fixes for DirectX12 in the latest DirectX-Headers.
* CMake and MSBuild project updates
* Code review
* `DDSTextureLoader11` and ``DDSTextureLoader12`` sync'd up with *DirectX Tool Kit* July 2022 changes.

### May 9, 2022
* TGA reader updated to support 24-bit paletted uncompressed color-mapped images (used by a DCC application)
* Added `IsBGR` utility method
* Workaround for driver issue on some systems using DirectX 11 `Capture` method
* Fix for problem with resizing/mipmaps generation for HDR content using box/fant filter which should avoid going through WIC code paths
* Minor updates for VS 2022 (17.2)
* CMake project updates (now supports MSVC, clang/LLVM, and MinGW)
* Updated D3DX12 internal copy with latest changes from DirectX-Headers GitHub
* Retired VS 2017 projects
* Code cleanup
* Reformat source using updated .editorconfig settings
* texconv: Improve `-nmap` handling for 16-bit sources going to BC formats

### March 24, 2022
* Fixed end-point bounds issue with BC6H CPU compressor if none of the pixels are in 0-1 range
* Fixed bug in alpha-to-coverage computation
* Add support for installable WIC codecs for HEIF and WEBP (if present)
* Update build switches for SDL recommendations
* CMake project updates and UWP platform CMakePresets
* Code cleaup for tools
* Optional C++17 usage in a few places

### February 28, 2022
* Updated D3DX12 internal copy with latest changes from GitHub
* Code and project review including fixing clang v13 warnings
* Added CMakePresets.json

### November 8, 2021
* VS 2022 support
* Updated D3DX12 internal copy with latest change from GitHubf
* Minor code and project review
* VS 2017 projects updated to require the Windows 10 SDK (19401)
* texassemble/texconv: Updated with 12.2 for ``-fl`` switch
* texassemble/texconv/texdiag: Fixed potential locale issue with ``-flist``

### September 28, 2021
* Minor code and project cleanup

### August 1, 2021
* Fixed weighting bug in custom linear filtering for wrap/mirroring
* Added VS 2022 Preview projects
* texconv: Made default output extension to be lower-case like most Windows applications
* texconv: updated colorspace rotation names for -rotatecolor switch
* texassemble, texconv: Order of operations fix for -swizzle using 0, 1
* Minor code review

### June 9, 2021
* Minor bug fix for metadata behavior when using ``DDS_FLAGS_NO_16BPP`` flag for B5G6R5 content
* texdiag: added ``-c`` and ``-t`` switches for diff command
* texconv: Fixed bug in ``-m`` switch handling when resizing changes the max mipmap count
* texconv et al: improved ``-flist`` switch to support wildcards and file exclusions
* texconv et al: Added 'BGR' alias to ``-f`` switch for the B8G8R8X8_UNORM format
* WICTextureLoader / DDSTextureLoader12 updated to use typed enum bitmask flags for loadFlags parameter
* Minor code review

### April 6, 2021
* DDS reader updated to accept nVidia Texture Tool v1 single-channel and dual-channel files marked as RGB instead of LUMINANCE
* Fixed TGA reader regression with files smaller than 26 bytes total
* Removed use of ``CreateStreamOnHGlobal``, ``CreateStreamOverRandomAccessStream``, and ``SHCreateMemStream`` for WICToMemory functions
* Fix for the DirectX 12 ``CaptureTexture`` for reserved and MSAA resources
* Minor code and project cleanup
* texassemble: added ``-stripmips`` switch
* texassemble, texconv: the ``swizzle`` switch now accepts ``0`` and ``1`` values in swizzle masks
* texconv: added "709toDisplayP3" and "DisplayP3to709" to ``-rotatecolor`` switch
* texconv: Fixed ``-reconstructz`` for UNORM formats
* texassemble, texconv, texdiag: Updated with  descriptions for HRESULT failure codes, and always uses exit code 1 on failure

### January 9, 2021
* Windows Subsystem for Linux support
* Code review for improved conformance
* CMake updated to support package install
* texassemble: Merge command now supports ``-swizzle`` option
* texconv: Updated with ``-r:keep`` and ``-swizzle`` options

### November 11, 2020
* Use ``SHCreateMemStream`` instead of ``CreateStreamOnHGlobal`` for Win32 on Windows 8.x or Windows 10
* Updated D3DX12 internal copy with latest change from GitHub
* Minor code and project cleanup

### September 30, 2020
* Added ``TGA_FLAGS`` [flags](https://github.com/microsoft/DirectXTex/wiki/TGA-I-O-Functions#related-flags) to TGA reader/writer to control RGB vs. BGR, handling for all zero alpha channels, and TGA 2.0 colorspace metadata
  * TGA reader will now return a ``DXGI_FORMAT_*_SRGB`` format if TGA 2.0 colorspace metadata contains 2.2 or 2.4 gamma
  * Added forwarders for existing non-flags TGA functions, so there are no breaking changes
* ``R16_SNORM`` and ``R8_SNORM`` pixel write code updated to round instead of truncate to better match DirectXMath behavior
* Fixed bug in standalone WICTextureLoader for DX11/DX12 that resulted in ``WINCODEC_ERR_INSUFFICIENTBUFFER`` for some resize requests
* Added ``Ex`` variants for DDSTextureLoader/WICTextureLoader for DX9 to support loading resources for non-DEFAULT pools
* Minor code and project cleanup

### August 15, 2020
* Added ``DDS_FLAGS_ALLOW_LARGE_FILES`` flag for DDS loader to allow textures with dimensions that are too big for Direct3D
* Added ``FormatDataType`` function
* Fixed bug with DX12 ``Capture`` with 'small alignment' textures
* Code review and project updates
* Added GDK projects
* texassemble: updated with ``-fl`` switch for feature level based warning
* texconv: updated with ``-reconstructz`` switch for BC5 compressed normal map view conversion

### July 2, 2020
* Minor warning fixes for VS 2019 (16.7)
* CMake updates
* texassemble: Fixed animated gif handling with transparency

### June 15, 2020
* Code cleanup for some new VC++ 16.7 warnings and static code analysis
* texconv: Updated with support for Portable Pix Map (ppm) & Portable Float Map (pfm) file formats

### June 1, 2020
* Converted to typed enum bitmask flags (see release notes for details on this potential *breaking change*)
  + **ComputePitch**, **xxxDDSxxx**, **xxxWICxxx**, **FlipRotate**, **Resize**, **Convert**, **GenerateMipMaps**, **GenerateMipMaps3D**, **PremultiplyAlpha**, **Compress**, **ComputeNormalMap**, **CopyRectangle**, **ComputeMSE**
* ``WIC_FLAGS_DEFAULT_SRGB`` / ``WIC_LOADER_SRGB_DEFAULT`` flag added when loading image via WIC without explicit colorspace metadata
*  WIC loader for  ``PNG`` codec now checks ``gAMA`` chunk to determine colorspace if the ``sRGB`` chunk is not found for legacy sRGB detection.
* Fixed conformance issues when using ``/Zc:preprocessor``
* CMake project updates

### May 10, 2020
* HDR (RGBE Radiance) file format writer updated to accept half16 input
* Code cleanup
* Updated D3DX12 internal copy to Windows 10 SDK (19041) version
* texassemble, texconv, texdiag: Updated with ``-l`` switch for case-sensitive file systems
* texconv: Added ``-dx9`` switch to force legacy compatible DDS files
* texconv: Collapsed ``-bcuniform``, ``-bcdither``, ``-bcquick``, and ``-bcmax`` into one ``-bc`` switch
* Updates to **DDSTextureLoader**, **ScreenGrab**, and **WICTextureLoader** including new DX9 version

### April 3, 2020
* Updated D3DX12 internal copy to latest version
* DDS loader updated for another BC7 FourCC variant
* Code review (``constexpr`` / ``noexcept`` usage)
* CMake updated for PCH usage with 3.16 or later

### February 14, 2020
* Fixed quality bug in BC4S/BC5S compressor
* Guard for divide-by-zero case in **PremultiplyAlpha**
* texconv: added ``-at`` switch for alpha threshold value with BC1 compression
* texconv: Fixed ``-nmap`` when outputting compressed UNORM formats
* Code and project cleaup
* Retired VS 2015 projects

### December 17, 2019
* Added ARM64 platform to VS 2019 Win32 desktop Win10 project
* Updated CMake project
* Code cleaup

### October 17, 2019
* Codec readers updated to return ``TEX_ALPHA_MODE_OPAQUE`` if reader returned an alpha channel due to conversion
* Added DDS reader support for 'non-standard' BC6H/BC7 FourCC codes used by nVidia texture tools
* TGA codec updated for TGA 2.0
* Minor code review
* Updated ScreenGrab module
* texconv: Added ``-fixbc4x4switch``

### August 21, 2019
* Updated D3DX12 internal copy to latest version
* Added texassemble, texconv, and texdiag to CMake project
* Code cleanup

### June 30, 2019
* Additional validation for Direct3D 11 texture loaders
* Clang/LLVM warning cleanup
* Renamed ``DirectXTex_Windows10.vcxproj`` to ``_Windows10_2017.vcxproj``
* Added VS 2019 UWP project

### May 30, 2019
* Regenerated shaders using Windows 10 April 2019 Update SDK (18362)
* Added CMake project files
* Code cleanup

### April 26, 2019
* Added VS 2019 desktop projects
* Code cleanup for texture loaders
* Officially dropped Windows Vista support
* Minor code cleanup

### February 7, 2019
* Added **ScaleMipMapsAlphaForCoverage** function to the library
* WIC Writer now has two new flags: ``WIC_FLAGS_FORCE_SRGB`` and ``WIC_FLAGS_FORCE_LINEAR``
* texassemble: added ``array-strip`` command
* texconv: added ``-inverty``, ``-keepcoverage`` switches

### November 16, 2018
* VS 2017 updated for Windows 10 October 2018 Update SDK (17763)
* ARM64 platform configurations added to UWP projects

### October 25, 2018
* Use UTF-8 instead of ANSI for narrow strings
* Updated D3DX12 internal copy to latest version
* Minor code cleanup

### August 17, 2018
* Fixed problem loading legacy DDS files containing FOURCC pixel formats with ``ALPHAPIXELS`` also set
* Fixed ``FlipRotate`` bug when doing 180 degree rotation
* Updated for VS 2017 15.8

### August 5, 2018
* Improved support and validation for 16k textures (requires x64 native)
* ``ComputePitch`` now returns an HRESULT
* Fix BC7 GPU shaders on WARP device

### July 3, 2018
* BC7 CPU codec fix for 3subsets/``-bcmax`` and minor optimization
* BC7 GPU codec quantize fix and pbit optimization
* BC6H CPU codec bounds checking fix
* Code and project cleanup

### May 31, 2018
* Fix for **IsAlphaAllOpaque** for 'near opaque' values
* VS 2017 updated for Windows 10 April 2018 Update SDK (17134)

### May 11, 2018
* Workaround for WIC issue doing FP32->FP16 conversions
* Updated for VS 2017 15.7 update warnings
* Code and project cleanup
* Retired VS 2013 projects

### April 23, 2018
* Code cleanup
* texconv: Updated with support reading "Extended BMP" files using DXTn
* texconv: Updated to handle non-power-of-2 volume textures with mipmaps
* texassemble, texconv, texdiag: support format name aliases like DXT3, RGBA, BGRA, FP16, etc. in addition to truncated ``DXGI_FORMAT_`` values

### February 9, 2018
* HDR (RGBE Radiance) file format reader updated to support ``#?RGBE`` signature
* texconv: Added ``-rotatecolor`` and ``-nits`` switches
* texassemble: Added merge and gif commands
* texdiag: added dumpdds command

### February 7, 2018
* Fixed bug with GPU BC7 encoding (mode 1, fixup 6)
* Updated for a few more VS 2017 warnings
* Code cleanup

### December 13, 2017
* Updated for VS 2017 15.5 update warnings
* Support building library with ``_XM_NO_XMVECTOR_OVERLOADS_``
* Code cleanup

### November 1, 2017
* VS 2017 updated for Windows 10 Fall Creators Update SDK (16299)

### September 22, 2017
* Updated for VS 2017 15.3 update ``/permissive-`` changes
* WIC writer and ScreenGrab updated to use non-sRGB metadata for PNG
* texassemble, texconv, texdiag: added ``-flist`` option

### July 26, 2017
* Support for reading non-standard DDS files written by nVidia Texture Tools (NVTT)
* Fix for **ComputeMSE** when using ``CMSE_IMAGE2_X2_BIAS``
* Fix for WIC writer then codec target format requires a palette    
* Code cleanup

### April 24, 2017
* VS 2017 project updates
* Regenerated shaders using Windows 10 Creators Update SDK (15063)
* Updated D3DX12 internal copy to latest version

### April 7, 2017
* VS 2017 updated for Windows Creators Update SDK (15063)
* texassemble: ``-tonemap`` switch
* texconv: ``-wicmulti`` switch

### January 31, 2017
* DirectX 12 versions of **IsSupported**, **CreateTexture** (PrepareUpload), and **CaptureTexture**
* Update to DirectX 11 version of **IsSupported**
* WIC format 40bppCMYKAlpha should be converted to RGBA8 rather than RGBA16
* DDS support for L8A8 with bit-count 8 rather than 16
* ``DXGI_FORMAT_R32G8X24_TYPELESS`` and ``DXGI_FORMAT_R24G8_TYPELESS`` should be IsDepthStencil formats
* Updates to DDSTextureLoader, ScreenGrab, and WICTextureLoader
* Minor code cleanup

### December 5, 2016
* Fixed over-validation in DDS header parsing
* VS 2017 RC projects added
* Minor code cleanup

### October 5, 2016
* *breaking change* Renamed Evaluate to **EvaluateImage**, Transform to **TransformImage**
* texdiag: new command-line tool for texture debugging
* texconv: ``-bcmax``, ``-bcquick``, ``-tonemap``, and ``-x2bias`` switches
* texconv: overwrite writing and ``-y`` switch
* texconv/texassemble: optional OpenEXR support
* texassemble: command syntax with support for generating strip and cross images from cubemap
* Updates to DDSTextureLoader, WICTextureLoader, and ScreenGrab
* Minor code cleanup

### September 14, 2016
* [HDR (RGBE Radiance)](https://en.wikipedia.org/wiki/RGBE_image_format) file format reader and writer
* **Evaluate** and **Transform** functions for computing user-defined functions on images
* Fix BC6H GPU shaders on WARP device
* Fix for alignment issues on ARM devices in software compression codec
* Added ``TEX_THRESHOLD_DEFAULT`` (0.5f) constant default alpha threshold value for Convert & Compress
* Minor **CaptureTexture** optimization
* texconv/texassemble: Support for .hdr file format
* texconv: added ``-gpu`` switch to specify adapter to use for GPU-based compression codecs
* texconv: added ``-badtails`` switch to enable loading of legacy DXTn DDS files with incomplete mipchain tails
* texconv: added ``-c`` switch for old-school colorkey/chromakey transparency to alpha conversion
* texconv: added ``-alpha`` switch for reverse premultiply along with ``TEX_PMALPHA_REVERSE`` flag
* texconv: added wildcard support for input filename and optional ``-r`` switch for recursive search

### August 4, 2016
* ``CompileShader`` script updated to build external pdbs
* Regenerated shaders using Windows 10 Anniversary Update SDK (14393)

### August 2, 2016
* Updated for VS 2015 Update 3 and Windows 10 SDK (14393)

### August 1, 2016
* Workaround for bug in XMStoreFloat3SE (impacts conversions to ``DXGI_FORMAT_R9G9B9E5_SHAREDEXP``)
* **DDSTextureLoader12**, **WICTextureLoader12**, and **ScreenGrab12** for Direct3D 12 support
* Minor code cleanup

### June 27, 2016
* texconv command-line tool ``-wicq`` and ``-wiclossless`` switches
* Code cleanup

### April 26, 2016
* Optional callback from WIC reader functions to query additional metadata
* Retired obsolete adapter code
* Minor code cleanup

### February 23, 2016
* Fix to clean up partial or zero-length image files on failed write
* Retired VS 2012 projects

### November 30, 2015
* texconv command-line tool ``-fl`` switch now supports 12.0 and 12.1 feature levels
* Updated for VS 2015 Update 1 and Windows 10 SDK (10586)

### October 30, 2015
* DDS support for legacy bumpmap formats (V8U8, Q8W8V8U8, V16U16)
* Fix for buffer overread in BC CPU compressor
* Minor code cleanup

### August 18, 2015
* Added **GetWICFactory** and **SetWICFactory**
* Updates for new DXGI 1.3 types
* Xbox One platform updates

### July 29, 2015
* Fixed rounding problem with 32-bit RGBA/BGRA format conversions
* texconv: use CPU parallel compression for BC1-BC5 (``-singleproc`` disables)
* Updated for VS 2015 and Windows 10 SDK RTM
* Retired VS 2010 and Windows 8.0 Store projects

### June 18, 2015
* New ``BC_FLAGS_USE_3SUBSETS`` option for BC7 compressors; now defaults to skipping 3 subset blocks
* Fixed bug with **MakeTypeless** and ``A8_UNORM``
* Fixed file length validation problem in **LoadDDSFromFile**

### March 27, 2015
* Added projects for Windows apps Technical Preview
* Fixed bug with WIC-based mipmap generation for non-WIC supported formats
* Fixed bug with WIC multiframe loader when resizing required
* texconv: Added ``-nmap``/``-nmapamp`` for generating normal maps from height maps
* texconv/texassemble: Updated to load multiframe WIC files (tiff, gif)
* Minor code cleanup

### November 24, 2014
* Updates for Visual Studio 2015 Technical Preview
* Minor code cleanup

### September 22, 2014
* Format conversion improvements and bug fixes (depth/stencil, alpha-only, float16, RGB -> 1 channel)
* Fixed issue when BC decompressing non-standard compressed rowPitch images
* Explicit calling-convention annotation for all 'public' functions
* Code cleanup
* Xbox One platform updates

### July 15, 2014
* texconv command-line tool fixes
* Fixed problem with 'wide' images with CPU **Compress**
* Updates to Xbox One platform support

### April 3, 2014
* Windows phone 8.1 platform support

### February 24, 2014
* Direct3D 11 video and Xbox One extended format support
* New APIs: **IsPlanar**, **IsPalettized**, **IsDepthStencil**, **ConvertToSinglePlane**
* Added 'alphaWeight' parameter to GPU **Compress** *breaking change*
* texconv ``-aw`` switch to control the alpha weighting for the BC7 GPU compressor
* Fixed bug with ordered dithering in non-WIC conversion codepaths
* Fixed **SaveToDDSxxx** functions when using arbitrary row pitch values

### January 24, 2014
* Added sRGB flags for **Compress** (``TEX_COMPRESS_SRGB*``)
* Added 'compress' flag parameter to GPU versions of **Compress** *breaking change*
* Minor fix for potential rounding problem in GPU **Compress**
* Code cleanup (removed ``DXGI_1_2_FORMATS`` control define; ``ScopedObject`` typedef removed)
* Dropped VS 2010 support without the Windows 8.1 SDK (removed ``USE_XNAMATH`` control define)

### December 24, 2013
* texconv updated with ``-fl`` and ``-pow2`` command-line switches
* Fixed bug in **Resize** when doing custom filtering which occurred when exactly doubling the image size
* Added move operators to **ScratchImage** and **Blob** classes
* Xbox One platform support

### October 21, 2013
* Updated for Visual Studio 2013 and Windows 8.1 SDK RTM
* **PremultiplyAlpha** updated with new 'flags' parameter and to use sRGB correct blending
* Fixed colorspace conversion issue with DirectCompute compressor when compressing for BC7 SRGB

### August 13, 2013
* DirectCompute 4.0 BC6H/BC7 compressor integration
* texconv utility uses DirectCompute compression by default for BC6H/BC7, ``-nogpu`` disables use of DirectCompute

### August 1, 2013
* Support for BC compression/decompression of non-power-of-2 mipmapped textures
* Fixes for BC6H / BC7 codecs to better match published standard
* Fix for BC4 / BC5 codecs when compressing RGB images
* Minor fix for the BC1-3 codec
* New optional flags for **ComputeMSE** to compare UNORM vs. SNORM images
* New WIC loading flag added to control use of WIC metadata to return sRGB vs. non-sRGB formats
* Code cleanup and /analyze fixes
* Project file cleanup
* texconv utility uses parallel BC compression by default for BC6H/BC7, ``-singleproc`` disables multithreaded behavior

### July 1, 2013
* VS 2013 Preview projects added
* SaveToWIC functions updated with new optional ``setCustomProps`` parameter

### June 15, 2013
* Custom filtering implementation for **Resize** & **GenerateMipMaps(3D)** - Point, Box, Linear, Cubic, and Triangle
  + ``TEX_FILTER_TRIANGLE`` finite low-pass triangle filter
  + ``TEX_FILTER_WRAP``, ``TEX_FILTER_MIRROR`` texture semantics for custom filtering
  + ``TEX_FILTER_BOX`` alias for ``TEX_FILTER_FANT WIC``
* Ordered and error diffusion dithering for non-WIC conversion
* sRGB gamma correct custom filtering and conversion
* ``DDS_FLAGS_EXPAND_LUMINANCE`` - Reader conversion option for L8, L16, and A8L8 legacy DDS files
* Added use of WIC metadata for sRGB pixel formats
* Added **BitsPerColor** utility function
* Fixed **Convert** threshold parameter usage
* Non-power-of-2 volume map support, fixed bug with non-square volume maps
* texconv utility update with ``-xlum``, ``-wrap``, and ``-mirror`` options; reworked ``-if`` options for improved dithering
* texassemble utility for creating cubemaps, volume maps, and texture arrays
* DDSTextureLoader and WICTextureLoader sync'd with DirectXTK versions

### April 16, 2013
* Updated alpha-mode metadata details in .DDS files
* Added new control flags for **Convert**
* Added new optional flags for **ComputeMSE**
* Fixed conversion handling for sRGB formats
* Fixed internal routines for handling ``R10G10B10_XR_BIAS_A2_UNORM``, ``R9G9B9E5_SHAREDEXP``, and ``FORMAT_R1_UNORM``
* Fixed WIC I/O for ``GUID_WICPixelFormat32bppRGBE``4 pixel format files (HD Photo)
* Fixed non-square image handling in **GenerateMipMaps3D**
* Fixed some error handling in the DDS load code

### March 22, 2013
* Supports reading and writing alpha-mode (straight, premultiplied, etc.) metadata in .DDS files
* Added build option to use WICCreateImagingFactory_Proxy instead of ``CoCreateInstance`` to obtain WIC factory

### January 29, 2013
* Added **PremultiplyAlpha** to DirectXTex; ``-pmalpha`` switch for texconv command-line tool
* Fixed problem with forceSRGB implementation for Ex versions of CreateTexture, CreateShaderResourceView, DDSTextureLoader and WICTextureLoader

### December 11, 2012
* Ex versions of **CreateTexture**, **CreateShaderResourceView**, **DDSTextureLoader** and **WICTextureLoader**
* Fixed BC2 and BC3 decompression issue for unusual color encoding case
* Converted annotation to SAL2 for improved VS 2012 /analyze experience
* Updated DirectXTex, DDSView, and Texconv with VS 2010 + Windows 8.0 SDK project using official 'property sheets'

### November 15, 2012
* Added support for WIC2 when available on Windows 8 and Windows 7 with KB 2670838
* Added optional ``targetGUID`` parameter to SaveWIC* APIs to influence final container pixel format choice
* Fixed bug in **SaveDDSxxx** which was generating invalid DDS files for 1D dimension textures
* Improved robustness of **CaptureTexture** when resolving MSAA source textures
* Sync'd DDSTextureLoader, ScreenGrab, and WICTextureLoader standalone versions with latest DirectXTK release

### September 28, 2012
* Added **ScreenGrab** module for creating runtime screenshots
* Renamed project files for better naming consistency
* New Typeless utilities for DirectXTex
* Some minor code cleanup for DirectXTex's WIC writer function
* Bug fixes and new ``-tu``/``-tf`` options for texconv

### June 22, 2012
* Moved to using XNA Math 2.05 instead of XNA Math 2.04 for ``USE_XNAMATH`` builds
* Fixed BGR vs. RGB color channel swizzle problem with 24bpp legacy .DDS files in DirectXTex
* Update to DirectXTex WIC and WICTextureLoader for additional 96bpp float format handling on Windows 8

### May 31, 2012
* Minor fix for DDSTextureLoader's retry fallback that can happen with 10level9 feature levels
* Switched to use ``_DEBUG`` instead of ``DEBUG`` and cleaned up debug warnings
* added Windows Store style application project files for DirectXTex

### April 20, 2012
* DirectTex's WIC-based writer opts-in for the Windows 8 BMP encoder option for writing 32 bpp RGBA files with the ``BITMAPV5HEADER``

### March 30, 2012
* WICTextureLoader updated with Windows 8 WIC pixel formats
* DirectXTex updated with limited non-power-of-2 texture support and ``TEX_FILTER_SEPARATE_ALPHA`` option
* Texconv updated with ``-sepalpha`` command-line option
* Added ``USE_XNAMATH`` control define to build DirectXTex using either XNAMath or DirectXMath
* Added VS 2012 project files (which use DirectXMath instead of XNAMath and define ``DXGI_1_2_FORMATS``)

### March 15, 2012
* Fix for resource leak in **CreateShaderResourceView** Direct3D 11 helper function in DirectXTex

### March 5, 2012
* Fix for too much temp memory allocated by WICTextureLoader; cleaned up legacy 'min/max' macro usage in DirectXTex

### February 21, 2012
* WICTextureLoader updated to handle systems and device drivers without BGRA or 16bpp format support

### February 20, 2012
* Some code cleanup for DirectXTex and DDSTextureLoader
* Fixed bug in 10:10:10:2 format fixup in the **LoadDDSFromMemory** function
* Fixed bugs in "non-zero alpha" special-case handling in **LoadTGAFromFile**
* Fixed bug in ``_SwizzleScanline`` when copying alpha channel for BGRA<->RGBA swizzling

### February 11, 2012
* Update of DDSTextureLoader to also build in Windows Store style apps; added **WICTextureLoader**
* Added CMYK WIC pixel formats to the DirectXTex conversion table

### January 30, 2012
* Minor code-cleanup for DirectXTex to enable use of PCH through 'directxtexp.h' header

### January 24, 2012
* Some code-cleanup for DirectXTex
* Added DXGI 1.2 implementation for DDSTextureLoader and DirectXTex guarded with ``DXGI_1_2_FORMATS`` compilation define

### December 16, 2011
* Fixed x64 compilation warnings in DDSTextureLoader

### November 30, 2011
* Fixed some of the constants used in **IsSupportedTexture**
* added ability to strip off top levels of mips in DDSTextureLoader
* changed DirectXTex to use CoCreateInstance rather than LoadLibrary to obtain the WIC factory
* a few minor ``/analyze`` related annotations for DirectXTex

### October 27, 2011
* Original release
