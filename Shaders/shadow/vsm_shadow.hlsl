#include "vsm_base.hlsl"

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
    vout.PosH = mul(float4(vin.PosL, 1.0f), m_mWorldViewProjection);
    return vout;
}

float2 PS(VertexOut pin) : SV_Target
{
    float2 rt;
    rt.x = pin.PosH.z;
    rt.y = rt.x * rt.x;
    return rt;
}