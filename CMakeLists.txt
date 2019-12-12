# DirectX Texture Library
#
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
#
# http://go.microsoft.com/fwlink/?LinkId=248926

cmake_minimum_required (VERSION 3.11)

project (DirectXTex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/CMake")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/CMake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/CMake")

add_library (${PROJECT_NAME} STATIC
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
    DirectXTex/DirectXTexD3D11.cpp
    DirectXTex/DirectXTexD3D12.cpp
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
    DirectXTex/DirectXTexWIC.cpp
)

source_group(${PROJECT_NAME} REGULAR_EXPRESSION DirectXTex/*.*)

target_include_directories(${PROJECT_NAME} PUBLIC DirectXTex)

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

    set(WarningsEXE "/wd4061" "/wd4062" "/wd4365" "/wd4668" "/wd4710" "/wd4820" "/wd5039" "/wd5045")
    target_compile_options(texassemble PRIVATE ${WarningsEXE})
    target_compile_options(texconv PRIVATE ${WarningsEXE})
    target_compile_options(texdiag PRIVATE ${WarningsEXE})
endif()

if(WIN32)
    # Windows 10 is used here to build the DirectX 12 code paths as well as 11
    target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0A00)
    target_compile_definitions(texassemble PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0A00)
    target_compile_definitions(texconv PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0A00)
    target_compile_definitions(texdiag PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0A00)
endif()
