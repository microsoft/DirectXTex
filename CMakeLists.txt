# DirectX Texture Library
#
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
#
# http://go.microsoft.com/fwlink/?LinkId=248926

cmake_minimum_required (VERSION 3.11)

project (DirectXTex LANGUAGES CXX)

# Includes the functions for creating Direct3D 11 resources at runtime
option(BUILD_DX11 "Build with DirectX11 Runtime support" ON)

# Includes the functions for creating Direct3D 12 resources at runtime
option(BUILD_DX12 "Build with DirectX12 Runtime support" ON)

# Enable the use of OpenMP for software BC6H/BC7 compression
option(BC_USE_OPENMP "Build with OpenMP support" ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/CMake")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/CMake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/CMake")

set(LIBRARY_SOURCES
    DirectXTex/BC.h
    DirectXTex/BCDirectCompute.h
    DirectXTex/d3dx12.h
    DirectXTex/DDS.h
    DirectXTex/DirectXTex.h
    DirectXTex/DirectXTexP.h
    DirectXTex/filters.h
    DirectXTex/scoped.h
    DirectXTex/BC.cpp
    DirectXTex/BC4BC5.cpp
    DirectXTex/BC6HBC7.cpp
    DirectXTex/BCDirectCompute.cpp
    DirectXTex/DirectXTexCompress.cpp
    DirectXTex/DirectXTexCompressGPU.cpp
    DirectXTex/DirectXTexConvert.cpp
    DirectXTex/DirectXTexDDS.cpp
    DirectXTex/DirectXTexFlipRotate.cpp
    DirectXTex/DirectXTexHDR.cpp
    DirectXTex/DirectXTexImage.cpp
    DirectXTex/DirectXTexMipmaps.cpp
    DirectXTex/DirectXTexMisc.cpp
    DirectXTex/DirectXTexNormalMaps.cpp
    DirectXTex/DirectXTexPMAlpha.cpp
    DirectXTex/DirectXTexResize.cpp
    DirectXTex/DirectXTexTGA.cpp
    DirectXTex/DirectXTexUtil.cpp
    DirectXTex/DirectXTexWIC.cpp)

set(SHADER_SOURCES
    DirectXTex/Shaders/BC6HEncode.hlsl
    DirectXTex/Shaders/BC7Encode.hlsl)

if(BUILD_DX11 MATCHES ON)
   set(LIBRARY_SOURCES ${LIBRARY_SOURCES} DirectXTex/DirectXTexD3D11.cpp)
endif()
if(BUILD_DX12 MATCHES ON)
   set(LIBRARY_SOURCES ${LIBRARY_SOURCES} DirectXTex/DirectXTexD3D12.cpp)
endif()

add_library(${PROJECT_NAME} STATIC ${LIBRARY_SOURCES} DirectXTex/Shaders/Compiled/BC6HEncode_EncodeBlockCS.inc)

add_custom_command(
    OUTPUT "${CMAKE_SOURCE_DIR}/DirectXTex/Shaders/Compiled/BC6HEncode_EncodeBlockCS.inc"
    MAIN_DEPENDENCY "${CMAKE_SOURCE_DIR}/DirectXTex/Shaders/CompileShaders.cmd"
    DEPENDS ${SHADER_SOURCES}
    COMMENT "Generating HLSL shaders..."
    COMMAND "CompileShaders.cmd"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/DirectXTex/Shaders"
    USES_TERMINAL)

source_group(${PROJECT_NAME} REGULAR_EXPRESSION DirectXTex/*.*)

target_include_directories(${PROJECT_NAME} PUBLIC DirectXTex)

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

add_executable(texassemble Texassemble/texassemble.cpp)
target_link_libraries(texassemble ${PROJECT_NAME})
source_group(texassemble REGULAR_EXPRESSION Texassemble/*.*)

add_executable(texconv Texconv/texconv.cpp)
target_link_libraries(texconv ${PROJECT_NAME})
source_group(texconv REGULAR_EXPRESSION Texconv/*.*)

add_executable(texdiag Texdiag/texdiag.cpp)
target_link_libraries(texdiag ${PROJECT_NAME})
source_group(texdiag REGULAR_EXPRESSION Texdiag/*.*)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /fp:fast)
    target_compile_options(texassemble PRIVATE /fp:fast)
    target_compile_options(texconv PRIVATE /fp:fast)
    target_compile_options(texdiag PRIVATE /fp:fast)

    if (${CMAKE_SIZEOF_VOID_P} EQUAL "4")
        target_compile_options(${PROJECT_NAME} PRIVATE /arch:SSE2)
        target_compile_options(texassemble PRIVATE /arch:SSE2)
        target_compile_options(texconv PRIVATE /arch:SSE2)
        target_compile_options(texdiag PRIVATE /arch:SSE2)
    endif()
endif()

if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    set(WarningsLib "-Wpedantic" "-Wextra")
    target_compile_options(${PROJECT_NAME} PRIVATE ${WarningsLib})

    # OpenMP is not supported for clang for Windows by default

    set(WarningsEXE ${WarningsLib} "-Wno-c++98-compat" "-Wno-c++98-compat-pedantic" "-Wno-switch" "-Wno-switch-enum" "-Wno-language-extension-token" "-Wno-missing-prototypes")
    target_compile_options(texassemble PRIVATE ${WarningsEXE})
    target_compile_options(texconv PRIVATE ${WarningsEXE})
    target_compile_options(texdiag PRIVATE ${WarningsEXE} "-Wno-double-promotion" )
endif()
if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
    target_compile_options(${PROJECT_NAME} PRIVATE /permissive- /JMC- /Zc:__cplusplus)
    target_compile_options(texassemble PRIVATE /permissive- /Zc:__cplusplus)
    target_compile_options(texconv PRIVATE /permissive- /Zc:__cplusplus)
    target_compile_options(texdiag PRIVATE /permissive- /Zc:__cplusplus)

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.26)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor /wd5105)
        target_compile_options(texassemble PRIVATE /Zc:preprocessor /wd5105)
        target_compile_options(texconv PRIVATE /Zc:preprocessor /wd5105)
        target_compile_options(texdiag PRIVATE /Zc:preprocessor /wd5105)
    endif()

    if(BC_USE_OPENMP MATCHES ON)
        target_compile_options(${PROJECT_NAME} PRIVATE /openmp /Zc:twoPhase-)
        target_compile_options(texconv PRIVATE /openmp /Zc:twoPhase-)
    endif()

    set(WarningsEXE "/wd4061" "/wd4062" "/wd4365" "/wd4668" "/wd4710" "/wd4820" "/wd5039" "/wd5045")
    target_compile_options(texassemble PRIVATE ${WarningsEXE})
    target_compile_options(texconv PRIVATE ${WarningsEXE})
    target_compile_options(texdiag PRIVATE ${WarningsEXE})
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE UNICODE)
    target_compile_definitions(texassemble PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0601)
    target_compile_definitions(texconv PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0601)
    target_compile_definitions(texdiag PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0601)

    if(BUILD_DX12 MATCHES ON)
        target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0A00)
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    endif()
endif()
