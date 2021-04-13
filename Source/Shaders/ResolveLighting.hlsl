#include "PbsHelpers.hlsli"
#include "TiledLightingCommon.hlsli"
#include "DebugHelpers.hlsli"
#include "DebugHelpers.hlsli"
#include "MathHelpers.hlsli"

Texture2D<float4> BaseColorAndRoughness : register(t0);
Texture2D<float4> NormalsAndMetalness : register(t1);
Texture2D<float> DepthBuffer : register(t2);
Texture2D<float> ShadowMap : register(t3);
Texture2D<float4> BRDFconvolution : register(t7);
TextureCube<float4> CubeMap : register(t4);
TextureCube<float4> DiffuseIBL : register(t5);
TextureCube<float4> SpecularIBL : register(t6);
SamplerComparisonState ShadowSampler : register(s0);
SamplerState CubeMapSampler : register(s1);


cbuffer TransformCB : register(b0)
{
	float4x4 MatInvProjView;
	float4x4 MatLightView;
	float4x4 MatLightProj;
	//float3   LightPos; // root constants became too big after adding padding (remember that float3 on C++ side should be aligned on float4 boundary)
	float3   CamPos; // that's why CamPos was getting the wrong data
};

 //ConstantBuffer<TiledLightingCB> lightingCB : register(b1);

float3 WorldToCubemap(float3 v)
{
	return v.xzy * float3(1.0, 1.0, 1.0);
}

float3 FresnelParam(float3 Albedo, float metalness, float cosa)
{
	float3 albedo;
	float3 F0;
	ComputeAlbedoRf0(Albedo, metalness, F0, albedo);
	return FresnelTermShlick(F0, cosa);
}

float4 WorldPosFromDepth(uint2 idx, float depth)
{

	float2 uv = (float2)idx / float2(2560.0, 1440.0) * 2.0 - 1.0;
	uv.y = -uv.y;
	float4 clipSpace = float4(uv.x, uv.y, depth, 1.0);
	float4 worldDenormalized = mul(MatInvProjView, clipSpace);
	return worldDenormalized / worldDenormalized.w;
}

float3 DiffuseIBLLight(TextureCube<float4>DiffuseIBL, float3 N, float cosb, float3 Albedo, float roughness)
{
	float3 albedo;
	float3 F0;
	float metalness = 0.1; //fixed by now
	ComputeAlbedoRf0(Albedo, metalness, F0, albedo);
	float3 ks = FresnelTermShlick(F0,cosb);
	float3 kd = 1.0 - ks;

	//float3 refractance = DiffuseTermBurley(roughness, cosa,cosb, 1);
	//N.yzx * (1,-1,1) - coordinate axes swap 

	float3 irradiance = DiffuseIBL.SampleLevel(CubeMapSampler, WorldToCubemap(N), 0.0f).xyz;
	//float3 diffuseAlbedo = kd * albedo;


	return irradiance * Albedo;
}

float GetLODbyRoughness(float roughness)
{
	uint MAX_LOD = 4;
	return 4*roughness;
}


struct Light
{
	float3 Pos;
	float Radius;
	uint Color;
};

float3 RGBToFloat3(uint c)
{
	float3 result;
	result.x = (c & 0xFF0000) >> 16;
	result.y = (c & 0x00FF00) >> 8;
	result.z = (c & 0x0000FF);

	return result / 255.0;
}
/*
#define MAX_LIGHTS 16
cbuffer LightsCB : register(b1)
{
	Light light[MAX_LIGHTS];
};
*/

RWTexture2D<float4> Result : register(u0);
#define NUM_LIGHTS 16 // should be the same as in PointLight.h
StructuredBuffer<Light> PointLights : register(t34);
StructuredBuffer<uint> LightingTilesBuffer : register(t69);

#define POISSON_DISK_SIZE				12

// https://github.com/bartwronski/PoissonSamplingGenerator
static const float2 PoissonDisk[POISSON_DISK_SIZE] =
{
	float2(0.0513813f,	 0.9426029f),
	float2(-0.6145764f,	 0.5032808f),
	float2(0.7889470f,	 0.5969112f),
	float2(0.3471859f,	 0.3151721f),
	float2(-0.6949928f,	 0.0541867f),
	float2(0.6672577f,	 0.0565204f),
	float2(0.1700522f,	-0.0035358f),
	float2(-0.1390025f,	-0.3784182f),
	float2(-0.8251187f,	-0.5110231f),
	float2(0.4386618f,	-0.7301227f),
	float2(-0.4512824f,	-0.8362952f),
	float2(0.0361475f,	-0.8477381f),
};

float3 GammaToLinear(float3 color)
{ 
return color * color;
}

float GetShadowFactor(float4 worldPos)
{
	float4 shadowPos = mul(MatLightView, worldPos);
	shadowPos = mul(MatLightProj, shadowPos);
	shadowPos /= shadowPos.w;
	// OK

	float2 shadowUV = shadowPos.xy * 0.5 + 0.5;
	shadowUV.y = 1.0 - shadowUV.y;


	float shadow = 0.0;
	const float filterWidth = 1.0 / 256.0;

	if (all(shadowUV >= 0.0 && shadowUV <= 1.0))
	{
		float shadowBias = 0.001;
		for (int i = 0; i < POISSON_DISK_SIZE; ++i)
		{
			shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, shadowUV + PoissonDisk[i] * filterWidth, shadowPos.z - shadowBias).r;
		}
	}
	else
	{
		return 0.0;
	}

	return 1.0 - (shadow / POISSON_DISK_SIZE);
}

#define DEBUG_LIGHTS_COUNT 0
#define TILED_LIGHTING 1

[numthreads(8, 8, 1)]
void main(uint3 threadId : SV_DispatchThreadId)
{	


	uint2 pixelId = threadId.xy;
	int3 sampleIdx = int3(pixelId, 0);
	float depth = DepthBuffer.Load(sampleIdx).r;
	float4 Pos = WorldPosFromDepth(pixelId, depth);
	

	float3 V1 = normalize(Pos.xyz - CamPos.xyz); // we're inside of the cube map
	float4 colorfrommap = CubeMap.SampleLevel(CubeMapSampler, WorldToCubemap(V1), 0.0f);


	

	uint2 tileId = pixelId / TILED_LIGHTING_TILE_SIZE;
	uint flattenedIdx = tileId.y * 160 + tileId.x;
	uint numLightsIdx = flattenedIdx * 17; //numlights + 1
	uint numLightsInTile = LightingTilesBuffer[numLightsIdx];
	uint2 pixelInTile = pixelId - tileId * TILED_LIGHTING_TILE_SIZE;

	#if DEBUG_LIGHTS_COUNT

	bool isDigit = false;
	if (numLightsInTile < 10)
	{
		isDigit = SampleDigit(numLightsInTile, pixelInTile / 2);
	}
	else
	{
		if (pixelInTile.x < 8)
		{
			isDigit = SampleDigit((numLightsInTile / 10) % 10, pixelInTile / 2);
		}
		else
		{
			isDigit = SampleDigit(numLightsInTile % 10, (pixelInTile - uint2(8, 0)) / 2);
		}
		
	}

	//color
	
	//float4 debugColor = isDigit ? 1.0 : lerp(float4(0.0, 1.0, 0.0, 1.0), float4(1.0, 0.0, 0.0, 1.0), numLightsInTile * 2/ 25.0);
	float3 debugColor = DebugColor(numLightsInTile);
	if (depth == 1.0)
	{
		Result[pixelId] = float4(debugColor,0.0f) * 0.5;
		return;
	}
	#else
	if (depth == 1.0)
	{
		Result[pixelId] =  colorfrommap;
		return;
	}
	#endif

	float3 baseColor = BaseColorAndRoughness.Load(sampleIdx).rgb;
	float roughness  = BaseColorAndRoughness.Load(sampleIdx).a;

	float3 N = NormalsAndMetalness.Load(sampleIdx).rgb;
	float3 V = normalize(CamPos.xyz - Pos.xyz);

	float3 shadedColor = 0.0;

	#if TILED_LIGHTING
	for (uint i = 0; i < numLightsInTile; ++i)
	{
		uint currLight = LightingTilesBuffer[numLightsIdx + i + 1];
		Light light = PointLights[currLight];

		float3 WorldLightVec = light.Pos - Pos.xyz;
		float DistanceSqr = dot(WorldLightVec, WorldLightVec);

		float3 L = normalize(light.Pos - Pos.xyz);
		float attenuation = 1.0 / (DistanceSqr + 1.0);

		attenuation = step(DistanceSqr, light.Radius * light.Radius); // hacky hack

		shadedColor += Brdf(roughness, 0.1, baseColor, N, V, L) * attenuation; //* RGBToFloat3(light.Color);
	}
	#else
	uint numLights = 0;
	for (uint i = 0; i < NUM_LIGHTS; ++i)
	{
		Light light = PointLights[i];

		float3 WorldLightVec = light.Pos - Pos.xyz;
		float DistanceSqr = dot(WorldLightVec, WorldLightVec);
		if (DistanceSqr < light.Radius * light.Radius)
		{
			numLights+=1;
		}

		float3 L = normalize(light.Pos - Pos.xyz);
		float attenuation = 1.0 / (DistanceSqr + 1.0);

		attenuation = step(DistanceSqr, light.Radius * light.Radius); // hacky hack

		shadedColor += Brdf(roughness, 0.1, baseColor, N, V, L) * attenuation;// *RGBToFloat3(light.Color);
	}
	debugColor = DebugColor(numLights);
	#endif


	float3 LightPos = float3(10.0, 10.0, 10.0);
	float3 L = normalize(LightPos - Pos.xyz);

	//Result[pixelId] = float4(N * 0.5 + 0.5, 1.0);

	float3 shadedColorBigLight = Brdf(roughness, 0.1, baseColor, N, V, L);
	shadedColorBigLight *= GetShadowFactor(Pos);

	//environment light
	float NoV = saturate(dot(N, V));
	float3 diffuse = DiffuseIBLLight(DiffuseIBL, N, NoV, baseColor,roughness) * 0.8;

	float3 reflectV = reflect(-V, N);
	float lod = GetLODbyRoughness(roughness);
	float3 preFilteredColor = SpecularIBL.SampleLevel(CubeMapSampler, WorldToCubemap(reflectV), lod).rgb;

	float2  BRDFvec = float2(max(NoV, 0.0), roughness);
	float2 BRDFIBL = BRDFconvolution.SampleLevel(CubeMapSampler, BRDFvec, 0).rg;
	float3 F = FresnelParam(baseColor, 0.3, NoV);
	float3 specular = preFilteredColor * (F * BRDFIBL.r + BRDFIBL.g);

	float4 finalColor = 0.0;
	#if DEBUG_LIGHTS_COUNT	
	finalColor = lerp(float4(shadedColor + shadedColorBigLight, 1.0), float4(debugColor,0.0f), 0.5);
	#else
	//Result[pixelId] = float4(shadedColor + shadedColorBigLight + diffuse, 1.0);
	finalColor = float4(diffuse + specular, 1.0);
	#endif

	float ZhdanFloorAOCoefficientSquareRoot = dot(N, float3(0.0, 0.0, 1.0)) * 0.5 + 0.5;
	finalColor *= ZhdanFloorAOCoefficientSquareRoot * ZhdanFloorAOCoefficientSquareRoot;

	Result[pixelId] = finalColor/ (finalColor + 1.0); // Reinhard tone mapping: https://64.github.io/tonemapping/
	
}