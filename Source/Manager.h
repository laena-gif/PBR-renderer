#pragma once



class MemoryManager
{
public:
	void Init(ID3D12Device* device,  uint32_t size, uint32_t numFramesInFlight); // initialize
	void OnNewFrame(); //  switch to frame
	D3D12_GPU_VIRTUAL_ADDRESS AllocateBuffer(uint32_t size, const void* InitialData);

	template <class T>
	inline D3D12_GPU_VIRTUAL_ADDRESS AllocateBuffer(const T& InitialData)
	{
		return AllocateBuffer(sizeof(T), &InitialData);
	}

private:
	ID3D12Resource* mBigConstantBuffer = nullptr; // big constant buffer in upload heap
	uint32_t mCurrentFrame = 0;
	size_t mCurrentOffset = 0; // amount of mem that already fullfiled 
	void* mMappedPointer = nullptr;
	uint32_t mSize = 0;
	uint32_t mNumFramesInFlight = 2;
};


