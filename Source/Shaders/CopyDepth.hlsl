#include "Common.hlsli"
Texture2D<float2> PixelColor : register(t0);


void main(in TriangleOutput In, out float depth : SV_Depth)
{
		uint2 pixelId = In.Pos.xy;
		depth = PixelColor.Load(float3(pixelId, 0)).y;
	
}