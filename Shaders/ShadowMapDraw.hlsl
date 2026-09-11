
/*
    1. we should not average depth values and use the Percentage closer filter(PCF),
       point filtering(MIN_MAG_MIP_POINT)
    2. bilinearly interpolate the shadow map result
    3. An observation is that PCF really only needs to be performed at the shadow edges.
    4. 因为需要多个sample 会很耗时,好在Direct3d11开始通过SampleCmpLevelZero方法来支持PCF:

    Texture2D gShadowMap : register(t1);
    SamplerComparisonState gsamShadow : register(s6);
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;
    // Depth in NDC space.
    float depth = shadowPosH.z;
    // Automatically does a 4-tap PCF.
    gShadowMap.SampleCmpLevelZero(gsamShadow,shadowPosH.xy, depth).r;
*/ 

#include "ShadowMapBase.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
};
struct VertexOut
{
    float4 PosH : SV_POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
    return vout;
}
// This is only used for alpha cut out geometry, so that shadows
// show up correctly. Geometry that does not need to sample a
// texture can use a NULL pixel shader for depth pass.
void PS(VertexOut pin)
{
}



