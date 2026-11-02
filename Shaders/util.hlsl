//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
//Transforms a normal map sample to world space.
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
// Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;
// Build orthonormal basis.
    float3 N = unitNormalW;
    
    /*
        Note that there is the assumption that unitNormalW is normalized.
    在进行interpolation后,tangent vector和normal vector可能已经不是orthonormal,
    下面一句代码的作用就是保证T对于N来说是northonormal
    */
    
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);
// Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);
    return bumpedNormalW;
}
 
 float3 TwoChannelNormalX2(float2 normal)
{
    float2 xy = 2.0f * normal - 1.0f;
    float z = sqrt(1 - dot(xy, xy));
    return float3(xy.x, xy.y, z);
}


//--------------------------------------------------------------------------------------
// This function calculates the screen space depth for shadow space texels
//具体的说明可以看 ：Introduction to 3D Game Programming with DirectX 12 书中shadow那个章节
//--------------------------------------------------------------------------------------
void CalculateRightAndUpTexelDepthDeltas ( in float3 vShadowTexDDX,
                                           in float3 vShadowTexDDY,
                                           float2 fTexeSize,
                                           out float fUpTextDepthWeight,
                                           out float fRightTextDepthWeight) 
 
 {
        
    // We use the derivatives in X and Y to create a transformation matrix.  Because these derivives give us the 
    // transformation from screen space to shadow space, we need the inverse matrix to take us from shadow space 
    // to screen space.  This new matrix will allow us to map shadow map texels to screen space.  This will allow 
    // us to find the screen space depth of a corresponding depth pixel.
    // This is not a perfect solution as it assumes the underlying geometry of the scene is a plane.  A more 
    // accureate way of finding the actual depth would be to do a deferred rendering approach and actually 
    //sample the depth.
    
    // Using an offset, or using variance shadow maps is a better approach to reducing these artifacts in most cases.
    
    float2x2 matScreentoShadow = float2x2( vShadowTexDDX.xy, vShadowTexDDY.xy );
    float fDeterminant = determinant ( matScreentoShadow );
    
    float fInvDeterminant = 1.0f / fDeterminant;
    
    //书中说明创建的 shadow space(texture space)到screen space的转换矩阵
    float2x2 matShadowToScreen = float2x2 (
        matScreentoShadow._22 * fInvDeterminant, matScreentoShadow._12 * -fInvDeterminant, 
        matScreentoShadow._21 * -fInvDeterminant, matScreentoShadow._11 * fInvDeterminant );

    float2 vRightShadowTexelLocation = float2( fTexeSize.x, 0.0f );
    float2 vUpShadowTexelLocation = float2( 0.0f, fTexeSize.y );  
    
    // Transform the right pixel by the shadow space to screen space matrix.
    float2 vRightTexelDepthRatio = mul( vRightShadowTexelLocation,  matShadowToScreen );
    float2 vUpTexelDepthRatio = mul( vUpShadowTexelLocation,  matShadowToScreen );

    // We can now caculate how much depth changes when you move up or right in the shadow map.
    // We use the ratio of change in x and y times the dervivite in X and Y of the screen space 
    // depth to calculate this change.
    //书中最后就是屏幕(screen space)的deltaZ
    fUpTextDepthWeight = 
        vUpTexelDepthRatio.x * vShadowTexDDX.z 
        + vUpTexelDepthRatio.y * vShadowTexDDY.z;

    fRightTextDepthWeight = 
        vRightTexelDepthRatio.x * vShadowTexDDX.z 
        + vRightTexelDepthRatio.y * vShadowTexDDY.z;
        
}

float LinearToSRGB(float c)
{
    return c <= 0.0031308f ? 12.92f * c
                           : 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
}

///参考自:GPU Gems chapter:8
float ChebyshevUpperBound(float2 Moments, float Mean, float MinVariance)
{
    float p = (Mean <= Moments.x);

    //compute variance
    float Variance = Moments.y - (Moments.x * Moments.x);
    Variance = min(1.0f, max(Variance, MinVariance));

    float d     = Mean - Moments.x;
    float p_max = Variance / (Variance + d*d);
    
    p_max = max(p, p_max); 
    
    return p_max;
    //fPercentLit = pow( p_max, 4 );
}

// Compute the filter size in pixels
float2 GetFilterSize(float2 dx, float2 dy, float2 TexSize)
{
    return 2 * (abs(dx) + abs(dy)) * TexSize;
}


/*
    具体看: GPU Gems3 :chapter 8
    tex:  是纹理对象
    texels:是纹理元素

    比如一个纹理大小是: 1024 x 1024 那么它就是一个纹理(tex),其中有1024x1024个texls

    其中MinFilterWidth,MaxSizeDerivatives,TexSize都是texels为单位的。如果是1024x1024大小的纹理，
    则 参数TexSize就是1024x1024。

    其他的参数都是归一化后的值
    MaxDerivates参数 MaxSizeDerivatives为(32x32)

     这个函数获取两个值: Title的大小(Title就是Filter 区域范围组成的Title)
                        以tc为中心 Title大小的范围,这个范围左上角Texels的(中心)位置


    关于ddx的例子,注意不是这个函数的步骤只是一些额外的介绍：

    第 1 步：两个相邻屏幕像素
        取屏幕上水平相邻的两个像素，第 5 行、第 100 列和第 101 列。它们经过光照空间投影后：
        - 像素(100,5 对应的纹理坐标为 (0.65, 0.425)
        - 像素(101,5)：对应的纹理坐标为  (0.655, 0.425)
    第 2 步：求 dtdx 
        dtdx = 像素(101,5) - 像素(100,5) = (0.655 - 0.65, 0.425 - 0.425) = (0.005, 0.000)
        单位：纹理坐标 / 像素。屏幕 x 挪 1 像素 → 光照纹理坐标横移 0.005。
    第 3 步：换成实际 texel
        0.005 × 1024 = 5.12 texel。即：这个屏幕像素在阴影贴图上横跨约 5 texel（放大看是缩小，一处像素投到多点）。
    第 4 步：算滤波核大小（Util.fx:44）
        GetFilterSize = 2 × (|dtdx| + |dtdy|) × TexSize = 2 × (0.005 + 0) × 1024 ≈ 10.24 texel

*/

float2 GetFilterTile(float2 tc, float2 dx, float2 dy, float2 TexSize,
                     float2 MinFilterWidth, float2 MaxSizeDerivatives,
                     out float2 Size)
{
    float2 TexelSize = 1 / TexSize;

    // Compute the filter size based on derivatives
    float2 SizeDerivatives = min(GetFilterSize(dx, dy, TexSize),
                                 MaxSizeDerivatives);
    
    // Force an integer tile size (in pixels) so that bilinear weights are consistent
    Size = round(max(SizeDerivatives, MinFilterWidth)) * TexelSize;
    
    // Compute upper left corner of the tile
    return (tc - 0.5 * (Size - TexelSize));
}

float linstep(float min, float max, float v)
{
    return clamp((v - min) / (max - min), 0, 1);
}

float2 GetFPBias()
{
    return float2(0.5, 0);
}

// Returns coordinates for the four pixels surround a given fragment.
// Given and returned Coords are normalized
// These are given by (in Fetch4 order) - where "R" is the returned value:
//   - R + (1, 0)
//   - R + (0, 1)
//   - R + (1, 1)
//   - R
// Also returns bilinear weights in the output parameter.
float2 GetBilCoordsAndWeights(float2 Coords, float2 TexSize, out float4 Weights)
{
    float2 TexelSize = 1 / TexSize;
    float2 TexelCoords = Coords * TexSize;
    
    // Compute weights
    Weights.xy = frac(TexelCoords + 0.5);
    Weights.zw = 1 - Weights.xy;
    Weights = Weights.xzxz * Weights.wyyw;
    
    // Compute upper-left pixel coordinates
    // NOTE: D3D texel alignment...
    return (floor(TexelCoords - 0.5) + 0.5) * TexelSize;
}

float BilinearChebyshev(float4 Coords, float4 BilWeights, float Distance,
                        float MinVariance)
{
    float2 X, Y, Z, W;
    SampleMomentsBilinear(Coords, X, Y, Z, W);
    
    // Manual bilinear interpolation
    float2 I;
    I.x = dot(BilWeights, float4(X.x, Y.x, Z.x, W.x));
    I.y = dot(BilWeights, float4(X.y, Y.y, Z.y, W.y));
    
    return ChebyshevUpperBound(I, Distance, MinVariance);
}