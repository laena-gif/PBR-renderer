cbuffer ClearCB
{
	uint NumElements;
}

RWStructuredBuffer<uint> BufferToClear : register(u0);

[numthreads(32, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadId)
{
	if (threadId.x < NumElements)
	{
		BufferToClear[threadId.x] = 0;
	}
}