![DirectX Logo](https://github.com/Microsoft/DirectXTex/wiki/X_jpg.jpg)

# DirectXTex texture processing library

http://go.microsoft.com/fwlink/?LinkId=248926

Copyright (c) Microsoft Corporation. All rights reserved.

**May 10, 2020**

This package contains DirectXTex, a shared source library for reading and writing DDS files, and performing various texture content processing operations including resizing, format conversion, mip-map generation, block compression for Direct3D runtime texture resources, and height-map to normal-map conversion. This library makes use of the Windows Image Component (WIC) APIs. It also includes ``.TGA`` and ``.HDR`` readers and writers since these image file formats are commonly used for texture content processing pipelines, but are not currently supported by a built-in WIC codec.

This code is designed to build with Visual Studio 2017 ([15.9](https://walbourn.github.io/vs-2017-15-9-update/)), Visual Studio 2019, or clang for Windows v9. It is recommended that you make use of the Windows 10 May 2019 Update SDK ([18362](https://walbourn.github.io/windows-10-may-2019-update/)).

These components are designed to work without requiring any content from the legacy DirectX SDK. For details, see [Where is the DirectX SDK?](https://aka.ms/dxsdk).

## Directory Layout

* ``DirectXTex\``

  + This contains the DirectXTex library. This includes a full-featured DDS reader and writer including legacy format conversions, a TGA reader and writer, a HDR reader and writer, a WIC-based bitmap reader and writer (BMP, JPEG, PNG, TIFF, and HD Photo), and various texture processing functions. This is intended primarily for tool usage.

> The majority of the header files here are intended for internal implementation
    of the library only (``BC.h``, ``BCDirectCompute.h``, ``DDS.h``, ``DirectXTexP.h``, etc.). Only ``DirectXTex.h`` and ``DirectXTex.inl`` are meant as the 'public' header for the library.

* ``Texconv\``

  + This DirectXTex sample is an implementation of the [texconv](https://github.com/Microsoft/DirectXTex/wiki/Texconv) command-line texture utility from the DirectX SDK utilizing DirectXTex rather than D3DX.

    It supports the same arguments as the *Texture Conversion Tool Extended* (texconvex.exe) legacy DirectX SDK utility. The primary differences are the ``-10`` and ``-11`` arguments are not applicable and the filter names (``POINT``, ``LINEAR``, ``CUBIC``, ``FANT`` or ``BOX``, ``TRIANGLE``, ``*_DITHER``, ``*_DITHER_DIFFUSION``). This also includes support for the JPEG XR (HD Photo) bitmap format.

* ``Texassemble\``

  + This DirectXTex sample is a [command-line utility](https://github.com/Microsoft/DirectXTex/wiki/Texassemble) for creating cubemaps, volume maps, or texture arrays from a set of individual input image files.

* ``Texdiag\``

  + This DirectXTex sample is a [command-line utility](https://github.com/Microsoft/DirectXTex/wiki/Texdiag) for analyzing image contents, primarily for debugging purposes.

* ``DDSView\``

  + This DirectXTex sample is a simple Direct3D 11-based viewer for DDS files. For array textures or volume maps, the "<" and ">" keyboard keys will show different images contained in the DDS. The "1" through "0" keys can also be used to jump to a specific image index.

* ``DDSTextureLoader\``

  + This contains a streamlined version of the legacy DirectX SDK sample *DDSWithoutD3DX11* texture loading code for a simple light-weight runtime DDS loader. There are versions for Direct3D 9, Direct3D 11, and Direct3D 12. This performs no runtime pixel data conversions. This is ideal for runtime usage, and supports the full complement of Direct3D texture  resources (1D, 2D, volume maps, cubemaps, mipmap levels, texture arrays, BC formats, etc.).

* ``ScreenGrab\``

  + This contains texture writing modules for Direct3D 9, Direct3D 11, and Direct3D 12 primarily intended for creating screenshots. The images are written as a DDS or as an image file format using WIC.

* ``WICTextureLoader\``

  + This contains a Direct3D 9, Direct3D 11 and Direct3D 12 2D texture loader that uses WIC to load a bitmap (BMP, JPEG, PNG, HD Photo, or other WIC supported file container), resize if needed based on the current feature level (or by explicit parameter), format convert to a DXGI_FORMAT if required, and then create a 2D texture. Note this does not support 1D textures, volume textures, cubemaps, or texture arrays. DDSTextureLoader is recommended for fully "precooked" textures for maximum performance and image quality, but this loader can be useful for creating simple 2D texture from standard image files at runtime.

> DDSTextureLoader11, ScreenGrab11, and WICTextureLoader11 are 'stand-alone' versions of the same modules provided in the [DirectX Tool Kit for DX11](https://github.com/Microsoft/DirectXTK)

> DDSTextureLoader12, ScreenGrab12, and WICTextureLoader12 are 'stand-alone' versions of the same modules provided in the [DirectX Tool Kit for DX12](https://github.com/Microsoft/DirectXTK12).

# Documentation

Documentation is available on the [GitHub wiki](https://github.com/Microsoft/DirectXTex/wiki).

## Notices

All content and source code for this package are subject to the terms of the [MIT License](http://opensource.org/licenses/MIT).

For the latest version of DirectXTex, bug reports, etc. please visit the project site on [GitHub](https://github.com/microsoft/DirectXTex).

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Release Notes

* Due to the underlying Windows BMP WIC codec, alpha channels are not supported for 16bpp or 32bpp BMP pixel format files. The Windows 8.x and Windows 10 version of the Windows BMP WIC codec does support 32bpp pixel formats with alpha when using the ``BITMAPV5HEADER`` file header. Note the updated WIC is available on Windows 7 SP1 with [KB 2670838](https://walbourn.github.io/directx-11-1-and-windows-7-update/) installed.

* While DXGI 1.0 and DXGI 1.1 include 5:6:5 (``DXGI_FORMAT_B5G6R5_UNORM``) and 5:5:5:1 (``DXGI_FORMAT_B5G5R5A1_UNORM``) pixel format enumerations, the DirectX 10.x and 11.0 Runtimes do not support these formats for use with Direct3D. The DirectX 11.1 runtime, DXGI 1.2, and the WDDM 1.2 driver model fully support 16bpp formats (5:6:5, 5:5:5:1, and 4:4:4:4).

* WICTextureLoader cannot load ``.TGA`` or ``.HDR`` files unless the system has a 3rd party WIC codec installed. You must use the DirectXTex library for TGA/HDR file format support without relying on an add-on WIC codec.

* Loading of 96bpp floating-point TIFF files results in a corrupted image prior to Windows 8. This fix is available on Windows 7 SP1 with KB 2670838 installed.

* The VS 2017/2019 projects make use of ``/permissive-`` for improved C++ standard conformance. Use of a Windows 10 SDK prior to the Fall Creators Update (16299) or an Xbox One XDK prior to June 2017 QFE 4 may result in failures due to problems with the system headers. You can work around these by disabling this switch in the project files which is found in the ``<ConformanceMode>`` elements, or in some cases adding ``/Zc:twoPhase-`` to the ``<AdditionalOptions>`` elements.

* The VS 2017 projects require the 15.5 update or later. For UWP and Win32 classic desktop projects with the 15.5 - 15.7 updates, you need to install the standalone Windows 10 SDK (17763) which is otherwise included in the 15.8.6 or later update. Older VS 2017 updates will fail to load the projects due to use of the <ConformanceMode> element. If using the 15.5 or 15.6 updates, you will see ``warning D9002: ignoring unknown option '/Zc:__cplusplus'`` because this switch isn't supported until 15.7. It is safe to ignore this warning, or you can edit the project files ``<AdditionalOptions>`` elements.

* The VS 2019 projects use a ``<WindowsTargetPlatformVersion>`` of ``10.0`` which indicates to use the latest installed version. This should be Windows 10 SDK (17763) or later.

* The UWP projects and the VS 2019 Win10 classic desktop project include configurations for the ARM64 platform. These require VS 2017 (15.9 update) or VS 2019 to build, with the ARM64 toolset installed.

* The ``CompileShaders.cmd`` script must have Windows-style (CRLF) line-endings. If it is changed to Linux-style (LF) line-endings, it can fail to build all the required shaders.
