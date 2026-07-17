#include "DynamicIndexContext.h"
#include "common/Geometry.h"
#include "common/App.h"
#include "common/Sky.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

bool DynamicIndexContext::InitDirect3D()
{
	m_sky = std::make_shared<Sky>(this);

	if (!D3DContext::InitDirect3D()) return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	initTextures(m_d3dDevice.Get(), m_CommandList.Get());

	int numberDescriptor = m_Textures.size() + m_TextureArrs.size() + 1;

	BuildSRVDescriptorHeap(m_d3dDevice.Get(), numberDescriptor);
	BuildSRCDescript(m_d3dDevice.Get(), m_CbvSrvUavDescriptorSize);
	BuildSampleDescriptorHeap(m_d3dDevice.Get());
	BuildSampleDescriptor(m_d3dDevice.Get(), m_CommandList.Get());

	int offsetInDescriptors = m_Textures.size() + m_TextureArrs.size();
	m_sky->BuildResource();
	m_sky->BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		offsetInDescriptors, m_CbvSrvUavDescriptorSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
			offsetInDescriptors, m_CbvSrvUavDescriptorSize),
		m_CbvSrvUavDescriptorSize);

	BuildShapeGeometry(m_d3dDevice.Get(), m_CommandList.Get());
	m_sky->BuildSkyGeometry();


	BuildMaterials();
	BuildRootSignature();
	BuildShadersAndInputLayout();

	m_sky->BuildRootSignature();
	m_sky->BuildLayout();

	BuildRenderItems();
	BuildFrameResources();

	BuildPSOs();

	m_sky->BuildPSO();

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

void DynamicIndexContext::BuildMaterials()
{
	auto bricks0 = std::make_unique<MaterialWithTexTran>();
	bricks0->Name = "bricks0";
	bricks0->MaterialCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->NormalSrvHeapIndex = 1;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	bricks0->Roughness = 0.3f;

	auto tile0 = std::make_unique<MaterialWithTexTran>();
	tile0->Name = "tile0";
	tile0->MaterialCBIndex = 2;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->NormalSrvHeapIndex = 3;
	tile0->DiffuseAlbedo = XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
	tile0->Roughness = 0.1f;

	auto mirror0 = std::make_unique<MaterialWithTexTran>();
	mirror0->Name = "mirror0";
	mirror0->MaterialCBIndex = 3;
	mirror0->DiffuseSrvHeapIndex = 4;
	mirror0->NormalSrvHeapIndex = 5;
	mirror0->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mirror0->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
	mirror0->Roughness = 0.1f;

	/*auto sky = std::make_unique<Material>();
	sky->Name = "sky";
	sky->MaterialCBIndex = 4;
	sky->DiffuseSrvHeapIndex = 6;
	sky->NormalSrvHeapIndex = 7;
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;*/

	m_Materials["bricks0"] = std::move(bricks0);
	m_Materials["tile0"] = std::move(tile0);
	m_Materials["mirror0"] = std::move(mirror0);
	//m_Materials["sky"] = std::move(sky);
}

void DynamicIndexContext::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItemWithTex>();
	XMStoreFloat4x4(&boxRitem->m_World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f));
	XMStoreFloat4x4(&boxRitem->m_TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->m_ObjCBIndex = 0;
	boxRitem->m_Material = m_Materials["bricks0"].get();
	boxRitem->m_Geo = m_Geometries["shapeGeo"].get();
	boxRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->m_IndexCount = boxRitem->m_Geo->m_DrawArgs["box"].m_IndexCount;
	boxRitem->m_StartIndexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_StartIndexLocation;
	boxRitem->m_BaseVertexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_BaseVertexLocation;
	m_AllRitems.push_back(std::move(boxRitem));

	auto globeRitem = std::make_unique<RenderItemWithTex>();
	XMStoreFloat4x4(&globeRitem->m_World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 2.0f, 0.0f));
	XMStoreFloat4x4(&globeRitem->m_TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	globeRitem->m_ObjCBIndex = 1;
	globeRitem->m_Material = m_Materials["mirror0"].get();
	globeRitem->m_Geo = m_Geometries["shapeGeo"].get();
	globeRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	globeRitem->m_IndexCount = globeRitem->m_Geo->m_DrawArgs["sphere"].m_IndexCount;
	globeRitem->m_StartIndexLocation = globeRitem->m_Geo->m_DrawArgs["sphere"].m_StartIndexLocation;
	globeRitem->m_BaseVertexLocation = globeRitem->m_Geo->m_DrawArgs["sphere"].m_BaseVertexLocation;
	m_AllRitems.push_back(std::move(globeRitem));


	auto gridRitem = std::make_unique<RenderItemWithTex>();
	gridRitem->m_World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->m_TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->m_ObjCBIndex = 2;
	gridRitem->m_Material = m_Materials["tile0"].get();
	gridRitem->m_Geo = m_Geometries["shapeGeo"].get();
	gridRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->m_IndexCount = gridRitem->m_Geo->m_DrawArgs["grid"].m_IndexCount;
	gridRitem->m_StartIndexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_StartIndexLocation;
	gridRitem->m_BaseVertexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_BaseVertexLocation;
	m_AllRitems.push_back(std::move(gridRitem));

	XMMATRIX brickTexTransform = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	UINT objCBIndex = 3;
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
		leftSphereRitem->m_Material = m_Materials["mirror0"].get();
		leftSphereRitem->m_Geo = m_Geometries["shapeGeo"].get();
		leftSphereRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->m_IndexCount = leftSphereRitem->m_Geo->m_DrawArgs["sphere"].m_IndexCount;
		leftSphereRitem->m_StartIndexLocation = leftSphereRitem->m_Geo->m_DrawArgs["sphere"].m_StartIndexLocation;
		leftSphereRitem->m_BaseVertexLocation = leftSphereRitem->m_Geo->m_DrawArgs["sphere"].m_BaseVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->m_World, rightSphereWorld);
		rightSphereRitem->m_TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->m_ObjCBIndex = objCBIndex++;
		rightSphereRitem->m_Material = m_Materials["mirror0"].get();
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

	textureFiles.emplace_back("bricksDiffuseMap", SourcePath() + L"/Textures/bricks2.dds");
	textureFiles.emplace_back("bricksNormalMap", SourcePath() + L"/Textures/bricks2_nmap.dds");

	textureFiles.emplace_back("tileDiffuseMap", SourcePath() + L"/Textures/tile.dds");
	textureFiles.emplace_back("tileNormalMap", SourcePath() + L"/Textures/tile_nmap.dds");
	
	textureFiles.emplace_back("defaultDiffuseMap", SourcePath() + L"/Textures/white1x1.dds");
	textureFiles.emplace_back("defaultNormalMap", SourcePath() + L"/Textures/default_nmap.dds");
	
	loadTextures(device, mCommandList, textureFiles);
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

	int testIdex = 0;
	for (auto& item : m_AllRitems)
	{
		DrawRenderItem(m_CommandList.Get(), item.get());
		testIdex++;
	}

	m_sky->DrawSky(m_CommandList.Get(), allocator, sampler, m_currFrameResource->getPassGpuAddress(),
		descriptorHeaps);

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

