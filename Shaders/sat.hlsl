
// test.hlsl -- SAT (summed area table) via DirectCompute
//
// Two axes, each a 3-stage hierarchical scan:
//   stage1  per-tile inclusive scan, emit per-tile sums
//   stage2  exclusive scan of tile sums
//   stage3  add tile offsets back
//
// Dispatch (GROUP_SIZE = 1024):
//   RowStage1 : (g_TilesPerRow, g_Height)
//   RowStage2 : (1, g_Height)
//   RowStage3 : (g_TilesPerRow, g_Height)
//   ColStage1 : (g_Width, g_TilesPerCol)
//   ColStage2 : (g_Width, 1)
//   ColStage3 : (g_Width, g_TilesPerCol)

#define GROUP_SIZE 1024

cbuffer cb : register(b0)
{
    uint g_Width;         // row length
    uint g_Height;        // column count
    uint g_TilesPerRow;   // ceil(g_Width / GROUP_SIZE)
    uint g_TilesPerCol;   // ceil(g_Height / GROUP_SIZE)

    int gSlice;
};

#if USE_ARRAY
Texture2DArray<float2>   g_Input      : register(t0);// input moments
RWTexture2DArray<float2> g_SAT        : register(u0); // final summed area table
#else
Texture2D<float2> gInput         : register(t0);
RWTexture2D<float2> gOutput      : register(u0);
#endif

RWStructuredBuffer<float2> g_RowBuf;     // row-scanned
RWStructuredBuffer<float2> g_BlockSum;   // per-tile sums
RWStructuredBuffer<float2> g_BlockOff;   // exclusive-scanned tile offsets

#if USE_ARRAY
int3 Tx(in int2 xy) { return int3(xy, gSlice); }
int FI(in int2 xy) {return gSlice * g_Width * g_Height + xy.y * g_Width + xy.x ;}
int RI(in int2 xy) {return gSlice * (g_TilesPerRow * g_Height) + xy.y * g_TilesPerRow + xy.x ;}
int CT(in int2 xy) {return gSlice * (g_Width * g_TilesPerCol) + xy.x * g_TilesPerCol + xy.y ;}
#else
int2 Tx(in int2 xy) { return xy; }
int FI(in int2 xy) {return xy.y * g_Width + xy.x;}
int RI(in int2 xy) {return xy.y * g_TilesPerRow + xy.x;}
int CT(in int2 xy) {return xy.x * g_TilesPerCol + xy.y;}
#endif



groupshared float2 s_Data[GROUP_SIZE];

// ---- Row: horizontal prefix sums ----
//dispatch(g_TilesPerRow,g_Height)
[numthreads(GROUP_SIZE, 1, 1)] //groupID是组的ID,SV_GroupThreadID是组内特定线程相对与该组的id
void RowStage1(uint3 GT : SV_GroupThreadID, uint3 G : SV_GroupID)
{
    uint row = G.y;
    uint tile = G.x;
    uint start = tile * GROUP_SIZE + GT.x;

    float2 v = (start < g_Width) ? g_Input[Tx(int2(start,row))] : 0;
    s_Data[GT.x] = v;
    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint d = 1; d < GROUP_SIZE; d <<= 1) {
        float2 t = (GT.x >= d) ? s_Data[GT.x - d] : 0;
        GroupMemoryBarrierWithGroupSync();
        if (GT.x >= d) s_Data[GT.x] += t;
        GroupMemoryBarrierWithGroupSync();
    }

    if (start < g_Width)
        g_RowBuf[ FI(int2(start,row))] = s_Data[GT.x];
    if (GT.x == GROUP_SIZE - 1)
        g_BlockSum[RI(int2(tile,row)) ] = s_Data[GT.x];
}

//dispatch(1,g_Height)
[numthreads(GROUP_SIZE, 1, 1)]
void RowStage2(uint3 GT : SV_GroupThreadID, uint3 G : SV_GroupID)
{
    uint row = G.y;
    uint tile = GT.x;
    float2 v = (tile < g_TilesPerRow) ? g_BlockSum[RI(int2(tile,row))] : 0;
    s_Data[GT.x] = v;
    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint d = 1; d < GROUP_SIZE; d <<= 1) {
        float2 t = (GT.x >= d) ? s_Data[GT.x - d] : 0;
        GroupMemoryBarrierWithGroupSync();
        if (GT.x >= d) s_Data[GT.x] += t;
        GroupMemoryBarrierWithGroupSync();
    }

    if (tile < g_TilesPerRow)
        g_BlockOff[TI(int2(tile,row))] = s_Data[GT.x] - v;
}

[numthreads(GROUP_SIZE, 1, 1)]
void RowStage3(uint3 GT : SV_GroupThreadID, uint3 G : SV_GroupID)
{
    uint row = G.y;
    uint tile = G.x;
    uint start = tile * GROUP_SIZE + GT.x;
    if (start < g_Width)
        g_SAT[Tx(int2(start,row))] =
            g_RowBuf[FI(int2(start,row))] + g_BlockOff[RI(int2(tile,row))];
}

// ---- Column: vertical prefix sums on g_SAT ----

[numthreads(GROUP_SIZE, 1, 1)]
void ColStage1(uint3 GT : SV_GroupThreadID, uint3 G : SV_GroupID)
{
    uint col = G.x;
    uint tile = G.y;
    uint start = tile * GROUP_SIZE + GT.x;

    float2 v = (start < g_Height) ? g_SAT[Tx(int2(col,start))] : 0;
    s_Data[GT.x] = v;
    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint d = 1; d < GROUP_SIZE; d <<= 1) {
        float2 t = (GT.x >= d) ? s_Data[GT.x - d] : 0;
        GroupMemoryBarrierWithGroupSync();
        if (GT.x >= d) s_Data[GT.x] += t;
        GroupMemoryBarrierWithGroupSync();
    }

    if (start < g_Height)
        g_RowBuf[FI(int2(col,start))] = s_Data[GT.x];
    if (GT.x == GROUP_SIZE - 1)
        g_BlockSum[CT(int2(col,tile))] = s_Data[GT.x];
}

[numthreads(GROUP_SIZE, 1, 1)]
void ColStage2(uint3 GT : SV_GroupThreadID, uint3 G : SV_GroupID)
{
    uint col = G.x;
    uint tile = GT.x;
    float2 v = (tile < g_TilesPerCol) ? g_BlockSum[CT(int2(col,tile))] : 0;
    s_Data[GT.x] = v;
    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint d = 1; d < GROUP_SIZE; d <<= 1) {
        float2 t = (GT.x >= d) ? s_Data[GT.x - d] : 0;
        GroupMemoryBarrierWithGroupSync();
        if (GT.x >= d) s_Data[GT.x] += t;
        GroupMemoryBarrierWithGroupSync();
    }

    if (tile < g_TilesPerCol)
        g_BlockOff[CT(int2(col,tile))] = s_Data[GT.x] - v;
}

[numthreads(GROUP_SIZE, 1, 1)]
void ColStage3(uint3 GT : SV_GroupThreadID, uint3 G : SV_GroupID)
{
    uint col = G.x;
    uint tile = G.y;
    uint start = tile * GROUP_SIZE + GT.x;
    if (start < g_Height)
        g_SAT[Tx(int2(col,start))] =
            g_RowBuf[FI(int2(col,start))] + g_BlockOff[CT(int2(col,tile))];
}



SamplerState sampSATPointState;
//--------------------------------------------------------------------------------------
// Point Sampling Functions:采样 SAT (sum area table) 获取局部区域的sum
//--------------------------------------------------------------------------------------
// Point sample using two corners in the SAT
// Coords.xy is upper left, Coords.zw is lower right.
// Inclusive on upper left, not on lower right.
// Returns the sum (not average) - mostly an internal function
float4 SampleSATSum(Texture2D Texture, float4 Coords, int2 CoordsOffset)
{
    // Sample four SAT corners
    float4 nn = Texture.SampleLevel(sampSATPointState, Coords.xy, 0, CoordsOffset);
    float4 np = Texture.SampleLevel(sampSATPointState, Coords.xw, 0, CoordsOffset);
    float4 pn = Texture.SampleLevel(sampSATPointState, Coords.zy, 0, CoordsOffset);
    float4 pp = Texture.SampleLevel(sampSATPointState, Coords.zw, 0, CoordsOffset);
    
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
float4 SampleSAT(Texture2D Texture, float4 Coords, float2 TexSize)
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
void SampleSATSumBilinear(Texture2D Texture, float4 Coords, float2 TexSize,
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