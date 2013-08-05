//-------------------------------------------------------------------------------------
// BCDirectCompute.h
//  
// Direct3D 11 Compute Shader BC Compressor
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif

#include "scoped.h"

namespace DirectX
{

class GPUCompressBC
{
public:
    GPUCompressBC();

    HRESULT Initialize( _In_ ID3D11Device* pDevice );

    HRESULT Prepare( _In_ size_t width, _In_ size_t height, _In_ DXGI_FORMAT format, _In_ float alphaWeight = 1.f );

    HRESULT Compress( _In_ const Image& srcImage, _In_ const Image& destImage );

    DXGI_FORMAT GetSourceFormat() const { return m_srcformat; }

private:
    DXGI_FORMAT                             m_bcformat;
    DXGI_FORMAT                             m_srcformat;
    float                                   m_alphaWeight;
    size_t                                  m_width;
    size_t                                  m_height;

    ScopedObject<ID3D11Device>              m_device;
    ScopedObject<ID3D11DeviceContext>       m_context;

    ScopedObject<ID3D11Buffer>              m_err1;
    ScopedObject<ID3D11UnorderedAccessView> m_err1UAV;
    ScopedObject<ID3D11ShaderResourceView>  m_err1SRV;

    ScopedObject<ID3D11Buffer>              m_err2;
    ScopedObject<ID3D11UnorderedAccessView> m_err2UAV;
    ScopedObject<ID3D11ShaderResourceView>  m_err2SRV;

    ScopedObject<ID3D11Buffer>              m_output;
    ScopedObject<ID3D11Buffer>              m_outputCPU;
    ScopedObject<ID3D11UnorderedAccessView> m_outputUAV;
    ScopedObject<ID3D11Buffer>              m_constBuffer;
    
    // Compute shader library
    ScopedObject<ID3D11ComputeShader>       m_BC6H_tryModeG10CS;
    ScopedObject<ID3D11ComputeShader>       m_BC6H_tryModeLE10CS;
    ScopedObject<ID3D11ComputeShader>       m_BC6H_encodeBlockCS;

    ScopedObject<ID3D11ComputeShader>       m_BC7_tryMode456CS;
    ScopedObject<ID3D11ComputeShader>       m_BC7_tryMode137CS;
    ScopedObject<ID3D11ComputeShader>       m_BC7_tryMode02CS;
    ScopedObject<ID3D11ComputeShader>       m_BC7_encodeBlockCS;    
};

}; // namespace