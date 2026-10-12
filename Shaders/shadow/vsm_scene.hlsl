#include "vsm_base.hlsl"
#include "../util.hlsl"

struct VertexIn
{
    float3 PosL :    POSITION;
    float3 NormalL : NORMAL;
    float2 TexC :   TEXCOORD;
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


void CalculateVarianceShadow ( in float4 vShadowTexCoord,  out float fPercentLit ) 
{
    fPercentLit = 0.0f;
    // This loop could be unrolled, and texture immediate offsets could be used if the kernel size were fixed.
    // This would be a performance improvment.
	        
    float2 mapDepth = 0;

    mapDepth = gShadowMap.Sample(gsamAnisotropicWrap, vShadowTexCoord.xy);
    
    // The sample instruction uses gradients for some filters.
		        
    float  fAvgZ  = mapDepth.x; // Filtered z
    float  fAvgZ2 = mapDepth.y; // Filtered z-squared
    
    if ( vShadowTexCoord.z <= fAvgZ ) // We put the z value in w so that we can index the texture array with Z.
    {
        fPercentLit = 1;
	}
	else 
	{
	    float variance = ( fAvgZ2 ) - ( fAvgZ * fAvgZ );
        variance       = min( 1.0f, max( 0.0f, variance + 0.00001f ) );
    
        float mean     = fAvgZ;
        float d        = vShadowTexCoord.z - mean; // We put the z value in w so that we can index the texture array with Z.
        float p_max    = variance / ( variance + d*d );

        // To combat light-bleeding, experiment with raising p_max to some power
        // (Try values from 0.1 to 100.0, if you like.)
        fPercentLit = pow( p_max, 4 );
	    
	}
    
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 vDiffuse = Texture.Sample(gsamLinearWrap, pin.TexC);

    float4 vShadowMapTextureCoord = pin.posInShadowViewSpace;
    vShadowMapTextureCoord = pin.posInShadowViewSpace * m_vShadowTextureScale + m_vShadowTextureOffset;

    float fPercentLit = 1.0f;
    CalculateVarianceShadow ( vShadowMapTextureCoord, fPercentLit );

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