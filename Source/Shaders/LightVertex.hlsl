#include "CommonDebug.hlsli"


cbuffer TransformCB : register(b0)
{
	float4x4 MatView;
	float4x4 MatProj;
	float4x4 MatInvProj;
};

cbuffer WorldTransformCB : register(b2)
{
  float4x4 MatTransNew[16];
};

void main(in VSIn In, out VSLightOut Out, uint lightId : SV_InstanceID)
{
	float4 viewposition = mul(MatTransNew[lightId], In.Position);
	
	//float4 position = mul(MatTrans, In.Position);
	float4 position = mul(MatView, viewposition);

	Out.Position = mul(MatProj, position);
	Out.Color = In.Color;
	Out.InstanceId = lightId;

}
