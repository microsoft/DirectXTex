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

- **Code Style**: The project uses an .editorconfig file to enforce coding standards. Follow the rules defined in `.editorconfig` for indentation, line endings, and other formatting. Additional information can be found on the wiki at [Implementation](https://github.com/microsoft/DirectXTK/wiki/Implementation). The code requires C++11/C++14 features.
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

### Patterns to Avoid

- Don’t use raw pointers for ownership.
- Avoid macros for constants—prefer `constexpr` or `inline` `const`.
- Don’t put implementation logic in header files unless using templates, although the SimpleMath library does use an .inl file for performance.
- Avoid using `using namespace` in header files to prevent polluting the global namespace.

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
