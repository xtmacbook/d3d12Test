#include "DynamicIndexComContex.h"
#include "common/BufferStruct.h"
#include "common/Geometry.h"
#include "common/BufferStruct.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

void DynamicIndexComContext::BuildRootSignature()
{
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
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; //change
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.RegisterSpace = 1; //change
	rootParameters[2].Descriptor.ShaderRegister = 0;
	m_rpi.m_Material_RootParameterIndex = 2;

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

void DynamicIndexComContext::BuildShadersAndInputLayout()
{
	BuildDynamicShaders();
	BuildLayout();
}

void DynamicIndexComContext::BuildFrameResources()
{
    for (int i = 0; i < m_NumFrameResources; ++i)
	{
		m_frameResources.push_back(
            std::make_unique<FrameResourceWithSRVMaterial<ObjectConstantsWithTexTranAndMaterialIndex,
			PassConstantsWithFrog, MaterialShadeRsourceWithTexIndex>  >(m_d3dDevice.Get(),
				1, (UINT)m_AllRitems.size(), (UINT)m_Materials.size()));
	}
}

void DynamicIndexComContext::UpdateObjectCBs(const GameTimer &gt)
{
    for (auto& e : m_AllRitems)
	{
		RenderItemWithTex* itemWithM = static_cast<RenderItemWithTex*>(e.get());

		if (itemWithM->m_NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->m_World);
			XMMATRIX texTransform = XMLoadFloat4x4(&itemWithM->m_TexTransform);

			ObjectConstantsWithTexTranAndMaterialIndex objConstants;
			XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
            objConstants.MaterialIndex = itemWithM->m_Material->MaterialCBIndex;
			m_currFrameResource->CopyConstData(itemWithM->m_ObjCBIndex, &objConstants);
			e->m_NumFramesDirty--;
		}
	}
}

void DynamicIndexComContext::UpdateMaterialCBs(const GameTimer &gt)
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

void DynamicIndexComContext::DrawRenderItem(ID3D12GraphicsCommandList *cmdList, const RenderItem *ritem)
{
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstantsWithTexTranAndMaterialIndex));
	UINT matCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(MaterialShadeRsourceWithTexIndex));

	const RenderItemWithTex* renderItem = static_cast<const RenderItemWithTex*>(ritem);

	//set vertex buffer
	cmdList->IASetVertexBuffers(0, 1, &ritem->m_Geo->VertexBufferView());
	cmdList->IASetIndexBuffer(&ritem->m_Geo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(ritem->m_PrimitiveType);

	//set object const buffer 
	UINT64 offset = static_cast<UINT64>(ritem->m_ObjCBIndex) * objCBByteSize;
	D3D12_GPU_VIRTUAL_ADDRESS startAddress = m_currFrameResource->getConstGpuAddress();
	cmdList->SetGraphicsRootConstantBufferView(m_rpi.m_CONST_RootParameterIndex, startAddress + offset);

	//draw
	cmdList->DrawIndexedInstanced(renderItem->m_IndexCount,
		1, renderItem->m_StartIndexLocation,
		renderItem->m_BaseVertexLocation, 0);
}
