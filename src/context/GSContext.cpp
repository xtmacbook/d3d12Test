#include "GSContext.h"

void GSContext::BuildShaders()
{
}

void GSContext::BuildLayout()
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

bool GSContext::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	//load textures
	std::unordered_map<std::string, std::wstring> textureFiles;
	textureFiles["grassTex"] = SourcePath() + L"/Textures/grass.dds";
	textureFiles["waterTex"] = SourcePath() + L"/Textures/water1.dds";
	textureFiles["fenceTex"] = SourcePath() + L"/Textures/WireFence.dds";
	loadTextures(m_d3dDevice.Get(), m_CommandList.Get(), textureFiles);

	BuildSRVDescriptorHeap(m_d3dDevice.Get());
	BuildSRCDescript(m_d3dDevice.Get(), m_CbvSrvUavDescriptorSize);
	BuildSampleDescriptorHeap(m_d3dDevice.Get());
	BuildSampleDescriptor(m_d3dDevice.Get(), m_CommandList.Get());

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

void GSContext::BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList)
{
}

void GSContext::BuildFrameResources()
{
}

void GSContext::BuildPSOs()
{
}

void GSContext::BuildMaterials()
{
}

void GSContext::BuildRenderItems()
{
}

void GSContext::Update(const GameTimer& gt)
{
}

void GSContext::UpdateMainPassCB(const GameTimer& gt)
{
}

void GSContext::Draw(const GameTimer& gt)
{
}

void GSContext::DrawFrameResource(ID3D12CommandAllocator*)
{
}
