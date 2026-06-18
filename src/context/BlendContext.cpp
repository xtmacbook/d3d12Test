#include "BlendContext.h"
#include "../common/Geometry.h"
#include "../common/Data.h"
#include "../common/App.h"
#include "Waves.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

class BlendFrameResource : public FrameResourceInterface
{
public:

	BlendFrameResource(ID3D12Device* device, UINT passCount, UINT objectCount,
		UINT materialCount, UINT waveVertCount)
	{
		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_CmdListAlloc.GetAddressOf())));


		m_ObjectCB = std::make_unique<UploadBuffer<ObjectConstantsWithTexTran>>(device, objectCount, true);
		m_PassCB = std::make_unique<UploadBuffer<PassConstantsWithFrog>>(device, passCount, true);
		m_MaterialCB = std::make_unique<UploadBuffer<MaterialConstantsWithTexTran>>(device, materialCount, true);
		m_WavesVB = std::make_unique<UploadBuffer<VertexNT>>(device, waveVertCount, false);

	}

	BlendFrameResource(const BlendFrameResource& rhs) = delete;
	BlendFrameResource& operator=(const BlendFrameResource& rhs) = delete;
	~BlendFrameResource() {};


	virtual void CopyConstData(int elementIndex, void* data) override
	{
		ObjectConstantsWithTexTran* content = static_cast<ObjectConstantsWithTexTran*>(data);
		m_ObjectCB->CopyData(elementIndex, *content);
	}
	virtual void CopyPassData(int elementIndex, void* data) override
	{
		PassConstantsWithFrog* content = static_cast<PassConstantsWithFrog*>(data);
		m_PassCB->CopyData(elementIndex, *content);
	}

	virtual void CopyWaveData(int elementIndex, void* data) override
	{
		VertexNT* content = static_cast<VertexNT*>(data);
		m_WavesVB->CopyData(elementIndex, *content);
	}

	virtual void CopyMaterialData(int elementIndex, void* data)
	{
		MaterialConstantsWithTexTran* content = static_cast<MaterialConstantsWithTexTran*>(data);
		m_MaterialCB->CopyData(elementIndex, *content);
	}

	virtual D3D12_GPU_VIRTUAL_ADDRESS getConstGpuAddress() override
	{
		return m_ObjectCB->Resource()->GetGPUVirtualAddress();

	}
	virtual D3D12_GPU_VIRTUAL_ADDRESS getPassGpuAddress() override
	{
		return m_PassCB->Resource()->GetGPUVirtualAddress();

	}
	virtual D3D12_GPU_VIRTUAL_ADDRESS getMaterialGpuAddress() override
	{
		return m_MaterialCB->Resource()->GetGPUVirtualAddress();
	}
	virtual D3D12_GPU_VIRTUAL_ADDRESS getWaveGpuAddress()  override
	{
		return m_WavesVB->Resource()->GetGPUVirtualAddress();
	}

	virtual ID3D12Resource* getWaveResouce()override
	{
		return m_WavesVB->Resource();
	}

	std::unique_ptr<UploadBuffer<PassConstantsWithFrog>>				m_PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstantsWithTexTran>>           m_ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstantsWithTexTran>>			m_MaterialCB = nullptr;
	std::unique_ptr<UploadBuffer<VertexNT>>								m_WavesVB = nullptr;

};

bool BlendContext::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	//load textures
	std::unordered_map<std::string, std::wstring> textureFiles;
	textureFiles["grassTex"] = SourcePath() +  L"/Textures/grass.dds";
	textureFiles["waterTex"] = SourcePath() +  L"/Textures/water1.dds";
	textureFiles["fenceTex"] = SourcePath() +  L"/Textures/WireFence.dds";
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

void BlendContext::BuildFrameResources()
{
	//const obj
	//pass
	//material
	for (int i = 0; i < m_NumFrameResources; i++)
		m_frameResources.emplace_back(std::make_unique<BlendFrameResource>(m_d3dDevice.Get(),
			1,
			m_AllRitems.size(),
			(UINT)m_Materials.size(), GetWave()->VertexCount()));

}

void BlendContext::BuildShadersAndInputLayout()
{
	BuildShaders();
	BuildLayout();
}

void BlendContext::BuildShapeGeometry(ID3D12Device* d3Device, ID3D12GraphicsCommandList* mCommandList)
{
	BuildBox(d3Device, mCommandList, { 8.0f, 8.0f, 8.0f });
	BuildLand(d3Device, mCommandList);
	BuildWave(d3Device, mCommandList);
}

void BlendContext::BuildRootSignature()
{
	//sampler despector heap

	/*
	In order to bind samplers to shaders for use, we need to bind descriptors to sampler objects
	*/

	D3D12_ROOT_PARAMETER rootParameters[5];

	//cbobject
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	m_rpi.m_CONST_RootParameterIndex = 0;

	//pass
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.RegisterSpace = 0;
	rootParameters[1].Descriptor.ShaderRegister = 1;
	m_rpi.m_PASS_RootParameterIndex = 1;

	//material
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.RegisterSpace = 0;
	rootParameters[2].Descriptor.ShaderRegister = 2;
	m_rpi.m_Material_RootParameterIndex = 2;

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

void BlendContext::BuildPSOs()
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

	//transparent
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;
	BlendPSO(&transparentPsoDesc);
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&m_PSOs["transparent"])));

	//alpha test
	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["alphaTestedPS"]->GetBufferPointer()),
		m_Shaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&m_PSOs["alphaTested"])));
}

void BlendContext::BuildMaterials()
{
	auto grass = std::make_unique<MaterialWithTexTran>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;
	m_Materials["grass"] = std::move(grass);

	auto water = std::make_unique<MaterialWithTexTran>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;
	m_Materials["water"] = std::move(water);


	auto wirefence = std::make_unique<MaterialWithTexTran>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	wirefence->Roughness = 0.25f;
	m_Materials["wirefence"] = std::move(wirefence);

}

void BlendContext::BuildRenderItems()
{
	auto wavesRitem = std::make_unique<RenderItemWithTex>();
	XMStoreFloat4x4(&wavesRitem->m_TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	wavesRitem->m_World = MathHelper::Identity4x4();
	wavesRitem->m_ObjCBIndex = 0;
	wavesRitem->m_Material = m_Materials["water"].get();
	wavesRitem->m_Geo = m_Geometries["waterGeo"].get();
	wavesRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->m_IndexCount = wavesRitem->m_Geo->m_DrawArgs["grid"].m_IndexCount;
	wavesRitem->m_StartIndexLocation = wavesRitem->m_Geo->m_DrawArgs["grid"].m_StartIndexLocation;
	wavesRitem->m_BaseVertexLocation = wavesRitem->m_Geo->m_DrawArgs["grid"].m_BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::Transparent].push_back(wavesRitem.get());

	m_WavesRitem = wavesRitem.get();
	m_AllRitems.push_back(std::move(wavesRitem));


	auto gridRitem = std::make_unique<RenderItemWithTex>();
	XMStoreFloat4x4(&gridRitem->m_TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	gridRitem->m_World = MathHelper::Identity4x4();
	gridRitem->m_ObjCBIndex = 1;
	gridRitem->m_Material = m_Materials["grass"].get();
	gridRitem->m_Geo = m_Geometries["landGeo"].get();
	gridRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->m_IndexCount = gridRitem->m_Geo->m_DrawArgs["grid"].m_IndexCount;
	gridRitem->m_StartIndexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_StartIndexLocation;
	gridRitem->m_BaseVertexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	m_AllRitems.push_back(std::move(gridRitem));

	auto boxRitem = std::make_unique<RenderItemWithTex>();
	boxRitem->m_ObjCBIndex = 2;
	XMStoreFloat4x4(&boxRitem->m_World, XMMatrixTranslation(3.0f, 2.0f, -9.0f));
	boxRitem->m_Material = m_Materials["wirefence"].get();
	boxRitem->m_Geo = m_Geometries["boxGeo"].get();
	boxRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->m_IndexCount = boxRitem->m_Geo->m_DrawArgs["box"].m_IndexCount;
	boxRitem->m_StartIndexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_StartIndexLocation;
	boxRitem->m_BaseVertexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_BaseVertexLocation;
	m_RitemLayer[(int)RenderLayer::AlphaTested].push_back(boxRitem.get());
	m_AllRitems.push_back(std::move(boxRitem));
}

void BlendContext::Update(const GameTimer& gt)
{
	D3DContext::Update(gt);  
	FrameResourceContextInterface::Update(gt, m_Fence.Get());

	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
	updateGeometry(gt);

	Waves* waveGeom = GetWave();
	if (waveGeom)
	{
		for (int i = 0; i < waveGeom->VertexCount(); ++i)
		{
			VertexNT v;

			v.Pos = waveGeom->Position(i);
			v.Normal = waveGeom->Normal(i);

			// Derive tex-coords from position by 
			// mapping [-w/2,w/2] --> [0,1]
			v.TexC.x = 0.5f + v.Pos.x / waveGeom->Width();
			v.TexC.y = 0.5f - v.Pos.z / waveGeom->Depth();

			m_currFrameResource->CopyWaveData(i, &v);
		}

		// Set the dynamic VB of the wave renderitem to the current frame VB.
		m_WavesRitem->m_Geo->m_VertexBufferGPU = m_currFrameResource->getWaveResouce();
	}
}

void BlendContext::UpdateObjectCBs(const GameTimer& gt)
{
	for (auto& e : m_AllRitems)
	{
		RenderItemWithTex* itemWithM = static_cast<RenderItemWithTex*>(e.get());

		if (itemWithM->m_NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->m_World);
			XMMATRIX texTransform = XMLoadFloat4x4(&itemWithM->m_TexTransform);

			ObjectConstantsWithTexTran objConstants;
			XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			m_currFrameResource->CopyConstData(itemWithM->m_ObjCBIndex, &objConstants);
			e->m_NumFramesDirty--;
		}
	}
}

void BlendContext::UpdateMaterialCBs(const GameTimer& gt)
{
	for (auto& e : m_Materials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		MaterialWithTexTran* mat = static_cast<MaterialWithTexTran*>(e.second.get());
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstantsWithTexTran matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			m_currFrameResource->CopyMaterialData(mat->MatCBIndex, &matConstants);

			mat->NumFramesDirty--;
		}
	}
}

void BlendContext::UpdateMainPassCB(const GameTimer& gt)
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

void BlendContext::Draw(const GameTimer& gt)
{
	D3DContext::Draw(gt);
	FrameResourceContextInterface::Draw(gt, m_CurrentFence, m_Fence.Get(), m_CommandQueue.Get());
}

void BlendContext::DrawFrameResource(ID3D12CommandAllocator* allocator)
{
	ThrowIfFailed(m_CommandList->Reset(allocator, m_PSOs["opaque"].Get()));
	BEFORE_DRAW_SET;

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescriptorHeap.Get(), m_SamplerDescriptorHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//set pass buffer
	m_CommandList->SetGraphicsRootConstantBufferView(m_rpi.m_PASS_RootParameterIndex, m_currFrameResource->getPassGpuAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE sampler(m_SamplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	m_CommandList->SetGraphicsRootDescriptorTable(4, sampler);

	//land
	DrawRenderItem(m_CommandList.Get(), m_RitemLayer[(int)RenderLayer::Opaque][0]);
	
	//box
	m_CommandList->SetPipelineState(m_PSOs["alphaTested"].Get());
	DrawRenderItem(m_CommandList.Get(), m_RitemLayer[(int)RenderLayer::AlphaTested][0]);

	//water
	m_CommandList->SetPipelineState(m_PSOs["transparent"].Get());
	DrawRenderItem(m_CommandList.Get(), m_AllRitems[0].get());


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


void BlendContext::DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const RenderItem* ritem)
{
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstantsWithTexTran));
	UINT matCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(MaterialConstantsWithTexTran));

	const RenderItemWithTex* renderItem = static_cast<const RenderItemWithTex*>(ritem);

	//set vertex buffer
	cmdList->IASetVertexBuffers(0, 1, &ritem->m_Geo->VertexBufferView());
	cmdList->IASetIndexBuffer(&ritem->m_Geo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(ritem->m_PrimitiveType);

	//set object const buffer 
	UINT64 offset = static_cast<UINT64>(ritem->m_ObjCBIndex) * objCBByteSize;
	D3D12_GPU_VIRTUAL_ADDRESS startAddress = m_currFrameResource->getConstGpuAddress();
	cmdList->SetGraphicsRootConstantBufferView(m_rpi.m_CONST_RootParameterIndex, startAddress + offset);

	//set material
	offset = static_cast<UINT64>(renderItem->m_Material->MatCBIndex) * matCBByteSize;
	startAddress = m_currFrameResource->getMaterialGpuAddress();
	cmdList->SetGraphicsRootConstantBufferView(m_rpi.m_Material_RootParameterIndex, startAddress + offset);

	//set texture buffer 
	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	tex.Offset(renderItem->m_Material->DiffuseSrvHeapIndex, m_CbvSrvUavDescriptorSize);
	cmdList->SetGraphicsRootDescriptorTable(3, tex);

	//draw
	cmdList->DrawIndexedInstanced(renderItem->m_IndexCount,
		1, renderItem->m_StartIndexLocation,
		renderItem->m_BaseVertexLocation, 0);

}
