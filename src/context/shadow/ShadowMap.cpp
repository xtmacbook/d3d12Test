#include "ShadowMap.h"

#include "common/BufferStruct.h"
#include "common/Geometry.h"
#include "common/model.h"
#include "common/SDKMeshModel.h"
#include "common/DirectXHelpers.h"
#include "ShadowInterface.h"

#include "interface/TexContextInterface.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;



bool ShadowMapBase::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;
	
	m_shadowInterface = std::make_shared<ShadowInterface>(this);
	m_shadowInterface->BuildShadowMap();

	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	m_sdkMeshModel = std::make_shared<SDKMesh::SDKMeshModel>(m_d3dDevice.Get());
	m_sdkMeshModel->LoadModel((SourcePath() + L"Models/powerplant/powerplant.sdkmesh").c_str());

	m_sceneBounds = m_sdkMeshModel->getBoundingSphere();

	BuildShapeGeometry(m_d3dDevice.Get(), m_CommandList.Get());

	BuildTextures();
	BuildDescriptorHeaps();
	BuildResourceView();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(m_CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void ShadowMapBase::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};

	UINT shadowMapCount = 1;
	UINT sdkMeshModelTextureCount = m_sdkMeshModel->GetTextureCount();
	UINT nullCount = 1; //null srv

	srvHeapDesc.NumDescriptors = sdkMeshModelTextureCount + shadowMapCount + nullCount;

	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&srvHeapDesc, IID_PPV_ARGS(&m_SrvDescriptorHeap)));

	m_HeapDescriptorOffsets.m_sdkMeshModelTextureHeapOffset = 0;
	m_HeapDescriptorOffsets.m_shadowMapHeapOffset = sdkMeshModelTextureCount;
	m_HeapDescriptorOffsets.m_nullHeapOffset = m_HeapDescriptorOffsets.m_shadowMapHeapOffset + 1;
}

void ShadowMapBase::CreateDsvDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2; //一个用于深度缓冲区，一个用于阴影贴图	
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;

	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(m_DsvHeap.GetAddressOf())));
}

void ShadowMapBase::BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList)
{
	m_sdkMeshModel->BuildShapeGeometry(device, mCommandList);
}

void ShadowMapBase::BuildRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[6];

	//cobject
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	//material
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; //change
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.RegisterSpace = 0; //change
	rootParameters[1].Descriptor.ShaderRegister = 2;

	//pass
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.RegisterSpace = 0;
	rootParameters[2].Descriptor.ShaderRegister = 1;

	//texture despector
	D3D12_DESCRIPTOR_RANGE texTable[1];
	texTable[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable[0].NumDescriptors = 1;
	texTable[0].BaseShaderRegister = 0;
	texTable[0].RegisterSpace = 0;
	texTable[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[3].DescriptorTable.pDescriptorRanges = texTable;
	
	D3D12_DESCRIPTOR_RANGE texTable1[1];
	texTable1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable1[0].NumDescriptors = 1;
	texTable1[0].BaseShaderRegister = 1;
	texTable1[0].RegisterSpace = 0;
	texTable1[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[4].DescriptorTable.pDescriptorRanges = texTable1;

	D3D12_DESCRIPTOR_RANGE texTable2[1];
	texTable2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable2[0].NumDescriptors = 1;
	texTable2[0].BaseShaderRegister = 2;
	texTable2[0].RegisterSpace = 0;
	texTable2[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[5].DescriptorTable.pDescriptorRanges = texTable2;

	auto staticSamplers = DirectX::getStaticSamplerDescriptor();

	D3D12_ROOT_SIGNATURE_DESC descRootSignature;
	descRootSignature.NumStaticSamplers = (UINT)staticSamplers.size();;
	descRootSignature.pStaticSamplers = staticSamplers.data();
	descRootSignature.pParameters = rootParameters;
	descRootSignature.NumParameters = _countof(rootParameters);
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
		IID_PPV_ARGS(m_RootSignature.GetAddressOf())));
}

void ShadowMapBase::BuildShadersAndInputLayout()
{
	//这里每个sdkmesh的mesh part使用不同的layout
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	m_Shaders["standardVS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/ShadowMap.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/ShadowMap.hlsl", nullptr, "PS", "ps_5_1");
	m_Shaders["alphaTestedPS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/ShadowMap.hlsl", alphaTestDefines, "PS", "ps_5_1");
}

void ShadowMapBase::BuildFrameResources()
{
	for (int i = 0; i < m_NumFrameResources; ++i)
	{
		m_frameResources.push_back(std::make_unique<FrameResourceWithConstMaterial<SDKMesh::SDKMeshObjectConstants,
			PassConstantsWithNLightAndShadow, SDKMesh::SDKMeshMaterialConstants>  >(m_d3dDevice.Get(),
				2, (UINT) m_sdkMeshModel->GetRenderItemCount(), (UINT)m_sdkMeshModel->GetMaterialCount()));
	}
}

void ShadowMapBase::Update(const GameTimer& gt)
{
	D3DContext::Update(gt);
	FrameResourceContextInterface::Update(gt, m_Fence.Get());
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	
	UpdateMainPassCB(gt);


	ShadowInterface::ShadowMapUpdateData shadowMapUpdateData;
	shadowMapUpdateData.m_sceneBounds = m_sceneBounds;
	shadowMapUpdateData.m_lightDir.x = m_MainPassCB.m_Lights[0].lightDirection.x;
	shadowMapUpdateData.m_lightDir.y = m_MainPassCB.m_Lights[0].lightDirection.y;
	shadowMapUpdateData.m_lightDir.z = m_MainPassCB.m_Lights[0].lightDirection.z;
	m_shadowInterface->UpdateShadowTransform(shadowMapUpdateData);

	m_shadowInterface->UpdateShadowPass(gt, m_currFrameResource, 1);

}

void ShadowMapBase::UpdateMaterialCBs(const GameTimer& gt)
{
	static int bufferNum = 3;
	if (bufferNum > 0)
	{
		m_sdkMeshModel->UpdateMaterialCBs(gt, m_currFrameResource);
		bufferNum--;
	}
}		

void ShadowMapBase::UpdateMainPassCB(const GameTimer& gt)
{
	UPDATE_MAIN_PASS;

	XMMATRIX shadowTransform = XMLoadFloat4x4(&m_shadowInterface->getShadowTransform());
	XMStoreFloat4x4(&m_MainPassCB.m_ShadowTransform, XMMatrixTranspose(shadowTransform));

	m_MainPassCB.m_Lights[0].lightDirection     = { -0.5265408f, -0.5735765f, -0.6275069f, 0 };
	m_MainPassCB.m_Lights[0].lightDiffuseColor  = { 1.0000000f, 0.9607844f, 0.8078432f, 0 };
	m_MainPassCB.m_Lights[0].lightSpecularColor = { 1.0000000f, 0.9607844f, 0.8078432f, 0 };

	m_MainPassCB.m_Lights[1].lightDirection     = { 0.7198464f,  0.3420201f,  0.6040227f, 0 };
	m_MainPassCB.m_Lights[1].lightDiffuseColor = { 0.9647059f, 0.7607844f, 0.4078432f, 0 };
	m_MainPassCB.m_Lights[1].lightSpecularColor= { 0.0000000f, 0.0000000f, 0.0000000f, 0 };
	
	m_MainPassCB.m_Lights[2].lightDirection    = { 0.4545195f, -0.7660444f,  0.4545195f, 0 };
	m_MainPassCB.m_Lights[2].lightDiffuseColor = { 0.3231373f, 0.3607844f, 0.3937255f, 0 };
	m_MainPassCB.m_Lights[2].lightSpecularColor= { 0.3231373f, 0.3607844f, 0.3937255f, 0 };

	m_currFrameResource->CopyPassData(0, &m_MainPassCB);
}

void ShadowMapBase::UpdateObjectCBs(const GameTimer& gt)
{
	static int bufferNum = 3;
	if (bufferNum > 0)
	{
		m_sdkMeshModel->UpdateObjectCBs(gt, m_currFrameResource);
		bufferNum--;
	}
}

void ShadowMapBase::Draw(const GameTimer& gt)
{
	FrameResourceContextInterface::Draw(gt, m_CurrentFence, m_Fence.Get(), m_CommandQueue.Get());
}

void ShadowMapBase::DrawFrameResource(ID3D12CommandAllocator* allocator)
{
	ThrowIfFailed(m_CommandList->Reset(allocator, nullptr));
	

	//DescriptorHeaps
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescriptorHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	//root sigurate
	m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

	UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(PassConstantsWithLightAndShadow));

	//shadow map pass
	{
		D3D12_GPU_VIRTUAL_ADDRESS shadowPassAddress = m_currFrameResource->getPassGpuAddress() + 1 * passCBByteSize;

		ShadowInterface::ShadowMapDrawData data;
		data.m_shadowPassAddress = shadowPassAddress;
		data.m_shadowPassRootParameterIndx = 2;

		data.m_drawCb = [&](ID3D12GraphicsCommandList*) {
			 m_sdkMeshModel->DrawRenderItemsWithShadowPass(allocator, m_d3dDevice.Get(),
					m_CommandList.Get(), m_currFrameResource, m_SrvDescriptorHeap.Get(),
					m_CbvSrvUavDescriptorSize, m_shadowInterface->GetDrawSceneToShadowMapPSO(),
				 m_HeapDescriptorOffsets.m_nullSrvGpuHandle);
			};

		m_shadowInterface->DrawSceneToShadowMap(m_CommandList.Get(), data);

	}

	//main pass
	{
		//set pass
		m_CommandList->SetGraphicsRootConstantBufferView(2, m_currFrameResource->getPassGpuAddress());
		m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, 
			D3D12_RESOURCE_STATE_RENDER_TARGET));
		m_CommandList->ClearRenderTargetView(CurrentCPUBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
		m_CommandList->ClearDepthStencilView(DepthStencilCPUView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
		m_CommandList->OMSetRenderTargets(1, &CurrentCPUBackBufferView(), true, &DepthStencilCPUView());

		m_CommandList->RSSetViewports(1, &m_ScreenViewport);
		m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

		CD3DX12_GPU_DESCRIPTOR_HANDLE shadowMapTex(m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		shadowMapTex.Offset(m_HeapDescriptorOffsets.m_shadowMapHeapOffset, m_CbvSrvUavDescriptorSize);
		m_CommandList->SetGraphicsRootDescriptorTable(5, shadowMapTex);

		m_sdkMeshModel->DrawRenderItems(allocator,m_d3dDevice.Get(),
		m_CommandList.Get(),m_currFrameResource, m_SrvDescriptorHeap.Get(),
		m_CbvSrvUavDescriptorSize, m_effects) ;
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

void ShadowMapBase::BuildRenderItems()
{
}

void ShadowMapBase::BuildMaterials()
{
}

void ShadowMapBase::BuildPSOs()
{ 
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc =  GetDefaultPSODesc();
	psoDesc.pRootSignature = m_RootSignature.Get();

	SDKMesh::EffectPipelineStateDescription epsd;
	epsd.standardVS = m_Shaders["standardVS"];
	epsd.opaquesPS = m_Shaders["opaquePS"];
	epsd.alphaPS = m_Shaders["alphaTestedPS"];
	epsd.device = m_d3dDevice.Get();
	epsd.desc = psoDesc;

	m_effects = m_sdkMeshModel->CreateEffect(epsd);

	m_shadowInterface->BuildDrawScenePSO(m_RootSignature.Get());	
}


void ShadowMapBase::BuildTextures()
{
	m_sdkMeshModel->BuildTextures(m_CommandList.Get());
}

void ShadowMapBase::BuildResourceView()
{
	//build resouce view
	auto srvCpuStart = m_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvGpuStart = m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	auto dsvCpuStart = m_DsvHeap->GetCPUDescriptorHandleForHeapStart();

	INT offset = 0;

	m_sdkMeshModel->BuildTextureResourceView(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, offset, m_CbvSrvUavDescriptorSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, offset, m_CbvSrvUavDescriptorSize),
		offset,
		m_CbvSrvUavDescriptorSize
	);


	UINT shadowMapHeapIndex = m_HeapDescriptorOffsets.m_shadowMapHeapOffset;
	{
		m_shadowInterface->BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, shadowMapHeapIndex, m_CbvSrvUavDescriptorSize),
			CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, shadowMapHeapIndex, m_CbvSrvUavDescriptorSize),
			CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, m_DsvDescriptorSize));
	}

	//some null descriptor
	auto nullSrv = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, m_HeapDescriptorOffsets.m_nullHeapOffset, m_CbvSrvUavDescriptorSize);
	m_HeapDescriptorOffsets.m_nullSrvGpuHandle = 
		CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, m_HeapDescriptorOffsets.m_nullHeapOffset, m_CbvSrvUavDescriptorSize);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	m_d3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);
}


