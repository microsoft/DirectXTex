//--------------------------------------------------------------------------------------
// File: DirectXTexPNG.h
//
// DirectXTex Auxillary functions for using the PNG(http://www.libpng.org/pub/png/libpng.html) library
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#pragma once

#include "DirectXTex.h"


namespace DirectX
{
    HRESULT __cdecl GetMetadataFromPNGFile(
        _In_z_ const wchar_t* szFile,
        _Out_ TexMetadata& metadata);

    HRESULT __cdecl LoadFromPNGFile(
        _In_z_ const wchar_t* szFile,
        _Out_opt_ TexMetadata* metadata,
        _Out_ ScratchImage& image);

    HRESULT __cdecl SaveToPNGFile(
        _In_ const Image& image,
        _In_z_ const wchar_t* szFile);
}