struct VSIn
{
	float4 Position : POSITION;
	float4 Color : Color;
};

struct VSOut
{
	float4 Position : SV_Position;
	float4 Color : Color;
};

struct VSLightOut
{
	 float4 Position : SV_Position;
	 float4 Color : Color;
	 nointerpolation uint InstanceId : SV_InstanceID; 
};
