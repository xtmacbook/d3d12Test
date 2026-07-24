 
// Defaults for number of lights.
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

struct MaterialData
{
    float4      DiffuseAlbedo;
    float3      FresnelR0;
    float       Roughness;
    float4x4    MatTransform;
    uint        DiffuseTextureMapIndex;
    uint        NormalMapIndex;
    uint        MatPad1;
    uint        MatPad2;
};


// An array of textures, which is only supported in shader model 5.1+.  Unlike Texture2DArray, the textures
// in this array can be different sizes and formats, making it more flexible than texture arrays.
Texture2D gShadowMap : register(t0);
TextureCube gCubeMap : register(t1);

Texture2D gTextureMaps[6] : register(t2);

//因为上面的texure array在space0中占用了t0,t1,..,t3.所以下面material使用space1
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);


SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5); 
SamplerComparisonState gsamShadow : register(s6);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
	float4x4 gTexTransform;
	uint gMaterialIndex;
	uint gObjPad0;
	uint gObjPad1;
	uint gObjPad2;
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
    float4x4 gShadowTransform;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
//Transforms a normal map sample to world space.
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
// Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;
// Build orthonormal basis.
    float3 N = unitNormalW;
    
    /*
        Note that there is the assumption that unitNormalW is normalized.
    在进行interpolation后,tangent vector和normal vector可能已经不是orthonormal,
    下面一句代码的作用就是保证T对于N来说是northonormal
    */
    
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);
// Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);
    return bumpedNormalW;
}
 
/*
void pfcFilter()
{

    static const float SMAP_SIZE = 2048.0f;
    static const float SMAP_DX = 1.0f / SMAP_SIZE;

    // Sample shadow map to get nearest depth to light.
    float s0 = gShadowMap.Sample(gShadowSam, projTexC.xy).r;

    float s1 = gShadowMap.Sample(gShadowSam, projTexC.xy + float2(SMAP_DX, 0)).r;

    float s2 = gShadowMap.Sample(gShadowSam, projTexC.xy + float2(0, SMAP_DX)).r;

    float s3 = gShadowMap.Sample(gShadowSam, projTexC.xy + float2(SMAP_DX, SMAP_DX)).r;

    // Is the pixel depth <= shadow map value?
    float result0 = depth <= s0;
    float result1 = depth <= s1;
    float result2 = depth <= s2;
    float result3 = depth <= s3;

    // 注意到这个并不是在阴影内或者不在阴影内这两种绝对的情况,而是可以部分在阴影内那种平滑过度(百分多少在阴影内)
    // 正常情况就是判断depth和采样的结果就完毕了,但是这里采用bilinearly interpolate进行返回小数(百分比)而不是简简单单的一个bool值

    // Transform to texel space.
    float2 texelPos = SMAP_SIZE * projTexC.xy;
    // Determine the interpolation amounts.
    float2 t = frac(texelPos);
    // Interpolate results.
    return lerp(lerp(result0, result1, t.x), lerp(result2, result3, t.x), t.y);
}

*/

// shoadowPosH in clip space
float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;
    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);
    // Texel size.
    float dx = 1.0f / (float) width;
    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
            float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
            float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
                                                    shadowPosH.xy + offsets[i], depth).r;
    }
    return percentLit / 9.0f;
}

//ddx 和 ddy 是估算相对于screen space x轴和y轴的偏导数,可以用来估算像素之间的变化
//目前暂且认为像素p周围的像素都在同一平面上,当然这个不一定正确
 
 //	当在场景中的变化,就可以映射到屏幕上的变化.  Well, when we build our PCF kernel, we offset our texture
//coordinates to sample neighboring values in the shadow map 
//If we move(deltaX, deltaY) units in screen space, then the light space depth moves 就可以知道
//deltaZ = ddx(shadowPosH.z) * deltaX + ddy(shadowPosH.z) * deltaY
