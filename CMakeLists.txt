# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required (VERSION 3.13)

set(DIRECTXTEX_VERSION 1.9.5)

project (DirectXTex
  VERSION ${DIRECTXTEX_VERSION}
  DESCRIPTION "DirectX Texture Library"
  HOMEPAGE_URL "https://go.microsoft.com/fwlink/?LinkId=248926"
  LANGUAGES CXX)

option(BUILD_TOOLS "Build tex command-line tools" ON)

option(BUILD_SAMPLE "Build DDSView sample" ON)

# Includes the functions for Direct3D 11 resources and DirectCompute compression
option(BUILD_DX11 "Build with DirectX11 Runtime support" ON)

# Includes the functions for creating Direct3D 12 resources at runtime
option(BUILD_DX12 "Build with DirectX12 Runtime support" ON)

# Enable the use of OpenMP for software BC6H/BC7 compression
option(BC_USE_OPENMP "Build with OpenMP support" ON)

option(ENABLE_CODE_ANALYSIS "Use Static Code Analysis on build" OFF)

option(USE_PREBUILT_SHADERS "Use externally built HLSL shaders" OFF)

# Includes the functions for loading/saving OpenEXR files at runtime
# NOTE requires adding DirectXTexEXR.h/.cpp source files (vcpkg does this automatically)
option(ENABLE_OPENEXR_SUPPORT "Build with OpenEXR support" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")

if (DEFINED VCPKG_TARGET_ARCHITECTURE)
    set(DIRECTX_ARCH ${VCPKG_TARGET_ARCHITECTURE})
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Ww][Ii][Nn]32$")
    set(DIRECTX_ARCH x86)
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Xx]64$")
    set(DIRECTX_ARCH x64)
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Aa][Rr][Mm]$")
    set(DIRECTX_ARCH arm)
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Aa][Rr][Mm]64$")
    set(DIRECTX_ARCH arm64)
endif()

#--- Library
set(LIBRARY_HEADERS
    DirectXTex/DirectXTex.h
    DirectXTex/DirectXTex.inl)

set(LIBRARY_SOURCES
    DirectXTex/BC.h
    DirectXTex/DDS.h
    DirectXTex/DirectXTexP.h
    DirectXTex/filters.h
    DirectXTex/scoped.h
    DirectXTex/BC.cpp
    DirectXTex/BC4BC5.cpp
    DirectXTex/BC6HBC7.cpp
    DirectXTex/DirectXTexCompress.cpp
    DirectXTex/DirectXTexConvert.cpp
    DirectXTex/DirectXTexDDS.cpp
    DirectXTex/DirectXTexHDR.cpp
    DirectXTex/DirectXTexImage.cpp
    DirectXTex/DirectXTexMipmaps.cpp
    DirectXTex/DirectXTexMisc.cpp
    DirectXTex/DirectXTexNormalMaps.cpp
    DirectXTex/DirectXTexPMAlpha.cpp
    DirectXTex/DirectXTexResize.cpp
    DirectXTex/DirectXTexTGA.cpp
    DirectXTex/DirectXTexUtil.cpp)

if(WIN32)
   set(LIBRARY_SOURCES ${LIBRARY_SOURCES}
       DirectXTex/DirectXTexFlipRotate.cpp
       DirectXTex/DirectXTexWIC.cpp)
endif()

set(SHADER_SOURCES
    DirectXTex/Shaders/BC6HEncode.hlsl
    DirectXTex/Shaders/BC7Encode.hlsl)

if(BUILD_DX11 AND WIN32)
   set(LIBRARY_SOURCES ${LIBRARY_SOURCES}
       DirectXTex/BCDirectCompute.h
       DirectXTex/BCDirectCompute.cpp
       DirectXTex/DirectXTexCompressGPU.cpp
       DirectXTex/DirectXTexD3D11.cpp)
endif()

if(BUILD_DX12)
   set(LIBRARY_SOURCES ${LIBRARY_SOURCES}
       DirectXTex/d3dx12.h
       DirectXTex/DirectXTexD3D12.cpp)
endif()

if(ENABLE_OPENEXR_SUPPORT)
   set(LIBRARY_HEADERS ${LIBRARY_HEADERS}
       DirectXTex/DirectXTexEXR.h)

   set(LIBRARY_SOURCES ${LIBRARY_SOURCES}
       DirectXTex/DirectXTexEXR.h
       DirectXTex/DirectXTexEXR.cpp)
endif()

if(BUILD_DX11 AND WIN32)
    if (NOT COMPILED_SHADERS)
        if (USE_PREBUILT_SHADERS)
            message(FATAL_ERROR "ERROR: Using prebuilt shaders requires the COMPILED_SHADERS variable is set")
        endif()
        set(COMPILED_SHADERS ${CMAKE_CURRENT_BINARY_DIR}/Shaders/Compiled)
        file(MAKE_DIRECTORY ${COMPILED_SHADERS})
    endif()

    set(LIBRARY_SOURCES ${LIBRARY_SOURCES}
        ${COMPILED_SHADERS}/BC6HEncode_EncodeBlockCS.inc)

    if (NOT USE_PREBUILT_SHADERS)
        add_custom_command(
            OUTPUT "${COMPILED_SHADERS}/BC6HEncode_EncodeBlockCS.inc"
            MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/DirectXTex/Shaders/CompileShaders.cmd"
            DEPENDS ${SHADER_SOURCES}
            COMMENT "Generating HLSL shaders..."
            COMMAND set CompileShadersOutput=${COMPILED_SHADERS}
            COMMAND CompileShaders.cmd
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/DirectXTex/Shaders"
            USES_TERMINAL)
    endif()
endif()

add_library(${PROJECT_NAME} STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})

if(BUILD_DX11 AND WIN32)
   target_include_directories(${PROJECT_NAME} PRIVATE ${COMPILED_SHADERS})
endif()

source_group(${PROJECT_NAME} REGULAR_EXPRESSION DirectXTex/*.*)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/DirectXTex>
  $<INSTALL_INTERFACE:include>)

if(ENABLE_OPENEXR_SUPPORT)
  find_package(OpenEXR REQUIRED)
  target_include_directories(${PROJECT_NAME} PRIVATE ${OPENEXR_INCLUDE_DIRS}/OpenEXR)
endif()

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.16")
    target_precompile_headers(${PROJECT_NAME} PRIVATE DirectXTex/DirectXTexP.h)
endif()

if(MSVC)
    # Use max Warning Level
    string(REPLACE "/W3 " "/Wall " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    string(REPLACE "/W3 " "/Wall " CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    string(REPLACE "/W3 " "/Wall " CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})

    # Not using typeid or dynamic_cast, so disable RTTI to save binary size
    string(REPLACE "/GR " "/GR- " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    string(REPLACE "/GR " "/GR- " CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    string(REPLACE "/GR " "/GR- " CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
endif()

if ((NOT WIN32) OR VCPKG_TOOLCHAIN)
    message("INFO: Using VCPKG for DirectX-Headers and DirectXMath.")
    find_package(directx-headers CONFIG REQUIRED)
    find_package(directxmath CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Microsoft::DirectX-Headers Microsoft::DirectXMath)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USING_DIRECTX_HEADERS)
endif()

#--- Package
include(CMakePackageConfigHelpers)

string(TOLOWER ${PROJECT_NAME} PACKAGE_NAME)

write_basic_package_version_file(
  ${PROJECT_BINARY_DIR}/cmake/${PACKAGE_NAME}-config-version.cmake
  VERSION ${DIRECTXTEX_VERSION}
  COMPATIBILITY AnyNewerVersion)

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}-targets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/.nuget/${PROJECT_NAME}-config.cmake.in
  ${PROJECT_BINARY_DIR}/cmake/${PACKAGE_NAME}-config.cmake
  INSTALL_DESTINATION cmake/})

install(EXPORT ${PROJECT_NAME}-targets
  FILE ${PROJECT_NAME}-targets.cmake
  NAMESPACE Microsoft::
  DESTINATION cmake/)

install(FILES ${LIBRARY_HEADERS}
  DESTINATION include)

install(FILES
    ${PROJECT_BINARY_DIR}/cmake/${PACKAGE_NAME}-config.cmake
    ${PROJECT_BINARY_DIR}/cmake/${PACKAGE_NAME}-config-version.cmake
  DESTINATION cmake/)

#--- Command-line tools
if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
  set(TOOL_EXES texassemble texconv texdiag)

  add_executable(texassemble
    Texassemble/texassemble.cpp
    Texassemble/AnimatedGif.cpp)
  target_link_libraries(texassemble ${PROJECT_NAME} ole32.lib version.lib)
  source_group(texassemble REGULAR_EXPRESSION Texassemble/*.*)

  add_executable(texconv
    Texconv/texconv.cpp
    Texconv/ExtendedBMP.cpp
    Texconv/PortablePixMap.cpp)
  target_link_libraries(texconv ${PROJECT_NAME} ole32.lib shell32.lib version.lib)
  source_group(texconv REGULAR_EXPRESSION Texconv/*.*)

  add_executable(texdiag Texdiag/texdiag.cpp)
  target_link_libraries(texdiag ${PROJECT_NAME} ole32.lib version.lib)
  source_group(texdiag REGULAR_EXPRESSION Texdiag/*.*)

  if(ENABLE_OPENEXR_SUPPORT)
    foreach(t IN LISTS TOOL_EXES)
      target_link_libraries(${t} ${OPENEXR_ILMIMF_LIBRARY})
      target_compile_options(${t} PRIVATE -DUSE_OPENEXR)
    endforeach()
  endif()

  if (VCPKG_TOOLCHAIN)
    foreach(t IN LISTS TOOL_EXES)
      target_link_libraries(${t} Microsoft::DirectXMath)
    endforeach()
  endif()
endif()

#--- DDSView sample
if(BUILD_SAMPLE AND WIN32 AND (NOT WINDOWS_STORE))
  list(APPEND TOOL_EXES ddsview)

  add_executable(ddsview WIN32
    DDSView/ddsview.cpp
    DDSView/ddsview.rc)
  target_link_libraries(ddsview ${PROJECT_NAME} d3d11.lib)
  source_group(ddsview REGULAR_EXPRESSION DDSView/*.*)
endif()

if(MSVC)
    foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
      target_compile_options(${t} PRIVATE /fp:fast "$<$<NOT:$<CONFIG:DEBUG>>:/guard:cf>")
      target_link_options(${t} PRIVATE /DYNAMICBASE /NXCOMPAT)
    endforeach()

    if((${CMAKE_SIZEOF_VOID_P} EQUAL 4) AND (NOT ${DIRECTX_ARCH} MATCHES "arm"))
        foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
          target_compile_options(${t} PRIVATE /arch:SSE2)
          target_link_options(${t} PRIVATE /SAFESEH)
        endforeach()
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(WarningsLib "-Wpedantic" "-Wextra")
    target_compile_options(${PROJECT_NAME} PRIVATE ${WarningsLib})

    # OpenMP is not supported for clang for Windows by default

    set(WarningsEXE ${WarningsLib} "-Wno-c++98-compat" "-Wno-c++98-compat-pedantic" "-Wno-switch" "-Wno-switch-enum" "-Wno-covered-switch-default" "-Wno-language-extension-token" "-Wno-missing-prototypes" "-Wno-global-constructors" "-Wno-double-promotion")
    foreach(t IN LISTS TOOL_EXES)
      target_compile_options(${t} PRIVATE ${WarningsEXE})
    endforeach()
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
      target_compile_options(${t} PRIVATE /sdl /permissive- /Zc:__cplusplus)
    endforeach()

    if(ENABLE_CODE_ANALYSIS)
      foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
        target_compile_options(${t} PRIVATE /analyze)
      endforeach()
    endif()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.24)
        foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
          target_compile_options(${t} PRIVATE /ZH:SHA_256)
        endforeach()
    endif()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.26)
        foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
          target_compile_options(${t} PRIVATE /Zc:preprocessor /wd5105)
        endforeach()
    endif()

    if ((CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.27) AND (NOT (${DIRECTX_ARCH} MATCHES "arm(64)?")))
        foreach(t IN LISTS TOOL_EXES)
          target_link_options(${t} PRIVATE /CETCOMPAT)
        endforeach()
    endif()

    if(BC_USE_OPENMP)
        target_compile_options(${PROJECT_NAME} PRIVATE /openmp /Zc:twoPhase-)
        if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
          target_compile_options(texconv PRIVATE /openmp /Zc:twoPhase-)
        endif()
    endif()

    set(WarningsEXE "/wd4061" "/wd4062" "/wd4365" "/wd4514" "/wd4625" "/wd4626" "/wd4627" "/wd4668" "/wd4710" "/wd4751" "/wd4820" "/wd5026" "/wd5027" "/wd5039" "/wd5045" "/wd5219")
    foreach(t IN LISTS TOOL_EXES)
      target_compile_options(${t} PRIVATE ${WarningsEXE})
    endforeach()
endif()

if(WIN32)
    foreach(t IN LISTS TOOL_EXES)
      target_compile_definitions(${t} PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0601)
    endforeach()

    if(WINDOWS_STORE)
      target_compile_definitions(${PROJECT_NAME} PRIVATE WINAPI_FAMILY=WINAPI_FAMILY_APP)
    endif()

    if(BUILD_DX12 OR WINDOWS_STORE)
        if(BUILD_DX12)
          message("INFO: Building with DirectX 12 Runtime support")
        endif()
        target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0A00)
    else()
        message("INFO: Building with Windows 7 compatibility")
        target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    endif()
endif()

if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT texconv)
endif()
