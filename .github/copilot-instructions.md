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
- You can make use of the nuget.org packages **directxtex_desktop_win10**, or **directxtex_uwp**.
- You can also use the library source code directly in your project or as a git submodule.

## General Guidelines

- **Code Style**: The project uses an .editorconfig file to enforce coding standards. Follow the rules defined in `.editorconfig` for indentation, line endings, and other formatting. Additional information can be found on the wiki at [Implementation](https://github.com/microsoft/DirectXTK/wiki/Implementation). The library's public API requires C++11, and the project builds with C++17 (`CMAKE_CXX_STANDARD 17`). The command-line tools also use C++17, including `<filesystem>` for long file path support. This code is designed to build with Visual Studio 2022, Visual Studio 2026, clang for Windows v12 or later, or MinGW 12.2.
> Notable `.editorconfig` rules: C/C++ and HLSL files use 4-space indentation, `crlf` line endings, and `latin1` charset — avoid non-ASCII characters in source files. HLSL files have separate indent/spacing rules defined in `.editorconfig`.
- **Documentation**: The project provides documentation in the form of wiki pages available at [Documentation](https://github.com/microsoft/DirectXTex/wiki/).
- **Error Handling**: Use C++ exceptions for error handling and uses RAII smart pointers to ensure resources are properly managed. For some functions that return HRESULT error codes, they are marked `noexcept`, use `std::nothrow` for memory allocation, and should not throw exceptions.
- **Testing**: Unit tests for this project are implemented in this repository [Test Suite](https://github.com/walbourn/directxtextest/) and can be run using CTest per the instructions at [Test Documentation](https://github.com/walbourn/directxtextest/wiki). See [test copilot instructions](https://github.com/walbourn/directxtextest/blob/main/.github/copilot-instructions.md) for additional information on the tests.
- **Security**: This project uses secure coding practices from the Microsoft Secure Coding Guidelines, and is subject to the `SECURITY.md` file in the root of the repository. Functions that read input from image files are subject to OneFuzz fuzz testing to ensure they are secure against malformed files.
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
Common/           # Common utility functions shared between the library and command-line tools.
DirectXTex/       # DirectXTex implementation files.
  Shaders/        # HLSL shader files.
DDSView/          # Sample application for viewing DDS texture files using DirectXTex.
Texassemble/      # CLI tool for creating complex DDS files from multiple image files.
Texconv/          # CLI tool for converting image files to DDS texture files including block compression, mipmaps, and resizing.
Texdiag/          # CLI tool for diagnosing and validating DDS texture files.
DDSTextureLoader/ # Standalone version of the DDS texture loader for Direct3D 9/11/12.
ScreenGrab/       # Standalone version of the screenshot capture utility for Direct3D 9/11/12.
WICTextureLoader/ # Standalone version of the WIC texture loader for Direct3D 9/11/12.
Tests/            # Tests are designed to be cloned from a separate repository at this location.
wiki/             # Local clone of the GitHub wiki documentation repository.
```

> Note that DDSTextureLoader, ScreenGrab, and WICTextureLoader are standalone version of utilities which are also included in the *DirectX Tool Kit for DirectX 11* and *DirectX Tool Kit for DirectX 12*.

## Patterns

### Patterns to Follow

- Use RAII for all resource ownership (memory, file handles, etc.).
- Many classes utilize the pImpl idiom to hide implementation details, and to enable optimized memory alignment in the implementation.
- Use `std::unique_ptr` for exclusive ownership.
- Use `Microsoft::WRL::ComPtr` for COM object management.
- Make use of anonymous namespaces to limit scope of functions and variables.
- Make use of `assert` for debugging checks, but be sure to validate input parameters in release builds.
- Explicitly `= delete` copy constructors and copy-assignment operators on all classes that use the pImpl idiom.
- Explicitly utilize `= default` or `=delete` for copy constructors, assignment operators, move constructors and move-assignment operators where appropriate.
- Use 16-byte alignment (`_aligned_malloc` / `_aligned_free`) to support SIMD operations in the implementation, but do not expose this requirement in public APIs.
> For non-Windows support, the implementation uses C++17 `aligned_alloc` instead of `_aligned_malloc`.
- All implementation `.cpp` files include `DirectXTexP.h` as their first include (precompiled header). MinGW builds skip precompiled headers.

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
- All public function declarations are prefixed with `DIRECTX_TEX_API`, which wraps `__declspec(dllexport)` / `__declspec(dllimport)` (or the MinGW `__attribute__` equivalent) when the `DIRECTX_TEX_EXPORT` or `DIRECTX_TEX_IMPORT` preprocessor symbols are defined. CMake sets these automatically when `BUILD_SHARED_LIBS=ON`.

#### `noexcept` Rules

- All query and utility functions that cannot fail (e.g., `IsCompressed`, `IsCubemap`, `ComputePitch`) are marked `noexcept`.
- All HRESULT-returning I/O and processing functions are also `noexcept` — errors are communicated via return code, never via exceptions.
> Note that HRESULT-returning functions that use `std::function` are not marked `noexcept` because the user-provided function could throw.

#### Enum Flags Pattern

Flags enums follow this pattern — a `uint32_t`-based unscoped enum with a `_NONE = 0` or `_DEFAULT = 0` base case, followed by a call to `DEFINE_ENUM_FLAG_OPERATORS` (invoked in `DirectXTex.inl`) to enable `|`, `&`, and `~` operators:

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
- Don't put implementation logic in header files unless using templates, although the DirectXTex library does use an .inl file for performance for a few specific utility functions that are called in tight loops (e.g., `IsValid`, `IsCompressed`).
- Avoid using `using namespace` in header files to prevent polluting the global namespace.

## Naming Conventions

| Element | Convention | Example |
| --- | --- | --- |
| Classes / structs | PascalCase | `ScratchImage`, `TexMetadata` |
| Public functions | PascalCase + `__cdecl` | `ComputePitch`, `IsCompressed` |
| Private data members | `m_` prefix | `m_nimages`, `m_metadata` |
| Enum type names | UPPER_SNAKE_CASE | `TEX_DIMENSION`, `CP_FLAGS` |
| Enum values | UPPER_SNAKE_CASE | `CP_FLAGS_NONE`, `TEX_ALPHA_MODE_PREMULTIPLIED` |
| Flag enum suffix | `_FLAGS` with `_NONE = 0` or `_DEFAULT = 0` base | `DDS_FLAGS`, `WIC_FLAGS` |
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
// https://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------
```

Section separators within files use:
- Major sections: `//-------------------------------------------------------------------------------------`
- Subsections:   `//---------------------------------------------------------------------------------`

The project does **not** use Doxygen. API documentation is maintained exclusively on the GitHub wiki.

## HLSL Shader Compilation

Shaders in `DirectXTex/Shaders/` are compiled with **FXC** (not DXC), producing embedded C++ header files (`.inc`):

- Each shader is compiled twice: `cs_5_0` (primary) and `cs_4_0` with `/DEMULATE_F16C` (legacy fallback).
- Standard compiler flags: `/nologo /WX /Ges /Zi /Zpc /Qstrip_reflect /Qstrip_debug`
- Use `CompileShaders.cmd` in `DirectXTex/Shaders/` to regenerate the `.inc` files.
- The CMake option `USE_PREBUILT_SHADERS` controls whether pre-compiled shaders are used.

## References

- [Source git repository on GitHub](https://github.com/microsoft/DirectXTex.git)
- [DirectXTex documentation git repository on GitHub](https://github.com/microsoft/DirectXTex.wiki.git)
- [DirectXTex test suite git repository on GitHub](https://github.com/walbourn/directxtextest.git).
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

- The code targets Win32 desktop applications for Windows 8.1 or later, Xbox One, Xbox Series X\|S, Universal Windows Platform (UWP) apps for Windows 10 and Windows 11, and Linux.
- Portability and conformance of the code is validated by building with Visual C++, clang/LLVM for Windows, MinGW, and GCC for Linux compilers.
- The project ships MSBuild projects for Visual Studio 2022 (`.sln` / `.vcxproj`) and Visual Studio 2026 (`.slnx` / `.vcxproj`). VS 2019 projects have been retired.

### Platform and Compiler `#ifdef` Guards

Use these established guards — do not invent new ones:

| Guard | Purpose |
| --- | --- |
| `_WIN32` | Windows platform (desktop, UWP, Xbox) |
| `_GAMING_XBOX` | Xbox platform (GDK - covers both Xbox One and Xbox Series X\|S) |
| `_GAMING_XBOX_SCARLETT` | Xbox Series X\|S (GDK with Xbox Extensions) |
| `_GAMING_XBOX_XBOXONE` | Xbox One (GDK with Xbox Extensions) |
| `_XBOX_ONE && _TITLE` | Legacy Xbox One XDK |
| `_MSC_VER` | MSVC-specific (and MSVC-like clang-cl) pragmas and warning suppression |
| `__clang__` | Clang/LLVM diagnostic suppressions |
| `__MINGW32__` | MinGW compatibility headers |
| `__GNUC__` | MinGW/GCC DLL attribute equivalents |
| `_M_ARM64` / `_M_X64` / `_M_IX86` | Architecture-specific code paths for MSVC (`#ifdef`) |
| `_M_ARM64EC` | ARM64EC ABI (ARM64 code with x64 interop) for MSVC |
| `__aarch64__` / `__x86_64__` / `__i386__` | Additional architecture-specific symbols for MinGW/GNUC (`#if`) |
| `USING_DIRECTX_HEADERS` | External DirectX-Headers package in use |

> `_M_ARM`/ `__arm__` is legacy 32-bit ARM which is deprecated.

Non-Windows builds (Linux/WSL) omit WIC entirely and use `<directx/dxgiformat.h>` and `<wsl/winadapter.h>` from the DirectX-Headers package instead of the Windows SDK.

### Error Codes

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
- Standalone modules for loading textures using WIC are located in `WICTextureLoader9.h`, `WICTextureLoader11.h`, and `WICTextureLoader12.h` in the `WICTextureLoader` folder.
- Standalone modules for capturing screenshots are located in `ScreenGrab9.h`, `ScreenGrab11.h`, and `ScreenGrab12.h` in the `ScreenGrab` folder.
- Compliance with the project's architecture and design patterns.
- Ensure that all public functions and classes are covered by unit tests located on [GitHub](https://github.com/walbourn/directxtextest.git) where applicable. Report any gaps in test coverage.
- Check for performance implications, especially in texture processing algorithms.
- Provide brutally honest feedback on code quality, design, and potential improvements as needed.

## Documentation Review Instructions

When reviewing documentation, do the following:

- Read the code located in [this git repository](https://github.com/microsoft/DirectXTex.git) in the main branch.
- Review the public interface defined in `DirectXTex.h`.
- Read the documentation on the wiki located in [this git repository](https://github.com/microsoft/DirectXTex.wiki.git).
- Report any specific gaps in the documentation compared to the public interface.

## Release Process

1. Ensure all changes are merged into the `main` branch and that all tests pass.
2. Git pull the local repository to ensure it is up to date with the `main` branch.
3. Run the PowerShell script `build\preparerelease.ps1` which will generate a topic branch for the release, update the version number in `CMakeLists.txt`, the `README.md` file, the release notes in the nuspec files, and create a stub in the `CHANGELOG.md` file for the new release.
4. Edit the `CHANGELOG.md` file to update it with a summary of changes.
5. Submit the topic branch for review and merge into `main` once approved. Allow the GitHub Actions workflows and the Azure DevOps pipelines to complete successfully before proceeding.
6. Run the PowerShell script `build\completerelease.ps1` which will set a tag on the project repo and the test repo, and create a release on GitHub with the release notes from `CHANGELOG.md`. Ensure you have set up GPG signing for your GitHub account so that the tags will be verified.
7. Git pull the local repository to ensure it is up to date with the `main` branch. Be sure to include `--tags`.
8. Push the `main` branch to the MSCodeHub mirror repository. Be sure to include `--tags`.
9. Create a PR on MSCodeHub from the `main` branch to the `release` branch.
10. Merge the PR on MSCodeHub to update the release branch, which will trigger the Azure DevOps pipeline to build signed binaries and the NuGet packages.
11. Run the PowerShell script `build\downloadartifacts.ps1` to download the signed binaries from the Azure DevOps pipeline artifacts.
12. Edit the GitHub release and upload the signed binaries to the release assets.
13. Download the GitHub source .zip archive from the release. Unzip and compare to the local repo to ensure it matches — keep in mind there may be some CR/LF differences. Run minisign on the .zip to generate a signature file, and upload the signature file to the release assets.
14. Validate the NuGet packages with <https://github.com/walbourn/contentexporter> by pushing the NuGet packages to a local Packages Source folder, updating the NuGet packages from that folder, and then build the project.
15. Run the PowerShell script `build\promotenuget.ps1` with the `-Release` parameter to promote the version to the Release view on the project-scoped ADO feed.
16. Run the MSCodeHub pipeline to publish the NuGet packages to nuget.org. The pipeline will automatically push the most recent package promoted to the Release view to nuget.org.
17. Git pull a local repository of VCPKG to `d:\vcpkg` in sync with the `main` branch of the VCPKG repository.
18. Run the PowerShell script `build\updatevcpkg.ps1` to update the DirectXTex port in VCPKG with the new release version. This will edit the files in `ports\directxtex`.
19. Test the VCPKG port using all appropriate triplets and features.
20. Run `.\vcpkg --x-add-version directxtex` to update the VCPKG versioning history.
21. Submit a PR to the VCPKG repository to update the DirectXTex port back to the main GitHub repo. The PR will be reviewed and merged by the VCPKG maintainers.
22. If relevant changes were made to the `texassemble`, `texconv` or `texdiag` tools, update the winget manifests for those tools in the `winget` repository.
- Git pull a local repository to `D:\winget-pkgs` in sync with the `master` branch of the WinGet repository.
- Run the PowerShell script `build\updatewinget.ps1` to update the winget manifests for the tools with the new release version.
- Submit a PR to the `winget` repository to update the manifests for each tool — they must be done as distinct PRs. The PRs will be reviewed and merged by the winget maintainers.

> When fully completed, be sure to update the GitHub release with links to the matching NuGet packages, the VCPKG port, and the winget manifests for the tools.
