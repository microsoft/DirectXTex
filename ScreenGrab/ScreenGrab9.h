//--------------------------------------------------------------------------------------
// File: ScreenGrab9.h
//
// Function for saving 2D surface to a file (aka a 'screenshot'
// when used on a Direct3D 9's GetFrontBufferData).
//
// Note these functions are useful as a light-weight runtime screen grabber. For
// full-featured texture capture, DDS writer, and texture processing pipeline,
// see the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once

#ifndef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x900
#endif

#include <d3d9.h>

#if defined(NTDDI_WIN10_FE) || defined(__MINGW32__)
#include <ocidl.h>
#else
#include <OCIdl.h>
#endif

#include <functional>


namespace DirectX
{
    HRESULT __cdecl SaveDDSTextureToFile(
        _In_ LPDIRECT3DSURFACE9 pSource,
        _In_z_ const wchar_t* fileName) noexcept;

    HRESULT __cdecl SaveWICTextureToFile(
        _In_ LPDIRECT3DSURFACE9 pSource,
        _In_ REFGUID guidContainerFormat,
        _In_z_ const wchar_t* fileName,
        _In_opt_ const GUID* targetFormat = nullptr,
        _In_opt_ std::function<void __cdecl(IPropertyBag2*)> setCustomProps = nullptr);
}
