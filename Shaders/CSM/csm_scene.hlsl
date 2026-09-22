
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
    float4 PosH :       SV_POSITION;
    float3 NormalW :    NORMAL;
    float3 TangentW :   TANGENT;
    float2 TexC :       TEXCOORD;
    float4 TexShadow:   TEXCOORD1;
    float  Depth :      TEXCOORD2;
};


VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
    
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, m_mWorldViewProjection);
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.TangentW = mul(vin.TangentU, (float3x3) gWorld); //转到世界坐标的矩阵
    vout.TexC = vin.TexC;
    vout.Depth = mul(float4(vin.PosL, 1.0f), m_mWorldView).z; //主镜头坐标系下的深度
    vout.TexShadow = mul(posW, m_mShadow); //转到shadow space
    return vout;
}


void ComputeCoordinatesTransform(in int iCascadeIndex,
                                      in out float4 vShadowTexCoord,
                                      in out float4 vShadowTexCoordViewSpace)
{
    // Now that we know the correct map, we can transform the world space position of the current fragment                
    if (SELECT_CASCADE_BY_INTERVAL_FLAG)
    {
        vShadowTexCoord = vShadowTexCoordViewSpace * m_vCascadeScale[iCascadeIndex];
        vShadowTexCoord += m_vCascadeOffset[iCascadeIndex];
    }
          
    vShadowTexCoord.x *= m_fShadowPartitionSize; // precomputed (float)iCascadeIndex / (float)CASCADE_CNT
    vShadowTexCoord.x += (m_fShadowPartitionSize * (float) iCascadeIndex);


}

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;
    float percentLit = gShadowMap.SampleCmpLevelZero(gsamShadow,
                                                    shadowPosH.xy, depth).r;
    return percentLit;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 vDiffuse = Texture.Sample(gsamAnisotropicWrap, pin.TexC);
    
    
    int iCurrentCascadeIndex = 0;
    
    float4 vCurrentPixelDepth = pin.Depth;
    float4 vShadowMapTextureCoordViewSpace = pin.TexShadow;
    float4 vShadowMapTextureCoord = 0.0f;

    //select cascade index
    if (SELECT_CASCADE_BY_INTERVAL_FLAG)
    {
        if (CASCADE_COUNT_FLAG > 1)
        {
            
            float4 fComparison = (vCurrentPixelDepth > m_fCascadeFrustumsEyeSpaceDepthsFloat[0]);
            float4 fComparison2 = (vCurrentPixelDepth > m_fCascadeFrustumsEyeSpaceDepthsFloat[1]);
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
    }
    else
    {
        
        if (CASCADE_COUNT_FLAG == 1)
        {
            vShadowMapTextureCoord = vShadowMapTextureCoordViewSpace * m_vCascadeScale[0];
            vShadowMapTextureCoord += m_vCascadeOffset[0];
        }
        
        int iCascadeFound = 0;
        if (CASCADE_COUNT_FLAG > 1)
        {
            
            for (int iCascadeIndex = 0; iCascadeIndex <  CASCADE_COUNT_FLAG
                && (iCascadeFound == 0); ++iCascadeIndex)
            {
                vShadowMapTextureCoord = vShadowMapTextureCoordViewSpace * m_vCascadeScale[iCascadeIndex];
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
    
    ComputeCoordinatesTransform(iCurrentCascadeIndex,
                                 vShadowMapTextureCoord,
                                 vShadowMapTextureCoordViewSpace);
    
    
    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = gShadowMap.SampleCmpLevelZero(gsamShadow, vShadowMapTextureCoord.xy, vShadowMapTextureCoord.z).r;
    
    float3 vLightDir1 = float3(-1.0f, 1.0f, -1.0f);
    float3 vLightDir2 = float3(1.0f, 1.0f, -1.0f);
    float3 vLightDir3 = float3(0.0f, -1.0f, 0.0f);
    float3 vLightDir4 = float3(1.0f, 1.0f, 1.0f);
   
    // Some ambient-like lighting.
    float fLighting = .0f;
                      //saturate(dot(vLightDir1, pin.NormalW)) * 0.05f +
                      //saturate(dot(vLightDir2, pin.NormalW)) * 0.05f +
                      //saturate(dot(vLightDir3, pin.NormalW)) * 0.05f +
                      //saturate(dot(vLightDir4, pin.NormalW)) * 0.05f;
    
    float vShadowLighting = fLighting * 0.5f;
    
    fLighting += saturate(dot(m_vLightDir, pin.NormalW));
    fLighting = lerp(vShadowLighting, fLighting, shadowFactor[0]);
    
    return fLighting * vDiffuse;
    
}