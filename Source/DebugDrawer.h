#pragma once

class DebugDrawer
{
public:
	void Initialize(ID3D12Device* device, ID3D12RootSignature* rootSignature);
	void DrawGrid();
	void DrawAABB(const float3& center, const float3& size, const float4& color = float4(1.0f, 0.0f, 0.0f, 0.0f));
	void DrawLine(const float3& p0, const float3& p1, const float4& color = float4(0.0f, 0.0f, 1.0f, 0.0f));
	void Execute(ID3D12GraphicsCommandList* commandList);

private:
	struct Vertex
	{
		float3 Position;
		float4 Color;
	};

	ComPtr<ID3D12PipelineState> mPSO;
	ComPtr<ID3D12Resource> mVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

	Vertex* mVertexBufferPtr = nullptr;
	uint32_t mCurrentNumVertices = 0;
	Vertex* mCurrentVertexBufferPtr = nullptr;

	float3 GetMax(const float3& center, const float3& size);
	float3 GetMin(const float3& center, const float3& size);
};