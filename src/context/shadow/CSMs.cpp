#include "CSMs.h"

#include "common/BufferStruct.h"
#include "common/Geometry.h"
#include "common/model.h"
#include "common/SDKMeshModel.h"
#include "common/DirectXHelpers.h"
#include "interface/TexContextInterface.h"

#include "CascadedShadowsManager.h"


using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;


bool CSMMapContext::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;

	static const XMVECTORF32 s_vecEye = { 100.0f, 5.0f, 5.0f, 0.f };
	mCamera.LookAt(s_vecEye, g_XMZero, { 0.0f, 1.0f, 0.0f,0.0f });

	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	m_sdkMeshModel = std::make_shared<SDKMesh::SDKMeshModel>(m_d3dDevice.Get());
	m_sdkMeshModel->LoadModel((SourcePath() + L"Models/powerplant/powerplant.sdkmesh").c_str());

	auto sceneBoundBox = m_sdkMeshModel->getBoundingBox();

	m_csmConfig = std::make_shared<CSMConfig>();
	m_csmConfig->m_iBufferSize = 1024;
	m_csmConfig->m_nCascadeLevels = 3;
	m_csmConfig->m_ShadowBufferFormat = CASCADE_DXGI_FORMAT_R32_TYPELESS;

	static const XMVECTORF32 s_lightEye = { -320.0f, 300.0f, -220.3f, 0.f };

	m_shadowLightCamera.LookAt(s_lightEye,g_XMZero, { 0.0f, 1.0f, 0.0f,0.0f });
	m_shadowLightCamera.SetLens(XM_PI / 4, 1.0f, 0.1f, 1000.0f);
	m_shadowLightCamera.UpdateViewMatrix();

	m_cascadedShadowsMgr = std::make_shared< CascadedShadowsManager>();
	m_cascadedShadowsMgr->init(m_d3dDevice.Get(), sceneBoundBox, &mCamera,
		&m_shadowLightCamera, m_csmConfig.get());
	m_cascadedShadowsMgr->BuildShadowMap();

	m_cascadedShadowsMgr->m_eSelectedCascadesFit = FIT_TO_SCENE;
	m_cascadedShadowsMgr->m_eSelectedNearFarFit = FIT_NEARFAR_SCENE_AABB;
	
	m_cascadedShadowsMgr->m_iCascadePartitionsMax = 100;
	m_cascadedShadowsMgr->m_iCascadePartitionsZeroToOne[0] = 5;
	m_cascadedShadowsMgr->m_iCascadePartitionsZeroToOne[1] = 15;
	m_cascadedShadowsMgr->m_iCascadePartitionsZeroToOne[2] = 60;
	m_cascadedShadowsMgr->m_iCascadePartitionsZeroToOne[3] = 100;
	m_cascadedShadowsMgr->m_iCascadePartitionsZeroToOne[4] = 100;
	m_cascadedShadowsMgr->m_iCascadePartitionsZeroToOne[5] = 100;
	m_cascadedShadowsMgr->m_iCascadePartitionsZeroToOne[6] = 100;
	m_cascadedShadowsMgr->m_iCascadePartitionsZeroToOne[7] = 100;

	m_cascadedShadowsMgr->m_iPCFBlurSize = 3;


	BuildShapeGeometry(m_d3dDevice.Get(), m_CommandList.Get());
	BuildTextures();
	BuildDescriptorHeaps();
	BuildResourceView();
	BuildFrameResources();
	BuildRootSignature();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(m_CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void CSMMapContext::BuildDescriptorHeaps()
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

void CSMMapContext::CreateDsvDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2; // 一个用于深度缓冲区，一个用于阴影贴图
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;

	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(m_DsvHeap.GetAddressOf())));
}

void CSMMapContext::BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList)
{
	m_sdkMeshModel->BuildShapeGeometry(device, mCommandList);
}

void CSMMapContext::BuildRootSignature()
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

void CSMMapContext::OnResize(int width, int heigh)
{
	D3DContext::OnResize(width, heigh);
	float fAspectRatio =  width / heigh;

	auto sceneBoundBox = m_sdkMeshModel->getBoundingBox();

	XMVECTOR vMeshExtents = XMVectorScale(XMLoadFloat3(&sceneBoundBox.Extents),2.0);
	XMVECTOR vMeshLength = XMVector3Length(vMeshExtents);
	FLOAT fMeshLength = XMVectorGetByIndex(vMeshLength, 0);
	
	mCamera.SetLens(XM_PI / 4, fAspectRatio, 0.05f, fMeshLength);

}

void CSMMapContext::BuildFrameResources()
{
	UINT cpasCount = MAX_CASCADES + 1; //MAX_CASCADES 是shadow 

	for (int i = 0; i < m_NumFrameResources; ++i)
	{
		m_frameResources.push_back(std::make_unique<FrameResourceWithConstMaterial<SDKMesh::SDKMeshObjectConstants,
			CSMPassConstants, SDKMesh::SDKMeshMaterialConstants>  >(m_d3dDevice.Get(),
				cpasCount, (UINT)m_sdkMeshModel->GetRenderItemCount(), (UINT)m_sdkMeshModel->GetMaterialCount()));
	}
}

void CSMMapContext::Update(const GameTimer& gt)
{
	D3DContext::Update(gt);

	FrameResourceContextInterface::Update(gt, m_Fence.Get());

	UpdateObjectCBs(gt);
	
	UpdateMaterialCBs(gt);

	UpdateMainPassCB(gt);
}

void CSMMapContext::UpdateMaterialCBs(const GameTimer& gt)
{
	static int bufferNum = 3;
	if (bufferNum > 0)
	{
		m_sdkMeshModel->UpdateMaterialCBs(gt, m_currFrameResource);
		bufferNum--;
	}
}

void CSMMapContext::UpdateMainPassCB(const GameTimer& gt)
{
	
	m_cascadedShadowsMgr->UpdateFrame(gt, &mCamera,&m_shadowLightCamera);
	
	CSMPassConstants mainConstantsData;

	XMMATRIX CameraViewProj = XMMatrixMultiply(mCamera.GetView(), mCamera.GetProj());
	XMStoreFloat4x4(&mainConstantsData.m_WorldViewProj, XMMatrixTranspose(CameraViewProj));
	XMStoreFloat4x4(&mainConstantsData.m_WorldView, XMMatrixTranspose(mCamera.GetView()));
	XMStoreFloat4x4(&mainConstantsData.m_World, XMMatrixIdentity());

	mainConstantsData.m_iPCFBlurForLoopStart = m_cascadedShadowsMgr->m_iPCFBlurSize / 2 + 1;
	mainConstantsData.m_iPCFBlurForLoopEnd = m_cascadedShadowsMgr->m_iPCFBlurSize / 2 - 2;
	mainConstantsData.m_fShadowBiasFromGUI = m_fPCFOffset;
	mainConstantsData.m_fCascadeBlendArea = m_fBlurBetweenCascadesAmount;
	m_cascadedShadowsMgr->UpdateMainPassData(mainConstantsData);

	XMVECTOR lightDir = XMVector3Normalize(-m_shadowLightCamera.GetLook());
	XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&mainConstantsData.m_vLightDir), lightDir);
	mainConstantsData.m_vLightDir.w = 1.0f;
	m_currFrameResource->CopyPassData(0, &mainConstantsData);

	for (UINT i = 0; i < m_csmConfig->m_nCascadeLevels; i++)
	{
		CSMPassConstants shadowConstantsData;
		XMStoreFloat4x4(&shadowConstantsData.m_WorldView, XMMatrixTranspose(m_shadowLightCamera.GetView()));
		XMStoreFloat4x4(&shadowConstantsData.m_World, XMMatrixIdentity());
		m_cascadedShadowsMgr->UpdateShadowPassData(i, shadowConstantsData);

		m_currFrameResource->CopyPassData(1 + i, &shadowConstantsData);
	}
	
}

void CSMMapContext::UpdateObjectCBs(const GameTimer& gt)
{
	static int bufferNum = 3;
	if (bufferNum > 0)
	{
		m_sdkMeshModel->UpdateObjectCBs(gt, m_currFrameResource);
		bufferNum--;
	}
}

void CSMMapContext::Draw(const GameTimer& gt)
{
	FrameResourceContextInterface::Draw(gt, m_CurrentFence, m_Fence.Get(), m_CommandQueue.Get());
}

void CSMMapContext::DrawFrameResource(ID3D12CommandAllocator* allocator)
{
	ThrowIfFailed(m_CommandList->Reset(allocator, nullptr));

	//DescriptorHeaps
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescriptorHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	//root sigurate
	m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

	UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(CSMPassConstants));

	D3D12_GPU_VIRTUAL_ADDRESS passAddress = m_currFrameResource->getPassGpuAddress();

	//shadow map pass
	{

		D3D12_GPU_VIRTUAL_ADDRESS shadowPassAddress = m_currFrameResource->getPassGpuAddress() + 
			1 * passCBByteSize;
		CSMShadowMapDrawData data;
		data.m_shadowPassAddress = shadowPassAddress;
		data.m_shadowPassRootParameterIndx = 2;

		data.m_drawCb = [&](ID3D12GraphicsCommandList*) {
			m_sdkMeshModel->DrawRenderItemsWithShadowPass(allocator, m_d3dDevice.Get(),
				m_CommandList.Get(), m_currFrameResource, m_SrvDescriptorHeap.Get(),
				m_CbvSrvUavDescriptorSize, m_cascadedShadowsMgr-> GetDrawSceneToShadowMapPSO(),
				m_HeapDescriptorOffsets.m_nullSrvGpuHandle);
			};

		m_cascadedShadowsMgr->RenderShadowsForAllCascades(m_CommandList.Get(), data);

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

		CSMShadowMapDrawData data;
		data.m_shadowPassAddress = passAddress;
		data.m_shadowPassRootParameterIndx = 2;

		data.m_drawCb = [&](ID3D12GraphicsCommandList*)
			{
				m_sdkMeshModel->DrawRenderItemsWithOnePass(allocator, m_d3dDevice.Get(),
					m_CommandList.Get(), m_currFrameResource, m_SrvDescriptorHeap.Get(),
					m_CbvSrvUavDescriptorSize, m_cascadedShadowsMgr->GetEffect());
			};
		m_cascadedShadowsMgr->RenderScene(m_CommandList.Get(), data);

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

void CSMMapContext::OnKeyboardInput(const GameTimer& gt)
{
	//update cascade map variable

	/*
	m_iPCFBlurSize
m_fPCFOffset
m_fBlurBetweenCascadesAmount
m_bMoveLightTexelSize
m_eSelectedCascadesFit
m_eSelectedCascadeSelection
m_iCascadePartitionsZeroToOne
m_iBlurBetweenCascades
	*/

}

void CSMMapContext::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = GetDefaultPSODesc();
	psoDesc.pRootSignature = m_RootSignature.Get();

	m_cascadedShadowsMgr->BuildPOS(m_sdkMeshModel.get(), psoDesc);
}


void CSMMapContext::BuildTextures()
{
	m_sdkMeshModel->BuildTextures(m_CommandList.Get());
}

void CSMMapContext::BuildResourceView()
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
		m_cascadedShadowsMgr->BuildDescriptors(
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


