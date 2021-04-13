#include <PCH.h>
#include "DebugDrawer.h"

#include "CompiledShaders.h"

void DebugDrawer::Initialize(ID3D12Device* device, ID3D12RootSignature* rootSignature)
{
	constexpr uint32_t kVertexBufferSize = 16 * 1024 * 1024;

	device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(kVertexBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mVertexBuffer));
	mVertexBuffer->Map(0, nullptr, (void**)&mVertexBufferPtr);
	mCurrentVertexBufferPtr = mVertexBufferPtr;

	mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.SizeInBytes = kVertexBufferSize;
	mVertexBufferView.StrideInBytes = sizeof(Vertex);


	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature;
	psoDesc.VS.pShaderBytecode = DebugVertex_ShaderData;
	psoDesc.VS.BytecodeLength = std::size(DebugVertex_ShaderData);
	psoDesc.PS.pShaderBytecode = DebugPixel_ShaderData;
	psoDesc.PS.BytecodeLength = std::size(DebugPixel_ShaderData);

	psoDesc.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
	psoDesc.SampleMask = 0xFFFFFFFF;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.InputLayout.NumElements = (UINT)std::size(inputLayout);
	psoDesc.InputLayout.pInputElementDescs = inputLayout;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO));
}

void DebugDrawer::DrawGrid()
{
	float4 defaultColor = float4(0.5f, 0.5f, 0.5f, 1.0f);
	const int xSize = 50;
	const int ySize = 50;

	for (int y = -ySize; y <= ySize; ++y)
	{
		DrawLine(float3(-(float)xSize, (float)y, 0.0f), float3((float)xSize, (float)y, 0.0f), y == 0 ? float4(1.0f, 0.0f, 0.0f, 1.0f) : defaultColor);
	}
	for (int x = -xSize; x <= xSize; ++x)
	{
		DrawLine(float3((float)x, -(float)ySize, 0.0f), float3((float)x, (float)ySize, 0.0f), x == 0 ? float4(0.0f, 1.0f, 0.0f, 1.0f) : defaultColor);
	}
}

void DebugDrawer::DrawAABB(const float3& center, const float3& size, const float4& color)
{
	float3 minAABB = GetMin(center, size);
	float3 maxAABB = GetMax(center, size);
	DrawLine(minAABB, float3(minAABB.x, minAABB.y + 2 * size.y, minAABB.z), color);
	DrawLine(minAABB, float3(minAABB.x + 2 * size.x, minAABB.y, minAABB.z), color);
	DrawLine(minAABB, float3(minAABB.x, minAABB.y, minAABB.z + 2 * size.z), color);
	//DrawLine(minAABB, maxAABB);
	DrawLine(float3(minAABB.x, minAABB.y, minAABB.z + 2 * size.z), float3(minAABB.x, minAABB.y + 2 * size.y, minAABB.z + 2 * size.z), color);
	DrawLine(float3(minAABB.x, minAABB.y + 2 * size.y, minAABB.z + 2 * size.z), float3(minAABB.x, minAABB.y + 2 * size.y, minAABB.z), color);
	DrawLine(float3(minAABB.x, minAABB.y + 2 * size.y, minAABB.z), float3(maxAABB.x, maxAABB.y, maxAABB.z-2*size.z), color);
	DrawLine(float3(maxAABB.x, maxAABB.y, maxAABB.z), float3(maxAABB.x, maxAABB.y, maxAABB.z - 2 * size.z), color);
	DrawLine(float3(minAABB.x, minAABB.y + 2 * size.y, minAABB.z + 2* size.z), maxAABB, color);
	DrawLine(maxAABB, float3(maxAABB.x, maxAABB.y - 2 * size.y, maxAABB.z), color);
	DrawLine(float3(maxAABB.x, maxAABB.y - 2 * size.y, maxAABB.z), float3(float(minAABB.x + 2 * size.x), minAABB.y, minAABB.z), color);
	DrawLine(float3(minAABB.x, minAABB.y, minAABB.z + 2 * size.z), float3(maxAABB.x, maxAABB.y - 2 * size.y, maxAABB.z), color);
	DrawLine(float3(minAABB.x + 2 * size.x, minAABB.y, minAABB.z), float3(maxAABB.x, maxAABB.y,maxAABB.z - 2 * size.z), color);
}

void DebugDrawer::DrawLine(const float3& p0, const float3& p1, const float4& color)
{
	mCurrentNumVertices += 2;
	mCurrentVertexBufferPtr[0].Position = p0;
	mCurrentVertexBufferPtr[0].Color = color;

	mCurrentVertexBufferPtr[1].Position = p1;
	mCurrentVertexBufferPtr[1].Color = color;
	mCurrentVertexBufferPtr += 2;
}

void DebugDrawer::Execute(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetPipelineState(mPSO.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	commandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
	commandList->DrawInstanced(mCurrentNumVertices, 1, 0, 0);

	mCurrentNumVertices = 0;
	mCurrentVertexBufferPtr = mVertexBufferPtr;
}

float3 DebugDrawer::GetMax(const float3& center, const float3& size)
{
	float3 p1 = center + size;
	float3 p2 = center - size;

	return float3(glm::max(p1.x, p2.x), glm::max(p1.y, p2.y), glm::max(p1.z, p2.z));
}

float3 DebugDrawer::GetMin(const float3& center, const float3& size)
{
	float3 p1 = center + size;
	float3 p2 = center - size;

	return float3(glm::min(p1.x, p2.x), glm::min(p1.y, p2.y), glm::min(p1.z, p2.z));
}
