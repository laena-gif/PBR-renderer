#pragma once

struct Vertex
{
	float3 position;
	float3 tangent;
	float3 binormal;
	float3 normal;
	float2 UV;

	Vertex() = default;

	Vertex(const float3& inPosition, const float2& inUV)
		: position(inPosition)
		, UV(inUV)
	{
	}
};


using VertexBuffer = std::vector<Vertex>;
using IndexBuffer = std::vector<uint32_t>;


class Mesh
{
public:
	void LoadFromFile(std::istream* inStream, size_t numObject);
	void LoadPositionsFromFile(std::istream* inStream, size_t numObject);

	VertexBuffer mVB;
	IndexBuffer mIB;
};
