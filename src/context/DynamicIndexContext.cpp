#include "DynamicIndexContext.h"
#include "common/Geometry.h"
#include "common/App.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

bool DynamicIndexContext::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	initTextures(m_d3dDevice.Get(), m_CommandList.Get());

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

void DynamicIndexContext::BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList)
{
	BuildLitShapesScene(device, mCommandList);
}

void DynamicIndexContext::BuildPSOs()
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

void DynamicIndexContext::BuildMaterials()
{
	auto bricks0 = std::make_unique<MaterialWithTexTran>();
	bricks0->Name = "bricks0";
	bricks0->MaterialCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto stone0 = std::make_unique<MaterialWithTexTran>();
	stone0->Name = "stone0";
	stone0->MaterialCBIndex = 1;
	stone0->DiffuseSrvHeapIndex = 1;
	stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;

	auto tile0 = std::make_unique<MaterialWithTexTran>();
	tile0->Name = "tile0";
	tile0->MaterialCBIndex = 2;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.3f;

	auto crate0 = std::make_unique<MaterialWithTexTran>();
	crate0->Name = "crate0";
	crate0->MaterialCBIndex = 3;
	crate0->DiffuseSrvHeapIndex = 3;
	crate0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	crate0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	crate0->Roughness = 0.2f;

	m_Materials["bricks0"] = std::move(bricks0);
	m_Materials["stone0"] = std::move(stone0);
	m_Materials["tile0"] = std::move(tile0);
	m_Materials["crate0"] = std::move(crate0);
}

void DynamicIndexContext::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItemWithTex>();
	XMStoreFloat4x4(&boxRitem->m_World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f));
	XMStoreFloat4x4(&boxRitem->m_TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->m_ObjCBIndex = 0;
	boxRitem->m_Material = m_Materials["crate0"].get();
	boxRitem->m_Geo = m_Geometries["shapeGeo"].get();
	boxRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->m_IndexCount = boxRitem->m_Geo->m_DrawArgs["box"].m_IndexCount;
	boxRitem->m_StartIndexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_StartIndexLocation;
	boxRitem->m_BaseVertexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_BaseVertexLocation;
	m_AllRitems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItemWithTex>();
	gridRitem->m_World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->m_TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->m_ObjCBIndex = 1;
	gridRitem->m_Material = m_Materials["tile0"].get();
	gridRitem->m_Geo = m_Geometries["shapeGeo"].get();
	gridRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->m_IndexCount = gridRitem->m_Geo->m_DrawArgs["grid"].m_IndexCount;
	gridRitem->m_StartIndexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_StartIndexLocation;
	gridRitem->m_BaseVertexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_BaseVertexLocation;
	m_AllRitems.push_back(std::move(gridRitem));

	XMMATRIX brickTexTransform = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	UINT objCBIndex = 2;
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItemWithTex>();
		auto rightCylRitem = std::make_unique<RenderItemWithTex>();
		auto leftSphereRitem = std::make_unique<RenderItemWithTex>();
		auto rightSphereRitem = std::make_unique<RenderItemWithTex>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4(&leftCylRitem->m_World, rightCylWorld);
		XMStoreFloat4x4(&leftCylRitem->m_TexTransform, brickTexTransform);
		leftCylRitem->m_ObjCBIndex = objCBIndex++;
		leftCylRitem->m_Material = m_Materials["bricks0"].get();
		leftCylRitem->m_Geo = m_Geometries["shapeGeo"].get();
		leftCylRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->m_IndexCount = leftCylRitem->m_Geo->m_DrawArgs["cylinder"].m_IndexCount;
		leftCylRitem->m_StartIndexLocation = leftCylRitem->m_Geo->m_DrawArgs["cylinder"].m_StartIndexLocation;
		leftCylRitem->m_BaseVertexLocation = leftCylRitem->m_Geo->m_DrawArgs["cylinder"].m_BaseVertexLocation;

		XMStoreFloat4x4(&rightCylRitem->m_World, leftCylWorld);
		XMStoreFloat4x4(&rightCylRitem->m_TexTransform, brickTexTransform);
		rightCylRitem->m_ObjCBIndex = objCBIndex++;
		rightCylRitem->m_Material = m_Materials["bricks0"].get();
		rightCylRitem->m_Geo = m_Geometries["shapeGeo"].get();
		rightCylRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->m_IndexCount = rightCylRitem->m_Geo->m_DrawArgs["cylinder"].m_IndexCount;
		rightCylRitem->m_StartIndexLocation = rightCylRitem->m_Geo->m_DrawArgs["cylinder"].m_StartIndexLocation;
		rightCylRitem->m_BaseVertexLocation = rightCylRitem->m_Geo->m_DrawArgs["cylinder"].m_BaseVertexLocation;

		XMStoreFloat4x4(&leftSphereRitem->m_World, leftSphereWorld);
		leftSphereRitem->m_TexTransform = MathHelper::Identity4x4();
		leftSphereRitem->m_ObjCBIndex = objCBIndex++;
		leftSphereRitem->m_Material = m_Materials["stone0"].get();
		leftSphereRitem->m_Geo = m_Geometries["shapeGeo"].get();
		leftSphereRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->m_IndexCount = leftSphereRitem->m_Geo->m_DrawArgs["sphere"].m_IndexCount;
		leftSphereRitem->m_StartIndexLocation = leftSphereRitem->m_Geo->m_DrawArgs["sphere"].m_StartIndexLocation;
		leftSphereRitem->m_BaseVertexLocation = leftSphereRitem->m_Geo->m_DrawArgs["sphere"].m_BaseVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->m_World, rightSphereWorld);
		rightSphereRitem->m_TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->m_ObjCBIndex = objCBIndex++;
		rightSphereRitem->m_Material = m_Materials["stone0"].get();
		rightSphereRitem->m_Geo = m_Geometries["shapeGeo"].get();
		rightSphereRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->m_IndexCount = rightSphereRitem->m_Geo->m_DrawArgs["sphere"].m_IndexCount;
		rightSphereRitem->m_StartIndexLocation = rightSphereRitem->m_Geo->m_DrawArgs["sphere"].m_StartIndexLocation;
		rightSphereRitem->m_BaseVertexLocation = rightSphereRitem->m_Geo->m_DrawArgs["sphere"].m_BaseVertexLocation;

		m_AllRitems.push_back(std::move(leftCylRitem));
		m_AllRitems.push_back(std::move(rightCylRitem));
		m_AllRitems.push_back(std::move(leftSphereRitem));
		m_AllRitems.push_back(std::move(rightSphereRitem));
	}

	// All the render items are opaque.
	for (auto& e : m_AllRitems)
		m_OpaqueRitems.push_back(e.get());
}

void DynamicIndexContext::initTextures(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList)
{
	//load textures
	std::vector<TextureLoadDesc> textureFiles;
	textureFiles.emplace_back("bricksTex", SourcePath() + L"/Textures/bricks.dds");
	textureFiles.emplace_back("stoneTex", SourcePath() + L"/Textures/stone.dds");
	textureFiles.emplace_back("tileTex", SourcePath() + L"/Textures/tile.dds");
	textureFiles.emplace_back("crateTex", SourcePath() + L"/Textures/WoodCrate01.dds");
	loadTextures(device, mCommandList, textureFiles);
}

void DynamicIndexContext::Update(const GameTimer& gt)
{
	D3DContext::Update(gt);
	FrameResourceContextInterface::Update(gt, m_Fence.Get());
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void DynamicIndexContext::UpdateMainPassCB(const GameTimer& gt)
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

void DynamicIndexContext::Draw(const GameTimer& gt)
{
	D3DContext::Draw(gt);
	FrameResourceContextInterface::Draw(gt, m_CurrentFence, m_Fence.Get(), m_CommandQueue.Get());
}

void DynamicIndexContext::DrawFrameResource(ID3D12CommandAllocator* allocator)
{
	ThrowIfFailed(m_CommandList->Reset(allocator, m_PSOs["opaque"].Get()));
	BEFORE_DRAW_SET;

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescriptorHeap.Get(), m_SamplerDescriptorHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//set pass buffer
	m_CommandList->SetGraphicsRootConstantBufferView(m_rpi.m_PASS_RootParameterIndex, m_currFrameResource->getPassGpuAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE sampler(m_SamplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	m_CommandList->SetGraphicsRootDescriptorTable(4, sampler);

	//material
	m_CommandList->SetGraphicsRootShaderResourceView(m_rpi.m_Material_RootParameterIndex, m_currFrameResource->getMaterialGpuAddress());

	m_CommandList->SetGraphicsRootDescriptorTable(3, m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	for(auto& item : m_AllRitems)
		DrawRenderItem(m_CommandList.Get(), item.get());
	
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

