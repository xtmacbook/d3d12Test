
#include "csm_base.hlsl"

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
    float3 PosL :       POSITION;
    float3 NormalL :    NORMAL;
    float2 TexC :       TEXCOORD;
    float3 TangentU :   TANGENT;
};

struct VertexOut
{
    float4 PosH :                                           SV_POSITION;
    float3 NormalW :                                        NORMAL;
    float3 TangentW :                                       TANGENT;
    float2 TexC :                                           TEXCOORD;
    float4 posInShadowViewSpace:                            TEXCOORD1;
    float  DepthInMainCameraViewSpace :                     TEXCOORD2;
};


VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
    vout.PosH = mul(float4(vin.PosL, 1.0f), m_mWorldViewProjection);
    vout.NormalW = mul(vin.NormalL, (float3x3) m_mWorld);
    vout.TangentW = mul(vin.TangentU, (float3x3) m_mWorld); //转到世界坐标的矩阵
    vout.TexC = vin.TexC;
    vout.DepthInMainCameraViewSpace = mul(float4(vin.PosL, 1.0f), m_mWorldView).z; //主镜头坐标系下的深度
    vout.posInShadowViewSpace = mul(float4(vin.PosL, 1.0f), m_mShadow); //转到shadow space
    return vout;
}



float LinearToSRGB(float c) 
{
    return c <= 0.0031308f ? 12.92f * c
                           : 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
}


float4 PS(VertexOut pin) : SV_Target
{
    float4 vDiffuse = Texture.Sample(gsamLinearWrap, pin.TexC);
    float4 vCurrentPixelDepthInMainCameraViewSpace = pin.DepthInMainCameraViewSpace;
    
    float4 vShadowMapTextureCoord = 0.0f;
    int iCurrentCascadeIndex = 0;

    /*
    这里的CASCADE_COUNT_FLAG: 就是目前使用多少个cascade
    */

    if (SELECT_CASCADE_BY_INTERVAL_FLAG) //选择cascade的方式
    {
        if (CASCADE_COUNT_FLAG > 1)
        {
            
            float4 fComparison = (vCurrentPixelDepthInMainCameraViewSpace > m_fCascadeFrustumsEyeSpaceDepthsFloat[0]);
            float4 fComparison2 = (vCurrentPixelDepthInMainCameraViewSpace > m_fCascadeFrustumsEyeSpaceDepthsFloat[1]);
            float fIndex = dot(
                            float4(CASCADE_COUNT_FLAG > 0,
                                    CASCADE_COUNT_FLAG > 1,
                                    CASCADE_COUNT_FLAG > 2,
                                    CASCADE_COUNT_FLAG > 3)
                            , fComparison)
                         + dot(
                            float4(
                                    CASCADE_COUNT_FLAG > 4,
                                    CASCADE_COUNT_FLAG > 5,
                                    CASCADE_COUNT_FLAG > 6,
                                    CASCADE_COUNT_FLAG > 7)
                            , fComparison2);
                                    
            fIndex = min(fIndex, CASCADE_COUNT_FLAG - 1);
            iCurrentCascadeIndex = (int) fIndex;
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
            for (int iCascadeIndex = 0; iCascadeIndex <  CASCADE_COUNT_FLAG
                && (iCascadeFound == 0); ++iCascadeIndex)
            {
                vShadowMapTextureCoord = pin.posInShadowViewSpace * m_vCascadeScale[iCascadeIndex];
                vShadowMapTextureCoord += m_vCascadeOffset[iCascadeIndex];

                if (min(vShadowMapTextureCoord.x, vShadowMapTextureCoord.y) > m_fMinBorderPadding
                  && max(vShadowMapTextureCoord.x, vShadowMapTextureCoord.y) < m_fMaxBorderPadding)
                {
                    iCurrentCascadeIndex = iCascadeIndex;
                    iCascadeFound = 1;
                }
            }
            
        }
    }
    
    //因为是多个cascade拼接出来的一个大纹理 ,所以要转到真实的纹理位置
    vShadowMapTextureCoord.x *= m_fShadowPartitionSize; // precomputed (float)iCascadeIndex / (float)CASCADE_CNT
    vShadowMapTextureCoord.x += (m_fShadowPartitionSize * (float) iCurrentCascadeIndex);
    
    
    float shadowFactor = gShadowMap.SampleCmpLevelZero(gsamShadow, vShadowMapTextureCoord.xy, vShadowMapTextureCoord.z).r;
    
    float3 vLightDir1 = float3(-1.0f, 1.0f, -1.0f);
    float3 vLightDir2 = float3(1.0f, 1.0f, -1.0f);
    float3 vLightDir3 = float3(0.0f, -1.0f, 0.0f);
    float3 vLightDir4 = float3(1.0f, 1.0f, 1.0f);
   
   float gammaS = LinearToSRGB(0.05);//与官方的cascade shadow mapping 11不同,这里将参数进行了gamma处理.官方的rtv是rgb，这里是unorm,同时输入纹理也是unorm

    // Some ambient-like lighting.
    float fLighting = saturate(dot(vLightDir1, pin.NormalW)) * gammaS +
                      saturate(dot(vLightDir2, pin.NormalW)) * gammaS +
                      saturate(dot(vLightDir3, pin.NormalW)) * gammaS +
                      saturate(dot(vLightDir4, pin.NormalW)) * gammaS;
    
    float vShadowLighting = fLighting * LinearToSRGB(0.5);
    

    float cosLight = dot(m_vLightDir, pin.NormalW);
    fLighting += saturate(cosLight);
    
    fLighting = lerp(vShadowLighting, fLighting, shadowFactor);
    
    float4 color = fLighting * vDiffuse;
    color.w = 1.0;

    return color;
    
}