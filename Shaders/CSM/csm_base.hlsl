


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
    
    //这个和下面的scale矩阵构成了到每个cascade转换的矩阵,因为这个矩阵的对称的，
    //所以下面缩放只是使用了对角线而不是全部矩阵
    //是从shadow space的view 转到project
    float4 m_vCascadeOffset[8];
    float4 m_vCascadeScale[8];
    
    // Number of Cascades
    int m_nCascadeLevels;
    
    int m_iVisualizeCascades; // 1 is to visualize the cascades in different colors. 0 is to just draw the scene
   
    int m_iPCFBlurForLoopStart; // For loop begin value. For a 5x5 Kernal this would be -2.
    int m_iPCFBlurForLoopEnd; // For loop end value. For a 5x5 kernel this would be 3.

    // For Map based selection scheme, this keeps the pixels inside of the the valid range.
    // When there is no boarder, these values are 0 and 1 respectivley.
    // The border padding values keep the pixel shader from reading the borders during PCF filtering.
    float m_fMinBorderPadding; //可能是调整了bufferisze 1/buffsize 
    float m_fMaxBorderPadding; //buffersize-1 /buffersize
    float m_fShadowBiasFromGUI; // A shadow map offset to deal with self shadow artifacts. (0.002f) 
                                           //These artifacts are aggravated by PCF.
    float m_fShadowPartitionSize; // 1 / nCascadeLevels
    float m_fCascadeBlendArea; // Amount to overlap when blending between cascades.(0.005)
    float m_fTexelSize; // 1 / 一直是调整前的 buffersize
    float m_fNativeTexelSizeInX; //texlSize / cascadeLevels
    
    float m_fPaddingForCB3; // Padding variables exist because CBs must be a multiple of 16 bytes.
    
    float4 m_fCascadeFrustumsEyeSpaceDepthsFloat[2]; // The values along Z that seperate the cascades.
    float4 m_fCascadeFrustumsEyeSpaceDepthsFloat4[8]; // the values along Z that separte the cascades.  
                                                          // Wastefully stored in float4 so they are array indexable. 
    float3 m_vLightDir;
    float m_fPaddingCB4;

};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseColor;
    float3 gEmissiveColor;
    float cObjectPad;
    float3 gSpecularColor;
    float gSpecularPower;
};
