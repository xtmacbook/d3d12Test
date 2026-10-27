//=============================================================================
// Performs a separable Guassian blur with a configurable max radius.
// Define USE_ARRAY to blur a Texture2DArray (each slice independently).
// The max blur radius can be customized by defining MAX_BLUR_RADIUS (default 5).
//=============================================================================

#ifndef MAX_BLUR_RADIUS
#define MAX_BLUR_RADIUS 5
#endif

cbuffer cbSettings : register(b0)
{
	// We cannot have an array entry in a constant buffer that gets mapped onto
	// root constants, so list each element.

	int gBlurRadius;

	// Support up to (2*MAX_BLUR_RADIUS + 1) blur weights.
	// Root constants cannot index a cbuffer array, so we list each weight
	// as a separate scalar to keep the layout flat/predictable.
	float w0;
	float w1;
	float w2;
	float w3;
	float w4;
	float w5;
	float w6;
	float w7;
	float w8;
	float w9;
	float w10;

	// Which array slice is being blurred (only used when USE_ARRAY is defined).
	int gSlice;
};

float W(in int idx)
{
	switch(idx)
	{
		case 0: return w0;
		case 1: return w1;
		case 2: return w2;
		case 3: return w3;
		case 4: return w4;
		case 5: return w5;
		case 6: return w6;
		case 7: return w7;
		case 8: return w8;
		case 9: return w9;
		case 10: return w10;
	}
	return 0.0f;
}

#if USE_ARRAY
Texture2DArray gInput            : register(t0);
RWTexture2DArray<float4> gOutput : register(u0);
#else
Texture2D gInput                 : register(t0);
RWTexture2D<float4> gOutput      : register(u0);
#endif

#define N 256
#define CacheSize (N + 2 * MAX_BLUR_RADIUS)
groupshared float4 gCache[CacheSize];

#if USE_ARRAY
int3 G(in int2 xy) { return int3(xy, gSlice); }
#else
int2 G(in int2 xy) { return xy; }
#endif

[numthreads(N, 1, 1)]
void HorzBlurCS(int3 groupThreadID : SV_GroupThreadID,
				int3 dispatchThreadID : SV_DispatchThreadID)
{
	//
	// Fill local thread storage to reduce bandwidth.  To blur
	// N pixels, we will need to load N + 2*BlurRadius pixels
	// due to the blur radius.
	//

	// This thread group runs N threads.  To get the extra 2*BlurRadius pixels,
	// have 2*BlurRadius threads sample an extra pixel.
	if(groupThreadID.x < gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int x = max(dispatchThreadID.x - gBlurRadius, 0);
		gCache[groupThreadID.x] = gInput[G(int2(x, dispatchThreadID.y))];
	}
	if(groupThreadID.x >= N - gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int x = min(dispatchThreadID.x + gBlurRadius, gInput.Length.x - 1);
		gCache[groupThreadID.x + 2 * gBlurRadius] = gInput[G(int2(x, dispatchThreadID.y))];
	}

	// Clamp out of bound samples that occur at image borders.
	gCache[groupThreadID.x + gBlurRadius] = gInput[G(min(dispatchThreadID.xy, int2(gInput.Length.xy - 1)))];

	// Wait for all threads to finish.
	GroupMemoryBarrierWithGroupSync();

	//
	// Now blur each pixel.
	//

	float4 blurColor = float4(0, 0, 0, 0);

	for(int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		int k = groupThreadID.x + gBlurRadius + i;
		blurColor += W(i + gBlurRadius) * gCache[k];
	}

	gOutput[G(dispatchThreadID.xy)] = blurColor;
}

[numthreads(1, N, 1)]
void VertBlurCS(int3 groupThreadID : SV_GroupThreadID,
				int3 dispatchThreadID : SV_DispatchThreadID)
{
	//
	// Fill local thread storage to reduce bandwidth.  To blur
	// N pixels, we will need to load N + 2*BlurRadius pixels
	// due to the blur radius.
	//

	// This thread group runs N threads.  To get the extra 2*BlurRadius pixels,
	// have 2*BlurRadius threads sample an extra pixel.
	if(groupThreadID.y < gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int y = max(dispatchThreadID.y - gBlurRadius, 0);
		gCache[groupThreadID.y] = gInput[G(int2(dispatchThreadID.x, y))];
	}
	if(groupThreadID.y >= N - gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int y = min(dispatchThreadID.y + gBlurRadius, gInput.Length.y - 1);
		gCache[groupThreadID.y + 2 * gBlurRadius] = gInput[G(int2(dispatchThreadID.x, y))];
	}

	// Clamp out of bound samples that occur at image borders.
	gCache[groupThreadID.y + gBlurRadius] = gInput[G(min(dispatchThreadID.xy, int2(gInput.Length.xy - 1)))];

	// Wait for all threads to finish.
	GroupMemoryBarrierWithGroupSync();

	//
	// Now blur each pixel.
	//

	float4 blurColor = float4(0, 0, 0, 0);

	for(int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		int k = groupThreadID.y + gBlurRadius + i;
		blurColor += W(i + gBlurRadius) * gCache[k];
	}

	gOutput[G(dispatchThreadID.xy)] = blurColor;
}
