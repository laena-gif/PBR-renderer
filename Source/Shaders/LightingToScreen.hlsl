#include "Common.hlsli"
Texture2D<float4> PixelColor : register(t0);

struct LightingOut
{

	float4 Color : SV_Target;

};

void main(in TriangleOutput In, out LightingOut Out)
{
		uint2 pixelId = In.Pos.xy;
		float4 color = PixelColor.Load(float3(pixelId, 0));
		Out.Color = color;
}