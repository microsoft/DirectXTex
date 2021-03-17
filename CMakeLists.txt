# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required (VERSION 3.11)

set(DIRECTXTEX_VERSION 1.9.1)

project (DirectXTex
  VERSION ${DIRECTXTEX_VERSION}
  DESCRIPTION "DirectX Texture Library"
  HOMEPAGE_URL "https://go.microsoft.com/fwlink/?LinkId=248926"
  LANGUAGES CXX)

option(BUILD_TOOLS "Build tex command-line tools" ON)

# Includes the functions for Direct3D 11 resources and DirectCompute compression
option(BUILD_DX11 "Build with DirectX11 Runtime support" ON)

# Includes the functions for creating Direct3D 12 resources at runtime
option(BUILD_DX12 "Build with DirectX12 Runtime support" ON)

# Enable the use of OpenMP for software BC6H/BC7 compression
option(BC_USE_OPENMP "Build with OpenMP support" ON)

option(ENABLE_CODE_ANALYSIS "Use Static Code Analysis on build" OFF)

# Includes the functions for loading/saving OpenEXR files at runtime
# NOTE requires adding DirectXTexEXR.h/.cpp source files (vcpkg does this automatically)
option(ENABLE_OPENEXR_SUPPORT "Build with OpenEXR support" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")

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
    set(LIBRARY_SOURCES ${LIBRARY_SOURCES}
        DirectXTex/Shaders/Compiled/BC6HEncode_EncodeBlockCS.inc)

   add_custom_command(
       OUTPUT "${PROJECT_SOURCE_DIR}/DirectXTex/Shaders/Compiled/BC6HEncode_EncodeBlockCS.inc"
       MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/DirectXTex/Shaders/CompileShaders.cmd"
       DEPENDS ${SHADER_SOURCES}
       COMMENT "Generating HLSL shaders..."
       COMMAND "CompileShaders.cmd"
       WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/DirectXTex/Shaders"
       USES_TERMINAL)
endif()

add_library(${PROJECT_NAME} STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})

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
    target_link_libraries(texassemble ${OPENEXR_ILMIMF_LIBRARY})
    target_link_libraries(texconv ${OPENEXR_ILMIMF_LIBRARY})
    target_link_libraries(texdiag ${OPENEXR_ILMIMF_LIBRARY})

    target_compile_options(texassemble PRIVATE -DUSE_OPENEXR)
    target_compile_options(texconv PRIVATE -DUSE_OPENEXR)
    target_compile_options(texdiag PRIVATE -DUSE_OPENEXR)
  endif()

  if (VCPKG_TOOLCHAIN)
    target_link_libraries(texassemble Microsoft::DirectXMath)
    target_link_libraries(texconv Microsoft::DirectXMath)
    target_link_libraries(texdiag Microsoft::DirectXMath)
  endif()
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /fp:fast)
    if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
      target_compile_options(texassemble PRIVATE /fp:fast)
      target_compile_options(texconv PRIVATE /fp:fast)
      target_compile_options(texdiag PRIVATE /fp:fast)
    endif()

    if ((${CMAKE_SIZEOF_VOID_P} EQUAL "4") AND (NOT ${CMAKE_VS_PLATFORM_NAME} MATCHES "arm"))
        target_compile_options(${PROJECT_NAME} PRIVATE /arch:SSE2)
        if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
          target_compile_options(texassemble PRIVATE /arch:SSE2)
          target_compile_options(texconv PRIVATE /arch:SSE2)
          target_compile_options(texdiag PRIVATE /arch:SSE2)
        endif()
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(WarningsLib "-Wpedantic" "-Wextra")
    target_compile_options(${PROJECT_NAME} PRIVATE ${WarningsLib})

    # OpenMP is not supported for clang for Windows by default

    if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
      set(WarningsEXE ${WarningsLib} "-Wno-c++98-compat" "-Wno-c++98-compat-pedantic" "-Wno-switch" "-Wno-switch-enum" "-Wno-language-extension-token" "-Wno-missing-prototypes")
      target_compile_options(texassemble PRIVATE ${WarningsEXE})
      target_compile_options(texconv PRIVATE ${WarningsEXE} "-Wno-global-constructors")
      target_compile_options(texdiag PRIVATE ${WarningsEXE} "-Wno-double-promotion")
    endif()
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE /permissive- /JMC- /Zc:__cplusplus)
    if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
      target_compile_options(texassemble PRIVATE /permissive- /Zc:__cplusplus)
      target_compile_options(texconv PRIVATE /permissive- /Zc:__cplusplus)
      target_compile_options(texdiag PRIVATE /permissive- /Zc:__cplusplus)
    endif()

    if(ENABLE_CODE_ANALYSIS)
      target_compile_options(${PROJECT_NAME} PRIVATE /analyze)
      if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
        target_compile_options(texassemble PRIVATE /analyze)
        target_compile_options(texconv PRIVATE /analyze)
        target_compile_options(texdiag PRIVATE /analyze)
      endif()
    endif()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.26)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor /wd5105)
        if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
          target_compile_options(texassemble PRIVATE /Zc:preprocessor /wd5105)
          target_compile_options(texconv PRIVATE /Zc:preprocessor /wd5105)
          target_compile_options(texdiag PRIVATE /Zc:preprocessor /wd5105)
        endif()
    endif()

    if(BC_USE_OPENMP)
        target_compile_options(${PROJECT_NAME} PRIVATE /openmp /Zc:twoPhase-)
        if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
          target_compile_options(texconv PRIVATE /openmp /Zc:twoPhase-)
        endif()
    endif()

    if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
      set(WarningsEXE "/wd4061" "/wd4062" "/wd4365" "/wd4514" "/wd4625" "/wd4626" "/wd4627" "/wd4668" "/wd4710" "/wd4751" "/wd4820" "/wd5026" "/wd5027" "/wd5039" "/wd5045" "/wd5219")
      target_compile_options(texassemble PRIVATE ${WarningsEXE})
      target_compile_options(texconv PRIVATE ${WarningsEXE})
      target_compile_options(texdiag PRIVATE ${WarningsEXE})
    endif()
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE UNICODE)
    if(BUILD_TOOLS AND (NOT WINDOWS_STORE))
      target_compile_definitions(texassemble PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0601)
      target_compile_definitions(texconv PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0601)
      target_compile_definitions(texdiag PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0601)
    endif()

    if(BUILD_DX12 OR WINDOWS_STORE)
        target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0A00)
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    endif()
endif()

if(BUILD_TOOLS AND WIN32 AND (NOT WINDOWS_STORE))
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT texconv)
endif()
