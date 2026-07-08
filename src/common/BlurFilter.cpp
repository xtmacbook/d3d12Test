#include "BlurFilter.h"

#include "context/interface/TexContextInterface.h"

#include <assert.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

BlurFilter::BlurFilter(ID3D12Device* device, UINT width,
	UINT height, DXGI_FORMAT format)
{
	md3dDevice = device;

	mWidth = width;
	mHeight = height;
	mFormat = format;

	BuildResources();
}

void BlurFilter::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth = newWidth;
		mHeight = newHeight;
		// Rebuild the off-screen texture resource with new dimensions.
		BuildResources();
		// New resources, so we need new descriptors to that resource.
		BuildDescriptors();
	}
}

void BlurFilter::BuildDescriptors()
{
	TextureOutDes desc;
	desc.Width = mWidth;
	desc.Height = mHeight;
	desc.Format = mFormat;

	TextureOutResouce outRes0{ mBlurMap0, mBlur0CpuSrv, mBlur0CpuUav };
	TextureOutResouce outRes1{ mBlurMap1, mBlur1CpuSrv, mBlur1CpuUav };

	TexContextInterface::BuildUAVTextureResouceView(md3dDevice, desc, mBlurMap0, mBlur0CpuSrv, mBlur0CpuUav);
	TexContextInterface::BuildUAVTextureResouceView(md3dDevice, desc, mBlurMap1, mBlur1CpuSrv, mBlur1CpuUav);

}

void BlurFilter::BuildCSShader()
{
	m_BlurShader["horzBlurCS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/Blur.hlsl", nullptr, "HorzBlurCS", "cs_5_1");
	m_BlurShader["vertBlurCS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/Blur.hlsl", nullptr, "VertBlurCS", "cs_5_1");
}

void BlurFilter::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	slotRootParameter[0].InitAsConstants(12, 0);
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable);
	slotRootParameter[2].InitAsDescriptorTable(1, &uavTable);

	D3D12_ROOT_SIGNATURE_DESC descRootSignature;
	descRootSignature.NumStaticSamplers = 0;
	descRootSignature.pStaticSamplers = nullptr;
	descRootSignature.pParameters = slotRootParameter;
	descRootSignature.NumParameters = _countof(slotRootParameter);
	descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_BlurSignature.GetAddressOf())));
}

void BlurFilter::BuildComputePipeLineState()
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	ZeroMemory(&computePsoDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	computePsoDesc.pRootSignature = m_BlurSignature.Get();
	computePsoDesc.CS = {
		reinterpret_cast<BYTE*>(m_BlurShader["horzBlurCS"]->GetBufferPointer()),
		m_BlurShader["horzBlurCS"]->GetBufferSize()
	};
	computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_BlurPSOs["horzBlur"])));

	computePsoDesc.CS = {
		reinterpret_cast<BYTE*>(m_BlurShader["vertBlurCS"]->GetBufferPointer()),
		m_BlurShader["vertBlurCS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_BlurPSOs["vertBlur"])));
}

void BlurFilter::BuildResources()
{
	TextureOutDes desc;
	desc.Width = mWidth;
	desc.Height = mHeight;
	desc.Format = mFormat;

	TexContextInterface::BuildUAVTextureResouce(md3dDevice, desc,  mBlurMap0 );
	TexContextInterface::BuildUAVTextureResouce(md3dDevice, desc,  mBlurMap1);
}

void BlurFilter::BuildDescriptors(
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
	UINT descriptorSize)
{
	mBlur0CpuSrv = hCpuDescriptor;
	mBlur0CpuUav = hCpuDescriptor.Offset(1, descriptorSize);
	
	mBlur1CpuSrv = hCpuDescriptor.Offset(1, descriptorSize);
	mBlur1CpuUav = hCpuDescriptor.Offset(1, descriptorSize);

	mBlur0GpuSrv = hGpuDescriptor;
	mBlur0GpuUav = hGpuDescriptor.Offset(1, descriptorSize);
	
	mBlur1GpuSrv = hGpuDescriptor.Offset(1, descriptorSize);
	mBlur1GpuUav = hGpuDescriptor.Offset(1, descriptorSize);

	BuildDescriptors();
}

void BlurFilter::Execute(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* input, 
	int blurCount)
{
	std::vector<float> weights = CalcGaussWeights(2.5f);
	int blurRadius = (int)weights.size() / 2;
	cmdList->SetComputeRootSignature(m_BlurSignature.Get());
	cmdList->SetComputeRoot32BitConstants(0, 1, &blurRadius, 0); //此处是使用的函数和之前的都不同，不需要buffer view
	cmdList->SetComputeRoot32BitConstants(0, (UINT)weights.size(), weights.
		data(), 1);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(input,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.
		Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	cmdList->CopyResource(mBlurMap0.Get(), input);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.
		Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.
		Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	for (int i = 0; i < blurCount; ++i)
	{
		cmdList->SetPipelineState(m_BlurPSOs["horzBlur"].Get());
		cmdList->SetComputeRootDescriptorTable(1, mBlur0GpuSrv);
		cmdList->SetComputeRootDescriptorTable(2, mBlur1GpuUav);
		// How many groups do we need to dispatch to cover a row of pixels, where
		// each group covers 256 pixels (the 256 is defined in the ComputeShader).
		
		UINT numGroupsX = (UINT)ceilf(mWidth / 256.0f);

		cmdList->Dispatch(numGroupsX, mHeight, 1);

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			mBlurMap0.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			mBlurMap1.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_GENERIC_READ));
		//
		// Vertical Blur pass.
		//
		cmdList->SetPipelineState(m_BlurPSOs["vertBlur"].Get());
		cmdList->SetComputeRootDescriptorTable(1, mBlur1GpuSrv);
		cmdList->SetComputeRootDescriptorTable(2, mBlur0GpuUav);
		// How many groups do we need to dispatch to cover a column of pixels,
		// where each group covers 256 pixels (the 256 is defined in the
		// ComputeShader).
		UINT numGroupsY = (UINT)ceilf(mHeight / 256.0f);
		cmdList->Dispatch(mWidth, numGroupsY, 1);

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			mBlurMap0.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_GENERIC_READ));
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			mBlurMap1.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}

}

ID3D12Resource* BlurFilter::Output()
{
	return mBlurMap0.Get();
}

std::vector<float> BlurFilter::CalcGaussWeights(float sigma)
{
	float twoSigma2 = 2.0f * sigma * sigma;

	// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
	// For example, for sigma = 3, the width of the bell curve is 
	int blurRadius = (int)ceil(2.0f * sigma);

	assert(blurRadius <= 5);

	std::vector<float> weights;
	weights.resize(2 * blurRadius + 1);

	float weightSum = 0.0f;

	for (int i = -blurRadius; i <= blurRadius; ++i)
	{
		float x = (float)i;

		weights[i + blurRadius] = expf(-x * x / twoSigma2);

		weightSum += weights[i + blurRadius];
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (int i = 0; i < weights.size(); ++i)
	{
		weights[i] /= weightSum;
	}

	return weights;
}

