#include <PCH.h>
#include "ProxyLightDrawer.h"
#include "Mesh.h"
#include "PointLight.h"
#include "CompiledShaders.h"

void LightDrawer::Initialize(ID3D12Device* device, ID3D12RootSignature* rootSignature)
{
	std::string pointLightMesh = "..\\Scene\\OBJ\\ProxyLight.obj";
	std::ifstream ifs(pointLightMesh);
	Mesh lightMesh;
	lightMesh.LoadFromFile(&ifs, 0);
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(lightMesh.mVB.size() * sizeof(Vertex)), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mLightRenderMesh.mVertexBuffer));

	void* mappedData = nullptr;
	mLightRenderMesh.mVertexBuffer->Map(0, nullptr, &mappedData);
	memcpy(mappedData, lightMesh.mVB.data(), lightMesh.mVB.size() * sizeof(Vertex));
	mLightRenderMesh.mVertexBuffer->Unmap(0, nullptr);

	mLightRenderMesh.mVertexBufferView.BufferLocation = mLightRenderMesh.mVertexBuffer->GetGPUVirtualAddress();
	mLightRenderMesh.mSize = lightMesh.mVB.size();
	mLightRenderMesh.mSizeIndexed = lightMesh.mIB.size();
	mLightRenderMesh.mVertexBufferView.SizeInBytes = (UINT)(mLightRenderMesh.mSize * sizeof(Vertex));
	mLightRenderMesh.mVertexBufferView.StrideInBytes = sizeof(Vertex);

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature;
	psoDesc.VS.pShaderBytecode = LightVertex_ShaderData;
	psoDesc.VS.BytecodeLength = std::size(LightVertex_ShaderData);
	psoDesc.PS.pShaderBytecode = LightPixel_ShaderData;
	psoDesc.PS.BytecodeLength = std::size(LightPixel_ShaderData);

	psoDesc.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
	psoDesc.SampleMask = 0xFFFFFFFF;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.InputLayout.NumElements = (UINT)std::size(inputLayout);
	psoDesc.InputLayout.pInputElementDescs = inputLayout;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO));


}

void LightDrawer::Execute(ID3D12GraphicsCommandList* commandList, uint32_t lightindex)
{
	
	commandList->SetPipelineState(mPSO.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &mLightRenderMesh.mVertexBufferView);
	//commandList->DrawInstanced(mLightRenderMesh.mSize, lightindex + 1, 0, lightindex);
	commandList->DrawInstanced(mLightRenderMesh.mSize, 16, 0, 0);


}
