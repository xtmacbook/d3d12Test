#pragma once
#include "UploadBuffer.h"

#include <memory>
#include <unordered_map>
#include <vector>

struct BlurWeights
{
	int gBlurRadius;

	// Support up to 11 blur weights.
	float w0;
	float w1;
	float w2;
	float w3;
	float w4;
	float w5;
	float w6;
	float w7;
	float w8;
	float w9;
	float w10;
};


class BlurFilter
{
public:

	BlurFilter(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format);

	void OnResize(UINT newWidth, UINT newHeight);

	void BuildResources();
	void BuildDescriptors();

	void BuildCSShader();
	void BuildRootSignature();
	void BuildComputePipeLineState();

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
		UINT descriptorSize);

	void Execute(ID3D12GraphicsCommandList* cmdList,
		ID3D12Resource* input,
		int blurCount);

	ID3D12Resource* Output();

private:
	std::vector<float> CalcGaussWeights(float sigma);
private:
	UINT mWidth = 0;
	UINT mHeight = 0;

	ID3D12Device* md3dDevice = nullptr;
	DXGI_FORMAT		mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap1;
	D3D12_RESOURCE_STATES mBlurMap0State = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES mBlurMap1State = D3D12_RESOURCE_STATE_COMMON;

	std::unique_ptr<UploadBuffer<BlurWeights> >         m_WeightsCB;
	Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_BlurSignature = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>	m_BlurShader;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_BlurPSOs;


	CD3DX12_CPU_DESCRIPTOR_HANDLE			mBlur0CpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE			mBlur0CpuUav;

	CD3DX12_CPU_DESCRIPTOR_HANDLE			mBlur1CpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE			mBlur1CpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE			mBlur0GpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE			mBlur0GpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE			mBlur1GpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE			mBlur1GpuUav;
};