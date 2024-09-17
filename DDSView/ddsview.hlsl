//--------------------------------------------------------------------------------------
// File: ddsview.fx
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
Texture1D tx1D : register(t0);
Texture1DArray tx1DArray : register(t0);

Texture2D tx2D : register(t0);
Texture2DArray tx2DArray : register(t0);

Texture3D tx3D : register(t0);

SamplerState samLinear : register(s0);

cbuffer cbArrayControl  : register(b0)
{
    float Index;
};

//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float4 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Tex : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = input.Pos;
    output.Tex = input.Tex;
    return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS_1D(PS_INPUT input) : SV_Target
{
    return tx1D.Sample(samLinear, input.Tex.x);
}

float4 PS_1DArray(PS_INPUT input) : SV_Target
{
    return tx1DArray.Sample(samLinear, float2(input.Tex.x, Index));
}

float4 PS_2D(PS_INPUT input) : SV_Target
{
    return tx2D.Sample(samLinear, input.Tex.xy);
}

float4 PS_2DArray(PS_INPUT input) : SV_Target
{
    return tx2DArray.Sample(samLinear, float3(input.Tex.xy, Index));
}

float4 PS_3D(PS_INPUT input) : SV_Target
{
    int Width, Height, Depth;
    tx3D.GetDimensions(Width, Height, Depth);

    return tx3D.Sample(samLinear, float3(input.Tex.xy, Index / Depth));
}

float4 PS_Cube(PS_INPUT input) : SV_Target
{
    return tx2DArray.Sample(samLinear, float3(input.Tex.xy, input.Tex.z + (6*Index)));
}
