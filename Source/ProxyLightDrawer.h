#pragma once

#include "Shaders/TiledLightingCommon.hlsli"

class LightDrawer
{
public:
	void Initialize(ID3D12Device* device, ID3D12RootSignature* rootSignature);
	void Execute(ID3D12GraphicsCommandList* commandList, uint32_t lightindex);

	static constexpr uint32_t kTileSize = TILED_LIGHTING_TILE_SIZE;

private:
	struct RenderMesh
	{
		ID3D12Resource* mVertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
		uint32_t mSize;
		uint32_t mSizeIndexed;
	};

	ComPtr<ID3D12PipelineState> mPSO;
	RenderMesh mLightRenderMesh;
	
};