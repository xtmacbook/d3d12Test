#include "ShadowMap.h"
#include "common/Geometry.h"
#include "common/App.h"
#include "common/Sky.h"
#include "common/ShadowMap.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

bool ShadowMapContext::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;
	
	m_shadow = std::make_shared<ShadowInterface>(this);
	m_shadow->BuildShadowMap();

	m_sceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_sceneBounds.Radius = sqrtf(10.0f * 10.0f + 15.0f * 15.0f);

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	initTextures(m_d3dDevice.Get(), m_CommandList.Get());

	BuildRootSignature();
	BuildDescriptorHeaps();

	BuildShapeGeometry(m_d3dDevice.Get(), m_CommandList.Get());

	BuildMaterials();
	BuildShadersAndInputLayout();
	m_shadow->BuildShaders();
	 
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

void ShadowMapContext::BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList)
{
	BuildLitShapesScene(device, mCommandList);
}

void ShadowMapContext::BuildMaterials()
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

	auto sky = std::make_unique<Material>();
	sky->Name = "sky";
	sky->MaterialCBIndex = 4;
	sky->DiffuseSrvHeapIndex = 6;
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;

	m_Materials["bricks0"] = std::move(bricks0);
	m_Materials["tile0"] = std::move(tile0);
	m_Materials["mirror0"] = std::move(mirror0);
	m_Materials["sky"] = std::move(sky);
}

void ShadowMapContext::BuildRenderItems()
{
	auto skyRitem = std::make_unique<RenderItemWithTex>();
	XMStoreFloat4x4(&skyRitem->m_World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	skyRitem->m_TexTransform = MathHelper::Identity4x4();
	skyRitem->m_ObjCBIndex = 0;
	skyRitem->m_Material = m_Materials["sky"].get();
	skyRitem->m_Geo = m_Geometries["shapeGeo"].get();
	skyRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->m_IndexCount = skyRitem->m_Geo->m_DrawArgs["sphere"].m_IndexCount;
	skyRitem->m_StartIndexLocation = skyRitem->m_Geo->m_DrawArgs["sphere"].m_StartIndexLocation;
	skyRitem->m_BaseVertexLocation = skyRitem->m_Geo->m_DrawArgs["sphere"].m_BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());
	m_AllRitems.push_back(std::move(skyRitem));

	auto quadRitem = std::make_unique<RenderItemWithTex>();
	quadRitem->m_TexTransform = MathHelper::Identity4x4();
	quadRitem->m_ObjCBIndex = 1;
	quadRitem->m_Material = m_Materials["bricks0"].get();
	quadRitem->m_Geo = m_Geometries["shapeGeo"].get();
	quadRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	quadRitem->m_IndexCount = quadRitem->m_Geo->m_DrawArgs["quad"].m_IndexCount;
	quadRitem->m_StartIndexLocation = quadRitem->m_Geo->m_DrawArgs["quad"].m_StartIndexLocation;
	quadRitem->m_BaseVertexLocation = quadRitem->m_Geo->m_DrawArgs["quad"].m_BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::Debug].push_back(quadRitem.get());
	m_AllRitems.push_back(std::move(quadRitem));


	auto boxRitem = std::make_unique<RenderItemWithTex>();
	XMStoreFloat4x4(&boxRitem->m_World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f));
	XMStoreFloat4x4(&boxRitem->m_TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->m_ObjCBIndex = 2;
	boxRitem->m_Material = m_Materials["bricks0"].get();
	boxRitem->m_Geo = m_Geometries["shapeGeo"].get();
	boxRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->m_IndexCount = boxRitem->m_Geo->m_DrawArgs["box"].m_IndexCount;
	boxRitem->m_StartIndexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_StartIndexLocation;
	boxRitem->m_BaseVertexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());
	m_AllRitems.push_back(std::move(boxRitem));


	auto globeRitem = std::make_unique<RenderItemWithTex>();
	XMStoreFloat4x4(&globeRitem->m_World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 2.0f, 0.0f));
	XMStoreFloat4x4(&globeRitem->m_TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	globeRitem->m_ObjCBIndex = 3;
	globeRitem->m_Material = m_Materials["mirror0"].get();
	globeRitem->m_Geo = m_Geometries["shapeGeo"].get();
	globeRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	globeRitem->m_IndexCount = globeRitem->m_Geo->m_DrawArgs["sphere"].m_IndexCount;
	globeRitem->m_StartIndexLocation = globeRitem->m_Geo->m_DrawArgs["sphere"].m_StartIndexLocation;
	globeRitem->m_BaseVertexLocation = globeRitem->m_Geo->m_DrawArgs["sphere"].m_BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::Opaque].push_back(globeRitem.get());
	m_AllRitems.push_back(std::move(globeRitem));


	auto gridRitem = std::make_unique<RenderItemWithTex>();
	gridRitem->m_World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->m_TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->m_ObjCBIndex = 4;
	gridRitem->m_Material = m_Materials["tile0"].get();
	gridRitem->m_Geo = m_Geometries["shapeGeo"].get();
	gridRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->m_IndexCount = gridRitem->m_Geo->m_DrawArgs["grid"].m_IndexCount;
	gridRitem->m_StartIndexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_StartIndexLocation;
	gridRitem->m_BaseVertexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	m_AllRitems.push_back(std::move(gridRitem));

	XMMATRIX brickTexTransform = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	UINT objCBIndex = 5;
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
		
		m_RitemLayer[(int)RenderLayer::Opaque].push_back(leftCylRitem.get());
		m_RitemLayer[(int)RenderLayer::Opaque].push_back(rightCylRitem.get());
		m_RitemLayer[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
		m_RitemLayer[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());

		m_AllRitems.push_back(std::move(leftCylRitem));
		m_AllRitems.push_back(std::move(rightCylRitem));
		m_AllRitems.push_back(std::move(leftSphereRitem));
		m_AllRitems.push_back(std::move(rightSphereRitem));
	}

	// All the render items are opaque.
	for (auto& e : m_AllRitems)
		m_OpaqueRitems.push_back(e.get());
}

void ShadowMapContext::BuildDescriptorHeaps()
{
	int baseTextureNum = m_Textures.size() + m_TextureArrs.size() + m_TextureCubes.size();

	int numberDescriptor = baseTextureNum +  1 + 1 ; //一个shadow texture 一个null 

	BuildSRVDescriptorHeap(m_d3dDevice.Get(), numberDescriptor);
	BuildSRCDescript(m_d3dDevice.Get(), m_CbvSrvUavDescriptorSize);

	auto srvCpuStart = m_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvGpuStart = m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	auto dsvCpuStart = m_DsvHeap->GetCPUDescriptorHandleForHeapStart();
	
	UINT shadowMapHeapIndex = baseTextureNum;
	{
		m_shadow->BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, shadowMapHeapIndex, m_CbvSrvUavDescriptorSize),
			CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, shadowMapHeapIndex, m_CbvSrvUavDescriptorSize),
			CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, m_DsvDescriptorSize));
	}

	UINT mNullCubeSrvIndex = shadowMapHeapIndex + 1;
	auto nullSrv = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mNullCubeSrvIndex, m_CbvSrvUavDescriptorSize);
	m_NullSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mNullCubeSrvIndex, m_CbvSrvUavDescriptorSize);

	//some null descriptor
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	m_d3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);
}

void ShadowMapContext::initTextures(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList)
{
	//load textures
	std::vector<TextureLoadDesc> textureFiles;

	textureFiles.emplace_back("bricksDiffuseMap", SourcePath() + L"/Textures/bricks2.dds");
	textureFiles.emplace_back("bricksNormalMap", SourcePath() + L"/Textures/bricks2_nmap.dds");

	textureFiles.emplace_back("tileDiffuseMap", SourcePath() + L"/Textures/tile.dds");
	textureFiles.emplace_back("tileNormalMap", SourcePath() + L"/Textures/tile_nmap.dds");
	
	textureFiles.emplace_back("defaultDiffuseMap", SourcePath() + L"/Textures/white1x1.dds");
	textureFiles.emplace_back("defaultNormalMap", SourcePath() + L"/Textures/default_nmap.dds");
	
	TextureLoadDesc cubeDesc;
	cubeDesc.Name = "cubeMap";
	cubeDesc.TextureCube = true;
	cubeDesc.FileName = SourcePath() + L"/Textures/snowcube1024.dds";
	//cube map
	textureFiles.emplace_back(cubeDesc);

	loadTextures(device, mCommandList, textureFiles);
}

void ShadowMapContext::DrawFrameResource(ID3D12CommandAllocator* allocator)
{
	ThrowIfFailed(m_CommandList->Reset(allocator, m_PSOs["opaque"].Get()));
	
	//DescriptorHeaps
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescriptorHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//root sigurate
	m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

	//material
	m_CommandList->SetGraphicsRootShaderResourceView(m_rpi.m_Material_RootParameterIndex,
		m_currFrameResource->getMaterialGpuAddress());

	//base texture
	m_CommandList->SetGraphicsRootDescriptorTable(5, m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	//some address
	D3D12_GPU_VIRTUAL_ADDRESS mainPassAddress = m_currFrameResource->getPassGpuAddress();
	UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(PassConstantsWithLightAndShadow));

	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTex(m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyTex.Offset(6, m_CbvSrvUavDescriptorSize);

	CD3DX12_GPU_DESCRIPTOR_HANDLE shadowMapTex(m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	shadowMapTex.Offset(7, m_CbvSrvUavDescriptorSize);

	//shadow map pass
	{
		D3D12_GPU_VIRTUAL_ADDRESS shadowPassAddress = mainPassAddress + 1 * passCBByteSize;

		//null texture for shadowmap
		m_CommandList->SetGraphicsRootDescriptorTable(3, m_NullSrv);
		 
		ShadowInterface::ShadowMapDrawData data;
		data.m_shadowPassAddress = shadowPassAddress;
		data.m_shadowPassRootParameterIndx = m_rpi.m_PASS_RootParameterIndex;
		data.m_drawCb = [&](ID3D12GraphicsCommandList*) {
			DrawRenderItems(m_CommandList.Get(), m_RitemLayer[(int)RenderLayer::Opaque]);
		};
		m_shadow->DrawSceneToShadowMap(m_CommandList.Get(), data);

	}

	//main pass
	{
		m_CommandList->SetPipelineState(m_PSOs["opaque"].Get());
		m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		m_CommandList->ClearRenderTargetView(CurrentCPUBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
		m_CommandList->ClearDepthStencilView(DepthStencilCPUView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
		m_CommandList->OMSetRenderTargets(1, &CurrentCPUBackBufferView(), true, &DepthStencilCPUView());

		m_CommandList->RSSetViewports(1, &m_ScreenViewport);
		m_CommandList->RSSetScissorRects(1, &m_ScissorRect);
		m_CommandList->SetGraphicsRootConstantBufferView(m_rpi.m_PASS_RootParameterIndex, mainPassAddress);
		
		m_CommandList->SetGraphicsRootDescriptorTable(3, shadowMapTex);
		m_CommandList->SetGraphicsRootDescriptorTable(4, skyTex);
		
		DrawRenderItems(m_CommandList.Get(), m_RitemLayer[(int)RenderLayer::Opaque]);

		m_CommandList->SetPipelineState(m_shadow->getDebugPSO().Get());
		DrawRenderItems(m_CommandList.Get(), m_RitemLayer[(int)RenderLayer::Debug]);

		////draw sky
		m_CommandList->SetPipelineState(m_PSOs["sky"].Get());
		DrawRenderItems(m_CommandList.Get(), m_RitemLayer[(int)RenderLayer::Sky]);
	}

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

void ShadowMapContext::Update(const GameTimer& gt)
{
	D3DContext::Update(gt);
	FrameResourceContextInterface::Update(gt, m_Fence.Get());

	//update light transform
	m_LightRotationAngle += 0.1f * gt.DeltaTime();
	XMMATRIX R = XMMatrixRotationY(m_LightRotationAngle);
	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&m_BaseLightDirections[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&m_RotatedLightDirections[i], lightDir);
	}

	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);

	ShadowInterface::ShadowMapUpdateData shadowMapUpdateData;
	shadowMapUpdateData.m_sceneBounds = m_sceneBounds;
	shadowMapUpdateData.m_lightDir = m_RotatedLightDirections[0];
	m_shadow->UpdateShadowTransform(shadowMapUpdateData);

	UpdateMainPassCB(gt);

	m_shadow->UpdateShadowPass(gt, m_currFrameResource, 1);

}

void ShadowMapContext::BuildFrameResources()
{
	for (int i = 0; i < m_NumFrameResources; ++i)
	{
		m_frameResources.push_back(
			std::make_unique<FrameResourceWithSRVMaterial<ObjectConstantsWithTexTranAndMaterialIndex,
			PassConstantsWithLightAndShadow, MaterialShadeRsourceWithDiffuseAndNormalTextIndex>  >(m_d3dDevice.Get(),
				2, (UINT)m_AllRitems.size(), (UINT)m_Materials.size()));
	}
}

void ShadowMapContext::UpdateMainPassCB(const GameTimer& gt)
{
	
	UPDATE_MAIN_PASS;
	XMMATRIX shadowTransform = XMLoadFloat4x4(&m_shadow->getShadowTransform());
	XMStoreFloat4x4(&m_MainPassCB.m_ShadowTransform, XMMatrixTranspose(shadowTransform));
	m_MainPassCB.m_AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	m_MainPassCB.m_Lights[0].Direction = m_RotatedLightDirections[0];
	m_MainPassCB.m_Lights[0].Strength = { 0.9f, 0.9f, 0.8f };
	m_MainPassCB.m_Lights[1].Direction = m_RotatedLightDirections[1];
	m_MainPassCB.m_Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	m_MainPassCB.m_Lights[2].Direction = m_RotatedLightDirections[2];
	m_MainPassCB.m_Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	m_currFrameResource->CopyPassData(0, &m_MainPassCB);
}

void ShadowMapContext::BuildPSOs()
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPso = opaquePsoDesc;
	skyPso.VS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["skyVS"]->GetBufferPointer()),
		m_Shaders["skyVS"]->GetBufferSize()
	};
	skyPso.PS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["skyPS"]->GetBufferPointer()),
		m_Shaders["skyPS"]->GetBufferSize()
	};

	skyPso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&skyPso, IID_PPV_ARGS(&m_PSOs["sky"])));

	m_shadow->BuildPSO(m_RootSignature.Get());
}

void ShadowMapContext::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;

	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(m_RtvHeap.GetAddressOf())));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;

	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(m_DsvHeap.GetAddressOf())));
}

 
