
#include "Common.hlsl"

/*
    1. we should not average depth values and use the Percentage closer filter(PCF),point filtering(MIN_MAG_MIP_POINT)
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
struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};
struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};
VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
    MaterialData matData = gMaterialData[gMaterialIndex];
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);
    // Output vertex attributes for interpolation across triangle.
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, matData.MatTransform).xy;
    return vout;
}
// This is only used for alpha cut out geometry, so that shadows
// show up correctly. Geometry that does not need to sample a
// texture can use a NULL pixel shader for depth pass.
void PS(VertexOut pin)
{
    // Fetch the material data.
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    uint diffuseMapIndex = matData.DiffuseTextureMapIndex;
    // Dynamically look up the texture in the array.
    diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
#ifdef ALPHA_TEST
    // Discard pixel if texture alpha < 0.1. We do this test as soon
    // as possible in the shader so that we can potentially exit the
    // shader early, thereby skipping the rest of the shader code.
    clip(diffuseAlbedo.a - 0.1f);
#endif
}



