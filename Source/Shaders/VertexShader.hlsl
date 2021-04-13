#include "Common.hlsli"

cbuffer TransformCB : register(b0)
{
	float4x4 MatView;
	float4x4 MatProj;
	float4x4 MatInvProjView;
};

void main(in VSIn In, out VSOut Out)
{
	float4 position = mul(MatView, In.Position);
	Out.Position = mul(MatProj, position);
	Out.Tangent = In.Tangent;
	Out.Binormal = In.Binormal;
	Out.Normal = In.Normal;
	Out.UV = In.UV;
}