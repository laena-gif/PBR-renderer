#include "Common.hlsli"

Texture2D<float4> TexBaseColor : register(t0);
Texture2D<float4> TexNormalMap : register(t1);
Texture2D<float4> TexRoughnessMap : register(t2);
SamplerState SampLinear : register(s0);

struct PSOut
{
	float4 BaseColorAndRoughness : SV_Target0;
	float4 NormalsAndMetalness : SV_Target1;
};

float4 Checkerboard(float2 uv, float4 color1, float4 color2)
{
	float scale = 10.0;
	float2 n = uv * scale;
	float s = fmod(floor(n.x) + floor(n.y), 2.0);
	return lerp(color1, color2, s);
}
void main(in VSOut In, out PSOut Out)
{
	// Out.BaseColorAndRoughness = Checkerboard(In.UV, float4(0.0, 0.0, 1.0, 1.0), float4(0.0, 0.0, 0.5, 1.0));

	// convert from Blender/Maya convention to DX one
	float2 uv = In.UV;
	uv.y = 1.0 - uv.y;

	Out.BaseColorAndRoughness = TexBaseColor.Sample(SampLinear, uv);

	float3x3 TBN = transpose(float3x3(In.Tangent, In.Binormal, In.Normal));
	float3 N = TexNormalMap.Sample(SampLinear, uv).rgb * 2.0 - 1.0;
	N = mul(TBN, N);

	Out.NormalsAndMetalness = float4(N, 0.5);
	Out.BaseColorAndRoughness.a = TexRoughnessMap.Sample(SampLinear, uv).r;
}