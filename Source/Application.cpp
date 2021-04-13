#include <PCH.h>
#include "Application.h"
#include "d3dx12.h"
#include "DDSTextureLoader12.h"
#include "PointLight.h"
#include "Halton.h"

#include "../CompiledShaders/CompiledShaders.h"

#include "Shaders/TiledLightingCommon.hlsli"

Application* GApplication = nullptr;

void Application::Init(uint32_t width, uint32_t height)
{


	GApplication = this;

	mWidth = width;
	mHeight = height;

	InitWindow();
	InitD3D12();
	InitMeshes();
	InitTextures();
	InitLighting();
}

void Application::Run()
{
	MSG msg{};
	int64_t timeFreq = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&timeFreq);
	int64_t timeCurrent = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&timeCurrent);
	int64_t timeLast = timeCurrent;

	mIsWorking = true;
	while (mIsWorking)
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				mIsWorking = false;
				std::quick_exit(0); // fuck that shit
				break;
			}

			DispatchMessage(&msg);
		}
		QueryPerformanceCounter((LARGE_INTEGER*)&timeCurrent);
		float timeDelta = (timeCurrent - timeLast) / (float)timeFreq;
		timeLast = timeCurrent;
		Animate(timeDelta);
		Render();
		WaitForFrame();

	}
}

void Application::Render()
{


	mPerFrame.matView = mLightSource.GetViewMatrix();
	mPerFrame.matProj = mLightSource.GetProjMatrix();
	mPerFrame.matInvProjView = glm::inverse(mPerFrame.matProj * mPerFrame.matView);

	mAllocator->Reset();
	mCommandList->Reset(mAllocator.Get(), nullptr);
	

	uint32_t frameIndex = mSwapChain->GetCurrentBackBufferIndex();

	float4 clearColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBackBufferTextures[frameIndex].Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->ClearRenderTargetView(mRTVs[frameIndex], (const float*)&clearColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(mDSVs[frameIndex], D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);

	D3D12_VIEWPORT viewport{};
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissorRect{};

	RENDER_PASS("Shadow Map creation")
	{
		viewport.Width = (float)2048;
		viewport.Height = (float)2048;
		scissorRect.right = 2048;
		scissorRect.bottom = 2048;
		mCommandList->ClearDepthStencilView(mShadowMapDSVs[frameIndex], D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
		mCommandList->RSSetViewports(1, &viewport);
		mCommandList->RSSetScissorRects(1, &scissorRect);
		mCommandList->OMSetRenderTargets(0, nullptr, 0, &mShadowMapDSVs[frameIndex]);
		mCommandList->SetPipelineState(mShadowMapPSO.Get());

		ID3D12DescriptorHeap* descriptorHeaps[1] = { mSRVHeap.Get() };
		mCommandList->SetDescriptorHeaps(1, descriptorHeaps);
		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		uint32_t currentTexture = 0;
		for (RenderMesh& mesh : mRenderMeshes)
		{
			mCommandList->SetGraphicsRoot32BitConstants(0, sizeof(mPerFrame) / 4, &mPerFrame, 0);
			CD3DX12_GPU_DESCRIPTOR_HANDLE textureHandle{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), (INT)(kTextureSRVsOffset + currentTexture), mSRVDescriptorSize };
			mCommandList->SetGraphicsRootDescriptorTable(1, textureHandle);
			mCommandList->IASetVertexBuffers(0, 1, &mesh.mVertexBufferView);
			mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			mCommandList->DrawInstanced(mesh.mSize, 1, 0, 0);

			++currentTexture;
		}
	}

	mPerFrame.matView = mCamera.GetViewMatrix();
	mPerFrame.matProj = mCamera.GetProjMatrix();
	mPerFrame.matInvProjView = glm::inverse(mPerFrame.matProj * mPerFrame.matView);

	//std::ofstream output_file;
	//output_file.open("C:\\temp\\pool\\output.txt", std::ios_base::app);
	//output_file << mCurrFrame << std::endl;
	//output_file << "***" << std::endl;
	//output_file.close();

	float dx = ((Halton2D(mCurrFrame) - 0.5f) * 2.0f).r;
	float dy = ((Halton2D(mCurrFrame) - 0.5f) * 2.0f).g;
	mPerFrame.matProj[2][0] += dx / mWidth;
	mPerFrame.matProj[2][1] += dy / mHeight;

	RENDER_PASS("Rendering GBuffer")
	{
		viewport.Width = (float)mWidth;
		viewport.Height = (float)mHeight;
		scissorRect.right = mWidth;
		scissorRect.bottom = mHeight;

		mCommandList->RSSetViewports(1, &viewport);
		mCommandList->RSSetScissorRects(1, &scissorRect);
		mCommandList->OMSetRenderTargets(kNumGBufferTextures, &mGBufferRTVs[frameIndex], TRUE, &mDSVs[frameIndex]);
		mCommandList->SetPipelineState(mPSO.Get());

		ID3D12DescriptorHeap* descriptorHeaps[1] = { mSRVHeap.Get() };
		mCommandList->SetDescriptorHeaps(1, descriptorHeaps);
		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		uint32_t currentTexture = 0;
		for (RenderMesh& mesh : mRenderMeshes)
		{
				mCommandList->SetGraphicsRoot32BitConstants(0, sizeof(mPerFrame) / 4, &mPerFrame, 0);
				CD3DX12_GPU_DESCRIPTOR_HANDLE textureHandle{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), (INT)(kTextureSRVsOffset + (currentTexture * 3)), mSRVDescriptorSize };
				mCommandList->SetGraphicsRootDescriptorTable(1, textureHandle);
				mCommandList->IASetVertexBuffers(0, 1, &mesh.mVertexBufferView);
				mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				mCommandList->DrawInstanced(mesh.mSize, 1, 0, 0);
				++currentTexture;
		}

	}

	RENDER_PASS("Downsampling")
	{

		uint32_t downsampledWidth = DivideRoundingUp(mWidth, LightDrawer::kTileSize);
		uint32_t downsampledHeight = DivideRoundingUp(mHeight, LightDrawer::kTileSize);
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthTextures[frameIndex].Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
		using namespace RootSignature;
		CD3DX12_GPU_DESCRIPTOR_HANDLE computeInSRV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::DepthBufferSRV, mSRVDescriptorSize };
		CD3DX12_GPU_DESCRIPTOR_HANDLE computeOutUAV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::DownSampledDepthUAV, mSRVDescriptorSize };
		mCommandList->SetPipelineState(mDownsamplingPSO.Get());
		mCommandList->SetComputeRootSignature(mDownsamplingRootSignature.Get());
		mCommandList->SetComputeRootDescriptorTable(DepthDownsampling::DownsampledTexture, computeOutUAV);
		mCommandList->SetComputeRootDescriptorTable(DepthDownsampling::TextureToDownsample, computeInSRV);
		mCommandList->Dispatch(downsampledWidth, downsampledHeight, 1);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthTextures[frameIndex].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	}

	RENDER_PASS("Copy depth")
	{
		using namespace RootSignature;
		scissorRect.right = DivideRoundingUp(mWidth, LightDrawer::kTileSize);
		scissorRect.bottom = DivideRoundingUp(mHeight, LightDrawer::kTileSize);
		viewport.Width = (float)scissorRect.right;
		viewport.Height = (float)scissorRect.bottom;

		mCommandList->RSSetViewports(1, &viewport);
		mCommandList->RSSetScissorRects(1, &scissorRect);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDownsampledDepthComputeOutput[frameIndex].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		CD3DX12_GPU_DESCRIPTOR_HANDLE renderInSRVs{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::DownSampledDepthSRV, mSRVDescriptorSize };
		mCommandList->SetPipelineState(mCopyDepthPSO.Get());
		mCommandList->OMSetRenderTargets(0, nullptr, TRUE, &mLightCullingDepthMaxDSVs[frameIndex]);
		mCommandList->SetGraphicsRootSignature(mCopyDepthRootSignature.Get());
		ID3D12DescriptorHeap* descriptorHeaps[1] = { mSRVHeap.Get() };
		mCommandList->SetDescriptorHeaps(1, descriptorHeaps);
		mCommandList->SetGraphicsRootDescriptorTable(CopyDepth::HiZ, renderInSRVs);
		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mCommandList->DrawInstanced(3, 1, 0, 0);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDownsampledDepthComputeOutput[frameIndex].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}

	RENDER_PASS("Light source render")
	{
		scissorRect.right = DivideRoundingUp(mWidth, LightDrawer::kTileSize);
		scissorRect.bottom = DivideRoundingUp(mHeight, LightDrawer::kTileSize);
		viewport.Width = (float)scissorRect.right;
		viewport.Height = (float)scissorRect.bottom;
		
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDownsampledDepthComputeOutput[frameIndex].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		CD3DX12_GPU_DESCRIPTOR_HANDLE renderInSRVs{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::DownSampledDepthSRV, mSRVDescriptorSize };
		CD3DX12_GPU_DESCRIPTOR_HANDLE renderInCBVs{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::TiledLightingPixelShaderCBV, mSRVDescriptorSize };
		CD3DX12_GPU_DESCRIPTOR_HANDLE TransMatrixCBV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::TransMatrixCBV, mSRVDescriptorSize };
		CD3DX12_GPU_DESCRIPTOR_HANDLE PerLightPixelCBV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::PerLightPixelCBV, mSRVDescriptorSize };

		TiledLightingCB tiledLightingCB;
		tiledLightingCB.NumTilesX = DivideRoundingUp(mWidth, LightDrawer::kTileSize);
		tiledLightingCB.NumTilesY = DivideRoundingUp(mHeight, LightDrawer::kTileSize);
		tiledLightingCB.NumLights = kNumPointLights;

		using namespace RootSignature;

		RENDER_PASS("Clear lighting tiles buffer")
		{
			mCommandList->SetPipelineState(mUAVClearPSO.Get());
			uint32_t numElements = tiledLightingCB.NumTilesX * tiledLightingCB.NumTilesY * (kNumPointLights + 1);
			mCommandList->SetComputeRootSignature(mUAVClearRootSignature.Get());
			mCommandList->SetComputeRoot32BitConstant(UAVClear::ConstantBuffer, numElements, 0);
			mCommandList->SetComputeRootUnorderedAccessView(UAVClear::BufferToClear, mLightingTilesBuffer->GetGPUVirtualAddress());
			mCommandList->Dispatch(DivideRoundingUp(numElements, 32), 1, 1);
		}

		mPerLight.matView = mCamera.GetViewMatrix();
		mPerLight.matProj = mCamera.GetProjMatrix();
		mPerLight.matInvProj = glm::inverse(mCamera.GetProjMatrix());

		mCommandList->ClearRenderTargetView(mProxySphereRTVs[frameIndex], (const float*)&clearColor, 0, nullptr);
		mCommandList->RSSetViewports(1, &viewport);
		mCommandList->RSSetScissorRects(1, &scissorRect);
		if (GetAsyncKeyState('P'))
		{
			mCommandList->OMSetRenderTargets(1, &mProxySphereRTVs[frameIndex], TRUE, nullptr);
		}
		else
		{
			mCommandList->OMSetRenderTargets(1, &mProxySphereRTVs[frameIndex], TRUE, &mLightCullingDepthMaxDSVs[frameIndex]);
		}

		
		mCommandList->SetGraphicsRootSignature(mProxyLightRootSignature.Get());
		mCommandList->SetGraphicsRootUnorderedAccessView(ProxyLight::LightingTilesBuffer, mLightingTilesBuffer->GetGPUVirtualAddress());
		mCommandList->SetGraphicsRootDescriptorTable(ProxyLight::MinMaxDepths, renderInSRVs);
		mCommandList->SetGraphicsRootDescriptorTable(ProxyLight::TransMatrixConstantBuffer, TransMatrixCBV);
		mCommandList->SetGraphicsRootDescriptorTable(ProxyLight::PerLightConstantBuffer, PerLightPixelCBV);
		mCommandList->SetGraphicsRoot32BitConstants(ProxyLight::VertexConstantBuffer, sizeof(mPerLight) / 4, &mPerLight, 0);
		mCommandList->SetGraphicsRoot32BitConstants(ProxyLight::PixelConstantBuffer, sizeof(tiledLightingCB) / 4, &tiledLightingCB, 0);
	
		mLightDrawer.Execute(mCommandList.Get(), 0);

		



		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDownsampledDepthComputeOutput[frameIndex].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}

	RENDER_PASS("Resolving lighting")
	{
		mPerFrameCompute.matInvProjView = glm::inverse(mPerFrame.matProj * mPerFrame.matView);
		mPerFrameCompute.matLightSourceView = mLightSource.GetViewMatrix();
		mPerFrameCompute.matLightSourceProj = mLightSource.GetProjMatrix();
		//mPerFrameCompute.lightPosition = mLightSource.GetLightPos();
		mPerFrameCompute.camPosition = mCamera.GetCameraPos();

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBaseColorAndRoughness[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mNormalsAndMetalness[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthTextures[frameIndex].Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMapTextures[frameIndex].Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));


		CD3DX12_GPU_DESCRIPTOR_HANDLE computeInSRVs{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::GBuffer, mSRVDescriptorSize };
		CD3DX12_GPU_DESCRIPTOR_HANDLE computeOutUAV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::LightingUAV, mSRVDescriptorSize };
		CD3DX12_GPU_DESCRIPTOR_HANDLE computeDepthInputSRV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::DepthBufferSRV, mSRVDescriptorSize };
		CD3DX12_GPU_DESCRIPTOR_HANDLE computeShadowMapInputSRV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::ShadowMapSRV, mSRVDescriptorSize };
		CD3DX12_GPU_DESCRIPTOR_HANDLE computeCubeMapInputSRV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), (INT)(kTextureSRVsOffset + mNumTexturesLoaded - 4), mSRVDescriptorSize };
		CD3DX12_GPU_DESCRIPTOR_HANDLE computeIBLInputSRV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), (INT)(kTextureSRVsOffset + mNumTexturesLoaded - 3), mSRVDescriptorSize };
	

		mCommandList->SetPipelineState(mComputePSO.Get());

		TiledLightingCB tiledLightingCB;
		tiledLightingCB.NumTilesX = DivideRoundingUp(mWidth, LightDrawer::kTileSize);
		tiledLightingCB.NumTilesY = DivideRoundingUp(mHeight, LightDrawer::kTileSize);
		using namespace RootSignature;

		mCommandList->SetComputeRootSignature(mComputeRootSignature.Get());
		mCommandList->SetComputeRootDescriptorTable(0, computeInSRVs);
		mCommandList->SetComputeRootDescriptorTable(1, computeOutUAV);
		mCommandList->SetComputeRoot32BitConstants(2, sizeof(mPerFrameCompute) / 4, &mPerFrameCompute, 0);
		mCommandList->SetComputeRootDescriptorTable(3, computeDepthInputSRV);
		mCommandList->SetComputeRootDescriptorTable(4, computeShadowMapInputSRV);
		mCommandList->SetComputeRootDescriptorTable(7, computeCubeMapInputSRV);
		mCommandList->SetComputeRootDescriptorTable(8, computeIBLInputSRV);
		mCommandList->SetComputeRootShaderResourceView(5, mLightsBuffer->GetGPUVirtualAddress());
		mCommandList->SetComputeRootShaderResourceView(6, mLightingTilesBuffer->GetGPUVirtualAddress());

		mCommandList->Dispatch(mWidth / 8, mHeight / 8, 1);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBaseColorAndRoughness[frameIndex].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mNormalsAndMetalness[frameIndex].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthTextures[frameIndex].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMapTextures[frameIndex].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	}


	RENDER_PASS("Vertical Blur")
	{
	//	using namespace RootSignature;
	//	CD3DX12_GPU_DESCRIPTOR_HANDLE verticalBlurOutUAV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::VerticalBlurUAV, mSRVDescriptorSize };
	//	CD3DX12_GPU_DESCRIPTOR_HANDLE verticalBlurInSRV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::LightingSRV, mSRVDescriptorSize };
	//	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mComputeOutput[frameIndex].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

		//mCommandList->SetPipelineState(mVerticalBlurPSO.Get());
		//mCommandList->SetComputeRootSignature(mBlurRootSignature.Get());
		//mCommandList->SetComputeRootDescriptorTable(Blur::TextureToBlur, verticalBlurInSRV);
		//mCommandList->SetComputeRootDescriptorTable(Blur::BlurredTexture, verticalBlurOutUAV);
	//	mCommandList->Dispatch(mWidth / 8, mHeight / 8, 1);

	//	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mComputeOutput[frameIndex].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}


	RENDER_PASS("Horizontal Blur")
	{
		//using namespace RootSignature;
		//CD3DX12_GPU_DESCRIPTOR_HANDLE horizontalBlurInSRV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::VerticalBlurSRV, mSRVDescriptorSize };
		//CD3DX12_GPU_DESCRIPTOR_HANDLE horizontalBlurOutUAV{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::LightingUAV, mSRVDescriptorSize };
		//mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mVerticalBlurTexture[frameIndex].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

		//mCommandList->SetPipelineState(mHorizontalBlurPSO.Get());
		//mCommandList->SetComputeRootSignature(mBlurRootSignature.Get());
		//mCommandList->SetComputeRootDescriptorTable(Blur::TextureToBlur, horizontalBlurInSRV);
		//mCommandList->SetComputeRootDescriptorTable(Blur::BlurredTexture, horizontalBlurOutUAV);
		//mCommandList->Dispatch(mWidth / 8, mHeight / 8, 1);

		//mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mVerticalBlurTexture[frameIndex].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}


	RENDER_PASS("Output to backbuffer")
	{
		using namespace RootSignature;
		viewport.Width = (float)mWidth;
		viewport.Height = (float)mHeight;
		scissorRect.right = mWidth;
		scissorRect.bottom = mHeight;

		mCommandList->RSSetViewports(1, &viewport);
		mCommandList->RSSetScissorRects(1, &scissorRect);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mComputeOutput[frameIndex].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		CD3DX12_GPU_DESCRIPTOR_HANDLE renderInSRVs{ mSRVHeap->GetGPUDescriptorHandleForHeapStart(), SRVLayout::Count * (int)frameIndex + SRVLayout::LightingSRV, mSRVDescriptorSize };
		mCommandList->SetPipelineState(mRenderPSO.Get());
		mCommandList->OMSetRenderTargets(1, &mRTVs[frameIndex], TRUE, NULL);
		mCommandList->SetGraphicsRootSignature(mRootSignatureRender.Get());
		ID3D12DescriptorHeap* descriptorHeaps[1] = { mSRVHeap.Get() };
		mCommandList->SetDescriptorHeaps(1, descriptorHeaps);
		mCommandList->SetGraphicsRootDescriptorTable(BackBuffer::CountedLighting, renderInSRVs);
		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mCommandList->DrawInstanced(3, 1, 0, 0);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mComputeOutput[frameIndex].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}

	RENDER_PASS("Debug visualization")
	{
		mCommandList->OMSetRenderTargets(1, &mRTVs[frameIndex], TRUE, &mDSVs[frameIndex]);
		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
		mCommandList->SetGraphicsRoot32BitConstants(0, sizeof(mPerFrame) / 4, &mPerFrame, 0);
		mDebugDrawer.DrawGrid();
		mDebugDrawer.DrawAABB(mLightSource.GetLightPos(), float3(0.2, 0.2, 0.2), float4(1.0, 0.5, 0.0, 1.0));
		mDebugDrawer.Execute(mCommandList.Get());
	}

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBackBufferTextures[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
	mCommandList->Close();

	ID3D12CommandList* commandLists[1] = { mCommandList.Get() };

	mQueue->ExecuteCommandLists((UINT)std::size(commandLists), commandLists);
	mQueue->Signal(mFence.Get(), ++mFenceSignaledValue);
	mSwapChain->Present(0, 0);

	//mCurrFrame = (mFence->GetCompletedValue())%16;
	mCurrFrame++;

}
void Application::Animate(float timeDelta)
{
	if (GetAsyncKeyState(VK_LEFT))
	{
		mLightSource.AdvanceYaw(glm::radians(30.0f) * timeDelta);
	}

	if (GetAsyncKeyState(VK_RIGHT))
	{
		mLightSource.AdvanceYaw(-glm::radians(30.0f) * timeDelta);
	}

	if (GetAsyncKeyState(VK_UP))
	{
		mLightSource.AdvancePitch(glm::radians(30.0f) * timeDelta);
	}

	if (GetAsyncKeyState(VK_DOWN))
	{
		mLightSource.AdvancePitch(-glm::radians(30.0f) * timeDelta);
	}

	if (GetAsyncKeyState('A'))
	{
		mCamera.AdvancePosY(-2 * timeDelta);
	}

	if (GetAsyncKeyState('D'))
	{
		mCamera.AdvancePosY(2 * timeDelta);
	}

	if (GetAsyncKeyState('W'))
	{
		mCamera.AdvancePosX(2 * timeDelta);
	}

	if (GetAsyncKeyState('S'))
	{
		mCamera.AdvancePosX(-2 * timeDelta);
	}

	if (GetAsyncKeyState('E'))
	{
		mCamera.AdvancePosZ(2 * timeDelta);
	}

	if (GetAsyncKeyState('Q'))
	{
		mCamera.AdvancePosZ(-2 * timeDelta);
	}

	if (GetAsyncKeyState('Y'))
	{
		mLightSource.AdvancePosX(2 * timeDelta);
	}
	if (GetAsyncKeyState('H'))
	{
		mLightSource.AdvancePosX(-2 * timeDelta);
	}
	if (GetAsyncKeyState('G'))
	{
		mLightSource.AdvancePosY(2 * timeDelta);
	}
	if (GetAsyncKeyState('J'))
	{
		mLightSource.AdvancePosY(-2 * timeDelta);
	}


	ProcessCameraDrag(timeDelta);
}

void Application::InitWindow()
{
	DWORD dwExStyle = WS_EX_WINDOWEDGE;
	DWORD dwStyle = WS_CAPTION | WS_SYSMENU | WS_VISIBLE;

	WNDCLASSEX wc{};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = L"PoolWindowClass";

	RegisterClassEx(&wc);

	RECT rect{};
	rect.right = mWidth;
	rect.bottom = mHeight;
	AdjustWindowRectEx(&rect, dwStyle, FALSE, dwExStyle);

	int32_t realWidth = rect.right - rect.left;
	int32_t realHeight = rect.bottom - rect.top;

	mWindowHandle = CreateWindowEx(dwExStyle, wc.lpszClassName, L"Pool", dwStyle,
		(GetSystemMetrics(SM_CXSCREEN) - realWidth) / 2,
		(GetSystemMetrics(SM_CYSCREEN) - realHeight) / 2,
		realWidth, realHeight,
		NULL, NULL, wc.hInstance, NULL);

	ShowWindow(mWindowHandle, SW_SHOW);
}

void Application::InitD3D12()
{
	// creating d3d12 device
	D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice));

	// creating command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mQueue));

	// creating swap chain
	ComPtr<IDXGIFactory2> dxgiFactory;
	CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));

	ComPtr<IDXGISwapChain1> swapChain;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = mWidth;
	swapChainDesc.Height = mHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = kNumFramesInFlight;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = 0;
	dxgiFactory->CreateSwapChainForHwnd(mQueue.Get(), mWindowHandle, &swapChainDesc, nullptr, nullptr, &swapChain);
	swapChain->QueryInterface<IDXGISwapChain4>(&mSwapChain);

	// creating command list and command allocator
	mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mAllocator));
	mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mAllocator.Get(), nullptr, IID_PPV_ARGS(&mCommandList));
	mCommandList->Close();

	// creating fence
	mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));

	// getting descriptor sizes
	mSRVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDSVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	// creating RTV descriptor heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapDesc.NumDescriptors = RTVLayout::Count * kNumFramesInFlight;
		mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mRTVHeap));


		for (int32_t i = 0; i < kNumFramesInFlight; ++i)
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvCpuDescriptor{ mRTVHeap->GetCPUDescriptorHandleForHeapStart(), RTVLayout::Count * i + RTVLayout::Backbuffer, mRTVDescriptorSize };
			mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBackBufferTextures[i]));
			mDevice->CreateRenderTargetView(mBackBufferTextures[i].Get(), nullptr, rtvCpuDescriptor);
			mRTVs[i] = rtvCpuDescriptor;
		}
	}

	// creating DSV decriptor heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		heapDesc.NumDescriptors = DSVLayout::Count * kNumFramesInFlight;
		mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDSVHeap));

		D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuDescriptor = mDSVHeap->GetCPUDescriptorHandleForHeapStart();

		for (int32_t i = 0; i < kNumFramesInFlight; ++i)
		{
			D3D12_RESOURCE_DESC resDesc{};
			resDesc.SampleDesc.Count = 1;
			resDesc.SampleDesc.Quality = 0;
			resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, mWidth, mHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;




			D3D12_CLEAR_VALUE clearValue{};
			clearValue.Format = DXGI_FORMAT_D32_FLOAT;
			clearValue.DepthStencil.Depth = 1.0f;
			clearValue.DepthStencil.Stencil = 0;
			mDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&mDepthTextures[i]));

			resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, 2048, 2048, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
			mDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&mShadowMapTextures[i]));

			resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, DivideRoundingUp(mWidth, LightDrawer::kTileSize), DivideRoundingUp(mHeight, LightDrawer::kTileSize), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
			mDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&mLightCullingDepthMax[i]));


			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = 0;

			mDevice->CreateDepthStencilView(mDepthTextures[i].Get(), &dsvDesc, dsvCpuDescriptor);
			mDSVs[i] = dsvCpuDescriptor;
			dsvCpuDescriptor.ptr += mDSVDescriptorSize;

			mDevice->CreateDepthStencilView(mShadowMapTextures[i].Get(), &dsvDesc, dsvCpuDescriptor);
			mShadowMapDSVs[i] = dsvCpuDescriptor;
			dsvCpuDescriptor.ptr += mDSVDescriptorSize;

			mDevice->CreateDepthStencilView(mLightCullingDepthMax[i].Get(), &dsvDesc, dsvCpuDescriptor);
			mLightCullingDepthMaxDSVs[i] = dsvCpuDescriptor;
			dsvCpuDescriptor.ptr += mDSVDescriptorSize;

		}
	}


	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = kNumSRVDescriptors;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mSRVHeap));


		for (int32_t i = 0; i < kNumFramesInFlight; ++i)
		{
			D3D12_RESOURCE_DESC resDesc{};
			resDesc.SampleDesc.Count = 1;
			resDesc.SampleDesc.Quality = 0;
			resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, mWidth, mHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			mDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&mBaseColorAndRoughness[i]));
			mDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&mNormalsAndMetalness[i]));

			// setting debug names so we can see them in PIX
			mBaseColorAndRoughness[i]->SetName(L"BaseColor");
			mNormalsAndMetalness[i]->SetName(L"Normals");

			CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuDescriptor{ mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::Count * i + SRVLayout::GBuffer, mSRVDescriptorSize };
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvCpuDescriptor{ mRTVHeap->GetCPUDescriptorHandleForHeapStart(), RTVLayout::Count * i + RTVLayout::GBuffer, mRTVDescriptorSize };

			mGBufferSRVs[i] = srvCpuDescriptor;

			mDevice->CreateShaderResourceView(mBaseColorAndRoughness[i].Get(), nullptr, srvCpuDescriptor);
			srvCpuDescriptor.ptr += mSRVDescriptorSize;
			mDevice->CreateShaderResourceView(mNormalsAndMetalness[i].Get(), nullptr, srvCpuDescriptor);
			srvCpuDescriptor.ptr += mSRVDescriptorSize;

			mGBufferRTVs[i] = rtvCpuDescriptor;
			mDevice->CreateRenderTargetView(mBaseColorAndRoughness[i].Get(), nullptr, rtvCpuDescriptor);
			rtvCpuDescriptor.ptr += mRTVDescriptorSize;
			mDevice->CreateRenderTargetView(mNormalsAndMetalness[i].Get(), nullptr, rtvCpuDescriptor);
			rtvCpuDescriptor.ptr += mRTVDescriptorSize;

			// #IZ: creating compute output
			resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, mWidth, mHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			mDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mComputeOutput[i]));
			mComputeOutput[i]->SetName(L"Compute output");

			//creating compute output for Gaussian blur

			resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, mWidth, mHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			mDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mVerticalBlurTexture[i]));
			mComputeOutput[i]->SetName(L"Vertical blur texture");

			//creating SRV and UAV for compute output
			srvCpuDescriptor = { mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::Count * i + SRVLayout::LightingUAV, mSRVDescriptorSize };
			mDevice->CreateUnorderedAccessView(mComputeOutput[i].Get(), nullptr, nullptr, srvCpuDescriptor);
			srvCpuDescriptor = { mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::Count * i + SRVLayout::LightingSRV, mSRVDescriptorSize };
			mDevice->CreateShaderResourceView(mComputeOutput[i].Get(), nullptr, srvCpuDescriptor);

			srvCpuDescriptor = { mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::Count * i + SRVLayout::VerticalBlurUAV, mSRVDescriptorSize };
			mDevice->CreateUnorderedAccessView(mVerticalBlurTexture[i].Get(), nullptr, nullptr, srvCpuDescriptor);
			srvCpuDescriptor = { mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::Count * i + SRVLayout::VerticalBlurSRV, mSRVDescriptorSize };
			mDevice->CreateShaderResourceView(mVerticalBlurTexture[i].Get(), nullptr, srvCpuDescriptor);

			// creating SRV for depth
			srvCpuDescriptor = { mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::Count * i + SRVLayout::DepthBufferSRV, mSRVDescriptorSize };
			D3D12_SHADER_RESOURCE_VIEW_DESC dsvDesc{};
			dsvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipLevels = 1;
			dsvDesc.Texture2D.MostDetailedMip = 0;
			dsvDesc.Texture2D.PlaneSlice = 0;
			dsvDesc.Texture2D.ResourceMinLODClamp = 0;
			dsvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			dsvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			mDevice->CreateShaderResourceView(mDepthTextures[i].Get(), &dsvDesc, srvCpuDescriptor);

			//creating SRV for shadow map
			srvCpuDescriptor = { mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::Count * i + SRVLayout::ShadowMapSRV, mSRVDescriptorSize };
			mDevice->CreateShaderResourceView(mShadowMapTextures[i].Get(), &dsvDesc, srvCpuDescriptor);


			//resorces & views for tiled lighting
			resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, DivideRoundingUp(mWidth, LightDrawer::kTileSize), DivideRoundingUp(mHeight, LightDrawer::kTileSize), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
			mDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&mProxySphereOutput[i]));

			resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32_FLOAT, DivideRoundingUp(mWidth, LightDrawer::kTileSize), DivideRoundingUp(mHeight, LightDrawer::kTileSize), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			mDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mDownsampledDepthComputeOutput[i]));



			// setting debug names so we can see them in PIX
			mProxySphereOutput[i]->SetName(L"ProxySphereOutput");
			mProxySphereRTVs[i] = rtvCpuDescriptor;
			mDevice->CreateRenderTargetView(mProxySphereOutput[i].Get(), nullptr, rtvCpuDescriptor);
			rtvCpuDescriptor.ptr += mRTVDescriptorSize;


			srvCpuDescriptor = { mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::Count * i + SRVLayout::DownSampledDepthUAV, mSRVDescriptorSize };
			mDevice->CreateUnorderedAccessView(mDownsampledDepthComputeOutput[i].Get(), nullptr, nullptr, srvCpuDescriptor);

			srvCpuDescriptor = { mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::Count * i + SRVLayout::DownSampledDepthSRV, mSRVDescriptorSize };
			mDevice->CreateShaderResourceView(mDownsampledDepthComputeOutput[i].Get(), nullptr, srvCpuDescriptor);




			resDesc = CD3DX12_RESOURCE_DESC::Buffer(((sizeof(TiledLightingCB) + 255) & ~255) * kNumPointLights); // CB size is required to be 256-byte aligned.
			mDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mUploadConstantBuffer[i]));

			//srvCpuDescriptor = { mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::Count * i + SRVLayout::TiledLightingPixelShaderCBV, mSRVDescriptorSize };
			// D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			//cbvDesc.BufferLocation = mLightPixelConstantBuffer[i]->GetGPUVirtualAddress();
			//cbvDesc.SizeInBytes = resDesc.Width;
			//mDevice->CreateConstantBufferView(&cbvDesc, srvCpuDescriptor);

		}
	}



	InitPipelineState();

	mDebugDrawer.Initialize(mDevice.Get(), mRootSignature.Get());
	mLightDrawer.Initialize(mDevice.Get(), mProxyLightRootSignature.Get());
}

void Application::InitPipelineState()
{
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_DESCRIPTOR_RANGE descriptorRangeTextureSRV{};
	descriptorRangeTextureSRV.BaseShaderRegister = 0;
	descriptorRangeTextureSRV.NumDescriptors = 3;
	descriptorRangeTextureSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	D3D12_ROOT_PARAMETER rootParameters[2];
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[0].Constants.Num32BitValues = sizeof(PerFrameCB) / 4;
	rootParameters[0].Constants.ShaderRegister = 0;
	rootParameters[0].Constants.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRangeTextureSRV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


	D3D12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.NumParameters = ARRAYSIZE(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.pStaticSamplers = &samplerDesc;

	ComPtr<ID3DBlob> serializedRootSignature;
	D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &serializedRootSignature, nullptr);

	mDevice->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS.pShaderBytecode = VertexShader_ShaderData;
	psoDesc.VS.BytecodeLength = std::size(VertexShader_ShaderData);
	psoDesc.PS.pShaderBytecode = PixelShader_ShaderData;
	psoDesc.PS.BytecodeLength = std::size(PixelShader_ShaderData);

	psoDesc.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
	psoDesc.SampleMask = 0xFFFFFFFF;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.InputLayout.NumElements = (UINT)std::size(inputLayout);
	psoDesc.InputLayout.pInputElementDescs = inputLayout;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = kNumGBufferTextures;
	for (uint32_t i = 0; i < kNumGBufferTextures; ++i)
	{
		psoDesc.RTVFormats[i] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	}
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO));

	//shadow map PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoShadowMapDesc{};
	psoShadowMapDesc.pRootSignature = mRootSignature.Get();
	psoShadowMapDesc.VS.pShaderBytecode = VertexShader_ShaderData;
	psoShadowMapDesc.VS.BytecodeLength = std::size(VertexShader_ShaderData);
	psoShadowMapDesc.PS.pShaderBytecode = PixelShader_ShaderData;
	psoShadowMapDesc.PS.BytecodeLength = std::size(PixelShader_ShaderData);

	psoShadowMapDesc.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
	psoShadowMapDesc.SampleMask = 0xFFFFFFFF;
	psoShadowMapDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoShadowMapDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoShadowMapDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoShadowMapDesc.InputLayout.NumElements = (UINT)std::size(inputLayout);
	psoShadowMapDesc.InputLayout.pInputElementDescs = inputLayout;
	psoShadowMapDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoShadowMapDesc.NumRenderTargets = 0;
	psoShadowMapDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoShadowMapDesc.SampleDesc.Count = 1;
	psoShadowMapDesc.SampleDesc.Quality = 0;
	mDevice->CreateGraphicsPipelineState(&psoShadowMapDesc, IID_PPV_ARGS(&mShadowMapPSO));

	//compute PSO

	D3D12_DESCRIPTOR_RANGE descriptorRangeSRV{};
	descriptorRangeSRV.BaseShaderRegister = 0;
	descriptorRangeSRV.NumDescriptors = 2;
	descriptorRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	D3D12_DESCRIPTOR_RANGE descriptorRangeUAV{};
	descriptorRangeUAV.BaseShaderRegister = 0;
	descriptorRangeUAV.NumDescriptors = 1;
	descriptorRangeUAV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	D3D12_DESCRIPTOR_RANGE descriptorRangeDepthSRV{};
	descriptorRangeDepthSRV.BaseShaderRegister = 2;
	descriptorRangeDepthSRV.NumDescriptors = 1;
	descriptorRangeDepthSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	D3D12_DESCRIPTOR_RANGE descriptorShadowMapSRV{};
	descriptorShadowMapSRV.BaseShaderRegister = 3;
	descriptorShadowMapSRV.NumDescriptors = 1;
	descriptorShadowMapSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	D3D12_DESCRIPTOR_RANGE descriptorCubeMapSRV{};
	descriptorCubeMapSRV.BaseShaderRegister = 4;
	descriptorCubeMapSRV.NumDescriptors = 1;
	descriptorCubeMapSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	D3D12_DESCRIPTOR_RANGE descriptorIBLLightingSRV{};
	descriptorIBLLightingSRV.BaseShaderRegister = 5;
	descriptorIBLLightingSRV.NumDescriptors = 3;
	descriptorIBLLightingSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;


	D3D12_ROOT_PARAMETER rootParametersCompute[9]{};
	rootParametersCompute[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersCompute[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParametersCompute[0].DescriptorTable.pDescriptorRanges = &descriptorRangeSRV;
	rootParametersCompute[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParametersCompute[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersCompute[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParametersCompute[1].DescriptorTable.pDescriptorRanges = &descriptorRangeUAV;
	rootParametersCompute[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParametersCompute[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParametersCompute[2].Constants.Num32BitValues = sizeof(PerFrameComputeCB) / 4;
	rootParametersCompute[2].Constants.ShaderRegister = 0;
	rootParametersCompute[2].Constants.RegisterSpace = 0;
	rootParametersCompute[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParametersCompute[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersCompute[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParametersCompute[3].DescriptorTable.pDescriptorRanges = &descriptorRangeDepthSRV;
	rootParametersCompute[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParametersCompute[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersCompute[4].DescriptorTable.NumDescriptorRanges = 1;
	rootParametersCompute[4].DescriptorTable.pDescriptorRanges = &descriptorShadowMapSRV;
	rootParametersCompute[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// creating parameter for StructuredBuffer<Light> PointLights : register(t34); in ResolveLighting.hlsl
	// no need to create descriptor for buffer
	rootParametersCompute[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParametersCompute[5].Descriptor.ShaderRegister = 34;
	rootParametersCompute[5].Descriptor.RegisterSpace = 0;
	rootParametersCompute[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParametersCompute[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParametersCompute[6].Descriptor.ShaderRegister = 69;
	rootParametersCompute[6].Descriptor.RegisterSpace = 0;
	rootParametersCompute[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParametersCompute[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersCompute[7].DescriptorTable.NumDescriptorRanges = 1;
	rootParametersCompute[7].DescriptorTable.pDescriptorRanges = &descriptorCubeMapSRV;
	rootParametersCompute[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParametersCompute[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersCompute[8].DescriptorTable.NumDescriptorRanges = 1;
	rootParametersCompute[8].DescriptorTable.pDescriptorRanges = &descriptorIBLLightingSRV;
	rootParametersCompute[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;



	D3D12_STATIC_SAMPLER_DESC resolveLightingSamplers[2];
	resolveLightingSamplers[0] = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_POINT);
	resolveLightingSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	resolveLightingSamplers[0].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;

	resolveLightingSamplers[1] = CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT);

	D3D12_ROOT_SIGNATURE_DESC rootSignatureComputeDesc{};
	rootSignatureComputeDesc.NumParameters = ARRAYSIZE(rootParametersCompute);
	rootSignatureComputeDesc.pParameters = rootParametersCompute;
	rootSignatureComputeDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rootSignatureComputeDesc.NumStaticSamplers = std::size(resolveLightingSamplers);
	rootSignatureComputeDesc.pStaticSamplers = resolveLightingSamplers;

	ID3DBlob* rootSignatureBlob = nullptr;
	ID3DBlob* rootSignatureErrors = nullptr;
	auto b = D3D12SerializeRootSignature(&rootSignatureComputeDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, &rootSignatureErrors);

	if (FAILED(b))
	{
		MessageBoxA(GetForegroundWindow(), (LPCSTR)rootSignatureErrors->GetBufferPointer(), "", 0);
	}
	mDevice->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&mComputeRootSignature));

	D3D12_RASTERIZER_DESC rastDesc{};
	rastDesc.CullMode = D3D12_CULL_MODE_FRONT;

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoCompDesc{};
	psoCompDesc.pRootSignature = mComputeRootSignature.Get();
	psoCompDesc.CS.pShaderBytecode = ResolveLighting_ShaderData;
	psoCompDesc.CS.BytecodeLength = ARRAYSIZE(ResolveLighting_ShaderData);
	mDevice->CreateComputePipelineState(&psoCompDesc, IID_PPV_ARGS(&mComputePSO));

	//PSO for back buffer rendering
	{
		using namespace RootSignature;

		D3D12_DESCRIPTOR_RANGE descriptorRangeSRV{};
		descriptorRangeSRV.BaseShaderRegister = 0;
		descriptorRangeSRV.NumDescriptors = 1;
		descriptorRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

		D3D12_ROOT_PARAMETER rootParameters[BackBuffer::Count];
		rootParameters[BackBuffer::CountedLighting].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[BackBuffer::CountedLighting].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[BackBuffer::CountedLighting].DescriptorTable.pDescriptorRanges = &descriptorRangeSRV;
		rootParameters[BackBuffer::CountedLighting].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = ARRAYSIZE(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		ID3DBlob* rootSignatureBlob = nullptr;
		auto a = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, nullptr);
		mDevice->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&mRootSignatureRender));

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = mRootSignatureRender.Get();
		psoDesc.VS.pShaderBytecode = FullScreenTriangle_ShaderData;
		psoDesc.VS.BytecodeLength = std::size(FullScreenTriangle_ShaderData);
		psoDesc.PS.pShaderBytecode = LightingToScreen_ShaderData;
		psoDesc.PS.BytecodeLength = std::size(LightingToScreen_ShaderData);

		psoDesc.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
		psoDesc.SampleMask = 0xFFFFFFFF;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.InputLayout.NumElements = NULL;
		psoDesc.InputLayout.pInputElementDescs = NULL;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;
		mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mRenderPSO));
	}

	//Compute PSO blur effect
	{
		using namespace RootSignature;

		D3D12_DESCRIPTOR_RANGE descriptorRangeSRV{};
		descriptorRangeSRV.BaseShaderRegister = 0;
		descriptorRangeSRV.NumDescriptors = 1;
		descriptorRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

		D3D12_DESCRIPTOR_RANGE descriptorRangeUAV{};
		descriptorRangeUAV.BaseShaderRegister = 0;
		descriptorRangeUAV.NumDescriptors = 1;
		descriptorRangeUAV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

		D3D12_ROOT_PARAMETER rootParameters[Blur::Count]{};
		rootParameters[Blur::TextureToBlur].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[Blur::TextureToBlur].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[Blur::TextureToBlur].DescriptorTable.pDescriptorRanges = &descriptorRangeSRV;
		rootParameters[Blur::TextureToBlur].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters[Blur::BlurredTexture].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[Blur::BlurredTexture].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[Blur::BlurredTexture].DescriptorTable.pDescriptorRanges = &descriptorRangeUAV;
		rootParameters[Blur::BlurredTexture].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureBlurDesc{};
		rootSignatureBlurDesc.NumParameters = ARRAYSIZE(rootParameters);
		rootSignatureBlurDesc.pParameters = rootParameters;
		rootSignatureBlurDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		b = D3D12SerializeRootSignature(&rootSignatureBlurDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, &rootSignatureErrors);

		if (FAILED(b))
		{
			MessageBoxA(GetForegroundWindow(), (LPCSTR)rootSignatureErrors->GetBufferPointer(), "", 0);
		}
		mDevice->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&mBlurRootSignature));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoVertcalBlurDesc{};
		psoVertcalBlurDesc.pRootSignature = mBlurRootSignature.Get();
		psoVertcalBlurDesc.CS.pShaderBytecode = VerticalBlur_ShaderData;
		psoVertcalBlurDesc.CS.BytecodeLength = ARRAYSIZE(VerticalBlur_ShaderData);
		mDevice->CreateComputePipelineState(&psoVertcalBlurDesc, IID_PPV_ARGS(&mVerticalBlurPSO));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoHorizontalBlurDesc{};
		psoHorizontalBlurDesc.pRootSignature = mBlurRootSignature.Get();
		psoHorizontalBlurDesc.CS.pShaderBytecode = HorizontalBlur_ShaderData;
		psoHorizontalBlurDesc.CS.BytecodeLength = ARRAYSIZE(HorizontalBlur_ShaderData);
		mDevice->CreateComputePipelineState(&psoHorizontalBlurDesc, IID_PPV_ARGS(&mHorizontalBlurPSO));
	}

	//root signature for proxy light render

	{
		using namespace RootSignature;



		D3D12_DESCRIPTOR_RANGE descriptorRangeSRV{};
		descriptorRangeSRV.BaseShaderRegister = 0;
		descriptorRangeSRV.NumDescriptors = 1;
		descriptorRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

		//D3D12_DESCRIPTOR_RANGE descriptorRangeCBV{};
		//descriptorRangeCBV.BaseShaderRegister = 1;
		//descriptorRangeCBV.NumDescriptors = 1;
		//descriptorRangeCBV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

		D3D12_DESCRIPTOR_RANGE descriptorRangeTransMatrixCBV{};
		descriptorRangeTransMatrixCBV.BaseShaderRegister = 2;
		descriptorRangeTransMatrixCBV.NumDescriptors = 1;
		descriptorRangeTransMatrixCBV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

		D3D12_DESCRIPTOR_RANGE descriptorRangePerLightCBV{};
		descriptorRangePerLightCBV.BaseShaderRegister = 3;
		descriptorRangePerLightCBV.NumDescriptors = 1;
		descriptorRangePerLightCBV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;



		D3D12_ROOT_PARAMETER rootParameters[ProxyLight::Count];
		rootParameters[ProxyLight::VertexConstantBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameters[ProxyLight::VertexConstantBuffer].Constants.Num32BitValues = sizeof(PerLightCB) / 4;
		rootParameters[ProxyLight::VertexConstantBuffer].Constants.ShaderRegister = 0;
		rootParameters[ProxyLight::VertexConstantBuffer].Constants.RegisterSpace = 0;
		rootParameters[ProxyLight::VertexConstantBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters[ProxyLight::PixelConstantBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameters[ProxyLight::PixelConstantBuffer].Constants.Num32BitValues = sizeof(TiledLightingCB) / 4;
		rootParameters[ProxyLight::PixelConstantBuffer].Constants.ShaderRegister = 1;
		rootParameters[ProxyLight::PixelConstantBuffer].Constants.RegisterSpace = 0;
		rootParameters[ProxyLight::PixelConstantBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		//rootParameters[ProxyLight::PixelConstantBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		//rootParameters[ProxyLight::PixelConstantBuffer].DescriptorTable.NumDescriptorRanges = 1;
		//rootParameters[ProxyLight::PixelConstantBuffer].DescriptorTable.pDescriptorRanges = &descriptorRangeCBV;
		//rootParameters[ProxyLight::PixelConstantBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		rootParameters[ProxyLight::TransMatrixConstantBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[ProxyLight::TransMatrixConstantBuffer].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[ProxyLight::TransMatrixConstantBuffer].DescriptorTable.pDescriptorRanges = &descriptorRangeTransMatrixCBV;
		rootParameters[ProxyLight::TransMatrixConstantBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters[ProxyLight::PerLightConstantBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[ProxyLight::PerLightConstantBuffer].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[ProxyLight::PerLightConstantBuffer].DescriptorTable.pDescriptorRanges = &descriptorRangePerLightCBV;
		rootParameters[ProxyLight::PerLightConstantBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		rootParameters[ProxyLight::LightingTilesBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		rootParameters[ProxyLight::LightingTilesBuffer].Descriptor.ShaderRegister = 0;
		rootParameters[ProxyLight::LightingTilesBuffer].Descriptor.RegisterSpace = 0;
		rootParameters[ProxyLight::LightingTilesBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		rootParameters[ProxyLight::MinMaxDepths].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[ProxyLight::MinMaxDepths].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[ProxyLight::MinMaxDepths].DescriptorTable.pDescriptorRanges = &descriptorRangeSRV;
		rootParameters[ProxyLight::MinMaxDepths].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;



		D3D12_ROOT_SIGNATURE_DESC rootSignatureProxyLightDesc{};
		rootSignatureProxyLightDesc.NumParameters = ARRAYSIZE(rootParameters);
		rootSignatureProxyLightDesc.pParameters = rootParameters;
		rootSignatureProxyLightDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSignatureProxyLightDesc.NumStaticSamplers = 0;


		ComPtr<ID3DBlob> serializedRootSignatureLight;
		D3D12SerializeRootSignature(&rootSignatureProxyLightDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &serializedRootSignatureLight, nullptr);
		mDevice->CreateRootSignature(0, serializedRootSignatureLight->GetBufferPointer(), serializedRootSignatureLight->GetBufferSize(), IID_PPV_ARGS(&mProxyLightRootSignature));
	}

	// UAVClear
	{
		using namespace RootSignature;
		D3D12_ROOT_PARAMETER rootParameters[UAVClear::Count];

		rootParameters[UAVClear::ConstantBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameters[UAVClear::ConstantBuffer].Constants.Num32BitValues = 1;
		rootParameters[UAVClear::ConstantBuffer].Constants.RegisterSpace = 0;
		rootParameters[UAVClear::ConstantBuffer].Constants.ShaderRegister = 0;
		rootParameters[UAVClear::ConstantBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters[UAVClear::BufferToClear].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		rootParameters[UAVClear::BufferToClear].Descriptor.ShaderRegister = 0;
		rootParameters[UAVClear::BufferToClear].Descriptor.RegisterSpace = 0;
		rootParameters[UAVClear::BufferToClear].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;



		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = ARRAYSIZE(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		rootSignatureDesc.NumStaticSamplers = 0;

		ComPtr<ID3DBlob> serializedRootSignature;
		D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &serializedRootSignature, nullptr);
		mDevice->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&mUAVClearRootSignature));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = mUAVClearRootSignature.Get();
		psoDesc.CS.pShaderBytecode = UAVClear_ShaderData;
		psoDesc.CS.BytecodeLength = std::size(UAVClear_ShaderData);

		mDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mUAVClearPSO));
	}
	//DownSampling depth 
	{
		using namespace RootSignature;
		D3D12_ROOT_PARAMETER rootParameters[DepthDownsampling::Count];

		D3D12_DESCRIPTOR_RANGE descriptorRangeSRV{};
		descriptorRangeSRV.BaseShaderRegister = 0;
		descriptorRangeSRV.NumDescriptors = 1;
		descriptorRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

		D3D12_DESCRIPTOR_RANGE descriptorRangeUAV{};
		descriptorRangeUAV.BaseShaderRegister = 0;
		descriptorRangeUAV.NumDescriptors = 1;
		descriptorRangeUAV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;


		rootParameters[DepthDownsampling::DownsampledTexture].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[DepthDownsampling::DownsampledTexture].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[DepthDownsampling::DownsampledTexture].DescriptorTable.pDescriptorRanges = &descriptorRangeUAV;
		rootParameters[DepthDownsampling::DownsampledTexture].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters[DepthDownsampling::TextureToDownsample].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[DepthDownsampling::TextureToDownsample].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[DepthDownsampling::TextureToDownsample].DescriptorTable.pDescriptorRanges = &descriptorRangeSRV;
		rootParameters[DepthDownsampling::TextureToDownsample].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = ARRAYSIZE(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		rootSignatureDesc.NumStaticSamplers = 0;

		ComPtr<ID3DBlob> serializedRootSignature;
		D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &serializedRootSignature, nullptr);
		mDevice->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&mDownsamplingRootSignature));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = mDownsamplingRootSignature.Get();
		psoDesc.CS.pShaderBytecode = Down_ShaderData;
		psoDesc.CS.BytecodeLength = std::size(Down_ShaderData);

		mDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mDownsamplingPSO));
	}

	//copy depth
	{
		using namespace RootSignature;

		D3D12_DESCRIPTOR_RANGE descriptorRangeSRV{};
		descriptorRangeSRV.BaseShaderRegister = 0;
		descriptorRangeSRV.NumDescriptors = 1;
		descriptorRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

		D3D12_ROOT_PARAMETER rootParameters[CopyDepth::Count];
		rootParameters[CopyDepth::HiZ].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[CopyDepth::HiZ].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[CopyDepth::HiZ].DescriptorTable.pDescriptorRanges = &descriptorRangeSRV;
		rootParameters[CopyDepth::HiZ].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = ARRAYSIZE(rootParameters);
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		ID3DBlob* rootSignatureBlob = nullptr;
		auto a = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, nullptr);
		mDevice->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&mCopyDepthRootSignature));

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = mCopyDepthRootSignature.Get();
		psoDesc.VS.pShaderBytecode = FullScreenTriangle_ShaderData;
		psoDesc.VS.BytecodeLength = std::size(FullScreenTriangle_ShaderData);
		psoDesc.PS.pShaderBytecode = CopyDepth_ShaderData;
		psoDesc.PS.BytecodeLength = std::size(CopyDepth_ShaderData);

		psoDesc.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
		psoDesc.SampleMask = 0xFFFFFFFF;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.InputLayout.NumElements = NULL;
		psoDesc.InputLayout.pInputElementDescs = NULL;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 0;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;
		mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mCopyDepthPSO));
	}
}

void Application::InitMeshes()
{
	std::string meshNames[] =
	{
		"..\\Scene\\OBJ\\chest_1.obj",
		"..\\Scene\\OBJ\\chest_2.obj",
		"..\\Scene\\OBJ\\Ground.obj",
		"..\\Scene\\OBJ\\Props.obj",
		"..\\Scene\\OBJ\\Snake.obj"
	};

	std::string shaderBallMesh = "..\\Scene\\OBJ\\ShaderBalls.obj";
	size_t numObjects = 7;

	for (size_t i = 0; i < numObjects; ++i)
	{
		std::ifstream ifs(shaderBallMesh);
		Mesh mesh;

		mesh.LoadFromFile(&ifs, i);
		//mesh.LoadFromFile(meshNames[i]);

		mRenderMeshes.emplace_back();
		RenderMesh& renderMesh = mRenderMeshes.back();

		//mMesh.LoadFromFile("..\\cubes.obj");
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		mDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(mesh.mVB.size() * sizeof(Vertex)), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&renderMesh.mVertexBuffer));

		void* mappedData = nullptr;
		renderMesh.mVertexBuffer->Map(0, nullptr, &mappedData);
		memcpy(mappedData, mesh.mVB.data(), mesh.mVB.size() * sizeof(Vertex));
		renderMesh.mVertexBuffer->Unmap(0, nullptr);

		renderMesh.mVertexBufferView.BufferLocation = renderMesh.mVertexBuffer->GetGPUVirtualAddress();
		renderMesh.mSize = mesh.mVB.size();
		renderMesh.mVertexBufferView.SizeInBytes = (UINT)(renderMesh.mSize * sizeof(Vertex));
		renderMesh.mVertexBufferView.StrideInBytes = sizeof(Vertex);


	}

	//adding to VB proxy sphere 
	std::string pointLightMesh = "..\\Scene\\OBJ\\ProxyLight.obj";
	std::ifstream ifs(pointLightMesh);
	Mesh lightMesh;
	lightMesh.LoadFromFile(&ifs, 0);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	mDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(lightMesh.mVB.size() * sizeof(Vertex)), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mLightRenderMesh.mVertexBuffer));

	void* mappedData = nullptr;
	mLightRenderMesh.mVertexBuffer->Map(0, nullptr, &mappedData);
	memcpy(mappedData, lightMesh.mVB.data(), lightMesh.mVB.size() * sizeof(Vertex));
	mLightRenderMesh.mVertexBuffer->Unmap(0, nullptr);

	mLightRenderMesh.mVertexBufferView.BufferLocation = mLightRenderMesh.mVertexBuffer->GetGPUVirtualAddress();
	mLightRenderMesh.mSize = lightMesh.mVB.size();
	mLightRenderMesh.mVertexBufferView.SizeInBytes = (UINT)(mLightRenderMesh.mSize * sizeof(Vertex));
	mLightRenderMesh.mVertexBufferView.StrideInBytes = sizeof(Vertex);

}

void Application::InitTextures()
{
	const wchar_t* textureNames[] =
	{
		L"..\\Scene\\ShaderBall_DDS\\T_Tile_White_D.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Tile_White_N.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Tile_White_R.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Beige_D.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Beige_N.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Beige_R.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Cinder_D.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Cinder_N.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Cinder_R.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Concrete_D.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Concrete_N.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Concrete_R.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Yellow_D.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Yellow_N.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Brick_Yellow_R.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Concrete_Shimizu_D.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Concrete_Shimizu_N.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Concrete_Shimizu_R.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Glass_D.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Glass_N.dds",
		L"..\\Scene\\ShaderBall_DDS\\T_Glass_R.dds",
		L"..\\Scene\\ShaderBall_DDS\\cubemap.dds",
		L"..\\Scene\\ShaderBall_DDS\\cubemapDiffuseHDR.dds",
		L"..\\Scene\\ShaderBall_DDS\\cubemapSpecularHDR.dds",
		L"..\\Scene\\ShaderBall_DDS\\BRDF.dds"
	};

	using namespace DirectX;

	for (size_t i = 0; i < std::size(textureNames); ++i)
	{
		std::unique_ptr<uint8_t[]> ddsData;
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		ID3D12Resource* tex;

		//LoadDDSTextureFromFile(mDevice.Get(), L"..\\Scene\\Textures_DDS\\snake.dds", &tex, ddsData, subresources);
		bool isCubeMap = false;
		LoadDDSTextureFromFile(mDevice.Get(), textureNames[i], &tex, ddsData, subresources, 0, nullptr, &isCubeMap);
		assert(tex != nullptr && "Can't load the texture");

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex, 0, static_cast<UINT>(subresources.size()));

		// Create the GPU upload buffer.
		ComPtr<ID3D12Resource> uploadHeap;
		mDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadHeap.GetAddressOf()));

		mAllocator->Reset();
		mCommandList->Reset(mAllocator.Get(), nullptr);
		UpdateSubresources(mCommandList.Get(), tex, uploadHeap.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(tex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		mCommandList->Close();
		mQueue->ExecuteCommandLists(1, CommandListCast(mCommandList.GetAddressOf()));
		mQueue->Signal(mFence.Get(), ++mFenceSignaledValue);
		WaitForFrame();
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle{ mSRVHeap->GetCPUDescriptorHandleForHeapStart(), (INT)(kTextureSRVsOffset + i), mSRVDescriptorSize };

		if (isCubeMap)
		{
			D3D12_RESOURCE_DESC texDesc = tex->GetDesc();
			
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = texDesc.Format;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels = texDesc.MipLevels;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

			mDevice->CreateShaderResourceView(tex, &srvDesc, srvHandle);
		}
		else
		{
			mDevice->CreateShaderResourceView(tex, nullptr, srvHandle);
		}

		++mNumTexturesLoaded;
	}
}

void Application::InitLighting()
{
	// initializing lights
	D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(kNumPointLights * sizeof(PointLight));
	mDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mLightsBuffer));

	mLights = nullptr;
	mLightsBuffer->Map(0, nullptr, (void**)&mLights);

	constexpr float kStep = glm::two_pi<float>() / (float)kNumPointLights;

	for (uint32_t i = 0; i < kNumPointLights; ++i)
	{
		float alpha = i * kStep;

		PointLight& light = mLights[i];
		light.position = float3(4.0f * cos(alpha), 4.0f * sin(alpha), 0.5f);
		light.radius = 2.0f;
		light.color = (((rand() % 255) & 0xFF) << 16) |
			(((rand() % 255) & 0xFF) << 15) |
			(((rand() % 255) & 0xFF) << 8) |
			((rand() % 255) & 0xFF);
	}

	mLightsBuffer->Unmap(0, nullptr);

	// creating buffer for lighting tiles

	uint32_t numTilesX = DivideRoundingUp(mWidth, TILED_LIGHTING_TILE_SIZE);
	uint32_t numTilesY = DivideRoundingUp(mHeight, TILED_LIGHTING_TILE_SIZE);

	resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(numTilesX * numTilesY * (kNumPointLights + 1) * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	mDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mLightingTilesBuffer));

	resourceDesc = CD3DX12_RESOURCE_DESC::Buffer((kNumPointLights * sizeof(float4x4)+255)&~255);
	mDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mMatTransConstantBuffer));

	mMatTransArray = nullptr;
	mMatTransConstantBuffer->Map(0, nullptr, (void**)&mMatTransArray);

	for (uint32_t i = 0; i < kNumPointLights; ++i)
	{
		float4x4& matTrans = mMatTransArray[i];
		matTrans = glm::translate(glm::mat4(1.0), mLights[i].position)* glm::scale(float3(mLights[i].radius));
	}

	mMatTransConstantBuffer->Unmap(0, nullptr);
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuDescriptor = { mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::TransMatrixCBV, mSRVDescriptorSize };
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = mMatTransConstantBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = resourceDesc.Width;
	mDevice->CreateConstantBufferView(&cbvDesc, srvCpuDescriptor);

	resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(kNumPointLights * sizeof(PerLightPixelCB));
	mDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mPerLightPixelConstantBuffer));
	mPerLightCBArray = nullptr;
	mPerLightPixelConstantBuffer->Map(0, nullptr, (void**)&mPerLightCBArray);

	for (uint32_t i = 0; i < kNumPointLights; ++i)
	{
		PerLightPixelCB& pixelCB = mPerLightCBArray[i];
		pixelCB.Position = mLights[i].position;
		pixelCB.Radius = mLights[i].radius;

	}
	mPerLightPixelConstantBuffer->Unmap(0, nullptr);

	srvCpuDescriptor = { mSRVHeap->GetCPUDescriptorHandleForHeapStart(), SRVLayout::PerLightPixelCBV, mSRVDescriptorSize };
	cbvDesc = {};
	cbvDesc.BufferLocation = mPerLightPixelConstantBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = resourceDesc.Width;
	mDevice->CreateConstantBufferView(&cbvDesc, srvCpuDescriptor);
}

void Application::WaitForFrame()
{
	while (mFence->GetCompletedValue() < mFenceSignaledValue)
	{
		SwitchToThread();
	}
}

void Application::StartCameraDrag()
{
	while (!ShowCursor(FALSE));
	GetCursorPos(&mLastCursorPos);
	mIsCameraDrag = true;
}

void Application::StopCameraDrag()
{
	while (ShowCursor(TRUE));
	mIsCameraDrag = false;
}

void Application::ProcessCameraDrag(float timeDelta)
{
	if (!mIsCameraDrag)
	{
		return;
	}

	POINT currentCursorPos;
	GetCursorPos(&currentCursorPos);
	long deltaX = currentCursorPos.x - mLastCursorPos.x;
	long deltaY = currentCursorPos.y - mLastCursorPos.y;
	SetCursorPos(mLastCursorPos.x, mLastCursorPos.y);
	const float kCameraFactor = timeDelta * glm::radians(60.0f);

	if (abs(deltaX) > 0)
	{
		mCamera.AdvanceYaw(-kCameraFactor * deltaX);
	}

	if (abs(deltaY) > 0)
	{
		mCamera.AdvancePitch(-kCameraFactor * deltaY);
	}
}

LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
	{
		PostQuitMessage(0);
		return 1;
	}

	case WM_RBUTTONDOWN:
	{
		GApplication->StartCameraDrag();
		return 1;
	}

	case WM_RBUTTONUP:
	{
		GApplication->StopCameraDrag();
		return 1;
	}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
