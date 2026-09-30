
#include "csm_base.hlsl"
#include "../util.hlsl"

#ifndef USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG
#define USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG 0
#endif

// This flag enables the shadow to blend between cascades.  This is most useful when the
// the shadow maps are small and artifact can be seen between the various cascade layers.
#ifndef BLEND_BETWEEN_CASCADE_LAYERS_FLAG
#define BLEND_BETWEEN_CASCADE_LAYERS_FLAG 0
#endif

// There are two methods for selecting the proper cascade a fragment lies in.  Interval selection
// compares the depth of the fragment against the frustum's depth partition.
// Map based selection compares the texture coordinates against the acutal cascade maps.
// Map based selection gives better coverage.
// Interval based selection is easier to extend and understand.
#ifndef SELECT_CASCADE_BY_INTERVAL_FLAG
#define SELECT_CASCADE_BY_INTERVAL_FLAG 0
#endif

// The number of cascades
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

float LinearToSRGB(float c)
{
    return c <= 0.0031308f ? 12.92f * c
                           : 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
}

//--------------------------------------------------------------------------------------
// Use PCF to sample the depth map and return a percent lit value.
//--------------------------------------------------------------------------------------
void CalculatePCFPercentLit(in float4 vShadowTexCoord,
                            in float fRightTexelDepthDelta,
                            in float fUpTexelDepthDelta,
                            in float fBlurRowSize,
                            out float fPercentLit)
{
    fPercentLit = 0.0f;
    // This loop could be unrolled, and texture immediate offsets could be used if the kernel size were fixed.
    // This would be performance improvment.
    for (int x = m_iPCFBlurForLoopStart; x < m_iPCFBlurForLoopEnd; ++x)
    {
        for (int y = m_iPCFBlurForLoopStart; y < m_iPCFBlurForLoopEnd; ++y)
        {
            float depthcompare = vShadowTexCoord.z;
            // A very simple solution to the depth bias problems of PCF is to use an offset.
            // Unfortunately, too much offset can lead to Peter-panning (shadows near the base of object disappear )
            // Too little offset can lead to shadow acne ( objects that should not be in shadow are partially self shadowed ).
            depthcompare -= m_fShadowBiasFromGUI;
            if (USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
            {
                // Add in derivative computed depth scale based on the x and y pixel.
                depthcompare += fRightTexelDepthDelta * ((float)x) + fUpTexelDepthDelta * ((float)y);
            }
            // Compare the transformed pixel depth to the depth read from the map.
            fPercentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
                                                         float2(
                                                             vShadowTexCoord.x + (((float)x) * m_fNativeTexelSizeInX),
                                                             vShadowTexCoord.y + (((float)y) * m_fTexelSize)),
                                                         depthcompare);
        }
    }
    fPercentLit /= (float)fBlurRowSize;
}

void CalculateBlendAmountForInterval(in int iCurrentCascadeIndex,
                                     in out float fPixelDepth,
                                     in out float fCurrentPixelsBlendBandLocation,
                                     out float fBlendBetweenCascadesAmount)
{

    // We need to calculate the band of the current shadow map where it will fade into the next cascade.
    // We can then early out of the expensive PCF for loop.
    //
    float fBlendInterval = m_fCascadeFrustumsEyeSpaceDepthsFloat4[iCurrentCascadeIndex].x;

    int fBlendIntervalbelowIndex = min(0, iCurrentCascadeIndex - 1);
    fPixelDepth -= m_fCascadeFrustumsEyeSpaceDepthsFloat4[fBlendIntervalbelowIndex].x;
    fBlendInterval -= m_fCascadeFrustumsEyeSpaceDepthsFloat4[fBlendIntervalbelowIndex].x;

    // The current pixel's blend band location will be used to determine when we need to blend and by how much.
    fCurrentPixelsBlendBandLocation = fPixelDepth / fBlendInterval;
    fCurrentPixelsBlendBandLocation = 1.0f - fCurrentPixelsBlendBandLocation;
    // The fBlendBetweenCascadesAmount is our location in the blend band.
    fBlendBetweenCascadesAmount = fCurrentPixelsBlendBandLocation / m_fCascadeBlendArea;
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

    float4 vShadowMapTextureCoord = 0.0f;
    int iCurrentCascadeIndex = 0;
    int iNextCascadeIndex = 1;

    int iBlurRowSize = m_iPCFBlurForLoopEnd - m_iPCFBlurForLoopStart;
    iBlurRowSize *= iBlurRowSize;
    float fBlurRowSize = (float)iBlurRowSize;

    float fPercentLit = 0.0f;

    /*
    这里的CASCADE_COUNT_FLAG: 就是目前使用多少个cascade
    */

    if (SELECT_CASCADE_BY_INTERVAL_FLAG) // 选择cascade的方式
    {
        if (CASCADE_COUNT_FLAG > 1)
        {

            float4 fComparison = (pin.DepthInMainCameraViewSpace > m_fCascadeFrustumsEyeSpaceDepthsFloat[0]);
            float4 fComparison2 = (pin.DepthInMainCameraViewSpace > m_fCascadeFrustumsEyeSpaceDepthsFloat[1]);
            float fIndex = dot(
                               float4(CASCADE_COUNT_FLAG > 0,
                                      CASCADE_COUNT_FLAG > 1,
                                      CASCADE_COUNT_FLAG > 2,
                                      CASCADE_COUNT_FLAG > 3),
                               fComparison) +
                           dot(
                               float4(
                                   CASCADE_COUNT_FLAG > 4,
                                   CASCADE_COUNT_FLAG > 5,
                                   CASCADE_COUNT_FLAG > 6,
                                   CASCADE_COUNT_FLAG > 7),
                               fComparison2);

            fIndex = min(fIndex, CASCADE_COUNT_FLAG - 1);
            iCurrentCascadeIndex = (int)fIndex;
        }

        vShadowMapTextureCoord = pin.posInShadowViewSpace * m_vCascadeScale[iCurrentCascadeIndex];
        vShadowMapTextureCoord += m_vCascadeOffset[iCurrentCascadeIndex];
    }
    else // screne map sel
    {

        if (CASCADE_COUNT_FLAG == 1)
        {
            vShadowMapTextureCoord = pin.posInShadowViewSpace * m_vCascadeScale[0];
            vShadowMapTextureCoord += m_vCascadeOffset[0];
        }

        int iCascadeFound = 0;
        if (CASCADE_COUNT_FLAG > 1)
        {
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
        }
    }

    float fUpTextDepthWeight = 0;
    float fRightTextDepthWeight = 0;

    float3 vShadowMapTextureCoordDDX;
    float3 vShadowMapTextureCoordDDY;
    
    if (USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
    {
        vShadowMapTextureCoordDDX = ddx(pin.posInShadowViewSpace);
        vShadowMapTextureCoordDDY = ddy(pin.posInShadowViewSpace);

        vShadowMapTextureCoordDDX *= m_vCascadeScale[iCurrentCascadeIndex];
        vShadowMapTextureCoordDDY *= m_vCascadeScale[iCurrentCascadeIndex];

        // 主要是计算fUpTextDepthWeight和fRightTextDepthWeight
        CalculateRightAndUpTexelDepthDeltas(vShadowMapTextureCoordDDX, vShadowMapTextureCoordDDY,
                                            m_fTexelSize, fUpTextDepthWeight, fRightTextDepthWeight);
    }

    float4 vShadowMapTextureCoordLocal = vShadowMapTextureCoord; // 临时保存,因为下面就紧接着要真正的纹理坐标系了

    // 因为是多个cascade拼接出来的一个大纹理 ,所以要转到真实的纹理位置
    vShadowMapTextureCoord.x *= m_fShadowPartitionSize; // precomputed (float)iCascadeIndex / (float)CASCADE_CNT
    vShadowMapTextureCoord.x += (m_fShadowPartitionSize * (float)iCurrentCascadeIndex);

    // 计算percentLit : 1则全部是lit,0则全部是被遮罩的
    CalculatePCFPercentLit(vShadowMapTextureCoord, fRightTextDepthWeight,
                           fUpTextDepthWeight, fBlurRowSize, fPercentLit);

    if (BLEND_BETWEEN_CASCADE_LAYERS_FLAG)
    {
        // Repeat text coord calculations for the next cascade.
        // The next cascade index is used for blurring between maps.
        // 就是当前cascade和next cascade进行blend
        iNextCascadeIndex = min(CASCADE_COUNT_FLAG - 1, iCurrentCascadeIndex + 1);

        float fBlendBetweenCascadesAmount = 1.0f;
        float fCurrentPixelsBlendBandLocation = 1.0f;

        if (SELECT_CASCADE_BY_INTERVAL_FLAG)
        {
            if (BLEND_BETWEEN_CASCADE_LAYERS_FLAG && CASCADE_COUNT_FLAG > 1)
            {
                CalculateBlendAmountForInterval(iCurrentCascadeIndex, pin.DepthInMainCameraViewSpace,
                                                fCurrentPixelsBlendBandLocation, fBlendBetweenCascadesAmount);
            }
        }
        else
        {

            if (BLEND_BETWEEN_CASCADE_LAYERS_FLAG)
            {
                CalculateBlendAmountForMap(vShadowMapTextureCoordLocal,
                                           fCurrentPixelsBlendBandLocation, fBlendBetweenCascadesAmount);
            }
        }

        if (fCurrentPixelsBlendBandLocation < m_fCascadeBlendArea)
        {
            // 在next cascade中的坐标
            float4 vShadowMapTextureCoord_blend =
                pin.posInShadowViewSpace * m_vCascadeScale[iNextCascadeIndex];
            vShadowMapTextureCoord_blend += m_vCascadeOffset[iNextCascadeIndex];

            vShadowMapTextureCoord_blend.x *= m_fShadowPartitionSize; // precomputed (float)iCascadeIndex / (float)CASCADE_CNT
            vShadowMapTextureCoord_blend.x += (m_fShadowPartitionSize * (float)iNextCascadeIndex);

            // 计算在next cascade中的 percent
            float fPercentLit_blend = 0.0;
            CalculatePCFPercentLit(vShadowMapTextureCoord_blend, fUpTextDepthWeight, fBlurRowSize,
                                   fBlurRowSize, fPercentLit_blend);

            // blend 两个cascade的percent

            fPercentLit = lerp(fPercentLit_blend, fPercentLit, fBlendBetweenCascadesAmount);
        }
    }

    float3 vLightDir1 = float3(-1.0f, 1.0f, -1.0f);
    float3 vLightDir2 = float3(1.0f, 1.0f, -1.0f);
    float3 vLightDir3 = float3(0.0f, -1.0f, 0.0f);
    float3 vLightDir4 = float3(1.0f, 1.0f, 1.0f);

    float gammaS = LinearToSRGB(0.03); // 与官方的cascade shadow mapping 11不同,这里将参数进行了gamma处理.官方的rtv是rgb，这里是unorm,同时输入纹理也是unorm

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