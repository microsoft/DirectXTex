//--------------------------------------------------------------------------------------
// File: Texenvmap.hlsl
//
// DirectX Texture environment map tool shaders
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

Texture2D<float4> Texture : register(t0);
sampler Sampler : register(s0);

cbuffer Parameters : register(b0)
{
    float4x4 Transform;
}

struct VSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 PositionPS : SV_Position;
    float2 TexCoord : TEXCOORD0;
    float3 LocalPos : TEXCOORD1;
};

// Vertex shader: basic.
VSOutput VSBasic(VSInput vin)
{
    VSOutput vout;

    vout.LocalPos = vin.Position.xyz;
    vout.PositionPS = mul(vin.Position, Transform);
    vout.TexCoord = vin.TexCoord;

    return vout;
}

// Pixel shader: basic
float4 PSBasic(VSOutput pin) : SV_Target0
{
    float3 color = Texture.Sample(Sampler, pin.TexCoord).rgb;
    return float4(color, 1.0);
}

// Pixel shader: Equirectangular projection to cubemap
float2 SphereMap(float3 vec)
{
    float2 uv = float2(atan2(vec.z, vec.x), asin(vec.y));
    uv *= float2(0.1591, 0.3183);
    uv += 0.5;
    return uv;
}

float4 PSEquiRect(VSOutput pin) : SV_Target0
{
    float2 uv = SphereMap(normalize(pin.LocalPos));

    float3 color = Texture.Sample(Sampler, uv).rgb;
    return float4(color, 1.0);
}
