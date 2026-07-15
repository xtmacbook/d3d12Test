#include "CubeMapContext.h"

#include "common/BufferStruct.h"
#include "common/Geometry.h"
#include "common/BufferStruct.h"
#include "common/Sky.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

bool CubeMapContext::InitDirect3D()
{
	m_sky = std::make_shared<Sky>(this);

	if (!D3DContext::InitDirect3D()) return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	std::vector<TextureLoadDesc> textureFiles;
	textureFiles.emplace_back("bricksTex", SourcePath() + L"/Textures/bricks.dds");
	textureFiles.emplace_back("bricks", SourcePath() + L"/Textures/bricks2.dds");
	textureFiles.emplace_back("rockTex", SourcePath() + L"/Textures/rock.dds");
	textureFiles.emplace_back("stoneTex", SourcePath() + L"/Textures/stone.dds");
	textureFiles.emplace_back("bricks3Tex", SourcePath() + L"/Textures/bricks3.dds");
	textureFiles.emplace_back("iceTex", SourcePath() + L"/Textures/ice.dds");
	textureFiles.emplace_back("WoodTex", SourcePath() + L"/Textures/WoodCrate01.dds");
	loadTextures(m_d3dDevice.Get(), m_CommandList.Get(), textureFiles);

	int numberDescriptor =  m_Textures.size() + m_TextureArrs.size() + 1;
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

void CubeMapContext::BuildRootSignature()
{
	D3D12_ROOT_PARAMETER rootParameters[5];

	//instanceData
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].Descriptor.RegisterSpace = 1;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	m_rpi.m_CONST_RootParameterIndex = 0;

	//material
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; //change
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.RegisterSpace = 1; //change
	rootParameters[1].Descriptor.ShaderRegister = 1;
	m_rpi.m_Material_RootParameterIndex = 1;

	//pass
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.RegisterSpace = 0;
	rootParameters[2].Descriptor.ShaderRegister = 0;
	m_rpi.m_PASS_RootParameterIndex = 2;

	//texture despector
	D3D12_DESCRIPTOR_RANGE texTable[1];
	texTable[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable[0].NumDescriptors = m_Textures.size();
	texTable[0].BaseShaderRegister = 0;
	texTable[0].RegisterSpace = 0;
	texTable[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//change
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[3].DescriptorTable.pDescriptorRanges = texTable;

	//sampler
	D3D12_DESCRIPTOR_RANGE samplerTable[1];
	samplerTable[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	samplerTable[0].NumDescriptors = 1;
	samplerTable[0].BaseShaderRegister = 0;
	samplerTable[0].RegisterSpace = 0;
	samplerTable[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[4].DescriptorTable.pDescriptorRanges = samplerTable;


	D3D12_ROOT_SIGNATURE_DESC descRootSignature;
	descRootSignature.NumStaticSamplers = 0;
	descRootSignature.pStaticSamplers = nullptr;
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

void CubeMapContext::BuildShadersAndInputLayout()
{
	BuildInstanceShaders();
	BuildLayout();
}

void CubeMapContext::BuildFrameResources()
{
	UINT instanctCount = 0;

	for (auto& ritem : m_AllRitems)
		instanctCount += ritem->m_Instances.size();

	for (int i = 0; i < m_NumFrameResources; ++i)
	{
		m_frameResources.push_back(
			std::make_unique<FrameInstanceResource<InstanceData,
			PassConstantsWithFrog, MaterialShadeRsourceWithTexIndex>  >(m_d3dDevice.Get(),
				1, instanctCount, (UINT)m_Materials.size()));
	}
}

void CubeMapContext::Update(const GameTimer& gt)
{
	D3DContext::Update(gt);
	FrameResourceContextInterface::Update(gt, m_Fence.Get());
	UpdateInstance();
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void CubeMapContext::UpdateMaterialCBs(const GameTimer& gt)
{
	for (auto& e : m_Materials)
	{
		MaterialWithTexTran* mat = static_cast<MaterialWithTexTran*>(e.second.get());
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialShadeRsourceWithTexIndex matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));
			matConstants.DiffuseTextureMapIndex = mat->DiffuseSrvHeapIndex;
			m_currFrameResource->CopyMaterialData(mat->MaterialCBIndex, &matConstants);

			mat->NumFramesDirty--;
		}
	}
}

void CubeMapContext::DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const RenderItem* ritem)
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

void CubeMapContext::DrawFrameResource(ID3D12CommandAllocator* allocator)
{
	ThrowIfFailed(m_CommandList->Reset(allocator, m_PSOs["opaque"].Get()));
	BEFORE_DRAW_SET;

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescriptorHeap.Get(), m_SamplerDescriptorHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//set pass buffer
	m_CommandList->SetGraphicsRootConstantBufferView(m_rpi.m_PASS_RootParameterIndex, m_currFrameResource->getPassGpuAddress());//2

	CD3DX12_GPU_DESCRIPTOR_HANDLE sampler(m_SamplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	m_CommandList->SetGraphicsRootDescriptorTable(4, sampler); //4

	m_CommandList->SetGraphicsRootShaderResourceView(0, m_currFrameResource->getInstanceGpuAddress());
	m_CommandList->SetGraphicsRootShaderResourceView(1, m_currFrameResource->getMaterialGpuAddress());

	m_CommandList->SetGraphicsRootDescriptorTable(3, m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	//pass1
	DrawRenderItem(m_CommandList.Get(), m_AllRitems[0].get());

	//sky
	m_sky->DrawSky(m_CommandList.Get(), allocator, sampler,m_currFrameResource->getPassGpuAddress(), descriptorHeaps);

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

void CubeMapContext::BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList)
{
	BuildRock(device, mCommandList);
}

void CubeMapContext::BuildRenderItems()
{
	auto rocketRitem = std::make_unique<RenderItemWithTex>();
	rocketRitem->m_Geo = m_Geometries["rockGeo"].get();
	rocketRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rocketRitem->FillWithDrawArgs(&(rocketRitem->m_Geo->m_DrawArgs["rock"]));
	rocketRitem->m_Bounds = rocketRitem->m_Geo->m_DrawArgs["rock"].m_Bounds;

	const int n = 5;

	float width = 200.0f;
	float height = 200.0f;
	float depth = 200.0f;

	float x = -0.5f * width;
	float y = -0.5f * height;
	float z = -0.5f * depth;
	float dx = width / (n - 1);
	float dy = height / (n - 1);
	float dz = depth / (n - 1);
	for (int k = 0; k < n; ++k)
	{
		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < n; ++j)
			{
				int index = k * n * n + i * n + j;

				std::shared_ptr< InstanceData> instanceData(new InstanceData);

				instanceData->World = XMFLOAT4X4(
					1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
					x + j * dx, y + i * dy, z + k * dz, 1.0f);

				XMStoreFloat4x4(&instanceData->TexTransform, XMMatrixScaling(2.0f, 2.0f, 1.0f));
				instanceData->MaterialIndex = index % m_Materials.size();

				rocketRitem->m_Instances.emplace_back(instanceData);
			}
		}
	}

	m_AllRitems.push_back(std::move(rocketRitem));

}

void CubeMapContext::BuildMaterials()
{
	auto bricks0 = std::make_unique<MaterialWithTexTran>();
	bricks0->Name = "bricksTex";
	bricks0->MaterialCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto stone0 = std::make_unique<MaterialWithTexTran>();
	stone0->Name = "bricks";
	stone0->MaterialCBIndex = 1;
	stone0->DiffuseSrvHeapIndex = 1;
	stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;

	auto tile0 = std::make_unique<MaterialWithTexTran>();
	tile0->Name = "rockTex";
	tile0->MaterialCBIndex = 2;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.3f;

	auto crate0 = std::make_unique<MaterialWithTexTran>();
	crate0->Name = "stoneTex";
	crate0->MaterialCBIndex = 3;
	crate0->DiffuseSrvHeapIndex = 3;
	crate0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	crate0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	crate0->Roughness = 0.2f;

	auto ice0 = std::make_unique<MaterialWithTexTran>();
	ice0->Name = "bricks3Tex";
	ice0->MaterialCBIndex = 4;
	ice0->DiffuseSrvHeapIndex = 4;
	ice0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	ice0->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	ice0->Roughness = 0.0f;

	auto grass0 = std::make_unique<MaterialWithTexTran>();
	grass0->Name = "iceTex";
	grass0->MaterialCBIndex = 5;
	grass0->DiffuseSrvHeapIndex = 5;
	grass0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	grass0->Roughness = 0.2f;

	auto skullMat = std::make_unique<MaterialWithTexTran>();
	skullMat->Name = "WoodTex";
	skullMat->MaterialCBIndex = 6;
	skullMat->DiffuseSrvHeapIndex = 6;
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	skullMat->Roughness = 0.5f;

	m_Materials["bricksTex"] = std::move(bricks0);
	m_Materials["bricks"] = std::move(stone0);
	m_Materials["rockTex"] = std::move(tile0);
	m_Materials["stoneTex"] = std::move(crate0);
	m_Materials["bricks3Tex"] = std::move(ice0);
	m_Materials["iceTex"] = std::move(grass0);
	m_Materials["WoodTex"] = std::move(skullMat);
}

void CubeMapContext::BuildTexture()
{

	//environment map is projected onto the spherre's surface
 
 
}

