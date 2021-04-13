#pragma once

static constexpr uint32_t kNumPointLights = 16;

struct PointLight
{
	float3   position;
	float    radius;
	uint32_t color;
};