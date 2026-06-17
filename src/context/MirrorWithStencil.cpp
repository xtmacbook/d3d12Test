#include "MirrorWithStencil.h"
#include "../common/Geometry.h"
#include "../common/Data.h"
#include "../common/App.h"
#include "Waves.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;


bool MirrorWithStencil::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	//load textures
	std::unordered_map<std::string, std::wstring> textureFiles;
	textureFiles["bricksTex"] = L"../Textures/bricks3.dds";
	textureFiles["checkboardTex"] = L"../Textures/checkboard.dds";
	textureFiles["iceTex"] = L"../Textures/ice.dds";
	textureFiles["white1x1Tex"] = L"../Textures/white1x1.dds";
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

void MirrorWithStencil::BuildShapeGeometry(ID3D12Device*device, ID3D12GraphicsCommandList* mCommandList)
{
	BuildMirror(device, mCommandList);
	BuildSkull(device, mCommandList);
}

void MirrorWithStencil::BuildFrameResources()
{
	for (int i = 0; i < m_NumFrameResources; ++i)
	{
		m_frameResources.push_back(std::make_unique<FrameResourceWithMaterial<ObjectConstantsWithTexTran,
			PassConstantsWithFrog, RenderItemWithTex>  >(m_d3dDevice.Get(),
				2, (UINT)m_AllRitems.size(), (UINT)m_Materials.size()));
	}
}

void MirrorWithStencil::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { m_InputLayout.data(), (UINT)m_InputLayout.size() };
	opaquePsoDesc.pRootSignature = m_RootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["standardVS"]->GetBufferPointer()),
		m_Shaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["opaquePS"]->GetBufferPointer()),
		m_Shaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = m_BackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = m_DepthStencilFormat;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&m_PSOs["opaque"])));

}

void MirrorWithStencil::BuildMaterials()
{
	auto bricks = std::make_unique<MaterialWithTexTran>();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 0;
	bricks->DiffuseSrvHeapIndex = 0;
	bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	bricks->Roughness = 0.25f;

	auto checkertile = std::make_unique<MaterialWithTexTran>();
	checkertile->Name = "checkertile";
	checkertile->MatCBIndex = 1;
	checkertile->DiffuseSrvHeapIndex = 1;
	checkertile->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	checkertile->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
	checkertile->Roughness = 0.3f;

	auto icemirror = std::make_unique<MaterialWithTexTran>();
	icemirror->Name = "icemirror";
	icemirror->MatCBIndex = 2;
	icemirror->DiffuseSrvHeapIndex = 2;
	icemirror->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
	icemirror->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	icemirror->Roughness = 0.5f;

	auto skullMat = std::make_unique<MaterialWithTexTran>();
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	skullMat->DiffuseSrvHeapIndex = 3;
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	skullMat->Roughness = 0.3f;

	auto shadowMat = std::make_unique<MaterialWithTexTran>();
	shadowMat->Name = "shadowMat";
	shadowMat->MatCBIndex = 4;
	shadowMat->DiffuseSrvHeapIndex = 3;
	shadowMat->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
	shadowMat->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
	shadowMat->Roughness = 0.0f;

	m_Materials["bricks"] = std::move(bricks);
	m_Materials["checkertile"] = std::move(checkertile);
	m_Materials["icemirror"] = std::move(icemirror);
	m_Materials["skullMat"] = std::move(skullMat);
	m_Materials["shadowMat"] = std::move(shadowMat);
}

void MirrorWithStencil::BuildRenderItems()
{
	//floor
	auto floorRitem = std::make_unique<RenderItemWithTex>();
	floorRitem->m_World = MathHelper::Identity4x4();
	floorRitem->m_TexTransform = MathHelper::Identity4x4();
	floorRitem->m_ObjCBIndex = 0;
	floorRitem->m_Material = m_Materials["checkertile"].get();
	floorRitem->m_Geo = m_Geometries["roomGeo"].get();
	floorRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRitem->FillWithDrawArgs(&(floorRitem->m_Geo->m_DrawArgs["floor"]));

	auto wallsRitem = std::make_unique<RenderItemWithTex>();
	wallsRitem->m_World = MathHelper::Identity4x4();
	wallsRitem->m_TexTransform = MathHelper::Identity4x4();
	wallsRitem->m_ObjCBIndex = 1;
	wallsRitem->m_Material = m_Materials["bricks"].get();
	wallsRitem->m_Geo = m_Geometries["roomGeo"].get();
	wallsRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wallsRitem->FillWithDrawArgs(&(wallsRitem->m_Geo->m_DrawArgs["wall"]));

	auto skullRitem = std::make_unique<RenderItemWithTex>();
	skullRitem->m_World = MathHelper::Identity4x4();
	skullRitem->m_TexTransform = MathHelper::Identity4x4();
	skullRitem->m_ObjCBIndex = 2;
	skullRitem->m_Material = m_Materials["skullMat"].get();
	skullRitem->m_Geo = m_Geometries["skullGeo"].get();
	skullRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->FillWithDrawArgs(&(skullRitem->m_Geo->m_DrawArgs["skull"]));

	// Reflected skull will have different world matrix, so it needs to be its own render item.
	auto reflectedSkullRitem = std::make_unique<RenderItemWithTex>();
	*reflectedSkullRitem = *skullRitem;
	reflectedSkullRitem->m_ObjCBIndex = 3;

	// Shadowed skull will have different world matrix, so it needs to be its own render item.
	auto shadowedSkullRitem = std::make_unique<RenderItemWithTex>();
	*shadowedSkullRitem = *skullRitem;
	shadowedSkullRitem->m_ObjCBIndex = 4;
	shadowedSkullRitem->m_Material = m_Materials["shadowMat"].get();

	auto mirrorRitem = std::make_unique<RenderItemWithTex>();
	mirrorRitem->m_World = MathHelper::Identity4x4();
	mirrorRitem->m_TexTransform = MathHelper::Identity4x4();
	mirrorRitem->m_ObjCBIndex = 5;
	mirrorRitem->m_Material = m_Materials["icemirror"].get();
	mirrorRitem->m_Geo = m_Geometries["roomGeo"].get();
	mirrorRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mirrorRitem->FillWithDrawArgs(&(mirrorRitem->m_Geo->m_DrawArgs["mirror"]));

	m_AllRitems.push_back(std::move(floorRitem));
	m_AllRitems.push_back(std::move(wallsRitem));
	m_AllRitems.push_back(std::move(skullRitem));
		
	m_AllRitems.push_back(std::move(reflectedSkullRitem));
	m_AllRitems.push_back(std::move(shadowedSkullRitem));
	m_AllRitems.push_back(std::move(mirrorRitem));
}

void MirrorWithStencil::Update(const GameTimer& gt)
{
	D3DContext::Update(gt);
	FrameResourceContextInterface::Update(gt, m_Fence.Get());
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}


void MirrorWithStencil::UpdateMainPassCB(const GameTimer& gt)
{
	UPDATE_MAIN_PASS;

	m_MainPassCB.m_AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	m_MainPassCB.m_Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	m_MainPassCB.m_Lights[0].Strength = { 0.9f, 0.9f, 0.8f };
	m_MainPassCB.m_Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	m_MainPassCB.m_Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	m_MainPassCB.m_Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	m_MainPassCB.m_Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	m_currFrameResource->CopyPassData(0, &m_MainPassCB);
}

void MirrorWithStencil::Draw(const GameTimer& gt)
{
	D3DContext::Draw(gt);
	FrameResourceContextInterface::Draw(gt, m_CurrentFence, m_Fence.Get(), m_CommandQueue.Get());
}

void MirrorWithStencil::DrawFrameResource(ID3D12CommandAllocator* allocator)
{
	ThrowIfFailed(m_CommandList->Reset(allocator, m_PSOs["opaque"].Get()));
	BEFORE_DRAW_SET;

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescriptorHeap.Get(), m_SamplerDescriptorHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//set pass buffer
	m_CommandList->SetGraphicsRootConstantBufferView(m_rpi.m_PASS_RootParameterIndex, m_currFrameResource->getPassGpuAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE sampler(m_SamplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	m_CommandList->SetGraphicsRootDescriptorTable(4, sampler);

	DrawRenderItem(m_CommandList.Get(), m_AllRitems[0].get());
	DrawRenderItem(m_CommandList.Get(), m_AllRitems[1].get());

	// Indicate a state transition on the resource usage.
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_CommandList->Close());
	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(m_SwapChain->Present(0, 0));
	m_CurrBackBuffer = (m_CurrBackBuffer + 1) % SwapChainBufferCount;
}

