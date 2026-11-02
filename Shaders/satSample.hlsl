
SamplerState sampSATPointState;
//--------------------------------------------------------------------------------------
// Point Sampling Functions:采样 SAT (sum area table) 获取局部区域的sum
//--------------------------------------------------------------------------------------
// Point sample using two corners in the SAT
// Coords.xy is upper left, Coords.zw is lower right.
// Inclusive on upper left, not on lower right.
// Returns the sum (not average) - mostly an internal function
float4 SampleSATSum(Texture2DArray Texture, float4 Coords, float z,int2 CoordsOffset)
{
    // Sample four SAT corners
    float4 nn = Texture.SampleLevel(sampSATPointState, float3(Coords.xy, z), 0, CoordsOffset);
    float4 np = Texture.SampleLevel(sampSATPointState, float3(Coords.xw, z), 0, CoordsOffset);
    float4 pn = Texture.SampleLevel(sampSATPointState, float3(Coords.zy, z), 0, CoordsOffset);
    float4 pp = Texture.SampleLevel(sampSATPointState, float3(Coords.zw, z), 0, CoordsOffset);
    
    // Clamping
    // Disable this for now... only really necessary when we want exact reproduction
    // around the x|y == 0 lines. Otherwise just wastes some time.
    /*
    bool4 InBounds = (Coords >= 0);
    bool4 Mask = InBounds.xxzz && InBounds.ywyw;
    nn *= Mask.x;
    np *= Mask.y;
    pn *= Mask.z;
    pp *= Mask.w;
    */
    
    // Return the sum of the area within
    return (pp - pn - np + nn);
}

// As above, but average instead of sum
float4 SampleSAT(Texture2DArray Texture, float4 Coords, float2 TexSize)
{
    float2 TexelSize = 1 / TexSize;
    
    // Work out normalized coordinates and area
    float4 RealCoords = (Coords - TexelSize.xyxy);
    float2 Dims = (Coords.zw - Coords.xy) * TexSize;
    
    // Sample sum and divide to get average
    return SampleSATSum(Texture, RealCoords, int2(0, 0)) / (Dims.x * Dims.y);
}

//--------------------------------------------------------------------------------------
// Bilinear Sampling Functions
//--------------------------------------------------------------------------------------
void SampleSATSumBilinear(Texture2DArray Texture, float4 Coords, float2 TexSize,
                          out float4 X, out float4 Y, out float4 Z, out float4 W)
{
    float2 TexelSize = 1 / TexSize;
    float4 RealCoords = (Coords - TexelSize.xyxy);
       
    // Sample the four rectangles
    X = SampleSATSum(Texture, RealCoords, int2(1, 0));
    Y = SampleSATSum(Texture, RealCoords, int2(0, 1));
    Z = SampleSATSum(Texture, RealCoords, int2(1, 1));
    W = SampleSATSum(Texture, RealCoords, int2(0, 0));
}

// As above, but average instead of sum
void SampleSATBilinear(Texture2D Texture, float4 Coords, float2 TexSize,
                       out float4 X, out float4 Y, out float4 Z, out float4 W)
{
    SampleSATSumBilinear(Texture, Coords, TexSize, X, Y, Z, W);
    
    // Average
    float2 Dims = (Coords.zw - Coords.xy) * TexSize;
    float InvArea = 1 / float(Dims.x * Dims.y);
    X *= InvArea;
    Y *= InvArea;
    Z *= InvArea;
    W *= InvArea;
}


//--------------------------------------------------------------------------------------
// Summed area tables
// NOTE: Could generalize these to take a Texture2D if we have more than one SAT
//--------------------------------------------------------------------------------------
// Sample and return a single fragment
float2 SampleMomentsPoint(float4 Coords)
{
    float4 Value = SampleSAT(texSAT, Coords, g_ShadowTextureSize);
    
    float2 V;
    if (g_DistributePrecision) {
        V = RecombineFP(Value);
    } else {
        V = Value.xy;
    }

    // Unbias
    V += GetFPBias();
    return V;
}

// Returns into the X, Y, Z, W coordinates the four rectangle averages
void SampleMomentsBilinear(float4 Coords,
                           out float2 X, out float2 Y, out float2 Z, out float2 W)
{
    float4 X1, Y1, Z1, W1;
    SampleSATBilinear(texSAT, Coords, g_ShadowTextureSize, X1, Y1, Z1, W1);

    if (g_DistributePrecision) {
        X = RecombineFP(X1);
        Y = RecombineFP(Y1);
        Z = RecombineFP(Z1);
        W = RecombineFP(W1);
    } else {
        X = X1.xy;
        Y = Y1.xy;
        Z = Z1.xy;
        W = W1.xy;
    }
    
    // Unbias
    float2 FPBias = GetFPBias();
    X += FPBias;
    Y += FPBias;
    Z += FPBias;
    W += FPBias;
}