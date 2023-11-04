//--------------------------------------------------------------------------------------
// File: DirectXTexJPEG.h
//
// DirectXTex Auxillary functions for using the JPEG(https://www.ijg.org) library
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#pragma once

#include "DirectXTex.h"


namespace DirectX
{
    HRESULT __cdecl GetMetadataFromJPEGFile(
        _In_z_ const wchar_t* szFile,
        _Out_ TexMetadata& metadata);

    HRESULT __cdecl LoadFromJPEGFile(
        _In_z_ const wchar_t* szFile,
        _Out_opt_ TexMetadata* metadata,
        _Out_ ScratchImage& image);

    HRESULT __cdecl SaveToJPEGFile(
        _In_ const Image& image,
        _In_z_ const wchar_t* szFile);
}