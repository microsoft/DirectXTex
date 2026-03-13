# GitHub Copilot Instructions

These instructions define how GitHub Copilot should assist with this project. The goal is to ensure consistent, high-quality code generation aligned with our conventions, stack, and best practices.

## Context

- **Project Type**: Graphics Library / DirectX / Direct3D 11 / Direct3D 12 / Image Processing
- **Project Name**: DirectXTex Texture Processing Library
- **Language**: C++
- **Framework / Libraries**: STL / CMake / CTest
- **Architecture**: Modular / RAII / OOP

## Getting Started

- See the tutorial at [Getting Started](https://github.com/microsoft/DirectXTex/wiki/Getting-Started).
- The recommended way to integrate *DirectXTex* into your project is by using the *vcpkg* Package Manager.
- You can make use of the nuget.org packages **directxtex_desktop_2019**, **directxtex_desktop_win10**, or **directxtex_uwp**.
- You can also use the library source code directly in your project or as a git submodule.

## General Guidelines

- **Code Style**: The project uses an .editorconfig file to enforce coding standards. Follow the rules defined in `.editorconfig` for indentation, line endings, and other formatting. Additional information can be found on the wiki at [Implementation](https://github.com/microsoft/DirectXTK/wiki/Implementation). The library implementation is written to be compatible with C++14 features, but C++17 is required to build the project for the command-line tools which utilize C++17 filesystem for long file path support.
> Notable `.editorconfig` rules: C/C++ files use 4-space indentation, `crlf` line endings, and `latin1` charset — avoid non-ASCII characters in source files. HLSL files have separate indent/spacing rules defined in `.editorconfig`.
- **Documentation**: The project provides documentation in the form of wiki pages available at [Documentation](https://github.com/microsoft/DirectXTex/wiki/).
- **Error Handling**: Use C++ exceptions for error handling and uses RAII smart pointers to ensure resources are properly managed. For some functions that return HRESULT error codes, they are marked `noexcept`, use `std::nothrow` for memory allocation, and should not throw exceptions.
- **Testing**: Unit tests for this project are implemented in this repository [Test Suite](https://github.com/walbourn/directxtextest/) and can be run using CTest per the instructions at [Test Documentation](https://github.com/walbourn/directxtextest/wiki).
- **Security**: This project uses secure coding practices from the Microsoft Secure Coding Guidelines, and is subject to the `SECURITY.md` file in the root of the repository. Functions that read input from geometry files are subject to OneFuzz fuzz testing to ensure they are secure against malformed files.
- **Dependencies**: The project uses CMake and VCPKG for managing dependencies, making optional use of DirectXMath and DirectX-Headers. The project can be built without these dependencies, relying on the Windows SDK for core functionality.
- **Continuous Integration**: This project implements GitHub Actions for continuous integration, ensuring that all code changes are tested and validated before merging. This includes building the project for a number of configurations and toolsets, running a subset of unit tests, and static code analysis including GitHub super-linter, CodeQL, and MSVC Code Analysis.
- **Code of Conduct**: The project adheres to the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). All contributors are expected to follow this code of conduct in all interactions related to the project.

## File Structure

```txt
.azuredevops/     # Azure DevOps pipeline configuration and policy files.
.github/          # GitHub Actions workflow files and linter configuration files.
.nuget/           # NuGet package configuration files.
build/            # Miscellaneous build files and scripts.
Auxiliary/        # Auxiliary functions such as Xbox tiling extensions, OpenEXR support, etc.
DirectXTex/       # DirectXTex implementation files.
  Shaders/        # HLSL shader files.
DDSView/          # Sample application for viewing DDS texture files using DirectXTex.
texassemble/      # CLI tool for creating complex DDS files from multiple image files.
texconv/          # CLI tool for converting image files to DDS texture files including block compression, mipmaps, and resizing.
texdiag/          # CLI tool for diagnosing and validating DDS texture files.
DDSTextureLoader/ # Standalone version of the DDS texture loader for Direct3D 9/11/12.
ScreenGrab/       # Standalone version of the screenshot capture utility for Direct3D 9/11/12.
WICTextureLoader/ # Standalone versoin of the WIC texture loader for Direct3D 9/11/12.
Tests/            # Tests are designed to be cloned from a separate repository at this location.
```

> Note that DDSTextureLoader, ScreenGrab, and WICTextureLoader are standalone version of utilities which are also included in the *DirectX Tool Kit for DirectX 11* and *DirectX Tool Kit for DirectX 12*.

## Patterns

### Patterns to Follow

- Use RAII for all resource ownership (memory, file handles, etc.).
- Many classes utilize the pImpl idiom to hide implementation details, and to enable optimized memory alignment in the implementation.
- Use `std::unique_ptr` for exclusive ownership and `std::shared_ptr` for shared ownership.
- Use `Microsoft::WRL::ComPtr` for COM object management.
- Make use of anonymous namespaces to limit scope of functions and variables.
- Make use of `assert` for debugging checks, but be sure to validate input parameters in release builds.
- Explicitly `= delete` copy constructors and copy-assignment operators on all classes that use the pImpl idiom.
- Explicitly utilize `= default` or `=delete` for copy constructors, assignment operators, move constructors and move-assignment operators where appropriate.
- Use 16-byte alignment (`_aligned_malloc` / `_aligned_free`) to support SIMD operations in the implementation, but do not expose this requirement in public APIs.
> For non-Windows support, the implementation uses C++17 `aligned_alloc` instead of `_aligned_malloc`.

#### SAL Annotations

All public API functions must use SAL annotations on every parameter. Use `_Use_decl_annotations_` at the top of each implementation that has SAL in the header declaration — never repeat the annotations in the `.cpp` or `.inl` file.

Common annotations:

| Annotation | Meaning |
| --- | --- |
| `_In_` | Input parameter |
| `_Out_` | Output parameter |
| `_Inout_` | Bidirectional parameter |
| `_In_reads_bytes_(n)` | Input buffer with byte count |
| `_In_reads_(n)` | Input array with element count |
| `_In_z_` | Null-terminated input string |
| `_Out_opt_` | Optional output parameter |

Example:

```cpp
// Header (DirectXTex.h)
DIRECTX_TEX_API HRESULT __cdecl GetMetadataFromDDSMemory(
    _In_reads_bytes_(size) const uint8_t* pSource, _In_ size_t size,
    _In_ DDS_FLAGS flags,
    _Out_ TexMetadata& metadata) noexcept;

// Implementation (.cpp)
_Use_decl_annotations_
HRESULT __cdecl GetMetadataFromDDSMemory(
    const uint8_t* pSource, size_t size,
    DDS_FLAGS flags,
    TexMetadata& metadata) noexcept
{ ... }
```

#### Calling Convention and DLL Export

- All public functions use `__cdecl` explicitly for ABI stability.
- All public function declarations are prefixed with `DIRECTX_TEX_API`, which wraps `__declspec(dllexport)` / `__declspec(dllimport)` or the GCC `__attribute__` equivalent when using `BUILD_SHARED_LIBS` in CMake.

#### `noexcept` Rules

- All query and utility functions that cannot fail (e.g., `IsCompressed`, `IsCubemap`, `ComputePitch`) are marked `noexcept`.
- All HRESULT-returning I/O and processing functions are also `noexcept` — errors are communicated via return code, never via exceptions.
- Constructors and functions that perform heap allocation or utilize Standard C++ containers that may throw are marked `noexcept(false)`.

#### Enum Flags Pattern

Flags enums follow this pattern — a `uint32_t`-based unscoped enum with a `_NONE = 0x0` base case, followed by a call to `DEFINE_ENUM_FLAG_OPERATORS` (defined in `DirectXTex.inl`) to enable `|`, `&`, and `~` operators:

```cpp
enum TEX_FILTER_FLAGS : uint32_t
{
    TEX_FILTER_DEFAULT = 0,

    TEX_FILTER_WRAP_U = 0x1,
    // Enables wrapping addressing on U coordinate
    ...
};

DEFINE_ENUM_FLAG_OPERATORS(TEX_FILTER_FLAGS);
```

See [this blog post](https://walbourn.github.io/modern-c++-bitmask-types/) for more information on this pattern.

### Patterns to Avoid

- Don’t use raw pointers for ownership.
- Avoid macros for constants—prefer `constexpr` or `inline` `const`.
- Don’t put implementation logic in header files unless using templates, although the SimpleMath library does use an .inl file for performance.
- Avoid using `using namespace` in header files to prevent polluting the global namespace.

## Naming Conventions

| Element | Convention | Example |
| --- | --- | --- |
| Classes / structs | PascalCase | `ScratchImage`, `TexMetadata` |
| Public functions | PascalCase + `__cdecl` | `ComputePitch`, `IsCompressed` |
| Private data members | `m_` prefix | `m_nimages`, `m_metadata` |
| Enum type names | UPPER_SNAKE_CASE | `TEX_DIMENSION`, `CP_FLAGS` |
| Enum values | UPPER_SNAKE_CASE | `CP_FLAGS_NONE`, `TEX_ALPHA_MODE_PREMULTIPLIED` |
| Flag enum suffix | `_FLAGS` with `_NONE = 0x0` base | `DDS_FLAGS`, `WIC_FLAGS` |
| Files | PascalCase | `DirectXTex.h`, `BC6HEncode.hlsl` |

## File Header Convention

Every source file (`.cpp`, `.h`, `.hlsl`, etc.) must begin with this block:

```cpp
//-------------------------------------------------------------------------------------
// {FileName}
//
// {One-line description}
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------
```

Section separators within files use:
- Major sections: `//-------------------------------------------------------------------------------------`
- Subsections:   `//---------------------------------------------------------------------------------`

The project does **not** use Doxygen. API documentation is maintained exclusively on the GitHub wiki.

## HLSL Shader Compilation

Shaders in `DirectXTex/Shaders/` are compiled with **FXC** (not DXC), producing embedded C++ header files (`.inc`) that are checked in alongside the source:

- Each shader is compiled twice: `cs_5_0` (primary) and `cs_4_0` with `/DEMULATE_F16C` (legacy fallback).
- Standard compiler flags: `/nologo /WX /Ges /Zi /Zpc /Qstrip_reflect /Qstrip_debug`
- Use `CompileShaders.cmd` in `DirectXTex/Shaders/` to regenerate the `.inc` files.
- The CMake option `USE_PREBUILT_SHADERS` controls whether pre-compiled shaders are used.

## References

- [Source git repository on GitHub](https://github.com/microsoft/DirectXTex.git)
- [DirectXTex documentation git repository on GitHub](https://github.com/microsoft/DirectXTex.wiki.git)
- [DirectXTex test suite git repository on GitHub](https://github.com/walbourn/directxtextest.wiki.git).
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [Microsoft Secure Coding Guidelines](https://learn.microsoft.com/en-us/security/develop/secure-coding-guidelines)
- [CMake Documentation](https://cmake.org/documentation/)
- [VCPKG Documentation](https://learn.microsoft.com/vcpkg/)
- [Games for Windows and the DirectX SDK blog - October 2021](https://walbourn.github.io/directxtex/)
- [Games for Windows and the DirectX SDK blog - April 2025](https://walbourn.github.io/github-project-updates-2025/)

## No speculation

When creating documentation:

### Document Only What Exists

- Only document features, patterns, and decisions that are explicitly present in the source code.
- Only include configurations and requirements that are clearly specified.
- Do not make assumptions about implementation details.

### Handle Missing Information

- Ask the user questions to gather missing information.
- Document gaps in current implementation or specifications.
- List open questions that need to be addressed.

### Source Material

- Always cite the specific source file and line numbers for documented features.
- Link directly to relevant source code when possible.
- Indicate when information comes from requirements vs. implementation.

### Verification Process

- Review each documented item against source code whenever related to the task.
- Remove any speculative content.
- Ensure all documentation is verifiable against the current state of the codebase.

## Cross-platform Support Notes

- The code supports building for Windows and Linux.
- Portability and conformance of the code is validated by building with Visual C++, clang/LLVM for Windows, MinGW, and GCC for Linux.

### Platform and Compiler `#ifdef` Guards

Use these established guards — do not invent new ones:

| Guard | Purpose |
| --- | --- |
| `_WIN32` | Windows platform (desktop, UWP, Xbox) |
| `_GAMING_XBOX_SCARLETT` | Xbox Series X\|S (check before `_GAMING_XBOX`) |
| `_GAMING_XBOX` | Xbox One / Scarlett GDK fallback |
| `_XBOX_ONE && _TITLE` | Xbox One XDK (legacy) |
| `_MSC_VER` | MSVC-specific pragmas and warning suppression |
| `__clang__` | Clang/LLVM diagnostic suppressions |
| `__MINGW32__` | MinGW compatibility headers |
| `__GNUC__` | GCC DLL attribute equivalents |
| `_M_ARM64` / `_M_IX86` | Architecture-specific code paths |
| `USING_DIRECTX_HEADERS` | External DirectX-Headers package in use |

Non-Windows builds (Linux/WSL) omit WIC entirely and use `<directx/dxgiformat.h>` and `<wsl/winadapter.h>` from the DirectX-Headers package instead of the Windows SDK.

The following symbols are not custom error codes, but aliases for `HRESULT_FROM_WIN32` error codes.

| Symbol | Standard Win32 HRESULT |
| -------- | ------------- |
| `HRESULT_ERROR_FILE_NOT_FOUND` | `HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)` |
| `HRESULT_E_ARITHMETIC_OVERFLOW` | `HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW)` |
| `HRESULT_E_NOT_SUPPORTED` | `HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)` |
| `HRESULT_E_HANDLE_EOF` | `HRESULT_FROM_WIN32(ERROR_HANDLE_EOF)` |
| `HRESULT_E_INVALID_DATA` | `HRESULT_FROM_WIN32(ERROR_INVALID_DATA)` |
| `HRESULT_E_FILE_TOO_LARGE` | `HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE)` |
| `HRESULT_E_CANNOT_MAKE` | `HRESULT_FROM_WIN32(ERROR_CANNOT_MAKE)` |

## Code Review Instructions

When reviewing code, focus on the following aspects:

- Adherence to coding standards defined in `.editorconfig` and on the [wiki](https://github.com/microsoft/DirectXTK/wiki/Implementation).
- Make coding recommendations based on the *C++ Core Guidelines*.
- Proper use of RAII and smart pointers.
- Correct error handling practices and C++ Exception safety.
- Clarity and maintainability of the code.
- Adequate comments where necessary.
- Public interfaces located in `DirectXTex.h` should be clearly defined and documented on the GitHub wiki.
- Optional functions are located in `DirectXTexEXR.h`, `DirectXTexJPEG.h`, `DirectXTexPNG.h`, and `DirectXTexXbox.h` in the `Auxiliary` folder.
- Standalone modules for loading textures from DDS Files are located in `DDSTextureLoader9.h`, `DDSTextureLoader11.h`, and `DDSTextureLoader12.h` in the `DDSTextureLoader` folder.
- Standalone modules are loading textures using WIC are located in `WICTextureLoader9.h`, `WICTextureLoader11.h`, and `WICTextureLoader12.h` in the `WICTextureLoader` folder.
- Standalone modules for capturing screenshots are located in `ScreenGrab9.h`, `ScreenGrab11.h`, and `ScreenGrab12.h` in the `ScreenGrab` folder.
- Compliance with the project's architecture and design patterns.
- Ensure that all public functions and classes are covered by unit tests located on [GitHub](https://github.com/walbourn/directxtextest.git) where applicable. Report any gaps in test coverage.
- Check for performance implications, especially in geometry processing algorithms.
- Provide brutally honest feedback on code quality, design, and potential improvements as needed.

## Documentation Review Instructions

When reviewing documentation, do the following:

- Read the code located in [this git repository](https://github.com/microsoft/DirectXTex.git) in the main branch.
- Review the public interface defined in `DirectXTex.h`.
- Read the documentation on the wiki located in [this git repository](https://github.com/microsoft/DirectXTex.wiki.git).
- Report any specific gaps in the documentation compared to the public interface
