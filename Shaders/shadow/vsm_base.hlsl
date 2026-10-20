

Texture2D<float4> Texture : register(t0);
Texture2D<float3> NormalTexture : register(t1);
Texture2DArray    gShadowMap : register(t2);

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

    matrix m_mShadow;

    float4 m_vCascadeOffset[8];
    float4 m_vCascadeScale[8];

    int m_nCascadeLevels; // Number of Cascades
    int m_iVisualizeCascades;
    float m_fMinBorderPadding;
    float m_fMaxBorderPadding;

    float m_fCascadeBlendArea; // Amount to overlap when blending between cascades.
    float m_fTexelSize;        // Padding variables exist because CBs must be a multiple of 16 bytes.
    float m_fNativeTexelSizeInX;
    float4 m_fCascadeFrustumsEyeSpaceDepthsData[2]; // The values along Z that seperate the cascades.
    // This code creates an array based pointer that points towards the vectorized input data.
    // This is the only way to index arbitrary arrays of data.
    // If the array is used at run time, the compiler will generate code that uses logic to index the correct component.

    static float m_fCascadeFrustumsEyeSpaceDepths[8] = (float[8])m_fCascadeFrustumsEyeSpaceDepthsData;

    float3 m_vLightDir;
    float m_LBRAmount; // 用于light bleeding reduction
    // int4   m_Bias;
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseColor;
    float3 gEmissiveColor;
    float cObjectPad;
    float3 gSpecularColor;
    float gSpecularPower;
};
