#include "Common.hlsli"

void main(uint id:SV_VERTEXID, out TriangleOutput Out)
{
	 TriangleOutput output;
	 Out.Pos.x = (float)(id / 2) * 4.0 - 1.0;
	 Out.Pos.y = (float)(id % 2) * 4.0 - 1.0;
	 Out.Pos.z = 0.0;
	 Out.Pos.w = 1.0;

	 Out.UV.x = (float)(id / 2) * 2.0;
	 Out.UV.y = 1.0 - (float)(id % 2) * 2.0;

	 Out.color = float4(1.0, 1.0, 1.0, 1.0);

}
