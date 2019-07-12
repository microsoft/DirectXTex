# DirectX Texture Library
#
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
#
# http://go.microsoft.com/fwlink/?LinkId=248926

cmake_minimum_required (VERSION 3.8)
project (DirectXTex_CMake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/CMake")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/CMake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/CMake")

set(WarningsLib "-Wall" "-Wpedantic" "-Wextra")
set(WarningsEXE ${WarningsLib} "-Wno-c++98-compat" "-Wno-c++98-compat-pedantic" "-Wno-switch" "-Wno-switch-enum" "-Wno-language-extension-token" "-Wno-missing-prototypes")

add_library (directxtex STATIC
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

target_include_directories( directxtex PUBLIC DirectXTex )

target_compile_options( directxtex PRIVATE /fp:fast )

add_executable(texassemble texassemble/texassemble.cpp)
target_link_libraries(texassemble directxtex)

add_executable(texconv texconv/texconv.cpp)
target_link_libraries(texconv directxtex)

add_executable(texdiag texdiag/texdiag.cpp)
target_link_libraries(texdiag directxtex)

if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    target_compile_options( directxtex PRIVATE ${WarningsLib} )
    target_compile_options( texassemble PRIVATE ${WarningsEXE}  )
    target_compile_options( texconv PRIVATE ${WarningsEXE}  )
    target_compile_options( texdiag PRIVATE ${WarningsEXE} "-Wno-double-promotion" )
    if (${CMAKE_SIZEOF_VOID_P} EQUAL "4")
        target_compile_options( directxtex PRIVATE /arch:SSE2 )
        target_compile_options( texassemble PRIVATE /arch:SSE2 )
        target_compile_options( texconv PRIVATE /arch:SSE2 )
        target_compile_options( texdiag PRIVATE /arch:SSE2 )
    endif()
endif()
if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
    target_compile_options( directxtex PRIVATE /Wall /permissive- /Zc:__cplusplus )
    target_compile_options( texassemble PRIVATE /Wall /permissive- /Zc:__cplusplus )
    target_compile_options( texconv PRIVATE /Wall /permissive- /Zc:__cplusplus )
    target_compile_options( texdiag PRIVATE /Wall /permissive- /Zc:__cplusplus )
endif()

# Windows 10 is used here to build the DirectX 12 code paths as well as 11
add_compile_definitions(_UNICODE UNICODE _WIN32_WINNT=0x0A00)
