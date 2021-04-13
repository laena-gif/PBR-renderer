#pragma once

#include <vector>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <fstream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#define USE_PIX
#include <pix3.h>
using Microsoft::WRL::ComPtr;

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/transform.hpp> 

using glm::int2;
using glm::int3;
using glm::int4;

using glm::float2;
using glm::float3;
using glm::float4;

using glm::float3x3;
using glm::float4x4;
using glm::quat;
using uint = uint32_t;

inline uint32_t DivideRoundingUp(uint32_t x, uint32_t y)
{
	return (x + y - 1) / y;
}