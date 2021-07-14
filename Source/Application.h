#pragma once
#include "PCH.h"
#include "Mesh.h"
#include "Camera.h"
#include "DebugDrawer.h"
#include "ProxyLightDrawer.h"
#include "LightSource.h"
#include "PointLight.h"
#include "Manager.h"
#include "Shaders/TiledLightingCommon.hlsli"

struct PerFrameCB
{
	float4x4 matView;
	float4x4 matProj;
	float4x4 matInvProjView;
};

struct PerFrameComputeCB
{
	float4x4 matInvProjView;
	float4x4 matLightSourceView;
	float4x4 matLightSourceProj;
	// float3   lightPosition;
	// uint32_t padding0;
	float3	 camPosition;
	uint32_t padding1;
};

struct PerLightCB
{
	float4x4 matView;
	float4x4 matProj;
	float4x4 matInvProj;
};

constexpr uint32_t kNumGBufferTextures = 2;
constexpr uint32_t kNumFramesInFlight = 2;
// RTV layout (one frame): 
// [backbuffer] [GBuffer] * kNumGBufferTextures
namespace RTVLayout
{
	enum
	{
		Backbuffer = 0,
		GBuffer,
		ProxyLight,
		DownSampledDepth,
		RTVcubemap,
		Count = DownSampledDepth + kNumGBufferTextures
	};
}

// SRV layout (one frame):
// [GBuffer] * kNumGBufferTextures [lighting output UAV] [lighting output SRV]
namespace SRVLayout
{
	enum
	{
		GBuffer,
		LightingUAV = GBuffer + kNumGBufferTextures,
		LightingSRV,
		VerticalBlurUAV,
		VerticalBlurSRV,
		DepthBufferSRV,
		ShadowMapSRV,
		DownSampledDepthUAV,
		DownSampledDepthSRV,
		TiledLightingPixelShaderCBV,
		TransMatrixCBV,
		PerLightPixelCBV,
		Count
	};
}

namespace DSVLayout
{
	enum
	{
		SceneDepth = 0,
		ShadowMap,
		LightCullingDepth,
		Count 
	};
}

constexpr uint32_t kNumTextures = 30;
constexpr uint32_t kTextureSRVsOffset = SRVLayout::Count * kNumFramesInFlight;
constexpr uint32_t kNumSRVDescriptors = kTextureSRVsOffset + kNumTextures;


// root signatures slots helpers
namespace RootSignature
{
	namespace BackBuffer
	{
		enum
		{
			CountedLighting = 0,
			Count
		};
	}


	namespace Blur
	{
		enum
		{
			TextureToBlur = 0,
			BlurredTexture,
			Count
		};
	}

	namespace ProxyLight
	{
		enum 
		{
			VertexConstantBuffer = 0,
			PixelConstantBuffer,
			TransMatrixConstantBuffer,
			LightingTilesBuffer,
			MinMaxDepths,
			PerLightConstantBuffer,
			Count
		};
	}

	namespace UAVClear
	{
		enum
		{
			ConstantBuffer,
			BufferToClear,
			Count
		};
	}

	namespace DepthDownsampling
	{
		enum
		{
			DownsampledTexture = 0,
			TextureToDownsample,
			Count

		};
	}

	namespace CopyDepth
	{
		enum
		{
			HiZ = 0,
			Count
		};
	}

}

struct RenderMesh
{
	ID3D12Resource* mVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
	uint32_t mSize;
};

class Application
{
public:
	void Init(uint32_t width, uint32_t height);
	void Run();

private:
	void Render();
	void Animate(float timeDelta);
	void InitWindow();
	void InitD3D12();
	void InitPipelineState();
	void InitMeshes();
	void InitTextures();
	void InitLighting();

	void WaitForFrame();

	void StartCameraDrag();
	void StopCameraDrag();
	void ProcessCameraDrag(float timeDelta);

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	ComPtr<ID3D12Device> mDevice;
	ComPtr<ID3D12CommandQueue> mQueue;
	ComPtr<ID3D12CommandAllocator> mAllocator;
	ComPtr<ID3D12GraphicsCommandList> mCommandList;
	ComPtr<ID3D12DescriptorHeap> mRTVHeap;
	ComPtr<ID3D12DescriptorHeap> mDSVHeap;
	ComPtr<ID3D12DescriptorHeap> mSRVHeap;

	// TODO: group root signature with PSO
	ComPtr<ID3D12RootSignature> mRootSignature;
	ComPtr<ID3D12RootSignature> mShadowMapRootSignature;
	ComPtr<ID3D12RootSignature> mComputeRootSignature;
	ComPtr<ID3D12RootSignature> mRootSignatureRender;
	ComPtr<ID3D12RootSignature> mProxyLightRootSignature;
	ComPtr<ID3D12RootSignature> mDownsamplingRootSignature;
	ComPtr<ID3D12RootSignature> mCopyDepthRootSignature;


	ComPtr<ID3D12PipelineState> mPSO;
	ComPtr<ID3D12PipelineState> mShadowMapPSO;
	ComPtr<ID3D12PipelineState> mComputePSO;
	ComPtr<ID3D12PipelineState> mRenderPSO;
	ComPtr<ID3D12PipelineState> mCopyDepthPSO;
	ComPtr<ID3D12PipelineState> mDownsamplingPSO;
	
	ComPtr<ID3D12RootSignature> mBlurRootSignature;
	ComPtr<ID3D12PipelineState> mVerticalBlurPSO;
	ComPtr<ID3D12PipelineState> mHorizontalBlurPSO;

	ComPtr<ID3D12RootSignature> mUAVClearRootSignature;
	ComPtr<ID3D12PipelineState> mUAVClearPSO;

	D3D12_CPU_DESCRIPTOR_HANDLE mRTVs[kNumFramesInFlight];
	D3D12_CPU_DESCRIPTOR_HANDLE mDSVs[kNumFramesInFlight];
	D3D12_CPU_DESCRIPTOR_HANDLE mShadowMapDSVs[kNumFramesInFlight];
	D3D12_CPU_DESCRIPTOR_HANDLE mGBufferSRVs[kNumFramesInFlight];
	D3D12_CPU_DESCRIPTOR_HANDLE mGBufferRTVs[kNumFramesInFlight];

	D3D12_CPU_DESCRIPTOR_HANDLE mProxySphereRTVs[kNumFramesInFlight];
	D3D12_CPU_DESCRIPTOR_HANDLE mLightCullingDepthMaxDSVs[kNumFramesInFlight];

	ComPtr<ID3D12Resource> mBackBufferTextures[kNumFramesInFlight];
	ComPtr<ID3D12Resource> mDepthTextures[kNumFramesInFlight];
	ComPtr<ID3D12Resource> mDownsampledDepthComputeOutput[kNumFramesInFlight];
	ComPtr<ID3D12Resource> mLightCullingDepthMax[kNumFramesInFlight];
	ComPtr<ID3D12Resource> mShadowMapTextures[kNumFramesInFlight];
	ComPtr<ID3D12Resource> mBaseColorAndRoughness[kNumFramesInFlight];
	ComPtr<ID3D12Resource> mNormalsAndMetalness[kNumFramesInFlight];
	ComPtr<ID3D12Resource> mComputeOutput[kNumFramesInFlight];
	ComPtr<ID3D12Resource> mVerticalBlurTexture[kNumFramesInFlight];
	ComPtr<ID3D12Resource> mProxySphereOutput[kNumFramesInFlight];
	
	// buffers
	ComPtr<ID3D12Resource> mLightsBuffer;
	ComPtr<ID3D12Resource> mLightingTilesBuffer;
	ComPtr<ID3D12Resource> mUploadConstantBuffer[kNumFramesInFlight];
	ComPtr<ID3D12Resource> mMatTransConstantBuffer;
	ComPtr<ID3D12Resource> mPerLightPixelConstantBuffer;

	UINT8* mLightPixelConstantBufferGPUAddress[kNumFramesInFlight];

	UINT mSRVDescriptorSize;
	UINT mRTVDescriptorSize;
	UINT mDSVDescriptorSize;

	ComPtr<IDXGISwapChain4> mSwapChain;
	ComPtr<ID3D12Fence> mFence;
	uint64_t mFenceSignaledValue = 0;

	HWND mWindowHandle = NULL;
	uint32_t mWidth = 0;
	uint32_t mHeight = 0;
	bool mIsWorking = false;

	std::vector<RenderMesh> mRenderMeshes;
	RenderMesh mLightRenderMesh;

	PerFrameCB mPerFrame;
	PerFrameComputeCB mPerFrameCompute;
	PerLightCB mPerLight;

	POINT mLastCursorPos;
	float4x4 mLightSourceMatView; 
	float4x4 mLightSourceMatProj;
	float4x4 mLightSourceMatInvProjView;
	//float4x4 mMatView; // transforms everything that it becomes relative to camera/eye. And camera/eye is always in float3(0, 0, 0)
	//float4x4 mMatProj; // from model*mMatView space to clip space
	
	Camera mCamera;
	LightSource mLightSource;
	DebugDrawer mDebugDrawer;
	LightDrawer mLightDrawer;

	PointLight* mLights;
	TiledLightingCB* mLightPixelCBDataArray;
	float4x4* mMatTransArray;
	PerLightPixelCB* mPerLightCBArray;

	bool mIsCameraDrag = false;
	uint32_t mNumTexturesLoaded = 0;
	uint32_t mCurrFrame = 0;

	MemoryManager mMemoryManager;

};

struct ScopedPerfMarker
{
	operator bool (){ return true; }

	ScopedPerfMarker(ID3D12GraphicsCommandList* commandList, const char* name)
		: mCommandList(commandList)
	{
		PIXBeginEvent(commandList, 0, name);
	}

	~ScopedPerfMarker()
	{
		PIXEndEvent(mCommandList);
	}

	ID3D12GraphicsCommandList* mCommandList;
};

#define RENDER_PASS(name) \
	if (auto scopedPerfMarker = ScopedPerfMarker(mCommandList.Get(), name))