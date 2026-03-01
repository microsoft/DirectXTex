//--------------------------------------------------------------------------------------
// File: DirectXTexEXR.h
//
// DirectXTex Auxilary functions for using the OpenEXR library
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#pragma once

#include "DirectXTex.h"


namespace DirectX
{
    DIRECTX_TEX_API HRESULT __cdecl GetMetadataFromEXRFile(
        _In_z_ const wchar_t* szFile,
        _Out_ TexMetadata& metadata);

    DIRECTX_TEX_API HRESULT __cdecl LoadFromEXRMemory(
        _In_reads_bytes_(size) const uint8_t* pSource, _In_ size_t size,
        _Out_opt_ TexMetadata* metadata, _Out_ ScratchImage& image);

    DIRECTX_TEX_API HRESULT __cdecl LoadFromEXRFile(
        _In_z_ const wchar_t* szFile,
        _Out_opt_ TexMetadata* metadata, _Out_ ScratchImage& image);

    DIRECTX_TEX_API HRESULT __cdecl SaveToEXRFile(
        _In_ const Image& image,
        _In_z_ const wchar_t* szFile);
}
