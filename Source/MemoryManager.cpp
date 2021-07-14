#include "PCH.h"
#include "MemoryManager.h"

void MemoryManager::Init(ID3D12Device* device, uint32_t size, uint32_t numFramesInFlight)
{
	mSize = size;
	mNumFramesInFlight = numFramesInFlight;
	D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(mSize * mNumFramesInFlight);
	device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mBigConstantBuffer));
	mBigConstantBuffer->Map(0, nullptr, &mMappedPointer);

}

void MemoryManager::OnNewFrame()
{
	mCurrentFrame = (mCurrentFrame + 1) % mNumFramesInFlight;
	mCurrentOffset = mSize * mCurrentFrame;

}

D3D12_GPU_VIRTUAL_ADDRESS MemoryManager::AllocateBuffer(uint32_t size, const void* InitialData)
{
	size_t StartOffset = mCurrentOffset;
	memcpy((uint8_t*)mMappedPointer + mCurrentOffset, InitialData, size);
	size_t EndOffset = mCurrentOffset + size;
	
	mCurrentOffset = EndOffset + 256 - (EndOffset % 256); //256 byte allignment
	return mBigConstantBuffer->GetGPUVirtualAddress() + StartOffset;
}
