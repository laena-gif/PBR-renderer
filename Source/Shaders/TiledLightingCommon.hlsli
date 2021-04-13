#ifndef _TILED_LIGHTING_COMMON_H
#define _TILED_LIGHTING_COMMON_H

#define TILED_LIGHTING_TILE_SIZE 16

struct TiledLightingCB
{
	uint NumTilesX;
	uint NumTilesY;
	uint NumLights;
	uint padding;
};

struct PerLightPixelCB
{
	float3 Position;
	float Radius;
};

#endif // _TILED_LIGHTING_COMMON_H