//--------------------------------------------------------------------------------------
// File: ScreenGrab12.h
//
// Function for capturing a 2D texture and saving it to a file (aka a 'screenshot'
// when used on a Direct3D 12 Render Target).
//
// Note these functions are useful as a light-weight runtime screen grabber. For
// full-featured texture capture, DDS writer, and texture processing pipeline,
// see the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#pragma once

#ifdef WIN32
#include <d3d12.h>

#ifdef NTDDI_WIN10_FE
#include <ocidl.h>
#else
#include <OCIdl.h>
#endif

#include <functional>
#else
#include <wsl/winadapter.h>
#include <wsl/wrladapter.h>
#include <directx/d3d12.h>
#endif


namespace DirectX
{
    HRESULT __cdecl SaveDDSTextureToFile(
        _In_ ID3D12CommandQueue* pCommandQueue,
        _In_ ID3D12Resource* pSource,
        _In_z_ const wchar_t* fileName,
        D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_RENDER_TARGET) noexcept;

#ifdef WIN32
    HRESULT __cdecl SaveWICTextureToFile(
        _In_ ID3D12CommandQueue* pCommandQ,
        _In_ ID3D12Resource* pSource,
        REFGUID guidContainerFormat,
        _In_z_ const wchar_t* fileName,
        D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_RENDER_TARGET,
        _In_opt_ const GUID* targetFormat = nullptr,
        _In_opt_ std::function<void __cdecl(IPropertyBag2*)> setCustomProps = nullptr,
        bool forceSRGB = false);
#endif
}
