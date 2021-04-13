Texture2D<float> TextureToDownsample : register(t0);

RWTexture2D<float2> DownSampledTexture : register(u0);

static int2 offset = int2(4, 4);


#define S_MAX  0
#define S_MIN  1
#define S_NUM  2

groupshared uint shared_mem[S_NUM];

[numthreads(8, 4, 1)]
void main(uint3 threadId : SV_DispatchThreadId, uint groupIdx : SV_GroupIndex, uint3 groupId : SV_GroupID)
{

	float depthMin = 10000000;
	float depthMax = 0;
	uint startPixelIdx = 0;
	uint startFlatIdx = groupIdx * 8;
	uint2 pixelId = groupId.xy;
	if (groupIdx == 0)
	{
	    shared_mem[S_MAX] = 0;
	    shared_mem[S_MIN] = 0xffffffff;
	}

    
	float depthValue = 0;
	for (int i = 0; i < 8; ++i)
	{
		uint currentFlatIdx = startFlatIdx + i;  
		uint2 pixelIdinTile = uint2(currentFlatIdx%16, currentFlatIdx/16);
		uint2 pixelIdinTexture = 16 * pixelId + pixelIdinTile;
		depthValue = TextureToDownsample[pixelIdinTexture];
		depthMin = min(depthMin, depthValue);
		depthMax = max(depthMax, depthValue);
	}

    
    GroupMemoryBarrierWithGroupSync();
	InterlockedMin(shared_mem[S_MIN], asuint(depthMin));
	InterlockedMax(shared_mem[S_MAX], asuint(depthMax));

	GroupMemoryBarrierWithGroupSync();

	float fMinDepth = asfloat(shared_mem[S_MIN]);
	float fMaxDepth = asfloat(shared_mem[S_MAX]);

	DownSampledTexture[pixelId] = float2(fMinDepth,fMaxDepth);
}
