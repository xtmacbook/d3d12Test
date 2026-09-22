#include "csm_base.hlsl"

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
    VertexOut vout = (VertexOut) 0.0f;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, m_mWorldViewProjection);
    return vout;
}

float4 PS(VertexOut pin)
{
    float4 color = { 1.0, 0.0, 0.0, 1.0 };
    return color;
}

