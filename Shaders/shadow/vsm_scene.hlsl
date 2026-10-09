#include "vsm_base.hlsl"

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

float4 PS(VertexOut pin) : SV_Target
{
    float4 color = Texture.Sample(gsamLinearClamp, pin.TexC);
    float3 lightDir = normalize(m_vLightDir);
    float NdotL = saturate(dot(pin.NormalW, -lightDir));
    float3 diffuse = gDiffuseColor.rgb * NdotL;
    float3 ambient = gEmissiveColor;
    float3 finalColor = diffuse + ambient;
    return float4(finalColor, 1.0f);
}