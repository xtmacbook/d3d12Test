#include "ComputeShader.h"
#include "../common/App.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

bool ComputeShaderContext::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;

	m_BlurFilter = 
		std::make_unique<BlurFilter>(m_d3dDevice.Get(), m_win->Width(), m_win->Height(),
			DXGI_FORMAT_R8G8B8A8_UNORM);

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	//load textures
	initTextures(m_d3dDevice.Get(), m_CommandList.Get());

	BuildSRVDescriptorHeap(m_d3dDevice.Get());
	BuildSRCDescript(m_d3dDevice.Get(), m_CbvSrvUavDescriptorSize);
	BuildSampleDescriptorHeap(m_d3dDevice.Get());
	BuildSampleDescriptor(m_d3dDevice.Get(), m_CommandList.Get());

	//因为前面有三个纹理占用了几个个描述符
	int offsetInDescriptors =  m_Textures.size() + m_TextureArrs.size();
	m_BlurFilter->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			m_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			offsetInDescriptors, m_CbvSrvUavDescriptorSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			offsetInDescriptors, m_CbvSrvUavDescriptorSize),
		m_CbvSrvUavDescriptorSize);

	BuildShapeGeometry(m_d3dDevice.Get(), m_CommandList.Get());
	BuildMaterials();
	BuildRootSignature();
	BuildShadersAndInputLayout();

	BuildRenderItems();
	BuildFrameResources();

	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(m_CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void ComputeShaderContext::OnResize()
{
	BlendContext::OnResize();
	m_BlurFilter->OnResize(m_win->Width(), m_win->Height());
}

void ComputeShaderContext::BuildSRVDescriptorHeap(ID3D12Device* md3dDevice)
{
	const int blurDescriptorCount = 4;

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = m_Textures.size() + m_TextureArrs.size() + blurDescriptorCount;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&srvHeapDesc, IID_PPV_ARGS(&m_SrvDescriptorHeap)));
}

void ComputeShaderContext::BuildShadersAndInputLayout()
{
	BlendContext::BuildShadersAndInputLayout();
	m_BlurFilter->BuildCSShader();
}

void ComputeShaderContext::BuildRootSignature()
{
	BlendContext::BuildRootSignature();
	m_BlurFilter->BuildRootSignature();
}

void ComputeShaderContext::BuildPSOs()
{
	BlendContext::BuildPSOs();
	m_BlurFilter->BuildComputePipeLineState();
}

bool ComputeShaderContext::DrawPostProcessFrameResource(ID3D12CommandAllocator* allocator)
{
	m_BlurFilter->Execute(m_CommandList.Get(), CurrentBackBuffer(), 4);

	// Prepare to copy blurred output to the back buffer.
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST));

	m_CommandList->CopyResource(CurrentBackBuffer(), m_BlurFilter->Output());

	// Transition to PRESENT state.
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));
	
	return true;
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


