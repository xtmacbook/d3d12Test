
#include "ShadowMapBase.hlsl"


struct VertexIn
{
    float3 PosL    :    POSITION;
    float3 NormalL :    NORMAL;
    float2 TexC    :    TEXCOORD;
    float3 TangentU :   TANGENT;
};

struct VertexOut
{
    float4 PosH    :    SV_POSITION;
    float4 ShadowPosH : POSITION0;
    float3 PosW    :    POSITION1;
    float3 NormalW :    NORMAL;
    float3 TangentW :   TANGENT;
    float2 TexC    :    TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);
    
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    vout.TangentW = mul(vin.TangentU, (float3x3) gWorld); //转到世界坐标的矩阵
    vout.TexC = vin.TexC;

    vout.ShadowPosH = mul(posW, gShadowTransform);
    return vout;
}

//因为当前该项目使用的是powerplant.sdkmesh,这个模型里的normal texture是无效的，所以不用normal texture的采样结果来计算bumpedNormalW,而是直接使用顶点着色器传过来的法线和切线来计算bumpedNormalW
float4 PS(VertexOut pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);
    pin.TangentW = normalize(pin.TangentW);

    //float3 normalMapSample = NormalTexture.Sample(gsamAnisotropicWrap, pin.TexC);
    //float3 localNoraml = TwoChannelNormalX2(normalMapSample.xy);
    //float3 bumpedNormalW = NormalSampleToWorldSpace(localNoraml, pin.NormalW, pin.TangentW);

    float3 toEyeW = gEyePosW - pin.PosW;
    float distToEye = length(toEyeW);
    toEyeW /= distToEye; // normalize


    float4 color = Texture.Sample(gsamAnisotropicWrap, pin.TexC);

    MaterialNoPBR mat = {gDiffuseColor,gEmissiveColor,gSpecularColor,gSpecularPower};

   //shadow 
    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);

    //ColorPair lightResult = ComputeLights(toEyeW, bumpedNormalW, gLights,mat, 3);
    ColorPair lightResult =  ComputeLights(toEyeW, pin.NormalW, gLights,mat, 3) ;

    color.rgb *= lightResult.Diffuse * shadowFactor[0];
    color.rgb += lightResult.Specular * color.a;

    return color;
}
