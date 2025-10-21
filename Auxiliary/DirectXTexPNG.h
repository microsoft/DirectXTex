//--------------------------------------------------------------------------------------
// File: DirectXTexPNG.h
//
// DirectXTex Auxilary functions for using the PNG(http://www.libpng.org/pub/png/libpng.html) library
//
// For the Windows platform, the strong recommendation is to make use of the WIC
// functions rather than using the open source library. This module exists to support
// Windows Subsystem on Linux.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#pragma once

#include "DirectXTex.h"


namespace DirectX
{
    enum PNG_FLAGS : uint32_t
    {
        PNG_FLAGS_NONE = 0x0,

        PNG_FLAGS_BGR = 0x1,
        // 24bpp files are returned as BGRX; 32bpp files are returned as BGRA

        PNG_FLAGS_IGNORE_SRGB = 0x2,
        // Ignores sRGB rendering intent

        PNG_FLAGS_DEFAULT_LINEAR = 0x4,
        // If no gamma or intent is specified assume linear

        PNG_FLAGS_FORCE_SRGB = 0x20,
        // Writes sRGB metadata into the file reguardless of format

        PNG_FLAGS_FORCE_LINEAR = 0x40,
        // Writes linear gamma metadata into the file reguardless of format
    };

    DIRECTX_TEX_API HRESULT __cdecl GetMetadataFromPNGFile(
        _In_z_ const wchar_t* szFile,
        PNG_FLAGS flags,
        _Out_ TexMetadata& metadata);

    DIRECTX_TEX_API HRESULT __cdecl LoadFromPNGFile(
        _In_z_ const wchar_t* szFile,
        PNG_FLAGS flags,
        _Out_opt_ TexMetadata* metadata,
        _Out_ ScratchImage& image);

    DIRECTX_TEX_API HRESULT __cdecl SaveToPNGFile(
        _In_ const Image& image,
        PNG_FLAGS flags,
        _In_z_ const wchar_t* szFile);

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#endif

DEFINE_ENUM_FLAG_OPERATORS(PNG_FLAGS);

#ifdef __clang__
#pragma clang diagnostic pop
#endif
}
