#include "ComputeShader.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

void ComputeShaderContext::BindOutputResources(ID3D12Device*device)
{
	TextureOutDes desc;
	TextureOutResouce resouce;
	BuildUAVTexture(m_d3dDevice.Get(), desc, resouce);
}

void ComputeShaderContext::CreateComputePipeLineState()
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	ZeroMemory(&computePsoDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	//computePsoDesc.pRootSignature = nullptr;
}

struct CSData
{
	XMFLOAT3 v1;
	XMFLOAT2 v2;
};

void ComputeShaderContext::CreateStructedBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList)
{
	UINT16 NumDataElements;
	// Generate some data to fill the SRV buffers with.
	std::vector<CSData> dataA(NumDataElements);
	std::vector<CSData> dataB(NumDataElements);
	for (int i = 0; i < NumDataElements; ++i)
	{
		dataA[i].v1 = XMFLOAT3(i, i, i);
		dataA[i].v2 = XMFLOAT2(i, 0);
		dataB[i].v1 = XMFLOAT3(-i, i, 0.0f);
		dataB[i].v2 = XMFLOAT2(0, -i);
	}
	UINT64 byteSize = dataA.size() * sizeof(CSData);

	// Create some buffers to be used as SRVs.
	//D3DUtil::CreateDefaultBuffer(
	//	device,
	//	mCommandList,
	//	dataA.data(),
	//	byteSize,
	//	mInputUploadBufferA);
	//D3DUtil::CreateDefaultBuffer(
	//	device,
	//	mCommandList,
	//	dataB.data(),
	//	byteSize,
	//	mInputUploadBufferB);

	//// Create the buffer that will be a UAV.
	//ThrowIfFailed(device->CreateCommittedResource(
	//	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
	//	D3D12_HEAP_FLAG_NONE,
	//	&CD3DX12_RESOURCE_DESC::Buffer(byteSize,
	//		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), 注意此处的unorder access
	//	D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
	//	nullptr,
	//	IID_PPV_ARGS(&mOutputBuffer)));

	 //后面可能是root signature

}

void ComputeShaderContext::CopyCSResultToSysMemory(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList)
{
	 //This required   to create system memory buffer with heap properties D3D12_HEAP_TYPE_READBACK.
	//Then we can use the ID3D12GraphicsCommandList::CopyResource method to copy the GPU resource to the system memory
	//Finally, we can map the system memory buffer with the mapping API to read it on the CPU.


	UINT64 byteSize;
	Microsoft::WRL::ComPtr<ID3D12Resource> mReadBackBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> mOutputBuffer;

	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&mReadBackBuffer)));

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mOutputBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_COPY_SOURCE));

	mCommandList->CopyResource(mReadBackBuffer.Get(), mOutputBuffer.Get());
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mOutputBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_COMMON));
	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());
	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList };
	//mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	// Wait for the work to finish.
	FlushCommandQueue();
	
	// Map the data so we can read it on CPU.
	////Data* mappedData = nullptr;
	//ThrowIfFailed(mReadBackBuffer->Map(0, nullptr,
	//	reinterpret_cast<void**>(&mappedData)));
	//std::ofstream fout("results.txt");
	//for (int i = 0; i < NumDataElements; ++i)
	//{
	//	fout << "(" << mappedData[i].v1.x << ", " <<
	//		mappedData[i].v1.y << ", " <<
	//		mappedData[i].v1.z << ", " <<
	//		mappedData[i].v2.x << ", " <<
	//		mappedData[i].v2.y << ")" << std::endl;
	//}
	//mReadBackBuffer->Unmap(0, nullptr);

}

void ComputeShaderContext::BuildComputeShaderRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	// Perfomance TIP: Order from most frequent to least frequent.
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

	ThrowIfFailed(m_d3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_BlurSignature.GetAddressOf())));

}
