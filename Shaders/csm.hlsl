#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D<float4> Texture : register(t0);
Texture2D<float3> NormalTexture : register(t1);

SamplerState gsamAnisotropicWrap  : register(s0);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gWorldViewProj;
};

// Constant data that varies per pass.
cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    
    float3 gEyePosW;
    float cbPerObjectPad1;
    
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;

    float gNearZ;
    float gFarZ;
    
    float gTotalTime;
    float gDeltaTime;
    
    float4 gAmbientLight;

    // Allow application to change fog parameters once per frame.
    // For example, we may only use fog for certain times of day.
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 cbPerObjectPad2;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2)
{
    float4  gDiffuseAlbedo; 
    float3  gEmissiveColor;             
    float3  gSpecularColor;            
    float   gSpecularPower;       
};

struct VertexIn
{
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC    : TEXCOORD;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

     
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
#ifdef ALPHA_TEST
#endif

    float4 litColor = float4(1.0f, 0.0f, 0.0f, 1.0f);

    return litColor;
}
