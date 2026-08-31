#include "TexContext.h"
#include "common/Geometry.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;



bool TexContext::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	//load textures
	std::vector<TextureLoadDesc> textureFiles;
	textureFiles.emplace_back("woodCrateTex", SourcePath() + L"/Textures/WoodCrate01.dds");
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

void TexContext::BuildFrameResources()
{
	//const obj
	//pass
	//material
	for (int i = 0; i < m_NumFrameResources; i++)
		m_frameResources.emplace_back(std::make_unique<FrameResourceWithConstMaterial <ObjectConstantsWithTexTran, PassConstantsWithLight, MaterialConstantsWithTexTran> >(m_d3dDevice.Get(),
			1, 
			m_AllRitems.size(), 
			(UINT)m_Materials.size()));

}

void TexContext::BuildShadersAndInputLayout()
{
	m_Shaders["standardVS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders\\Tex.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders\\Tex.hlsl", nullptr, "PS", "ps_5_1");

	m_InputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void TexContext::BuildRootSignature()
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
	rootParameters[0].Descriptor.ShaderRegister =0 ;
	//pass
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.RegisterSpace = 0;
	rootParameters[1].Descriptor.ShaderRegister = 1;
	//material
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.RegisterSpace = 0;
	rootParameters[2].Descriptor.ShaderRegister = 2;

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

void TexContext::BuildPSOs()
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
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&m_OpaquePSO)));

}

void TexContext::BuildMaterials()
{
	auto woodCrate = std::make_unique<MaterialWithTexTran>();
	woodCrate->Name = "woodCrate";
	woodCrate->MaterialCBIndex = 0;
	woodCrate->DiffuseSrvHeapIndex = 0;
	woodCrate->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	woodCrate->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	woodCrate->Roughness = 0.2f;
	m_Materials["woodCrate"] = std::move(woodCrate);
}

void TexContext::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItemWithTex>();
	boxRitem->m_ObjCBIndex = 0;
	boxRitem->m_Material = m_Materials["woodCrate"].get();
	boxRitem->m_Geo = m_Geometries["boxGeo"].get();
	boxRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->m_IndexCount = boxRitem->m_Geo->m_DrawArgs["box"].m_IndexCount;
	boxRitem->m_StartIndexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_StartIndexLocation;
	boxRitem->m_BaseVertexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_BaseVertexLocation;
	m_AllRitems.push_back(std::move(boxRitem));

	for (auto& e : m_AllRitems)
		m_OpaqueRitems.push_back(e.get());

}

void TexContext::Update(const GameTimer& gt)
{
	D3DContext::Update(gt);
	FrameResourceContextInterface::Update(gt, m_Fence.Get());

	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void TexContext::UpdateObjectCBs(const GameTimer& gt)
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
			itemWithM->m_NumFramesDirty--;
		}
	}
}

void TexContext::UpdateMaterialCBs(const GameTimer& gt)
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

			m_currFrameResource->CopyMaterialData(mat->MaterialCBIndex, &matConstants);

			mat->NumFramesDirty--;
		}
	}
}

void TexContext::UpdateMainPassCB(const GameTimer& gt)
{
	UPDATE_MAIN_PASS;

	m_MainPassCB.m_AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	m_MainPassCB.m_Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	m_MainPassCB.m_Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	m_MainPassCB.m_Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	m_MainPassCB.m_Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	m_MainPassCB.m_Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	m_MainPassCB.m_Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	m_currFrameResource->CopyPassData(0, &m_MainPassCB);
}

void TexContext::Draw(const GameTimer& gt)
{
	FrameResourceContextInterface::Draw(gt, m_CurrentFence, m_Fence.Get(), m_CommandQueue.Get());
}

void TexContext::DrawFrameResource(ID3D12CommandAllocator* allocator)
{
	ThrowIfFailed(m_CommandList->Reset(allocator, m_OpaquePSO.Get()));
	BEFORE_DRAW_SET;

	m_CommandList->SetGraphicsRootConstantBufferView(1, m_currFrameResource->getPassGpuAddress());

	DrawRenderItems(m_CommandList.Get(), m_OpaqueRitems);

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

void TexContext::DrawRenderItems(ID3D12GraphicsCommandList* cmdList,
	const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstantsWithTexTran));
	UINT matCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(MaterialConstantsWithTexTran));

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescriptorHeap.Get(), m_SamplerDescriptorHeap.Get()};
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	for (size_t i = 0; i < ritems.size(); ++i)
	{
		RenderItemWithTex* ri = static_cast<RenderItemWithTex*>(ritems[i]);
		cmdList->IASetVertexBuffers(0, 1, &ri->m_Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->m_Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->m_PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->m_Material->DiffuseSrvHeapIndex, m_CbvSrvUavDescriptorSize);

		CD3DX12_GPU_DESCRIPTOR_HANDLE sampler(m_SamplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress =
			m_currFrameResource->getConstGpuAddress() +
			ri->m_ObjCBIndex * objCBByteSize;

		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress =
			m_currFrameResource->getMaterialGpuAddress() +
			ri->m_Material->MaterialCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(3, tex);
		cmdList->SetGraphicsRootDescriptorTable(4, sampler);

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(2, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->m_IndexCount,
			1, ri->m_StartIndexLocation,
			ri->m_BaseVertexLocation, 0);
	}
}

