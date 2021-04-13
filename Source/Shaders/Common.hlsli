struct VSIn
{
	float4 Position : POSITION;
	float3 Tangent : TANGENT;
	float3 Binormal : BINORMAL;
	float3 Normal : NORMAL;
	float2 UV : UV;
};

struct VSOut
{
	float4 Position : SV_Position;
	float3 Tangent : TANGENT;
	float3 Binormal : BINORMAL;
	float3 Normal : NORMAL;
	float2 UV : UV;
};

struct TriangleOutput
{
	float4 Pos : SV_POSITION;
	float2 UV : UV;
	float4 color : COLOR;
};
