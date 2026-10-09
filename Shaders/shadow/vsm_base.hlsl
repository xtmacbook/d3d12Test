


Texture2D<float4> Texture :         register(t0);
Texture2D<float3> NormalTexture :   register(t1);
Texture2D gShadowMap :              register(t2);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbAllShadowData : register(b1)
{
    matrix m_mWorldViewProjection;
    matrix m_mWorld;
    matrix m_mWorldView;
    
    matrix m_mShadow; //是shadow的viewmat
    float3 m_vLightDir;
    float m_fPaddingCB4;

};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseColor;
    float3 gEmissiveColor;
    float  cObjectPad;
    float3 gSpecularColor;
    float  gSpecularPower;
};
