#include "CommonDebug.hlsli"
#include "TiledLightingCommon.hlsli"

RWStructuredBuffer<uint> LightingTilesBuffer : register(u0);
ConstantBuffer<TiledLightingCB> lightingCB : register(b1);
Texture2D<float2> MinMaxDepth : register(t0);

cbuffer TransformCB : register(b0)
{
	float4x4 MatView;
	float4x4 MatProj;
	float4x4 MatInvProj;
};

cbuffer PerLightPixelCB : register(b3)
{
  PerLightPixelCB lightParams[16];
};

[earlydepthstencil]
void main(in VSLightOut In, out float4 Out : SV_Target0)
{
	uint2 tileId = (int2)In.Position.xy;
	uint lightId = In.InstanceId;

	float minDepth = MinMaxDepth.Load(float3(tileId, 0)).x;
	float maxDepth = MinMaxDepth.Load(float3(tileId, 0)).x;

	float4x4 mat = MatInvProj;
	float viewVectorZ  = mat[2][3];
	float viewVectorW = mat[3][2] * minDepth + mat[3][3];
	float viewDepth = viewVectorZ /-viewVectorW;




	
	float4x4 matv =  MatView;
	float3 pos = lightParams[lightId].Position;
	float4 viewSphereCenter = mul(MatView, float4(lightParams[lightId].Position, 1.0f));
	float radius = lightParams[lightId].Radius;
	float4 sphereFarSide = viewSphereCenter - float4(0.0f, 0.0f, lightParams[lightId].Radius, 0.0f);
	float sphereFarSideZ = - sphereFarSide.z;

	uint flattenedIdx = tileId.y * lightingCB.NumTilesX + tileId.x;
	uint numlightsIdx = flattenedIdx * (lightingCB.NumLights + 1);
	uint index = 0;
	uint index1 = 0;
//	if (sphereFarSideZ > viewDepth)
	{

		InterlockedAdd(LightingTilesBuffer[numlightsIdx], 1, index);
		uint currLightIdx = numlightsIdx + index + 1;
		InterlockedAdd(LightingTilesBuffer[currLightIdx], lightId, index1);
		
	

	}

	Out = float4(0.0, 0.0, 1, 0.0);

}
