//--------------------------------------------------------------------------------------
// File: ScreenGrab.h
//
// Function for capturing a 2D texture and saving it to a file (aka a 'screenshot'
// when used on a Direct3D 11 Render Target).
//
// Note these functions are useful as a light-weight runtime screen grabber. For
// full-featured texture capture, DDS writer, and texture processing pipeline,
// see the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once

#include <d3d11_1.h>

#include <OCIdl.h>
#include <stdint.h>
#include <functional>


namespace DirectX
{
    HRESULT SaveDDSTextureToFile( _In_ ID3D11DeviceContext* pContext,
                                  _In_ ID3D11Resource* pSource,
                                  _In_z_ const wchar_t* fileName );

    HRESULT SaveWICTextureToFile( _In_ ID3D11DeviceContext* pContext,
                                  _In_ ID3D11Resource* pSource,
                                  _In_ REFGUID guidContainerFormat, 
                                  _In_z_ const wchar_t* fileName,
                                  _In_opt_ const GUID* targetFormat = nullptr,
                                  _In_opt_ std::function<void(IPropertyBag2*)> setCustomProps = nullptr );
}
