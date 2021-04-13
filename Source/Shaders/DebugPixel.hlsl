#include "CommonDebug.hlsli"

void main(in VSOut In, out float4 Out : SV_Target0)
{
	Out = In.Color;
}