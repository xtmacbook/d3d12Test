/**
 * we would have a filtering scheme that allows us to choose the filter size 
 * dynamically per pixel, ideally at constant cost and without dynamic branching. 
 * This capability is exactly what summed-area tables give us.
 * 
 
 Summed-area tables burn precision fairly quickly: log(width x height) bits. A 512x512 SAT, therefore, consumes 18 bits of precision (in the worst case), 
 leaving only 5 bits of the 23-bit mantissa for the data itself!
 
 */

#pragma once

#include "Util.h"
#include <unordered_map>


struct SATHeapDescriptor
{
    ID3D12DescriptorHeap*	m_descriptorHeap;

    UINT   m_GenerateSATUavOffset = 0;
    UINT   m_RowBufUavOffset;
    UINT   m_BlockSumUavOffset;
    UINT   m_BlockOffUavOffset;
};

//sum area table
 class SAT
 {
    public:

    SAT(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format,UINT depthOrArraySize = 1);

	void OnResize(UINT newWidth, UINT newHeight);

	void BuildResources();

	void BuildCSShader();
	void BuildRootSignature();
	void BuildComputePipeLineState();

	void UdateDescriptors(ID3D12DescriptorHeap* heapDescriptor,UINT offset);

	void Execute(ID3D12GraphicsCommandList* cmdList,
		ID3D12Resource* input,CD3DX12_GPU_DESCRIPTOR_HANDLE);

	ID3D12Resource* SATResource();
	
	UINT  GetCSUDescriptorCount()const;

private:
	UINT            m_Width = 0;
	UINT            m_Height = 0;
	UINT			m_DepthOrArraySize = 1;

	ID3D12Device*   m_d3dDevice = nullptr;
	DXGI_FORMAT		m_Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> m_GeneratedSATSource;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_RowBufSource;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_BlockSumSource;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_BlockOffSource;
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE   m_GenerateSATUav;

    CD3DX12_GPU_DESCRIPTOR_HANDLE   m_RowBufUav;
    CD3DX12_GPU_DESCRIPTOR_HANDLE   m_BlockSumUav;
    CD3DX12_GPU_DESCRIPTOR_HANDLE   m_BlockOffUav;

	Microsoft::WRL::ComPtr<ID3D12RootSignature>			                m_PSignature = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>	m_Shader;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_PSOs;

    SATHeapDescriptor           m_HeapDescriptor;
 };
