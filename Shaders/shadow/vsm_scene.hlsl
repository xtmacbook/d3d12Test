#include "vsm_base.hlsl"
#include "../util.hlsl"

#ifndef USE_WITH_CSM
#define USE_WITH_CSM 0
#endif

#ifndef CASCADE_COUNT_FLAG
#define CASCADE_COUNT_FLAG 3
#endif

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
    float4 posInShadowViewSpace : TEXCOORD1;
    float DepthInMainCameraViewSpace : TEXCOORD2;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
    vout.PosH = mul(float4(vin.PosL, 1.0f), m_mWorldViewProjection);
    vout.NormalW = mul(vin.NormalL, (float3x3)m_mWorld);
    vout.TangentW = mul(vin.TangentU, (float3x3)m_mWorld); // 转到世界坐标的矩阵
    vout.TexC = vin.TexC;
    vout.DepthInMainCameraViewSpace = mul(float4(vin.PosL, 1.0f), m_mWorldView).z; // 主镜头坐标系下的深度
    vout.posInShadowViewSpace = mul(float4(vin.PosL, 1.0f), m_mShadow);            // 转到shadow space
    return vout;
}

void CalculateVarianceShadow(in float4 vShadowTexCoord, in float4 vShadowMapTextureCoordViewSpace,  int iCascade,out float fPercentLit)
{
    fPercentLit = 0.0f;
    // This loop could be unrolled, and texture immediate offsets could be used if the kernel size were fixed.
    // This would be a performance improvment.

    float4 vShadowTexCoordDDX = ddx(vShadowMapTextureCoordViewSpace);
    float4 vShadowTexCoordDDY = ddy(vShadowMapTextureCoordViewSpace);

    vShadowTexCoordDDX *= m_vCascadeScale[iCascade];
    vShadowTexCoordDDY *= m_vCascadeScale[iCascade];

    float2 mapDepth = 0;

    // mapDepth = gShadowMap.Sample(gsamAnisotropicWrap, vShadowTexCoord.xy);
    mapDepth = gShadowMap.SampleGrad(gsamAnisotropicClamp, vShadowTexCoord.xyz,
                                     vShadowTexCoordDDX.xyz, vShadowTexCoordDDY.xyz);

    // The sample instruction uses gradients for some filters.
    // .z 是纹理数组层(级联索引), 深度保存在 .w 中用于 Chebyshev 比较
    fPercentLit = ChebyshevUpperBound(mapDepth, vShadowTexCoord.w, 0.000001f);
}

// Light bleeding reduction
float LBR(float p)
{
    // Lots of options here if we don't care about being an upper bound.
    // Use whatever falloff function works well for your scene.
    return linstep(m_LBRAmount, 1, p);
}

//--------------------------------------------------------------------------------------
// Calculate amount to blend between two cascades and the band where blending will occure.
//--------------------------------------------------------------------------------------
void CalculateBlendAmountForMap(in float4 vShadowMapTextureCoord,
                                in out float fCurrentPixelsBlendBandLocation,
                                out float fBlendBetweenCascadesAmount)
{
    // Calcaulte the blend band for the map based selection.
    float2 distanceToOne = float2(1.0f - vShadowMapTextureCoord.x, 1.0f - vShadowMapTextureCoord.y);
    fCurrentPixelsBlendBandLocation = min(vShadowMapTextureCoord.x, vShadowMapTextureCoord.y);
    float fCurrentPixelsBlendBandLocation2 = min(distanceToOne.x, distanceToOne.y);
    fCurrentPixelsBlendBandLocation =
        min(fCurrentPixelsBlendBandLocation, fCurrentPixelsBlendBandLocation2);
    fBlendBetweenCascadesAmount = fCurrentPixelsBlendBandLocation / m_fCascadeBlendArea;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 vDiffuse = Texture.Sample(gsamLinearWrap, pin.TexC);
    float fPercentLit = 0.0f;

    if (USE_WITH_CSM > 0)
    {
        float4 vShadowMapTextureCoord = 0.0f;
        int iCurrentCascadeIndex = 0;
        int iNextCascadeIndex = 1;

        int iCascadeFound = 0;
        for (int iCascadeIndex = 0; iCascadeIndex < CASCADE_COUNT_FLAG && (iCascadeFound == 0); ++iCascadeIndex)
        {
            vShadowMapTextureCoord = pin.posInShadowViewSpace * m_vCascadeScale[iCascadeIndex];
            vShadowMapTextureCoord += m_vCascadeOffset[iCascadeIndex];

            if (min(vShadowMapTextureCoord.x, vShadowMapTextureCoord.y) > m_fMinBorderPadding && max(vShadowMapTextureCoord.x, vShadowMapTextureCoord.y) < m_fMaxBorderPadding)
            {
                iCurrentCascadeIndex = iCascadeIndex;
                iCascadeFound = 1;
            }
        }

        float4 vShadowMapTextureCoordLocal = vShadowMapTextureCoord; // 临时保存,因为下面就紧接着要真正的纹理坐标系了

        vShadowMapTextureCoord.w = vShadowMapTextureCoord.z; // We put the z value in w so that we can index the texture array with Z.
        vShadowMapTextureCoord.z = iCurrentCascadeIndex; // 用选中的级联索引作为数组层

        CalculateVarianceShadow(vShadowMapTextureCoord, pin.posInShadowViewSpace, iCurrentCascadeIndex, fPercentLit);

        // blend between cascades
        //  Repeat text coord calculations for the next cascade.
        //  The next cascade index is used for blurring between maps.
        //  就是当前cascade和next cascade进行blend
        iNextCascadeIndex = min(CASCADE_COUNT_FLAG - 1, iCurrentCascadeIndex + 1);

        float fBlendBetweenCascadesAmount = 1.0f;
        float fCurrentPixelsBlendBandLocation = 1.0f;

        CalculateBlendAmountForMap(vShadowMapTextureCoordLocal,
                                   fCurrentPixelsBlendBandLocation, fBlendBetweenCascadesAmount);

        if (fCurrentPixelsBlendBandLocation < m_fCascadeBlendArea)
        {
            // Because the Z coordinate species the texture array,
            // the derivative will be 0 when there is no divergence

            float4 vShadowMapTextureCoord_blend =
                pin.posInShadowViewSpace * m_vCascadeScale[iNextCascadeIndex];
            vShadowMapTextureCoord_blend += m_vCascadeOffset[iNextCascadeIndex];

            vShadowMapTextureCoord_blend.w = vShadowMapTextureCoord_blend.z; // We put the z value in w so that we can index the texture array with Z.
            vShadowMapTextureCoord_blend.z = iNextCascadeIndex;

            float fDivergence = abs(ddy(vShadowMapTextureCoord_blend.z)) +
	                            abs(ddx(vShadowMapTextureCoord_blend.z));

            float fPercentLit_blend = .0f;

            CalculateVarianceShadow(vShadowMapTextureCoord_blend, pin.posInShadowViewSpace,
							iNextCascadeIndex, fPercentLit_blend);

            // blend 两个cascade的percent
            fPercentLit = lerp(fPercentLit_blend, fPercentLit, fBlendBetweenCascadesAmount);
        }
    }
    else
    {
        float4 vShadowMapTextureCoord = pin.posInShadowViewSpace * m_vCascadeScale[0] + m_vCascadeOffset[0];
        vShadowMapTextureCoord.w = vShadowMapTextureCoord.z; // 把深度存到 w
        vShadowMapTextureCoord.z = 0; // z 作为纹理数组层(级联0)
        CalculateVarianceShadow(vShadowMapTextureCoord, pin.posInShadowViewSpace, 0, fPercentLit);
        fPercentLit = LBR(fPercentLit);
        
       
    }
    
    float3 vLightDir1 = float3(-1.0f, 1.0f, -1.0f);
    float3 vLightDir2 = float3(1.0f, 1.0f, -1.0f);
    float3 vLightDir3 = float3(0.0f, -1.0f, 0.0f);
    float3 vLightDir4 = float3(1.0f, 1.0f, 1.0f);

    float gammaS = LinearToSRGB(0.03);

        // Some ambient-like lighting.
    float fLighting = saturate(dot(vLightDir1, pin.NormalW)) * gammaS +
                          saturate(dot(vLightDir2, pin.NormalW)) * gammaS +
                          saturate(dot(vLightDir3, pin.NormalW)) * gammaS +
                          saturate(dot(vLightDir4, pin.NormalW)) * gammaS;

    float vShadowLighting = fLighting * LinearToSRGB(0.5);

    float cosLight = dot(m_vLightDir, pin.NormalW);
    fLighting += saturate(cosLight);

    fLighting = lerp(vShadowLighting, fLighting, fPercentLit);

    float4 color = fLighting * vDiffuse;
    color.w = 1.0;

    return color;
}