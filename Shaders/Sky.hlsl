
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"


cbuffer cbPass : register(b0)
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

    PBRLight gLights[MaxLights];
};

TextureCube gCubeMap : register(t0);

SamplerState gsamAnisotropicWrap : register(s0);

struct VertexIn
{
    float3 PosL     : POSITION;
    float3 NormalL  : NORMAL;
    float2 TexC     : TEXCOORD;
};
struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
    float4x4 scaleMat = {
    5000,0,0,0,
    0,5000,0,0,
    0,0,5000,0,
    0,0,0,1
    };

    VertexOut vout;
// Use local vertex position as cubemap lookup vector.
    vout.PosL = vin.PosL;
// Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), scaleMat);
// Always center sky about camera.
    posW.xyz += gEyePosW;
// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
    vout.PosH = mul(posW, gViewProj).xyww;
    return vout;
}
float4 PS(VertexOut pin) : SV_Target
{
    return gCubeMap.Sample(gsamAnisotropicWrap, pin.PosL);
}