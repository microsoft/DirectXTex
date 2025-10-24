//--------------------------------------------------------------------------------------
// File: DirectXTexJPEG.h
//
// DirectXTex Auxilary functions for using the JPEG(https://www.ijg.org) library
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
    enum JPEG_FLAGS : uint32_t
    {
        JPEG_FLAGS_NONE = 0x0,

        JPEG_FLAGS_DEFAULT_LINEAR = 0x1,
        // Return non-SRGB formats intead of sRGB
    };

    DIRECTX_TEX_API HRESULT __cdecl GetMetadataFromJPEGFile(
        _In_z_ const wchar_t* szFile,
        JPEG_FLAGS flags,
        _Out_ TexMetadata& metadata);

    DIRECTX_TEX_API HRESULT __cdecl LoadFromJPEGFile(
        _In_z_ const wchar_t* szFile,
        JPEG_FLAGS flags,
        _Out_opt_ TexMetadata* metadata,
        _Out_ ScratchImage& image);

    DIRECTX_TEX_API HRESULT __cdecl SaveToJPEGFile(
        _In_ const Image& image,
        JPEG_FLAGS flags,
        _In_z_ const wchar_t* szFile);

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#endif

DEFINE_ENUM_FLAG_OPERATORS(JPEG_FLAGS);

#ifdef __clang__
#pragma clang diagnostic pop
#endif
}
