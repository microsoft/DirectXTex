# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required (VERSION 3.20)

set(DIRECTXTEX_VERSION 2.0.1)

if(WINDOWS_STORE OR (DEFINED XBOX_CONSOLE_TARGET))
  set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
endif()

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

# https://devblogs.microsoft.com/cppblog/spectre-mitigations-in-msvc/
option(ENABLE_SPECTRE_MITIGATION "Build using /Qspectre for MSVC" OFF)

option(DISABLE_MSVC_ITERATOR_DEBUGGING "Disable iterator debugging in Debug configurations with the MSVC CRT" OFF)

option(ENABLE_CODE_ANALYSIS "Use Static Code Analysis on build" OFF)

option(USE_PREBUILT_SHADERS "Use externally built HLSL shaders" OFF)

option(NO_WCHAR_T "Use legacy wide-character as unsigned short" OFF)

# Includes the functions for loading/saving OpenEXR files at runtime
option(ENABLE_OPENEXR_SUPPORT "Build with OpenEXR support" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")

if(DEFINED VCPKG_TARGET_ARCHITECTURE)
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

if(WINDOWS_STORE OR (DEFINED XBOX_CONSOLE_TARGET))
   set(BUILD_DX12 ON)
   set(BUILD_TOOLS OFF)
   set(BUILD_SAMPLE OFF)
endif()

if(DEFINED XBOX_CONSOLE_TARGET)
  set(BUILD_DX11 OFF)
endif()

include(GNUInstallDirs)

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
       Auxiliary/DirectXTexEXR.h)

   set(LIBRARY_SOURCES ${LIBRARY_SOURCES}
       Auxiliary/DirectXTexEXR.cpp)
endif()

if(BUILD_DX11 AND WIN32)
    if(NOT COMPILED_SHADERS)
        if(USE_PREBUILT_SHADERS)
            message(FATAL_ERROR "ERROR: Using prebuilt shaders requires the COMPILED_SHADERS variable is set")
        endif()
        set(COMPILED_SHADERS ${CMAKE_CURRENT_BINARY_DIR}/Shaders/Compiled)
        file(MAKE_DIRECTORY ${COMPILED_SHADERS})
    else()
        file(TO_CMAKE_PATH ${COMPILED_SHADERS} COMPILED_SHADERS)
    endif()

    set(LIBRARY_SOURCES ${LIBRARY_SOURCES}
        ${COMPILED_SHADERS}/BC6HEncode_EncodeBlockCS.inc)

    if(NOT USE_PREBUILT_SHADERS)
        add_custom_command(
            OUTPUT "${COMPILED_SHADERS}/BC6HEncode_EncodeBlockCS.inc"
            MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/DirectXTex/Shaders/CompileShaders.cmd"
            DEPENDS ${SHADER_SOURCES}
            COMMENT "Generating HLSL shaders..."
            COMMAND ${CMAKE_COMMAND} -E env CompileShadersOutput="${COMPILED_SHADERS}" CompileShaders.cmd > "${COMPILED_SHADERS}/compileshaders.log"
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
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)

if(BC_USE_OPENMP)
  find_package(OpenMP)
  if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX)
  else()
    set(BC_USE_OPENMP OFF)
  endif()
endif()

if(ENABLE_OPENEXR_SUPPORT)
  find_package(OpenEXR REQUIRED)
  target_link_libraries(${PROJECT_NAME} PUBLIC OpenEXR::OpenEXR)
endif()

if(NOT MINGW)
    target_precompile_headers(${PROJECT_NAME} PRIVATE DirectXTex/DirectXTexP.h)
endif()

if(MINGW OR (NOT WIN32))
    find_package(directxmath CONFIG REQUIRED)
    find_package(directx-headers CONFIG REQUIRED)
else()
    find_package(directxmath CONFIG QUIET)
    find_package(directx-headers CONFIG QUIET)
endif()

if(directxmath_FOUND)
    message(STATUS "Using DirectXMath package")
    target_link_libraries(${PROJECT_NAME} PUBLIC Microsoft::DirectXMath)
endif()

if(directx-headers_FOUND)
    message(STATUS "Using DirectX-Headers package")
    target_link_libraries(${PROJECT_NAME} PUBLIC Microsoft::DirectX-Headers)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USING_DIRECTX_HEADERS)
endif()

include(CheckIncludeFileCXX)

if(DEFINED XBOX_CONSOLE_TARGET)
    message(STATUS "Building for Xbox Console Target: ${XBOX_CONSOLE_TARGET}")
    set(CMAKE_REQUIRED_QUIET ON)
    if(XBOX_CONSOLE_TARGET STREQUAL "durango")
        CHECK_INCLUDE_FILE_CXX(xdk.h XDKLegacy_HEADER)
        if(NOT XDKLegacy_HEADER)
            message(FATAL_ERROR "Legacy Xbox One XDK required to build for Durango.")
        endif()
        target_compile_definitions(${PROJECT_NAME} PUBLIC WINAPI_FAMILY=WINAPI_FAMILY_TV_TITLE _XBOX_ONE _TITLE MONOLITHIC=1)
    else()
        CHECK_INCLUDE_FILE_CXX(gxdk.h GXDK_HEADER)
        if(NOT GXDK_HEADER)
            message(FATAL_ERROR "Microsoft GDK with Xbox Extensions required to build for Xbox. See https://aka.ms/gdkx")
        endif()
        target_compile_definitions(${PROJECT_NAME} PUBLIC WINAPI_FAMILY=WINAPI_FAMILY_GAMES)
        if(XBOX_CONSOLE_TARGET STREQUAL "scarlett")
            CHECK_INCLUDE_FILE_CXX(d3d12_xs.h D3D12XS_HEADER)
            if(NOT D3D12XS_HEADER)
                message(FATAL_ERROR "Microsoft GDK with Xbox Extensions environment needs to be set for Xbox Series X|S.")
            endif()
            target_compile_definitions(${PROJECT_NAME} PUBLIC _GAMING_XBOX _GAMING_XBOX_SCARLETT)
        elseif(XBOX_CONSOLE_TARGET STREQUAL "xboxone")
            CHECK_INCLUDE_FILE_CXX(d3d12_x.h D3D12X_HEADER)
            if(NOT D3D12X_HEADER)
                message(FATAL_ERROR "Microsoft GDK with Xbox Extensions environment needs to be set for Xbox One.")
            endif()
            target_compile_definitions(${PROJECT_NAME} PUBLIC _GAMING_XBOX _GAMING_XBOX_XBOXONE)
        else()
            message(FATAL_ERROR "Unknown XBOX_CONSOLE_TARGET")
        endif()
    endif()
elseif(WINDOWS_STORE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC WINAPI_FAMILY=WINAPI_FAMILY_APP)
endif()

#--- Package
include(CMakePackageConfigHelpers)

string(TOLOWER ${PROJECT_NAME} PACKAGE_NAME)

write_basic_package_version_file(
  ${PACKAGE_NAME}-config-version.cmake
  VERSION ${DIRECTXTEX_VERSION}
  COMPATIBILITY AnyNewerVersion)

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/build/${PROJECT_NAME}-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PACKAGE_NAME})

install(EXPORT ${PROJECT_NAME}-targets
  FILE ${PROJECT_NAME}-targets.cmake
  NAMESPACE Microsoft::
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PACKAGE_NAME})

install(FILES ${LIBRARY_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PACKAGE_NAME})

#--- Command-line tools
if(BUILD_TOOLS AND BUILD_DX11 AND WIN32)
  set(TOOL_EXES texassemble texconv texdiag)

  add_executable(texassemble
    Texassemble/texassemble.cpp
    Texassemble/texassemble.rc
    Texassemble/settings.manifest
    Texassemble/AnimatedGif.cpp)
  target_link_libraries(texassemble PRIVATE ${PROJECT_NAME} ole32.lib version.lib)
  source_group(texassemble REGULAR_EXPRESSION Texassemble/*.*)

  add_executable(texconv
    Texconv/texconv.cpp
    Texconv/texconv.rc
    Texconv/settings.manifest
    Texconv/ExtendedBMP.cpp
    Texconv/PortablePixMap.cpp)
  target_link_libraries(texconv PRIVATE ${PROJECT_NAME} ole32.lib shell32.lib version.lib)
  source_group(texconv REGULAR_EXPRESSION Texconv/*.*)

  add_executable(texdiag
    Texdiag/texdiag.cpp
    Texdiag/texdiag.rc
    Texdiag/settings.manifest)
  target_link_libraries(texdiag PRIVATE ${PROJECT_NAME} ole32.lib version.lib)
  source_group(texdiag REGULAR_EXPRESSION Texdiag/*.*)

  if(BC_USE_OPENMP)
    target_link_libraries(texconv PRIVATE OpenMP::OpenMP_CXX)
  endif()

  if(ENABLE_OPENEXR_SUPPORT)
    foreach(t IN LISTS TOOL_EXES)
      target_include_directories(${t} PRIVATE Auxiliary)
      target_link_libraries(${t} PRIVATE ${OPENEXR_ILMIMF_LIBRARY})
      target_compile_options(${t} PRIVATE -DUSE_OPENEXR)
    endforeach()
  endif()
endif()

#--- DDSView sample
if(BUILD_SAMPLE AND BUILD_DX11 AND WIN32)
  list(APPEND TOOL_EXES ddsview)

  if(NOT COMPILED_DDSVIEW_SHADERS)
      if(USE_PREBUILT_SHADERS)
          message(FATAL_ERROR "ERROR: Using prebuilt shaders requires the COMPILED_DDSVIEW_SHADERS variable is set")
      endif()
      set(COMPILED_DDSVIEW_SHADERS ${CMAKE_CURRENT_BINARY_DIR}/Shaders/Compiled)
      file(MAKE_DIRECTORY ${COMPILED_DDSVIEW_SHADERS})
  else()
      file(TO_CMAKE_PATH ${COMPILED_DDSVIEW_SHADERS} COMPILED_DDSVIEW_SHADERS)
  endif()

  add_executable(ddsview WIN32
    DDSView/ddsview.cpp
    DDSView/ddsview.rc
    ${COMPILED_DDSVIEW_SHADERS}/ddsview_ps1D.inc)
  target_link_libraries(ddsview PRIVATE ${PROJECT_NAME} d3d11.lib ole32.lib)
  source_group(ddsview REGULAR_EXPRESSION DDSView/*.*)

  target_include_directories(ddsview PRIVATE ${COMPILED_DDSVIEW_SHADERS})

  if(NOT USE_PREBUILT_SHADERS)
      add_custom_command(
            OUTPUT "${COMPILED_DDSVIEW_SHADERS}/ddsview_ps1D.inc"
            MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/DDSView/hlsl.cmd"
            DEPENDS "${PROJECT_SOURCE_DIR}/DDSView/ddsview.fx"
            COMMENT "Generating HLSL shaders for DDSView..."
            COMMAND COMMAND ${CMAKE_COMMAND} -E env CompileShadersOutput="${COMPILED_DDSVIEW_SHADERS}" hlsl.cmd > "${COMPILED_DDSVIEW_SHADERS}/hlsl.log"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/DDSView"
            USES_TERMINAL)
  endif()
endif()

if(directxmath_FOUND)
  foreach(t IN LISTS TOOL_EXES)
    target_link_libraries(${t} PRIVATE Microsoft::DirectXMath)
  endforeach()
endif()

if(MSVC)
    foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
      target_compile_options(${t} PRIVATE /Wall /GR- /fp:fast "$<$<NOT:$<CONFIG:DEBUG>>:/guard:cf>")
      target_link_options(${t} PRIVATE /DYNAMICBASE /NXCOMPAT)
    endforeach()

    if((CMAKE_SIZEOF_VOID_P EQUAL 4) AND (NOT (${DIRECTX_ARCH} MATCHES "^arm")))
      foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
        target_link_options(${t} PRIVATE /SAFESEH)
      endforeach()
    endif()

    if(ENABLE_SPECTRE_MITIGATION
       AND (MSVC_VERSION GREATER_EQUAL 1913)
       AND (NOT WINDOWS_STORE)
       AND (NOT ENABLE_OPENEXR_SUPPORT)
       AND (NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang")))
      message(STATUS "Building Spectre-mitigated libraries")
      foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
        target_compile_options(${t} PRIVATE "/Qspectre")
      endforeach()
    endif()

    if((MSVC_VERSION GREATER_EQUAL 1924)
       AND ((NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang")) OR (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16.0)))
      foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
        target_compile_options(${t} PRIVATE /ZH:SHA_256)
      endforeach()
    endif()

    if((MSVC_VERSION GREATER_EQUAL 1928)
       AND (CMAKE_SIZEOF_VOID_P EQUAL 8)
       AND NOT ENABLE_OPENEXR_SUPPORT
       AND ((NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang")) OR (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13.0)))
      foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
        target_compile_options(${t} PRIVATE "$<$<NOT:$<CONFIG:DEBUG>>:/guard:ehcont>")
        target_link_options(${t} PRIVATE "$<$<NOT:$<CONFIG:DEBUG>>:/guard:ehcont>")
      endforeach()
    endif()

    if(NO_WCHAR_T)
      message(STATUS "Using non-native wchar_t as unsigned short")
      foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
        target_compile_options(${t} PRIVATE "/Zc:wchar_t-")
      endforeach()
    endif()
else()
    foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
      target_compile_definitions(${t} PRIVATE $<IF:$<CONFIG:DEBUG>,_DEBUG,NDEBUG>)
    endforeach()
endif()

if(XBOX_CONSOLE_TARGET STREQUAL "scarlett")
    target_compile_options(${PROJECT_NAME} PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/favor:AMD64 /arch:AVX2,-march=znver2>)
elseif(XBOX_CONSOLE_TARGET MATCHES "(xboxone|durango)")
    target_compile_options(${PROJECT_NAME} PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/favor:AMD64 /arch:AVX,-march=btver2>)
elseif(NOT (${DIRECTX_ARCH} MATCHES "^arm"))
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(ARCH_SSE2 $<$<CXX_COMPILER_ID:MSVC>:/arch:SSE2> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-msse2>)
    else()
        set(ARCH_SSE2 $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-msse2>)
    endif()

    foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
      target_compile_options(${t} PRIVATE ${ARCH_SSE2})
    endforeach()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(WarningsLib -Wall -Wpedantic -Wextra)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16.0)
        list(APPEND WarningsLib "-Wno-unsafe-buffer-usage")
    endif()
    target_compile_options(${PROJECT_NAME} PRIVATE ${WarningsLib})

    set(WarningsEXE ${WarningsLib} "-Wno-c++98-compat" "-Wno-c++98-compat-pedantic" "-Wno-switch" "-Wno-switch-enum" "-Wno-covered-switch-default" "-Wno-language-extension-token" "-Wno-missing-prototypes" "-Wno-global-constructors" "-Wno-double-promotion")
    foreach(t IN LISTS TOOL_EXES)
      target_compile_options(${t} PRIVATE ${WarningsEXE})
    endforeach()
elseif(MINGW)
    foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
      target_compile_options(${t} PRIVATE -Wno-ignored-attributes)
      target_link_options(${t} PRIVATE -municode)
    endforeach()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
      target_compile_options(${t} PRIVATE /sdl /permissive- /JMC- /Zc:__cplusplus /Zc:inline)
    endforeach()

    if(ENABLE_CODE_ANALYSIS)
      foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
        target_compile_options(${t} PRIVATE /analyze)
      endforeach()
    endif()

    if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
      message(STATUS "Building using Whole Program Optimization")
      foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
        target_compile_options(${t} PRIVATE /Gy /Gw)
      endforeach()
    endif()

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.26)
        foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
          target_compile_options(${t} PRIVATE /Zc:preprocessor /wd5105)
        endforeach()
    endif()

    if((CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.27) AND (NOT (${DIRECTX_ARCH} MATCHES "^arm")))
        foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
          target_link_options(${t} PRIVATE /CETCOMPAT)
        endforeach()
    endif()

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.28)
        foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
          target_compile_options(${t} PRIVATE /Zc:lambda)
        endforeach()
    endif()

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.35)
        foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
          target_compile_options(${t} PRIVATE /Zc:checkGwOdr $<$<VERSION_GREATER_EQUAL:${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION},10.0.22000>:/Zc:templateScope>)
        endforeach()
    endif()

    if(BC_USE_OPENMP)
        # OpenMP in MSVC is not compatible with /permissive unless you disable two-phase lookup
        target_compile_options(${PROJECT_NAME} PRIVATE /Zc:twoPhase-)
        if(BUILD_TOOLS AND BUILD_DX11 AND WIN32)
          target_compile_options(texconv PRIVATE /Zc:twoPhase-)
        endif()
    endif()

    set(WarningsEXE "/wd4061" "/wd4062" "/wd4365" "/wd4514" "/wd4625" "/wd4626" "/wd4627" "/wd4668" "/wd4710" "/wd4751" "/wd4820" "/wd5026" "/wd5027" "/wd5039" "/wd5045" "/wd5219")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.34)
        list(APPEND WarningsEXE "/wd5262" "/wd5264")
    endif()
    foreach(t IN LISTS TOOL_EXES)
      target_compile_options(${t} PRIVATE ${WarningsEXE})
    endforeach()
endif()

if(WIN32)
    if (XBOX_CONSOLE_TARGET STREQUAL "durango")
        set(WINVER 0x0602)
    elseif(BUILD_DX12 OR (${DIRECTX_ARCH} MATCHES "^arm64"))
        message(STATUS "Building with DirectX 12 Runtime support")
        set(WINVER 0x0A00)
    elseif(${DIRECTX_ARCH} MATCHES "^arm")
        set(WINVER 0x0602)
    else()
        message(STATUS "Building with Windows 7 compatibility")
        set(WINVER 0x0601)
        target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN7_PLATFORM_UPDATE)
    endif()

    foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
      target_compile_definitions(${t} PRIVATE _UNICODE UNICODE _WIN32_WINNT=${WINVER})
    endforeach()

    if(DISABLE_MSVC_ITERATOR_DEBUGGING)
      foreach(t IN LISTS TOOL_EXES ITEMS ${PROJECT_NAME})
        target_compile_definitions(${t} PRIVATE _ITERATOR_DEBUG_LEVEL=0)
      endforeach()
    endif()
endif()

if(BUILD_TOOLS AND BUILD_DX11 AND WIN32)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT texconv)
endif()

#--- Test suite
include(CTest)
if(BUILD_TESTING AND WIN32 AND (NOT WINDOWS_STORE) AND (NOT (DEFINED XBOX_CONSOLE_TARGET))
   AND (EXISTS "${CMAKE_CURRENT_LIST_DIR}/Tests/CMakeLists.txt"))
  enable_testing()
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Tests)
endif()
