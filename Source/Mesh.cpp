#include "PCH.h"
#include "Mesh.h"

#include "tiny_obj_loader.h"

void Mesh::LoadFromFile(std::istream* inStream, size_t numObject)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;



	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, inStream);

	tinyobj::mesh_t& mesh = shapes[numObject].mesh;

	const float3* vertices = (const float3*)attrib.vertices.data(); // vertex data
	const float3* normals = (const float3*)attrib.normals.data(); // normal data
	const float2* uvs = (const float2*)attrib.texcoords.data(); // UV data

	for (size_t i = 0; i < mesh.indices.size(); ++i)
	{
		const tinyobj::index_t& idx = mesh.indices[i];

		Vertex vertex;
		vertex.position = vertices[idx.vertex_index];
		vertex.normal = normals[idx.normal_index];
		if ((idx.texcoord_index) >= 0)
		{
			vertex.UV = uvs[idx.texcoord_index];
		}
		mVB.push_back(vertex);

		mIB.push_back((uint32_t)i);
	}

	// Computing simple tangent basis
	// http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-13-normal-mapping/
	for (size_t tIdx = 0; tIdx < mIB.size() / 3; ++tIdx)
	{
		Vertex& v0 = mVB[mIB[tIdx * 3 + 0]];
		Vertex& v1 = mVB[mIB[tIdx * 3 + 1]];
		Vertex& v2 = mVB[mIB[tIdx * 3 + 2]];

		// Edges of the triangle : position delta
		float3 deltaPos1 = v1.position - v0.position;
		float3 deltaPos2 = v2.position - v0.position;

		// UV delta
		float2 deltaUV1 = v1.UV - v0.UV;
		float2 deltaUV2 = v2.UV - v0.UV;

		float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
		float3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
		float3 binormal = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;

		v0.tangent = v1.tangent = v2.tangent = normalize(tangent);
		v0.binormal = v1.binormal = v2.binormal = normalize(binormal);
	}
}
