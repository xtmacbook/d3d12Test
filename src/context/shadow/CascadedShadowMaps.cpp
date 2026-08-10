#include "CascadedShadowMaps.h"

#include "common/BufferStruct.h"
#include "common/Geometry.h"
#include "common/model.h"
#include "common/SDKMeshModel.h"
#include "common/DirectXHelpers.h"

#include "interface/TexContextInterface.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

bool CascadedShadowMaps::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	m_sdkMeshModel = std::make_shared<SDKMesh::SDKMeshModel>(m_d3dDevice.Get());
	m_sdkMeshModel->LoadModel((SourcePath() + L"Models/powerplant/powerplant.sdkmesh").c_str());

	BuildShapeGeometry(m_d3dDevice.Get(), m_CommandList.Get());

	BuildTextures();
	BuildDescriptorHeaps();
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

void CascadedShadowMaps::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = m_sdkMeshModel->GetTextureCount();
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&srvHeapDesc, IID_PPV_ARGS(&m_SrvDescriptorHeap)));

	auto srvCpuStart = m_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvGpuStart = m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	m_sdkMeshModel->BuildDescriptorHeaps(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, 0, m_CbvSrvUavDescriptorSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, 0, m_CbvSrvUavDescriptorSize),
		m_CbvSrvUavDescriptorSize
	);

}

void CascadedShadowMaps::BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList)
{
	m_sdkMeshModel->BuildShapeGeometry(device, mCommandList);
}

void CascadedShadowMaps::BuildRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[4];

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
	texTable[0].NumDescriptors = 2;
	texTable[0].BaseShaderRegister = 0;
	texTable[0].RegisterSpace = 0;
	texTable[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[3].DescriptorTable.pDescriptorRanges = texTable;

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

void CascadedShadowMaps::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	m_Shaders["standardVS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/csm.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/csm.hlsl", nullptr, "PS", "ps_5_1");
	m_Shaders["alphaTestedPS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/csm.hlsl", alphaTestDefines, "PS", "ps_5_1");
}

void CascadedShadowMaps::BuildFrameResources()
{
	for (int i = 0; i < m_NumFrameResources; ++i)
	{
		m_frameResources.push_back(std::make_unique<FrameResourceWithConstMaterial<SDKMesh::SDKMeshObjectConstants,
			PassConstantsWithFrog, SDKMesh::SDKMeshMaterialConstants>  >(m_d3dDevice.Get(),
				1, (UINT) m_sdkMeshModel->GetRenderItemCount(), (UINT)m_sdkMeshModel->GetMaterialCount()));
	}
}

void CascadedShadowMaps::Update(const GameTimer& gt)
{
	D3DContext::Update(gt);
	FrameResourceContextInterface::Update(gt, m_Fence.Get());
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void CascadedShadowMaps::UpdateMaterialCBs(const GameTimer& gt)
{
	m_sdkMeshModel->UpdateMaterialCBs(gt, m_currFrameResource);
}		

void CascadedShadowMaps::UpdateMainPassCB(const GameTimer& gt)
{
}

void CascadedShadowMaps::UpdateObjectCBs(const GameTimer& gt)
{
	m_sdkMeshModel->UpdateObjectCBs(gt, m_currFrameResource);
}

void CascadedShadowMaps::DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const RenderItem* ritem)
{
	const RenderItemWithTex* renderItem = static_cast<const RenderItemWithTex*>(ritem);

	//set vertex buffer
	cmdList->IASetVertexBuffers(0, 1, &ritem->m_Geo->VertexBufferView());
	cmdList->IASetIndexBuffer(&ritem->m_Geo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(ritem->m_PrimitiveType);

	//draw
	cmdList->DrawIndexedInstanced(renderItem->m_IndexCount,
		renderItem->m_InstanceCount, renderItem->m_StartIndexLocation,
		renderItem->m_BaseVertexLocation, 0);
}

void CascadedShadowMaps::DrawFrameResource(ID3D12CommandAllocator* allocator)
{
	 
}

void CascadedShadowMaps::BuildRenderItems()
{
	m_sdkMeshModel->BuildRenderItems(DirectX::XMMatrixIdentity(), 0);
}

void CascadedShadowMaps::BuildMaterials()
{
	m_sdkMeshModel->BuildMaterialsFromSDKMesh();
}

void CascadedShadowMaps::BuildPSOs()
{ 
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc =  GetDefaultPSODesc();
	psoDesc.pRootSignature = m_RootSignature.Get();

	m_sdkMeshModel->BuildPSOs(psoDesc,m_Shaders["standardVS"], m_Shaders["opaquePS"], m_Shaders["alphaTestedPS"]);
}

void CascadedShadowMaps::BuildTextures()
{
	m_sdkMeshModel->BuildTextures(m_CommandList.Get());
}


