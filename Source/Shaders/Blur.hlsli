Texture2D<float4> InputImage : register(t0);
RWTexture2D<float4> Result : register(u0);

#ifdef HORIZONTAL_BLUR
static float2 offset = float2(1.0, 0.0);
#else
static float2 offset = float2(0.0, 1.0);
#endif
#define  BLUR_RADIUS  3

static float weights[2*BLUR_RADIUS + 1] = {0.00598,	0.060626, 0.241843,	0.383103, 0.241843,	0.060626, 0.00598 };

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadId, uint3 groupThreadId : SV_GroupThreadID)
{
	int2 pixelId = dispatchThreadId.xy;


	int2 imageSize;
	InputImage.GetDimensions(imageSize.x, imageSize.y);

	float3 blurColor = float3(0.0, 0.0, 0.0);
	for (int i = -BLUR_RADIUS; i <= BLUR_RADIUS; ++i)
	{
		uint2 pixeloffset = i * offset;
		uint2 loadPos = clamp(pixelId + pixeloffset, 0, imageSize - 1);
		blurColor += InputImage[loadPos].rgb * weights[i + BLUR_RADIUS];
	}

	Result[pixelId] = float4(blurColor, 0);
}